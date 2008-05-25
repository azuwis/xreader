/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

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
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"
#include "dbg.h"
#include "simple_gettext.h"
#include "xrPrx/xrPrx.h"
#include "osk.h"

dword ctlkey[14], ctlkey2[14], ku, kd, kl, kr;
static volatile int ticks = 0, secticks = 0;
char g_titlename[256];
bool scene_readbook_in_raw_mode = false;

BookViewData cur_book_view, prev_book_view;

#ifdef _DEBUG
static void WriteByte(int fd, unsigned char b)
{
	sceIoWrite(fd, &b, 1);
}

static inline int calc_gi()
{
	int i = min(fs->crow, fs->row_count);
	p_textrow tr = fs->rows[i >> 10] + (i & 0x3FF);

	return tr->GI;
}

void ScreenShot()
{
	const char tgaHeader[] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	const int width = PSP_SCREEN_WIDTH;
	const int lineWidth = PSP_SCREEN_SCANLINE;
	const int height = PSP_SCREEN_HEIGHT;
	unsigned char lineBuffer[width * 4];
	u32 *vram = disp_get_vaddr(0, 0);
	int x, y;
	char filename[PATH_MAX];
	int i = 0;

	do {
		SPRINTF_S(filename, "ms0:/screenshot%02d.tga", i++);
	} while (utils_is_file_exists(filename));
	int fd =
		sceIoOpen(filename, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
	if (!fd)
		return;
	sceIoWrite(fd, tgaHeader, sizeof(tgaHeader));
	WriteByte(fd, width & 0xff);
	WriteByte(fd, width >> 8);
	WriteByte(fd, height & 0xff);
	WriteByte(fd, height >> 8);
	WriteByte(fd, 24);
	WriteByte(fd, 0);
	for (y = height - 1; y >= 0; y--) {
		for (x = 0; x < width; x++) {
			u32 color = vram[y * lineWidth + x];
			unsigned char red = color & 0xff;
			unsigned char green = (color >> 8) & 0xff;
			unsigned char blue = (color >> 16) & 0xff;

			lineBuffer[3 * x] = blue;
			lineBuffer[3 * x + 1] = green;
			lineBuffer[3 * x + 2] = red;
		}
		sceIoWrite(fd, lineBuffer, width * 3);
	}
	sceIoClose(fd);
}
#endif

#ifdef ENABLE_TTF
static void draw_infobar_info_ttf(PBookViewData pView, dword selidx,
								  int vertread)
{
	char ci[8] = "       ";
	char cr[512];

	switch (vertread) {
		case conf_vertread_reversal:
			disp_line(0, DISP_BOOK_FONTSIZE, (PSP_SCREEN_WIDTH - 1),
					  DISP_BOOK_FONTSIZE, config.forecolor);
			break;
		case conf_vertread_lvert:
			disp_line((PSP_SCREEN_WIDTH - 1) - DISP_BOOK_FONTSIZE, 0,
					  (PSP_SCREEN_WIDTH - 1) - DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_rvert:
			disp_line(DISP_BOOK_FONTSIZE, 0, DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_horz:
			disp_line(0, (PSP_SCREEN_HEIGHT - 1) - DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_WIDTH - 1),
					  (PSP_SCREEN_HEIGHT - 1) - DISP_BOOK_FONTSIZE,
					  config.forecolor);
			break;
		default:
			break;
	}
	utils_dword2string(fs->crow + 1, ci, 7);
	char autopageinfo[80];

	if (config.autopagetype == 2)
		autopageinfo[0] = 0;
	else if (config.autopagetype == 1) {
		if (config.autopage != 0)
			SPRINTF_S(autopageinfo, _("%s: 时间 %d 速度 %d"), _("自动滚屏"),
					  config.autolinedelay, config.autopage);
		else
			autopageinfo[0] = 0;
	} else {
		if (config.autopage != 0) {
			SPRINTF_S(autopageinfo, _("自动翻页: 时间 %d"), config.autopage);
		} else
			autopageinfo[0] = 0;
	}
	if (where == scene_in_chm) {
		char fname[PATH_MAX];

		charsets_utf8_conv((unsigned char *) filelist[selidx].
						   compname, (unsigned char *) fname);
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  fname, calc_gi(), autopageinfo);
	} else if (scene_readbook_in_raw_mode == true) {
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  g_titlename, calc_gi(), autopageinfo);
	} else {
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  filelist[selidx].compname->ptr, calc_gi(), autopageinfo);
	}
	int wordspace = 0;

	switch (vertread) {
		case conf_vertread_reversal:
			disp_putnstringreversal(0,
									PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE - 1,
									config.forecolor, (const byte *) cr,
									960 / DISP_BOOK_FONTSIZE, wordspace, 0,
									DISP_BOOK_FONTSIZE, 0);
			break;
		case conf_vertread_lvert:
			disp_putnstringlvert(PSP_SCREEN_WIDTH - DISP_BOOK_FONTSIZE - 1,
								 (PSP_SCREEN_HEIGHT - 1), config.forecolor,
								 (const byte *) cr, 544 / DISP_BOOK_FONTSIZE,
								 wordspace, 0, DISP_BOOK_FONTSIZE, 0);
			break;
		case conf_vertread_rvert:
			disp_putnstringrvert(DISP_BOOK_FONTSIZE, 0,
								 config.forecolor, (const byte *) cr,
								 544 / DISP_BOOK_FONTSIZE, wordspace, 0,
								 DISP_BOOK_FONTSIZE, 1);
			break;
		case conf_vertread_horz:
			disp_putnstringhorz(0, PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE - 1,
								config.forecolor, (const byte *) cr,
								960 / DISP_BOOK_FONTSIZE, wordspace, 0,
								DISP_BOOK_FONTSIZE, 0);
			break;
		default:
			break;
	}
}
#endif

static void draw_infobar_info(PBookViewData pView, dword selidx, int vertread)
{
	char ci[8] = "       ";
	char cr[512];

	switch (vertread) {
		case conf_vertread_reversal:
			disp_line(0, DISP_FONTSIZE, (PSP_SCREEN_WIDTH - 1), DISP_FONTSIZE,
					  config.forecolor);
			break;
		case conf_vertread_lvert:
			disp_line((PSP_SCREEN_WIDTH - 1) - DISP_FONTSIZE, 0,
					  (PSP_SCREEN_WIDTH - 1) - DISP_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_rvert:
			disp_line(DISP_FONTSIZE, 0, DISP_FONTSIZE, (PSP_SCREEN_HEIGHT - 1),
					  config.forecolor);
			break;
		case conf_vertread_horz:
			disp_line(0, (PSP_SCREEN_HEIGHT - 1) - DISP_FONTSIZE,
					  (PSP_SCREEN_WIDTH - 1),
					  (PSP_SCREEN_HEIGHT - 1) - DISP_FONTSIZE,
					  config.forecolor);
			break;
		default:
			break;
	}
	utils_dword2string(fs->crow + 1, ci, 7);
	char autopageinfo[80];

	if (config.autopagetype == 2)
		autopageinfo[0] = 0;
	else if (config.autopagetype == 1) {
		if (config.autopage != 0)
			SPRINTF_S(autopageinfo, _("%s: 时间 %d 速度 %d"), _("自动滚屏"),
					  config.autolinedelay, config.autopage);
		else
			autopageinfo[0] = 0;
	} else {
		if (config.autopage != 0) {
			SPRINTF_S(autopageinfo, _("自动翻页: 时间 %d"), config.autopage);
		} else
			autopageinfo[0] = 0;
	}
	if (where == scene_in_chm) {
		char fname[PATH_MAX];

		charsets_utf8_conv((unsigned char *) filelist[selidx].
						   compname, (unsigned char *) fname);
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  fname, calc_gi(), autopageinfo);
	} else if (scene_readbook_in_raw_mode == true) {
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  g_titlename, calc_gi(), autopageinfo);
	} else {
		SPRINTF_S(cr, "%s/%s  %s  %s  GI: %d  %s", ci, pView->trow,
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename
											  (config.encode)),
				  filelist[selidx].compname->ptr, calc_gi(), autopageinfo);
	}
	int wordspace = (DISP_FONTSIZE == 10 ? 1 : 0);

	switch (vertread) {
		case conf_vertread_reversal:
			disp_putnstringreversal_sys(0,
										PSP_SCREEN_HEIGHT - DISP_FONTSIZE,
										config.forecolor, (const byte *) cr,
										960 / DISP_FONTSIZE, wordspace, 0,
										DISP_FONTSIZE, 0);
			break;
		case conf_vertread_lvert:
			disp_putnstringlvert_sys(PSP_SCREEN_WIDTH - DISP_FONTSIZE,
									 (PSP_SCREEN_HEIGHT - 1), config.forecolor,
									 (const byte *) cr, 544 / DISP_FONTSIZE,
									 wordspace, 0, DISP_FONTSIZE, 0);
			break;
		case conf_vertread_rvert:
			disp_putnstringrvert_sys(DISP_FONTSIZE - 1, 0,
									 config.forecolor, (const byte *) cr,
									 544 / DISP_FONTSIZE, wordspace, 0,
									 DISP_FONTSIZE, 0);
			break;
		case conf_vertread_horz:
			disp_putnstringhorz_sys(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE,
									config.forecolor, (const byte *) cr,
									960 / DISP_FONTSIZE, wordspace, 0,
									DISP_FONTSIZE, 0);
			break;
		default:
			break;
	}
}

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
#ifdef ENABLE_TTF
static void draw_infobar_lyric_ttf(PBookViewData pView, dword selidx,
								   int vertread)
{
	switch (vertread) {
		case conf_vertread_reversal:
			disp_line(PSP_SCREEN_WIDTH - 1, DISP_BOOK_FONTSIZE,
					  0, DISP_BOOK_FONTSIZE, config.forecolor);
			break;
		case conf_vertread_lvert:
			disp_line((PSP_SCREEN_WIDTH - 1) - DISP_BOOK_FONTSIZE, 0,
					  (PSP_SCREEN_WIDTH - 1) - DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_rvert:
			disp_line(DISP_BOOK_FONTSIZE, 0, DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_horz:
			disp_line(0, (PSP_SCREEN_HEIGHT - 1) - DISP_BOOK_FONTSIZE,
					  (PSP_SCREEN_WIDTH - 1),
					  (PSP_SCREEN_HEIGHT - 1) - DISP_BOOK_FONTSIZE,
					  config.forecolor);
			break;
		default:
			break;
	}

	const char *ls[1];
	dword ss[1];
	int wordspace = 0;

	if (lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss)
		&& ls[0] != NULL) {
		if (ss[0] > 960 / DISP_BOOK_FONTSIZE)
			ss[0] = 960 / DISP_BOOK_FONTSIZE;
		char t[BUFSIZ];

		lyric_decode(ls[0], t, &ss[0]);
		switch (vertread) {
			case conf_vertread_reversal:
				disp_putnstringreversal((240 -
										 ss[0] * DISP_BOOK_FONTSIZE / 4),
										PSP_SCREEN_HEIGHT -
										DISP_BOOK_FONTSIZE - 1,
										config.forecolor, (const byte *) t,
										ss[0], wordspace, 0,
										DISP_BOOK_FONTSIZE, 0);
				break;
			case conf_vertread_lvert:
				disp_putnstringlvert(PSP_SCREEN_WIDTH - DISP_BOOK_FONTSIZE -
									 1,
									 (PSP_SCREEN_HEIGHT - 1) - (136 -
																ss[0] *
																DISP_BOOK_FONTSIZE
																/ 4),
									 config.forecolor, (const byte *) t,
									 ss[0], wordspace, 0,
									 DISP_BOOK_FONTSIZE, 0);
				break;
			case conf_vertread_rvert:
				disp_putnstringrvert(DISP_BOOK_FONTSIZE,
									 (136 - ss[0] * DISP_BOOK_FONTSIZE / 4),
									 config.forecolor, (const byte *) t,
									 ss[0], wordspace, 0,
									 DISP_BOOK_FONTSIZE, 0);
				break;
			case conf_vertread_horz:
				disp_putnstringhorz((240 - ss[0] * DISP_BOOK_FONTSIZE / 4),
									PSP_SCREEN_HEIGHT - DISP_BOOK_FONTSIZE -
									1, config.forecolor, (const byte *) t,
									ss[0], wordspace, 0, DISP_BOOK_FONTSIZE, 0);
				break;
			default:
				break;
		}
	}
}
#endif

static void draw_infobar_lyric(PBookViewData pView, dword selidx, int vertread)
{
	switch (vertread) {
		case conf_vertread_reversal:
			disp_line(PSP_SCREEN_WIDTH - 1, DISP_FONTSIZE,
					  0, DISP_FONTSIZE, config.forecolor);
			break;
		case conf_vertread_lvert:
			disp_line((PSP_SCREEN_WIDTH - 1) - DISP_FONTSIZE, 0,
					  (PSP_SCREEN_WIDTH - 1) - DISP_FONTSIZE,
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_rvert:
			disp_line(DISP_FONTSIZE, 0, DISP_FONTSIZE, (PSP_SCREEN_HEIGHT - 1),
					  config.forecolor);
			break;
		case conf_vertread_horz:
			disp_line(0, (PSP_SCREEN_HEIGHT - 1) - DISP_FONTSIZE,
					  (PSP_SCREEN_WIDTH - 1),
					  (PSP_SCREEN_HEIGHT - 1) - DISP_FONTSIZE,
					  config.forecolor);
			break;
		default:
			break;
	}

	const char *ls[1];
	dword ss[1];
	int wordspace = (DISP_FONTSIZE == 10 ? 1 : 0);

	if (lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss)
		&& ls[0] != NULL) {
		if (ss[0] > 960 / DISP_FONTSIZE)
			ss[0] = 960 / DISP_FONTSIZE;
		char t[BUFSIZ];

		lyric_decode(ls[0], t, &ss[0]);
		switch (vertread) {
			case conf_vertread_reversal:
				disp_putnstringreversal_sys((240 -
											 ss[0] * DISP_FONTSIZE / 4),
											PSP_SCREEN_HEIGHT -
											DISP_FONTSIZE,
											config.forecolor, (const byte *) t,
											ss[0], wordspace, 0, DISP_FONTSIZE,
											0);
				break;
			case conf_vertread_lvert:
				disp_putnstringlvert_sys(PSP_SCREEN_WIDTH - DISP_FONTSIZE,
										 (PSP_SCREEN_HEIGHT - 1) - (136 -
																	ss[0] *
																	DISP_FONTSIZE
																	/ 4),
										 config.forecolor, (const byte *) t,
										 ss[0], wordspace, 0, DISP_FONTSIZE, 0);
				break;
			case conf_vertread_rvert:
				disp_putnstringrvert_sys(DISP_FONTSIZE - 1,
										 (136 - ss[0] * DISP_FONTSIZE / 4),
										 config.forecolor, (const byte *) t,
										 ss[0], wordspace, 0, DISP_FONTSIZE, 0);
				break;
			case conf_vertread_horz:
				disp_putnstringhorz_sys((240 - ss[0] * DISP_FONTSIZE / 4),
										PSP_SCREEN_HEIGHT - DISP_FONTSIZE,
										config.forecolor, (const byte *) t,
										ss[0], wordspace, 0, DISP_FONTSIZE, 0);
				break;
			default:
				break;
		}
	}
}
#endif

void copy_book_view(PBookViewData dst, const PBookViewData src)
{
	if (!dst || !src)
		return;

	memcpy(dst, src, sizeof(BookViewData));
}

PBookViewData new_book_view(PBookViewData p)
{
	if (!p) {
		p = calloc(1, sizeof(BookViewData));
	}

	p->rrow = INVALID;
	p->rowtop = 0;
	memset(p->tr, 0, 8);
	p->trow = NULL;
	p->text_needrf = p->text_needrp = true;
	p->text_needrb = false;
	p->filename[0] = p->archname[0] = p->bookmarkname[0] = '\0';

	return p;
}

int scene_book_reload(PBookViewData pView, dword selidx)
{
	if (where == scene_in_zip || where == scene_in_chm || where == scene_in_rar) {
		STRCPY_S(pView->filename, filelist[selidx].compname->ptr);
		STRCPY_S(pView->bookmarkname, config.shortpath);
		STRCPY_S(pView->archname, config.shortpath);
		if (config.shortpath[strlen(config.shortpath) - 1] != '/' &&
			pView->filename[0] != '/')
			STRCAT_S(pView->bookmarkname, "/");
		STRCAT_S(pView->bookmarkname, pView->filename);
	} else {
		STRCPY_S(pView->filename, config.shortpath);
		STRCAT_S(pView->filename, filelist[selidx].shortname->ptr);
		STRCPY_S(pView->archname, pView->filename);
		STRCPY_S(pView->bookmarkname, pView->filename);
	}
	dbg_printf(d, "scene_book_reload: fn %s bookmarkname %s archname %s",
			   pView->filename, pView->bookmarkname, pView->archname);
	if (pView->rrow == INVALID) {
		// disable binary file type text's bookmark
		if (config.autobm
			&& (t_fs_filetype) filelist[selidx].data != fs_filetype_unknown) {
			pView->rrow = bookmark_autoload(pView->bookmarkname);
			pView->text_needrb = true;
		} else
			pView->rrow = 0;
	}
	if (fs != NULL) {
		text_close(fs);
		fs = NULL;
	}
	scene_power_save(false);

	extern bool g_force_text_view_mode;

	if (g_force_text_view_mode == false)
		fs = text_open_archive(pView->filename, pView->archname,
							   (t_fs_filetype) filelist[selidx].data,
							   pixelsperrow, config.wordspace, config.encode,
							   config.reordertxt, where, config.vertread);
	else
		fs = text_open_archive(pView->filename, pView->archname,
							   fs_filetype_txt,
							   pixelsperrow, config.wordspace, config.encode,
							   config.reordertxt, where, config.vertread);

	if (fs == NULL) {
		win_msg(_("文件打开失败"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		dbg_printf(d, _("scene_book_reload: 文件%s打开失败 where=%d"),
				   pView->filename, where);
		scene_power_save(true);
		return 1;
	}
	if (pView->text_needrb
		&& (t_fs_filetype) filelist[selidx].data != fs_filetype_unknown) {
		pView->rowtop = 0;
		fs->crow = 1;
		while (fs->crow < fs->row_count
			   && (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start -
			   fs->buf <= pView->rrow)
			fs->crow++;
		fs->crow--;
		pView->text_needrb = false;
	} else
		fs->crow = pView->rrow;

	if (fs->crow >= fs->row_count)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;

	pView->trow = &pView->tr[utils_dword2string(fs->row_count, pView->tr, 7)];
	STRCPY_S(config.lastfile, filelist[selidx].compname->ptr);
	scene_power_save(true);
	return 0;
}

static void scene_printtext_reversal(PBookViewData pView)
{
	int cidx;
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);

	disp_putnstringreversal(config.borderspace, config.borderspace,
							config.forecolor, (const byte *) tr->start,
							(int) tr->count, config.wordspace,
							pView->rowtop,
							DISP_BOOK_FONTSIZE - pView->rowtop, 0);
	for (cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx++) {
		tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
		disp_putnstringreversal(config.borderspace,
								config.borderspace +
								(DISP_BOOK_FONTSIZE +
								 config.rowspace) * cidx -
								pView->rowtop, config.forecolor,
								(const byte *) tr->start,
								(int) tr->count, config.wordspace, 0,
								DISP_BOOK_FONTSIZE,
								config.infobar ? PSP_SCREEN_HEIGHT -
#ifdef ENABLE_TTF
								(config.infobar_use_ttf_mode ?
								 DISP_BOOK_FONTSIZE : DISP_FONTSIZE) :
#else
	  DISP_FONTSIZE:
#endif
								PSP_SCREEN_HEIGHT);
	}
}

static void scene_printtext_rvert(PBookViewData pView)
{
	int cidx;
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);

	disp_putnstringrvert((PSP_SCREEN_WIDTH - 1) - config.borderspace,
						 config.borderspace, config.forecolor,
						 (const byte *) tr->start, (int) tr->count,
						 config.wordspace, pView->rowtop,
						 DISP_BOOK_FONTSIZE - pView->rowtop, 0);
	for (cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx++) {
		tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
		disp_putnstringrvert((PSP_SCREEN_WIDTH - 1) -
							 (DISP_BOOK_FONTSIZE +
							  config.rowspace) * cidx -
							 config.borderspace + pView->rowtop,
							 config.borderspace, config.forecolor,
							 (const byte *) tr->start, (int) tr->count,
							 config.wordspace, 0, DISP_BOOK_FONTSIZE,
							 config.infobar ?
#ifdef ENABLE_TTF
							 (config.
							  infobar_use_ttf_mode ? DISP_BOOK_FONTSIZE :
							  DISP_FONTSIZE) + 1
#else
							 DISP_FONTSIZE + 1
#endif
							 : 1);
	}
}

static void scene_printtext_lvert(PBookViewData pView)
{
	int cidx;
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);

	disp_putnstringlvert(config.borderspace,
						 (PSP_SCREEN_HEIGHT - 1) - config.borderspace,
						 config.forecolor, (const byte *) tr->start,
						 (int) tr->count, config.wordspace,
						 pView->rowtop, DISP_BOOK_FONTSIZE - pView->rowtop, 0);
	for (cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx++) {
		tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
		disp_putnstringlvert((DISP_BOOK_FONTSIZE +
							  config.rowspace) * cidx +
							 config.borderspace - pView->rowtop,
							 (PSP_SCREEN_HEIGHT - 1) -
							 config.borderspace, config.forecolor,
							 (const byte *) tr->start, (int) tr->count,
							 config.wordspace, 0, DISP_BOOK_FONTSIZE,
							 config.infobar ? (PSP_SCREEN_WIDTH - 1) -
#ifdef ENABLE_TTF
							 (config.infobar_use_ttf_mode ?
							  DISP_BOOK_FONTSIZE : DISP_FONTSIZE) :
#else
	  DISP_FONTSIZE:
#endif
							 PSP_SCREEN_WIDTH);
	}

}

static void scene_printtext_horz(PBookViewData pView)
{
	int cidx;
	p_textrow tr = fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF);

	disp_putnstringhorz(config.borderspace, config.borderspace,
						config.forecolor, (const byte *) tr->start,
						(int) tr->count, config.wordspace,
						pView->rowtop, DISP_BOOK_FONTSIZE - pView->rowtop, 0);
	for (cidx = 1; cidx < drperpage && fs->crow + cidx < fs->row_count; cidx++) {
		tr = fs->rows[(fs->crow + cidx) >> 10] + ((fs->crow + cidx) & 0x3FF);
		disp_putnstringhorz(config.borderspace,
							config.borderspace +
							(DISP_BOOK_FONTSIZE +
							 config.rowspace) * cidx -
							pView->rowtop, config.forecolor,
							(const byte *) tr->start,
							(int) tr->count, config.wordspace, 0,
							DISP_BOOK_FONTSIZE,
							config.infobar ? (PSP_SCREEN_HEIGHT - 1) -
#ifdef ENABLE_TTF
							(config.infobar_use_ttf_mode ?
							 DISP_BOOK_FONTSIZE : DISP_FONTSIZE) :
#else
	  DISP_FONTSIZE:
#endif
							PSP_SCREEN_HEIGHT);
	}
}

static void scene_draw_scrollbar_reversal(void)
{
	dword slen = config.infobar ? (270 - DISP_BOOK_FONTSIZE)
		: (PSP_SCREEN_HEIGHT - 1), bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_line(4, PSP_SCREEN_HEIGHT - 1, 4,
			  PSP_SCREEN_HEIGHT - 1 - slen, config.forecolor);
	disp_fillrect(3, PSP_SCREEN_HEIGHT - 1 - startp,
				  0, PSP_SCREEN_HEIGHT - 1 - endp, config.forecolor);
}

static void scene_draw_scrollbar_lvert(void)
{
	dword sright =
		(PSP_SCREEN_WIDTH - 1) -
		(config.infobar ? (DISP_BOOK_FONTSIZE + 1) : 0), slen =
		sright, bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_line(0, 4, sright, 4, config.forecolor);
	disp_fillrect(startp, 0, endp, 3, config.forecolor);
}

static void scene_draw_scrollbar_rvert(void)
{
	dword sleft =
		(config.infobar ? (DISP_BOOK_FONTSIZE + 1) : 0), slen =
		(PSP_SCREEN_WIDTH - 1) - sleft, bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, endp =
		(PSP_SCREEN_WIDTH - 1) - slen * fs->crow / fs->row_count, startp;
	if (endp - bsize < sleft)
		endp = sleft + bsize;
	startp = endp - bsize;
	disp_line(sleft, 267, (PSP_SCREEN_WIDTH - 1), 267, config.forecolor);
	disp_fillrect(startp, 268, endp, (PSP_SCREEN_HEIGHT - 1), config.forecolor);
}

static void scene_draw_scrollbar_horz(void)
{
	dword slen = config.infobar ? (270 - DISP_BOOK_FONTSIZE)
		: (PSP_SCREEN_HEIGHT - 1), bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_line(475, 0, 475, slen, config.forecolor);
	disp_fillrect(476, startp, (PSP_SCREEN_WIDTH - 1), endp, config.forecolor);
}

int scene_printbook(PBookViewData pView, dword selidx)
{
	disp_waitv();
#ifdef ENABLE_BG
	if (!bg_display())
#endif
		disp_fillvram(config.bgcolor);

	switch (config.vertread) {
		case conf_vertread_reversal:
			scene_printtext_reversal(pView);
			break;
		case conf_vertread_lvert:
			scene_printtext_lvert(pView);
			break;
		case conf_vertread_rvert:
			scene_printtext_rvert(pView);
			break;
		case conf_vertread_horz:
			scene_printtext_horz(pView);
			break;
		default:
			break;
	}

	if (config.infobar == conf_infobar_info) {
#ifdef ENABLE_TTF
		if (config.infobar_use_ttf_mode)
			draw_infobar_info_ttf(pView, selidx, config.vertread);
		else
#endif
			draw_infobar_info(pView, selidx, config.vertread);
	}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	else if (config.infobar == conf_infobar_lyric) {
#ifdef ENABLE_TTF
		if (config.infobar_use_ttf_mode)
			draw_infobar_lyric_ttf(pView, selidx, config.vertread);
		else
#endif
			draw_infobar_lyric(pView, selidx, config.vertread);
	}
#endif

	if (config.scrollbar) {
		switch (config.vertread) {
			case conf_vertread_reversal:
				scene_draw_scrollbar_reversal();
				break;
			case conf_vertread_lvert:
				scene_draw_scrollbar_lvert();
				break;
			case conf_vertread_rvert:
				scene_draw_scrollbar_rvert();
				break;
			case conf_vertread_horz:
				scene_draw_scrollbar_horz();
				break;
			default:
				break;
		}
	}
	disp_flip();
	return 0;
}

void move_line_up(PBookViewData pView, int line)
{
	pView->rowtop = 0;
	if (fs->crow > line)
		fs->crow -= line;
	else
		fs->crow = 0;
	pView->text_needrp = true;
}

void move_line_down(PBookViewData pView, int line)
{
	pView->rowtop = 0;
	fs->crow += line;
	if (fs->crow >= fs->row_count - 1)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
	pView->text_needrp = true;
}

void move_line_to_start(PBookViewData pView)
{
	pView->rowtop = 0;
	if (fs->crow != 0) {
		fs->crow = 0;
		pView->text_needrp = true;
	}
}

void move_line_to_end(PBookViewData pView)
{
	pView->rowtop = 0;
	if (fs->row_count > 0) {
		if (fs->crow != fs->row_count - 1) {
			fs->crow = fs->row_count - 1;
			pView->text_needrp = true;
		}
	} else {
		fs->crow = 0;
	}
}

void move_line_smooth(PBookViewData pView, int inc)
{
	pView->rowtop += inc;
	if (pView->rowtop < 0) {
		while (fs->crow > 0 && pView->rowtop < 0) {
			pView->rowtop += DISP_BOOK_FONTSIZE;
			fs->crow--;
		}
		// prevent dither when config.autopage < 5
		if (config.autopagetype == 1 && abs(config.autopage) < 5)
			pView->rowtop = DISP_BOOK_FONTSIZE + 1;
	} else if (pView->rowtop >= DISP_BOOK_FONTSIZE + config.rowspace) {
		while (fs->crow < fs->row_count - 1
			   && pView->rowtop >= DISP_BOOK_FONTSIZE + config.rowspace) {
			pView->rowtop -= DISP_BOOK_FONTSIZE;
			fs->crow++;
		}
		// prevent dither when config.autopage < 5
		if (config.autopagetype == 1 && abs(config.autopage) < 5)
			pView->rowtop = 0;
	}
	if (pView->rowtop < 0
		|| pView->rowtop >= DISP_BOOK_FONTSIZE + config.rowspace)
		pView->rowtop = 0;
	pView->text_needrp = true;
}

void move_line_analog(PBookViewData pView, int x, int y)
{
	switch (config.vertread) {
		case conf_vertread_reversal:
			move_line_smooth(pView, -y / 8);
			break;
		case conf_vertread_lvert:
			move_line_smooth(pView, x / 8);
			break;
		case conf_vertread_rvert:
			move_line_smooth(pView, -x / 8);
			break;
		case conf_vertread_horz:
			move_line_smooth(pView, y / 8);
			break;
		default:
			break;
	}
}

int move_page_up(PBookViewData pView, dword key, dword * selidx)
{
	pView->rowtop = 0;
	if (fs->crow == 0) {
		if (config.pagetonext && key != kl
			&& fs_is_txtbook((t_fs_filetype) filelist[*selidx].data)) {
			dword orgidx = *selidx;

			do {
				if (*selidx > 0)
					(*selidx)--;
				else
					*selidx = filecount - 1;
			} while (!fs_is_txtbook((t_fs_filetype) filelist[*selidx].data));
			if (*selidx != orgidx) {
				if (config.autobm)
					bookmark_autosave(pView->bookmarkname,
									  (fs->rows[fs->crow >> 10] +
									   (fs->crow & 0x3FF))->start - fs->buf);
				pView->text_needrf = pView->text_needrp = true;
				pView->text_needrb = false;
				pView->rrow = INVALID;
			}
		}
		return 1;
	}
	if (fs->crow > (config.rlastrow ? (rowsperpage - 1) : rowsperpage))
		fs->crow -= (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
	else
		fs->crow = 0;
	pView->text_needrp = true;
	return 0;
}

int move_page_down(PBookViewData pView, dword key, dword * selidx)
{
	pView->rowtop = 0;
	if (fs->crow >= fs->row_count - 1) {
		if (config.pagetonext
			&& fs_is_txtbook((t_fs_filetype) filelist[*selidx].data)) {
			dword orgidx = *selidx;

			do {
				if (*selidx < filecount - 1)
					(*selidx)++;
				else
					*selidx = 0;
			} while (!fs_is_txtbook((t_fs_filetype) filelist[*selidx].data));
			if (*selidx != orgidx) {
				if (config.autobm)
					bookmark_autosave(pView->bookmarkname,
									  (fs->rows[fs->crow >> 10] +
									   (fs->crow & 0x3FF))->start - fs->buf);
				pView->text_needrf = pView->text_needrp = true;
				pView->text_needrb = false;
				pView->rrow = INVALID;
			}
		} else
			fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
		return 1;
	}
	fs->crow += (config.rlastrow ? (rowsperpage - 1) : rowsperpage);
	if (fs->crow > fs->row_count - 1)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;
	pView->text_needrp = true;
	return 0;
}

static int scene_autopage(PBookViewData pView, dword * selidx)
{
	int key = 0;

	if (config.autopagetype != 2) {
		if (config.autopagetype == 1) {
			if (++ticks >= config.autolinedelay) {
				ticks = 0;
				move_line_smooth(&cur_book_view, config.autopage);
				// prevent LCD shut down by setting counter = 0
				scePowerTick(0);
				return 1;
			}
		} else {
			if (++ticks >= 50 * abs(config.autopage)) {
				ticks = 0;
				if (config.autopage > 0)
					move_page_down(&cur_book_view, key, selidx);
				else
					move_page_up(&cur_book_view, key, selidx);
				// prevent LCD shut down by setting counter = 0
				scePowerTick(0);
				return 1;
			}
		}
	}

	return 0;
}

static int gi_search(const void *k1, const void *k2)
{
	const p_textrow t1 = (const p_textrow) k1;
	const p_textrow t2 = (const p_textrow) k2;

	return t1->GI - t2->GI;
}

static void jump_to_gi(dword gi)
{
	dword offset = 0;
	dword i;

	for (i = 0; i < fs->row_count; ++i) {
		p_textrow tr;

		tr = fs->rows[i >> 10] + (i & 0x3FF);
		offset += tr->count;
		if (offset >= (gi - 1) * 1023) {
			cur_book_view.rowtop = 0;
			cur_book_view.rrow =
				(fs->rows[i >> 10] + (i & 0x3FF))->start - fs->buf;
			fs->crow = 1;
			while (fs->crow < fs->row_count
				   && (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start -
				   fs->buf <= cur_book_view.rrow)
				fs->crow++;
			fs->crow--;
			return;
		}
	}
	dbg_printf(d, "%s: 没有找到GI值%lu", __func__, gi);
}

int book_handle_input(PBookViewData pView, dword * selidx, dword key)
{
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	if (key == 0 && config.infobar == conf_infobar_lyric) {
		pView->text_needrp = true;
		pView->text_needrb = pView->text_needrf = false;
	} else
#endif
	if (key == (PSP_CTRL_SELECT | PSP_CTRL_START)) {
		return exit_confirm();
	} else if (key == ctlkey[11] || key == ctlkey2[11] || key == CTRL_PLAYPAUSE) {
		scene_power_save(false);
		if (config.autobm)
			bookmark_autosave(pView->bookmarkname,
							  (fs->rows[fs->crow >> 10] +
							   (fs->crow & 0x3FF))->start - fs->buf);
		text_close(fs);
		fs = NULL;
		disp_duptocachealpha(50);
		scene_power_save(true);
		return *selidx;
	} else if ((key == ctlkey[0] || key == ctlkey2[0])
			   && scene_readbook_in_raw_mode == false) {
		pView->rrow =
			(fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf;
		pView->text_needrb = scene_bookmark(&pView->rrow);
		pView->text_needrf = pView->text_needrb;
		pView->text_needrp = true;
	} else if (key == PSP_CTRL_START) {
		scene_mp3bar();
		pView->text_needrp = true;
	} else if (key == PSP_CTRL_SELECT) {
		switch (scene_options(&*selidx)) {
			case 2:
			case 4:
				pView->rrow =
					(fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start -
					fs->buf;
				pView->text_needrb = pView->text_needrf = true;
				break;
			case 3:
				pView->rrow = fs->crow;
				pView->text_needrf = true;
				break;
			case 1:
				scene_power_save(false);
				if (config.autobm)
					bookmark_autosave(pView->bookmarkname,
									  (fs->rows[fs->crow >> 10] +
									   (fs->crow & 0x3FF))->start - fs->buf);
				text_close(fs);
				fs = NULL;
				disp_duptocachealpha(50);
				scene_power_save(true);
				return *selidx;
		}
		scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
		pView->text_needrp = true;
	}
#ifdef ENABLE_ANALOG
	else if (key == CTRL_ANALOG && config.enable_analog) {
		int x, y;

		ctrl_analog(&x, &y);
		move_line_analog(&cur_book_view, x, y);
	}
#endif
	else if (key == ku) {
		move_line_up(&cur_book_view, 1);
	} else if (key == kd) {
		move_line_down(&cur_book_view, 1);
	} else if (key == ctlkey[1] || key == ctlkey2[1]) {
		if (config.vertread == conf_vertread_reversal) {
			if (move_page_down(&cur_book_view, key, selidx))
				goto next;
		} else {
			if (move_page_up(&cur_book_view, key, selidx))
				goto next;
		}

	} else if (key == kl || key == CTRL_BACK) {
		move_page_up(&cur_book_view, key, selidx);
	} else if (key == ctlkey[2] || key == ctlkey2[2]) {
		if (config.vertread == conf_vertread_reversal) {
			if (move_page_up(&cur_book_view, key, selidx))
				goto next;
		} else {
			if (move_page_down(&cur_book_view, key, selidx))
				goto next;
		}
	} else if (key == kr || key == CTRL_FORWARD) {
		move_page_down(&cur_book_view, key, selidx);
	} else if (key == ctlkey[5] || key == ctlkey2[5]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_down(&cur_book_view, 500);
		else
			move_line_up(&cur_book_view, 500);
	} else if (key == ctlkey[6] || key == ctlkey2[6]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_up(&cur_book_view, 500);
		else
			move_line_down(&cur_book_view, 500);
	} else if (key == ctlkey[3] || key == ctlkey2[3]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_down(&cur_book_view, 100);
		else
			move_line_up(&cur_book_view, 100);
	} else if (key == ctlkey[4] || key == ctlkey2[4]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_up(&cur_book_view, 100);
		else
			move_line_down(&cur_book_view, 100);
	} else if (key == ctlkey[7] || key == ctlkey2[7]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_to_end(&cur_book_view);
		else
			move_line_to_start(&cur_book_view);
	} else if (key == ctlkey[8] || key == ctlkey2[8]) {
		if (config.vertread == conf_vertread_reversal)
			move_line_to_start(&cur_book_view);
		else
			move_line_to_end(&cur_book_view);
	} else if (key == ctlkey[9] || key == ctlkey2[9]) {
		dword orgidx = *selidx;

		if (config.autobm)
			bookmark_autosave(pView->bookmarkname,
							  (fs->rows[fs->crow >> 10] +
							   (fs->crow & 0x3FF))->start - fs->buf);
		do {
			if (*selidx > 0)
				(*selidx)--;
			else
				*selidx = filecount - 1;
		} while (!fs_is_txtbook((t_fs_filetype) filelist[*selidx].data));
		if (*selidx != orgidx) {
			if (config.autobm)
				bookmark_autosave(pView->bookmarkname,
								  (fs->rows[fs->crow >> 10] +
								   (fs->crow & 0x3FF))->start - fs->buf);
			pView->text_needrf = pView->text_needrp = true;
			pView->text_needrb = false;
			pView->rrow = INVALID;
		}
	} else if (key == ctlkey[10] || key == ctlkey2[10]) {
		dword orgidx = *selidx;

		do {
			if (*selidx < filecount - 1)
				(*selidx)++;
			else
				*selidx = 0;
		} while (!fs_is_txtbook((t_fs_filetype) filelist[*selidx].data));
		if (*selidx != orgidx) {
			if (config.autobm)
				bookmark_autosave(pView->bookmarkname,
								  (fs->rows[fs->crow >> 10] +
								   (fs->crow & 0x3FF))->start - fs->buf);
			pView->text_needrf = pView->text_needrp = true;
			pView->text_needrb = false;
			pView->rrow = INVALID;
		}
	} else if (key == ctlkey[12] || key == ctlkey2[12]) {
		if (config.autopagetype == 2) {
			config.autopagetype = config.prev_autopage;
		} else {
			config.prev_autopage = config.autopagetype;
			config.autopagetype = 2;
		}
		pView->text_needrp = true;
	} else if (key == ctlkey[13] || key == ctlkey2[13]) {
		scene_power_save(false);
		char buf[128];

		if (get_osk_input(buf, 128) == 1 && strcmp(buf, "") != 0) {
			dbg_printf(d, "%s: input %s", __func__, buf);
			unsigned long gi = strtoul(buf, NULL, 10);

			if (gi != LONG_MIN) {
				dbg_printf(d, "%s: GI值为%ld", __func__, gi);
				jump_to_gi((dword) gi);
			} else {
				dbg_printf(d, "%s: 输入错误GI值%s", __func__, buf);
			}
		}

		disp_duptocache();
		disp_waitv();
		cur_book_view.text_needrp = true;

		scene_power_save(true);
	} else
		pView->text_needrp = pView->text_needrb = pView->text_needrf = false;
	// reset ticks
	ticks = 0;
  next:
	secticks = 0;
	return -1;
}

static void scene_text_delay_action()
{
	extern bool prx_loaded;

	if (config.dis_scrsave)
		scePowerTick(0);
	if (prx_loaded) {
		xrSetBrightness(config.brightness);
	}
}

dword scene_reload_raw(const char *title, const unsigned char *data,
					   size_t size, t_fs_filetype ft)
{
	fs = text_open_in_raw(title, data, size, ft, pixelsperrow, config.wordspace,
						  config.encode, config.reordertxt);

	if (fs == NULL) {
		return 1;
	}

	if (cur_book_view.rrow == INVALID) {
		// disable binary bookmark
		if (config.autobm && ft != fs_filetype_unknown) {
			cur_book_view.rrow = bookmark_autoload(cur_book_view.bookmarkname);
			cur_book_view.text_needrb = true;
		} else
			cur_book_view.rrow = 0;
	}

	if (cur_book_view.text_needrb && ft != fs_filetype_unknown) {
		cur_book_view.rowtop = 0;
		fs->crow = 1;
		while (fs->crow < fs->row_count
			   && (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start -
			   fs->buf <= cur_book_view.rrow)
			fs->crow++;
		fs->crow--;
		cur_book_view.text_needrb = false;
	} else
		fs->crow = cur_book_view.rrow;

	if (fs->crow >= fs->row_count)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;

	cur_book_view.trow =
		&cur_book_view.
		tr[utils_dword2string(fs->row_count, cur_book_view.tr, 7)];

	return 0;
}

dword scene_readbook_raw(const char *title, const unsigned char *data,
						 size_t size, t_fs_filetype ft)
{
	bool prev_raw = scene_readbook_in_raw_mode;

	scene_readbook_in_raw_mode = true;

	// dummy selidx
	dword selidx = 0;
	p_text prev_text = NULL;

	STRCPY_S(g_titlename, title);

	copy_book_view(&prev_book_view, &cur_book_view);
	new_book_view(&cur_book_view);

	u64 timer_start, timer_end;

	sceRtcGetCurrentTick(&timer_start);
	scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
	while (1) {
		if (cur_book_view.text_needrf) {
			if (fs != NULL) {
				prev_text = fs;
				fs = NULL;
			}

			if (scene_reload_raw(title, data, size, ft)) {
				win_msg(_("文件打开失败"), COLOR_WHITE, COLOR_WHITE,
						config.msgbcolor);
				dbg_printf(d, _("scene_readbook_raw: 文件%s打开失败 where=%d"),
						   title, where);
				fs = prev_text;
				copy_book_view(&cur_book_view, &prev_book_view);

				scene_readbook_in_raw_mode = prev_raw;
				return 1;
			}

			cur_book_view.text_needrf = false;
		}
		if (cur_book_view.text_needrp) {
			scene_printbook(&cur_book_view, selidx);
			cur_book_view.text_needrp = false;
		}

		int delay = 20000;
		dword key;

		while ((key = ctrl_read()) == 0) {
			sceKernelDelayThread(delay);
			sceRtcGetCurrentTick(&timer_end);
			if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
				sceRtcGetCurrentTick(&timer_start);
				secticks++;
			}
			if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
#ifdef ENABLE_MUSIC
				mp3_powerdown();
#endif
				fat_powerdown();
				scePowerRequestSuspend();
				secticks = 0;
			}

			if (config.autopage) {
				if (scene_autopage(&cur_book_view, &selidx))
					goto redraw;
			}
			scene_text_delay_action();
		}
		dword selidx = 0;
		int ret = book_handle_input(&cur_book_view, &selidx, key);

		if (ret != -1) {
			text_close(fs);
			fs = prev_text;
			copy_book_view(&cur_book_view, &prev_book_view);

			scene_readbook_in_raw_mode = prev_raw;
			return ret;
		}
	  redraw:
		;
	}
	text_close(fs);
	fs = prev_text;
	copy_book_view(&cur_book_view, &prev_book_view);
	disp_duptocachealpha(50);

	scene_readbook_in_raw_mode = prev_raw;
	return INVALID;
}

dword scene_readbook(dword selidx)
{
	u64 timer_start, timer_end;

	sceRtcGetCurrentTick(&timer_start);

	new_book_view(&cur_book_view);

	scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
	while (1) {
		if (cur_book_view.text_needrf) {
			if (scene_book_reload(&cur_book_view, selidx)) {
				return selidx;
			}
			cur_book_view.text_needrf = false;
		}
		if (cur_book_view.text_needrp) {
			scene_printbook(&cur_book_view, selidx);
#ifdef _DEBUG
//          disp_flip();
//          ScreenShot();
//          dbg_printf(d, "Screen Captured");
//          disp_flip();
#endif
			cur_book_view.text_needrp = false;
		}

		int delay = 20000;
		dword key;

		while ((key = ctrl_read()) == 0) {
			sceKernelDelayThread(delay);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if (config.infobar == conf_infobar_lyric
				&& lyric_check_changed(mp3_get_lyric())) {
				break;
			}
#endif
			sceRtcGetCurrentTick(&timer_end);
			if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
				sceRtcGetCurrentTick(&timer_start);
				secticks++;
			}
			if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
#ifdef ENABLE_MUSIC
				mp3_powerdown();
#endif
				fat_powerdown();
				scePowerRequestSuspend();
				secticks = 0;
			}

			if (config.autopage) {
				if (scene_autopage(&cur_book_view, &selidx))
					goto redraw;
			}
			scene_text_delay_action();
		}
		int ret = book_handle_input(&cur_book_view, &selidx, key);

		if (ret != -1) {
			scene_power_save(true);
			return ret;
		}
	  redraw:
		;
	}
	scene_power_save(false);
	if (config.autobm)
		bookmark_autosave(cur_book_view.bookmarkname,
						  (fs->rows[fs->crow >> 10] +
						   (fs->crow & 0x3FF))->start - fs->buf);
	text_close(fs);
	fs = NULL;
	disp_duptocachealpha(50);
	scene_power_save(true);
	return selidx;
}
