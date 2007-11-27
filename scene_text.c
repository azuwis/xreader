#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include "display.h"
#include "win.h"
#include "ctrl.h"
#include "fs.h"
#include "image.h"
#ifdef ENABLE_USB
#include "usb.h"
#endif
#include "power.h"
#include "bookmark.h"
#include "conf.h"
#include "charsets.h"
#include "fat.h"
#include "location.h"
#ifdef ENABLE_MUSIC
#include "mp3.h"
#ifdef ENABLE_LYRIC
#include "lyric.h"
#endif
#endif
#include "text.h"
#include "bg.h"
#include "copy.h"
#ifdef ENABLE_PMPAVC
#include "avc.h"
#endif
#include "version.h"
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"

dword ctlkey[13], ctlkey2[13], ku, kd, kl, kr;
dword cidx, rrow = INVALID;
volatile int ticks = 0;
int rowtop = 0;
char tr[8], * trow = NULL;
bool text_needrf = true, text_needrp = true, text_needrb = false;
char filename[256], archname[256];
int autopage_prevsetting = 0;

int scene_book_reload(dword selidx)
{
	if(where == scene_in_gz) {
		strcpy(filename, config.path);
		strcpy(archname, config.shortpath);
	}
	else if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
	{
		strcpy(filename, filelist[selidx].compname);
		strcpy(archname, config.shortpath);
		strcat(archname, filename);
	}
	else
	{
		strcpy(filename, config.shortpath);
		strcat(filename, filelist[selidx].shortname);
		strcpy(archname, filename);
	}
	if(rrow == INVALID)
	{
		if(config.autobm)
		{
			rrow = bookmark_autoload(archname);
			text_needrb = true;
		}
		else
			rrow = 0;
	}
	if(fs != NULL)
	{
		text_close(fs);
		fs = NULL;
	}
	scene_power_save(false);
	if(where == scene_in_zip)
		fs = text_open_in_zip(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if(where == scene_in_gz)
		fs = text_open_in_gz(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if(where == scene_in_chm)
		fs = text_open_in_chm(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if(where == scene_in_rar)
		fs = text_open_in_rar(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if((t_fs_filetype)filelist[selidx].data == fs_filetype_unknown)
	{
		int t = config.vertread;
		if(t == 3)
			t = 0;
		fs = text_open_binary(filename, t);
	}
	else
		fs = text_open(filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	if(fs == NULL)
	{
		disp_duptocachealpha(50);
		return 1;
	}
	if(text_needrb && (t_fs_filetype)filelist[selidx].data != fs_filetype_unknown)
	{
		rowtop = 0;
		fs->crow = 1;
		while(fs->crow < fs->row_count && (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf <= rrow)
			fs->crow ++;
		fs->crow --;
		text_needrb = false;
	}
	else
		fs->crow = rrow;

	if (fs->crow >= fs->row_count)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;

	trow = &tr[utils_dword2string(fs->row_count, tr, 7)];
	strcpy(config.lastfile, filelist[selidx].compname);
	scene_power_save(true);
	return 0;
}

int scene_printbook(dword selidx)
{
	char ci[8] = "       ", cr[512];
	disp_waitv();
#ifdef ENABLE_BG
	if(!bg_display())
#endif
		disp_fillvram(config.bgcolor);
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);
	switch(config.vertread)
	{
		case 3:
			disp_putnstringreversal(config.borderspace, config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				disp_putnstringreversal(config.borderspace, config.borderspace + (DISP_BOOK_FONTSIZE + config.rowspace) * cidx - rowtop, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE) : PSP_SCREEN_HEIGHT);
			}
			if(config.infobar == conf_infobar_info)
			{
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				disp_line(0, DISP_BOOK_FONTSIZE, 479, DISP_BOOK_FONTSIZE, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				char autopageinfo[80];
				if(config.autopagetype == 2)
					autopageinfo[0] = 0;
				else if(config.autopagetype == 1) {
					snprintf(autopageinfo, 80, "%s: 时间 %d 速度 %d", "自动滚屏", config.autolinedelay, config.autopage);
					autopageinfo[80-1] = '\0';
				}
				else {
					if(config.autopage != 0) {
						snprintf(autopageinfo, 80, "自动翻页: 时间 %d", config.autopage);
						autopageinfo[80-1] = '\0';
					}
					else
						autopageinfo[0] = 0;
				}
				snprintf(cr, 512, "%s/%s  %s  %s  GI: %d  %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, offset, autopageinfo);
				cr[512-1]='\0';
				disp_putnstringreversal(0, PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE, config.forecolor, (const byte *)cr, 960 / DISP_BOOK_FONTSIZE, 0, 0, DISP_BOOK_FONTSIZE, 0);
			}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			else if(config.infobar == conf_infobar_lyric)
			{
				disp_line(480 - 1, 272 - 1 - 271 + DISP_BOOK_FONTSIZE, 480 - 1 - 479, 272 - 1 - 271 + DISP_BOOK_FONTSIZE, config.forecolor);
				const char * ls[1];
				dword ss[1];
				if(lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss) && ls[0] != NULL)
				{
					if(ss[0] > 960 / DISP_BOOK_FONTSIZE)
						ss[0] = 960 / DISP_BOOK_FONTSIZE;
					char t[BUFSIZ];
					lyric_decode(ls[0], t, &ss[0]);
					disp_putnstringreversal((240 - ss[0] * DISP_BOOK_FONTSIZE / 4), PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE, config.forecolor, (const byte *)t, ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
			break;			
		case 2:
			disp_putnstringlvert(config.borderspace, 271 - config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				disp_putnstringlvert((DISP_BOOK_FONTSIZE + config.rowspace) * cidx + config.borderspace - rowtop, 271 - config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (479 -  DISP_BOOK_FONTSIZE) : PSP_SCREEN_WIDTH);
			}
			if(config.infobar == conf_infobar_info)
			{
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				disp_line(479 - DISP_BOOK_FONTSIZE, 0, 479 - DISP_BOOK_FONTSIZE, 271, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				char autopageinfo[80];
				if(config.autopagetype == 2)
					autopageinfo[0] = 0;
				if(config.autopagetype == 1) {
					snprintf(autopageinfo, 80, "滚屏: 时%d 速%d", config.autolinedelay, config.autopage);
					autopageinfo[80-1]='\0';
				}
				if(config.autopagetype == 0) {
					snprintf(autopageinfo, 80, "翻页: 时%d", config.autopage);
					autopageinfo[80-1]='\0';
				}
				snprintf(cr, 512, "%s/%s  %s  %s  %d  %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, offset, autopageinfo);
				cr[512-1]='\0';
				disp_putnstringlvert(PSP_SCREEN_WIDTH - DISP_BOOK_FONTSIZE, 271, config.forecolor, (const byte *)cr, 544 / DISP_BOOK_FONTSIZE, 0, 0, DISP_BOOK_FONTSIZE, 0);
			}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			else if(config.infobar == conf_infobar_lyric)
			{
				disp_line(479 - DISP_BOOK_FONTSIZE, 0, 479 - DISP_BOOK_FONTSIZE, 271, config.forecolor);
				const char * ls[1];
				dword ss[1];
				if(lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss) && ls[0] != NULL)
				{
					if(ss[0] > 544 / DISP_BOOK_FONTSIZE)
						ss[0] = 544 / DISP_BOOK_FONTSIZE;
					char t[BUFSIZ];
					lyric_decode(ls[0], t, &ss[0]);
					disp_putnstringlvert(PSP_SCREEN_WIDTH - DISP_BOOK_FONTSIZE, 271 - (136 - ss[0] * DISP_BOOK_FONTSIZE / 4), config.forecolor, (const byte *)t, ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
			break;
		case 1:
			disp_putnstringrvert(479 - config.borderspace, config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				disp_putnstringrvert(479 - (DISP_BOOK_FONTSIZE + config.rowspace) * cidx - config.borderspace + rowtop, config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (DISP_BOOK_FONTSIZE + 1) : 0);
			}
			if(config.infobar == conf_infobar_info)
			{
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				disp_line(DISP_BOOK_FONTSIZE, 0, DISP_BOOK_FONTSIZE, 271, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				char autopageinfo[80];
				if(config.autopagetype == 2)
					autopageinfo[0] = 0;
				if(config.autopagetype == 1) {
					snprintf(autopageinfo, 80, "滚屏: 时%d 速%d", config.autolinedelay, config.autopage);
					autopageinfo[80-1]='\0';
				}
				if(config.autopagetype == 0) {
					snprintf(autopageinfo, 80, "翻页: 时%d", config.autopage);
					autopageinfo[80-1]='\0';
				}
				snprintf(cr, 512, "%s/%s  %s  %s  %d  %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, offset, autopageinfo);
				cr[512-1]='\0';
				disp_putnstringrvert(DISP_BOOK_FONTSIZE - 1, 0, config.forecolor, (const byte *)cr, 544 / DISP_BOOK_FONTSIZE, 0, 0, DISP_BOOK_FONTSIZE, 0);
			}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			else if(config.infobar == conf_infobar_lyric)
			{
				disp_line(DISP_BOOK_FONTSIZE, 0, DISP_BOOK_FONTSIZE, 271, config.forecolor);
				const char * ls[1];
				dword ss[1];
				if(lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss) && ls[0] != NULL)
				{
					if(ss[0] > 544 / DISP_BOOK_FONTSIZE)
						ss[0] = 544 / DISP_BOOK_FONTSIZE;
					char t[BUFSIZ];
					lyric_decode(ls[0], t, &ss[0]);
					disp_putnstringrvert(DISP_BOOK_FONTSIZE - 1, (136 - ss[0] * DISP_BOOK_FONTSIZE / 4), config.forecolor, (const byte *)t, ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
			break;
		default:
			{
			disp_putnstringhorz(config.borderspace, config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				/*
				if(!havedisplayed) {
					char infomsg[80];
					sprintf(infomsg, "%d+(%d+%d)*%d-%d=%d", config.borderspace, DISP_BOOK_FONTSIZE, config.rowspace, cidx, rowtop, config.borderspace + (DISP_BOOK_FONTSIZE + config.rowspace) * cidx - rowtop);
					win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
					
					havedisplayed = true;
				}
				*/
				disp_putnstringhorz(config.borderspace, config.borderspace + (DISP_BOOK_FONTSIZE + config.rowspace) * cidx - rowtop, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (271 - DISP_BOOK_FONTSIZE) : PSP_SCREEN_HEIGHT);
			}
			}
			if(config.infobar == conf_infobar_info)
			{
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				disp_line(0, 271 - DISP_BOOK_FONTSIZE, 479, 271 - DISP_BOOK_FONTSIZE, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				char autopageinfo[80];
				if(config.autopagetype == 2)
					autopageinfo[0] = 0;
				else if(config.autopagetype == 1) {
					snprintf(autopageinfo, 80, "%s: 时间 %d 速度 %d", "自动滚屏", config.autolinedelay, config.autopage);
					autopageinfo[80-1] = '\0';
				}
				else {
					if(config.autopage != 0)
					{
						snprintf(autopageinfo, 80, "自动翻页: 时间 %d", config.autopage);
						autopageinfo[80-1] = '\0';
					}
					else
						autopageinfo[0] = 0;
				}
				snprintf(cr, 512, "%s/%s  %s  %s  GI: %d  %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, offset, autopageinfo);
				cr[512-1]='\0';
				disp_putnstringhorz(0, PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE, config.forecolor, (const byte *)cr, 960 / DISP_BOOK_FONTSIZE, 0, 0, DISP_BOOK_FONTSIZE, 0);
			}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			else if(config.infobar == conf_infobar_lyric)
			{
				disp_line(0, 271 - DISP_BOOK_FONTSIZE, 479, 271 - DISP_BOOK_FONTSIZE, config.forecolor);
				const char * ls[1];
				dword ss[1];
				if(lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss) && ls[0] != NULL)
				{
					if(ss[0] > 960 / DISP_BOOK_FONTSIZE)
						ss[0] = 960 / DISP_BOOK_FONTSIZE;
					char t[BUFSIZ];
					lyric_decode(ls[0], t, &ss[0]);
					disp_putnstringhorz((240 - ss[0] * DISP_BOOK_FONTSIZE / 4), PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE, config.forecolor, (const byte *)t, ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
	}
	if(config.scrollbar)
	{
		switch(config.vertread)
		{
			case 3:
				{
					dword slen = config.infobar ? (270 - DISP_BOOK_FONTSIZE) : 271, bsize = 2 + (slen - 2) * rowsperpage / fs->row_count, startp = slen * fs->crow / fs->row_count, endp;
					if(startp + bsize > slen)
						startp = slen - bsize;
					endp = startp + bsize;
					disp_line(480 - 1 - 475, 272 - 1 - 0, 480 - 1 - 475, 272 - 1 - slen, config.forecolor);
					disp_fillrect(480 - 1 - 476, 272 - 1 - startp, 480 - 1 - 479, 272 - 1 - endp, config.forecolor);
				}
				break;
			case 2:
				{
					dword sright = 479 - (config.infobar ? (DISP_BOOK_FONTSIZE + 1) : 0), slen = sright, bsize = 2 + (slen - 2) * rowsperpage / fs->row_count, startp = slen * fs->crow / fs->row_count, endp;
					if(startp + bsize > slen)
						startp = slen - bsize;
					endp = startp + bsize;
					disp_line(0, 4, sright, 4, config.forecolor);
					disp_fillrect(startp, 0, endp, 3, config.forecolor);
				}
				break;
			case 1:
				{
					dword sleft = (config.infobar ? (DISP_BOOK_FONTSIZE + 1) : 0), slen = 479 - sleft, bsize = 2 + (slen - 2) * rowsperpage / fs->row_count, endp = 479 - slen * fs->crow / fs->row_count, startp;
					if(endp - bsize < sleft)
						endp = sleft + bsize;
					startp = endp - bsize;
					disp_line(sleft, 267, 479, 267, config.forecolor);
					disp_fillrect(startp, 268, endp, 271, config.forecolor);
				}
				break;
			default:
				{
					dword slen = config.infobar ? (270 - DISP_BOOK_FONTSIZE) : 271, bsize = 2 + (slen - 2) * rowsperpage / fs->row_count, startp = slen * fs->crow / fs->row_count, endp;
					if(startp + bsize > slen)
						startp = slen - bsize;
					endp = startp + bsize;
					disp_line(475, 0, 475, slen, config.forecolor);
					disp_fillrect(476, startp, 479, endp, config.forecolor);
				}
				break;
		}
	}
	disp_flip();
	return 0;
}

void move_line_up(int line)
{
	rowtop = 0;
	if (fs->crow > line)
		fs->crow -= line;
	else
		fs->crow = 0;
	text_needrp = true;
}

void move_line_down(int line)
{
	rowtop = 0;
	fs->crow += line;
	if(fs->crow >= fs->row_count - 1)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
	text_needrp = true;
}

void move_line_to_start()
{
	rowtop = 0;
	if(fs->crow != 0)
	{
		fs->crow = 0;
		text_needrp = true;
	}
}

void move_line_to_end()
{
	rowtop = 0;
	if(fs->row_count > 0) {
		if(fs->crow != fs->row_count - 1) {
			fs->crow = fs->row_count - 1;
			text_needrp = true;
		}
	}
	else {
		fs->crow = 0;
	}
}

void move_line_analog(int x, int y)
{
	extern void move_line_smooth(int inc);
	
	switch(config.vertread)
	{
		case 3:
			move_line_smooth(-y/8);
			break;
		case 2:
			move_line_smooth(x/8);
			break;
		case 1:
			move_line_smooth(-x/8);
			break;
		default:
			move_line_smooth(y/8);
			break;
	}
}

void move_line_smooth(int inc)
{
	rowtop += inc;
	if(rowtop < 0)
	{
		while(fs->crow > 0 && rowtop < 0)
		{
			rowtop += DISP_BOOK_FONTSIZE;
			fs->crow --;
		}
		// prevent dither when config.autopage < 5
		if(config.autopagetype == 1 && abs(config.autopage) < 5)
			rowtop = DISP_BOOK_FONTSIZE + 1;
	}
	else if(rowtop >= DISP_BOOK_FONTSIZE + config.rowspace)
	{
		while(fs->crow < fs->row_count - 1 && rowtop >= DISP_BOOK_FONTSIZE + config.rowspace)
		{
			rowtop -= DISP_BOOK_FONTSIZE;
			fs->crow ++;
		}
		// prevent dither when config.autopage < 5
		if(config.autopagetype == 1 && abs(config.autopage) < 5)
			rowtop = 0;
	}
	if(rowtop < 0 || rowtop >= DISP_BOOK_FONTSIZE + config.rowspace)
		rowtop = 0;
	text_needrp = true;
}

int move_page_up(dword key, dword *selidx)
{
	rowtop = 0;
	if(fs->crow == 0)
	{
		if(config.pagetonext && key != kl && fs_is_txtbook((t_fs_filetype)filelist[*selidx].data))
		{
			dword orgidx = *selidx;
			do {
				if(*selidx > 0)
					(*selidx) --;
				else
					*selidx = filecount - 1;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[*selidx].data));
			if(*selidx != orgidx)
			{
				if(config.autobm)
					bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
				text_needrf = text_needrp = true;
				text_needrb = false;
				rrow = INVALID;
			}
		}
		return 1;
	}
	if (fs->crow > (config.rlastrow ? (rowsperpage - 1) : rowsperpage))
		fs->crow -= (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
	else
		fs->crow = 0;
	text_needrp = true;
	return 0;
}

int move_page_down(dword key, dword *selidx)
{
	rowtop = 0;
	if(fs->crow >= fs->row_count - 1)
	{
		if(config.pagetonext && fs_is_txtbook((t_fs_filetype)filelist[*selidx].data))
		{
			dword orgidx = *selidx;
			do {
				if(*selidx < filecount - 1)
					(*selidx) ++;
				else
					*selidx = 0;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[*selidx].data));
			if(*selidx != orgidx)
			{
				if(config.autobm)
					bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
				text_needrf = text_needrp = true;
				text_needrb = false;
				rrow = INVALID;
			}
		}
		else
			fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
		return 1;
	}
	fs->crow += (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
	if(fs->crow > fs->row_count - 1)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
	text_needrp = true;
	return 0;
}

int book_handle_input(dword *selidx, dword key)
{
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	if(key == 0 && config.infobar == conf_infobar_lyric)
	{
		text_needrp = true;
		text_needrb = text_needrf = false;
	} else
#endif
		if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
		{
			return exit_confirm();
		}
		else if(key == ctlkey[11] || key == ctlkey2[11] || key == CTRL_PLAYPAUSE)
		{
			scene_power_save(false);
			if(config.autobm)
				bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
			text_close(fs);
			fs = NULL;
			disp_duptocachealpha(50);
			return *selidx;
		}
		else if(key == ctlkey[0] || key == ctlkey2[0])
		{
			rrow = (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf;
			text_needrb = scene_bookmark(&rrow);
			text_needrf = text_needrb;
			text_needrp = true;
		}
#ifdef ENABLE_MUSIC
		else if(key == PSP_CTRL_START)
		{
			scene_mp3bar();
			text_needrp = true;
		}
#endif
		else if(key == PSP_CTRL_SELECT)
		{
			switch(scene_options(&*selidx))
			{
				case 2: case 4:
					rrow = (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf;
					text_needrb = text_needrf = true;
					break;
				case 3:
					rrow = fs->crow;
					text_needrf = true;
					break;
				case 1:
					scene_power_save(false);
					if(config.autobm)
						bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
					text_close(fs);
					fs = NULL;
					disp_duptocachealpha(50);
					return *selidx;
			}
			scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
			text_needrp = true;
		}
#ifdef ENABLE_ANALOG
		else if(key == CTRL_ANALOG && config.enable_analog)
		{
			int x, y;
			ctrl_analog(&x, &y);
			move_line_analog(x, y);
		}
#endif
		else if(key == ku)
		{
			move_line_up(1);
		}
		else if(key == kd)
		{
			move_line_down(1);
		}
		else if(key == ctlkey[1] || key == ctlkey2[1]) {
			if(config.vertread == 3) {
				if(move_page_down(key, selidx))
					goto next;
			}
			else {
				if(move_page_up(key, selidx))
					goto next;
			}

		}
		else if(key == kl || key == CTRL_BACK)
		{
			move_page_up(key, selidx);
		}
		else if(key == ctlkey[2] || key == ctlkey2[2]) {
			if(config.vertread == 3) {
				if(move_page_up(key, selidx))
					goto next;
			}
			else {
				if(move_page_down(key, selidx))
					goto next;
			}
		}
		else if(key == kr || key == CTRL_FORWARD)
		{
			move_page_down(key, selidx);
		}
		else if(key == ctlkey[5] || key == ctlkey2[5])
		{
			if(config.vertread == 3)
				move_line_down(500);
			else
				move_line_up(500);
		}
		else if(key == ctlkey[6] || key == ctlkey2[6])
		{
			if(config.vertread == 3)
				move_line_up(500);
			else
				move_line_down(500);
		}
		else if(key == ctlkey[3] || key == ctlkey2[3])
		{
			if(config.vertread == 3)
				move_line_down(100);
			else
				move_line_up(100);
		}
		else if(key == ctlkey[4] || key == ctlkey2[4])
		{
			if(config.vertread == 3)
				move_line_up(100);
			else
				move_line_down(100);
		}
		else if(key == ctlkey[7] || key == ctlkey2[7])
		{
			if(config.vertread == 3)
				move_line_to_end();
			else
				move_line_to_start();
		}
		else if(key == ctlkey[8] || key == ctlkey2[8])
		{
			if(config.vertread == 3)
				move_line_to_start();
			else
				move_line_to_end();
		}
		else if(key == ctlkey[9] || key == ctlkey2[9])
		{
			dword orgidx = *selidx;
			if(config.autobm)
				bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
			do {
				if(*selidx > 0)
					(*selidx) --;
				else
					*selidx = filecount - 1;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[*selidx].data));
			if(*selidx != orgidx)
			{
				if(config.autobm)
					bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
				text_needrf = text_needrp = true;
				text_needrb = false;
				rrow = INVALID;
			}
		}
		else if(key == ctlkey[10] || key == ctlkey2[10])
		{
			dword orgidx = *selidx;
			do {
				if(*selidx < filecount - 1)
					(*selidx) ++;
				else
					*selidx = 0;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[*selidx].data));
			if(*selidx != orgidx)
			{
				if(config.autobm)
					bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
				text_needrf = text_needrp = true;
				text_needrb = false;
				rrow = INVALID;
			}
		}
		else if(key == ctlkey[12] || key == ctlkey2[12]) {
			if(config.autopagetype == 2) {
				config.autopagetype = autopage_prevsetting;
			}
			else {
				autopage_prevsetting = config.autopagetype;
				config.autopagetype = 2;
			}
			text_needrp = true;
		}
		else
			text_needrp = text_needrb = text_needrf = false;
	// reset ticks
	ticks = 0;
next:
	return -1;
}

dword scene_readbook(dword selidx)
{
	rrow = INVALID;
	rowtop = 0;
	trow = NULL;
	text_needrf = true, text_needrp = true, text_needrb = false;
	scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
	while(1)
	{
		if(text_needrf)
		{
			if(scene_book_reload(selidx)) {
				return selidx;
			}
			text_needrf = false;
		}
		if(text_needrp)
		{
			scene_printbook(selidx);
			text_needrp = false;
		}
		
		int delay = 20000;
		dword key;
		while((key = ctrl_read()) == 0) 
		{
			sceKernelDelayThread(delay);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if(config.infobar == conf_infobar_lyric && lyric_check_changed(mp3_get_lyric())) 
			{
				break;
			}
#endif
			if(config.autopage) {
				if(config.autopagetype != 2) {
					if(config.autopagetype == 1) {
						if(++ticks >= config.autolinedelay) {
							ticks = 0;
							move_line_smooth(config.autopage);
							// prevent LCD shut down by setting counter = 0
							scePowerTick(0);
							goto redraw;
							delay = 0;
						}
					}
					else {
						if(++ticks >= 50 * abs(config.autopage)) {
							ticks = 0;
							if(config.autopage > 0)
								move_page_down(key, &selidx);
							else 
								move_page_up(key, &selidx);
							// prevent LCD shut down by setting counter = 0
							scePowerTick(0);
							goto redraw;
							delay = 20000;
						}
					}
				}
			}
			if(config.dis_scrsave) {
				scePowerTick(0);
			}
		}
		int ret = book_handle_input(&selidx, key);
		if(ret != -1) {
			scene_power_save(true);
			return ret;
		}
redraw:
		;
	}
	scene_power_save(false);
	if(config.autobm)
		bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
	text_close(fs);
	fs = NULL;
	disp_duptocachealpha(50);
	scene_power_save(true);
	return selidx;
}
