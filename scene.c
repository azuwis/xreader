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
#include "msgresource.h"
#include "dbg.h"
#ifdef ENABLE_ME
#include "eReader2Lib/musicwrapper.h"
#endif

#ifdef ENABLE_PMPAVC
bool pmp_restart = false;
#endif
char appdir[256], copydir[256], cutdir[256];
dword drperpage, rowsperpage, pixelsperrow;
p_bookmark bm = NULL;
p_text fs = NULL;
t_conf config;
p_win_menuitem filelist = NULL, copylist = NULL, cutlist = NULL;
dword filecount = 0, copycount = 0, cutcount = 0;
#ifdef ENABLE_BG
bool repaintbg = true;
#endif
bool imgreading = false, locreading = false;
bool prx_loaded = false;
int locaval[10];
t_fonts fonts[5], bookfonts[21];
static int fontcount = 0, fontindex = 0, bookfontcount = 0, bookfontindex = 0, ttfsize = 0;
int offset = 0;

int freq_list[][2] = {
	{ 15,  33  },
	{ 33,  16  },
	{ 66,  33  },
	{ 111, 55  },
	{ 166, 83  },
	{ 222, 111 },
	{ 266, 133 },
	{ 300, 150 },
	{ 333, 166 }
};

extern bool use_prx_power_save;
extern bool img_needrf, img_needrp, img_needrc;

static int config_num = 0;

static void load_fontsize_to_config()
{
	if(!config.usettf) {
		config.fontsize = fonts[fontindex].size;
		config.bookfontsize = bookfonts[bookfontindex].size;
	}
	else {
		config.fontsize = fonts[fontindex].size;
		config.bookfontsize = ttfsize;
	}
}

extern dword GetBGColorByTime(void);

t_win_menu_op exit_confirm()
{
	if(win_msgbox(getmsgbyid(EXIT_PROMPT), getmsgbyid(YES), getmsgbyid(NO),
			   	COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
	{
		scene_exit();
		return win_menu_op_continue;
	}
	return win_menu_op_force_redraw;
}

bool scene_load_font()
{
	char fontzipfile[256], efontfile[256], cfontfile[256];
	if(fontindex >= fontcount)
		fontindex = 0;
	config.fontsize = fonts[fontindex].size;
	STRCPY_S(fontzipfile, appdir);
	STRCAT_S(fontzipfile, "fonts.zip");
	SPRINTF_S(efontfile, "ASC%d", config.fontsize);
	SPRINTF_S(cfontfile, "GBK%d", config.fontsize);
	scene_power_save(false);
	if(!disp_load_zipped_font(fontzipfile, efontfile, cfontfile))
	{
		SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, config.fontsize);
		SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, config.fontsize);
		if(!disp_load_font(efontfile, cfontfile)) {
			scene_power_save(true);
			return false;
		}
	}
	disp_set_fontsize(config.fontsize);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	if(config.lyricex > (155 / (DISP_FONTSIZE + 1) - 1) / 2)
		config.lyricex = (155 / (DISP_FONTSIZE + 1) - 1) / 2;
#else
	config.lyricex = 0;
#endif
	scene_power_save(true);
	return true;
}

bool scene_load_book_font()
{
	char fontzipfile[256], efontfile[256], cfontfile[256];
	if(bookfontindex >= bookfontcount)
		bookfontindex = 0;
	if(config.usettf)
		config.bookfontsize = ttfsize;
	else
	{
		config.bookfontsize = bookfonts[bookfontindex].size;
		if(config.bookfontsize == config.fontsize)
		{
			disp_assign_book_font();
			disp_set_book_fontsize(config.bookfontsize);
			memset(disp_ewidth, config.bookfontsize / 2, 0x80);
			return true;
		}
	}
	bool loaded = false;
#ifdef ENABLE_TTF
	if(config.usettf)
	{
		scene_power_save(false);
		STRCPY_S(fontzipfile, appdir);
		STRCAT_S(fontzipfile, "fonts.zip");
		loaded = disp_load_zipped_truetype_book_font(fontzipfile, "ASC.TTF", "GBK.TTF", config.bookfontsize);
		if(!loaded)
		{
			SPRINTF_S(efontfile, "%sfonts/ASC.TTF", appdir);
			SPRINTF_S(cfontfile, "%sfonts/GBK.TTF", appdir);
			loaded = disp_load_truetype_book_font(efontfile, cfontfile, config.bookfontsize);
		}
		scene_power_save(imgreading || fs != NULL);
	}
#endif
	if(!loaded)
	{
		config.usettf = false;
		STRCPY_S(fontzipfile, appdir);
		STRCAT_S(fontzipfile, "fonts.zip");
		SPRINTF_S(efontfile, "ASC%d", config.bookfontsize);
		SPRINTF_S(cfontfile, "GBK%d", config.bookfontsize);
		scene_power_save(false);
		if(!disp_load_zipped_book_font(fontzipfile, efontfile, cfontfile))
		{
			SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, config.bookfontsize);
			SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, config.bookfontsize);
			if(!disp_load_book_font(efontfile, cfontfile)) {
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

t_win_menu_op scene_txtkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox(getmsgbyid(EXIT_PROMPT), getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		disp_duptocache();
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)getmsgbyid(BUTTON_PRESS_PROMPT));
		disp_flip();
		dword key, key2;
		SceCtrlData ctl;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
			key = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
		} while((key & ~(PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT))== 0);
		key2 = key;
		while((key2 & key) == key)
		{
			key = key2;
			sceCtrlReadBufferPositive(&ctl, 1);
			key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
		}
		if(config.txtkey[*index] == key || config.txtkey2[*index] == key)
			return win_menu_op_force_redraw;
		int i;
		for(i = 0; i < 12; i ++)
		{
			if(i == *index)
				continue;
			if(config.txtkey[i] == key)
			{
				config.txtkey[i] = config.txtkey2[*index];
				if(config.txtkey[i] == 0)
				{
					config.txtkey[i] = config.txtkey2[i];
					config.txtkey2[i] = 0;
				}
				break;
			}
			if(config.txtkey2[i] == key)
			{
				config.txtkey2[i] = config.txtkey2[*index];
				break;
			}
		}
		config.txtkey2[*index] = config.txtkey[*index];
		config.txtkey[*index] = key;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
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

void scene_txtkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 145 + 7 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 144 + 7 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(BUTTON_SETTING_PROMPT));
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.txtkey[i], keyname);
		if(config.txtkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.txtkey2[i], keyname2);
			STRCAT_S(keyname, getmsgbyid(BUTTON_SETTING_SEP));
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

dword scene_txtkey(dword * selidx)
{
	t_win_menuitem item[13];
	STRCPY_S(item[0].name, getmsgbyid(BOOKMARK_MENU_KEY));
	STRCPY_S(item[1].name, getmsgbyid(PREV_PAGE_KEY));
	STRCPY_S(item[2].name, getmsgbyid(NEXT_PAGE_KEY));
	STRCPY_S(item[3].name, getmsgbyid(PREV_100_LINE_KEY));
	STRCPY_S(item[4].name, getmsgbyid(NEXT_100_LINE_KEY));
	STRCPY_S(item[5].name, getmsgbyid(PREV_500_LINE_KEY));
	STRCPY_S(item[6].name, getmsgbyid(NEXT_500_LINE_KEY));
	STRCPY_S(item[7].name, getmsgbyid(TO_START_KEY));
	STRCPY_S(item[8].name, getmsgbyid(TO_END_KEY));
	STRCPY_S(item[9].name, getmsgbyid(PREV_FILE));
	STRCPY_S(item[10].name, getmsgbyid(NEXT_FILE));
	STRCPY_S(item[11].name, getmsgbyid(EXIT_KEY));
	STRCPY_S(item[12].name, getmsgbyid(SWITCH_PAGE));
	dword i;
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_txtkey_predraw, NULL, scene_txtkey_menucb)) != INVALID);
	return 0;
}

t_win_menu_op scene_flkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_CIRCLE:
		disp_duptocache();
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)getmsgbyid(BUTTON_PRESS_PROMPT));
		disp_flip();
		dword key, key2;
		SceCtrlData ctl;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
			key = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
		} while((key & ~(PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT))== 0);
		key2 = key;
		while((key2 & key) == key)
		{
			key = key2;
			sceCtrlReadBufferPositive(&ctl, 1);
			key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			sceKernelDelayThread(20000);
		}
		if(config.flkey[*index] == key || config.flkey2[*index] == key)
			return win_menu_op_force_redraw;
		int i;
		for(i = 0; i < 7; i ++)
		{
			if(i == *index)
				continue;
			if(config.flkey[i] == key)
			{
				config.flkey[i] = config.flkey2[*index];
				if(config.flkey[i] == 0)
				{
					config.flkey[i] = config.flkey2[i];
					config.flkey2[i] = 0;
				}
				break;
			}
			if(config.flkey2[i] == key)
			{
				config.flkey2[i] = config.flkey2[*index];
				break;
			}
		}
		config.flkey2[*index] = config.flkey[*index];
		config.flkey[*index] = key;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
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

void scene_flkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 139 + DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 138 + DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(BUTTON_SETTING_PROMPT));
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.flkey[i], keyname);
		if(config.flkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.flkey2[i], keyname2);
			STRCAT_S(keyname, getmsgbyid(BUTTON_SETTING_SEP));
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

dword scene_flkey(dword * selidx)
{
	t_win_menuitem item[7];
	STRCPY_S(item[0].name, getmsgbyid(FL_SELECTED));
	STRCPY_S(item[1].name, getmsgbyid(FL_FIRST));
	STRCPY_S(item[2].name, getmsgbyid(FL_LAST));
	STRCPY_S(item[3].name, getmsgbyid(FL_PARENT));
	STRCPY_S(item[4].name, getmsgbyid(FL_REFRESH));
	STRCPY_S(item[5].name, getmsgbyid(FL_FILE_OPS));
	STRCPY_S(item[6].name, getmsgbyid(FL_SELECT_FILE));
	dword i;
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_flkey_predraw, NULL, scene_flkey_menucb)) != INVALID);
	return 0;
}

static const char * GetFileExt(const char * filename)
{
	const char * strExt = strrchr(filename, '.');
	if(strExt == NULL)
		return "";
	else
		return strExt;
}

int scene_filelist_compare_ext(void * data1, void * data2)
{
	const char *fn1 = ((p_win_menuitem)data1)->compname, *fn2 = ((p_win_menuitem)data2)->compname;
	int cmp = stricmp(GetFileExt(fn1), GetFileExt(fn2));
	if(cmp)
		return cmp;
	return stricmp(fn1, fn2);
}

int scene_filelist_compare_name(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 == fs_filetype_dir)
	{
		if(ft2 != fs_filetype_dir)
			return -1;
	}
	else if(ft2 == fs_filetype_dir)
		return 1;
	return stricmp(((p_win_menuitem)data1)->compname, ((p_win_menuitem)data2)->compname);
}

int scene_filelist_compare_size(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 == fs_filetype_dir)
	{
		if(ft2 != fs_filetype_dir)
			return -1;
	}
	else if(ft2 == fs_filetype_dir)
		return 1;
	return (int)((p_win_menuitem)data1)->data3 - (int)((p_win_menuitem)data2)->data3;
}

int scene_filelist_compare_ctime(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 == fs_filetype_dir)
	{
		if(ft2 != fs_filetype_dir)
			return -1;
	}
	else if(ft2 == fs_filetype_dir)
		return 1;
	return (((int)((p_win_menuitem)data1)->data2[0]) << 16) + (int)((p_win_menuitem)data1)->data2[1] - ((((int)((p_win_menuitem)data2)->data2[0]) << 16) + (int)((p_win_menuitem)data2)->data2[1]);
}

int scene_filelist_compare_mtime(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 == fs_filetype_dir)
	{
		if(ft2 != fs_filetype_dir)
			return -1;
	}
	else if(ft2 == fs_filetype_dir)
		return 1;
	return (((int)((p_win_menuitem)data1)->data2[2]) << 16) + (int)((p_win_menuitem)data1)->data2[3] - ((((int)((p_win_menuitem)data2)->data2[2]) << 16) + (int)((p_win_menuitem)data2)->data2[3]);
}

qsort_compare compare_func[] = {
	scene_filelist_compare_ext,
	scene_filelist_compare_name,
	scene_filelist_compare_size,
	scene_filelist_compare_ctime,
	scene_filelist_compare_mtime
};

#ifdef ENABLE_IMAGE
t_win_menu_op scene_ioptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
			config.bicubic = !config.bicubic;
			break;
		case 1:
			if(config.slideinterval == 1)
				config.slideinterval = 120;
			else
				config.slideinterval --;
			break;
		case 2:
			if(config.viewpos == conf_viewpos_leftup)
				config.viewpos = conf_viewpos_rightdown;
			else
				config.viewpos --;
			break;
		case 3:
			if(config.imgmvspd == 1)
				config.imgmvspd = 20;
			else
				config.imgmvspd --;
			break;
		case 4:
			if(config.imgpaging == conf_imgpaging_direct)
				config.imgpaging = conf_imgpaging_leftright;
			else
				config.imgpaging --;
			break;
		case 5:
			if(config.imgpagereserve == 0)
				config.imgpagereserve = 50;
			else
				config.imgpagereserve --;
			break;
		case 6:
			if(config.thumb == conf_thumb_none)
				config.thumb = conf_thumb_always;
			else
				config.thumb --;
			break;
		case 7:
			if(--config.imgbrightness < 0)
				config.imgbrightness = 0;
			img_needrf = img_needrc = img_needrp = true;
			break;
		case 8:
			config.img_enable_analog = !config.img_enable_analog;
			break;
		case 9:
			config.load_exif= !config.load_exif;
			img_needrp = true;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
			config.bicubic = !config.bicubic;
			break;
		case 1:
			if(config.slideinterval == 120)
				config.slideinterval = 1;
			else
				config.slideinterval ++;
			break;
		case 2:
			if(config.viewpos == conf_viewpos_rightdown)
				config.viewpos = conf_viewpos_leftup;
			else
				config.viewpos ++;
			break;
		case 3:
			if(config.imgmvspd == 20)
				config.imgmvspd = 1;
			else
				config.imgmvspd ++;
			break;
		case 4:
			if(config.imgpaging == conf_imgpaging_leftright)
				config.imgpaging = conf_imgpaging_direct;
			else
				config.imgpaging ++;
			break;
		case 5:
			if(config.imgpagereserve == 50)
				config.imgpagereserve = 0;
			else
				config.imgpagereserve ++;
			break;
		case 6:
			if(config.thumb == conf_thumb_always)
				config.thumb = conf_thumb_none;
			else
				config.thumb ++;
			break;
		case 7:
			if(++config.imgbrightness > 100)
				config.imgbrightness = 100;
			img_needrf = img_needrc = img_needrp = true;
			break;
		case 8:
			config.img_enable_analog = !config.img_enable_analog;
			break;
		case 9:
			config.load_exif= !config.load_exif;
			img_needrp = true;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_LTRIGGER:
		switch(*index)
		{
		case 1:
			if(config.slideinterval < 11)
				config.slideinterval = 1;
			else
				config.slideinterval -= 10;
			break;
		case 5:
			if(config.imgpagereserve < 10)
				config.imgpagereserve = 0;
			else
				config.imgpagereserve -= 10;
			break;
		case 7:
			config.imgbrightness -= 10;
			if(config.imgbrightness < 0)
				config.imgbrightness = 0;
			img_needrf = img_needrc = img_needrp = true;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RTRIGGER:
		switch(*index)
		{
		case 1:
			if(config.slideinterval > 110)
				config.slideinterval = 120;
			else
				config.slideinterval += 10;
			break;
		case 5:
			if(config.imgpagereserve > 40)
				config.imgpagereserve = 50;
			else
				config.imgpagereserve += 10;
			break;
		case 7:
			config.imgbrightness += 10;
			if(config.imgbrightness > 100)
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

void scene_ioptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[7];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 135 + 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(IMG_OPT));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 134 + 5 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.bicubic ? getmsgbyid(IMG_OPT_CUBE) : getmsgbyid(IMG_OPT_LINEAR)));
	memset(number, ' ', 4);
	utils_dword2string(config.slideinterval, number, 4);
	STRCAT_S(number, getmsgbyid(IMG_OPT_SECOND));
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_viewposname(config.viewpos));
	memset(number, ' ', 4);
	utils_dword2string(config.imgmvspd, number, 4);
	disp_putstring(242, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_imgpagingname(config.imgpaging));
	utils_dword2string(config.imgpagereserve, number, 4);
	disp_putstring(242, 129, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 129 + DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_thumbname(config.thumb));
	char infomsg[80];
	SPRINTF_S(infomsg, "%d%%", config.imgbrightness);
	disp_putstring(240 + DISP_FONTSIZE, 130 + 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	disp_putstring(240 + DISP_FONTSIZE, 131 + 3 * DISP_FONTSIZE, COLOR_WHITE, config.img_enable_analog ? (const byte *) getmsgbyid(YES) : (const byte*) getmsgbyid(NO));
	disp_putstring(240 + DISP_FONTSIZE, 132 + 4 * DISP_FONTSIZE, COLOR_WHITE, config.load_exif ? (const byte *) getmsgbyid(YES) : (const byte*) getmsgbyid(NO));
}

dword scene_ioptions(dword * selidx)
{
	t_win_menuitem item[10];
	dword i;
	STRCPY_S(item[0].name, getmsgbyid(IMG_OPT_ALGO));
	STRCPY_S(item[1].name, getmsgbyid(IMG_OPT_DELAY));
	STRCPY_S(item[2].name, getmsgbyid(IMG_OPT_START_POS));
	STRCPY_S(item[3].name, getmsgbyid(IMG_OPT_FLIP_SPEED));
	STRCPY_S(item[4].name, getmsgbyid(IMG_OPT_FLIP_MODE));
	STRCPY_S(item[5].name, getmsgbyid(IMG_OPT_REVERSE_WIDTH));
	STRCPY_S(item[6].name, getmsgbyid(IMG_OPT_THUMB_VIEW));
	STRCPY_S(item[7].name, getmsgbyid(IMG_OPT_BRIGHTNESS));
	STRCPY_S(item[8].name, getmsgbyid(ENABLE_ANALOG_KEY));
	STRCPY_S(item[9].name, getmsgbyid(VIEW_EXIF));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	while(win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_ioptions_predraw, NULL, scene_ioptions_menucb) != INVALID);
	return 0;
}
#endif

t_win_menu_op scene_color_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	dword r, g, b;
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
			r = RGB_R(config.forecolor); g = RGB_G(config.forecolor); b = RGB_B(config.forecolor);
			if(r == 0)
				r = COLOR_MAX;
			else
				r --;
			config.forecolor = RGB2(r, g, b);
			break;
		case 1:
			r = RGB_R(config.forecolor); g = RGB_G(config.forecolor); b = RGB_B(config.forecolor);
			if(g == 0)
				g = COLOR_MAX;
			else
				g --;
			config.forecolor = RGB2(r, g, b);
			break;
		case 2:
			r = RGB_R(config.forecolor); g = RGB_G(config.forecolor); b = RGB_B(config.forecolor);
			if(b == 0)
				b = COLOR_MAX;
			else
				b --;
			config.forecolor = RGB2(r, g, b);
			break;
		case 3:
			r = RGB_R(config.bgcolor); g = RGB_G(config.bgcolor); b = RGB_B(config.bgcolor);
			if(r == 0)
				r = COLOR_MAX;
			else
				r --;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 4:
			r = RGB_R(config.bgcolor); g = RGB_G(config.bgcolor); b = RGB_B(config.bgcolor);
			if(g == 0)
				g = COLOR_MAX;
			else
				g --;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 5:
			r = RGB_R(config.bgcolor); g = RGB_G(config.bgcolor); b = RGB_B(config.bgcolor);
			if(b == 0)
				b = COLOR_MAX;
			else
				b --;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 6:
			if(config.grayscale == 0)
				config.grayscale = 100;
			else
				config.grayscale --;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(r == COLOR_MAX)
				r = 0;
			else
				r ++;
			config.forecolor = RGB2(r, g, b);
			break;
		case 1:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(g == COLOR_MAX)
				g = 0;
			else
				g ++;
			config.forecolor = RGB2(r, g, b);
			break;
		case 2:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(b == COLOR_MAX)
				b = 0;
			else
				b ++;
			config.forecolor = RGB2(r, g, b);
			break;
		case 3:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(r == COLOR_MAX)
				r = 0;
			else
				r ++;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 4:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(g == COLOR_MAX)
				g = 0;
			else
				g ++;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 5:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(b == COLOR_MAX)
				b = 0;
			else
				b ++;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 6:
			if(config.grayscale == 100)
				config.grayscale = 0;
			else
				config.grayscale ++;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
	case PSP_CTRL_SQUARE:
		return win_menu_op_continue;
	case PSP_CTRL_LTRIGGER:
		switch(*index)
		{
		case 0:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(r > (COLOR_MAX * 25 / 255))
				r -= (COLOR_MAX * 25 / 255);
			else
				r = 0;
			config.forecolor = RGB2(r, g, b);
			break;
		case 1:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(g > (COLOR_MAX * 25 / 255))
				g -= (COLOR_MAX * 25 / 255);
			else
				g = 0;
			config.forecolor = RGB2(r, g, b);
			break;
		case 2:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(b > (COLOR_MAX * 25 / 255))
				b -= (COLOR_MAX * 25 / 255);
			else
				b = 0;
			config.forecolor = RGB2(r, g, b);
			break;
		case 3:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(r > (COLOR_MAX * 25 / 255))
				r -= (COLOR_MAX * 25 / 255);
			else
				r = 0;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 4:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(g > (COLOR_MAX * 25 / 255))
				g -= (COLOR_MAX * 25 / 255);
			else
				g = 0;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 5:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(b > (COLOR_MAX * 25 / 255))
				b -= (COLOR_MAX * 25 / 255);
			else
				b = 0;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 6:
			if(config.grayscale < 10)
				config.grayscale = 0;
			else
				config.grayscale -= 10;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RTRIGGER:
		switch(*index)
		{
		case 0:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(r < COLOR_MAX - (COLOR_MAX * 25 / 255))
				r += (COLOR_MAX * 25 / 255);
			else
				r = COLOR_MAX;
			config.forecolor = RGB2(r, g, b);
			break;
		case 1:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(g < COLOR_MAX - (COLOR_MAX * 25 / 255))
				g += (COLOR_MAX * 25 / 255);
			else
				g = COLOR_MAX;
			config.forecolor = RGB2(r, g, b);
			break;
		case 2:
			r = RGB_R(config.forecolor), g = RGB_G(config.forecolor), b = RGB_B(config.forecolor);
			if(b < COLOR_MAX - (COLOR_MAX * 25 / 255))
				b += (COLOR_MAX * 25 / 255);
			else
				b = COLOR_MAX;
			config.forecolor = RGB2(r, g, b);
			break;
		case 3:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(r < COLOR_MAX - (COLOR_MAX * 25 / 255))
				r += (COLOR_MAX * 25 / 255);
			else
				r = COLOR_MAX;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 4:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(g < COLOR_MAX - (COLOR_MAX * 25 / 255))
				g += (COLOR_MAX * 25 / 255);
			else
				g = COLOR_MAX;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 5:
			r = RGB_R(config.bgcolor), g = RGB_G(config.bgcolor), b = RGB_B(config.bgcolor);
			if(b < COLOR_MAX - (COLOR_MAX * 25 / 255))
				b += (COLOR_MAX * 25 / 255);
			else
				b = COLOR_MAX;
			config.bgcolor = RGB2(r, g, b);
			break;
		case 6:
			if(config.grayscale > 90)
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

void scene_color_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	int pad = 0;
	if(config.fontsize <= 10) {
		pad = 6 * (12 - config.fontsize);
	}
	disp_rectangle(239 - DISP_FONTSIZE * 6, 120 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6 + pad, 131 + DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 121 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6 + pad, 120 - 6 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 121 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(TEXT_COLOR_OPT));
	disp_line(240 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6 + pad, 121 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6 + pad, 130 + DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_fillrect(280, 127 - 6 * DISP_FONTSIZE, 310, 157 - 6 * DISP_FONTSIZE, config.forecolor);
	disp_fillrect(280, 130 - 3 * DISP_FONTSIZE, 310, 160 - 3 * DISP_FONTSIZE, config.bgcolor);
	memset(number, ' ', 4);
	utils_dword2string(RGB_R(config.forecolor), number, 4);
	disp_putstring(242, 123 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(RGB_G(config.forecolor), number, 4);
	disp_putstring(242, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(RGB_B(config.forecolor), number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(RGB_R(config.bgcolor), number, 4);
	disp_putstring(242, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(RGB_G(config.bgcolor), number, 4);
	disp_putstring(242, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(RGB_B(config.bgcolor), number, 4);
	disp_putstring(242, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
#ifdef ENABLE_BG
	utils_dword2string(config.grayscale, number, 4);
	disp_putstring(242, 129, COLOR_WHITE, (const byte *)number);
#else
	disp_putstring(248, 129, COLOR_WHITE, (const byte *)getmsgbyid(NO_SUPPORT));
#endif
}

dword scene_color(dword * selidx)
{
	char infomsg[80];
	t_win_menuitem item[7];
	dword i;
	SPRINTF_S(infomsg, "%s：%s", getmsgbyid(FONT_COLOR), getmsgbyid(FONT_COLOR_RED));
	STRCPY_S(item[0].name, infomsg);
	SPRINTF_S(infomsg, "%10s%s", " ", getmsgbyid(FONT_COLOR_GREEN));
	STRCPY_S(item[1].name, infomsg);
	SPRINTF_S(infomsg, "%10s%s", " ", getmsgbyid(FONT_COLOR_BLUE));
	STRCPY_S(item[2].name, infomsg);
	SPRINTF_S(infomsg, "%s：%s", getmsgbyid(FONT_BACK_COLOR), getmsgbyid(FONT_COLOR_RED));
	STRCPY_S(item[3].name, infomsg);
#ifdef ENABLE_BG
	SPRINTF_S(infomsg, "(%s)  %s", getmsgbyid(FONT_COLOR_GRAY), getmsgbyid(FONT_COLOR_GREEN));
	STRCPY_S(item[4].name, infomsg);
#else
	SPRINTF_S(infomsg, "%10s%s", " ", getmsgbyid(FONT_COLOR_GREEN));
	STRCPY_S(item[4].name, infomsg);
#endif
	SPRINTF_S(infomsg, "%10s%s", " ", getmsgbyid(FONT_COLOR_BLUE));
	STRCPY_S(item[5].name, infomsg);
	STRCPY_S(item[6].name, getmsgbyid(FONT_BACK_GRAY));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
#ifdef ENABLE_BG
	dword orgbgcolor = config.bgcolor;
	dword orggrayscale = config.grayscale;
#endif
	while(win_menu(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_color_predraw, NULL, scene_color_menucb) != INVALID);
#ifdef ENABLE_BG
	if(orgbgcolor != config.bgcolor || orggrayscale != config.grayscale) {
		dbg_printf(d, "更换背景: %s gray: %d", config.bgfile, config.grayscale);
		bg_load(config.bgfile, config.bgcolor, fs_file_get_type(config.bgfile), config.grayscale);
	}
#endif
	return 0;
}

t_win_menu_op scene_boptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
			if(config.wordspace == 0)
				config.wordspace = 5;
			else {
				config.wordspace --;
				cur_book_view.text_needrf = true;
			}
			break;
		case 1:
			if(config.rowspace == 0)
				config.rowspace = 20;
			else
				config.rowspace --;
			break;
		case 2:
			if(config.borderspace == 0)
				config.borderspace = 10;
			else
				config.borderspace --;
			break;
		case 3:
			if(config.infobar == conf_infobar_none)
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
				config.infobar = conf_infobar_lyric;
#else
				config.infobar = conf_infobar_info;
#endif
			else
				config.infobar --;
			break;
		case 4:
			config.rlastrow = !config.rlastrow;
			break;
		case 5:
			if(config.vertread == 0)
				config.vertread = 3;
			else
				config.vertread --;
			break;
		case 6:
			if(config.encode == 0)
				config.encode = 4;
			else
				config.encode --;
			break;
		case 7:
			config.scrollbar = !config.scrollbar;
			break;
		case 8:
			config.autobm = !config.autobm;
			break;
		case 9:
			config.reordertxt = !config.reordertxt;
			break;
		case 10:
			config.pagetonext = !config.pagetonext;
			break;
		case 11:
			if(--config.autopagetype < 0)
				config.autopagetype = 2;
			config.prev_autopage = config.autopagetype;
			break;
		case 12:
			if(--config.autopage < -1000)
				config.autopage = -1000;
			break;
		case 13:
			if(--config.autolinedelay < 0)
				config.autolinedelay = 0;
			break;
		case 14:
			config.enable_analog = !config.enable_analog;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
			if(config.wordspace == 5)
				config.wordspace = 0;
			else {
				config.wordspace ++;
				cur_book_view.text_needrf = true;
			}
			break;
		case 1:
			if(config.rowspace == 20)
				config.rowspace = 0;
			else
				config.rowspace ++;
			break;
		case 2:
			if(config.borderspace == 10)
				config.borderspace = 0;
			else
				config.borderspace ++;
			break;
		case 3:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if(config.infobar == conf_infobar_lyric)
#else
			if(config.infobar == conf_infobar_info)
#endif
				config.infobar = conf_infobar_none;
			else
				config.infobar ++;
			break;
		case 4:
			config.rlastrow = !config.rlastrow;
			break;
		case 5:
			if(config.vertread == 3)
				config.vertread = 0;
			else
				config.vertread ++;
			break;
		case 6:
			if(config.encode == 4)
				config.encode = 0;
			else
				config.encode ++;
			break;
		case 7:
			config.scrollbar = !config.scrollbar;
			break;
		case 8:
			config.autobm = !config.autobm;
			break;
		case 9:
			config.reordertxt = !config.reordertxt;
			break;
		case 10:
			config.pagetonext = !config.pagetonext;
			break;
		case 11:
			config.autopagetype = (config.autopagetype + 1) % 3;
			config.prev_autopage = config.autopagetype;
			break;
		case 12:
			if(++config.autopage > 1000)
				config.autopage = 1000;
			break;
		case 13:
			if(++config.autolinedelay > 1000)
				config.autolinedelay = 1000;
			break;
		case 14:
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

void scene_boptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 140 + 9 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(READ_OPTION));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 139 + 9 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	memset(number, ' ', 4);
	utils_dword2string(config.wordspace, number, 4);
	disp_putstring(242, 124 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.rowspace, number, 4);
	disp_putstring(242, 125 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.borderspace, number, 4);
	disp_putstring(242, 126 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(242 + DISP_FONTSIZE, 127 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_infobarname(config.infobar));
	disp_putstring(242 + DISP_FONTSIZE, 128 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.rlastrow ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 129 - 1 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)((config.vertread == 3) ? getmsgbyid(REVERSAL_DIRECT) : (config.vertread == 2) ? getmsgbyid(LEFT_DIRECT) : (config.vertread == 1 ? getmsgbyid(RIGHT_DIRECT) : getmsgbyid(NONE_DIRECT))));
	disp_putstring(242 + DISP_FONTSIZE, 130, COLOR_WHITE, (const byte *)conf_get_encodename(config.encode));
	disp_putstring(242 + DISP_FONTSIZE, 131 + 1 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.scrollbar ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 132 + 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.autobm ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 133 + 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.reordertxt ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 134 + 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.pagetonext ? getmsgbyid(NEXT_ARTICLE) : getmsgbyid(NO_ACTION)));
	if(config.autopagetype == 2) {
		disp_putstring(242 + DISP_FONTSIZE, 135 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"无");
	}
	else if(config.autopagetype == 1) {
		disp_putstring(242 + DISP_FONTSIZE, 135 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"自动滚屏");
	}
	else {
		disp_putstring(242 + DISP_FONTSIZE, 135 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"自动翻页");
	}
	if(config.autopage) {
		char infomsg[80];
		SPRINTF_S(infomsg, "%2d", config.autopage);
		disp_putstring(242 + DISP_FONTSIZE, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	}
	else {
		disp_putstring(242 + DISP_FONTSIZE, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(HAVE_SHUTDOWN));
	}
	if(config.autolinedelay) {
		memset(number, ' ', 4);
		utils_dword2string(config.autolinedelay, number, 2);
		disp_putstring(242 + DISP_FONTSIZE, 137 + 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	}
	else {
		disp_putstring(242 + DISP_FONTSIZE, 137 + 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(HAVE_SHUTDOWN));
	}
	disp_putstring(242 + DISP_FONTSIZE, 138 + 8 * DISP_FONTSIZE, COLOR_WHITE, config.enable_analog ? (const byte *)getmsgbyid(YES) :  (const byte *)getmsgbyid(NO));
}

dword scene_boptions(dword * selidx)
{
	t_win_menuitem item[15];
	dword i;
	STRCPY_S(item[0].name, getmsgbyid(WORD_SPACE));
	STRCPY_S(item[1].name, getmsgbyid(LINE_SPACE));
	STRCPY_S(item[2].name, getmsgbyid(REVERSE_SPAN_SPACE));
	STRCPY_S(item[3].name, getmsgbyid(INFO_BAR_DISPLAY));
	STRCPY_S(item[4].name, getmsgbyid(REVERSE_ONE_LINE_WHEN_PAGE_DOWN));
	STRCPY_S(item[5].name, getmsgbyid(TEXT_DIRECTION));
	STRCPY_S(item[6].name, getmsgbyid(TEXT_ENCODING));
	STRCPY_S(item[7].name, getmsgbyid(SCROLLBAR));
	STRCPY_S(item[8].name, getmsgbyid(AUTOSAVE_BOOKMARK));
	STRCPY_S(item[9].name, getmsgbyid(TEXT_REALIGNMENT));
	STRCPY_S(item[10].name, getmsgbyid(TEXT_TAIL_PAGE_DOWN));
	STRCPY_S(item[11].name, getmsgbyid(AUTOPAGE_MODE));
	if(!config.autopagetype) {
		STRCPY_S(item[12].name, getmsgbyid(AUTOPAGE_DELAY));
	}
	else
		STRCPY_S(item[12].name, getmsgbyid(AUTOLINE_STEP));
	STRCPY_S(item[13].name, getmsgbyid(AUTOLINE_TIME));
	STRCPY_S(item[14].name, getmsgbyid(ENABLE_ANALOG_KEY));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	bool orgvert = config.vertread;
	dword orgrowspace = config.rowspace;
	bool orgibar = config.infobar;
	bool orgscrollbar = config.scrollbar;
	dword orgwordspace = config.wordspace;
	dword orgborderspace = config.borderspace;
	dword orgreordertxt = config.reordertxt;
	dword orgencode = config.encode;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 6 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_boptions_predraw, NULL, scene_boptions_menucb)) != INVALID);
	dword result = 0;
	if(orgibar != config.infobar || orgvert != config.vertread || orgrowspace != config.rowspace || orgborderspace != config.borderspace)
	{
		int t = config.vertread;
		if(t == 3)
			t = 0;
		drperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	}
	if(orgibar != config.infobar || orgvert != config.vertread || orgwordspace != config.wordspace || orgborderspace != config.borderspace || orgscrollbar != config.scrollbar)
	{
		dword orgpixelsperrow = pixelsperrow;
		int t = config.vertread;
		if(t == 3)
			t = 0;
		pixelsperrow = (t ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		if(orgpixelsperrow != pixelsperrow)
			result = 4;
	}
	if(orgreordertxt != config.reordertxt)
		result = 4;
	if(fs != NULL && fs->ucs == 0 && orgencode != config.encode)
		result = 3;
	return result;
}

t_win_menu_op scene_ctrlset_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
#ifdef ENABLE_HPRM
			config.hprmctrl = !config.hprmctrl;
#endif
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
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

void scene_ctrlset_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 126 - 4 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(OPERATION_SETTING));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 125 - 4 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
#ifdef ENABLE_HPRM
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.hprmctrl ? getmsgbyid(CONTROL_PAGE) : getmsgbyid(CONTROL_MUSIC)));
#else
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(NO_SUPPORT));
#endif
}

dword scene_ctrlset(dword * selidx)
{
	t_win_menuitem item[1];
	dword i;
	STRCPY_S(item[0].name, getmsgbyid(LINE_CTRL_MODE));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
#ifdef ENABLE_HPRM
	bool orghprmctrl = config.hprmctrl;
#endif
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_ctrlset_predraw, NULL, scene_ctrlset_menucb)) != INVALID);
#ifdef ENABLE_HPRM
	if(orghprmctrl != config.hprmctrl)
	{
		ctrl_enablehprm(config.hprmctrl);
#ifdef ENABLE_MUSIC
		mp3_set_hprm(!config.hprmctrl);
#endif
	}
#endif
	return 0;
}

t_win_menu_op scene_fontsel_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
			if(fontindex == 0)
				fontindex = fontcount - 1;
			else
				fontindex --;
			break;
		case 1:
			if(config.usettf)
			{
				ttfsize --;
				if(ttfsize < 12)
					ttfsize = 32;
			}
			else
			{
				if(bookfontindex == 0)
					bookfontindex = bookfontcount - 1;
				else
					bookfontindex --;
			}
			break;
		case 2:
#ifdef ENABLE_TTF
			config.usettf = !config.usettf;
			if(config.usettf)
				ttfsize = bookfonts[bookfontindex].size;
			else
			{
				bookfontindex = 0;
				while(bookfontindex < bookfontcount && bookfonts[bookfontindex].size <= ttfsize)
					bookfontindex ++;
				if(bookfontindex > 0)
					bookfontindex --;
			}
#endif
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
			if(fontindex == fontcount - 1)
				fontindex = 0;
			else
				fontindex ++;
			break;
		case 1:
			if(config.usettf)
			{
				ttfsize ++;
				if(ttfsize > 32)
					ttfsize = 12;
			}
			else
			{
				if(bookfontindex == bookfontcount - 1)
					bookfontindex = 0;
				else
					bookfontindex ++;
			}
			break;
		case 2:
#ifdef ENABLE_TTF
			config.usettf = !config.usettf;
			if(config.usettf)
				ttfsize = bookfonts[bookfontindex].size;
			else
			{
				bookfontindex = 0;
				while(bookfontindex < bookfontcount && bookfonts[bookfontindex].size <= ttfsize)
					bookfontindex ++;
				if(bookfontindex > 0)
					bookfontindex --;
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

void scene_fontsel_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 128 - 2 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 127 - 2 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(FONT_SETTING));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 126 - 3 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	memset(number, ' ', 4);
	utils_dword2string(fonts[fontindex].size, number, 4);
	disp_putstring(242, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.usettf ? ttfsize : bookfonts[bookfontindex].size, number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
#ifdef ENABLE_TTF
	disp_putstring(240 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.usettf ? getmsgbyid(YES) : getmsgbyid(NO)));
#else
	disp_putstring(240 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(getmsgbyid(HAVE_SHUTDOWN)));
#endif
}

dword scene_fontsel(dword * selidx)
{
	t_win_menuitem item[3];
	dword i;
	STRCPY_S(item[0].name, getmsgbyid(MENU_FONT_SIZE));
	STRCPY_S(item[1].name, getmsgbyid(BOOK_FONT_SIZE));
	STRCPY_S(item[2].name, getmsgbyid(TTF_FONT_SIZE));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	int orgttfsize = ttfsize;
	bool orgusettf = config.usettf;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_fontsel_predraw, NULL, scene_fontsel_menucb)) != INVALID);
	if(orgfontindex != fontindex || orgusettf != config.usettf || (!config.usettf && orgbookfontindex != bookfontindex) || (config.usettf && orgttfsize != ttfsize))
	{
		if(orgfontindex != fontindex)
			scene_load_font();
		scene_load_book_font();
		int t = config.vertread;
		if(t == 3)
			t = 0;
		drperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		pixelsperrow = (t ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		return 2;
	}
	return 0;
}

t_win_menu_op scene_musicopt_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
#if defined(ENABLE_MUSIC)
			config.autoplay = !config.autoplay;
#endif
			break;
		case 1:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if(config.lyricex == 0)
				config.lyricex = (155 / (DISP_FONTSIZE + 1) - 1) / 2;
			else
				config.lyricex --;
#else
			config.lyricex = 0;
#endif
			break;
		case 2:
			if(config.lyricencode)
				config.lyricencode --;
			else
				config.lyricencode = 4;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
#if defined(ENABLE_MUSIC)
			config.autoplay = !config.autoplay;
#endif
			break;
		case 1:
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if(config.lyricex >= (155 / (DISP_FONTSIZE + 1) - 1) / 2)
				config.lyricex = 0;
			else
				config.lyricex ++;
#else
			config.lyricex = 0;
#endif
			break;
		case 2:
			config.lyricencode ++;
			if(config.lyricencode > 4)
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

void scene_musicopt_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 128 - 2 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 123 - 3 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(MUSIC_SETTING));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 127 - 2 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
#ifdef ENABLE_MUSIC
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.autoplay ? getmsgbyid(YES) : getmsgbyid(NO)));
#else
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(NO_SUPPORT));
#endif
	memset(number, ' ', 4);
	utils_dword2string(config.lyricex * 2 + 1, number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(242 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_encodename(config.lyricencode));
}

dword scene_musicopt(dword * selidx)
{
	t_win_menuitem item[3];
	dword i;
	STRCPY_S(item[0].name, getmsgbyid(AUTO_START_PLAYBACK));
	STRCPY_S(item[1].name, getmsgbyid(LYRIC_DISPLAY_LINENUM));
	STRCPY_S(item[2].name, getmsgbyid(LYRIC_DISPLAY_ENCODE));
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_musicopt_predraw, NULL, scene_musicopt_menucb)) != INVALID);
	return 0;
}

t_win_menu_op scene_moptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
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
			if(config.arrange == conf_arrange_ext)
				config.arrange = conf_arrange_mtime;
			else
				config.arrange --;
			break;
		case 5:
#ifdef ENABLE_USB
			config.enableusb = !config.enableusb;
#endif
			break;
		case 6:
			if(config.autosleep > 0)
				config.autosleep--;
			break;
		case 7:
		case 8:
		case 9:
			if(config.freqs[*index-7] > 0)
				config.freqs[*index-7]--;
			if(config.freqs[*index-7] > NELEMS(freq_list) - 1)
				config.freqs[*index-7] = NELEMS(freq_list) - 1;
			scene_power_save(true);
			break;
		case 10:
			config.dis_scrsave = ! config.dis_scrsave;
			break;
		case 11:
			config.launchtype = config.launchtype == 2 ? 4 : 2;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
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
			if(config.arrange == conf_arrange_mtime)
				config.arrange = conf_arrange_ext;
			else
				config.arrange ++;
			break;
		case 5:
#ifdef ENABLE_USB
			config.enableusb = !config.enableusb;
#endif
			break;
		case 6:
			if(config.autosleep < 1000)
				config.autosleep++;
			break;
		case 7:
		case 8:
		case 9:
			if(config.freqs[*index-7] < NELEMS(freq_list) - 1)
				config.freqs[*index-7]++;
			if(config.freqs[*index-7] < 0)
				config.freqs[*index-7] = 0;
			scene_power_save(true);
			break;
		case 10:
			config.dis_scrsave = ! config.dis_scrsave;
			break;
		case 11:
			config.launchtype = config.launchtype == 2 ? 4 : 2;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_moptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 137 + 7 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 4 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(SYSTEM_OPTION));
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 136 + 7 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showhidden ? getmsgbyid(DISPLAY_OPTION) : getmsgbyid(HIDDEN_OPTION)));
	disp_putstring(242 + DISP_FONTSIZE, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showunknown ? getmsgbyid(DISPLAY_OPTION) : getmsgbyid(HIDDEN_OPTION)));
	disp_putstring(242 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showfinfo ? getmsgbyid(DISPLAY_OPTION) : getmsgbyid(HIDDEN_OPTION)));
	disp_putstring(242 + DISP_FONTSIZE, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.allowdelete ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_arrangename(config.arrange));
#ifdef ENABLE_USB
	disp_putstring(242 + DISP_FONTSIZE, 129, COLOR_WHITE, (const byte *)(config.enableusb ? getmsgbyid(YES) : getmsgbyid(NO)));
#else
	disp_putstring(242 + DISP_FONTSIZE, 129, COLOR_WHITE, (const byte *)getmsgbyid(NO_SUPPORT));
#endif
	char infomsg[80];
	if(config.autosleep == 0) {
		STRCPY_S(infomsg, getmsgbyid(HAVE_SHUTDOWN));
	}
	else {
		SPRINTF_S(infomsg, "%d分钟", config.autosleep);
	}
	disp_putstring(242 + DISP_FONTSIZE, 130 + 1 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[0]][0], freq_list[config.freqs[0]][1]);
	disp_putstring(242 + DISP_FONTSIZE, 131 + 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[1]][0], freq_list[config.freqs[1]][1]);
	disp_putstring(242 + DISP_FONTSIZE, 132 + 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	SPRINTF_S(infomsg, "%d/%d", freq_list[config.freqs[2]][0], freq_list[config.freqs[2]][1]);
	disp_putstring(242 + DISP_FONTSIZE, 133 + 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	disp_putstring(242 + DISP_FONTSIZE, 134 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.dis_scrsave ? getmsgbyid(YES) : getmsgbyid(NO)));
	disp_putstring(242 + DISP_FONTSIZE, 135 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.launchtype == 2 ? "普通程序" : "PS游戏"));
}

dword scene_moptions(dword * selidx)
{
	t_win_menuitem item[12];
	dword i;
	STRCPY_S(item[0].name, "    隐藏文件");
	STRCPY_S(item[1].name, "未知类型文件");
	STRCPY_S(item[2].name, "    文件信息");
	STRCPY_S(item[3].name, "    允许删除");
	STRCPY_S(item[4].name, "文件排序方式");
	STRCPY_S(item[5].name, "    USB 连接");
	STRCPY_S(item[6].name, getmsgbyid(AUTO_SLEEP));
	STRCPY_S(item[7].name, "设置最低频率");
	STRCPY_S(item[8].name, "设置普通频率");
	STRCPY_S(item[9].name, "设置最高频率");
	STRCPY_S(item[10].name, "禁用屏幕保护");
	STRCPY_S(item[11].name, "启动程序类型");
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	bool orgshowhidden = config.showhidden;
	bool orgshowunknown = config.showunknown;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	t_conf_arrange orgarrange = config.arrange;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_moptions_predraw, NULL, scene_moptions_menucb)) != INVALID);
#ifdef ENABLE_USB
	if(config.isreading == false) {
		if(config.enableusb)
			usb_activate();
		else
			usb_deactivate();
	}
#endif
	if(orgfontindex != fontindex || orgbookfontindex != bookfontindex)
	{
		scene_load_font();
		scene_load_book_font();
		int t = config.vertread;
		if(t == 3)
			t = 0;
		drperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		pixelsperrow = (t ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		return 2;
	}
	if(orgshowhidden != config.showhidden || orgshowunknown != config.showunknown || orgarrange != config.arrange)
		return 1;
	return 0;
}

t_win_menu_op scene_locsave_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_CIRCLE:
		if(*index < 10)
		{
			if(location_set(*index, config.path, config.shortpath, filelist[(dword)item[1].data].compname, config.isreading))
			{
				dbg_printf(d, "location_set: %s %s %s %d",
						config.path, config.shortpath, filelist[(dword)item[1].data].compname, config.isreading);
				locaval[*index] = true;
				STRCPY_S(item[*index].name, config.path);
				if(config.path[strlen(config.path) - 1] != '/' && filelist[(dword)item[1].data].compname[0] != '/')
					STRCAT_S(item[*index].name, "/");
				STRCAT_S(item[*index].name, filelist[(dword)item[1].data].compname);
				if(strlen(item[*index].name) > 36)
				{
					item[*index].name[36] = item[*index].name[37] = item[*index].name[38] = '.';
					item[*index].name[39] = 0;
				}
				if(config.isreading) {
					STRCAT_S(item[*index].name, "*");
				}
				item[*index].width = strlen(item[*index].name);
			}
//			win_msg("保存成功！", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
			return win_menu_op_force_redraw;
		}
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_locsave_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(237 - DISP_FONTSIZE * 10, 122 - 5 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 10, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(238 - DISP_FONTSIZE * 10, 123 - 5 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 122 - 4 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(238 - DISP_FONTSIZE * 3, 123 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"保存文件位置");
	disp_line(238 - DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, COLOR_WHITE);
}

void scene_loc_enum(dword index, char * comppath, char * shortpath, char * compname, bool isreading, void * data)
{
	dbg_printf(d, "scene_loc_enum: %s %s %s %d", comppath, shortpath, compname, isreading);
	p_win_menuitem item = (p_win_menuitem)data;
	if(index < 10)
	{
		STRCPY_S(item[index].name, comppath);
		if(comppath[strlen(comppath) - 1] != '/' && compname[0] != '/') {
			STRCAT_S(item[index].name, "/");
		}
		STRCAT_S(item[index].name, compname);
		if(strlen(item[index].name) > 36)
		{
			item[index].name[36] = item[index].name[37] = item[index].name[38] = '.';
			item[index].name[39] = 0;
		}
		if(isreading) {
			STRCAT_S(item[index].name, "*");
		}
		item[index].width = strlen(item[index].name);
	}
}

dword scene_locsave(dword * selidx)
{
	t_win_menuitem item[10];
	dword i;
	for(i = 0; i < NELEMS(item); i ++)
	{
		STRCPY_S(item[i].name, "未使用");
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;
	item[1].data = (void *)*selidx;
	index = win_menu(238 - DISP_FONTSIZE * 10, 124 - 4 * DISP_FONTSIZE, 40, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_locsave_predraw, NULL, scene_locsave_menucb);
	return 0;
}

t_win_menu_op scene_locload_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_CIRCLE:
		if(*index < 10)
		{
			char comppath[256] = "", shortpath[256] = "", compname[256] = "";
			if(location_get(*index, comppath, shortpath, compname, &locreading) && comppath[0] != 0 && shortpath[0] != 0 && compname[0] != 0)
			{
				dbg_printf(d, "location_get %s %s %s %d",
						comppath, shortpath, compname, locreading);
				where = scene_in_dir;
				STRCPY_S(config.path, comppath);
				STRCPY_S(config.shortpath, shortpath);
				STRCPY_S(config.lastfile, compname);
				config.isreading = locreading;
				dword plen = strlen(config.path);
				if(plen > 0 && config.path[plen - 1] == '/') {
					if(strnicmp(config.path, "ms0:/", 5) == 0)
						filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
					else
						filecount = fs_flashdir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
				}
				else
					switch(fs_file_get_type(config.path))
					{
					case fs_filetype_zip:
					{
						where = scene_in_zip;
						filecount = fs_zip_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
						break;
					}
					case fs_filetype_chm:
					{
						where = scene_in_chm;
						filecount = fs_chm_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
						break;
					}
					case fs_filetype_rar:
					{
						where = scene_in_rar;
						filecount = fs_rar_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
						break;
					}
					default:
						STRCPY_S(config.path, "ms0:/");
						STRCPY_S(config.shortpath, "ms0:/");
						filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
						break;
					}
				if(filelist == 0)
				{
					where = scene_in_dir;
					STRCPY_S(config.path, "ms0:/");
					STRCPY_S(config.shortpath, "ms0:/");
					STRCPY_S(config.lastfile, "");
					filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
				}
				quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
				dword idx = 0;
				while(idx < filecount && stricmp(filelist[idx].compname, config.lastfile) != 0)
					idx ++;
				if(idx >= filecount)
				{
					idx = 0;
					config.isreading = locreading = false;
				}
				item[0].data = (void *)true;
				item[1].data = (void *)idx;
				return win_menu_op_cancel;
			}
		}
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_locload_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(237 - DISP_FONTSIZE * 10, 122 - 5 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 10, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(238 - DISP_FONTSIZE * 10, 123 - 5 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 122 - 4 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(238 - DISP_FONTSIZE * 3, 123 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"读取文件位置");
	disp_line(238 - DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, COLOR_WHITE);
}

dword scene_locload(dword * selidx)
{
	t_win_menuitem item[10];
	dword i;
	for(i = 0; i < NELEMS(item); i ++)
	{
		STRCPY_S(item[i].name, "未使用");
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;
	item[0].data = (void *)false;
	item[1].data = (void *)*selidx;
	index = win_menu(238 - DISP_FONTSIZE * 10, 124 - 4 * DISP_FONTSIZE, 40, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_locload_predraw, NULL, scene_locload_menucb);
	*selidx = (dword)item[1].data;
	return (bool)item[0].data;
}

void scene_setting_mgr_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 128 - 2 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"设置管理");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 127 - 2 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	char infomsg[80];
	SPRINTF_S(infomsg, "%d号设置", config_num);
	char conffile[256], appdir[256];
	getcwd(appdir, 256);
	STRCAT_S(appdir, "/");
	STRCPY_S(conffile, appdir);
	char conffilename[80];
	SPRINTF_S(conffilename, "%s%d%s", "xreader", config_num, ".conf");
	STRCAT_S(conffile, conffilename);
	if(utils_is_file_exists(conffile)) {
		STRCAT_S(infomsg, "*");
	}
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	disp_putstring(242 + DISP_FONTSIZE, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
	disp_putstring(242 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)infomsg);
}

int detect_config_change(const p_conf prev, const p_conf curr)
{
#ifdef ENABLE_USB
	if(!prev->enableusb && curr->enableusb)
		usb_activate();
	else if(prev->enableusb && !curr->enableusb)
		usb_deactivate();
#endif
	if(prev->brightness != curr->brightness)
	{
		if(prx_loaded) {
			dbg_printf(d, "亮度设置为%d", curr->brightness);
			xrSetBrightness(curr->brightness);
		}
	}

	if(!curr->usettf) {
		int i=0;
		for(i=0; i<fontcount; ++i) {
			if(fonts[i].size == curr->fontsize)
			{
				break;
			}
		}
		if(i != fontcount) {
			fontindex = i;
			scene_load_font();
		}
		else {
			fontindex = 0;
			scene_load_font();
		}

		for(i=0; i<bookfontcount; ++i) {
			if(bookfonts[i].size == curr->bookfontsize)
			{
				break;
			}
		}
		if(i != bookfontcount) {
			bookfontindex = i;
			scene_load_book_font();
		}
		else {
			bookfontindex = 0;
			scene_load_book_font();
		}
	}
	else {
		scene_load_font();
		ttfsize = curr->bookfontsize;
		scene_load_book_font();
	}
	
	int t = curr->vertread;
	if(t == 3)
		t = 0;
	drperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - curr->borderspace * 2 + curr->rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (curr->rowspace + DISP_BOOK_FONTSIZE);
	rowsperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (curr->infobar ? DISP_BOOK_FONTSIZE : 0) - curr->borderspace * 2) / (curr->rowspace + DISP_BOOK_FONTSIZE);
	pixelsperrow = (t ? (curr->scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (curr->scrollbar ? 475 : PSP_SCREEN_WIDTH)) - curr->borderspace * 2;

#ifdef ENABLE_BG
	if(curr->bgfile && curr->bgfile[0] != '\0' && (prev->bgcolor != curr->bgcolor || prev->grayscale != curr->grayscale)) {
		dbg_printf(d, "检测到背景更换: %s %d", curr->bgfile, curr->grayscale);
		bg_load(curr->bgfile, curr->bgcolor, fs_file_get_type(curr->bgfile), curr->grayscale);
	}
#endif
	
	int i;
	for(i=0; i<3; ++i) {
		if(prev->freqs[i] != curr->freqs[i])
			break;
	}
	if(i != 3) {
		scene_power_save(true);
	}

	STRCPY_S(curr->path, prev->path);
	STRCPY_S(curr->shortpath, prev->shortpath);
	STRCPY_S(curr->lastfile, prev->lastfile);
	curr->isreading = prev->isreading;

	img_needrf = img_needrc = img_needrp = true;
	cur_book_view.text_needrf = cur_book_view.text_needrp = cur_book_view.text_needrb = true;

	return 0;
}

t_win_menu_op scene_setting_mgr_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_LEFT:
		if(--config_num < 0) {
			config_num = 9;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		if(++config_num > 9) {
			config_num = 0;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		{
			disp_waitv();
			char conffile[256], appdir[256];
			getcwd(appdir, 256);
			STRCAT_S(appdir, "/");
			STRCPY_S(conffile, appdir);
			char conffilename[80];
			SPRINTF_S(conffilename, "%s%d%s", "xreader", config_num, ".conf");
			STRCAT_S(conffile, conffilename);
			conf_set_file(conffile);
			if(*index == 0) {
				t_conf prev_config;
				memcpy(&prev_config, &config, sizeof(t_conf));
				// load
				if(!conf_load(&config))
				{
					win_msg("读取设置失败!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
					return win_menu_op_redraw;
				}
				detect_config_change(&prev_config, &config);
			}
			else if(*index == 1) {
				// save
				load_fontsize_to_config();
				if(!conf_save(&config))
				{
					win_msg("保存设置失败!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
					return win_menu_op_redraw;
				}
			}
			else {
				// delete
				utils_del_file(conffile);
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
	t_win_menuitem item[3];
	STRCPY_S(item[0].name, "读取设置");
	STRCPY_S(item[1].name, "保存设置");
	STRCPY_S(item[2].name, "删除设置");
	dword i;
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;
	item[0].data = (void *)false;
	item[1].data = (void *)*selidx;
	index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_setting_mgr_predraw, NULL, scene_setting_mgr_menucb);

	if(index == INVALID)
		return win_menu_op_continue;
	return win_menu_op_ok;
}

typedef dword (*t_scene_option_func)(dword * selidx);

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

t_win_menu_op scene_options_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_CIRCLE:
		if(*index == 13)
			return exit_confirm();
		if(*index == 14) {
			pixel * saveimage = (pixel *)memalign(16, PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
			if(saveimage)
				disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
			scene_readbook_raw("调试信息", (const unsigned char*)dbg_memory_buffer->ptr, dbg_memory_buffer->used, fs_filetype_txt);
			ctrl_waitrelease();

			if(saveimage)
			{
				disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
				disp_flip();
				disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
				free((void *)saveimage);
			}

			return win_menu_op_cancel;
		}
		if(scene_option_func[*index] != NULL)
		{
			item[0].data = (void *)scene_option_func[*index](item[1].data);
			if(item[0].data != win_menu_op_continue)
				return win_menu_op_cancel;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SELECT:
		ctrl_waitreleaseintime(10000);
		return win_menu_op_cancel;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_options_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
#ifdef _DEBUG
	disp_rectangle(237 - DISP_FONTSIZE * 3, 120 - 7 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 3, 139 + 9 * DISP_FONTSIZE, COLOR_WHITE);
#else
	disp_rectangle(237 - DISP_FONTSIZE * 3, 120 - 7 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 3, 138 + 8 * DISP_FONTSIZE, COLOR_WHITE);
#endif
	disp_fillrect(238 - DISP_FONTSIZE * 3, 121 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 3, 120 - 6 * DISP_FONTSIZE, config.usedyncolor ? GetBGColorByTime() : config.menubcolor);
	disp_putstring(238 - DISP_FONTSIZE * 2, 121 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)getmsgbyid(SETTING_OPTION));
	disp_line(238 - DISP_FONTSIZE * 3, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 3, 121 - 6 * DISP_FONTSIZE, COLOR_WHITE);
}

dword scene_options(dword * selidx)
{
#ifdef _DEBUG
	t_win_menuitem item[15];
#else
	t_win_menuitem item[14];
#endif
	dword i;
	STRCPY_S(item[0].name, "  字体设置");
	STRCPY_S(item[1].name, "  颜色设置");
	STRCPY_S(item[2].name, "  系统选项");
	STRCPY_S(item[3].name, "  系统按键");
	STRCPY_S(item[4].name, "  阅读选项");
	STRCPY_S(item[5].name, "  阅读按键");
#ifdef ENABLE_IMAGE
	STRCPY_S(item[6].name, "  看图选项");
	STRCPY_S(item[7].name, "  看图按键");
#else
	STRCPY_S(item[6].name, "*看图已关闭*");
	STRCPY_S(item[7].name, "*看图已关闭*");
#endif
	STRCPY_S(item[8].name, "  操作设置");
#ifdef ENABLE_MUSIC
	STRCPY_S(item[9].name, "  音乐播放");
#else
	STRCPY_S(item[9].name, "*音乐已关闭*");
#endif
	STRCPY_S(item[10].name, "保存文件位置");
	STRCPY_S(item[11].name, "读取文件位置");
	STRCPY_S(item[12].name, "  设置管理");
	STRCPY_S(item[13].name, "  退出软件");
#ifdef _DEBUG
	STRCPY_S(item[14].name, "查看调试信息");
#endif
	for(i = 0; i < NELEMS(item); i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	item[0].data = (void *)0;
	item[1].data = (void *)selidx;
	dword index = 0;
	while((index = win_menu(238 - DISP_FONTSIZE * 3, 122 - 6 * DISP_FONTSIZE, 12, NELEMS(item), item, NELEMS(item), index, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_options_predraw, NULL, scene_options_menucb)) != INVALID);
	return (dword)item[0].data;
}

t_win_menu_op scene_bookmark_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		return exit_confirm();
	case PSP_CTRL_SELECT:
		bookmark_delete(bm);
		memset(&bm->row[0], 0xFF, 10 * sizeof(dword));
		win_msg("已删除书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		return win_menu_op_cancel;
	case PSP_CTRL_START:
		if(win_msgbox("是否要导出书签？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
		{
			char bmfn[256];
			if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar || where == scene_in_gz)
			{
				STRCPY_S(bmfn, config.shortpath);
				STRCAT_S(bmfn, fs->filename);
			}
			else
				STRCPY_S(bmfn, fs->filename);
			STRCAT_S(bmfn, ".ebm");
			bookmark_export(bm, bmfn);
			win_msg("已导出书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SQUARE:
		STRCPY_S(item[*index].name, "       ");
		bm->row[(*index) + 1] = *(dword *)item[0].data;
		utils_dword2string(bm->row[(*index) + 1] / 2, item[*index].name, 7);
		bookmark_save(bm);
		return win_menu_op_redraw;
	case PSP_CTRL_TRIANGLE:
		bm->row[(*index) + 1] = INVALID;
		STRCPY_S(item[*index].name, "  NONE ");
		bookmark_save(bm);
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		if(bm->row[(*index) + 1] != INVALID)
		{
			*(dword *)item[0].data = bm->row[(*index) + 1];
			item[1].data = (void *)true;
			return win_menu_op_ok;
		}
		else
			return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_bookmark_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(63, 60 - DISP_FONTSIZE, 416, 64 + (1 + DISP_FONTSIZE) * 10, COLOR_WHITE);
	disp_fillrect(64, 61 - DISP_FONTSIZE, 415, 60, config.titlecolor);
	disp_putstring(75, 61 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"书签      ○读取  ×取消  □保存  △删除");
	disp_fillrect(68 + 7 * DISP_FONTSIZE / 2, 62, 415, 63 + (1 + DISP_FONTSIZE) * 10, config.titlecolor);
	disp_line(64, 61, 415, 61, COLOR_WHITE);
	disp_line(64, 64 + (1 + DISP_FONTSIZE) * 9, 415, 64 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE);
	disp_fillrect(64, 65 + (1 + DISP_FONTSIZE) * 9, 67 + 7 * DISP_FONTSIZE / 2, 63 + (1 + DISP_FONTSIZE) * 10, config.titlecolor);
	disp_line(67 + 7 * DISP_FONTSIZE / 2, 62, 67 + 7 * DISP_FONTSIZE / 2, 63 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE);
	++ index;
	disp_putstring(64, 65 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE, (const byte *)"SELECT 删除全部书签    START 导出书签");
	if(bm->row[index] < fs->size && fs_file_get_type(fs->filename) != fs_filetype_unknown)
	{
		t_text preview;
		memset(&preview, 0, sizeof(t_text));
		preview.buf = fs->buf + min(fs->size, bm->row[index]);
		if(fs->buf + fs->size - preview.buf < 8 * ((347 - 7 * DISP_FONTSIZE / 2) / (DISP_FONTSIZE / 2)))
			preview.size = fs->buf + fs->size - preview.buf;
		else
			preview.size = 8 * ((347 - 7 * DISP_FONTSIZE / 2) / (DISP_FONTSIZE / 2));
		byte bp[0x80];
		memcpy(bp, disp_ewidth, 0x80);
		memset(disp_ewidth, DISP_FONTSIZE / 2, 0x80);
		text_format(&preview, 347 - 7 * DISP_FONTSIZE / 2, config.wordspace);
		memcpy(disp_ewidth, bp, 0x80);
		if(preview.rows[0] != NULL)
		{
			dword i;
			if(preview.row_count > 8)
				preview.row_count = 8;
			for(i = 0; i < preview.row_count; i ++)
				disp_putnstring(70 + 7 * DISP_FONTSIZE / 2, 66 + (2 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)preview.rows[0][i].start, preview.rows[0][i].count, 0, 0, DISP_FONTSIZE, 0);
			free((void *)preview.rows[0]);
		}
	}
}

bool scene_bookmark(dword * orgp)
{
	char archname[256];
	if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar || where == scene_in_gz)
	{
		STRCPY_S(archname, config.shortpath);
		STRCAT_S(archname, fs->filename);
	}
	else
		STRCPY_S(archname, fs->filename);
	bm = bookmark_open(archname);
	if(bm == NULL)
	{
		win_msg("无法打开书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		return 0;
	}
	dword i;
	t_win_menuitem item[9];
	for(i = 0; i < 9; i ++)
	{
		if(bm->row[i + 1] != INVALID)
		{
			STRCPY_S(item[i].name, "       ");
			utils_dword2string(bm->row[i + 1] / 2, item[i].name, 7);
		}
		else
			STRCPY_S(item[i].name, "  NONE ");
		item[i].width = 7;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.usedyncolor ? GetBGColorByTime() : config.menubcolor;
		item[i].selbcolor = config.selbcolor;
	}
	item[0].data = (void *)orgp;
	item[1].data = (void *)false;
	dword index;
	if((index = win_menu(64, 62, 7, 9, item, 9, 0, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, true, scene_bookmark_predraw, NULL, scene_bookmark_menucb)) != INVALID);
	bookmark_close(bm);
	bm = NULL;
	return (bool)item[1].data;
}

void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku, dword * kd, dword * kl, dword * kr)
{
	dword i;
	memcpy(ctlkey, config.txtkey, 13 * sizeof(dword));
	memcpy(ctlkey2, config.txtkey2, 13 * sizeof(dword));
	switch(config.vertread)
	{
	case 3:
		*ku = PSP_CTRL_DOWN;
		*kd = PSP_CTRL_UP;
		*kl = PSP_CTRL_RIGHT;
		*kr = PSP_CTRL_LEFT;
		break;
	case 2:
		for (i = 0; i < 13; i ++)
		{
			if((config.txtkey[i] & PSP_CTRL_LEFT) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_DOWN;
			if((config.txtkey[i] & PSP_CTRL_RIGHT) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_UP;
			if((config.txtkey[i] & PSP_CTRL_UP) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_UP) | PSP_CTRL_LEFT;
			if((config.txtkey[i] & PSP_CTRL_DOWN) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_RIGHT;
			if((config.txtkey2[i] & PSP_CTRL_LEFT) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_DOWN;
			if((config.txtkey2[i] & PSP_CTRL_RIGHT) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_UP;
			if((config.txtkey2[i] & PSP_CTRL_UP) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_UP) | PSP_CTRL_LEFT;
			if((config.txtkey2[i] & PSP_CTRL_DOWN) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_RIGHT;
		}
		*ku = PSP_CTRL_LEFT;
		*kd = PSP_CTRL_RIGHT;
		*kl = PSP_CTRL_DOWN;
		*kr = PSP_CTRL_UP;
		break;
	case 1:
		for (i = 0; i < 13; i ++)
		{
			if((config.txtkey[i] & PSP_CTRL_LEFT) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_UP;
			if((config.txtkey[i] & PSP_CTRL_RIGHT) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_DOWN;
			if((config.txtkey[i] & PSP_CTRL_UP) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_UP) | PSP_CTRL_RIGHT;
			if((config.txtkey[i] & PSP_CTRL_DOWN) > 0)
				ctlkey[i] = (ctlkey[i] & ~PSP_CTRL_DOWN) | PSP_CTRL_LEFT;
			if((config.txtkey2[i] & PSP_CTRL_LEFT) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_LEFT) | PSP_CTRL_UP;
			if((config.txtkey2[i] & PSP_CTRL_RIGHT) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_RIGHT) | PSP_CTRL_DOWN;
			if((config.txtkey2[i] & PSP_CTRL_UP) > 0)
				ctlkey2[i] = (ctlkey2[i] & ~PSP_CTRL_UP) | PSP_CTRL_RIGHT;
			if((config.txtkey2[i] & PSP_CTRL_DOWN) > 0)
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

t_win_menu_op scene_filelist_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	dword orgidx = * index;
	t_win_menu_op op = win_menu_op_continue;
	if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
	{
		return exit_confirm();
	}
	else if(key == config.flkey[3] || key == config.flkey2[3])
	{
		ctrl_waitrelease();
		* index = 0;
		if(item[*index].compname[0] == '.')
			return win_menu_op_ok;
		return win_menu_op_redraw;
	}
	else if(key == config.flkey[6] || key == config.flkey2[6])
	{
		if(strcmp(item[*index].compname, "..") == 0)
			return win_menu_op_continue;
		item[*index].selected = !item[*index].selected;
		if(*index < filecount - 1)
			++ *index;
		return win_menu_op_redraw;
	}
	else if(key == config.flkey[5] || key == config.flkey2[5])
	{
		int sel, selcount = 0, selidx = 0, sidx, bmcount = 0, dircount = 0, mp3count = 0;
		for(sel = 0; sel < filecount; sel ++)
			if(item[sel].selected)
			{
				selidx = sel;
				selcount ++;
				switch((t_fs_filetype)item[selidx].data)
				{
				case fs_filetype_dir:
					dircount ++;
					break;
#ifdef ENABLE_MUSIC
				case fs_filetype_mp3:
				case fs_filetype_aa3:
#ifdef ENABLE_WMA
				case fs_filetype_wma:
#endif
					mp3count ++;
					break;
#endif
				case fs_filetype_ebm:
					bmcount ++;
					break;
				default:
					break;
				}
			}
		if(selcount == 0)
			selidx = *index;
		if(selcount <= 1 && strcmp(item[selidx].compname, "..") == 0 && cutcount + copycount <= 0)
			return win_menu_op_continue;
		if(selcount == 0)
			item[selidx].selected = true;
		pixel * saveimage = (pixel *)memalign(16, PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
		if(saveimage)
			disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
		disp_duptocachealpha(50);
		disp_rectangle(240 - DISP_FONTSIZE * 3 - 1, 136 - DISP_FONTSIZE * 3 - 1, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE * 5, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 3, 240 + DISP_FONTSIZE * 3 - 1, 136 + DISP_FONTSIZE * 5 - 1, config.msgbcolor);
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 3, COLOR_WHITE, (const byte *)"△  退出操作");
		if(selcount <= 1 && strcmp(item[selidx].compname, "..") != 0)
		{
			switch((t_fs_filetype)item[selidx].data)
			{
			case fs_filetype_ebm:
				if(where == scene_in_dir)
				{
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  导入书签");
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  导入书签");
				}
				break;
			case fs_filetype_dir:
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  进入目录");
#ifdef ENABLE_MUSIC
				if(where == scene_in_dir)
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  添加音乐");
#endif
				break;
#ifdef ENABLE_PMPAVC
			case fs_filetype_pmp:
				if(where == scene_in_dir)
				{
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  继续播放");
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  从头播放");
				}
				break;
#endif
			case fs_filetype_chm:
			case fs_filetype_zip:
			case fs_filetype_rar:
				if(where == scene_in_dir)
				{
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  进入文档");
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  进入文档");
				}
				break;
#ifdef ENABLE_MUSIC
			case fs_filetype_mp3:
			case fs_filetype_aa3:
#ifdef ENABLE_WMA
			case fs_filetype_wma:
#endif
				if(where == scene_in_dir)
				{
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  直接播放");
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  添加音乐");
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
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  查看图片");
#endif
#ifdef ENABLE_BG
				if(where == scene_in_dir)
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  设为背景");
#endif
				if(config.load_exif && (t_fs_filetype)item[selidx].data == fs_filetype_jpg) {
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 + 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"SELECT EXIF");
				}
				break;
#endif
			default:
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  阅读文本");
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  阅读文本");
				break;
			}
		}
		else
		{
			if(mp3count + dircount > 0 && bmcount > 0)
			{
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  添加音乐");
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  导入书签");
			}
			else if(mp3count + dircount > 0)
			{
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  添加音乐");
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  添加音乐");
			}
			else if(bmcount > 0)
			{
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)"○  导入书签");
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"□  导入书签");
			}
		}
		if(where == scene_in_dir)
		{
			if(selcount > 1 || strcmp(item[selidx].compname, "..") != 0)
			{
				if(strnicmp(config.path, "ms0:/", 5) == 0)
				{
					if(config.allowdelete)
						disp_putstring(240 - DISP_FONTSIZE * 3, 136, COLOR_WHITE, (const byte *)"×    删除");
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)" R    剪切");
				}
				if(config.path[0] != 0)
					disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE, COLOR_WHITE, (const byte *)" L    复制");
			}
			if(strnicmp(config.path, "ms0:/", 5) == 0 && ((copycount > 0 && stricmp(copydir, config.shortpath) != 0) || (cutcount > 0 && stricmp(cutdir, config.shortpath) != 0)))
				disp_putstring(240 - DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE * 3, COLOR_WHITE, (const byte *)"START 粘贴");
		}
		disp_flip();
		bool inop = true;
		t_win_menu_op retop = win_menu_op_continue;
		while(inop)
		{
			dword key = ctrl_waitany();
			switch(key)
			{
			case PSP_CTRL_TRIANGLE:
				inop = false;
				retop = win_menu_op_continue;
				break;
			case PSP_CTRL_CROSS:
				if(!config.allowdelete || where != scene_in_dir || strnicmp(config.path, "ms0:/", 5) != 0)
					break;
				if(win_msgbox("删除所选文件？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
				{
					char fn[256];
					config.lastfile[0] = 0;
					for(sidx = 0; sidx < filecount; sidx ++) 
						if(item[sidx].selected)
						{
							STRCPY_S(fn, config.shortpath);
							STRCAT_S(fn, item[sidx].shortname);
							if((t_fs_filetype)item[sidx].data == fs_filetype_dir)
								utils_del_dir(fn);
							else
								utils_del_file(fn);
							if(config.lastfile[0] == 0)
							{
								int idx = sidx + 1;
								while(idx < filecount && item[idx].selected)
									idx ++;
								if(idx < filecount)
									STRCPY_S(config.lastfile, item[idx].compname);
								else if(sidx > 0)
									STRCPY_S(config.lastfile, item[idx - 1].compname);
							}
						}
					retop = win_menu_op_cancel;
				}
				inop = false;
				break;
			case PSP_CTRL_SQUARE:
				if(selcount <= 1)
				{
					sidx = selidx;
					switch((t_fs_filetype)item[sidx].data)
					{
#ifdef ENABLE_MUSIC
					case fs_filetype_dir:
						if(win_msgbox("添加目录内所有歌曲到播放列表？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
						{
							char cfn[256];
							SPRINTF_S(cfn, "%s%s/", config.path, item[sidx].compname);
							mp3_list_add_dir(cfn);
							win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
						}
						break;
					case fs_filetype_mp3:
					case fs_filetype_aa3:
#ifdef ENABLE_WMA
					case fs_filetype_wma:
#endif
						if(win_msgbox("添加歌曲到播放列表？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
						{
							char mp3name[256], mp3longname[256];
							STRCPY_S(mp3name, config.shortpath);
							STRCAT_S(mp3name, filelist[sidx].shortname);
							STRCPY_S(mp3longname, config.path);
							STRCAT_S(mp3longname, filelist[sidx].compname);
							mp3_list_add(mp3name, mp3longname);
							win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
						}
						break;
#endif
#ifdef ENABLE_PMPAVC
					case fs_filetype_pmp:
						if(where == scene_in_dir)
						{
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
							char bgfile[256];
							STRCPY_S(bgfile, config.shortpath);
							STRCAT_S(bgfile, item[*index].shortname);
							if(stricmp(bgfile, config.bgfile) == 0)
							{
								if(win_msgbox("是否取消背景图？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
								{
									dbg_printf(d, "取消背景图");
									STRCPY_S(config.bgfile, " ");
									bg_cancel();
									disp_fillvram(0);
									disp_flip();
									disp_fillvram(0);
								}
							}
							else
							{
								if(win_msgbox("是否将当前图片文件设为背景图？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
								{
									STRCPY_S(config.bgfile, bgfile);
									dbg_printf(d, "设置背景图: %s %d", config.bgfile, config.grayscale);
									bg_load(config.bgfile, config.bgcolor, (t_fs_filetype)item[*index].data, config.grayscale);
									repaintbg = true;
								}
							}
						}
						break;
#endif
					default:
						retop = win_menu_op_ok;
						break;
					}
				}
				else if(mp3count + dircount == 0 && bmcount == 0)
					break;
				else
				{
					if(where != scene_in_dir)
						break;
					if(bmcount > 0)
					{
						if(!win_msgbox("是否要导入书签？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
							break;
					}
					else
					{
						if(!win_msgbox("添加歌曲到播放列表？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
							break;
					}
					for(sidx = 0; sidx < filecount; sidx ++) if(item[sidx].selected)
					{
						switch((t_fs_filetype)item[sidx].data)
						{
#ifdef ENABLE_MUSIC
						case fs_filetype_dir:
							if(bmcount == 0)
							{
								char cfn[256];
								SPRINTF_S(cfn, "%s%s/", config.path, item[sidx].compname);
								mp3_list_add_dir(cfn);
							}
							break;
#endif
						case fs_filetype_ebm:
							if(bmcount > 0)
							{
								char bmfn[256];
								STRCPY_S(bmfn, config.path);
								STRCAT_S(bmfn, item[sidx].shortname);
								bookmark_import(bmfn);
							}
							break;
#ifdef ENABLE_MUSIC
						case fs_filetype_mp3:
						case fs_filetype_aa3:
#ifdef ENABLE_WMA
						case fs_filetype_wma:
#endif
							if(bmcount == 0)
							{
								char mp3name[256], mp3longname[256];
								STRCPY_S(mp3name, config.shortpath);
								STRCAT_S(mp3name, filelist[sidx].shortname);
								STRCPY_S(mp3longname, config.path);
								STRCAT_S(mp3longname, filelist[sidx].compname);
								mp3_list_add(mp3name, mp3longname);
							}
							break;
#endif
						default:
							break;
						}
					}
					if(mp3count + dircount == 0 && bmcount > 0)
						win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
#ifdef ENABLE_MUSIC
					else if(mp3count + dircount > 0)
						win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
#endif
				}
				inop = false;
				break;
			case PSP_CTRL_CIRCLE:
				if(selcount <= 1)
					retop = win_menu_op_ok;
				else if(mp3count + dircount == 0 && bmcount == 0)
					break;
				else
				{
					if(where != scene_in_dir)
						break;
					if(mp3count + dircount == 0)
					{
						if(!win_msgbox("是否要导入书签？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
							break;
					}
#ifdef ENABLE_MUSIC
					else
					{
						if(!win_msgbox("添加歌曲到播放列表？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
							break;
					}
#endif
					for(sidx = 0; sidx < filecount; sidx ++) if(item[sidx].selected)
					{
						switch((t_fs_filetype)item[sidx].data)
						{
#ifdef ENABLE_MUSIC
						case fs_filetype_dir:
							if(dircount > 0)
							{
								char cfn[256];
								SPRINTF_S(cfn, "%s%s/", config.path, item[sidx].compname);
								mp3_list_add_dir(cfn);
							}
							break;
#endif
						case fs_filetype_ebm:
							if(mp3count + dircount == 0)
							{
								char bmfn[256];
								STRCPY_S(bmfn, config.path);
								STRCAT_S(bmfn, item[sidx].shortname);
								bookmark_import(bmfn);
							}
							break;
#ifdef ENABLE_MUSIC
						case fs_filetype_mp3:
						case fs_filetype_aa3:
#ifdef ENABLE_WMA
						case fs_filetype_wma:
#endif
							if(mp3count > 0)
							{
								char mp3name[256], mp3longname[256];
								STRCPY_S(mp3name, config.shortpath);
								STRCAT_S(mp3name, filelist[sidx].shortname);
								STRCPY_S(mp3longname, config.path);
								STRCAT_S(mp3longname, filelist[sidx].compname);
								mp3_list_add(mp3name, mp3longname);
							}
							break;
#endif
						default:
							break;
						}
					}
					if(mp3count + dircount == 0 && bmcount > 0)
						win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
#ifdef ENABLE_MUSIC
					else if(mp3count + dircount > 0)
						win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
#endif
				}
				inop = false;
				break;
			case PSP_CTRL_LTRIGGER:
				if(where != scene_in_dir)
					break;
				if(copycount > 0 && copylist != NULL)
				{
					copycount = 0;
					free((void *)copylist);
					copylist = NULL;
				}
				if(cutcount > 0 && cutlist != NULL)
				{
					cutcount = 0;
					free((void *)cutlist);
					cutlist = NULL;
				}
				STRCPY_S(copydir, config.shortpath);
				copylist = (p_win_menuitem)malloc((selcount > 0 ? selcount : 1) * sizeof(t_win_menuitem));
				if(selcount < 1)
				{
					copycount = 1;
					memcpy(&copylist[0], &filelist[selidx], sizeof(t_win_menuitem));
				}
				else
				{
					copycount = selcount;
					sidx = 0;
					for(selidx = 0; selidx < filecount; selidx ++) if(item[selidx].selected)
						memcpy(&copylist[sidx ++], &filelist[selidx], sizeof(t_win_menuitem));
				}
				inop = false;
				break;
			case PSP_CTRL_RTRIGGER:
				if(where != scene_in_dir || strnicmp(config.path, "ms0:/", 5) != 0)
					break;
				if(copycount > 0 && copylist != NULL)
				{
					copycount = 0;
					free((void *)copylist);
					copylist = NULL;
				}
				if(cutcount > 0 && cutlist != NULL)
				{
					cutcount = 0;
					free((void *)cutlist);
					cutlist = NULL;
				}
				STRCPY_S(cutdir, config.shortpath);
				cutlist = (p_win_menuitem)malloc((selcount > 0 ? selcount : 1) * sizeof(t_win_menuitem));
				if(selcount < 1)
				{
					cutcount = 1;
					memcpy(&cutlist[0], &filelist[selidx], sizeof(t_win_menuitem));
				}
				else
				{
					cutcount = selcount;
					sidx = 0;
					for(selidx = 0; selidx < filecount; selidx ++) if(item[selidx].selected)
						memcpy(&cutlist[sidx ++], &filelist[selidx], sizeof(t_win_menuitem));
				}
				inop = false;
				break;
			case PSP_CTRL_START:
				if(where != scene_in_dir || strnicmp(config.path, "ms0:/", 5) != 0)
					break;
				if(copycount > 0)
				{
					for(sidx = 0; sidx < copycount; sidx ++)
					{
						char copysrc[260], copydest[260];
						STRCPY_S(copysrc, copydir);
						STRCAT_S(copysrc, copylist[sidx].shortname);
						STRCPY_S(copydest, config.shortpath);
						STRCAT_S(copydest, copylist[sidx].shortname);
						if((t_fs_filetype)copylist[sidx].data == fs_filetype_dir)
							copy_dir(copysrc, copydest, NULL, NULL, NULL);
						else
							copy_file(copysrc, copydest, NULL, NULL, NULL);
					}
					STRCPY_S(config.lastfile, item[*index].compname);
					retop = win_menu_op_cancel;
					inop = false;
				}
				else if(cutcount > 0)
				{
					for(sidx = 0; sidx < cutcount; sidx ++)
					{
						char cutsrc[260], cutdest[260];
						STRCPY_S(cutsrc, cutdir);
						STRCAT_S(cutsrc, cutlist[sidx].shortname);
						STRCPY_S(cutdest, config.shortpath);
						STRCAT_S(cutdest, cutlist[sidx].shortname);
						if((t_fs_filetype)cutlist[sidx].data == fs_filetype_dir)
							move_dir(cutsrc, cutdest, NULL, NULL, NULL);
						else
							move_file(cutsrc, cutdest, NULL, NULL, NULL);
					}
					STRCPY_S(config.lastfile, item[*index].compname);
					retop = win_menu_op_cancel;
					inop = false;
					cutcount = 0;
					free((void *)cutlist);
					cutlist = NULL;
				}
				break;
			case PSP_CTRL_SELECT:
				{
					if(((t_fs_filetype)filelist[selidx].data) != fs_filetype_jpg)
						break;
					int result;
					dword width, height;
					pixel* imgdata = NULL;
					pixel bgcolor = 0;
					char filename[256];
					
					if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
						STRCPY_S(filename, filelist[selidx].compname);
					else
					{
						STRCPY_S(filename, config.shortpath);
						STRCAT_S(filename, filelist[selidx].shortname);
					}

					switch(where)
					{
						case scene_in_zip:
							result = exif_readjpg_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
							break;
						case scene_in_chm:
							result = exif_readjpg_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
							break;
						case scene_in_rar:
							result = exif_readjpg_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
							break;
						default:
							{
								result = exif_readjpg(filename, &width, &height, &imgdata, &bgcolor);
							}
							break;
					}

					free((void *)imgdata);
					
					inop = false;
					if(exif_array && exif_array->used) {
						char infotitle[256];
						if(strrchr(filename, '/') != NULL) {
							SPRINTF_S(infotitle, "%s的EXIF信息", strrchr(filename, '/')+1);
						}
						else
							SPRINTF_S(infotitle, "%s的EXIF信息", filename);
						infotitle[255] = '\0';
						buffer *b = buffer_init();
						int i;
						for(i=0; i<exif_array->used-1; ++i) {
							buffer_append_string(b, exif_array->ptr[i]->ptr);
							buffer_append_string(b, "\r\n");
						}
						buffer_append_string(b, exif_array->ptr[i]->ptr);

						scene_readbook_raw(infotitle, (const unsigned char*)b->ptr, b->used, fs_filetype_txt);
						buffer_free(b);
					}
					else 
						win_msg("无EXIF信息", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
				}
				break;
			}
		}
		if(selcount == 0)
			item[selidx].selected = false;
		if(saveimage)
		{
			disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
			disp_flip();
			disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
			free((void *)saveimage);
		}
		return retop;
	}
	else if(key == config.flkey[4] || key == config.flkey2[4])
		return win_menu_op_cancel;
	else if(key == PSP_CTRL_SELECT)
	{
		{
			switch(scene_options(index))
			{
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
	}
	else if(key == PSP_CTRL_START)
	{
		scene_mp3bar();
		return win_menu_op_force_redraw;
	}
	else if(key == PSP_CTRL_UP || key == CTRL_BACK)
	{
		if(*index == 0)
			*index = *count - 1;
		else
			(*index) --;
		op = win_menu_op_redraw;
	}
	else if(key == PSP_CTRL_DOWN || key == CTRL_FORWARD)
	{
		if(*index == *count - 1)
			*index = 0;
		else
			(*index) ++;
		op = win_menu_op_redraw;
	}
	else if(key == PSP_CTRL_LEFT)
	{
		if(*index < max_height - 1 )
			*index = 0;
		else
			*index -= max_height - 1;
		op = win_menu_op_redraw;
	}
	else if(key == PSP_CTRL_RIGHT)
	{
		if(*index + (max_height - 1) >= *count)
			*index = *count - 1;
		else
			*index += max_height - 1;
		op = win_menu_op_redraw;
	}
	else if(key == config.flkey[1] || key == config.flkey2[1])
	{
		*index = 0;
		op = win_menu_op_redraw;
	}
	else if(key == config.flkey[2] || key == config.flkey2[2])
	{
		*index = *count - 1;
		op = win_menu_op_redraw;
	}
	else if(key == config.flkey[0] || key == config.flkey2[0] || key == CTRL_PLAYPAUSE)
	{
		ctrl_waitrelease();
		op = win_menu_op_ok;
	}
	if(((t_fs_filetype)item[*index].data == fs_filetype_dir && (t_fs_filetype)item[orgidx].data != fs_filetype_dir) || (*index != orgidx && *index >= *topindex && *index < *topindex + HRR * 2 && ((orgidx - *topindex < HRR && *index - *topindex >= HRR) || (orgidx - *topindex >= HRR && *index - *topindex < HRR))))
		return win_menu_op_force_redraw;
	return op;
}

void scene_filelist_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
#ifdef ENABLE_BG
	if(repaintbg)
	{
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
	STRCAT_S(infomsg, " 调试版");
#endif
	disp_putstring(0, 0, COLOR_WHITE, (const byte *)infomsg);
	disp_line(0, DISP_FONTSIZE, 479, DISP_FONTSIZE, COLOR_WHITE);
	disp_rectangle(239 - WRR * DISP_FONTSIZE, 138 - (HRR + 1) * (DISP_FONTSIZE + 1), 243 + WRR * DISP_FONTSIZE, 141 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
	disp_fillrect(240 - WRR * DISP_FONTSIZE, 139 - (HRR + 1) * (DISP_FONTSIZE + 1), 242 + WRR * DISP_FONTSIZE, 137 - HRR * (DISP_FONTSIZE + 1), config.titlecolor);
	disp_putnstring(240 - WRR * DISP_FONTSIZE, 139 - (HRR + 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)config.path, 40, 0, 0, DISP_FONTSIZE, 0);
	disp_line(240 - WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1), 242 + WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
	disp_line(0, 271 - DISP_FONTSIZE, 479, 271 - DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
	disp_putstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"START 音乐播放控制   SELECT 选项   选项内按□进入按键设置");
}

void scene_filelist_postdraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	STRCPY_S(config.lastfile, item[index].compname);
	if(!config.showfinfo)
		return;
	if((t_fs_filetype)item[index].data != fs_filetype_dir)
	{
		if(where == scene_in_dir)
		{
			if(index - topindex < HRR)
			{
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE, 135 + (HRR - 3) * (DISP_FONTSIZE + 1), 243 + (WRR - 2) * DISP_FONTSIZE, 136 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 3) * (DISP_FONTSIZE + 1), 242 + (WRR - 2) * DISP_FONTSIZE, 135 + HRR * (DISP_FONTSIZE + 1), RGB(0x20, 0x20, 0x20));
				char outstr[256];
				SPRINTF_S(outstr, "文件大小: %u 字节\n", (unsigned int)item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 3) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				SPRINTF_S(outstr, "创建时间: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[0] >> 9) + 1980, (item[index].data2[0] & 0x01FF) >> 5, item[index].data2[0] & 0x01F, item[index].data2[1] >> 11, (item[index].data2[1] & 0x07FF) >> 5, (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 2) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				SPRINTF_S(outstr, "最后修改: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[2] >> 9) + 1980, (item[index].data2[2] & 0x01FF) >> 5, item[index].data2[2] & 0x01F, item[index].data2[3] >> 11, (item[index].data2[3] & 0x07FF) >> 5, (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
			else
			{
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE, 141 - HRR * (DISP_FONTSIZE + 1), 243 + (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 3) * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), 242 + (WRR - 2) * DISP_FONTSIZE, 141 - (HRR - 3) * (DISP_FONTSIZE + 1), RGB(0x20, 0x20, 0x20));
				char outstr[256];
				SPRINTF_S(outstr, "文件大小: %u 字节\n", (unsigned int)item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				SPRINTF_S(outstr, "创建时间: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[0] >> 9) + 1980, (item[index].data2[0] & 0x01FF) >> 5, item[index].data2[0] & 0x01F, item[index].data2[1] >> 11, (item[index].data2[1] & 0x07FF) >> 5, (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				SPRINTF_S(outstr, "最后修改: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[2] >> 9) + 1980, (item[index].data2[2] & 0x01FF) >> 5, item[index].data2[2] & 0x01F, item[index].data2[3] >> 11, (item[index].data2[3] & 0x07FF) >> 5, (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 2) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
		}
		else
		{
			char outstr[256];
			SPRINTF_S(outstr, "文件大小: %s 字节\n", item[index].shortname);
			if(index - topindex < HRR)
			{
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE, 135 + (HRR - 1) * (DISP_FONTSIZE + 1), 243 + (WRR - 2) * DISP_FONTSIZE, 136 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 1) * (DISP_FONTSIZE + 1), 242 + (WRR - 2) * DISP_FONTSIZE, 135 + HRR * (DISP_FONTSIZE + 1), RGB(0x20, 0x20, 0x20));
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
			else
			{
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE, 141 - HRR * (DISP_FONTSIZE + 1), 243 + (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), 242 + (WRR - 2) * DISP_FONTSIZE, 141 - (HRR - 1) * (DISP_FONTSIZE + 1), RGB(0x20, 0x20, 0x20));
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
		}
	}
}

p_win_menuitem make_up_a_empty_dir(dword *filecount, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	p_win_menuitem p;
	p = (p_win_menuitem)malloc(sizeof(t_win_menuitem));
	if(p == NULL)
	{
		*filecount = 0;
		return NULL;
	}
	memset(p, 0, sizeof(t_win_menuitem));
	STRCPY_S(p->name, "<..>");
	STRCPY_S(p->compname, "..");
	p->data = (void *)fs_filetype_dir;
	p->width = 4;
	p->selected = false;
	p->icolor = icolor;
	p->selicolor = selicolor;
	p->selrcolor = selrcolor;
	p->selbcolor = selbcolor;

	*filecount = 1;
	return p;
}

char *strtoupper(char *d, const char *s)
{
    char *origd = d;

    while((*d++ = toupper(*s))) s++;

    return origd;
}

void exec_homebrew(int method, char* path)
{
	int err;
	struct SceKernelLoadExecVSHParam param;
	memset(&param, 0, sizeof(param));
	param.size = sizeof(struct SceKernelLoadExecVSHParam);
	param.args = strlen(path)+1;
	param.argp = path;
	if(method == 2)
		param.key = "game";
	else if(method == 4) {
		param.key = "psx";
	}
	else
		param.key = "updater";
	if((err = xrKernelLoadExecVSHMsX(method, path, &param)) < 0) {
		char infomsg[256];
		SPRINTF_S(infomsg, "%s: 方法%d启动错误, 返回错误代码: %08x", path, method, err);
		win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		dbg_printf(d, infomsg);
	}
	else 
		sceKernelExitDeleteThread(0);

}

void scene_filelist()
{
	dword idx = 0;
	where = scene_in_dir;
	if(strlen(config.shortpath) == 0)
	{
		STRCPY_S(config.path, "ms0:/");
		STRCPY_S(config.shortpath, "ms0:/");
	}
	dword plen = strlen(config.path);
	if(plen > 0 && config.path[plen - 1] == '/')
		filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
	else
		switch(fs_file_get_type(config.path))
		{
		case fs_filetype_zip:
		{
			where = scene_in_zip;
			filecount = fs_zip_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			break;
		}
		case fs_filetype_chm:
		{
			where = scene_in_chm;
			filecount = fs_chm_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			break;
		}
		case fs_filetype_rar:
		{
			where = scene_in_rar;
			filecount = fs_rar_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			break;
		}
		default:
			STRCPY_S(config.path, "ms0:/");
			STRCPY_S(config.shortpath, "ms0:/");
			filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
			break;
		}
	if(filecount == 0)
	{
		STRCPY_S(config.path, "ms0:/");
		STRCPY_S(config.shortpath, "ms0:/");
		filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
	}
	quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
	while(idx < filecount && stricmp(filelist[idx].compname, config.lastfile) != 0)
		idx ++;
	if(idx >= filecount)
	{
		config.isreading = false;
		idx = 0;
	}
#ifdef ENABLE_USB
	if(config.enableusb)
		usb_activate();
	else
		usb_deactivate();
#endif
	while (1)
	{
		if(!config.isreading && !locreading) {
			if(filelist == 0 || filecount == 0)
			{
				// empty directory ?
				if(where == scene_in_dir) {
					filelist = make_up_a_empty_dir(&filecount,  config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
					idx = 0;
					idx = win_menu(240 - WRR * DISP_FONTSIZE, 139 - HRR * (DISP_FONTSIZE + 1), WRR * 4, HRR * 2, filelist, filecount, idx, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, false, scene_filelist_predraw, scene_filelist_postdraw, scene_filelist_menucb);
				}
				else {
					char infomsg[80];
					SPRINTF_S(infomsg, "%s压缩包格式损坏", config.path);
					win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
					int ll;
					if((ll = strlen(config.path) - 1) >= 0)
						while(config.path[ll] == '/' && ll >= 0)
						{
							config.path[ll] = 0;
							ll --;
						}
					char * lps;
					if((lps = strrchr(config.path, '/')) != NULL)
					{
						lps ++;
						*lps = 0;
					}
					else
						config.path[0] = 0;
					where = scene_in_dir;
					// when exit to menu specify idx to INVALID
					idx = INVALID;
				}
			}
			else 
				idx = win_menu(240 - WRR * DISP_FONTSIZE, 139 - HRR * (DISP_FONTSIZE + 1), WRR * 4, HRR * 2, filelist, filecount, idx, 0, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, false, scene_filelist_predraw, scene_filelist_postdraw, scene_filelist_menucb);
		}
		else
		{
			config.isreading = false;
			locreading = false;
		}
		if(idx == INVALID)
		{
			switch(where)
			{
			case scene_in_zip:
				filecount = fs_zip_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
				break;
			case scene_in_chm:
				filecount = fs_chm_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
				break;
			case scene_in_rar:
				filecount = fs_rar_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
				break;
			default:
				filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
			}
			if(filelist == 0)
			{
				STRCPY_S(config.path, "ms0:/");
				STRCPY_S(config.shortpath, "ms0:/");
				filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
			}
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			idx = 0;
			while(idx < filecount && stricmp(filelist[idx].compname, config.lastfile) != 0)
				idx ++;
			if(idx >= filecount)
			{
				config.isreading = false;
				idx = 0;
			}
			continue;
		}
		switch((t_fs_filetype)filelist[idx].data)
		{
		case fs_filetype_prog:
			{
			   	char infomsg[80];
				SPRINTF_S(infomsg, "是否以%s方式执行该程序?", config.launchtype == 2 ? "普通程序" : "PS游戏");
				if(win_msgbox(infomsg, "是", "否", COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
				{
					char path[256], upper[256];
					STRCPY_S(path, config.path);
					strtoupper(upper, filelist[idx].compname);
					STRCAT_S(path, upper);
					exec_homebrew(config.launchtype, path);
				}
			}
			break;
		case fs_filetype_dir:
		{
			char pdir[256];
			bool isup = false;
			pdir[0] = 0;
			if(strcmp(filelist[idx].compname, "..") == 0)
			{
				if(where == scene_in_dir)
				{
					int ll;
					if((ll = strlen(config.path) - 1) >= 0)
						while(config.path[ll] == '/' && ll >= 0)
						{
							config.path[ll] = 0;
							ll --;
						}
				}
				char * lps;
				isup = true;
				if((lps = strrchr(config.path, '/')) != NULL)
				{
					lps ++;
					STRCPY_S(pdir, lps);
					*lps = 0;
				}
				else
					config.path[0] = 0;
			}
			else if(where == scene_in_dir)
			{
				STRCAT_S(config.path, filelist[idx].compname);
				STRCAT_S(config.path, "/");
			}
			if(config.path[0] == 0)
				filecount = fs_list_device(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			else if(strnicmp(config.path, "ms0:/", 5) == 0)
				filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor, config.showhidden, config.showunknown);
			else
				filecount = fs_flashdir_to_menu(config.path, config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			if(isup)
			{
				for(idx = 0; idx < filecount; idx ++)
					if(stricmp(filelist[idx].compname, pdir) == 0)
						break;
				if(idx == filecount)
					idx = 0;
			}
			else
				idx = 0;
			where = scene_in_dir;
			break;
		}
		case fs_filetype_gz:
#if 0
			if(strlen(filelist[idx].compname) >= 7) {
				int len = strlen(filelist[idx].compname) - 7;
				if(stricmp(filelist[idx].compname + len, ".tar.gz") == 0) {
					win_msg("TODO: SUPPORT Tar ", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
					break;
				}
			}
#endif
#ifdef ENABLE_USB
			usb_deactivate();
#endif
			config.isreading = true;
			idx = scene_readbook(idx);
			config.isreading = false;
#ifdef ENABLE_USB
			if(config.enableusb)
				usb_activate();
			else
				usb_deactivate();
#endif
			break;
		case fs_filetype_zip:
			where = scene_in_zip;
			STRCAT_S(config.path, filelist[idx].compname);
			STRCAT_S(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_zip_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
		case fs_filetype_chm:
			where = scene_in_chm;
			STRCAT_S(config.path, filelist[idx].compname);
			STRCAT_S(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_chm_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
		case fs_filetype_rar:
			where = scene_in_rar;
			STRCAT_S(config.path, filelist[idx].compname);
			STRCAT_S(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_rar_to_menu(config.shortpath, &filelist, config.menutextcolor, config.selicolor, config.usedyncolor ? GetBGColorByTime() : config.menubcolor, config.selbcolor);
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
#ifdef ENABLE_PMPAVC
		case fs_filetype_pmp:
		{
			char pmpname[256];
			STRCPY_S(pmpname, config.shortpath);
			STRCAT_S(pmpname, filelist[idx].shortname);
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
			if(config.enableusb)
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
			if(config.enableusb)
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
			if(config.enableusb)
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
			if(config.enableusb)
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
			if(config.enableusb)
				usb_activate();
			else
				usb_deactivate();
#endif
			break;
#endif
		case fs_filetype_ebm:
			if(win_msgbox("是否要导入书签？", getmsgbyid(YES), getmsgbyid(NO), COLOR_WHITE, COLOR_WHITE, config.msgbcolor))
			{
				char bmfn[256];
				STRCPY_S(bmfn, config.shortpath);
				STRCAT_S(bmfn, filelist[idx].shortname);
				bookmark_import(bmfn);
				win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
			}
#ifdef ENABLE_MUSIC
		case fs_filetype_mp3:
		case fs_filetype_aa3:
#ifdef ENABLE_WMA
		case fs_filetype_wma:
#endif
			{
				char fn[256], lfn[256];
				STRCPY_S(fn, config.shortpath);
				STRCAT_S(fn, filelist[idx].shortname);
				STRCPY_S(lfn, config.path);
				STRCAT_S(lfn, filelist[idx].compname);
				mp3_directplay(fn, lfn);
			}
			break;
#endif
		default:
#ifdef ENABLE_USB
			usb_deactivate();
#endif
			config.isreading = true;
			idx = scene_readbook(idx);
			config.isreading = false;
#ifdef ENABLE_USB
			if(config.enableusb)
				usb_activate();
			else
				usb_deactivate();
#endif
			break;
		}
		if(config.dis_scrsave)
			scePowerTick(0);
	}
	if(filelist != NULL)
	{
		free((void *)filelist);
		filelist = NULL;
		filecount = 0;
	}
#ifdef ENABLE_USB
	usb_deactivate();
#endif
}

typedef struct {
	char efont[6];
	char cfont[6];
	dword size;
} t_font_type;

extern double pspDiffTime(u64 *t1, u64 *t2);

#undef printf
#define printf pspDebugScreenPrintf

static const bool is_log = true;
DBG *d = 0;

extern void scene_init()
{
	char logfile[256];
	getcwd(appdir, 256);
	STRCAT_S(appdir, "/");
	STRCPY_S(logfile, appdir);
	STRCAT_S(logfile, "log.txt");
	bool printDebugInfo = false;
	d = dbg_init();
#ifdef _DEBUG
	dbg_switch(d, 1);
#else
	dbg_switch(d, 0);
#endif
	dbg_open_memorylog(d);
	power_set_clock(333, 166);
	ctrl_init();
	if(ctrl_read() == PSP_CTRL_LTRIGGER) {
		printDebugInfo = true;
		dbg_switch(d, 1);
		dbg_open_psp_logfile(d, logfile);
		pspDebugScreenInit();
		dbg_open_psp(d);
	}
	dbg_printf(d, "xReader now loading...");
	u64 dbgnow, dbglasttick;
	u64 start, end;
	sceRtcGetCurrentTick(&dbglasttick);
	start = dbglasttick;
#ifdef ENABLE_USB
	if(config.enableusb)
		usb_open();
#endif
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "usb_open(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));

	sceRtcGetCurrentTick(&dbglasttick);
	char fontzipfile[256], efontfile[256], cfontfile[256], conffile[256], locconf[256], bmfile[256]
#ifdef ENABLE_MUSIC
		, mp3conf[256]
#endif
		;
#ifdef HOMEBREW_2XX
	{
		int i = 0, l = strlen(appdir);
		for(i = 0; i < l; i ++)
			if(appdir[i] == '%')
			{
				memmove(&appdir[i], &appdir[i + 1], l - i);
				i --; l --;
			}
	}
#endif
	STRCPY_S(config.path, "ms0:/");
	STRCPY_S(config.shortpath, "ms0:/");

	STRCPY_S(conffile, appdir);
	char conffilename[80];
	SPRINTF_S(conffilename, "%s%d%s", "xreader", config_num, ".conf");
	STRCAT_S(conffile, conffilename);
	conf_set_file(conffile);
	conf_load(&config);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "conf_load() etc: %.2fs", pspDiffTime(&dbgnow, &dbglasttick));

#ifdef ENABLE_BG
	if(config.bgfile[0] == 0)
	{
		STRCPY_S(config.bgfile, appdir);
		STRCAT_S(config.bgfile, "bg.png");
	}
	sceRtcGetCurrentTick(&dbglasttick);
	bg_load(config.bgfile, config.bgcolor, fs_file_get_type(config.bgfile), config.grayscale);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "bg_load(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
#endif

	STRCPY_S(fontzipfile, appdir);
	STRCAT_S(fontzipfile, "fonts.zip");
	int _fsize;
	for(_fsize = 10; _fsize <= 32; _fsize += 2)
	{
		sceRtcGetCurrentTick(&dbglasttick);
		SPRINTF_S(efontfile, "ASC%02d", _fsize);
		SPRINTF_S(cfontfile, "GBK%02d", _fsize);
		if(disp_has_zipped_font(fontzipfile, efontfile, cfontfile))
		{
			if(_fsize <= 16)
			{
				fonts[fontcount].size = _fsize;
				fonts[fontcount].zipped = true;
				if(fonts[fontcount].size == config.fontsize)
					fontindex = fontcount;
				fontcount ++;
			}
			bookfonts[bookfontcount].size = _fsize;
			bookfonts[bookfontcount].zipped = true;
			if(bookfonts[bookfontcount].size == config.bookfontsize)
				bookfontindex = bookfontcount;
			ttfsize = config.bookfontsize;
			bookfontcount ++;
		}
		else
		{
			SPRINTF_S(efontfile, "%sfonts/ASC%d", appdir, _fsize);
			SPRINTF_S(cfontfile, "%sfonts/GBK%d", appdir, _fsize);
			if(disp_has_font(efontfile, cfontfile))
			{
				if(_fsize <= 16)
				{
					fonts[fontcount].size = _fsize;
					fonts[fontcount].zipped = false;
					if(fonts[fontcount].size == config.fontsize)
						fontindex = fontcount;
					fontcount ++;
				}
				bookfonts[bookfontcount].size = _fsize;
				bookfonts[bookfontcount].zipped = true;
				if(bookfonts[bookfontcount].size == config.bookfontsize)
					bookfontindex = bookfontcount;
				ttfsize = config.bookfontsize;
				bookfontcount ++;
			}
		}
		sceRtcGetCurrentTick(&dbgnow);
		dbg_printf(d, "has_font(%d): %.2f second", _fsize, pspDiffTime(&dbgnow, &dbglasttick));
	}
	sceRtcGetCurrentTick(&dbglasttick);
	if(fontcount == 0 || !scene_load_font() || !scene_load_book_font())
	{
		pspDebugScreenInit();
		pspDebugScreenPrintf("Error loading font file! Press any buttun for exit!");
		ctrl_waitany();
		sceKernelExitGame();
	}
	int t = config.vertread;
	if(t == 3)
		t = 0;
	drperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	rowsperpage = ((t ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	pixelsperrow = (t ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;

	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "scene_load_font() & scene_load_book_font(): %.2f second", pspDiffTime(&dbgnow, &dbglasttick));
	sceRtcGetCurrentTick(&dbglasttick);
#ifdef ENABLE_HPRM
	ctrl_enablehprm(config.hprmctrl);
#endif
#ifdef ENABLE_GE
	init_gu();
#endif
	fat_init();

	STRCPY_S(bmfile, appdir);
	STRCAT_S(bmfile, "bookmark.conf");
	bookmark_init(bmfile);
	STRCPY_S(locconf, appdir);
	STRCAT_S(locconf, "location.conf");
	location_init(locconf, locaval);

	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "misc_init(): %.2f second", pspDiffTime(&dbgnow, &dbglasttick));

#ifdef ENABLE_MUSIC
	sceRtcGetCurrentTick(&dbglasttick);
	mp3_init();
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "mp3_init(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));

	STRCPY_S(mp3conf, appdir);
	STRCAT_S(mp3conf, "music.lst");
	if(config.confver < 0x00090100 || !mp3_list_load(mp3conf)) {
		sceRtcGetCurrentTick(&dbglasttick);
		mp3_list_add_dir("ms0:/MUSIC/");
		sceRtcGetCurrentTick(&dbgnow);
		dbg_printf(d, "mp3_list_add_dir(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
	}
	sceRtcGetCurrentTick(&dbglasttick);
	mp3_start();
	mp3_set_encode(config.mp3encode);
#ifdef ENABLE_HPRM
	mp3_set_hprm(!config.hprmctrl);
#endif
	mp3_set_cycle(config.mp3cycle);
	if(config.autoplay)
		mp3_resume();
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "mp3_start() etc: %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
#endif

	sceRtcGetCurrentTick(&dbglasttick);
	if(sceKernelDevkitVersion() >= 0x03070100) {
		char prxfn[256];
		SPRINTF_S(prxfn, "%sxrPrx.prx", appdir);
		SceUID uid = pspSdkLoadStartModule(prxfn, PSP_MEMORY_PARTITION_KERNEL);
		if(uid >= 0) {
			if(sceKernelDevkitVersion() < 0x03080000)
				use_prx_power_save = true;
			prx_loaded = true;
		}
	}
	scene_power_save(true);
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "pspSdkLoadStartModule(): %.2fs", pspDiffTime(&dbgnow, &dbglasttick));
	
	sceRtcGetCurrentTick(&end);
	dbg_printf(d, "Load finished in %.2fs, press any key to continue", pspDiffTime(&end, &start));
	if(printDebugInfo) {
		dbg_close_handle(d, 1);
		while(ctrl_read() == PSP_CTRL_LTRIGGER) {
			sceKernelDelayThread(100000);
		}
		ctrl_waitany();
	}

	disp_init();
	disp_fillvram(0);
	disp_flip();
	disp_fillvram(0);

	if(prx_loaded) {
		dbg_printf(d, "亮度设置为%d", config.brightness);
		xrSetBrightness(config.brightness);
	}

	dword c = GetBGColorByTime();
	dbg_printf(d, "GetBGColorByTime() return %d/%d/%d", RGB_R(c), RGB_G(c), RGB_B(c));

	scene_filelist();
}

extern void scene_exit()
{
	/*
	const char* log = dbg_get_memorylog();
	if(log) {
		SceUID fd = sceIoOpen("ms0:/log.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
		if(fd >= 0) {
			sceIoWrite(fd, log, strlen(log));
		}
	}
	*/
	dbg_close(d);
	power_set_clock(222, 111);
	if(bm != NULL)
	{
		bookmark_close(bm);
		bm = NULL;
	}
	if(fs != NULL)
	{
		char archname[256];
		if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
		{
			STRCPY_S(archname, config.shortpath);
			if(archname[strlen(archname) - 1] != '/' && fs->filename[0] != '/')
				STRCAT_S(archname, "/");
			STRCAT_S(archname, fs->filename);
		}
		else
			STRCPY_S(archname, fs->filename);
		if(config.autobm) {
			bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
		}
	}
	// always save to xreader0.conf
	char conffile[256], appdir[256];
	getcwd(appdir, 256);
	STRCAT_S(appdir, "/");
	STRCPY_S(conffile, appdir);
	char conffilename[80];
	SPRINTF_S(conffilename, "%s%d%s", "xreader", 0, ".conf");
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
	char mp3conf[256];
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
	ctrl_destroy();
#ifdef ENABLE_GE
	sceGuTerm();
#endif
	sceKernelExitGame();
}

extern void scene_power_save(bool save)
{
	if(save
#ifdef ENABLE_MUSIC
		&& mp3_paused()
#endif
		)
		power_set_clock(freq_list[config.freqs[0]][0], freq_list[config.freqs[0]][1]);
	else if(imgreading 
#ifdef ENABLE_MUSIC
	&&	!mp3_paused()
#endif
		)
	{
		power_set_clock(freq_list[config.freqs[2]][0], freq_list[config.freqs[2]][1]);
	}
	else
		power_set_clock(freq_list[config.freqs[1]][0], freq_list[config.freqs[1]][1]);
}

extern void scene_exception()
{
//	config.savesucc = false;
}

extern const char * scene_appdir()
{
	return appdir;
}

dword ColorLerp(dword color1,dword color2,float fWeight)
{
	/*
	byte r1, g1, b1;
	byte r2, g2, b2;
	r1 = RGB_R(color1);
	g1 = RGB_G(color1);
	b1 = RGB_B(color1);
	r2 = RGB_R(color2);
	g2 = RGB_G(color2);
	b2 = RGB_B(color2);

	dword result = RGB(
			255-(255-r1)*fWeight-(255-r2)*(1-fWeight), 
			255-(255-g1)*fWeight-(255-g2)*(1-fWeight), 
			255-(255-b1)*fWeight-(255-b2)*(1-fWeight)
			);

	return result;
	*/
	fWeight = 1.0 - fWeight;
	if(fWeight<=0.01f) return color1;
	else if(fWeight>=1) return color2; 
	else
	{ 
		byte Weight=(byte)(fWeight*255);
		byte IWeight=~Weight;
		dword dwTemp=0;
		dwTemp =(((0xFF00FF00&color1)>>8)*IWeight+ 
				((0xFF00FF00&color2)>>8)*Weight
				)&0xFF00FF00;

		dwTemp|=(((0x00FF00FF&color1)*IWeight+
					(0x00FF00FF&color2)*Weight
				 )&0xFF00FF00)>>8; 
		return dwTemp;
	}
}

static dword pspBGColorTable[12] = { 
	RGB(182, 182, 182),
	RGB(232, 195,  44),
	RGB(141, 213,  15),
	RGB(241, 141, 167),
	RGB( 10, 184,   9),
	RGB(174, 115, 233),
	RGB(  3, 204, 194),
	RGB( 11, 103, 186),
	RGB(180,  66, 190),
	RGB(236, 175,  22),
	RGB(133,  91,  31),
	RGB(251,  26,  22)
};

static int s_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define IsLeapYear(year)  (((year) % 4 == 0 && (year) % 100) || (year) % 400 == 0)

int  GetDaysOfMonth(int y, int m) {
	return s_month[m - 1] + (m == 2) * IsLeapYear(y);
}

/* time in 24 hours */
int GetDiffSecondOfTime(int srcHour, int srcMinute, int srcSeconds, 
					   	  int dstHour, int dstMinute, int dstSeconds)
{
	return (dstHour * 60 * 60 + dstMinute * 60 + dstSeconds - (srcHour * 60 * 60 + srcMinute * 60 + srcSeconds));
}

dword GetBGColorByTime(void)
{
	pspTime tm;
	sceRtcGetCurrentClockLocalTime(&tm);

	int cur_month = tm.month;
	int next_month = tm.month == 12 ? 1 : tm.month + 1;
	dword origColor;

	origColor = ColorLerp(pspBGColorTable[cur_month-1], pspBGColorTable[next_month-1], 1.0 * tm.day / GetDaysOfMonth(tm.year, cur_month));

	if(tm.hour >= 12) {
//		dbg_printf(d, "c1: %f", (1.0 * GetDiffSecondOfTime(tm.hour, tm.minutes, 24, 0) / (12.0 * 60)));
		origColor = ColorLerp(origColor, RGB(0, 0, 0), 1.0 * GetDiffSecondOfTime(tm.hour, tm.minutes, tm.seconds, 24, 0, 0) / (12 * 60 * 60));
	}
	else {
//		dbg_printf(d, "c2: %f", (1.0 * GetDiffSecondOfTime(0, 0, tm.hour, tm.minutes) / (12.0 * 60.0)));
		origColor = ColorLerp(origColor, RGB(0, 0, 0), 1.0 * GetDiffSecondOfTime(0, 0, 0, tm.hour, tm.minutes, tm.seconds) / (12 * 60 * 60));
	}

	return origColor;
}
