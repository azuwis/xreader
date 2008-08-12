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
#include "osk.h"

#define MAX_TXT_KEY 14

dword ctlkey[MAX_TXT_KEY], ctlkey2[MAX_TXT_KEY], ku, kd, kl, kr;
static volatile int ticks = 0, secticks = 0;
char g_titlename[256];
bool scene_readbook_in_raw_mode = false;

BookViewData cur_book_view, prev_book_view;
extern win_menu_predraw_data g_predraw;

static byte bgalpha = 0x40, fgalpha = 0xa0;
static pixel *infobar_saveimage = NULL;

#ifdef ENABLE_TTF
extern p_ttf cttf, ettf;
#endif

static inline int calc_gi(void)
{
	int i = min(fs->crow, fs->row_count);
	p_textrow tr = fs->rows[i >> 10] + (i & 0x3FF);

	return tr->GI;
}

#ifdef _DEBUG
static void write_byte(int fd, unsigned char b)
{
	sceIoWrite(fd, &b, 1);
}

void get_screen_shot(void)
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
		SPRINTF_S(filename, "ms0:/get_screen_shot%02d.tga", i++);
	} while (utils_is_file_exists(filename));
	int fd =
		sceIoOpen(filename, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
	if (!fd)
		return;
	sceIoWrite(fd, tgaHeader, sizeof(tgaHeader));
	write_byte(fd, width & 0xff);
	write_byte(fd, width >> 8);
	write_byte(fd, height & 0xff);
	write_byte(fd, height >> 8);
	write_byte(fd, 24);
	write_byte(fd, 0);
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

static float calc_percent(dword current, dword total)
{
	float percent;

	if (total == 0)
		percent = 0.0;
	else
		percent = 100.0 * MIN(current + 1, total) / total;
	return percent;
}

static char *strfloatpad(char *str, int maxsize, float num, int pad,
						 const char *afterstr)
{
	int d, i;

	snprintf_s(str, maxsize, "  %.2f%%%n", num, &d);
	for (i = 0; i < pad - d; ++i) {
		strcat_s(str, maxsize, " ");
	}
	strcat_s(str, maxsize, afterstr);

	return str;
}

/**
 * 8位单通道alpha混合算法
 * @param wpSrc 源颜色指针
 * @param wpDes 目的颜色指针
 * @param wAlpha alpha值(0-255)
 * @note 目的颜色 = 目的颜色 * alpha + ( 1 - alpha ) * 源颜色
 */
static inline void MakeAlpha(byte * wpSrc, byte * wpDes, byte wAlpha)
{
	word result;

	if (*wpDes == *wpSrc && wAlpha == 255)
		return;

	if (wAlpha == 0)
		*wpDes = *wpSrc;

	result = wAlpha * (*wpDes) + (255 - wAlpha) * (*wpSrc);

	*wpDes = result / 255;
}

static void disp_fillrect_tran(dword x1, dword y1, dword x2, dword y2,
							   pixel color, byte alpha)
{
	pixel *vsram, *vsram_end, *vram, *vram_end;
	dword wdwords;
	byte rd, gd, bd;
	byte rs, gs, bs;

	if (x1 >= PSP_SCREEN_WIDTH)
		x1 = PSP_SCREEN_WIDTH - 1;
	if (x2 >= PSP_SCREEN_WIDTH)
		x2 = PSP_SCREEN_WIDTH - 1;
	if (y1 >= PSP_SCREEN_HEIGHT)
		y1 = PSP_SCREEN_HEIGHT - 1;
	if (y2 >= PSP_SCREEN_HEIGHT)
		y2 = PSP_SCREEN_HEIGHT - 1;

	if (x1 > x2) {
		dword t = x1;

		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		dword t = y1;

		y1 = y2;
		y2 = t;
	}
	vsram = disp_get_vaddr(x1, y1);
	vsram_end = vsram + 512 * (y2 - y1);
	wdwords = (x2 - x1);
	for (; vsram <= vsram_end; vsram += 512) {
		for (vram = vsram, vram_end = vram + wdwords; vram <= vram_end; vram++) {
			pixel origcolor = *vram;

			rs = RGB_R(origcolor);
			gs = RGB_G(origcolor);
			bs = RGB_B(origcolor);

			rd = RGB_R(color);
			gd = RGB_G(color);
			bd = RGB_B(color);

			MakeAlpha(&rs, &rd, alpha);
			MakeAlpha(&gs, &gd, alpha);
			MakeAlpha(&bs, &bd, alpha);

			*vram = (RGB(rd, gd, bd));
		}
	}
}

extern int scene_get_infobar_height(void)
{
	if (config.infobar == conf_infobar_none)
		return 0;
	return config.infobar_fontsize + 1;
}

static void draw_infobar_single_line(int vertread)
{
	switch (vertread) {
		case conf_vertread_reversal:
			disp_line(0, scene_get_infobar_height(), (PSP_SCREEN_WIDTH - 1),
					  scene_get_infobar_height(), config.forecolor);
			break;
		case conf_vertread_lvert:
			disp_line((PSP_SCREEN_WIDTH - 1) - scene_get_infobar_height(), 0,
					  (PSP_SCREEN_WIDTH - 1) - scene_get_infobar_height(),
					  (PSP_SCREEN_HEIGHT - 1), config.forecolor);
			break;
		case conf_vertread_rvert:
			disp_line(scene_get_infobar_height(), 0,
					  scene_get_infobar_height(), (PSP_SCREEN_HEIGHT - 1),
					  config.forecolor);
			break;
		case conf_vertread_horz:
			disp_line(0, (PSP_SCREEN_HEIGHT - 1) - scene_get_infobar_height(),
					  (PSP_SCREEN_WIDTH - 1),
					  (PSP_SCREEN_HEIGHT - 1) - scene_get_infobar_height(),
					  config.forecolor);
			break;
		default:
			break;
	}
}

static void draw_infobar_rect(int vertread)
{
	switch (vertread) {
		case conf_vertread_reversal:
			disp_fillrect_tran(0, scene_get_infobar_height(),
							   (PSP_SCREEN_WIDTH - 1), 0, config.forecolor,
							   bgalpha);
			break;
		case conf_vertread_lvert:
			disp_fillrect_tran(PSP_SCREEN_WIDTH - scene_get_infobar_height() -
							   1, 0, (PSP_SCREEN_WIDTH - 1),
							   (PSP_SCREEN_HEIGHT - 1), config.forecolor,
							   bgalpha);
			break;
		case conf_vertread_rvert:
			disp_fillrect_tran(0, 0, scene_get_infobar_height(),
							   (PSP_SCREEN_HEIGHT - 1), config.forecolor,
							   bgalpha);
			break;
		case conf_vertread_horz:
			disp_fillrect_tran(0,
							   (PSP_SCREEN_HEIGHT - 1) -
							   (scene_get_infobar_height()),
							   (PSP_SCREEN_WIDTH - 1), (PSP_SCREEN_HEIGHT - 1),
							   config.forecolor, bgalpha);
			break;
		default:
			break;
	}
}

static void get_infobar_system_string(char *dest, int size)
{
	pspTime tm;
	char t[512];
	int percent, unused;

	sceRtcGetCurrentClockLocalTime(&tm);

	power_get_battery(&percent, &unused, &unused, &unused);
	if (tm.seconds % 2 == 0) {
		if (percent == 100)
			SPRINTF_S(t, "[%02u:%02u]", tm.hour, tm.minutes);
		else
			SPRINTF_S(t, "[%02u:%02u %d%%]", tm.hour, tm.minutes, percent);
	} else {
		if (percent == 100)
			SPRINTF_S(t, "[%02u %02u]", tm.hour, tm.minutes);
		else
			SPRINTF_S(t, "[%02u %02u %d%%]", tm.hour, tm.minutes, percent);
	}
	strcpy_s(dest, size, t);
}

static void get_infobar_string(dword selidx, char *dest, int size)
{
	char autopageinfo[80];
	char cr[512];

	if (config.autopagetype == 2)
		autopageinfo[0] = 0;
	else if (config.autopagetype == 1) {
		if (config.autopage != 0)
			SPRINTF_S(autopageinfo, _("%s: 时间 %d 速度 %d"),
					  _("自动滚屏"), config.autolinedelay, config.autopage);
		else
			autopageinfo[0] = 0;
	} else {
		if (config.autopage != 0) {
			SPRINTF_S(autopageinfo, _("自动翻页: 时间 %d"), config.autopage);
		} else
			autopageinfo[0] = 0;
	}

	if (where == scene_in_chm) {
		char t[512], u[512];

		if (config.infobar_show_timer)
			get_infobar_system_string(u, sizeof(u));
		else
			STRCPY_S(u, "");

		SPRINTF_S(t, "%s %s GI: %d %s %s",
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename(config.
																  encode)),
				  filelist[selidx].name, calc_gi(), autopageinfo, u);
		strfloatpad(cr, sizeof(cr), calc_percent(fs->crow, fs->row_count), 10,
					t);
	} else if (scene_readbook_in_raw_mode == true) {
		char t[512], u[512];

		if (config.infobar_show_timer)
			get_infobar_system_string(u, sizeof(u));
		else
			STRCPY_S(u, "");
		SPRINTF_S(t, "%s %s GI: %d %s %s",
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename(config.
																  encode)),
				  g_titlename, calc_gi(), autopageinfo, u);
		strfloatpad(cr, sizeof(cr), calc_percent(fs->crow, fs->row_count), 10,
					t);
	} else {
		char t[512], u[512];

		if (config.infobar_show_timer)
			get_infobar_system_string(u, sizeof(u));
		else
			STRCPY_S(u, "");
		SPRINTF_S(t, "%s %s GI: %d %s %s",
				  (fs->ucs == 2) ? "UTF-8" : (fs->ucs ==
											  1 ? "UCS " :
											  conf_get_encodename(config.
																  encode)),
				  filelist[selidx].compname->ptr, calc_gi(), autopageinfo, u);
		strfloatpad(cr, sizeof(cr), calc_percent(fs->crow, fs->row_count), 10,
					t);
	}

	strcpy_s(dest, size, cr);
}

#ifdef ENABLE_TTF
static void draw_infobar_info_ttf(PBookViewData pView, dword selidx,
								  int vertread)
{
	char cr[512];
	int wordspace = 0;

	if (config.infobar_style == 0)
		draw_infobar_rect(vertread);
	else
		draw_infobar_single_line(vertread);
	get_infobar_string(selidx, cr, sizeof(cr));

	switch (vertread) {
		case conf_vertread_reversal:
			disp_putnstringreversal(0,
									PSP_SCREEN_HEIGHT -
									scene_get_infobar_height() - 1,
									config.forecolor, (const byte *) cr,
									960 / config.infobar_fontsize, wordspace, 0,
									config.infobar_fontsize, 0);
			break;
		case conf_vertread_lvert:
			disp_putnstringlvert(PSP_SCREEN_WIDTH -
								 scene_get_infobar_height() - 1,
								 (PSP_SCREEN_HEIGHT - 1), config.forecolor,
								 (const byte *) cr,
								 544 / config.infobar_fontsize, wordspace, 0,
								 config.infobar_fontsize, 0);
			break;
		case conf_vertread_rvert:
			disp_putnstringrvert(scene_get_infobar_height(), 0,
								 config.forecolor, (const byte *) cr,
								 544 / config.infobar_fontsize, wordspace, 0,
								 config.infobar_fontsize, 0);
			break;
		case conf_vertread_horz:
			disp_putnstringhorz(0,
								PSP_SCREEN_HEIGHT -
								scene_get_infobar_height() - 1,
								config.forecolor, (const byte *) cr,
								960 / config.infobar_fontsize, wordspace, 0,
								config.infobar_fontsize, 0);
			break;
		default:
			break;
	}
}
#endif

static void draw_infobar_info(PBookViewData pView, dword selidx, int vertread)
{
	char cr[512];
	int wordspace = (config.infobar_fontsize == 10 ? 1 : 0);

	if (config.infobar_style == 0)
		draw_infobar_rect(vertread);
	else
		draw_infobar_single_line(vertread);

	get_infobar_string(selidx, cr, sizeof(cr));

	switch (vertread) {
		case conf_vertread_reversal:
			disp_putnstringreversal_sys(0,
										PSP_SCREEN_HEIGHT -
										config.infobar_fontsize,
										config.forecolor, (const byte *) cr,
										960 / config.infobar_fontsize,
										wordspace, 0, config.infobar_fontsize,
										0);
			break;
		case conf_vertread_lvert:
			disp_putnstringlvert_sys(PSP_SCREEN_WIDTH - config.infobar_fontsize,
									 (PSP_SCREEN_HEIGHT - 1),
									 config.forecolor, (const byte *) cr,
									 544 / config.infobar_fontsize, wordspace,
									 0, config.infobar_fontsize, 0);
			break;
		case conf_vertread_rvert:
			disp_putnstringrvert_sys(config.infobar_fontsize - 1, 0,
									 config.forecolor, (const byte *) cr,
									 544 / config.infobar_fontsize, wordspace,
									 0, config.infobar_fontsize, 0);
			break;
		case conf_vertread_horz:
			disp_putnstringhorz_sys(0,
									PSP_SCREEN_HEIGHT - config.infobar_fontsize,
									config.forecolor, (const byte *) cr,
									960 / config.infobar_fontsize, wordspace, 0,
									config.infobar_fontsize, 0);
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
	if (config.infobar_style == 0)
		draw_infobar_rect(vertread);
	else
		draw_infobar_single_line(vertread);

	const char *ls[1];
	dword ss[1];
	int wordspace = 0;

	if (lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss)
		&& ls[0] != NULL) {
		if (ss[0] > 960 / config.infobar_fontsize)
			ss[0] = 960 / config.infobar_fontsize;
		char t[BUFSIZ];

		lyric_decode(ls[0], t, &ss[0]);
		switch (vertread) {
			case conf_vertread_reversal:
				disp_putnstringreversal((240 -
										 ss[0] * config.infobar_fontsize /
										 4),
										PSP_SCREEN_HEIGHT -
										scene_get_infobar_height() - 1,
										config.forecolor,
										(const byte *) t, ss[0],
										wordspace, 0, config.infobar_fontsize,
										0);
				break;
			case conf_vertread_lvert:
				disp_putnstringlvert(PSP_SCREEN_WIDTH -
									 scene_get_infobar_height() - 1,
									 (PSP_SCREEN_HEIGHT - 1) - (136 -
																ss[0] *
																config.
																infobar_fontsize
																/ 4),
									 config.forecolor, (const byte *) t, ss[0],
									 wordspace, 0, config.infobar_fontsize, 0);
				break;
			case conf_vertread_rvert:
				disp_putnstringrvert(scene_get_infobar_height(),
									 (136 -
									  ss[0] * config.infobar_fontsize / 4),
									 config.forecolor, (const byte *) t,
									 ss[0], wordspace, 0,
									 config.infobar_fontsize, 0);
				break;
			case conf_vertread_horz:
				disp_putnstringhorz((240 -
									 ss[0] * config.infobar_fontsize / 4),
									PSP_SCREEN_HEIGHT -
									scene_get_infobar_height() - 1,
									config.forecolor, (const byte *) t,
									ss[0], wordspace, 0,
									config.infobar_fontsize, 0);
				break;
			default:
				break;
		}
	}
}
#endif

static void draw_infobar_lyric(PBookViewData pView, dword selidx, int vertread)
{
	if (config.infobar_style == 0)
		draw_infobar_rect(vertread);
	else
		draw_infobar_single_line(vertread);

	const char *ls[1];
	dword ss[1];
	int wordspace = (config.infobar_fontsize == 10 ? 1 : 0);

	if (lyric_get_cur_lines(mp3_get_lyric(), 0, ls, ss)
		&& ls[0] != NULL) {
		if (ss[0] > 960 / config.infobar_fontsize)
			ss[0] = 960 / config.infobar_fontsize;
		char t[BUFSIZ];

		lyric_decode(ls[0], t, &ss[0]);
		switch (vertread) {
			case conf_vertread_reversal:
				disp_putnstringreversal_sys((240 -
											 ss[0] * config.infobar_fontsize /
											 4),
											PSP_SCREEN_HEIGHT -
											scene_get_infobar_height(),
											config.forecolor, (const byte *) t,
											ss[0], wordspace, 0,
											config.infobar_fontsize, 0);
				break;
			case conf_vertread_lvert:
				disp_putnstringlvert_sys(PSP_SCREEN_WIDTH -
										 scene_get_infobar_height(),
										 (PSP_SCREEN_HEIGHT - 1) -
										 (136 -
										  ss[0] * config.infobar_fontsize / 4),
										 config.forecolor,
										 (const byte *) t, ss[0],
										 wordspace, 0, config.infobar_fontsize,
										 0);
				break;
			case conf_vertread_rvert:
				disp_putnstringrvert_sys(scene_get_infobar_height() - 1,
										 (136 -
										  ss[0] * config.infobar_fontsize / 4),
										 config.forecolor,
										 (const byte *) t, ss[0],
										 wordspace, 0, config.infobar_fontsize,
										 0);
				break;
			case conf_vertread_horz:
				disp_putnstringhorz_sys((240 -
										 ss[0] * config.infobar_fontsize / 4),
										PSP_SCREEN_HEIGHT -
										scene_get_infobar_height(),
										config.forecolor, (const byte *) t,
										ss[0], wordspace, 0,
										config.infobar_fontsize, 0);
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
	if (p == NULL) {
		p = calloc(1, sizeof(*p));
	}

	p->rrow = INVALID;
	p->rowtop = 0;
	p->text_needrf = p->text_needrp = true;
	p->text_needrb = false;
	p->filename[0] = p->archname[0] = p->bookmarkname[0] = '\0';

	return p;
}

int scene_book_reload(PBookViewData pView, dword selidx)
{
	if (where == scene_in_zip || where == scene_in_chm || where == scene_in_rar) {
		STRCPY_S(pView->filename, filelist[selidx].compname->ptr);
		STRCPY_S(pView->archname, config.shortpath);
		if (sceKernelDevkitVersion() <= 0x03070110) {
			STRCPY_S(pView->bookmarkname, config.shortpath);
		} else {
			STRCPY_S(pView->bookmarkname, config.path);
		}
		if (config.shortpath[strlen(config.shortpath) - 1] != '/' &&
			pView->filename[0] != '/')
			STRCAT_S(pView->bookmarkname, "/");
		STRCAT_S(pView->bookmarkname, pView->filename);
	} else {
		STRCPY_S(pView->filename, config.path);
		STRCAT_S(pView->filename, filelist[selidx].compname->ptr);
		STRCPY_S(pView->archname, config.shortpath);
		STRCAT_S(pView->archname, filelist[selidx].shortname->ptr);
		if (sceKernelDevkitVersion() <= 0x03070110) {
			STRCPY_S(pView->bookmarkname, pView->archname);
		} else {
			STRCPY_S(pView->bookmarkname, pView->filename);
		}
	}
	dbg_printf(d, "%s: fn %s bookmarkname %s archname %s", __func__,
			   pView->filename, pView->bookmarkname, pView->archname);
	if (pView->rrow == INVALID) {
		if (!config.autobm
			|| (t_fs_filetype) filelist[selidx].data == fs_filetype_unknown) {
			// disable binary file type text's bookmark
			pView->rrow = 0;
		} else {
			pView->rrow = bookmark_autoload(pView->bookmarkname);
			pView->text_needrb = true;
		}
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
							   pixelsperrow, config.wordspace,
							   config.encode, config.reordertxt, where,
							   config.vertread);
	else
		fs = text_open_archive(pView->filename, pView->archname,
							   fs_filetype_txt,
							   pixelsperrow, config.wordspace,
							   config.encode, config.reordertxt, where,
							   config.vertread);

	if (fs == NULL) {
		win_msg(_("文件打开失败"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		dbg_printf(d, _("scene_book_reload: 文件%s打开失败 where=%d"),
				   pView->filename, where);
		dbg_printf(d, _("scene_book_reload: %s %s %s"), filelist[selidx].name,
				   filelist[selidx].shortname->ptr,
				   filelist[selidx].compname->ptr, where);
		scene_power_save(true);
		return 1;
	}

	if (pView->rrow == (dword) - 2) {
		p_textrow tr;

		if (fs->row_count != 0)
			tr = fs->rows[(fs->row_count - 1) >> 10] +
				((fs->row_count - 1) & 0x3FF);
		else
			tr = fs->rows[0];
		pView->rrow = tr->start - fs->buf;
		pView->text_needrb = true;
	}
	if (pView->text_needrb
		&& (t_fs_filetype) filelist[selidx].data != fs_filetype_unknown) {
		pView->rowtop = 0;
		fs->crow = 1;
		while (fs->crow < fs->row_count
			   && (fs->rows[fs->crow >> 10] +
				   (fs->crow & 0x3FF))->start - fs->buf <= pView->rrow)
			fs->crow++;
		fs->crow--;
		pView->text_needrb = false;
	} else
		fs->crow = pView->rrow;

	if (fs->crow >= fs->row_count)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;

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
								(const byte *) tr->start, (int) tr->count,
								config.wordspace, 0, DISP_BOOK_FONTSIZE,
								config.infobar ? PSP_SCREEN_HEIGHT -
								scene_get_infobar_height() : PSP_SCREEN_HEIGHT);
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
							 config.infobar ? scene_get_infobar_height() +
							 1 : 1);
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
							 scene_get_infobar_height() : PSP_SCREEN_WIDTH);
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
							config.borderspace + (DISP_BOOK_FONTSIZE +
												  config.rowspace) *
							cidx - pView->rowtop, config.forecolor,
							(const byte *) tr->start, (int) tr->count,
							config.wordspace, 0, DISP_BOOK_FONTSIZE,
							config.infobar ? (PSP_SCREEN_HEIGHT - 1) -
							scene_get_infobar_height() : PSP_SCREEN_HEIGHT);
	}
}

static void scene_draw_scrollbar_reversal(void)
{
	dword slen =
		config.infobar ? (PSP_SCREEN_HEIGHT - 1 - 1 -
						  scene_get_infobar_height())
		: (PSP_SCREEN_HEIGHT - 1), bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_fillrect_tran(0, PSP_SCREEN_HEIGHT - 1,
					   config.scrollbar_width, PSP_SCREEN_HEIGHT - 1 - slen,
					   config.forecolor, bgalpha);
	disp_fillrect_tran(0, PSP_SCREEN_HEIGHT - 1 - endp, config.scrollbar_width,
					   PSP_SCREEN_HEIGHT - 1 - startp, config.forecolor,
					   fgalpha);
}

static void scene_draw_scrollbar_lvert(void)
{
	dword sright =
		(PSP_SCREEN_WIDTH - 1) - 1 -
		(config.infobar ? scene_get_infobar_height() : 0), slen =
		sright, bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_fillrect_tran(0, 0, sright, config.scrollbar_width,
					   config.forecolor, bgalpha);
	disp_fillrect_tran(startp, 0, endp, config.scrollbar_width,
					   config.forecolor, fgalpha);
}

static void scene_draw_scrollbar_rvert(void)
{
	dword sleft =
		(config.infobar ? scene_get_infobar_height() : 0) + 1, slen =
		(PSP_SCREEN_WIDTH - 1) - sleft, bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, endp =
		(PSP_SCREEN_WIDTH - 1) - slen * fs->crow / fs->row_count, startp;
	if (endp - bsize < sleft)
		endp = sleft + bsize;
	startp = endp - bsize;
	disp_fillrect_tran(sleft, PSP_SCREEN_HEIGHT - config.scrollbar_width,
					   PSP_SCREEN_WIDTH - 1,
					   PSP_SCREEN_HEIGHT - 1, config.forecolor, bgalpha);
	disp_fillrect_tran(startp, PSP_SCREEN_HEIGHT - (config.scrollbar_width + 1),
					   endp, PSP_SCREEN_HEIGHT - 1, config.forecolor, fgalpha);
}

static void scene_draw_scrollbar_horz(void)
{
	dword slen =
		config.infobar ? (PSP_SCREEN_HEIGHT - 1 - 1 -
						  scene_get_infobar_height())
		: (PSP_SCREEN_HEIGHT - 1), bsize =
		2 + (slen - 2) * rowsperpage / fs->row_count, startp =
		slen * fs->crow / fs->row_count, endp;
	if (startp + bsize > slen)
		startp = slen - bsize;
	endp = startp + bsize;
	disp_fillrect_tran(PSP_SCREEN_WIDTH - config.scrollbar_width - 1, 0,
					   PSP_SCREEN_WIDTH - 1, slen, config.forecolor, bgalpha);
	disp_fillrect_tran(PSP_SCREEN_WIDTH - config.scrollbar_width - 1, startp,
					   PSP_SCREEN_WIDTH - 1, endp, config.forecolor, fgalpha);
}

static void scene_draw_infobar(PBookViewData pView, dword selidx)
{
	if (config.infobar == conf_infobar_info) {
#ifdef ENABLE_TTF
		if (config.infobar_use_ttf_mode && config.usettf)
			draw_infobar_info_ttf(pView, selidx, config.vertread);
		else
#endif
			draw_infobar_info(pView, selidx, config.vertread);
	}
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	else if (config.infobar == conf_infobar_lyric) {
#ifdef ENABLE_TTF
		if (config.infobar_use_ttf_mode && config.usettf)
			draw_infobar_lyric_ttf(pView, selidx, config.vertread);
		else
#endif
			draw_infobar_lyric(pView, selidx, config.vertread);
	}
#endif
}

static void scene_draw_scrollbar(void)
{
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
}

static void free_infobar_image(void)
{
	if (infobar_saveimage != NULL) {
		free(infobar_saveimage);
		infobar_saveimage = NULL;
	}
}

static void save_infobar_image(void)
{
	if (!config.infobar)
		return;

	free_infobar_image();

	switch (config.vertread) {
		case conf_vertread_reversal:
		case conf_vertread_horz:
			infobar_saveimage =
				(pixel *) memalign(16,
								   (scene_get_infobar_height() +
									1) * PSP_SCREEN_WIDTH * sizeof(pixel));
			break;
		case conf_vertread_rvert:
		case conf_vertread_lvert:
			infobar_saveimage =
				(pixel *) memalign(16,
								   (scene_get_infobar_height() +
									1) * PSP_SCREEN_HEIGHT * sizeof(pixel));
			break;
	}

	switch (config.vertread) {
		case conf_vertread_reversal:
			disp_getimage_draw(0, 0, PSP_SCREEN_WIDTH,
							   scene_get_infobar_height() + 1,
							   infobar_saveimage);
			break;
		case conf_vertread_lvert:
			disp_getimage_draw(PSP_SCREEN_WIDTH - scene_get_infobar_height() -
							   1, 0, scene_get_infobar_height() + 1,
							   PSP_SCREEN_HEIGHT, infobar_saveimage);
			break;
		case conf_vertread_rvert:
			disp_getimage_draw(0, 0, scene_get_infobar_height() + 1,
							   PSP_SCREEN_HEIGHT, infobar_saveimage);
			break;
		case conf_vertread_horz:
			disp_getimage_draw(0,
							   PSP_SCREEN_HEIGHT - 1 -
							   scene_get_infobar_height(), PSP_SCREEN_WIDTH,
							   scene_get_infobar_height() + 1,
							   infobar_saveimage);
			break;
	}
}

static void load_infobar_image(void)
{
	if (!config.infobar || infobar_saveimage == NULL)
		return;

	switch (config.vertread) {
		case conf_vertread_reversal:
			disp_putimage(0, 0, PSP_SCREEN_WIDTH,
						  scene_get_infobar_height() + 1, 0, 0,
						  infobar_saveimage);
			break;
		case conf_vertread_lvert:
			disp_putimage(PSP_SCREEN_WIDTH - scene_get_infobar_height() - 1, 0,
						  scene_get_infobar_height() + 1, PSP_SCREEN_HEIGHT, 0,
						  0, infobar_saveimage);
			break;
		case conf_vertread_rvert:
			disp_putimage(0, 0, scene_get_infobar_height() + 1,
						  PSP_SCREEN_HEIGHT, 0, 0, infobar_saveimage);
			break;
		case conf_vertread_horz:
			disp_putimage(0, PSP_SCREEN_HEIGHT - 1 - scene_get_infobar_height(),
						  PSP_SCREEN_WIDTH, scene_get_infobar_height() + 1, 0,
						  0, infobar_saveimage);
			break;
	}
}

int scene_printbook(PBookViewData pView, dword selidx)
{
	disp_waitv();
#ifdef ENABLE_BG
	if (!bg_display())
#endif
		disp_fillvram(config.bgcolor);
#ifdef ENABLE_TTF
	ttf_set_pixel_size(cttf, DISP_BOOK_FONTSIZE);
	ttf_set_pixel_size(ettf, DISP_BOOK_FONTSIZE);
#endif
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
									  (fs->
									   rows[fs->
											crow >> 10] +
									   (fs->crow & 0x3FF))->start - fs->buf);
				pView->text_needrf = pView->text_needrp = true;
				pView->text_needrb = false;
				pView->rrow = (dword) - 2;
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
									  (fs->
									   rows[fs->
											crow >> 10] +
									   (fs->crow & 0x3FF))->start - fs->buf);
				pView->text_needrf = pView->text_needrp = true;
				pView->text_needrb = false;
				pView->rrow = 0;
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

static void jump_to_gi(dword gi)
{
	dword i;

	for (i = 0; i < fs->row_count; ++i) {
		p_textrow tr;

		tr = fs->rows[i >> 10] + (i & 0x3FF);
		if (tr->GI == gi) {
			cur_book_view.rowtop = 0;
			cur_book_view.rrow = tr->start - fs->buf;
			fs->crow = 1;
			while (fs->crow < fs->row_count
				   && (fs->rows[fs->crow >> 10] +
					   (fs->crow & 0x3FF))->start - fs->buf <=
				   cur_book_view.rrow)
				fs->crow++;
			fs->crow--;
			return;
		}
	}
	dbg_printf(d, "%s: 没有找到GI值%lu", __func__, gi);
}

static void jump_to_percent(float percent)
{
	dword i;
	p_textrow tr;

	if (percent < 0.0)
		percent = 0.0;
	if (percent > 100.0)
		percent = 100.0;

	i = (dword) (percent * fs->row_count / 100.0);
	tr = fs->rows[i >> 10] + (i & 0x3FF);

	cur_book_view.rowtop = 0;
	cur_book_view.rrow = tr->start - fs->buf;
	fs->crow = 1;
	while (fs->crow < fs->row_count
		   && (fs->rows[fs->crow >> 10] +
			   (fs->crow & 0x3FF))->start - fs->buf <= cur_book_view.rrow)
		fs->crow++;
	fs->crow--;
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
	} else if (key == ctlkey[11] || key == ctlkey2[11]
			   || key == CTRL_PLAYPAUSE) {
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
					(fs->rows[fs->crow >> 10] +
					 (fs->crow & 0x3FF))->start - fs->buf;
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
			if (strchr(buf, '%') != NULL) {
				dbg_printf(d, "%s: 输入百分率%s", __func__, buf);
				float percent;

				if (sscanf(buf, "%f%%", &percent) == 1)
					jump_to_percent(percent);
			} else {
				unsigned long gi = strtoul(buf, NULL, 10);

				if (gi != LONG_MIN) {
					dbg_printf(d, "%s: GI值为%ld", __func__, gi);
					jump_to_gi((dword) gi);
				} else {
					dbg_printf(d, "%s: 输入错误GI值%s", __func__, buf);
				}
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

static void scene_text_delay_action(void)
{
	if (config.dis_scrsave)
		scePowerTick(0);
}

dword scene_reload_raw(const char *title, const unsigned char *data,
					   size_t size, t_fs_filetype ft)
{
	fs = text_open_in_raw(title, data, size, ft, pixelsperrow,
						  config.wordspace, config.encode, config.reordertxt);

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
			   && (fs->rows[fs->crow >> 10] +
				   (fs->crow & 0x3FF))->start - fs->buf <= cur_book_view.rrow)
			fs->crow++;
		fs->crow--;
		cur_book_view.text_needrb = false;
	} else
		fs->crow = cur_book_view.rrow;

	if (fs->crow >= fs->row_count)
		fs->crow = (fs->row_count > 0) ? fs->row_count - 1 : 0;

	return 0;
}

static void _redraw_infobar(dword selidx)
{
	int prev = DISP_BOOK_FONTSIZE;

#ifdef ENABLE_TTF
	ttf_set_pixel_size(cttf, config.infobar_fontsize);
	ttf_set_pixel_size(ettf, config.infobar_fontsize);
#endif
	DISP_BOOK_FONTSIZE = config.infobar_fontsize;
	scene_draw_infobar(&cur_book_view, selidx);
	DISP_BOOK_FONTSIZE = prev;
}

static void redraw_book(dword selidx)
{
	if (config.usettf && !config.ttf_load_to_memory) {
		ttf_lock();
	}
	scene_printbook(&cur_book_view, selidx);
	save_infobar_image();
	_redraw_infobar(selidx);
	scene_draw_scrollbar();
	disp_flip();
	cur_book_view.text_needrp = false;
	if (config.usettf && !config.ttf_load_to_memory) {
		ttf_unlock();
	}
}

static void redraw_infobar(dword selidx)
{
	if (config.usettf && !config.ttf_load_to_memory) {
		ttf_lock();
	}
	disp_duptocache();
	load_infobar_image();
	_redraw_infobar(selidx);
	disp_flip();
	if (config.usettf && !config.ttf_load_to_memory) {
		ttf_unlock();
	}
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
				win_msg(_("文件打开失败"), COLOR_WHITE,
						COLOR_WHITE, config.msgbcolor);
				dbg_printf(d,
						   _
						   ("scene_readbook_raw: 文件%s打开失败 where=%d"),
						   title, where);
				fs = prev_text;
				copy_book_view(&cur_book_view, &prev_book_view);

				scene_readbook_in_raw_mode = prev_raw;
				free_infobar_image();
				return 1;
			}

			cur_book_view.text_needrf = false;
		}
		if (cur_book_view.text_needrp) {
			redraw_book(selidx);
		}

		dword key;

		while ((key = ctrl_read()) == 0) {
			sceKernelDelayThread(20000);
			sceRtcGetCurrentTick(&timer_end);
			if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
				sceRtcGetCurrentTick(&timer_start);
				secticks++;
				if (config.infobar_show_timer) {
					redraw_infobar(selidx);
				}
			}
			if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
				power_down();
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
		int ret;

		ret = book_handle_input(&cur_book_view, &selidx, key);

		if (ret != -1) {
			text_close(fs);
			fs = prev_text;
			copy_book_view(&cur_book_view, &prev_book_view);

			scene_readbook_in_raw_mode = prev_raw;
			free_infobar_image();
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
	free_infobar_image();
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
				free_infobar_image();
				return selidx;
			}
			cur_book_view.text_needrf = false;
		}
		if (cur_book_view.text_needrp) {
			redraw_book(selidx);
		}

		dword key;

		while ((key = ctrl_read()) == 0) {
			sceKernelDelayThread(20000);
			if (config.infobar_show_timer) {
				static u64 start, end;

				sceRtcGetCurrentTick(&end);
				if (pspDiffTime(&end, &start) >= 1.0) {
					redraw_infobar(selidx);
				}
			}
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
				power_down();
				scePowerRequestSuspend();
				secticks = 0;
			}

			if (config.autopage) {
				if (scene_autopage(&cur_book_view, &selidx))
					goto redraw;
			}
			scene_text_delay_action();
		}
		int ret;

		ret = book_handle_input(&cur_book_view, &selidx, key);

		if (ret != -1) {
			scene_power_save(true);
			free_infobar_image();
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
	free_infobar_image();
	return selidx;
}

t_win_menu_op scene_txtkey_menucb(dword key, p_win_menuitem item, dword * count,
								  dword max_height, dword * topindex,
								  dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox
				(_("是否退出软件?"), _("是"), _("否"),
				 COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				scene_exit();
				return win_menu_op_continue;
			}
			return win_menu_op_force_redraw;
		case PSP_CTRL_CIRCLE:
			disp_duptocache();
			disp_waitv();
			prompt_press_any_key();
			disp_flip();
			dword key, key2;
			SceCtrlData ctl;

			do {
				sceCtrlReadBufferPositive(&ctl, 1);
			} while (ctl.Buttons != 0);
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
				key = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			} while ((key &
					  ~(PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT |
						PSP_CTRL_RIGHT)) == 0);
			key2 = key;
			while ((key2 & key) == key) {
				key = key2;
				sceCtrlReadBufferPositive(&ctl, 1);
				key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			}
			if (config.txtkey[*index] == key || config.txtkey2[*index] == key)
				return win_menu_op_force_redraw;
			int i;

			for (i = 0; i < MAX_TXT_KEY; i++) {
				if (i == *index)
					continue;
				if (config.txtkey[i] == key) {
					config.txtkey[i] = config.txtkey2[*index];
					if (config.txtkey[i] == 0) {
						config.txtkey[i] = config.txtkey2[i];
						config.txtkey2[i] = 0;
					}
					break;
				}
				if (config.txtkey2[i] == key) {
					config.txtkey2[i] = config.txtkey2[*index];
					break;
				}
			}
			config.txtkey2[*index] = config.txtkey[*index];
			config.txtkey[*index] = key;
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
			} while (ctl.Buttons != 0);
			return win_menu_op_force_redraw;
		case PSP_CTRL_TRIANGLE:
			config.txtkey[*index] = config.txtkey2[*index];
			config.txtkey2[*index] = 0;
			return win_menu_op_redraw;
		case PSP_CTRL_SQUARE:
			return win_menu_op_cancel;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_txtkey_predraw(p_win_menuitem item, dword index, dword topindex,
						  dword max_height)
{
	char keyname[256];

	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("按键设置   △ 删除"), max_height, &left,
					&right, &upper, &bottom, 8 * DISP_FONTSIZE + 4);

	dword i;

	for (i = topindex; i < topindex + max_height; i++) {
		conf_get_keyname(config.txtkey[i], keyname);
		if (config.txtkey2[i] != 0) {
			char keyname2[256];

			conf_get_keyname(config.txtkey2[i], keyname2);
			STRCAT_S(keyname, _(" | "));
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(left + (right - left) / 2,
					   upper + 2 + (lines + 1 +
									g_predraw.linespace) * (1 +
															DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) keyname);
		lines++;
	}
}

dword scene_txtkey(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));
	t_win_menuitem item[14];

	STRCPY_S(item[0].name, _("书签菜单"));
	STRCPY_S(item[1].name, _("  上一页"));
	STRCPY_S(item[2].name, _("  下一页"));
	STRCPY_S(item[3].name, _(" 前100行"));
	STRCPY_S(item[4].name, _(" 后100行"));
	STRCPY_S(item[5].name, _(" 前500行"));
	STRCPY_S(item[6].name, _(" 后500行"));
	STRCPY_S(item[7].name, _("  第一页"));
	STRCPY_S(item[8].name, _("最后一页"));
	STRCPY_S(item[9].name, _("上一文件"));
	STRCPY_S(item[10].name, _("下一文件"));
	STRCPY_S(item[11].name, _("退出阅读"));
	STRCPY_S(item[12].name, _("切换翻页"));
	STRCPY_S(item[13].name, _("输入GI值"));
	dword i, index;

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? get_bgcolor_by_time() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? get_bgcolor_by_time() : config.
					 menubcolor, true, scene_txtkey_predraw, NULL,
					 scene_txtkey_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}
