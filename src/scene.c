/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pspsdk.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include "xrPrx/xrPrx.h"
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
#include "dbg.h"
#include "simple_gettext.h"

#ifdef ENABLE_PMPAVC
bool pmp_restart = false;
#endif
char appdir[PATH_MAX], copydir[PATH_MAX], cutdir[PATH_MAX];
bool copy_archmode = false;
int copy_where = scene_in_dir;
dword drperpage, rowsperpage, pixelsperrow;
p_bookmark bm = NULL;
p_text fs = NULL;
t_conf config;
p_win_menuitem filelist = NULL, copylist = NULL, cutlist = NULL;
dword filecount = 0, copycount = 0, cutcount = 0;

static bool fat_inited = false;

#ifdef ENABLE_BG
bool repaintbg = true;
#endif
bool imgreading = false, locreading = false;
bool prx_loaded = false;
int locaval[10];
t_fonts fonts[5], bookfonts[21];
static int fontcount = 0, fontindex = 0, bookfontcount = 0, bookfontindex =
	0, ttfsize = 0;
int offset = 0;

bool g_force_text_view_mode = false;

int freq_list[][2] = {
	{15, 33},
	{33, 16},
	{66, 33},
	{111, 55},
	{166, 83},
	{222, 111},
	{266, 133},
	{300, 150},
	{333, 166}
};

extern bool use_prx_power_save;
extern bool img_needrf, img_needrp, img_needrc;

static int config_num = 0;
win_menu_predraw_data g_predraw;

extern int get_center_pos(int left, int right, const char *str)
{
	return left + (right - left) / 2 - strlen(str) * DISP_FONTSIZE / 2 / 2;
}

int default_predraw(const win_menu_predraw_data * pData, const char *str,
					int max_height, int *left, int *right, int *upper,
					int *bottom, int width_fixup)
{
	if (pData == NULL || left == NULL || right == NULL || bottom == NULL)
		return -1;

	*left = pData->left - 1;
	*right =
		pData->x + width_fixup + DISP_FONTSIZE / 2 * pData->max_item_len / 2;
	*upper = pData->upper - (DISP_FONTSIZE + 1) - 1;
	*bottom =
		pData->upper + 2 + 1 + DISP_FONTSIZE + (max_height - 1) * (1 +
																   DISP_FONTSIZE
																   +
																   pData->
																   linespace);

	disp_rectangle(*left, *upper, *right, *bottom, COLOR_WHITE);
	disp_fillrect(*left + 1, *upper + 1,
				  *right - 1, *bottom - 1,
				  config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(get_center_pos(*left, *right, str), *upper + 1,
				   COLOR_WHITE, (const byte *) str);
	disp_line(*left, *upper + 1 + DISP_FONTSIZE,
			  *right, *upper + 1 + DISP_FONTSIZE, COLOR_WHITE);

	return 0;
}

static void load_fontsize_to_config()
{
	if (!config.usettf) {
		config.fontsize = fonts[fontindex].size;
		config.bookfontsize = bookfonts[bookfontindex].size;
	} else {
		config.fontsize = fonts[fontindex].size;
		config.bookfontsize = ttfsize;
	}
}

t_win_menu_op exit_confirm()
{
	if (win_msgbox(_("是否退出软件?"), _("是"), _("否"),
				   COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
		scene_exit();
		return win_menu_op_continue;
	}
	return win_menu_op_force_redraw;
}

bool scene_load_font()
{
	char fontzipfile[PATH_MAX], efontfile[PATH_MAX], cfontfile[PATH_MAX];

	if (fontindex >= fontcount)
		fontindex = 0;
	config.fontsize = fonts[fontindex].size;
	STRCPY_S(fontzipfile, appdir);
	STRCAT_S(fontzipfile, "fonts.zip");
	SPRINTF_S(efontfile, "ASC%d", config.fontsize);
	SPRINTF_S(cfontfile, "GBK%d", config.fontsize);
	scene_power_save(false);
	if (!disp_load_zipped_font(fontzipfile, efontfile, cfontfile)) {
		SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, config.fontsize);
		SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, config.fontsize);
		if (!disp_load_font(efontfile, cfontfile)) {
			scene_power_save(true);
			return false;
		}
	}
	disp_set_fontsize(config.fontsize);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	if (config.lyricex > (155 / (DISP_FONTSIZE + 1) - 1) / 2)
		config.lyricex = (155 / (DISP_FONTSIZE + 1) - 1) / 2;
#else
	config.lyricex = 0;
#endif
	scene_power_save(true);
	return true;
}

bool scene_load_book_font()
{
	char fontzipfile[PATH_MAX], efontfile[PATH_MAX], cfontfile[PATH_MAX];

	if (bookfontindex >= bookfontcount)
		bookfontindex = 0;
	if (config.usettf)
		config.bookfontsize = ttfsize;
	else {
		config.bookfontsize = bookfonts[bookfontindex].size;
		if (config.bookfontsize == config.fontsize) {
			disp_assign_book_font();
			disp_set_book_fontsize(config.bookfontsize);
			memset(disp_ewidth, config.bookfontsize / 2, 0x80);
			return true;
		}
	}
	bool loaded = false;

#ifdef ENABLE_TTF
	if (config.usettf) {
		scene_power_save(false);
		loaded =
			disp_load_zipped_truetype_book_font(config.ettfarch,
												config.cttfarch,
												config.ettfpath,
												config.cttfpath,
												config.bookfontsize);
		scene_power_save(imgreading || fs != NULL);
	}
#endif
	if (!loaded) {
		config.usettf = false;
		STRCPY_S(fontzipfile, appdir);
		STRCAT_S(fontzipfile, "fonts.zip");
		SPRINTF_S(efontfile, "ASC%d", config.bookfontsize);
		SPRINTF_S(cfontfile, "GBK%d", config.bookfontsize);
		scene_power_save(false);
		if (!disp_load_zipped_book_font(fontzipfile, efontfile, cfontfile)) {
			SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, config.bookfontsize);
			SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, config.bookfontsize);
			if (!disp_load_book_font(efontfile, cfontfile)) {
				scene_power_save(true);
				return false;
			}
		}
		memset(disp_ewidth, config.bookfontsize / 2, 0x80);
	}
	disp_set_book_fontsize(config.bookfontsize);
	scene_power_save(true);
	return true;
}

extern int prompt_press_any_key(void)
{
	int len = strlen(_("请按对应按键")) / 2 + 1;
	int left = 240 - DISP_FONTSIZE * len / 2;
	int right = 240 + DISP_FONTSIZE * len / 2;
	int center = get_center_pos(left + 1, right - 1, _("请按对应按键"));

	disp_rectangle(left,
				   135 - DISP_FONTSIZE / 2,
				   right, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
	disp_fillrect(left + 1,
				  136 - DISP_FONTSIZE / 2,
				  right - 1, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
	disp_putstring(center,
				   136 - DISP_FONTSIZE / 2, COLOR_WHITE,
				   (const byte *) _("请按对应按键"));

	return 0;
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

			for (i = 0; i < 12; i++) {
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
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
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
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
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
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_txtkey_predraw, NULL,
					 scene_txtkey_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}

t_win_menu_op scene_flkey_menucb(dword key, p_win_menuitem item, dword * count,
								 dword max_height, dword * topindex,
								 dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
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
				sceKernelDelayThread(20000);
			}
			if (config.flkey[*index] == key || config.flkey2[*index] == key)
				return win_menu_op_force_redraw;
			int i;

			for (i = 0; i < 7; i++) {
				if (i == *index)
					continue;
				if (config.flkey[i] == key) {
					config.flkey[i] = config.flkey2[*index];
					if (config.flkey[i] == 0) {
						config.flkey[i] = config.flkey2[i];
						config.flkey2[i] = 0;
					}
					break;
				}
				if (config.flkey2[i] == key) {
					config.flkey2[i] = config.flkey2[*index];
					break;
				}
			}
			config.flkey2[*index] = config.flkey[*index];
			config.flkey[*index] = key;
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
			} while (ctl.Buttons != 0);
			return win_menu_op_force_redraw;
		case PSP_CTRL_TRIANGLE:
			config.flkey[*index] = config.flkey2[*index];
			config.flkey2[*index] = 0;
			return win_menu_op_redraw;
		case PSP_CTRL_SQUARE:
			return win_menu_op_cancel;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_flkey_predraw(p_win_menuitem item, dword index, dword topindex,
						 dword max_height)
{
	char keyname[256];
	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("按键设置   △ 删除"), max_height, &left,
					&right, &upper, &bottom, 8 * DISP_FONTSIZE + 4);

	dword i;

	for (i = topindex; i < topindex + max_height; i++) {
		conf_get_keyname(config.flkey[i], keyname);
		if (config.flkey2[i] != 0) {
			char keyname2[256];

			conf_get_keyname(config.flkey2[i], keyname2);
			STRCAT_S(keyname, _(" | "));
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(left + (right - left) / 2,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) keyname);
		lines++;
	}
}

dword scene_flkey(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[7];

	STRCPY_S(item[0].name, _("    选定"));
	STRCPY_S(item[1].name, _("  第一项"));
	STRCPY_S(item[2].name, _("最后一项"));
	STRCPY_S(item[3].name, _("上级目录"));
	STRCPY_S(item[4].name, _("刷新列表"));
	STRCPY_S(item[5].name, _("文件操作"));
	STRCPY_S(item[6].name, _("选择文件"));
	dword i, index;

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
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
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_flkey_predraw, NULL,
					 scene_flkey_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}

static const char *GetFileExt(const char *filename)
{
	const char *strExt = strrchr(filename, '.');

	if (strExt == NULL)
		return "";
	else
		return strExt;
}

#ifdef ENABLE_BG
static void set_background_image(p_win_menuitem item, dword * index)
{
	char bgfile[PATH_MAX], bgarchname[PATH_MAX];

	if (where == scene_in_dir) {
		STRCPY_S(bgfile, config.shortpath);
		STRCAT_S(bgfile, item[*index].shortname->ptr);
		bgarchname[0] = '\0';
	} else {
		STRCPY_S(bgarchname, config.shortpath);
		STRCPY_S(bgfile, item[*index].compname->ptr);
	}

	dbg_printf(d, "bgarchname: %s", bgarchname);
	dbg_printf(d, "bgfile: %s", bgfile);

	if ((where == scene_in_dir && stricmp(bgfile, config.bgfile) == 0) ||
		(where != scene_in_dir && stricmp(bgfile, config.bgfile) == 0 &&
		 stricmp(bgarchname, config.bgarch) == 0)
		) {
		if (win_msgbox
			(_("是否取消背景图？"), _("是"), _("否"), COLOR_WHITE,
			 COLOR_WHITE, config.msgbcolor)) {
			dbg_printf(d, _("取消背景图"));
			STRCPY_S(config.bgfile, "");
			STRCPY_S(config.bgarch, "");
			config.bgwhere = scene_in_dir;
			bg_cancel();
			disp_fillvram(0);
			disp_flip();
			disp_fillvram(0);
		}
	} else {
		if (win_msgbox
			(_("是否将当前图片文件设为背景图？"),
			 _("是"), _("否"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
			config.bgwhere = where;
			STRCPY_S(config.bgfile, bgfile);
			STRCPY_S(config.bgarch, bgarchname);
			dbg_printf(d, "bgarch: %s", config.bgarch);
			dbg_printf(d, "bgfile: %s", config.bgfile);
			dbg_printf(d, "bgwhere: %d", config.bgwhere);
			bg_load(config.bgfile,
					config.bgarch, config.bgcolor, (t_fs_filetype)
					item[*index].data, config.grayscale, config.bgwhere);
			repaintbg = true;
		}
	}
}
#endif

int scene_filelist_compare_ext(void *data1, void *data2)
{
	if (((p_win_menuitem) data1)->compname == NULL
		|| ((p_win_menuitem) data2)->compname == NULL) {
		dbg_printf(d, "%s: !!!!", __func__);
		return 0;
	}
	const char *fn1 = ((p_win_menuitem) data1)->compname->ptr, *fn2 =
		((p_win_menuitem) data2)->compname->ptr;
	int cmp = stricmp(GetFileExt(fn1), GetFileExt(fn2));

	if (cmp)
		return cmp;
	return stricmp(fn1, fn2);
}

int scene_filelist_compare_name(void *data1, void *data2)
{
	if (((p_win_menuitem) data1)->compname == NULL
		|| ((p_win_menuitem) data2)->compname == NULL) {
		dbg_printf(d, "%s: !!!!", __func__);
		return 0;
	}
	t_fs_filetype ft1 = (t_fs_filetype) ((p_win_menuitem) data1)->data, ft2 =
		(t_fs_filetype) ((p_win_menuitem) data2)->data;
	if (ft1 == fs_filetype_dir) {
		if (ft2 != fs_filetype_dir)
			return -1;
	} else if (ft2 == fs_filetype_dir)
		return 1;
	return stricmp(((p_win_menuitem) data1)->compname->ptr,
				   ((p_win_menuitem) data2)->compname->ptr);
}

int scene_filelist_compare_size(void *data1, void *data2)
{
	t_fs_filetype ft1 = (t_fs_filetype) ((p_win_menuitem) data1)->data, ft2 =
		(t_fs_filetype) ((p_win_menuitem) data2)->data;
	if (ft1 == fs_filetype_dir) {
		if (ft2 != fs_filetype_dir)
			return -1;
	} else if (ft2 == fs_filetype_dir)
		return 1;
	return (int) ((p_win_menuitem) data1)->data3 -
		(int) ((p_win_menuitem) data2)->data3;
}

int scene_filelist_compare_ctime(void *data1, void *data2)
{
	t_fs_filetype ft1 = (t_fs_filetype) ((p_win_menuitem) data1)->data, ft2 =
		(t_fs_filetype) ((p_win_menuitem) data2)->data;
	if (ft1 == fs_filetype_dir) {
		if (ft2 != fs_filetype_dir)
			return -1;
	} else if (ft2 == fs_filetype_dir)
		return 1;
	return (((int) ((p_win_menuitem) data1)->data2[0]) << 16) +
		(int) ((p_win_menuitem) data1)->data2[1] -
		((((int) ((p_win_menuitem) data2)->data2[0]) << 16) +
		 (int) ((p_win_menuitem) data2)->data2[1]);
}

int scene_filelist_compare_mtime(void *data1, void *data2)
{
	t_fs_filetype ft1 = (t_fs_filetype) ((p_win_menuitem) data1)->data, ft2 =
		(t_fs_filetype) ((p_win_menuitem) data2)->data;
	if (ft1 == fs_filetype_dir) {
		if (ft2 != fs_filetype_dir)
			return -1;
	} else if (ft2 == fs_filetype_dir)
		return 1;
	return (((int) ((p_win_menuitem) data1)->data2[2]) << 16) +
		(int) ((p_win_menuitem) data1)->data2[3] -
		((((int) ((p_win_menuitem) data2)->data2[2]) << 16) +
		 (int) ((p_win_menuitem) data2)->data2[3]);
}

qsort_compare compare_func[] = {
	scene_filelist_compare_ext,
	scene_filelist_compare_name,
	scene_filelist_compare_size,
	scene_filelist_compare_ctime,
	scene_filelist_compare_mtime
};

#ifdef ENABLE_IMAGE
t_win_menu_op scene_ioptions_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
					config.bicubic = !config.bicubic;
					break;
				case 1:
					if (config.slideinterval == 1)
						config.slideinterval = 120;
					else
						config.slideinterval--;
					break;
				case 2:
					if (config.viewpos == conf_viewpos_leftup)
						config.viewpos = conf_viewpos_rightdown;
					else
						config.viewpos--;
					break;
				case 3:
					if (config.imgmvspd == 1)
						config.imgmvspd = 20;
					else
						config.imgmvspd--;
					break;
				case 4:
					if (config.imgpaging == conf_imgpaging_direct)
						config.imgpaging = conf_imgpaging_leftright;
					else
						config.imgpaging--;
					break;
				case 5:
					if (config.imgpagereserve == 0)
						config.imgpagereserve = 50;
					else
						config.imgpagereserve--;
					break;
				case 6:
					if (config.thumb == conf_thumb_none)
						config.thumb = conf_thumb_always;
					else
						config.thumb--;
					break;
				case 7:
					if (--config.imgbrightness < 0)
						config.imgbrightness = 0;
					img_needrf = img_needrc = img_needrp = true;
					break;
				case 8:
					config.img_enable_analog = !config.img_enable_analog;
					break;
				case 9:
					config.load_exif = !config.load_exif;
					img_needrp = true;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
					config.bicubic = !config.bicubic;
					break;
				case 1:
					if (config.slideinterval == 120)
						config.slideinterval = 1;
					else
						config.slideinterval++;
					break;
				case 2:
					if (config.viewpos == conf_viewpos_rightdown)
						config.viewpos = conf_viewpos_leftup;
					else
						config.viewpos++;
					break;
				case 3:
					if (config.imgmvspd == 20)
						config.imgmvspd = 1;
					else
						config.imgmvspd++;
					break;
				case 4:
					if (config.imgpaging == conf_imgpaging_leftright)
						config.imgpaging = conf_imgpaging_direct;
					else
						config.imgpaging++;
					break;
				case 5:
					if (config.imgpagereserve == 50)
						config.imgpagereserve = 0;
					else
						config.imgpagereserve++;
					break;
				case 6:
					if (config.thumb == conf_thumb_always)
						config.thumb = conf_thumb_none;
					else
						config.thumb++;
					break;
				case 7:
					if (++config.imgbrightness > 100)
						config.imgbrightness = 100;
					img_needrf = img_needrc = img_needrp = true;
					break;
				case 8:
					config.img_enable_analog = !config.img_enable_analog;
					break;
				case 9:
					config.load_exif = !config.load_exif;
					img_needrp = true;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_LTRIGGER:
			switch (*index) {
				case 1:
					if (config.slideinterval < 11)
						config.slideinterval = 1;
					else
						config.slideinterval -= 10;
					break;
				case 5:
					if (config.imgpagereserve < 10)
						config.imgpagereserve = 0;
					else
						config.imgpagereserve -= 10;
					break;
				case 7:
					config.imgbrightness -= 10;
					if (config.imgbrightness < 0)
						config.imgbrightness = 0;
					img_needrf = img_needrc = img_needrp = true;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RTRIGGER:
			switch (*index) {
				case 1:
					if (config.slideinterval > 110)
						config.slideinterval = 120;
					else
						config.slideinterval += 10;
					break;
				case 5:
					if (config.imgpagereserve > 40)
						config.imgpagereserve = 50;
					else
						config.imgpagereserve += 10;
					break;
				case 7:
					config.imgbrightness += 10;
					if (config.imgbrightness > 100)
						config.imgbrightness = 100;
					img_needrf = img_needrc = img_needrp = true;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_ioptions_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	int left, right, upper, bottom, lines = 0;

	if (strcmp(simple_textdomain(NULL), "zh_CN") == 0)
		default_predraw(&g_predraw, _("看图选项"), max_height, &left, &right,
						&upper, &bottom, 4 * DISP_FONTSIZE + 4);
	else
		default_predraw(&g_predraw, _("看图选项"), max_height, &left, &right,
						&upper, &bottom, 6 * DISP_FONTSIZE + 4);
	char number[20];

	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.
								   bicubic ? _("三次立方") : _("两次线性")));
	lines++;
	SPRINTF_S(number, "%d %s", config.slideinterval, _("秒"));
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_viewposname(config.viewpos));
	lines++;
	memset(number, ' ', 4);
	SPRINTF_S(number, "%d", config.imgmvspd);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_imgpagingname(config.imgpaging));
	lines++;
	SPRINTF_S(number, "%d", config.imgpagereserve);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_thumbname(config.thumb));
	lines++;
	char infomsg[80];

	SPRINTF_S(infomsg, "%d%%", config.imgbrightness);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   config.
				   img_enable_analog ? (const byte *) _("是") : (const byte *)
				   _("否"));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   config.load_exif ? (const byte *) _("是") : (const byte *)
				   _("否"));
	lines++;
}

dword scene_ioptions(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[10];
	dword i, index;

	STRCPY_S(item[0].name, _("    缩放算法"));
	STRCPY_S(item[1].name, _("幻灯播放间隔"));
	STRCPY_S(item[2].name, _("图片起始位置"));
	STRCPY_S(item[3].name, _("    卷动速度"));
	STRCPY_S(item[4].name, _("    翻页模式"));
	STRCPY_S(item[5].name, _("翻动保留宽度"));
	STRCPY_S(item[6].name, _("  缩略图查看"));
	STRCPY_S(item[7].name, _("    图像亮度"));
	STRCPY_S(item[8].name, _("  启用类比键"));
	STRCPY_S(item[9].name, _("查看EXIF信息"));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.linespace = 0;
	g_predraw.left = g_predraw.x - (DISP_FONTSIZE / 2) * g_predraw.max_item_len;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_ioptions_predraw, NULL,
					 scene_ioptions_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}
#endif

t_win_menu_op scene_color_menucb(dword key, p_win_menuitem item, dword * count,
								 dword max_height, dword * topindex,
								 dword * index)
{
	dword r, g, b;

	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
					r = RGB_R(config.forecolor);
					g = RGB_G(config.forecolor);
					b = RGB_B(config.forecolor);
					if (r == 0)
						r = COLOR_MAX;
					else
						r--;
					config.forecolor = RGB2(r, g, b);
					break;
				case 1:
					r = RGB_R(config.forecolor);
					g = RGB_G(config.forecolor);
					b = RGB_B(config.forecolor);
					if (g == 0)
						g = COLOR_MAX;
					else
						g--;
					config.forecolor = RGB2(r, g, b);
					break;
				case 2:
					r = RGB_R(config.forecolor);
					g = RGB_G(config.forecolor);
					b = RGB_B(config.forecolor);
					if (b == 0)
						b = COLOR_MAX;
					else
						b--;
					config.forecolor = RGB2(r, g, b);
					break;
				case 3:
					r = RGB_R(config.bgcolor);
					g = RGB_G(config.bgcolor);
					b = RGB_B(config.bgcolor);
					if (r == 0)
						r = COLOR_MAX;
					else
						r--;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 4:
					r = RGB_R(config.bgcolor);
					g = RGB_G(config.bgcolor);
					b = RGB_B(config.bgcolor);
					if (g == 0)
						g = COLOR_MAX;
					else
						g--;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 5:
					r = RGB_R(config.bgcolor);
					g = RGB_G(config.bgcolor);
					b = RGB_B(config.bgcolor);
					if (b == 0)
						b = COLOR_MAX;
					else
						b--;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 6:
					if (config.grayscale == 0)
						config.grayscale = 100;
					else
						config.grayscale--;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (r == COLOR_MAX)
						r = 0;
					else
						r++;
					config.forecolor = RGB2(r, g, b);
					break;
				case 1:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (g == COLOR_MAX)
						g = 0;
					else
						g++;
					config.forecolor = RGB2(r, g, b);
					break;
				case 2:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (b == COLOR_MAX)
						b = 0;
					else
						b++;
					config.forecolor = RGB2(r, g, b);
					break;
				case 3:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (r == COLOR_MAX)
						r = 0;
					else
						r++;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 4:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (g == COLOR_MAX)
						g = 0;
					else
						g++;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 5:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (b == COLOR_MAX)
						b = 0;
					else
						b++;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 6:
					if (config.grayscale == 100)
						config.grayscale = 0;
					else
						config.grayscale++;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
		case PSP_CTRL_SQUARE:
			return win_menu_op_continue;
		case PSP_CTRL_LTRIGGER:
			switch (*index) {
				case 0:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (r > (COLOR_MAX * 25 / 255))
						r -= (COLOR_MAX * 25 / 255);
					else
						r = 0;
					config.forecolor = RGB2(r, g, b);
					break;
				case 1:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (g > (COLOR_MAX * 25 / 255))
						g -= (COLOR_MAX * 25 / 255);
					else
						g = 0;
					config.forecolor = RGB2(r, g, b);
					break;
				case 2:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (b > (COLOR_MAX * 25 / 255))
						b -= (COLOR_MAX * 25 / 255);
					else
						b = 0;
					config.forecolor = RGB2(r, g, b);
					break;
				case 3:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (r > (COLOR_MAX * 25 / 255))
						r -= (COLOR_MAX * 25 / 255);
					else
						r = 0;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 4:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (g > (COLOR_MAX * 25 / 255))
						g -= (COLOR_MAX * 25 / 255);
					else
						g = 0;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 5:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (b > (COLOR_MAX * 25 / 255))
						b -= (COLOR_MAX * 25 / 255);
					else
						b = 0;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 6:
					if (config.grayscale < 10)
						config.grayscale = 0;
					else
						config.grayscale -= 10;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RTRIGGER:
			switch (*index) {
				case 0:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (r < COLOR_MAX - (COLOR_MAX * 25 / 255))
						r += (COLOR_MAX * 25 / 255);
					else
						r = COLOR_MAX;
					config.forecolor = RGB2(r, g, b);
					break;
				case 1:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (g < COLOR_MAX - (COLOR_MAX * 25 / 255))
						g += (COLOR_MAX * 25 / 255);
					else
						g = COLOR_MAX;
					config.forecolor = RGB2(r, g, b);
					break;
				case 2:
					r = RGB_R(config.forecolor), g =
						RGB_G(config.forecolor), b = RGB_B(config.forecolor);
					if (b < COLOR_MAX - (COLOR_MAX * 25 / 255))
						b += (COLOR_MAX * 25 / 255);
					else
						b = COLOR_MAX;
					config.forecolor = RGB2(r, g, b);
					break;
				case 3:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (r < COLOR_MAX - (COLOR_MAX * 25 / 255))
						r += (COLOR_MAX * 25 / 255);
					else
						r = COLOR_MAX;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 4:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (g < COLOR_MAX - (COLOR_MAX * 25 / 255))
						g += (COLOR_MAX * 25 / 255);
					else
						g = COLOR_MAX;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 5:
					r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b =
						RGB_B(config.bgcolor);
					if (b < COLOR_MAX - (COLOR_MAX * 25 / 255))
						b += (COLOR_MAX * 25 / 255);
					else
						b = COLOR_MAX;
					config.bgcolor = RGB2(r, g, b);
					break;
				case 6:
					if (config.grayscale > 90)
						config.grayscale = 100;
					else
						config.grayscale += 10;
					break;
			}
			return win_menu_op_redraw;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_color_predraw(p_win_menuitem item, dword index, dword topindex,
						 dword max_height)
{
	char number[5];
	int pad = 0;

	if (config.fontsize <= 10) {
		pad = 6 * (12 - config.fontsize);
	}

	int left, right, upper, bottom, lines = 0;
	int npad;

	if (strcmp(simple_textdomain(NULL), "zh_CN") == 0)
		npad = 35;
	else
		npad = 0;

	default_predraw(&g_predraw, _("颜色选项"), max_height, &left, &right,
					&upper, &bottom, pad + npad);

	disp_fillrect(g_predraw.x + 40, upper + 3 + DISP_FONTSIZE, g_predraw.x + 70,
				  upper + 3 + DISP_FONTSIZE + 30, config.forecolor);
	disp_fillrect(g_predraw.x + 40,
				  upper + 2 + (3 + 1 + g_predraw.linespace) * (1 +
															   DISP_FONTSIZE),
				  g_predraw.x + 70,
				  upper + 2 + (3 + 1 + g_predraw.linespace) * (1 +
															   DISP_FONTSIZE) +
				  30, config.bgcolor);

	memset(number, ' ', 4);
	utils_dword2string(RGB_R(config.forecolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(RGB_G(config.forecolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(RGB_B(config.forecolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(RGB_R(config.bgcolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(RGB_G(config.bgcolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(RGB_B(config.bgcolor), number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
#ifdef ENABLE_BG
	utils_dword2string(config.grayscale, number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
#else
	disp_putstring(g_predraw.x + 8,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) _("不支持"));
	lines++;
#endif
}

dword scene_color(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	char infomsg[80];
	t_win_menuitem item[7];
	dword i;

	SPRINTF_S(infomsg, "%s：%s", _("字体颜色"), _("红"));
	STRCPY_S(item[0].name, infomsg);
	SPRINTF_S(infomsg, "%10s%s", " ", _("绿"));
	STRCPY_S(item[1].name, infomsg);
	SPRINTF_S(infomsg, "%10s%s", " ", _("蓝"));
	STRCPY_S(item[2].name, infomsg);
	SPRINTF_S(infomsg, "%s：%s", _("背景颜色"), _("红"));
	STRCPY_S(item[3].name, infomsg);
#ifdef ENABLE_BG
	SPRINTF_S(infomsg, "(%s)  %s", _("灰度色"), _("绿"));
	STRCPY_S(item[4].name, infomsg);
#else
	SPRINTF_S(infomsg, "%10s%s", " ", _("绿"));
	STRCPY_S(item[4].name, infomsg);
#endif
	SPRINTF_S(infomsg, "%10s%s", " ", _("蓝"));
	STRCPY_S(item[5].name, infomsg);
	STRCPY_S(item[6].name, _("  背景图灰度"));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

#ifdef ENABLE_BG
	dword orgbgcolor = config.bgcolor;
	dword orggrayscale = config.grayscale;
#endif
	dword index;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_color_predraw, NULL,
					 scene_color_menucb)) != INVALID);

#ifdef ENABLE_BG
	if (orgbgcolor != config.bgcolor || orggrayscale != config.grayscale) {
		dbg_printf(d, "更换背景: %s %s gray: %d", config.bgarch, config.bgfile,
				   config.grayscale);
		bg_load(config.bgfile, config.bgarch, config.bgcolor,
				fs_file_get_type(config.bgfile), config.grayscale,
				config.bgwhere);
	}
#endif

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return 0;
}

t_win_menu_op scene_boptions_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
					if (config.infobar == conf_infobar_none)
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
						config.infobar = conf_infobar_lyric;
#else
						config.infobar = conf_infobar_info;
#endif
					else
						config.infobar--;
					break;
				case 1:
					config.rlastrow = !config.rlastrow;
					break;
				case 2:
					if (config.vertread == 0)
						config.vertread = 3;
					else
						config.vertread--;
					break;
				case 3:
					if (config.encode == 0)
						config.encode = 4;
					else
						config.encode--;
					break;
				case 4:
					config.scrollbar = !config.scrollbar;
					break;
				case 5:
					config.autobm = !config.autobm;
					break;
				case 6:
					config.reordertxt = !config.reordertxt;
					break;
				case 7:
					config.pagetonext = !config.pagetonext;
					break;
				case 8:
					if (--config.autopagetype < 0)
						config.autopagetype = 2;
					config.prev_autopage = config.autopagetype;
					break;
				case 9:
					if (--config.autopage < -1000)
						config.autopage = -1000;
					break;
				case 10:
					if (--config.autolinedelay < 0)
						config.autolinedelay = 0;
					break;
				case 11:
					config.enable_analog = !config.enable_analog;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
					if (config.infobar == conf_infobar_lyric)
#else
					if (config.infobar == conf_infobar_info)
#endif
						config.infobar = conf_infobar_none;
					else
						config.infobar++;
					break;
				case 1:
					config.rlastrow = !config.rlastrow;
					break;
				case 2:
					if (config.vertread == 3)
						config.vertread = 0;
					else
						config.vertread++;
					break;
				case 3:
					if (config.encode == 4)
						config.encode = 0;
					else
						config.encode++;
					break;
				case 4:
					config.scrollbar = !config.scrollbar;
					break;
				case 5:
					config.autobm = !config.autobm;
					break;
				case 6:
					config.reordertxt = !config.reordertxt;
					break;
				case 7:
					config.pagetonext = !config.pagetonext;
					break;
				case 8:
					config.autopagetype = (config.autopagetype + 1) % 3;
					config.prev_autopage = config.autopagetype;
					break;
				case 9:
					if (++config.autopage > 1000)
						config.autopage = 1000;
					break;
				case 10:
					if (++config.autolinedelay > 1000)
						config.autolinedelay = 1000;
					break;
				case 11:
					config.enable_analog = !config.enable_analog;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_boptions_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	char number[5];

	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("阅读选项"), max_height, &left, &right,
					&upper, &bottom, 2 * DISP_FONTSIZE + 4);

	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_infobarname(config.infobar));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.rlastrow ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) ((config.vertread == 3) ? _("颠倒")
								   : (config.vertread ==
									  2) ? _("左向") : (config.vertread ==
														1 ? _("右向") :
														_("无"))));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_encodename(config.encode));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.scrollbar ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.autobm ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.reordertxt ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.
								   pagetonext ? _("下篇文章") : _("无动作")));
	lines++;
	if (config.autopagetype == 2) {
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) _("无"));
		lines++;
	} else if (config.autopagetype == 1) {
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) _("自动滚屏"));
		lines++;
	} else {
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) _("自动翻页"));
		lines++;
	}
	if (config.autopage) {
		char infomsg[80];

		SPRINTF_S(infomsg, "%2d", config.autopage);
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) infomsg);
		lines++;
	} else {
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) _("已关闭"));
		lines++;
	}
	if (config.autolinedelay) {
		memset(number, ' ', 4);
		utils_dword2string(config.autolinedelay, number, 2);
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) number);
		lines++;
	} else {
		disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
					   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																		DISP_FONTSIZE),
					   COLOR_WHITE, (const byte *) _("已关闭"));
		lines++;
	}
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   config.
				   enable_analog ? (const byte *) _("是") : (const byte *)
				   _("否"));
	lines++;
}

static void recalcSize(dword * drperpage, dword * rowsperpage,
					   dword * pixelsperrow)
{
	int t = config.vertread;

	if (t == 3)
		t = 0;

	*drperpage =
		((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) -
		 config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 -
		 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	*rowsperpage =
		((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) -
		 (config.infobar != conf_infobar_none ? DISP_BOOK_FONTSIZE : 0) -
		 config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	*pixelsperrow = (t ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT)
					 : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) -
		config.borderspace * 2;
}

dword scene_boptions(dword * selidx)
{
	t_win_menuitem item[12];
	dword i;

	STRCPY_S(item[0].name, _("      信息栏"));
	STRCPY_S(item[1].name, _("翻页保留一行"));
	STRCPY_S(item[2].name, _("    文字竖看"));
	STRCPY_S(item[3].name, _("    文字编码"));
	STRCPY_S(item[4].name, _("      滚动条"));
	STRCPY_S(item[5].name, _("自动保存书签"));
	STRCPY_S(item[6].name, _("重新编排文本"));
	STRCPY_S(item[7].name, _("文章末尾翻页"));
	STRCPY_S(item[8].name, _("自动翻页模式"));
	if (!config.autopagetype) {
		STRCPY_S(item[9].name, _("    翻页时间"));
	} else
		STRCPY_S(item[9].name, _("    滚屏速度"));
	STRCPY_S(item[10].name, _("    滚屏时间"));
	STRCPY_S(item[11].name, _("  启用类比键"));

	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	bool orgvert = config.vertread;
	bool orgibar = config.infobar;
	bool orgscrollbar = config.scrollbar;
	dword orgreordertxt = config.reordertxt;
	dword orgencode = config.encode;
	dword orgrowspace = config.rowspace;
	dword orgwordspace = config.wordspace;
	dword orgborderspace = config.borderspace;

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
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_boptions_predraw, NULL,
					 scene_boptions_menucb)) != INVALID);

	dword result = 0;

	if (orgibar != config.infobar || orgvert != config.vertread
		|| orgrowspace != config.rowspace
		|| orgwordspace != config.wordspace
		|| orgborderspace != config.borderspace
		|| orgscrollbar != config.scrollbar) {
		dword orgpixelsperrow = pixelsperrow;

		recalcSize(&drperpage, &rowsperpage, &pixelsperrow);
		cur_book_view.text_needrf = true;
		if (orgpixelsperrow != pixelsperrow)
			result = 4;
	}
	if (orgreordertxt != config.reordertxt)
		result = 4;
	if (fs != NULL && fs->ucs == 0 && orgencode != config.encode)
		result = 3;

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return result;
}

t_win_menu_op scene_ctrlset_menucb(dword key, p_win_menuitem item,
								   dword * count, dword max_height,
								   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
#ifdef ENABLE_HPRM
					config.hprmctrl = !config.hprmctrl;
#endif
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
#ifdef ENABLE_HPRM
					config.hprmctrl = !config.hprmctrl;
#endif
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_ctrlset_predraw(p_win_menuitem item, dword index, dword topindex,
						   dword max_height)
{
	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("操作设置"), max_height, &left, &right,
					&upper, &bottom, 4 * DISP_FONTSIZE + 4);

#ifdef ENABLE_HPRM
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.
								   hprmctrl ? _("控制翻页") : _("控制音乐")));
	lines++;
#else
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) _("不支持"));
	lines++;
#endif
}

dword scene_ctrlset(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[1];
	dword i;

	STRCPY_S(item[0].name, _("    线控模式"));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));
	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123 - 3 * DISP_FONTSIZE;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - (g_predraw.item_count - 1) * DISP_FONTSIZE;
	g_predraw.linespace = 0;

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;

#ifdef ENABLE_HPRM
	bool orghprmctrl = config.hprmctrl;
#endif
	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_ctrlset_predraw, NULL,
					 scene_ctrlset_menucb)) != INVALID);
#ifdef ENABLE_HPRM
	if (orghprmctrl != config.hprmctrl) {
		ctrl_enablehprm(config.hprmctrl);
#ifdef ENABLE_MUSIC
		mp3_set_hprm(!config.hprmctrl);
#endif
	}
#endif
	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}

t_win_menu_op scene_fontsel_menucb(dword key, p_win_menuitem item,
								   dword * count, dword max_height,
								   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
					if (fontindex == 0)
						fontindex = fontcount - 1;
					else
						fontindex--;
					break;
				case 1:
					if (config.usettf) {
						ttfsize--;
						if (ttfsize < 8)
							ttfsize = 128;
					} else {
						if (bookfontindex == 0)
							bookfontindex = bookfontcount - 1;
						else
							bookfontindex--;
					}
					break;
				case 2:
					if (config.wordspace == 0)
						config.wordspace = 128;
					else {
						config.wordspace--;
						cur_book_view.text_needrf = true;
					}
					break;
				case 3:
					if (config.rowspace == 0)
						config.rowspace = 128;
					else
						config.rowspace--;
					break;
				case 4:
					if (config.borderspace == 0)
						config.borderspace = 10;
					else
						config.borderspace--;
					break;
				case 5:
#ifdef ENABLE_TTF
					config.usettf = !config.usettf;
					if (config.usettf)
						ttfsize = bookfonts[bookfontindex].size;
					else {
						bookfontindex = 0;
						while (bookfontindex < bookfontcount
							   && bookfonts[bookfontindex].size <= ttfsize)
							bookfontindex++;
						if (bookfontindex > 0)
							bookfontindex--;
					}
#endif
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
					if (fontindex == fontcount - 1)
						fontindex = 0;
					else
						fontindex++;
					break;
				case 1:
					if (config.usettf) {
						ttfsize++;
						if (ttfsize > 128)
							ttfsize = 8;
					} else {
						if (bookfontindex == bookfontcount - 1)
							bookfontindex = 0;
						else
							bookfontindex++;
					}
					break;
				case 2:
					if (config.wordspace == 128)
						config.wordspace = 0;
					else {
						config.wordspace++;
						cur_book_view.text_needrf = true;
					}
					break;
				case 3:
					if (config.rowspace == 128)
						config.rowspace = 0;
					else
						config.rowspace++;
					break;
				case 4:
					if (config.borderspace == 10)
						config.borderspace = 0;
					else
						config.borderspace++;
					break;
				case 5:
#ifdef ENABLE_TTF
					config.usettf = !config.usettf;
					if (config.usettf)
						ttfsize = bookfonts[bookfontindex].size;
					else {
						bookfontindex = 0;
						while (bookfontindex < bookfontcount
							   && bookfonts[bookfontindex].size <= ttfsize)
							bookfontindex++;
						if (bookfontindex > 0)
							bookfontindex--;
					}
#endif
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_fontsel_predraw(p_win_menuitem item, dword index, dword topindex,
						   dword max_height)
{
	char number[5];
	int left, right, upper, bottom;

	default_predraw(&g_predraw, _("字体设置"), max_height, &left, &right,
					&upper, &bottom, 4);

	memset(number, ' ', 4);
	utils_dword2string(fonts[fontindex].size, number, 4);

	int lines = 0;

	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(config.usettf ? ttfsize : bookfonts[bookfontindex].size,
					   number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(config.wordspace, number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(config.rowspace, number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	memset(number, ' ', 4);
	utils_dword2string(config.borderspace, number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
#ifdef ENABLE_TTF
	disp_putstring(g_predraw.x + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.usettf ? _("是") : _("否")));
	lines++;
#else
	disp_putstring(g_predraw.x + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) (_("已关闭")));
	lines++;
#endif
}

dword scene_fontsel(dword * selidx)
{
	t_win_menuitem item[6];
	dword i;

	STRCPY_S(item[0].name, _("菜单字体大小"));
	STRCPY_S(item[1].name, _("阅读字体大小"));
	STRCPY_S(item[2].name, _("      字间距"));
	STRCPY_S(item[3].name, _("      行间距"));
	STRCPY_S(item[4].name, _("    保留边距"));
	STRCPY_S(item[5].name, _(" 使用TTF字体"));

	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	int orgttfsize = ttfsize;
	bool orgusettf = config.usettf;
	bool orgvert = config.vertread;
	bool orgibar = config.infobar;
	bool orgscrollbar = config.scrollbar;
	dword orgrowspace = config.rowspace;
	dword orgwordspace = config.wordspace;
	dword orgborderspace = config.borderspace;

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));
	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - (g_predraw.item_count - 1) * DISP_FONTSIZE;
	g_predraw.linespace = 0;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_fontsel_predraw, NULL,
					 scene_fontsel_menucb)) != INVALID);
	if (orgfontindex != fontindex || orgusettf != config.usettf
		|| (!config.usettf && orgbookfontindex != bookfontindex)
		|| (config.usettf && orgttfsize != ttfsize)) {
		if (orgfontindex != fontindex)
			scene_load_font();
		scene_load_book_font();
		recalcSize(&drperpage, &rowsperpage, &pixelsperrow);

		memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
		return win_menu_op_ok;
	}

	dword result = win_menu_op_continue;

	if (orgrowspace != config.rowspace || orgborderspace != config.borderspace) {
		recalcSize(&drperpage, &rowsperpage, &pixelsperrow);
		cur_book_view.text_needrf = true;
	}
	if (orgibar != config.infobar || orgvert != config.vertread
		|| orgwordspace != config.wordspace
		|| orgscrollbar != config.scrollbar) {
		dword orgpixelsperrow = pixelsperrow;

		cur_book_view.text_needrf = true;
		recalcSize(&drperpage, &rowsperpage, &pixelsperrow);
		if (orgpixelsperrow != pixelsperrow)
			result = win_menu_op_force_redraw;
	}

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return 0;
}

t_win_menu_op scene_musicopt_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
#if defined(ENABLE_MUSIC)
					config.autoplay = !config.autoplay;
#endif
					break;
				case 1:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
					if (config.lyricex == 0)
						config.lyricex = (155 / (DISP_FONTSIZE + 1) - 1) / 2;
					else
						config.lyricex--;
#else
					config.lyricex = 0;
#endif
					break;
				case 2:
					if (config.lyricencode)
						config.lyricencode--;
					else
						config.lyricencode = 4;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
#if defined(ENABLE_MUSIC)
					config.autoplay = !config.autoplay;
#endif
					break;
				case 1:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
					if (config.lyricex >= (155 / (DISP_FONTSIZE + 1) - 1) / 2)
						config.lyricex = 0;
					else
						config.lyricex++;
#else
					config.lyricex = 0;
#endif
					break;
				case 2:
					config.lyricencode++;
					if (config.lyricencode > 4)
						config.lyricencode = 0;
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_musicopt_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	char number[5];

	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("音乐设置"), max_height, &left, &right,
					&upper, &bottom, 4);

#ifdef ENABLE_MUSIC
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.autoplay ? _("是") : _("否")));
	lines++;
#else
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) _("不支持"));
	lines++;
#endif
	memset(number, ' ', 4);
	utils_dword2string(config.lyricex * 2 + 1, number, 4);
	disp_putstring(g_predraw.x + 2,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) number);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_encodename(config.lyricencode));
	lines++;
}

dword scene_musicopt(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[3];
	dword i;

	STRCPY_S(item[0].name, _("自动开始播放"));
	STRCPY_S(item[1].name, _("歌词显示行数"));
	STRCPY_S(item[2].name, _("歌词显示编码"));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	dword index;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_musicopt_predraw, NULL,
					 scene_musicopt_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return 0;
}

static void set_language()
{
	char msgpath[PATH_MAX];

	SPRINTF_S(msgpath, "%smsg", appdir);
	simple_bindtextdomain(config.language, msgpath);
	if (simple_textdomain(config.language) == NULL) {
		STRCPY_S(config.language, "zh_CN");
		dbg_printf(d, "language file not found!");
		simple_bindtextdomain("zh_CN", msgpath);
		simple_textdomain("zh_CN");
	}
}

t_win_menu_op scene_moptions_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
		case PSP_CTRL_LEFT:
			switch (*index) {
				case 0:
					config.showhidden = !config.showhidden;
					break;
				case 1:
					config.showunknown = !config.showunknown;
					break;
				case 2:
					config.showfinfo = !config.showfinfo;
					break;
				case 3:
					config.allowdelete = !config.allowdelete;
					break;
				case 4:
					if (config.arrange == conf_arrange_ext)
						config.arrange = conf_arrange_mtime;
					else
						config.arrange--;
					break;
				case 5:
#ifdef ENABLE_USB
					config.enableusb = !config.enableusb;
#endif
					break;
				case 6:
					if (config.autosleep > 0)
						config.autosleep--;
					break;
				case 7:
				case 8:
				case 9:
					if (config.freqs[*index - 7] > 0)
						config.freqs[*index - 7]--;
					if (config.freqs[*index - 7] > NELEMS(freq_list) - 1)
						config.freqs[*index - 7] = NELEMS(freq_list) - 1;
					scene_power_save(true);
					break;
				case 10:
					config.dis_scrsave = !config.dis_scrsave;
					break;
				case 11:
					config.launchtype = config.launchtype == 2 ? 4 : 2;
					break;
				case 12:
					if (config.brightness > 28) {
						config.brightness--;
						if (prx_loaded) {
							xrSetBrightness(config.brightness);
						}
					}
					break;
				case 13:
					if (strcmp(config.language, "zh_CN") == 0) {
						STRCPY_S(config.language, "en_US");
					} else {
						STRCPY_S(config.language, "zh_CN");
					}
					set_language();
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			switch (*index) {
				case 0:
					config.showhidden = !config.showhidden;
					break;
				case 1:
					config.showunknown = !config.showunknown;
					break;
				case 2:
					config.showfinfo = !config.showfinfo;
					break;
				case 3:
					config.allowdelete = !config.allowdelete;
					break;
				case 4:
					if (config.arrange == conf_arrange_mtime)
						config.arrange = conf_arrange_ext;
					else
						config.arrange++;
					break;
				case 5:
#ifdef ENABLE_USB
					config.enableusb = !config.enableusb;
#endif
					break;
				case 6:
					if (config.autosleep < 1000)
						config.autosleep++;
					break;
				case 7:
				case 8:
				case 9:
					if (config.freqs[*index - 7] < NELEMS(freq_list) - 1)
						config.freqs[*index - 7]++;
					if (config.freqs[*index - 7] < 0)
						config.freqs[*index - 7] = 0;
					scene_power_save(true);
					break;
				case 10:
					config.dis_scrsave = !config.dis_scrsave;
					break;
				case 11:
					config.launchtype = config.launchtype == 2 ? 4 : 2;
					break;
				case 12:
					if (config.brightness < 99) {
						config.brightness++;
						if (prx_loaded) {
							xrSetBrightness(config.brightness);
						}
					}
					break;
				case 13:
					if (strcmp(config.language, "zh_CN") == 0) {
						STRCPY_S(config.language, "en_US");
					} else {
						STRCPY_S(config.language, "zh_CN");
					}
					set_language();
					break;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static const char *GetLanguageHelpString(const char *language)
{
	static char *langList[] = {
		"中文",
		"English"
	};

	if (strcmp(language, "zh_CN") == 0) {
		return langList[0];
	}
	if (strcmp(language, "en_US") == 0) {
		return langList[1];
	}

	return NULL;
}

void scene_moptions_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("系统选项"), max_height, &left, &right,
					&upper, &bottom, 3 * DISP_FONTSIZE + 4);

	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.showhidden ? _("显示") : _("隐藏")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.showunknown ? _("显示") : _("隐藏")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.showfinfo ? _("显示") : _("隐藏")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.allowdelete ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) conf_get_arrangename(config.arrange));
	lines++;
#ifdef ENABLE_USB
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.enableusb ? _("是") : _("否")));
	lines++;
#else
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) _("不支持"));
	lines++;
#endif
	char infomsg[80];

	if (config.autosleep == 0) {
		STRCPY_S(infomsg, _("已关闭"));
	} else {
		SPRINTF_S(infomsg, _("%d分钟"), config.autosleep);
	}
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[0]][0],
			  freq_list[config.freqs[0]][1]);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[1]][0],
			  freq_list[config.freqs[1]][1]);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[2]][0],
			  freq_list[config.freqs[2]][1]);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.dis_scrsave ? _("是") : _("否")));
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE,
				   (const byte *) (config.launchtype ==
								   2 ? _("普通程序") : _("PS游戏")));
	lines++;

	SPRINTF_S(infomsg, "%d", config.brightness);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	SPRINTF_S(infomsg, "%s", GetLanguageHelpString(simple_textdomain(NULL)));
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
}

dword scene_moptions(dword * selidx)
{
	t_win_menuitem item[14];
	dword i;

	STRCPY_S(item[0].name, _("    隐藏文件"));
	STRCPY_S(item[1].name, _("未知类型文件"));
	STRCPY_S(item[2].name, _("    文件信息"));
	STRCPY_S(item[3].name, _("    允许删除"));
	STRCPY_S(item[4].name, _("文件排序方式"));
	STRCPY_S(item[5].name, _("    USB 连接"));
	STRCPY_S(item[6].name, _("    自动休眠"));
	STRCPY_S(item[7].name, _("设置最低频率"));
	STRCPY_S(item[8].name, _("设置普通频率"));
	STRCPY_S(item[9].name, _("设置最高频率"));
	STRCPY_S(item[10].name, _("禁用屏幕保护"));
	STRCPY_S(item[11].name, _("启动程序类型"));
	STRCPY_S(item[12].name, _("        亮度"));
	STRCPY_S(item[13].name, _("        语言"));

	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	bool orgshowhidden = config.showhidden;
	bool orgshowunknown = config.showunknown;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	int orgbrightness = config.brightness;
	t_conf_arrange orgarrange = config.arrange;
	char orglanguage[20];

	STRCPY_S(orglanguage, config.language);

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.linespace = 0;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper =
		g_predraw.y - DISP_FONTSIZE * (g_predraw.item_count - 1) / 2;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, NELEMS(item), 0,
					 g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_moptions_predraw, NULL,
					 scene_moptions_menucb)) != INVALID);

	if (strcmp(orglanguage, config.language) != 0) {
		set_language();
	}

	if (prx_loaded) {
		if (orgbrightness != config.brightness) {
			dbg_printf(d, _("亮度设置为%d"), config.brightness);
			xrSetBrightness(config.brightness);
			orgbrightness = config.brightness;
		}
	}
#ifdef ENABLE_USB
	if (config.isreading == false) {
		if (config.enableusb)
			usb_activate();
		else
			usb_deactivate();
	}
#endif
	if (orgfontindex != fontindex || orgbookfontindex != bookfontindex) {
		scene_load_font();
		scene_load_book_font();
		recalcSize(&drperpage, &rowsperpage, &pixelsperrow);

		memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
		return 2;
	}
	if (orgshowhidden != config.showhidden
		|| orgshowunknown != config.showunknown
		|| orgarrange != config.arrange) {
		memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
		return 1;
	}

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return 0;
}

static int scene_locname_to_itemname(char *dst, size_t dstsize, const char *src,
									 size_t srcsize, bool isreading)
{
	if (dst == NULL || dstsize == 0 || src == NULL || srcsize < 4)
		return -1;

	if (isreading) {
		if (strlen(src) > srcsize - 4) {
			mbcsncpy_s((byte *) dst, srcsize - 4 + 1, (const byte *) src, -1);
			strcat_s(dst, dstsize, "...");
			strcat_s(dst, dstsize, "*");
		} else {
			strcpy_s(dst, dstsize, src);
		}
	} else {
		if (strlen(src) > srcsize - 3) {
			mbcsncpy_s((byte *) dst, srcsize - 3 + 1, (const byte *) src, -1);
			strcat_s(dst, dstsize, "...");
		} else {
			strcpy_s(dst, dstsize, src);
		}
	}

	return 0;
}

t_win_menu_op scene_locsave_menucb(dword key, p_win_menuitem item,
								   dword * count, dword max_height,
								   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_CIRCLE:
			if (*index < 10) {
				if (location_set
					(*index, config.path, config.shortpath,
					 filelist[(dword) item[1].data].compname->ptr,
					 config.isreading)) {
					dbg_printf(d, "location_set: %s %s %s %d", config.path,
							   config.shortpath,
							   filelist[(dword) item[1].data].compname->ptr,
							   config.isreading);
					locaval[*index] = true;
					char t[128];

					STRCPY_S(t, config.path);
					if (config.path[strlen(config.path) - 1] != '/'
						&& filelist[(dword) item[1].data].compname->ptr[0] !=
						'/')
						STRCAT_S(t, "/");
					STRCAT_S(t, filelist[(dword) item[1].data].compname->ptr);
					scene_locname_to_itemname(item[*index].name,
											  NELEMS(item[*index].name), t,
											  (config.filelistwidth /
											   config.fontsize * 4),
											  config.isreading);
					item[*index].width = strlen(item[*index].name);
				}
//          win_msg("保存成功！", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
				return win_menu_op_force_redraw;
			}
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_locsave_predraw(p_win_menuitem item, dword index, dword topindex,
						   dword max_height)
{
	int left, right, upper, bottom;

	default_predraw(&g_predraw, _("保存文件位置"), max_height, &left, &right,
					&upper, &bottom, 4);
}

void scene_loc_enum(dword index, char *comppath, char *shortpath,
					char *compname, bool isreading, void *data)
{
	dbg_printf(d, "scene_loc_enum: %s %s %s %d", comppath, shortpath, compname,
			   isreading);
	p_win_menuitem item = (p_win_menuitem) data;

	if (index < 10) {
		char t[128];

		STRCPY_S(t, comppath);
		if (comppath[strlen(comppath) - 1] != '/' && compname[0] != '/') {
			STRCAT_S(t, "/");
		}
		STRCAT_S(t, compname);
		scene_locname_to_itemname(item[index].name, NELEMS(item[index].name), t,
								  (config.filelistwidth / config.fontsize * 4),
								  isreading);
		item[index].width = strlen(item[index].name);
	}
}

dword scene_locsave(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[10];
	dword i;

	for (i = 0; i < NELEMS(item); i++) {
		STRCPY_S(item[i].name, _("未使用"));
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;

	g_predraw.max_item_len = (config.filelistwidth / config.fontsize * 4);
	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left =
		g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2 / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	item[1].data = (void *) *selidx;

	index =
		win_menu(g_predraw.left,
				 g_predraw.upper, g_predraw.max_item_len,
				 g_predraw.item_count, item, NELEMS(item), 0,
				 g_predraw.linespace,
				 config.usedyncolor ? GetBGColorByTime() : config.
				 menubcolor, true, scene_locsave_predraw, NULL,
				 scene_locsave_menucb);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return 0;
}

t_win_menu_op scene_locload_menucb(dword key, p_win_menuitem item,
								   dword * count, dword max_height,
								   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_CIRCLE:
			if (*index < 10) {
				char comppath[PATH_MAX] = "", shortpath[PATH_MAX] =
					"", compname[PATH_MAX] = "";
				if (location_get
					(*index, comppath, shortpath, compname, &locreading)
					&& comppath[0] != 0 && shortpath[0] != 0
					&& compname[0] != 0) {
					dbg_printf(d, "location_get %s %s %s %d", comppath,
							   shortpath, compname, locreading);
					where = scene_in_dir;
					STRCPY_S(config.path, comppath);
					STRCPY_S(config.shortpath, shortpath);
					STRCPY_S(config.lastfile, compname);
					config.isreading = locreading;
					dword plen = strlen(config.path);

					if (plen > 0 && config.path[plen - 1] == '/') {
						if (strnicmp(config.path, "ms0:/", 5) == 0)
							filecount =
								fs_dir_to_menu(config.path, config.shortpath,
											   &filelist, config.menutextcolor,
											   config.selicolor,
											   config.
											   usedyncolor ? GetBGColorByTime()
											   : config.menubcolor,
											   config.selbcolor,
											   config.showhidden,
											   config.showunknown);
						else
							filecount =
								fs_flashdir_to_menu(config.path,
													config.shortpath, &filelist,
													config.menutextcolor,
													config.selicolor,
													config.
													usedyncolor ?
													GetBGColorByTime() : config.
													menubcolor,
													config.selbcolor);
					} else
						switch (fs_file_get_type(config.path)) {
							case fs_filetype_zip:
								{
									where = scene_in_zip;
									filecount =
										fs_zip_to_menu(config.shortpath,
													   &filelist,
													   config.menutextcolor,
													   config.selicolor,
													   config.
													   usedyncolor ?
													   GetBGColorByTime() :
													   config.menubcolor,
													   config.selbcolor);
									break;
								}
							case fs_filetype_chm:
								{
									where = scene_in_chm;
									filecount =
										fs_chm_to_menu(config.shortpath,
													   &filelist,
													   config.menutextcolor,
													   config.selicolor,
													   config.
													   usedyncolor ?
													   GetBGColorByTime() :
													   config.menubcolor,
													   config.selbcolor);
									break;
								}
							case fs_filetype_rar:
								{
									where = scene_in_rar;
									filecount =
										fs_rar_to_menu(config.shortpath,
													   &filelist,
													   config.menutextcolor,
													   config.selicolor,
													   config.
													   usedyncolor ?
													   GetBGColorByTime() :
													   config.menubcolor,
													   config.selbcolor);
									break;
								}
							default:
								STRCPY_S(config.path, "ms0:/");
								STRCPY_S(config.shortpath, "ms0:/");
								filecount =
									fs_dir_to_menu(config.path,
												   config.shortpath, &filelist,
												   config.menutextcolor,
												   config.selicolor,
												   config.
												   usedyncolor ?
												   GetBGColorByTime() : config.
												   menubcolor, config.selbcolor,
												   config.showhidden,
												   config.showunknown);
								break;
						}
					if (filelist == 0) {
						where = scene_in_dir;
						STRCPY_S(config.path, "ms0:/");
						STRCPY_S(config.shortpath, "ms0:/");
						STRCPY_S(config.lastfile, "");
						filecount =
							fs_dir_to_menu(config.path, config.shortpath,
										   &filelist, config.menutextcolor,
										   config.selicolor,
										   config.
										   usedyncolor ? GetBGColorByTime() :
										   config.menubcolor, config.selbcolor,
										   config.showhidden,
										   config.showunknown);
					}
					quicksort(filelist,
							  (filecount > 0
							   && filelist[0].compname->ptr[0] == '.') ? 1 : 0,
							  filecount - 1, sizeof(t_win_menuitem),
							  compare_func[(int) config.arrange]);
					dword idx = 0;

					while (idx < filecount
						   && stricmp(filelist[idx].compname->ptr,
									  config.lastfile) != 0)
						idx++;
					if (idx >= filecount) {
						idx = 0;
						config.isreading = locreading = false;
					}
					item[0].data = (void *) true;
					item[1].data = (void *) idx;
					return win_menu_op_cancel;
				}
			}
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_locload_predraw(p_win_menuitem item, dword index, dword topindex,
						   dword max_height)
{
	int left, right, upper, bottom;

	default_predraw(&g_predraw, _("读取文件位置"), max_height, &left, &right,
					&upper, &bottom, 4);
}

dword scene_locload(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[10];
	dword i;

	for (i = 0; i < NELEMS(item); i++) {
		STRCPY_S(item[i].name, _("未使用"));
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;

	g_predraw.max_item_len = (config.filelistwidth / config.fontsize * 4);
	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left =
		g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2 / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	item[0].data = (void *) false;
	item[1].data = (void *) *selidx;
	index =
		win_menu(g_predraw.left,
				 g_predraw.upper, g_predraw.max_item_len,
				 g_predraw.item_count, item, NELEMS(item), 0,
				 g_predraw.linespace,
				 config.usedyncolor ? GetBGColorByTime() : config.
				 menubcolor, true, scene_locload_predraw, NULL,
				 scene_locload_menucb);

	*selidx = (dword) item[1].data;

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));
	return (bool) item[0].data;
}

void scene_setting_mgr_predraw(p_win_menuitem item, dword index, dword topindex,
							   dword max_height)
{
	int left, right, upper, bottom, lines = 0;

	default_predraw(&g_predraw, _("设置管理"), max_height, &left, &right,
					&upper, &bottom, 4 * DISP_FONTSIZE + 4);

	char infomsg[80];

	SPRINTF_S(infomsg, _("%d号设置"), config_num);
	char conffile[PATH_MAX], appdir[PATH_MAX];

	getcwd(appdir, PATH_MAX);
	STRCAT_S(appdir, "/");
	STRCPY_S(conffile, appdir);
	char conffilename[80];

	SPRINTF_S(conffilename, "%s%d%s", "config", config_num, ".ini");
	STRCAT_S(conffile, conffilename);
	if (utils_is_file_exists(conffile)) {
		STRCAT_S(infomsg, "*");
	}
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	lines++;
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
	disp_putstring(g_predraw.x + 2 + DISP_FONTSIZE,
				   upper + 2 + (lines + 1 + g_predraw.linespace) * (1 +
																	DISP_FONTSIZE),
				   COLOR_WHITE, (const byte *) infomsg);
}

int detect_config_change(const p_conf prev, const p_conf curr)
{
	if (strcmp(prev->language, curr->language) != 0) {
		set_language();
	}
#ifdef ENABLE_USB
	if (!prev->enableusb && curr->enableusb)
		usb_activate();
	else if (prev->enableusb && !curr->enableusb)
		usb_deactivate();
#endif
	if (prev->brightness != curr->brightness) {
		if (prx_loaded) {
			dbg_printf(d, _("亮度设置为%d"), curr->brightness);
			xrSetBrightness(curr->brightness);
		}
	}

	if (!curr->usettf) {
		int i = 0;

		for (i = 0; i < fontcount; ++i) {
			if (fonts[i].size == curr->fontsize) {
				break;
			}
		}
		if (i != fontcount) {
			fontindex = i;
			scene_load_font();
		} else {
			fontindex = 0;
			scene_load_font();
		}

		for (i = 0; i < bookfontcount; ++i) {
			if (bookfonts[i].size == curr->bookfontsize) {
				break;
			}
		}
		if (i != bookfontcount) {
			bookfontindex = i;
			scene_load_book_font();
		} else {
			bookfontindex = 0;
			scene_load_book_font();
		}
	} else {
		scene_load_font();
		ttfsize = curr->bookfontsize;
		scene_load_book_font();
	}

	recalcSize(&drperpage, &rowsperpage, &pixelsperrow);

#ifdef ENABLE_BG
	bg_load(curr->bgfile, curr->bgarch, curr->bgcolor,
			fs_file_get_type(curr->bgfile), curr->grayscale, curr->bgwhere);
#endif

	int i;

	for (i = 0; i < 3; ++i) {
		if (prev->freqs[i] != curr->freqs[i])
			break;
	}
	if (i != 3) {
		scene_power_save(true);
	}

	STRCPY_S(curr->path, prev->path);
	STRCPY_S(curr->shortpath, prev->shortpath);
	STRCPY_S(curr->lastfile, prev->lastfile);
	curr->isreading = prev->isreading;

#ifdef ENABLE_IMAGE
	img_needrf = img_needrc = img_needrp = true;
#endif
	cur_book_view.text_needrf = cur_book_view.text_needrp =
		cur_book_view.text_needrb = true;

	return 0;
}

t_win_menu_op scene_setting_mgr_menucb(dword key, p_win_menuitem item,
									   dword * count, dword max_height,
									   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_LEFT:
			if (--config_num < 0) {
				config_num = 9;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			if (++config_num > 9) {
				config_num = 0;
			}
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			{
				disp_waitv();
				char conffile[PATH_MAX], appdir[PATH_MAX];

				getcwd(appdir, PATH_MAX);
				STRCAT_S(appdir, "/");
				STRCPY_S(conffile, appdir);
				char conffilename[80];

				SPRINTF_S(conffilename, "%s%d%s", "config", config_num, ".ini");
				STRCAT_S(conffile, conffilename);
				conf_set_file(conffile);
				if (*index == 0) {
					t_conf prev_config;

					memcpy(&prev_config, &config, sizeof(t_conf));
					// load
					if (!conf_load(&config)) {
						win_msg(_("读取设置失败!"), COLOR_WHITE, COLOR_WHITE,
								config.msgbcolor);
						return win_menu_op_redraw;
					}
					detect_config_change(&prev_config, &config);
				} else if (*index == 1) {
					// save
					load_fontsize_to_config();
					if (!conf_save(&config)) {
						win_msg(_("保存设置失败!"), COLOR_WHITE, COLOR_WHITE,
								config.msgbcolor);
						return win_menu_op_redraw;
					}
				} else {
					// delete
					if (!utils_del_file(conffile)) {
						win_msg(_("删除设置失败!"), COLOR_WHITE, COLOR_WHITE,
								config.msgbcolor);
						return win_menu_op_redraw;
					}
				}
				ctrl_waitrelease();
				return win_menu_op_ok;
			}
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

dword scene_setting_mgr(dword * selidx)
{
	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	t_win_menuitem item[3];

	STRCPY_S(item[0].name, _("读取设置"));
	STRCPY_S(item[1].name, _("保存设置"));
	STRCPY_S(item[2].name, _("删除设置"));
	dword i;

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 240;
	g_predraw.y = 123 - 3 * DISP_FONTSIZE;
	g_predraw.linespace = 0;
	g_predraw.left = g_predraw.x - (DISP_FONTSIZE / 2) * g_predraw.max_item_len;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;

	dword index;

	item[0].data = (void *) false;
	item[1].data = (void *) *selidx;

	index =
		win_menu(g_predraw.left, g_predraw.upper, g_predraw.max_item_len,
				 g_predraw.item_count, item, NELEMS(item), 0,
				 g_predraw.linespace,
				 config.usedyncolor ? GetBGColorByTime() : config.menubcolor,
				 true, scene_setting_mgr_predraw, NULL,
				 scene_setting_mgr_menucb);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	if (index == INVALID)
		return win_menu_op_continue;
	return win_menu_op_ok;
}

typedef dword(*t_scene_option_func) (dword * selidx);

t_scene_option_func scene_option_func[] = {
	scene_fontsel,
	scene_color,
	scene_moptions,
	scene_flkey,
	scene_boptions,
	scene_txtkey,
#ifdef ENABLE_IMAGE
	scene_ioptions,
	scene_imgkey,
#else
	NULL,
	NULL,
#endif
	scene_ctrlset,
#ifdef ENABLE_MUSIC
	scene_musicopt,
#else
	NULL,
#endif
	scene_locsave,
	scene_locload,
	scene_setting_mgr,
	NULL,
	NULL
};

t_win_menu_op scene_options_menucb(dword key, p_win_menuitem item,
								   dword * count, dword max_height,
								   dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_CIRCLE:
			if (*index == 13)
				return exit_confirm();
			if (*index == 14) {
				if (dbg_memory_buffer == NULL) {
					win_msg(_("无法打开调试信息!"), COLOR_WHITE, COLOR_WHITE,
							config.msgbcolor);
					return win_menu_op_cancel;
				}
				pixel *saveimage = (pixel *) memalign(16,
													  PSP_SCREEN_WIDTH *
													  PSP_SCREEN_HEIGHT *
													  sizeof(pixel));
				if (saveimage)
					disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
								  saveimage);
				scene_readbook_raw(_("调试信息"),
								   (const unsigned char *) dbg_memory_buffer->
								   ptr, dbg_memory_buffer->used,
								   fs_filetype_txt);
				g_force_text_view_mode = false;
				ctrl_waitrelease();

				if (saveimage) {
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
								  0, saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
								  0, saveimage);
					free((void *) saveimage);
				}

				return win_menu_op_cancel;
			}
			if (scene_option_func[*index] != NULL) {
				item[0].data =
					(void *) scene_option_func[*index] (item[1].data);
				if (item[0].data != win_menu_op_continue)
					return win_menu_op_cancel;
			}
			return win_menu_op_force_redraw;
		case PSP_CTRL_SELECT:
			ctrl_waitreleaseintime(10000);
			return win_menu_op_cancel;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_options_predraw(p_win_menuitem item, dword index, dword topindex,
						   dword max_height)
{
	int left = g_predraw.left - 1;
	int right =
		g_predraw.x - (DISP_FONTSIZE / 2) * g_predraw.max_item_len / 2 + 2 +
		g_predraw.max_item_len * (DISP_FONTSIZE / 2) + 1;
	int upper = g_predraw.upper - (DISP_FONTSIZE + 1) - 1;
	int bottom =
		g_predraw.upper + 2 + 1 + DISP_FONTSIZE + (max_height - 1) * (1 +
																	  DISP_FONTSIZE
																	  +
																	  g_predraw.
																	  linespace);

	disp_rectangle(left, upper, right, bottom, COLOR_WHITE);

	disp_fillrect(left + 1, upper + 1, right - 1, upper + DISP_FONTSIZE,
				  config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(get_center_pos(left, right, _("设置选项")),
				   upper + 1, COLOR_WHITE, (const byte *) _("设置选项"));
	disp_line(left, upper + 1 + DISP_FONTSIZE, right, upper + 1 + DISP_FONTSIZE,
			  COLOR_WHITE);
}

dword scene_options(dword * selidx)
{
#ifdef _DEBUG
	t_win_menuitem item[15];
#else
	t_win_menuitem item[14];
#endif
	dword i;

	STRCPY_S(item[0].name, _("  字体设置"));
	STRCPY_S(item[1].name, _("  颜色设置"));
	STRCPY_S(item[2].name, _("  系统选项"));
	STRCPY_S(item[3].name, _("  系统按键"));
	STRCPY_S(item[4].name, _("  阅读选项"));
	STRCPY_S(item[5].name, _("  阅读按键"));
#ifdef ENABLE_IMAGE
	STRCPY_S(item[6].name, _("  看图选项"));
	STRCPY_S(item[7].name, _("  看图按键"));
#else
	STRCPY_S(item[6].name, _("*看图已关闭*"));
	STRCPY_S(item[7].name, _("*看图已关闭*"));
#endif
	STRCPY_S(item[8].name, _("  操作设置"));
#ifdef ENABLE_MUSIC
	STRCPY_S(item[9].name, _("  音乐播放"));
#else
	STRCPY_S(item[9].name, _("*音乐已关闭*"));
#endif
	STRCPY_S(item[10].name, _("保存文件位置"));
	STRCPY_S(item[11].name, _("读取文件位置"));
	STRCPY_S(item[12].name, _("  设置管理"));
	STRCPY_S(item[13].name, _("  退出软件"));
#ifdef _DEBUG
	STRCPY_S(item[14].name, _("查看调试信息"));
#endif
	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	item[0].data = (void *) 0;
	item[1].data = (void *) selidx;
	dword index = 0;

	g_predraw.item_count = NELEMS(item);
	g_predraw.x = 238;
	g_predraw.y = 122;
	g_predraw.linespace = 0;
	g_predraw.left =
		g_predraw.x - (DISP_FONTSIZE / 2) * g_predraw.max_item_len / 2;
	g_predraw.upper =
		g_predraw.y - DISP_FONTSIZE * (g_predraw.item_count - 1) / 2;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper,
					 g_predraw.max_item_len, NELEMS(item), item, NELEMS(item),
					 index, g_predraw.linespace,
					 config.usedyncolor ? GetBGColorByTime() : config.
					 menubcolor, true, scene_options_predraw, NULL,
					 scene_options_menucb)) != INVALID);

	return (dword) item[0].data;
}

t_win_menu_op scene_bookmark_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_SELECT:
			bookmark_delete(bm);
			memset(&bm->row[0], 0xFF, 10 * sizeof(dword));
			win_msg(_("已删除书签!"), COLOR_WHITE, COLOR_WHITE,
					config.msgbcolor);
			return win_menu_op_cancel;
		case PSP_CTRL_START:
			if (win_msgbox
				(_("是否要导出书签？"), _("是"), _("否"),
				 COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				char bmfn[PATH_MAX];

				if (where == scene_in_zip || where == scene_in_chm
					|| where == scene_in_rar) {
					STRCPY_S(bmfn, config.shortpath);
					STRCAT_S(bmfn, fs->filename);
				} else
					STRCPY_S(bmfn, fs->filename);
				STRCAT_S(bmfn, ".ebm");
				bookmark_export(bm, bmfn);
				win_msg(_("已导出书签!"), COLOR_WHITE, COLOR_WHITE,
						config.msgbcolor);
			}
			return win_menu_op_force_redraw;
		case PSP_CTRL_SQUARE:
			STRCPY_S(item[*index].name, "       ");
			bm->row[(*index) + 1] = *(dword *) item[0].data;
			utils_dword2string(bm->row[(*index) + 1] / 2, item[*index].name, 7);
			bookmark_save(bm);
			return win_menu_op_redraw;
		case PSP_CTRL_TRIANGLE:
			bm->row[(*index) + 1] = INVALID;
			STRCPY_S(item[*index].name, _("  NONE "));
			bookmark_save(bm);
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			if (bm->row[(*index) + 1] != INVALID) {
				*(dword *) item[0].data = bm->row[(*index) + 1];
				item[1].data = (void *) true;
				return win_menu_op_ok;
			} else
				return win_menu_op_continue;
		default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_bookmark_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	disp_rectangle(63, 60 - DISP_FONTSIZE, 416, 64 + (1 + DISP_FONTSIZE) * 10,
				   COLOR_WHITE);
	disp_fillrect(64, 61 - DISP_FONTSIZE, 415, 60, config.titlecolor);
	disp_putstring(75, 61 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)
				   _("书签      ○读取  ×取消  □保存  △删除"));
	disp_fillrect(68 + 7 * DISP_FONTSIZE / 2, 62, 415,
				  63 + (1 + DISP_FONTSIZE) * 10, config.titlecolor);
	disp_line(64, 61, 415, 61, COLOR_WHITE);
	disp_line(64, 64 + (1 + DISP_FONTSIZE) * 9, 415,
			  64 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE);
	disp_fillrect(64, 65 + (1 + DISP_FONTSIZE) * 9, 67 + 7 * DISP_FONTSIZE / 2,
				  63 + (1 + DISP_FONTSIZE) * 10, config.titlecolor);
	disp_line(67 + 7 * DISP_FONTSIZE / 2, 62, 67 + 7 * DISP_FONTSIZE / 2,
			  63 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE);
	++index;
	disp_putstring(64, 65 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE,
				   (const byte *) _("SELECT 删除全部书签    START 导出书签"));
	if (bm->row[index] < fs->size
		&& fs_file_get_type(fs->filename) != fs_filetype_unknown) {
		t_text preview;

		memset(&preview, 0, sizeof(t_text));
		preview.buf = fs->buf + min(fs->size, bm->row[index]);
		if (fs->buf + fs->size - preview.buf <
			8 * ((347 - 7 * DISP_FONTSIZE / 2) / (DISP_FONTSIZE / 2)))
			preview.size = fs->buf + fs->size - preview.buf;
		else
			preview.size =
				8 * ((347 - 7 * DISP_FONTSIZE / 2) / (DISP_FONTSIZE / 2));
		byte bp[0x80];

		memcpy(bp, disp_ewidth, 0x80);
		memset(disp_ewidth, DISP_FONTSIZE / 2, 0x80);
		int old_book_fontsize = DISP_BOOK_FONTSIZE;

		DISP_BOOK_FONTSIZE = DISP_FONTSIZE;
		text_format(&preview, 347 - 7 * DISP_FONTSIZE / 2,
					config.fontsize <= 10 ? 1 : 0, false);
		memcpy(disp_ewidth, bp, 0x80);
		if (preview.rows[0] != NULL) {
			dword i;

			if (preview.row_count > 8)
				preview.row_count = 8;
			for (i = 0; i < preview.row_count; i++)
				disp_putnstring(70 + 7 * DISP_FONTSIZE / 2,
								66 + (2 + DISP_FONTSIZE) * i, COLOR_WHITE,
								(const byte *) preview.rows[0][i].start,
								preview.rows[0][i].count,
								config.fontsize <= 10 ? 1 : 0, 0, DISP_FONTSIZE,
								0);
			free((void *) preview.rows[0]);
		}
		DISP_BOOK_FONTSIZE = old_book_fontsize;
	}
}

bool scene_bookmark(dword * orgp)
{
	char archname[PATH_MAX];

	if (where == scene_in_zip || where == scene_in_chm || where == scene_in_rar) {
		STRCPY_S(archname, config.shortpath);
		STRCAT_S(archname, fs->filename);
	} else
		STRCPY_S(archname, fs->filename);
	bm = bookmark_open(archname);
	if (bm == NULL) {
		win_msg(_("无法打开书签!"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		return 0;
	}
	dword i;
	t_win_menuitem item[9];

	for (i = 0; i < 9; i++) {
		if (bm->row[i + 1] != INVALID) {
			STRCPY_S(item[i].name, "       ");
			utils_dword2string(bm->row[i + 1] / 2, item[i].name, 7);
		} else
			STRCPY_S(item[i].name, _("  NONE "));
		item[i].width = 7;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor =
			config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
	}
	item[0].data = (void *) orgp;
	item[1].data = (void *) false;
	dword index;

	if ((index =
		 win_menu(64, 62, 7, 9, item, 9, 0, 0,
				  config.usedyncolor ? GetBGColorByTime() : config.menubcolor,
				  true, scene_bookmark_predraw, NULL,
				  scene_bookmark_menucb)) != INVALID);
	bookmark_close(bm);
	bm = NULL;
	return (bool) item[1].data;
}

void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku, dword * kd,
					  dword * kl, dword * kr)
{
	dword i;

	memcpy(ctlkey, config.txtkey, 14 * sizeof(dword));
	memcpy(ctlkey2, config.txtkey2, 14 * sizeof(dword));
	switch (config.vertread) {
		case 3:
			*ku = PSP_CTRL_DOWN;
			*kd = PSP_CTRL_UP;
			*kl = PSP_CTRL_RIGHT;
			*kr = PSP_CTRL_LEFT;
			break;
		case 2:
			for (i = 0; i < 14; i++) {
				if ((config.txtkey[i] & PSP_CTRL_LEFT) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_DOWN;
				if ((config.txtkey[i] & PSP_CTRL_RIGHT) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_UP;
				if ((config.txtkey[i] & PSP_CTRL_UP) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_UP) | PSP_CTRL_LEFT;
				if ((config.txtkey[i] & PSP_CTRL_DOWN) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_RIGHT;
				if ((config.txtkey2[i] & PSP_CTRL_LEFT) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_DOWN;
				if ((config.txtkey2[i] & PSP_CTRL_RIGHT) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_UP;
				if ((config.txtkey2[i] & PSP_CTRL_UP) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_UP) | PSP_CTRL_LEFT;
				if ((config.txtkey2[i] & PSP_CTRL_DOWN) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_RIGHT;
			}
			*ku = PSP_CTRL_LEFT;
			*kd = PSP_CTRL_RIGHT;
			*kl = PSP_CTRL_DOWN;
			*kr = PSP_CTRL_UP;
			break;
		case 1:
			for (i = 0; i < 14; i++) {
				if ((config.txtkey[i] & PSP_CTRL_LEFT) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_UP;
				if ((config.txtkey[i] & PSP_CTRL_RIGHT) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_DOWN;
				if ((config.txtkey[i] & PSP_CTRL_UP) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_UP) | PSP_CTRL_RIGHT;
				if ((config.txtkey[i] & PSP_CTRL_DOWN) > 0)
					ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_LEFT;
				if ((config.txtkey2[i] & PSP_CTRL_LEFT) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_UP;
				if ((config.txtkey2[i] & PSP_CTRL_RIGHT) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_DOWN;
				if ((config.txtkey2[i] & PSP_CTRL_UP) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_UP) | PSP_CTRL_RIGHT;
				if ((config.txtkey2[i] & PSP_CTRL_DOWN) > 0)
					ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_LEFT;
			}
			*ku = PSP_CTRL_RIGHT;
			*kd = PSP_CTRL_LEFT;
			*kl = PSP_CTRL_UP;
			*kr = PSP_CTRL_DOWN;
			break;
		default:
			*ku = PSP_CTRL_UP;
			*kd = PSP_CTRL_DOWN;
			*kl = PSP_CTRL_LEFT;
			*kr = PSP_CTRL_RIGHT;
	}
}

static bool is_contain_hanzi_str(const char *str)
{
	size_t i;
	size_t len;

	if (str == NULL)
		return false;

	for (i = 0, len = strlen(str); i < len; ++i) {
		if (str[i] < 0) {
			return true;
		}
	}

	return false;
}

static const char *remove_hanzi(char *dst, const char *src, size_t size)
{
	size_t i;
	char *p = dst;

	if (size == 0 || dst == NULL || src == NULL)
		return NULL;

	for (i = 0; i < size - 1 && *src != '\0';) {
		if (*src < 0) {
			src += 2;
			continue;
		}
		*p++ = *src++;
		++i;
	}
	*p = '\0';

	return dst;
}

static const char *insert_string(char *str, size_t size, const char *insstr,
								 int inspos)
{
	size_t len, inslen;

	if (str == NULL || size == 0 || insstr == NULL)
		return NULL;

	len = strlen(str);
	inslen = strlen(insstr);

	if (len + inslen + 1 > size)
		return NULL;

	if (inspos > len)
		return NULL;

	memmove(str + inspos + inslen, str + inspos, len - inspos + 1);
	memcpy(str + inspos, insstr, inslen);

	return str;
}

static const char *get_file_basename(char *dst, size_t dstsize, const char *src)
{
	char *p, *q;

	if ((p = strrchr(src, '/')) == NULL) {
		if ((q = strrchr(src, '.')) == NULL) {
			strcpy_s(dst, dstsize, src);
		} else {
			size_t n = q - src;

			if (n != 0)
				strncpy_s(dst, dstsize, src, n);
			else
				dst[0] = '\0';
		}
	} else {
		if ((q = strrchr(p, '.')) == NULL) {
			size_t n = dstsize - (p - src);

			if (n != 0)
				strncpy_s(dst, dstsize, p + 1, n);
			else
				dst[0] = '\0';
		} else {
			size_t n = q - p - 1;

			if (n != 0)
				strncpy_s(dst, dstsize, p + 1, n);
			else
				dst[0] = '\0';
		}
	}

	return dst;
}

static bool confirm_overwrite(const char *filename, void *dummy)
{
	char infomsg[PATH_MAX];

	SPRINTF_S(infomsg, _("是否覆盖文件%s？"), filename);
	if (win_msgbox(infomsg, _("是"), _("否"),
				   COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
		return true;
	}
	return false;
}

static void scene_copy_files(int sidx)
{
	if (copy_archmode == true) {
		char archname[PATH_MAX], copydest[PATH_MAX];
		char temp[PATH_MAX];

		STRCPY_S(archname, copydir);
		STRCPY_S(copydest, config.shortpath);
		if (copy_where == scene_in_chm) {
			// CHM Compname is UTF8 encoding.
			char fname[PATH_MAX];

			charsets_utf8_conv((unsigned char *) copylist[sidx].
							   compname, (unsigned char *) fname);
			STRCPY_S(temp, fname);
		} else
			STRCPY_S(temp, copylist[sidx].compname->ptr);
		if (strrchr(temp, '/') != NULL) {
			char t[PATH_MAX];

			STRCPY_S(t, strrchr(temp, '/') + 1);
			STRCPY_S(temp, t);
		}
		if (is_contain_hanzi_str(temp) == true) {
			remove_hanzi(temp, temp, NELEMS(temp));
			if (strrchr(temp, '.') == NULL) {
				STRCAT_S(temp, copylist[sidx].shortname->ptr);
			} else {
				char basename[PATH_MAX];

				get_file_basename(basename, PATH_MAX, temp);
				if (basename[0] != '\0') {
					insert_string(temp, NELEMS(temp),
								  "_", strrchr(temp, '.') - temp);
				}
				insert_string(temp, NELEMS(temp),
							  copylist[sidx].shortname->ptr,
							  strrchr(temp, '.') - temp);
			}
		}
		STRCAT_S(copydest, temp);

		if ((t_fs_filetype) copylist[sidx].data != fs_filetype_dir) {
			dword result;

			result = extract_archive_file(archname,
										  copylist[sidx].compname->ptr,
										  copydest, NULL, confirm_overwrite,
										  NULL);
			if (result == (dword) false) {
				win_msg(_("文件解压失败!"), COLOR_WHITE,
						COLOR_WHITE, config.msgbcolor);
			}
		}
	} else {
		char copysrc[PATH_MAX], copydest[PATH_MAX];
		dword result;

		STRCPY_S(copysrc, copydir);
		STRCAT_S(copysrc, copylist[sidx].shortname->ptr);
		STRCPY_S(copydest, config.shortpath);
		STRCAT_S(copydest, copylist[sidx].shortname->ptr);
		if ((t_fs_filetype) copylist[sidx].data == fs_filetype_dir)
			result = copy_dir(copysrc, copydest, NULL, confirm_overwrite, NULL);
		else
			result =
				(dword) copy_file(copysrc, copydest, NULL, confirm_overwrite,
								  NULL);
		if (result == (dword) false) {
			win_msg(_("文件/目录复制失败!"), COLOR_WHITE,
					COLOR_WHITE, config.msgbcolor);
		}
	}
}

int scene_single_file_ops_draw(p_win_menuitem item, dword selidx)
{
	switch ((t_fs_filetype) item[selidx].data) {
		case fs_filetype_ebm:
			if (where == scene_in_dir) {
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
							   (const byte *) _("○  导入书签"));
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("□  导入书签"));
			}
			break;
		case fs_filetype_dir:
			disp_putstring(240 - DISP_FONTSIZE * 3,
						   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
						   (const byte *) _("○  进入目录"));
#ifdef ENABLE_MUSIC
			if (where == scene_in_dir)
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("□  添加音乐"));
#endif
			break;
#ifdef ENABLE_PMPAVC
		case fs_filetype_pmp:
			if (where == scene_in_dir) {
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
							   (const byte *) _("○  继续播放"));
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("□  从头播放"));
			}
			break;
#endif
		case fs_filetype_chm:
		case fs_filetype_zip:
		case fs_filetype_rar:
			if (where == scene_in_dir) {
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
							   (const byte *) _("○  进入文档"));
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("□  进入文档"));
			}
			break;
#ifdef ENABLE_MUSIC
		case fs_filetype_mp3:
		case fs_filetype_aa3:
#ifdef ENABLE_WMA
		case fs_filetype_wma:
#endif
			if (where == scene_in_dir) {
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
							   (const byte *) _("○  直接播放"));
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 - DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("□  添加音乐"));
			}
			break;
#endif
#if defined(ENABLE_IMAGE) || defined(ENABLE_BG)
		case fs_filetype_png:
		case fs_filetype_gif:
		case fs_filetype_jpg:
		case fs_filetype_tga:
		case fs_filetype_bmp:
#ifdef ENABLE_IMAGE
			disp_putstring(240 - DISP_FONTSIZE * 3,
						   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
						   (const byte *) _("○  查看图片"));
#endif
#ifdef ENABLE_BG
			disp_putstring(240 - DISP_FONTSIZE * 3,
						   136 - DISP_FONTSIZE, COLOR_WHITE,
						   (const byte *) _("□  设为背景"));
#endif
			if (config.load_exif
				&& (t_fs_filetype) item[selidx].data == fs_filetype_jpg) {
				disp_putstring(240 - DISP_FONTSIZE * 3,
							   136 + 4 * DISP_FONTSIZE, COLOR_WHITE,
							   (const byte *) _("SELECT EXIF"));
			}
			break;
#endif
		default:
			disp_putstring(240 - DISP_FONTSIZE * 3,
						   136 - DISP_FONTSIZE * 2, COLOR_WHITE,
						   (const byte *) _("○  阅读文本"));
			disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE,
						   COLOR_WHITE, (const byte *) _("□  阅读文本"));
			break;
	}

	return 0;
}

t_win_menu_op scene_filelist_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	dword orgidx = *index;
	t_win_menu_op op = win_menu_op_continue;

	if (key == (PSP_CTRL_SELECT | PSP_CTRL_START)) {
		return exit_confirm();
	} else if (key == config.flkey[3] || key == config.flkey2[3]) {
		ctrl_waitrelease();
		*index = 0;
		if (item[*index].compname->ptr[0] == '.')
			return win_menu_op_ok;
		return win_menu_op_redraw;
	} else if (key == config.flkey[6] || key == config.flkey2[6]) {
		if (strcmp(item[*index].compname->ptr, "..") == 0)
			return win_menu_op_continue;
		item[*index].selected = !item[*index].selected;
		if (*index < filecount - 1)
			++ * index;
		return win_menu_op_redraw;
	} else if (key == config.flkey[5] || key == config.flkey2[5]) {
		int sel, selcount = 0, selidx = 0, sidx, bmcount = 0, dircount =
			0, mp3count = 0;
		for (sel = 0; sel < filecount; sel++)
			if (item[sel].selected) {
				selidx = sel;
				selcount++;
				switch ((t_fs_filetype) item[selidx].data) {
					case fs_filetype_dir:
						dircount++;
						break;
#ifdef ENABLE_MUSIC
					case fs_filetype_mp3:
					case fs_filetype_aa3:
#ifdef ENABLE_WMA
					case fs_filetype_wma:
#endif
						mp3count++;
						break;
#endif
					case fs_filetype_ebm:
						bmcount++;
						break;
					default:
						break;
				}
			}
		if (selcount == 0)
			selidx = *index;
		if (selcount <= 1 && strcmp(item[selidx].compname->ptr, "..") == 0
			&& cutcount + copycount <= 0)
			return win_menu_op_continue;
		if (selcount == 0)
			item[selidx].selected = true;
		pixel *saveimage = (pixel *) memalign(16,
											  PSP_SCREEN_WIDTH *
											  PSP_SCREEN_HEIGHT *
											  sizeof(pixel));
		if (saveimage)
			disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
		disp_duptocachealpha(50);
		int left, right;

		if (strcmp(simple_textdomain(NULL), "zh_CN") == 0) {
			left = 240 - DISP_FONTSIZE * 3 - 1;
			right = 240 + DISP_FONTSIZE * 3 + 1;
		} else {
			left = 240 - DISP_FONTSIZE * 3 - 1;
			right = 240 + DISP_FONTSIZE * 12 + 1;
		}

		disp_rectangle(left, 136 - DISP_FONTSIZE * 3 - 1,
					   right, 136 + DISP_FONTSIZE * 5, COLOR_WHITE);
		disp_fillrect(left + 1, 136 - DISP_FONTSIZE * 3,
					  right - 1, 136 + DISP_FONTSIZE * 5 - 1, config.msgbcolor);
		disp_putstring(left + 1, 136 - DISP_FONTSIZE * 3,
					   COLOR_WHITE, (const byte *) _("△  退出操作"));
		if (selcount <= 1 && strcmp(item[selidx].compname->ptr, "..") != 0) {
			scene_single_file_ops_draw(item, selidx);
		} else {
			if (mp3count + dircount > 0 && bmcount > 0) {
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2,
							   COLOR_WHITE, (const byte *) _("○  添加音乐"));
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE,
							   COLOR_WHITE, (const byte *) _("□  导入书签"));
			} else if (mp3count + dircount > 0) {
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2,
							   COLOR_WHITE, (const byte *) _("○  添加音乐"));
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE,
							   COLOR_WHITE, (const byte *) _("□  添加音乐"));
			} else if (bmcount > 0) {
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2,
							   COLOR_WHITE, (const byte *) _("○  导入书签"));
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE,
							   COLOR_WHITE, (const byte *) _("□  导入书签"));
			}
		}
		if (where == scene_in_dir) {
			if (selcount > 1 || strcmp(item[selidx].compname->ptr, "..") != 0) {
				if (strnicmp(config.path, "ms0:/", 5) == 0) {
					if (config.allowdelete)
						disp_putstring(240 - DISP_FONTSIZE * 3, 136,
									   COLOR_WHITE,
									   (const byte *) _("×    删除"));
					disp_putstring(240 - DISP_FONTSIZE * 3,
								   136 + DISP_FONTSIZE * 2, COLOR_WHITE,
								   (const byte *) _(" R    剪切"));
				}
				if (config.path[0] != 0)
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE,
								   COLOR_WHITE, (const byte *) _(" L    复制"));
			}
			if (strnicmp(config.path, "ms0:/", 5) == 0
				&& ((copycount > 0 && stricmp(copydir, config.shortpath) != 0)
					|| (cutcount > 0
						&& stricmp(cutdir, config.shortpath) != 0)))
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE * 3,
							   COLOR_WHITE, (const byte *) _("START 粘贴"));
		}
		if (where == scene_in_zip || where == scene_in_chm
			|| where == scene_in_rar) {
			if (selcount > 1 || strcmp(item[selidx].compname->ptr, "..") != 0) {
				if (config.path[0] != 0)
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE,
								   COLOR_WHITE, (const byte *) _(" L    复制"));
			}
		}
		disp_flip();
		bool inop = true;
		t_win_menu_op retop = win_menu_op_continue;

		while (inop) {
			dword key = ctrl_waitany();

			switch (key) {
				case PSP_CTRL_TRIANGLE:
					inop = false;
					retop = win_menu_op_continue;
					break;
				case PSP_CTRL_CROSS:
					if (!config.allowdelete || where != scene_in_dir
						|| strnicmp(config.path, "ms0:/", 5) != 0)
						break;
					if (win_msgbox
						(_("删除所选文件？"), _("是"), _("否"),
						 COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
						char fn[PATH_MAX];

						config.lastfile[0] = 0;
						for (sidx = 0; sidx < filecount; sidx++)
							if (item[sidx].selected) {
								STRCPY_S(fn, config.shortpath);
								STRCAT_S(fn, item[sidx].shortname->ptr);
								if ((t_fs_filetype) item[sidx].data ==
									fs_filetype_dir) {
									if (!utils_del_dir(fn)) {
										win_msg(_("删除目录失败!"), COLOR_WHITE,
												COLOR_WHITE, config.msgbcolor);
										return win_menu_op_redraw;
									}
								} else {
									if (!utils_del_file(fn)) {
										win_msg(_("删除文件失败!"), COLOR_WHITE,
												COLOR_WHITE, config.msgbcolor);
										return win_menu_op_redraw;
									}
								}
								if (config.lastfile[0] == 0) {
									int idx = sidx + 1;

									while (idx < filecount
										   && item[idx].selected)
										idx++;
									if (idx < filecount)
										STRCPY_S(config.lastfile,
												 item[idx].compname->ptr);
									else if (sidx > 0)
										STRCPY_S(config.lastfile,
												 item[idx - 1].compname->ptr);
								}
							}
						retop = win_menu_op_cancel;
					}
					inop = false;
					break;
				case PSP_CTRL_SQUARE:
					if (selcount <= 1) {
						sidx = selidx;
						switch ((t_fs_filetype) item[sidx].data) {
#ifdef ENABLE_MUSIC
							case fs_filetype_dir:
								if (win_msgbox
									(_("添加目录内所有歌曲到播放列表？"),
									 _("是"), _("否"),
									 COLOR_WHITE, COLOR_WHITE,
									 config.msgbcolor)) {
									char cfn[PATH_MAX];

									SPRINTF_S(cfn, "%s%s/", config.path,
											  item[sidx].compname->ptr);
									mp3_list_add_dir(cfn);
									win_msg(_("已添加歌曲到列表!"), COLOR_WHITE,
											COLOR_WHITE, config.msgbcolor);
								}
								break;
							case fs_filetype_mp3:
							case fs_filetype_aa3:
#ifdef ENABLE_WMA
							case fs_filetype_wma:
#endif
								if (win_msgbox
									(_("添加歌曲到播放列表？"), _("是"),
									 _("否"), COLOR_WHITE, COLOR_WHITE,
									 config.msgbcolor)) {
									char mp3name[PATH_MAX],
										mp3longname[PATH_MAX];

									STRCPY_S(mp3name, config.shortpath);
									STRCAT_S(mp3name,
											 filelist[sidx].shortname->ptr);
									STRCPY_S(mp3longname, config.path);
									STRCAT_S(mp3longname,
											 filelist[sidx].compname->ptr);
									mp3_list_add(mp3name, mp3longname);
									win_msg(_("已添加歌曲到列表!"), COLOR_WHITE,
											COLOR_WHITE, config.msgbcolor);
								}
								break;
#endif
#ifdef ENABLE_PMPAVC
							case fs_filetype_pmp:
								if (where == scene_in_dir) {
									pmp_restart = true;
									retop = win_menu_op_ok;
								}
								break;
#endif
#ifdef ENABLE_BG
							case fs_filetype_png:
							case fs_filetype_gif:
							case fs_filetype_jpg:
							case fs_filetype_tga:
							case fs_filetype_bmp:
								{
									set_background_image(item, index);
								}
								break;
#endif
							default:
								g_force_text_view_mode = true;
								retop = win_menu_op_ok;
								break;
						}
					} else if (mp3count + dircount == 0 && bmcount == 0)
						break;
					else {
						if (where != scene_in_dir)
							break;
						if (bmcount > 0) {
							if (!win_msgbox
								(_("是否要导入书签？"), _("是"),
								 _("否"), COLOR_WHITE, COLOR_WHITE,
								 config.msgbcolor))
								break;
						} else {
							if (!win_msgbox
								(_("添加歌曲到播放列表？"), _("是"),
								 _("否"), COLOR_WHITE, COLOR_WHITE,
								 config.msgbcolor))
								break;
						}
						for (sidx = 0; sidx < filecount; sidx++)
							if (item[sidx].selected) {
								switch ((t_fs_filetype) item[sidx].data) {
#ifdef ENABLE_MUSIC
									case fs_filetype_dir:
										if (bmcount == 0) {
											char cfn[PATH_MAX];

											SPRINTF_S(cfn, "%s%s/", config.path,
													  item[sidx].compname->ptr);
											mp3_list_add_dir(cfn);
										}
										break;
#endif
									case fs_filetype_ebm:
										if (bmcount > 0) {
											char bmfn[PATH_MAX];

											STRCPY_S(bmfn, config.path);
											STRCAT_S(bmfn,
													 item[sidx].shortname->ptr);
											bookmark_import(bmfn);
										}
										break;
#ifdef ENABLE_MUSIC
									case fs_filetype_mp3:
									case fs_filetype_aa3:
#ifdef ENABLE_WMA
									case fs_filetype_wma:
#endif
										if (bmcount == 0) {
											char mp3name[PATH_MAX],
												mp3longname[PATH_MAX];

											STRCPY_S(mp3name, config.shortpath);
											STRCAT_S(mp3name,
													 filelist[sidx].shortname->
													 ptr);
											STRCPY_S(mp3longname, config.path);
											STRCAT_S(mp3longname,
													 filelist[sidx].compname->
													 ptr);
											mp3_list_add(mp3name, mp3longname);
										}
										break;
#endif
									default:
										break;
								}
							}
						if (mp3count + dircount == 0 && bmcount > 0)
							win_msg(_("已导入书签!"), COLOR_WHITE, COLOR_WHITE,
									config.msgbcolor);
#ifdef ENABLE_MUSIC
						else if (mp3count + dircount > 0)
							win_msg(_("已添加歌曲到列表!"), COLOR_WHITE,
									COLOR_WHITE, config.msgbcolor);
#endif
					}
					inop = false;
					break;
				case PSP_CTRL_CIRCLE:
					if (selcount <= 1)
						retop = win_menu_op_ok;
					else if (mp3count + dircount == 0 && bmcount == 0)
						break;
					else {
						if (where != scene_in_dir)
							break;
						if (mp3count + dircount == 0) {
							if (!win_msgbox
								(_("是否要导入书签？"), _("是"),
								 _("否"), COLOR_WHITE, COLOR_WHITE,
								 config.msgbcolor))
								break;
						}
#ifdef ENABLE_MUSIC
						else {
							if (!win_msgbox
								(_("添加歌曲到播放列表？"), _("是"),
								 _("否"), COLOR_WHITE, COLOR_WHITE,
								 config.msgbcolor))
								break;
						}
#endif
						for (sidx = 0; sidx < filecount; sidx++)
							if (item[sidx].selected) {
								switch ((t_fs_filetype) item[sidx].data) {
#ifdef ENABLE_MUSIC
									case fs_filetype_dir:
										if (dircount > 0) {
											char cfn[PATH_MAX];

											SPRINTF_S(cfn, "%s%s/", config.path,
													  item[sidx].compname->ptr);
											mp3_list_add_dir(cfn);
										}
										break;
#endif
									case fs_filetype_ebm:
										if (mp3count + dircount == 0) {
											char bmfn[PATH_MAX];

											STRCPY_S(bmfn, config.path);
											STRCAT_S(bmfn,
													 item[sidx].shortname->ptr);
											bookmark_import(bmfn);
										}
										break;
#ifdef ENABLE_MUSIC
									case fs_filetype_mp3:
									case fs_filetype_aa3:
#ifdef ENABLE_WMA
									case fs_filetype_wma:
#endif
										if (mp3count > 0) {
											char mp3name[PATH_MAX],
												mp3longname[PATH_MAX];

											STRCPY_S(mp3name, config.shortpath);
											STRCAT_S(mp3name,
													 filelist[sidx].shortname->
													 ptr);
											STRCPY_S(mp3longname, config.path);
											STRCAT_S(mp3longname,
													 filelist[sidx].compname->
													 ptr);
											mp3_list_add(mp3name, mp3longname);
										}
										break;
#endif
									default:
										break;
								}
							}
						if (mp3count + dircount == 0 && bmcount > 0)
							win_msg(_("已导入书签!"), COLOR_WHITE, COLOR_WHITE,
									config.msgbcolor);
#ifdef ENABLE_MUSIC
						else if (mp3count + dircount > 0)
							win_msg(_("已添加歌曲到列表!"), COLOR_WHITE,
									COLOR_WHITE, config.msgbcolor);
#endif
					}
					inop = false;
					break;
				case PSP_CTRL_LTRIGGER:
//                  if (where != scene_in_dir)
//                      break;
					if (copycount > 0 && copylist != NULL) {
						win_item_destroy(&copylist, &copycount);
					}
					if (cutcount > 0 && cutlist != NULL) {
						win_item_destroy(&cutlist, &cutcount);
					}
					if (where == scene_in_dir) {
						copy_archmode = false;
						STRCPY_S(copydir, config.shortpath);
					} else {
						copy_archmode = true;
						copy_where = where;
						STRCPY_S(copydir, config.shortpath);
					}
					copylist =
						win_realloc_items(NULL, 0,
										  (selcount > 0 ? selcount : 1));
					if (selcount < 1) {
						copycount = 1;
						win_copy_item(&copylist[0], &filelist[selidx]);
					} else {
						copycount = selcount;
						sidx = 0;
						for (selidx = 0; selidx < filecount; selidx++)
							if (item[selidx].selected)
								win_copy_item(&copylist[sidx++],
											  &filelist[selidx]);
					}
					inop = false;
					break;
				case PSP_CTRL_RTRIGGER:
					if (where != scene_in_dir
						|| strnicmp(config.path, "ms0:/", 5) != 0)
						break;
					if (copycount > 0 && copylist != NULL) {
						copycount = 0;
						free((void *) copylist);
						copylist = NULL;
					}
					if (cutcount > 0 && cutlist != NULL) {
						cutcount = 0;
						free((void *) cutlist);
						cutlist = NULL;
					}
					STRCPY_S(cutdir, config.shortpath);
					cutlist =
						win_realloc_items(NULL, 0,
										  (selcount > 0 ? selcount : 1));
					if (selcount < 1) {
						cutcount = 1;
						win_copy_item(&cutlist[0], &filelist[selidx]);
					} else {
						cutcount = selcount;
						sidx = 0;
						for (selidx = 0; selidx < filecount; selidx++)
							if (item[selidx].selected)
								win_copy_item(&cutlist[sidx++],
											  &filelist[selidx]);
					}
					inop = false;
					break;
				case PSP_CTRL_START:
					if (strnicmp(config.path, "ms0:/", 5) != 0)
						break;
//                  if (where != scene_in_dir
//                      || strnicmp(config.path, "ms0:/", 5) != 0)
//                      break;
					if (copycount > 0) {
						for (sidx = 0; sidx < copycount; sidx++) {
							scene_copy_files(sidx);
						}
						STRCPY_S(config.lastfile, item[*index].compname->ptr);
						retop = win_menu_op_cancel;
						inop = false;
					} else if (cutcount > 0) {
						for (sidx = 0; sidx < cutcount; sidx++) {
							char cutsrc[PATH_MAX], cutdest[PATH_MAX];

							STRCPY_S(cutsrc, cutdir);
							STRCAT_S(cutsrc, cutlist[sidx].shortname->ptr);
							STRCPY_S(cutdest, config.shortpath);
							STRCAT_S(cutdest, cutlist[sidx].shortname->ptr);
							dword result;

							if ((t_fs_filetype) cutlist[sidx].data ==
								fs_filetype_dir)
								result = (dword) move_dir(cutsrc, cutdest, NULL,
														  confirm_overwrite,
														  NULL);
							else
								result = move_file(cutsrc, cutdest, NULL,
												   confirm_overwrite, NULL);
							if (result == (dword) false) {
								win_msg(_("文件/目录移动失败!"), COLOR_WHITE,
										COLOR_WHITE, config.msgbcolor);
							}
						}
						STRCPY_S(config.lastfile, item[*index].compname->ptr);
						retop = win_menu_op_cancel;
						inop = false;
						cutcount = 0;
						free((void *) cutlist);
						cutlist = NULL;
					}
					break;
				case PSP_CTRL_SELECT:
					{
						if (((t_fs_filetype) filelist[selidx].data) !=
							fs_filetype_jpg)
							break;
						int result;
						dword width, height;
						pixel *imgdata = NULL;
						pixel bgcolor = 0;
						char filename[PATH_MAX];

						if (where == scene_in_zip || where == scene_in_chm
							|| where == scene_in_rar)
							STRCPY_S(filename, filelist[selidx].compname->ptr);
						else {
							STRCPY_S(filename, config.shortpath);
							STRCAT_S(filename, filelist[selidx].shortname->ptr);
						}

						switch (where) {
							case scene_in_zip:
								result =
									exif_readjpg_in_zip(config.shortpath,
														filename, &width,
														&height, &imgdata,
														&bgcolor);
								break;
							case scene_in_chm:
								result =
									exif_readjpg_in_chm(config.shortpath,
														filename, &width,
														&height, &imgdata,
														&bgcolor);
								break;
							case scene_in_rar:
								result =
									exif_readjpg_in_rar(config.shortpath,
														filename, &width,
														&height, &imgdata,
														&bgcolor);
								break;
							default:
								{
									result =
										exif_readjpg(filename, &width, &height,
													 &imgdata, &bgcolor);
								}
								break;
						}

						free((void *) imgdata);

						inop = false;
						if (exif_array && exif_array->used) {
							char infotitle[256];

							if (strrchr(filename, '/') != NULL) {
								SPRINTF_S(infotitle, _("%s的EXIF信息"),
										  strrchr(filename, '/') + 1);
							} else
								SPRINTF_S(infotitle, _("%s的EXIF信息"),
										  filename);
							infotitle[255] = '\0';
							buffer *b = buffer_init();
							int i;

							for (i = 0; i < exif_array->used - 1; ++i) {
								buffer_append_string(b,
													 exif_array->ptr[i]->ptr);
								buffer_append_string(b, "\r\n");
							}
							buffer_append_string(b, exif_array->ptr[i]->ptr);

							scene_readbook_raw(infotitle,
											   (const unsigned char *) b->ptr,
											   b->used, fs_filetype_txt);
							g_force_text_view_mode = false;
							buffer_free(b);
						} else
							win_msg(_("无EXIF信息"), COLOR_WHITE, COLOR_WHITE,
									config.msgbcolor);
					}
					break;
			}
		}
		if (selcount == 0)
			item[selidx].selected = false;
		if (saveimage) {
			disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
						  saveimage);
			disp_flip();
			disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
						  saveimage);
			free((void *) saveimage);
		}
		return retop;
	} else if (key == config.flkey[4] || key == config.flkey2[4])
		return win_menu_op_cancel;
	else if (key == PSP_CTRL_SELECT) {
		{
			switch (scene_options(index)) {
				case 2:
					disp_fillvram(0);
					disp_flip();
					disp_fillvram(0);
				case 1:
					return win_menu_op_cancel;
				default:
					return win_menu_op_force_redraw;
			}
		}
	} else if (key == PSP_CTRL_START) {
		scene_mp3bar();
		return win_menu_op_force_redraw;
	} else if (key == PSP_CTRL_UP || key == CTRL_BACK) {
		if (*index == 0)
			*index = *count - 1;
		else
			(*index)--;
		op = win_menu_op_redraw;
	} else if (key == PSP_CTRL_DOWN || key == CTRL_FORWARD) {
		if (*index == *count - 1)
			*index = 0;
		else
			(*index)++;
		op = win_menu_op_redraw;
	} else if (key == PSP_CTRL_LEFT) {
		if (*index < max_height - 1)
			*index = 0;
		else
			*index -= max_height - 1;
		op = win_menu_op_redraw;
	} else if (key == PSP_CTRL_RIGHT) {
		if (*index + (max_height - 1) >= *count)
			*index = *count - 1;
		else
			*index += max_height - 1;
		op = win_menu_op_redraw;
	} else if (key == config.flkey[1] || key == config.flkey2[1]) {
		*index = 0;
		op = win_menu_op_redraw;
	} else if (key == config.flkey[2] || key == config.flkey2[2]) {
		*index = *count - 1;
		op = win_menu_op_redraw;
	} else if (key == config.flkey[0] || key == config.flkey2[0]
			   || key == CTRL_PLAYPAUSE) {
		ctrl_waitrelease();
		op = win_menu_op_ok;
	}
	if (((t_fs_filetype) item[*index].data == fs_filetype_dir
		 && (t_fs_filetype) item[orgidx].data != fs_filetype_dir)
		|| (*index != orgidx && *index >= *topindex
			&& *index < *topindex + HRR * 2
			&& ((orgidx - *topindex < HRR && *index - *topindex >= HRR)
				|| (orgidx - *topindex >= HRR && *index - *topindex < HRR))))
		return win_menu_op_force_redraw;
	return op;
}

void scene_filelist_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
#ifdef ENABLE_BG
	if (repaintbg) {
		bg_display();
		disp_flip();
		bg_display();
		repaintbg = false;
	}
#endif
	disp_fillrect(0, 0, 479, DISP_FONTSIZE - 1, 0);
	char infomsg[80];

	STRCPY_S(infomsg, XREADER_VERSION_STR_LONG);
#ifdef _DEBUG
	STRCAT_S(infomsg, _(" 调试版"));
#endif
	disp_putstring(0, 0, COLOR_WHITE, (const byte *) infomsg);
	disp_line(0, DISP_FONTSIZE, 479, DISP_FONTSIZE, COLOR_WHITE);
	disp_rectangle(239 - WRR * DISP_FONTSIZE,
				   138 - (HRR + 1) * (DISP_FONTSIZE + 1),
				   243 + WRR * DISP_FONTSIZE, 141 + HRR * (DISP_FONTSIZE + 1),
				   COLOR_WHITE);
	disp_fillrect(240 - WRR * DISP_FONTSIZE,
				  139 - (HRR + 1) * (DISP_FONTSIZE + 1),
				  242 + WRR * DISP_FONTSIZE, 137 - HRR * (DISP_FONTSIZE + 1),
				  config.titlecolor);
	disp_putnstring(240 - WRR * DISP_FONTSIZE,
					139 - (HRR + 1) * (DISP_FONTSIZE + 1), COLOR_WHITE,
					(const byte *) config.path,
					config.filelistwidth * 4 / DISP_FONTSIZE - 3, 0, 0,
					DISP_FONTSIZE, 0);
	disp_line(240 - WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1),
			  242 + WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1),
			  COLOR_WHITE);
	disp_line(0, 271 - DISP_FONTSIZE, 479, 271 - DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
	disp_putstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE,
				   (const byte *)
				   _
				   ("START 音乐播放控制   SELECT 选项   选项内按□进入按键设置"));
}

void scene_filelist_postdraw(p_win_menuitem item, dword index, dword topindex,
							 dword max_height)
{
	STRCPY_S(config.lastfile, item[index].compname->ptr);
	if (!config.showfinfo)
		return;
	if ((t_fs_filetype) item[index].data != fs_filetype_dir) {
		if (where == scene_in_dir) {
			if (index - topindex < HRR) {
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE,
							   135 + (HRR - 3) * (DISP_FONTSIZE + 1),
							   243 + (WRR - 2) * DISP_FONTSIZE,
							   136 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE,
							  136 + (HRR - 3) * (DISP_FONTSIZE + 1),
							  242 + (WRR - 2) * DISP_FONTSIZE,
							  135 + HRR * (DISP_FONTSIZE + 1), RGB(0x20, 0x20,
																   0x20));
				char outstr[256];

				SPRINTF_S(outstr, _("文件大小: %u 字节\n"),
						  (unsigned int) item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   136 + (HRR - 3) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
				SPRINTF_S(outstr,
						  _("创建时间: %04d-%02d-%02d %02d:%02d:%02d\n"),
						  (item[index].data2[0] >> 9) + 1980,
						  (item[index].data2[0] & 0x01FF) >> 5,
						  item[index].data2[0] & 0x01F,
						  item[index].data2[1] >> 11,
						  (item[index].data2[1] & 0x07FF) >> 5,
						  (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   136 + (HRR - 2) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
				SPRINTF_S(outstr,
						  _("最后修改: %04d-%02d-%02d %02d:%02d:%02d\n"),
						  (item[index].data2[2] >> 9) + 1980,
						  (item[index].data2[2] & 0x01FF) >> 5,
						  item[index].data2[2] & 0x01F,
						  item[index].data2[3] >> 11,
						  (item[index].data2[3] & 0x07FF) >> 5,
						  (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   136 + (HRR - 1) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
			} else {
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE,
							   141 - HRR * (DISP_FONTSIZE + 1),
							   243 + (WRR - 2) * DISP_FONTSIZE,
							   142 - (HRR - 3) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE,
							  142 - HRR * (DISP_FONTSIZE + 1),
							  242 + (WRR - 2) * DISP_FONTSIZE,
							  141 - (HRR - 3) * (DISP_FONTSIZE + 1), RGB(0x20,
																		 0x20,
																		 0x20));
				char outstr[256];

				SPRINTF_S(outstr, _("文件大小: %u 字节\n"),
						  (unsigned int) item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   142 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE,
							   (const byte *) outstr);
				SPRINTF_S(outstr,
						  _("创建时间: %04d-%02d-%02d %02d:%02d:%02d\n"),
						  (item[index].data2[0] >> 9) + 1980,
						  (item[index].data2[0] & 0x01FF) >> 5,
						  item[index].data2[0] & 0x01F,
						  item[index].data2[1] >> 11,
						  (item[index].data2[1] & 0x07FF) >> 5,
						  (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   142 - (HRR - 1) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
				SPRINTF_S(outstr,
						  _("最后修改: %04d-%02d-%02d %02d:%02d:%02d\n"),
						  (item[index].data2[2] >> 9) + 1980,
						  (item[index].data2[2] & 0x01FF) >> 5,
						  item[index].data2[2] & 0x01F,
						  item[index].data2[3] >> 11,
						  (item[index].data2[3] & 0x07FF) >> 5,
						  (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   142 - (HRR - 2) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
			}
		} else {
			char outstr[256];

			SPRINTF_S(outstr, _("文件大小: %s 字节\n"),
					  item[index].shortname->ptr);
			if (index - topindex < HRR) {
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE,
							   135 + (HRR - 1) * (DISP_FONTSIZE + 1),
							   243 + (WRR - 2) * DISP_FONTSIZE,
							   136 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE,
							  136 + (HRR - 1) * (DISP_FONTSIZE + 1),
							  242 + (WRR - 2) * DISP_FONTSIZE,
							  135 + HRR * (DISP_FONTSIZE + 1), RGB(0x20, 0x20,
																   0x20));
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   136 + (HRR - 1) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE, (const byte *) outstr);
			} else {
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE,
							   141 - HRR * (DISP_FONTSIZE + 1),
							   243 + (WRR - 2) * DISP_FONTSIZE,
							   142 - (HRR - 1) * (DISP_FONTSIZE + 1),
							   COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE,
							  142 - HRR * (DISP_FONTSIZE + 1),
							  242 + (WRR - 2) * DISP_FONTSIZE,
							  141 - (HRR - 1) * (DISP_FONTSIZE + 1), RGB(0x20,
																		 0x20,
																		 0x20));
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE,
							   142 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE,
							   (const byte *) outstr);
			}
		}
	}
}

char *strtoupper(char *d, const char *s)
{
	char *origd = d;

	while ((*d++ = toupper(*s)))
		s++;

	return origd;
}

void exec_homebrew(int method, char *path)
{
	int err;
	struct SceKernelLoadExecVSHParam param;

	memset(&param, 0, sizeof(param));
	param.size = sizeof(struct SceKernelLoadExecVSHParam);
	param.args = strlen(path) + 1;
	param.argp = path;
	if (method == 2)
		param.key = "game";
	else if (method == 4) {
		param.key = "psx";
	} else
		param.key = "updater";
	if ((err = xrKernelLoadExecVSHMsX(method, path, &param)) < 0) {
		char infomsg[256];

		SPRINTF_S(infomsg, _("%s: 方法%d启动错误, 返回错误代码: %08x"), path,
				  method, err);
		win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		dbg_printf(d, infomsg);
	} else
		sceKernelExitDeleteThread(0);
}

void scene_filelist()
{
	dword idx = 0;

	where = scene_in_dir;
	if (strlen(config.shortpath) == 0) {
		STRCPY_S(config.path, "ms0:/");
		STRCPY_S(config.shortpath, "ms0:/");
	}
	dword plen = strlen(config.path);

	if (plen > 0 && config.path[plen - 1] == '/')
		filecount =
			fs_dir_to_menu(config.path, config.shortpath, &filelist,
						   config.menutextcolor, config.selicolor,
						   config.usedyncolor ? GetBGColorByTime() : config.
						   menubcolor, config.selbcolor, config.showhidden,
						   config.showunknown);
	else
		switch (fs_file_get_type(config.path)) {
			case fs_filetype_zip:
				{
					where = scene_in_zip;
					filecount =
						fs_zip_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				}
			case fs_filetype_chm:
				{
					where = scene_in_chm;
					filecount =
						fs_chm_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				}
			case fs_filetype_rar:
				{
					where = scene_in_rar;
					filecount =
						fs_rar_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				}
			default:
				STRCPY_S(config.path, "ms0:/");
				STRCPY_S(config.shortpath, "ms0:/");
				filecount =
					fs_dir_to_menu(config.path, config.shortpath, &filelist,
								   config.menutextcolor, config.selicolor,
								   config.
								   usedyncolor ? GetBGColorByTime() : config.
								   menubcolor, config.selbcolor,
								   config.showhidden, config.showunknown);
				break;
		}
	if (filecount == 0) {
		STRCPY_S(config.path, "ms0:/");
		STRCPY_S(config.shortpath, "ms0:/");
		filecount =
			fs_dir_to_menu(config.path, config.shortpath, &filelist,
						   config.menutextcolor, config.selicolor,
						   config.usedyncolor ? GetBGColorByTime() : config.
						   menubcolor, config.selbcolor, config.showhidden,
						   config.showunknown);
	}
	quicksort(filelist,
			  (filecount > 0
			   && filelist[0].compname->ptr[0] == '.') ? 1 : 0, filecount - 1,
			  sizeof(t_win_menuitem), compare_func[(int) config.arrange]);
	while (idx < filecount
		   && stricmp(filelist[idx].compname->ptr, config.lastfile) != 0)
		idx++;
	if (idx >= filecount) {
		config.isreading = false;
		idx = 0;
	}
#ifdef ENABLE_USB
	if (config.enableusb)
		usb_activate();
	else
		usb_deactivate();
#endif
	while (1) {
		if (!config.isreading && !locreading) {
			if (filelist == 0 || filecount == 0) {
				// empty directory ?
				if (where == scene_in_dir) {
					filelist =
						fs_empty_dir(&filecount, config.menutextcolor,
									 config.selicolor,
									 config.
									 usedyncolor ? GetBGColorByTime() :
									 config.menubcolor, config.selbcolor);
					idx = 0;
					idx =
						win_menu(240 - WRR * DISP_FONTSIZE,
								 139 - HRR * (DISP_FONTSIZE + 1), WRR * 4,
								 HRR * 2, filelist, filecount, idx, 0,
								 config.
								 usedyncolor ? GetBGColorByTime() : config.
								 menubcolor, false, scene_filelist_predraw,
								 scene_filelist_postdraw,
								 scene_filelist_menucb);
				} else {
					char infomsg[80];

					SPRINTF_S(infomsg, _("%s压缩包格式损坏"), config.path);
					win_msg(infomsg, COLOR_WHITE, COLOR_WHITE,
							config.msgbcolor);
					int ll;

					if ((ll = strlen(config.path) - 1) >= 0)
						while (config.path[ll] == '/' && ll >= 0) {
							config.path[ll] = 0;
							ll--;
						}
					char *lps;

					if ((lps = strrchr(config.path, '/')) != NULL) {
						lps++;
						*lps = 0;
					} else
						config.path[0] = 0;
					where = scene_in_dir;
					// when exit to menu specify idx to INVALID
					idx = INVALID;
				}
			} else
				idx =
					win_menu(240 - WRR * DISP_FONTSIZE,
							 139 - HRR * (DISP_FONTSIZE + 1), WRR * 4, HRR * 2,
							 filelist, filecount, idx, 0,
							 config.usedyncolor ? GetBGColorByTime() : config.
							 menubcolor, false, scene_filelist_predraw,
							 scene_filelist_postdraw, scene_filelist_menucb);
		} else {
			config.isreading = false;
			locreading = false;
		}
		if (idx == INVALID) {
			switch (where) {
				case scene_in_zip:
					filecount =
						fs_zip_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				case scene_in_chm:
					filecount =
						fs_chm_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				case scene_in_rar:
					filecount =
						fs_rar_to_menu(config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor);
					break;
				default:
					filecount =
						fs_dir_to_menu(config.path, config.shortpath, &filelist,
									   config.menutextcolor, config.selicolor,
									   config.
									   usedyncolor ? GetBGColorByTime() :
									   config.menubcolor, config.selbcolor,
									   config.showhidden, config.showunknown);
			}
			if (filelist == 0) {
				STRCPY_S(config.path, "ms0:/");
				STRCPY_S(config.shortpath, "ms0:/");
				filecount =
					fs_dir_to_menu(config.path, config.shortpath, &filelist,
								   config.menutextcolor, config.selicolor,
								   config.
								   usedyncolor ? GetBGColorByTime() : config.
								   menubcolor, config.selbcolor,
								   config.showhidden, config.showunknown);
			}
			quicksort(filelist,
					  (filecount > 0
					   && filelist[0].compname->ptr[0] == '.') ? 1 : 0,
					  filecount - 1, sizeof(t_win_menuitem),
					  compare_func[(int) config.arrange]);
			idx = 0;
			while (idx < filecount
				   && stricmp(filelist[idx].compname->ptr,
							  config.lastfile) != 0)
				idx++;
			if (idx >= filecount) {
				config.isreading = false;
				idx = 0;
			}
			continue;
		}
		switch ((t_fs_filetype) filelist[idx].data) {
			case fs_filetype_prog:
				{
					char infomsg[80];

					SPRINTF_S(infomsg, _("是否以%s方式执行该程序?"),
							  config.launchtype ==
							  2 ? _("普通程序") : _("PS游戏"));
					if (win_msgbox
						(infomsg, _("是"), _("否"), COLOR_WHITE, COLOR_WHITE,
						 config.msgbcolor)) {
						char path[PATH_MAX], upper[PATH_MAX];

						conf_save(&config);
						STRCPY_S(path, config.path);
						strtoupper(upper, filelist[idx].compname->ptr);
						STRCAT_S(path, upper);
						exec_homebrew(config.launchtype, path);
					}
				}
				break;
			case fs_filetype_dir:
				{
					char pdir[PATH_MAX];
					bool isup = false;

					pdir[0] = 0;
					if (strcmp(filelist[idx].compname->ptr, "..") == 0) {
						if (where == scene_in_dir) {
							int ll;

							if ((ll = strlen(config.path) - 1) >= 0)
								while (config.path[ll] == '/' && ll >= 0) {
									config.path[ll] = 0;
									ll--;
								}
						}
						char *lps;

						isup = true;
						if ((lps = strrchr(config.path, '/')) != NULL) {
							lps++;
							STRCPY_S(pdir, lps);
							*lps = 0;
						} else
							config.path[0] = 0;
					} else if (where == scene_in_dir) {
						STRCAT_S(config.path, filelist[idx].compname->ptr);
						STRCAT_S(config.path, "/");
					}
					if (config.path[0] == 0) {
						fat_inited = false;
						fat_free();
						filecount =
							fs_list_device(config.path, config.shortpath,
										   &filelist, config.menutextcolor,
										   config.selicolor,
										   config.
										   usedyncolor ? GetBGColorByTime() :
										   config.menubcolor, config.selbcolor);
					} else if (strnicmp(config.path, "ms0:/", 5) == 0) {
						if (fat_inited == false) {
							fat_init();
							fat_inited = true;
						}
						filecount =
							fs_dir_to_menu(config.path, config.shortpath,
										   &filelist, config.menutextcolor,
										   config.selicolor,
										   config.
										   usedyncolor ? GetBGColorByTime() :
										   config.menubcolor, config.selbcolor,
										   config.showhidden,
										   config.showunknown);
					} else
						filecount =
							fs_flashdir_to_menu(config.path, config.shortpath,
												&filelist, config.menutextcolor,
												config.selicolor,
												config.
												usedyncolor ? GetBGColorByTime()
												: config.menubcolor,
												config.selbcolor);
					quicksort(filelist,
							  (filecount > 0
							   && filelist[0].compname->ptr[0] == '.') ? 1 : 0,
							  filecount - 1, sizeof(t_win_menuitem),
							  compare_func[(int) config.arrange]);
					if (isup) {
						for (idx = 0; idx < filecount; idx++)
							if (stricmp(filelist[idx].compname->ptr, pdir) == 0)
								break;
						if (idx == filecount)
							idx = 0;
					} else
						idx = 0;
					where = scene_in_dir;
					break;
				}
			case fs_filetype_gz:
#if 0
				if (strlen(filelist[idx].compname->ptr) >= 7) {
					int len = strlen(filelist[idx].compname->ptr) - 7;

					if (stricmp(filelist[idx].compname->ptr + len, ".tar.gz") ==
						0) {
						win_msg("TODO: SUPPORT Tar ", COLOR_WHITE, COLOR_WHITE,
								config.msgbcolor);
						break;
					}
				}
#endif
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readbook(idx);
				g_force_text_view_mode = false;
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
			case fs_filetype_zip:
				where = scene_in_zip;
				STRCAT_S(config.path, filelist[idx].compname->ptr);
				STRCAT_S(config.shortpath, filelist[idx].shortname->ptr);
				idx = 0;
				filecount =
					fs_zip_to_menu(config.shortpath, &filelist,
								   config.menutextcolor, config.selicolor,
								   config.
								   usedyncolor ? GetBGColorByTime() : config.
								   menubcolor, config.selbcolor);
				quicksort(filelist,
						  (filecount > 0
						   && filelist[0].compname->ptr[0] == '.') ? 1 : 0,
						  filecount - 1, sizeof(t_win_menuitem),
						  compare_func[(int) config.arrange]);
				break;
			case fs_filetype_chm:
				where = scene_in_chm;
				STRCAT_S(config.path, filelist[idx].compname->ptr);
				STRCAT_S(config.shortpath, filelist[idx].shortname->ptr);
				idx = 0;
				filecount =
					fs_chm_to_menu(config.shortpath, &filelist,
								   config.menutextcolor, config.selicolor,
								   config.
								   usedyncolor ? GetBGColorByTime() : config.
								   menubcolor, config.selbcolor);
				quicksort(filelist,
						  (filecount > 0
						   && (filelist[0].compname->ptr)[0] == '.') ? 1 : 0,
						  filecount - 1, sizeof(t_win_menuitem),
						  compare_func[(int) config.arrange]);
				break;
			case fs_filetype_rar:
				where = scene_in_rar;
				STRCAT_S(config.path, filelist[idx].compname->ptr);
				STRCAT_S(config.shortpath, filelist[idx].shortname->ptr);
				idx = 0;
				filecount =
					fs_rar_to_menu(config.shortpath, &filelist,
								   config.menutextcolor, config.selicolor,
								   config.
								   usedyncolor ? GetBGColorByTime() : config.
								   menubcolor, config.selbcolor);
				quicksort(filelist,
						  (filecount > 0
						   && filelist[0].compname->ptr[0] == '.') ? 1 : 0,
						  filecount - 1, sizeof(t_win_menuitem),
						  compare_func[(int) config.arrange]);
				break;
#ifdef ENABLE_PMPAVC
			case fs_filetype_pmp:
				{
					char pmpname[PATH_MAX];

					STRCPY_S(pmpname, config.shortpath);
					STRCAT_S(pmpname, filelist[idx].shortname->ptr);
					avc_start();
					pmp_play(pmpname, !pmp_restart);
					avc_end();
#ifdef COLOR16BIT
					disp_fillvram(config.bgcolor);
					bg_display();
					disp_flip();
					disp_fillvram(config.bgcolor);
					bg_display();
					disp_flip();
#endif
					break;
				}
#endif
#ifdef ENABLE_IMAGE
			case fs_filetype_png:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readimage(idx);
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
			case fs_filetype_gif:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readimage(idx);
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
			case fs_filetype_jpg:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readimage(idx);
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
			case fs_filetype_bmp:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readimage(idx);
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
				break;
#endif
			case fs_filetype_tga:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readimage(idx);
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
#endif
			case fs_filetype_ebm:
				if (win_msgbox
					(_("是否要导入书签？"), _("是"), _("否"),
					 COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
					char bmfn[PATH_MAX];

					STRCPY_S(bmfn, config.shortpath);
					STRCAT_S(bmfn, filelist[idx].shortname->ptr);
					bookmark_import(bmfn);
					win_msg(_("已导入书签!"), COLOR_WHITE, COLOR_WHITE,
							config.msgbcolor);
				}
#ifdef ENABLE_MUSIC
			case fs_filetype_mp3:
			case fs_filetype_aa3:
#ifdef ENABLE_WMA
			case fs_filetype_wma:
#endif
				{
					char fn[PATH_MAX], lfn[PATH_MAX];

					STRCPY_S(fn, config.shortpath);
					STRCAT_S(fn, filelist[idx].shortname->ptr);
					STRCPY_S(lfn, config.path);
					STRCAT_S(lfn, filelist[idx].compname->ptr);
					mp3_directplay(fn, lfn);
				}
				break;
#endif
#ifdef ENABLE_TTF
			case fs_filetype_font:
				{
					char fn[PATH_MAX];

					if (where == scene_in_dir) {
						STRCPY_S(fn, config.shortpath);
						STRCAT_S(fn, filelist[idx].shortname->ptr);
					} else {
						STRCPY_S(fn, filelist[idx].compname->ptr);
					}

					if (win_msgbox
						(_("是否为中文字体？"), _("是"), _("否"),
						 COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
						STRCPY_S(config.cttfpath, fn);
						if (where == scene_in_dir)
							STRCPY_S(config.cttfarch, "");
						else
							STRCPY_S(config.cttfarch, config.shortpath);
					} else {
						STRCPY_S(config.ettfpath, fn);
						if (where == scene_in_dir)
							STRCPY_S(config.ettfarch, "");
						else
							STRCPY_S(config.ettfarch, config.shortpath);
					}
					dbg_printf(d, "ettf: %s %s cttf: %s %s", config.ettfarch,
							   config.ettfpath, config.cttfarch,
							   config.cttfpath);
					if (config.usettf) {
						scene_load_book_font();
					}
				}
				break;
#endif
			default:
#ifdef ENABLE_USB
				usb_deactivate();
#endif
				config.isreading = true;
				idx = scene_readbook(idx);
				g_force_text_view_mode = false;
				config.isreading = false;
#ifdef ENABLE_USB
				if (config.enableusb)
					usb_activate();
				else
					usb_deactivate();
#endif
				break;
		}
		if (config.dis_scrsave)
			scePowerTick(0);
	}
	if (filelist != NULL) {
		free((void *) filelist);
		filelist = NULL;
		filecount = 0;
	}
#ifdef ENABLE_USB
	usb_deactivate();
#endif
}

typedef struct
{
	char efont[6];
	char cfont[6];
	dword size;
} t_font_type;

extern double pspDiffTime(u64 * t1, u64 * t2);

#undef printf
#define printf pspDebugScreenPrintf

static const bool is_log = true;
DBG *d = 0;

extern void scene_init()
{
	char logfile[PATH_MAX];
	char infomsg[256];

	getcwd(appdir, PATH_MAX);
	STRCAT_S(appdir, "/");
	STRCPY_S(logfile, appdir);
	STRCAT_S(logfile, "log.txt");
	bool printDebugInfo = false;

	d = dbg_init();
#ifdef _DEBUG
	dbg_switch(d, 1);
	dbg_open_stream(d, stderr);
#else
	dbg_switch(d, 0);
#endif
	dbg_open_memorylog(d);
	power_set_clock(333, 166);
	ctrl_init();
	dword key = ctrl_read();

	if (key == PSP_CTRL_LTRIGGER) {
		printDebugInfo = true;
		dbg_switch(d, 1);
		dbg_open_psp_logfile(d, logfile);
		pspDebugScreenInit();
		dbg_open_psp(d);
	}

	SPRINTF_S(infomsg, "%s %s (gcc version %d.%d.%d %s) now loading...",
			  PACKAGE_NAME, VERSION, __GNUC__, __GNUC_MINOR__,
			  __GNUC_PATCHLEVEL__, "Built " __TIME__ " " __DATE__);
	dbg_printf(d, infomsg);
	u64 dbgnow, dbglasttick;
	u64 start, end;

	sceRtcGetCurrentTick(&dbglasttick);
	start = dbglasttick;
#ifdef ENABLE_USB
	if (config.enableusb)
		usb_open();
#endif
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "usb_open(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));

	sceRtcGetCurrentTick(&dbglasttick);
	char fontzipfile[PATH_MAX], efontfile[PATH_MAX], cfontfile[PATH_MAX],
		conffile[PATH_MAX], locconf[PATH_MAX], bmfile[PATH_MAX]
#ifdef ENABLE_MUSIC
	, mp3conf[PATH_MAX]
#endif
	;

#ifdef HOMEBREW_2XX
	{
		int i = 0, l = strlen(appdir);

		for (i = 0; i < l; i++)
			if (appdir[i] == '%') {
				memmove(&appdir[i], &appdir[i + 1], l - i);
				i--;
				l--;
			}
	}
#endif
	STRCPY_S(config.path, "ms0:/");
	STRCPY_S(config.shortpath, "ms0:/");

	STRCPY_S(conffile, appdir);
	char conffilename[80];

	SPRINTF_S(conffilename, "%s%d%s", "config", config_num, ".ini");
	STRCAT_S(conffile, conffilename);
	conf_set_file(conffile);
#ifdef ENABLE_MUSIC
	STRCPY_S(mp3conf, appdir);
	STRCAT_S(mp3conf, "music.lst");
#endif

	if (key == PSP_CTRL_RTRIGGER) {
#ifdef ENABLE_MUSIC
		utils_del_file(mp3conf);
#endif
		utils_del_file(conffile);
		conf_load(&config);
		conf_save(&config);
	} else {
		conf_load(&config);
		sceRtcGetCurrentTick(&dbgnow);
		dbg_printf(d, "conf_load() etc: %.2fs",
				   pspDiffTime(&dbgnow, &dbglasttick));
	}

#ifdef ENABLE_BG
	sceRtcGetCurrentTick(&dbglasttick);
	bg_load(config.bgfile, config.bgarch, config.bgcolor,
			fs_file_get_type(config.bgfile), config.grayscale, config.bgwhere);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "bg_load(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
#endif

	STRCPY_S(fontzipfile, appdir);
	STRCAT_S(fontzipfile, "fonts.zip");
	int _fsize;

	for (_fsize = 10; _fsize <= 32; _fsize += 2) {
		sceRtcGetCurrentTick(&dbglasttick);
		SPRINTF_S(efontfile, "ASC%02d", _fsize);
		SPRINTF_S(cfontfile, "GBK%02d", _fsize);
		if (disp_has_zipped_font(fontzipfile, efontfile, cfontfile)) {
			if (_fsize <= 16) {
				fonts[fontcount].size = _fsize;
				fonts[fontcount].zipped = true;
				if (fonts[fontcount].size == config.fontsize)
					fontindex = fontcount;
				fontcount++;
			}
			bookfonts[bookfontcount].size = _fsize;
			bookfonts[bookfontcount].zipped = true;
			if (bookfonts[bookfontcount].size == config.bookfontsize)
				bookfontindex = bookfontcount;
			ttfsize = config.bookfontsize;
			bookfontcount++;
		} else {
			SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, _fsize);
			SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, _fsize);
			if (disp_has_font(efontfile, cfontfile)) {
				if (_fsize <= 16) {
					fonts[fontcount].size = _fsize;
					fonts[fontcount].zipped = false;
					if (fonts[fontcount].size == config.fontsize)
						fontindex = fontcount;
					fontcount++;
				}
				bookfonts[bookfontcount].size = _fsize;
				bookfonts[bookfontcount].zipped = true;
				if (bookfonts[bookfontcount].size == config.bookfontsize)
					bookfontindex = bookfontcount;
				ttfsize = config.bookfontsize;
				bookfontcount++;
			}
		}
		sceRtcGetCurrentTick(&dbgnow);
		dbg_printf(d, "has_font(%d): %.2f second", _fsize,
				   pspDiffTime(&dbgnow, &dbglasttick));
	}
	sceRtcGetCurrentTick(&dbglasttick);
	if (fontcount == 0 || !scene_load_font() || !scene_load_book_font()) {
		pspDebugScreenInit();
		pspDebugScreenPrintf
			("Error loading font file! Press any buttun for exit!");
		ctrl_waitany();
		sceKernelExitGame();
	}
	recalcSize(&drperpage, &rowsperpage, &pixelsperrow);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "scene_load_font() & scene_load_book_font(): %.2f second",
			   pspDiffTime(&dbgnow, &dbglasttick));
	sceRtcGetCurrentTick(&dbglasttick);
#ifdef ENABLE_HPRM
	ctrl_enablehprm(config.hprmctrl);
#endif
#ifdef ENABLE_GE
	init_gu();
#endif
	if (fat_init())
		fat_inited = true;

	STRCPY_S(bmfile, appdir);
	STRCAT_S(bmfile, "bookmark.conf");
	bookmark_init(bmfile);
	STRCPY_S(locconf, appdir);
	STRCAT_S(locconf, "location.conf");
	location_init(locconf, locaval);

	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "misc_init(): %.2f second",
			   pspDiffTime(&dbgnow, &dbglasttick));

#ifdef ENABLE_MUSIC
	sceRtcGetCurrentTick(&dbglasttick);
	mp3_init();
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "mp3_init(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));

	if (config.confver < 0x00090100 || !mp3_list_load(mp3conf)) {
		sceRtcGetCurrentTick(&dbglasttick);
		mp3_list_add_dir("ms0:/MUSIC/");
		sceRtcGetCurrentTick(&dbgnow);
		dbg_printf(d, "mp3_list_add_dir(): %.2fs",
				   pspDiffTime(&dbgnow, &dbglasttick));
	}
	sceRtcGetCurrentTick(&dbglasttick);
	mp3_start();
	mp3_set_encode(config.mp3encode);
#ifdef ENABLE_HPRM
	mp3_set_hprm(!config.hprmctrl);
#endif
	mp3_set_cycle(config.mp3cycle);
	if (config.autoplay)
		mp3_resume();
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "mp3_start() etc: %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
#endif

	sceRtcGetCurrentTick(&dbglasttick);
	if (sceKernelDevkitVersion() >= 0x03070100) {
		char prxfn[PATH_MAX];

		SPRINTF_S(prxfn, "%sxrPrx.prx", appdir);
		SceUID uid = pspSdkLoadStartModule(prxfn, PSP_MEMORY_PARTITION_KERNEL);

		if (uid >= 0) {
			if (sceKernelDevkitVersion() < 0x03080000)
				use_prx_power_save = true;
			prx_loaded = true;
		}
	}
	scene_power_save(true);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "pspSdkLoadStartModule(): %.2fs",
			   pspDiffTime(&dbgnow, &dbglasttick));

	sceRtcGetCurrentTick(&end);
	dbg_printf(d, "Load finished in %.2fs, press any key to continue",
			   pspDiffTime(&end, &start));
	if (printDebugInfo) {
#ifdef _DEBUG
		dbg_close_handle(d, 3);
#else
		dbg_close_handle(d, 2);
#endif
		while (ctrl_read() == PSP_CTRL_LTRIGGER) {
			sceKernelDelayThread(100000);
		}
		ctrl_waitany();
	}

	disp_init();
	disp_fillvram(0);
	disp_flip();
	disp_fillvram(0);

	set_language();

	if (prx_loaded) {
		dbg_printf(d, _("亮度设置为%d"), config.brightness);
		xrSetBrightness(config.brightness);
	}

	dword c = GetBGColorByTime();

	dbg_printf(d, "GetBGColorByTime() return %lu/%lu/%lu", RGB_R(c), RGB_G(c),
			   RGB_B(c));

	scene_filelist();
}

extern void scene_exit()
{
	power_set_clock(222, 111);
	if (bm != NULL) {
		bookmark_close(bm);
		bm = NULL;
	}
	if (fs != NULL) {
		char archname[PATH_MAX];

		if (where == scene_in_zip || where == scene_in_chm
			|| where == scene_in_rar) {
			STRCPY_S(archname, config.shortpath);
			if (archname[strlen(archname) - 1] != '/' && fs->filename[0] != '/')
				STRCAT_S(archname, "/");
			STRCAT_S(archname, fs->filename);
		} else
			STRCPY_S(archname, fs->filename);
		if (config.autobm) {
			bookmark_autosave(archname,
							  (fs->rows[fs->crow >> 10] +
							   (fs->crow & 0x3FF))->start - fs->buf);
		}
	}
	// always save to config0.ini
	char conffile[PATH_MAX], appdir[PATH_MAX];

	getcwd(appdir, PATH_MAX);
	STRCAT_S(appdir, "/");
	STRCPY_S(conffile, appdir);
	char conffilename[80];

	SPRINTF_S(conffilename, "%s%d%s", "config", 0, ".ini");
	STRCAT_S(conffile, conffilename);
	conf_set_file(conffile);
	load_fontsize_to_config();
	conf_save(&config);

#ifdef ENABLE_MUSIC
	mp3_end();
#ifdef ENABLE_ME
	extern void *pMMgr;

	MusicMgrRelease(pMMgr);
	DirRelease();
#endif
	char mp3conf[PATH_MAX];

	STRCPY_S(mp3conf, appdir);
	STRCAT_S(mp3conf, "music.lst");
	mp3_list_save(mp3conf);
	mp3_list_free();
#endif

	fat_free();
	disp_free_font();
#ifdef ENABLE_PMPAVC
	avc_free();
#endif
#ifdef ENABLE_USB
	usb_close();
#endif
#ifdef ENABLE_GE
	sceGuTerm();
#endif
	dbg_close(d);
	simple_gettext_destroy();

	if ((ctrl_read() == PSP_CTRL_LTRIGGER)) {
		char path[PATH_MAX];

		STRCPY_S(path, appdir);
		STRCAT_S(path, "EBOOT.PBP");
		ctrl_destroy();
		exec_homebrew(2, path);
	} else {
		ctrl_destroy();
		sceKernelExitGame();
	}
}

extern void scene_power_save(bool save)
{
	if (save
#ifdef ENABLE_MUSIC
		&& mp3_paused()
#endif
		)
		power_set_clock(freq_list[config.freqs[0]][0],
						freq_list[config.freqs[0]][1]);
	else if (imgreading
#ifdef ENABLE_MUSIC
			 && !mp3_paused()
#endif
		) {
		power_set_clock(freq_list[config.freqs[2]][0],
						freq_list[config.freqs[2]][1]);
	} else
		power_set_clock(freq_list[config.freqs[1]][0],
						freq_list[config.freqs[1]][1]);
}

extern void scene_exception()
{
//  config.savesucc = false;
}

extern const char *scene_appdir()
{
	return appdir;
}

dword ColorLerp(dword color1, dword color2, float fWeight)
{
	fWeight = 1.0 - fWeight;
	if (fWeight <= 0.01f)
		return color1;
	else if (fWeight >= 1)
		return color2;
	else {
		byte Weight = (byte) (fWeight * 255);
		byte IWeight = ~Weight;
		dword dwTemp = 0;

		dwTemp = (((0xFF00FF00 & color1) >> 8) * IWeight +
				  ((0xFF00FF00 & color2) >> 8) * Weight) & 0xFF00FF00;

		dwTemp |= (((0x00FF00FF & color1) * IWeight +
					(0x00FF00FF & color2) * Weight) & 0xFF00FF00) >> 8;
		return dwTemp;
	}
}

static dword pspBGColorTable[12] = {
	RGB(182, 182, 182),
	RGB(232, 195, 44),
	RGB(141, 213, 15),
	RGB(241, 141, 167),
	RGB(10, 184, 9),
	RGB(174, 115, 233),
	RGB(3, 204, 194),
	RGB(11, 103, 186),
	RGB(180, 66, 190),
	RGB(236, 175, 22),
	RGB(133, 91, 31),
	RGB(251, 26, 22)
};

static int s_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define IsLeapYear(year)  (((year) % 4 == 0 && (year) % 100) || (year) % 400 == 0)

int GetDaysOfMonth(int y, int m)
{
	return s_month[m - 1] + (m == 2) * IsLeapYear(y);
}

/* time in 24 hours */
int GetDiffSecondOfTime(int srcHour, int srcMinute, int srcSeconds,
						int dstHour, int dstMinute, int dstSeconds)
{
	return (dstHour * 60 * 60 + dstMinute * 60 + dstSeconds -
			(srcHour * 60 * 60 + srcMinute * 60 + srcSeconds));
}

dword GetBGColorByTime(void)
{
	pspTime tm;

	sceRtcGetCurrentClockLocalTime(&tm);

	int cur_month = tm.month;
	int next_month = tm.month == 12 ? 1 : tm.month + 1;
	dword origColor;

	origColor =
		ColorLerp(pspBGColorTable[cur_month - 1],
				  pspBGColorTable[next_month - 1],
				  1.0 * tm.day / GetDaysOfMonth(tm.year, cur_month));

	if (tm.hour >= 12) {
//      dbg_printf(d, "c1: %f", (1.0 * GetDiffSecondOfTime(tm.hour, tm.minutes, 24, 0) / (12.0 * 60)));
		origColor =
			ColorLerp(origColor, RGB(0, 0, 0),
					  1.0 * GetDiffSecondOfTime(tm.hour, tm.minutes, tm.seconds,
												24, 0, 0) / (12 * 60 * 60));
	} else {
//      dbg_printf(d, "c2: %f", (1.0 * GetDiffSecondOfTime(0, 0, tm.hour, tm.minutes) / (12.0 * 60.0)));
		origColor =
			ColorLerp(origColor, RGB(0, 0, 0),
					  1.0 * GetDiffSecondOfTime(0, 0, 0, tm.hour, tm.minutes,
												tm.seconds) / (12 * 60 * 60));
	}

	return origColor;
}
