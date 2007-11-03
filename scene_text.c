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
#include "common/log.h"
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"

dword ctlkey[12], ctlkey2[12], ku, kd, kl, kr;
dword cidx, rrow = INVALID;
int ticks = 0;
bool nextpage = false;
int rowtop = 0;
char tr[8], * trow = NULL;
static bool needrf = true, needrp = true, needrb = false;
char filename[256], archname[256];

int scene_book_reload(dword selidx)
{
	if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
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
			needrb = true;
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
	else if(where == scene_in_chm)
		fs = text_open_in_chm(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if(where == scene_in_rar)
		fs = text_open_in_rar(config.shortpath, filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	else if((t_fs_filetype)filelist[selidx].data == fs_filetype_unknown)
		fs = text_open_binary(filename, config.vertread);
	else
		fs = text_open(filename, (t_fs_filetype)filelist[selidx].data, pixelsperrow, config.wordspace, config.encode, config.reordertxt);
	if(fs == NULL)
	{
		disp_duptocachealpha(50);
		return 1;
	}
	if(needrb && (t_fs_filetype)filelist[selidx].data != fs_filetype_unknown)
	{
		rowtop = 0;
		fs->crow = 1;
		while(fs->crow < fs->row_count && (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf <= rrow)
			fs->crow ++;
		fs->crow --;
		needrb = false;
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
	char ci[8] = "       ", cr[300];
	disp_waitv();
#ifdef ENABLE_BG
	if(!bg_display())
#endif
		disp_fillvram(config.bgcolor);
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);
	switch(config.vertread)
	{
		case 2:
			disp_putnstringlvert(config.borderspace, 271 - config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				disp_putnstringlvert((DISP_BOOK_FONTSIZE + config.rowspace) * cidx + config.borderspace - rowtop, 271 - config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (479 -  DISP_BOOK_FONTSIZE) : PSP_SCREEN_WIDTH);
			}
			if(config.infobar == conf_infobar_info)
			{
				char soffset[8] = "        ";
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				utils_dword2string(offset, soffset, 7);
				disp_line(479 - DISP_BOOK_FONTSIZE, 0, 479 - DISP_BOOK_FONTSIZE, 271, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				sprintf(cr, "%s/%s  %s  %s %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, soffset);
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
					disp_putnstringlvert(PSP_SCREEN_WIDTH - DISP_BOOK_FONTSIZE, 271 - (136 - ss[0] * DISP_BOOK_FONTSIZE / 4), config.forecolor, (const byte *)ls[0], ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
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
				char soffset[8] = "        ";
				int i;
				offset = 0;
				for(i=0; i<fs->crow && i<fs->row_count; ++i) {
					p_textrow tr;
					tr = fs->rows[i >> 10] + (i & 0x3FF);
					offset += tr->count;
				}
				offset = offset / 1023 + 1;
				utils_dword2string(offset, soffset, 7);
				disp_line(DISP_BOOK_FONTSIZE, 0, DISP_BOOK_FONTSIZE, 271, config.forecolor);
				utils_dword2string(fs->crow + 1, ci, 7);
				sprintf(cr, "%s/%s  %s  %s %s", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, soffset);
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
					disp_putnstringrvert(DISP_BOOK_FONTSIZE - 1, (136 - ss[0] * DISP_BOOK_FONTSIZE / 4), config.forecolor, (const byte *)ls[0], ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
			break;
		default:
			disp_putnstringhorz(config.borderspace, config.borderspace, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, rowtop, DISP_BOOK_FONTSIZE - rowtop, 0);
			for(cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx ++)
			{
				tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
				disp_putnstringhorz(config.borderspace, config.borderspace + (DISP_BOOK_FONTSIZE + config.rowspace) * cidx - rowtop, config.forecolor, (const byte *)tr->start, (int)tr->count, config.wordspace, 0, DISP_BOOK_FONTSIZE, config.infobar ? (271 - DISP_BOOK_FONTSIZE) : PSP_SCREEN_HEIGHT);
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
				sprintf(cr, "%s/%s  %s  %s  GI: %d", ci, trow, (fs->ucs == 2) ? "UTF-8" : (fs->ucs == 1 ? "UCS " : conf_get_encodename(config.encode)), filelist[selidx].compname, offset);
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
					disp_putnstringhorz((240 - ss[0] * DISP_BOOK_FONTSIZE / 4), PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE, config.forecolor, (const byte *)ls[0], ss[0], 0, 0, DISP_BOOK_FONTSIZE, 0);
				}
			}
#endif
	}
	if(config.scrollbar)
	{
		switch(config.vertread)
		{
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

int book_handle_input(dword *selidx, dword key)
{
	if(nextpage) {
		nextpage = false;
		rowtop = 0;
		if(fs->crow >= fs->row_count - 1)
		{
			if(config.pagetonext && key != kr && fs_is_txtbook((t_fs_filetype)filelist[*selidx].data))
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
					needrf = needrp = true;
					needrb = false;
					rrow = INVALID;
				}
			}
			else
				fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
			goto next;
		}
		fs->crow += (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
		if(fs->crow > fs->row_count - 1)
			fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
		needrp = true;
	} else
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
		if(key == 0 && config.infobar == conf_infobar_lyric)
		{
			needrp = true;
			needrb = needrf = false;
		} else
#endif
			if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
			{
				if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
				{
					scene_exit();
					return win_menu_op_continue;
				}
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
				needrb = scene_bookmark(&rrow);
				needrf = needrb;
				needrp = true;
			}
#ifdef ENABLE_MUSIC
			else if(key == PSP_CTRL_START)
			{
				scene_mp3bar();
				needrp = true;
			}
#endif
			else if(key == PSP_CTRL_SELECT)
			{
				switch(scene_options(&*selidx))
				{
					case 2: case 4:
						rrow = (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf;
						needrb = needrf = true;
						break;
					case 3:
						rrow = fs->crow;
						needrf = true;
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
				needrp = true;
			}
#ifdef ENABLE_ANALOG
			else if(key == CTRL_ANALOG)
			{
				int x, y;
				ctrl_analog(&x, &y);
				switch(config.vertread)
				{
					case 2:
						rowtop += x / 8;
						break;
					case 1:
						rowtop -= x / 8;
						break;
					default:
						rowtop += y / 8;
						break;
				}
				if(rowtop < 0)
				{
					while(fs->crow > 0 && rowtop < 0)
					{
						rowtop += DISP_BOOK_FONTSIZE;
						fs->crow --;
					}
				}
				else if(rowtop >= DISP_BOOK_FONTSIZE)
				{
					while(fs->crow < fs->row_count - 1 && rowtop >= DISP_BOOK_FONTSIZE)
					{
						rowtop -= DISP_BOOK_FONTSIZE;
						fs->crow ++;
					}
				}
				if(rowtop < 0 || rowtop >= DISP_BOOK_FONTSIZE)
					rowtop = 0;
				needrp = true;
			}
#endif
			else if(key == ku)
			{
				rowtop = 0;
				if (fs->crow > 0)
					fs->crow --;
				needrp = true;
			}
			else if(key == kd)
			{
				rowtop = 0;
				if(fs->crow >= fs->row_count - 1)
					fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
				else
					fs->crow ++;
				needrp = true;
			}
			else if(key == ctlkey[1] || key == ctlkey2[1] || key == kl || key == CTRL_BACK)
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
							needrf = needrp = true;
							needrb = false;
							rrow = INVALID;
						}
					}
					goto next;
				}
				if (fs->crow > (config.rlastrow ? (rowsperpage - 1) : rowsperpage))
					fs->crow -= (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
				else
					fs->crow = 0;
				needrp = true;
			}
			else if(key == ctlkey[2] || key == ctlkey2[2] || key == kr || key == CTRL_FORWARD)
			{
				rowtop = 0;
				if(fs->crow >= fs->row_count - 1)
				{
					if(config.pagetonext && key != kr && fs_is_txtbook((t_fs_filetype)filelist[*selidx].data))
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
							needrf = needrp = true;
							needrb = false;
							rrow = INVALID;
						}
					}
					else
						fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
					goto next;
				}
				fs->crow += (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
				if(fs->crow > fs->row_count - 1)
					fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
				needrp = true;
			}
			else if(key == ctlkey[5] || key == ctlkey2[5])
			{
				rowtop = 0;
				if (fs->crow > 500)
					fs->crow -= 500;
				else
					fs->crow = 0;
				needrp = true;
			}
			else if(key == ctlkey[6] || key == ctlkey2[6])
			{
				rowtop = 0;
				fs->crow += 500;
				if(fs->crow > fs->row_count - 1)
					fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
				needrp = true;
			}
			else if(key == ctlkey[3] || key == ctlkey2[3])
			{
				rowtop = 0;
				if (fs->crow > 100)
					fs->crow -= 100;
				else
					fs->crow = 0;
				needrp = true;
			}
			else if(key == ctlkey[4] || key == ctlkey2[4])
			{
				rowtop = 0;
				fs->crow += 100;
				if (fs->crow >= fs->row_count - 1)
					fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
				needrp = true;
			}
			else if(key == ctlkey[7] || key == ctlkey2[7])
			{
				rowtop = 0;
				if(fs->crow != 0)
				{
					fs->crow = 0;
					needrp = true;
				}
			}
			else if(key == ctlkey[8] || key == ctlkey2[8])
			{
				rowtop = 0;
				fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
				needrp = true;
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
					needrf = needrp = true;
					needrb = false;
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
					needrf = needrp = true;
					needrb = false;
					rrow = INVALID;
				}
			}
			else
				needrp = needrb = needrf = false;
		// reset ticks
		ticks = 0;
next:
		return -1;
}

dword scene_readbook(dword selidx)
{
	rrow = INVALID;
	ticks = 0;
	nextpage = false;
	rowtop = 0;
	trow = NULL;
	needrf = true, needrp = true, needrb = false;
	scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
	while(1)
	{
		if(needrf)
		{
			if(scene_book_reload(selidx)) {
				return selidx;
			}
			needrf = false;
		}
		if(needrp)
		{
			scene_printbook(selidx);
			needrp = false;
		}
		
		dword key;
		while((key = ctrl_read()) == 0) 
		{
			sceKernelDelayThread(20000);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if(config.infobar == conf_infobar_lyric && lyric_check_changed(mp3_get_lyric())) 
			{
				break;
			}
#endif
			if(config.autopage && ++ticks >= 50 * config.autopage) {
				ticks = 0;
				nextpage = true;
				break;
			}
		}
		int ret = book_handle_input(&selidx, key);
		if(ret != -1)
			return ret;
	}
	scene_power_save(false);
	if(config.autobm)
		bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
	text_close(fs);
	fs = NULL;
	disp_duptocachealpha(50);
	return selidx;
}
