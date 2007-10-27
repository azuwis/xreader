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
#include "scene.h"
#include "pspscreen.h"

#ifdef ENABLE_PMPAVC
bool pmp_restart = false;
#endif
static char appdir[256], copydir[256], cutdir[256];
static dword drperpage, rowsperpage, pixelsperrow;
static p_bookmark bm = NULL;
static p_text fs = NULL;
static t_conf config;
static p_win_menuitem filelist = NULL, copylist = NULL, cutlist = NULL;
static dword filecount = 0, copycount = 0, cutcount = 0;
#ifdef ENABLE_BG
static bool repaintbg = true;
#endif
static bool imgreading = false, locreading = false;
static int locaval[10];
static enum {
	scene_in_dir,
	scene_in_zip,
	scene_in_chm,
	scene_in_rar
} where;

typedef struct {
	int size;
	bool zipped;
} t_fonts;

static t_fonts fonts[5], bookfonts[21];
static int fontcount = 0, fontindex = 0, bookfontcount = 0, bookfontindex = 0, ttfsize = 0;
static int offset = 0;

static bool scene_load_font()
{
	char fontzipfile[256], efontfile[256], cfontfile[256];
	if(fontindex >= fontcount)
		fontindex = 0;
	config.fontsize = fonts[fontindex].size;
	strcpy(fontzipfile, appdir);
	strcat(fontzipfile, "fonts.zip");
	sprintf(efontfile, "ASC%d", config.fontsize);
	sprintf(cfontfile, "GBK%d", config.fontsize);
	if(!disp_load_zipped_font(fontzipfile, efontfile, cfontfile))
	{
		sprintf(efontfile, "%sfonts/ASC%d", appdir, config.fontsize);
		sprintf(cfontfile, "%sfonts/GBK%d", appdir, config.fontsize);
		if(!disp_load_font(efontfile, cfontfile))
			return false;
	}
	disp_set_fontsize(config.fontsize);
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	if(config.lyricex > (155 / (DISP_FONTSIZE + 1) - 1) / 2)
		config.lyricex = (155 / (DISP_FONTSIZE + 1) - 1) / 2;
#else
	config.lyricex = 0;
#endif
	return true;
}

static bool scene_load_book_font()
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
		strcpy(fontzipfile, appdir);
		strcat(fontzipfile, "fonts.zip");
		loaded = disp_load_zipped_truetype_book_font(fontzipfile, "ASC.TTF", "GBK.TTF", config.bookfontsize);
		if(!loaded)
		{
			sprintf(efontfile, "%sfonts/ASC.TTF", appdir);
			sprintf(cfontfile, "%sfonts/GBK.TTF", appdir);
			loaded = disp_load_truetype_book_font(efontfile, cfontfile, config.bookfontsize);
		}
		scene_power_save(imgreading || fs != NULL);
	}
#endif
	if(!loaded)
	{
		config.usettf = false;
		strcpy(fontzipfile, appdir);
		strcat(fontzipfile, "fonts.zip");
		sprintf(efontfile, "ASC%d", config.bookfontsize);
		sprintf(cfontfile, "GBK%d", config.bookfontsize);
		if(!disp_load_zipped_book_font(fontzipfile, efontfile, cfontfile))
		{
			sprintf(efontfile, "%sfonts/ASC%d", appdir, config.bookfontsize);
			sprintf(cfontfile, "%sfonts/GBK%d", appdir, config.bookfontsize);
			if(!disp_load_book_font(efontfile, cfontfile))
				return false;
		}
		memset(disp_ewidth, config.bookfontsize / 2, 0x80);
	}
	disp_set_book_fontsize(config.bookfontsize);
	return true;
}

#ifdef ENABLE_MUSIC
static t_win_menu_op scene_mp3_list_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SQUARE:
		if(*index < mp3_list_count() - 1)
		{
			t_win_menuitem sitem;
			dword mp3i = *index;
			mp3_list_movedown(mp3i);
			memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
			memcpy(&item[mp3i], &item[mp3i + 1], sizeof(t_win_menuitem));
			memcpy(&item[mp3i + 1], &sitem, sizeof(t_win_menuitem));
			(* index) ++;
			return win_menu_op_force_redraw;
		}
		return win_menu_op_continue;
	case PSP_CTRL_TRIANGLE:
		if(*index > 0)
		{
			t_win_menuitem sitem;
			dword mp3i = *index;
			mp3_list_moveup(mp3i);
			memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
			memcpy(&item[mp3i], &item[mp3i - 1], sizeof(t_win_menuitem));
			memcpy(&item[mp3i - 1], &sitem, sizeof(t_win_menuitem));
			(* index) --;
			return win_menu_op_force_redraw;
		}
		return win_menu_op_continue;
	case PSP_CTRL_CIRCLE:
		if(win_msgbox("从歌曲列表移除文件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			mp3_list_delete(*index);
			if(*index < *count - 1)
				memmove(&item[*index], &item[*index + 1], (*count - *index - 1) * sizeof(t_win_menuitem));
			if(*index == *count - 1)
				(* index) --;
			(* count) --;
		}
		return win_menu_op_ok;
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	}
	dword orgidx = * index;
	t_win_menu_op op = win_menu_defcb(key, item, count, max_height, topindex, index);
	if(*index != orgidx && ((orgidx - *topindex < 6 && *index - *topindex >= 6) || (orgidx - *topindex >= 6 && *index - *topindex < 6)))
		return win_menu_op_force_redraw;
	return op;
}

static void scene_mp3_list_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - 10 * DISP_FONTSIZE, 128 - 7 * DISP_FONTSIZE, 243 + 10 * DISP_FONTSIZE, 145 + 7 * DISP_FONTSIZE, COLOR_WHITE);
	disp_line(240 - 10 * DISP_FONTSIZE, 129 - 6 * DISP_FONTSIZE, 242 + 10 * DISP_FONTSIZE, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_line(240 - 10 * DISP_FONTSIZE, 144 + 6 * DISP_FONTSIZE, 242 + 10 * DISP_FONTSIZE, 144 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - 10 * DISP_FONTSIZE, 129 - 7 * DISP_FONTSIZE, 242 + 10 * DISP_FONTSIZE, 128 - 6 * DISP_FONTSIZE, RGB(0x20, 0x40, 0x30));
	disp_fillrect(240 - 10 * DISP_FONTSIZE, 145 + 6 * DISP_FONTSIZE, 242 + 10 * DISP_FONTSIZE, 144 + 7 * DISP_FONTSIZE, RGB(0x20, 0x40, 0x30));
	disp_putstring(240 - 10 * DISP_FONTSIZE, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)" ○删除  □下移  △上移  ×退出");
	disp_putstring(240 - 10 * DISP_FONTSIZE, 145 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"要添加乐曲请到文件列表选取音乐文件按○");
}

static void scene_mp3_list_postdraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char outstr[256];
	const char * fname = mp3_list_get(index);
	if(strlen(fname) <= 36)
		strcpy(outstr, fname);
	else
	{
		strncpy(outstr, fname, 33);
		outstr[33] = 0;
		strcat(outstr, "...");
	}
	if(index - topindex < 6)
	{
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 140 + 5 * DISP_FONTSIZE, 243 + 9 * DISP_FONTSIZE, 141 + 6 * DISP_FONTSIZE, COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE, 242 + 9 * DISP_FONTSIZE, 140 + 6 * DISP_FONTSIZE, RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)outstr);
	}
	else
	{
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 133 - 6 * DISP_FONTSIZE, 243 + 9 * DISP_FONTSIZE, 134 - 5 * DISP_FONTSIZE, COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE, 242 + 9 * DISP_FONTSIZE, 133 - 5 * DISP_FONTSIZE, RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)outstr);
	}
}

static void scene_mp3_list()
{
	p_win_menuitem item = (p_win_menuitem)calloc(mp3_list_count(), sizeof(t_win_menuitem));
	if(item == NULL)
		return;
	dword i;
	for(i = 0; i < mp3_list_count(); i ++)
	{
		char * rname = strrchr(mp3_list_get(i), '/');
		if(rname == NULL)
			rname = (char *)mp3_list_get(i);
		else
			rname ++;
		if(strlen(rname) <= 40)
			strcpy(item[i].name, rname);
		else
		{
			strncpy(item[i].name, rname, 37);
			item[i].name[37] = 0;
			strcat(item[i].name, "...");
		}
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x40, 0x40, 0x28);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
	}
	dword index = 0;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 40, 12, item, mp3_list_count(), index, 0, RGB(0x40, 0x40, 0x28), true, scene_mp3_list_predraw, scene_mp3_list_postdraw, scene_mp3_list_menucb)) != INVALID)
		if(mp3_list_count() == 0)
			break;
	free((void *)item);
}

static void scene_mp3bar()
{
	bool firstdup = true;
	pixel * saveimage = (pixel *)malloc(PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
	if(saveimage != NULL)
		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
	while(1)
	{
		if(firstdup)
		{
			disp_duptocachealpha(50);
			firstdup = false;
		}
		else
			disp_duptocache();
		disp_rectangle(5, 5, 474, 8 + DISP_FONTSIZE * 2, COLOR_WHITE);
		disp_fillrect(6, 6, 473, 7 + DISP_FONTSIZE * 2, RGB(0x18, 0x28, 0x50));
		char timestr[80];
		pspTime tm;
		sceRtcGetCurrentClockLocalTime(&tm);
		dword cpu, bus;
		power_get_clock(&cpu, &bus);
		sprintf(timestr, "%u年%u月%u日 %02u:%02u:%02u   CPU/BUS: %d/%d   剩余内存: %.1fMB", tm.year, tm.month, tm.day, tm.hour, tm.minutes, tm.seconds, (int)cpu, (int)bus, sceKernelTotalFreeMemSize() / 1048576.0);
		disp_putstring(6 + DISP_FONTSIZE * 2, 6, COLOR_WHITE, (const byte *)timestr);
		int percent, lifetime, tempe, volt;
		char battstr[80];
		power_get_battery(&percent, &lifetime, &tempe, &volt);
		if(percent >= 0)
			sprintf(battstr, "%s  剩余电量: %d%%(%d小时%d分钟)  电池温度: %d℃", power_get_battery_charging(), percent, lifetime / 60, lifetime % 60, tempe);
		else
			strcpy(battstr, "[电源供电]");
		disp_putstring(6 + DISP_FONTSIZE, 7 + DISP_FONTSIZE, COLOR_WHITE, (const byte *)battstr);

#ifdef ENABLE_LYRIC
		disp_rectangle(5, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex), 474, 136 + (DISP_FONTSIZE + 1) * config.lyricex, COLOR_WHITE);
		disp_fillrect(6, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) + 1, 473, 136 + (DISP_FONTSIZE + 1) * config.lyricex - 1, RGB(0x18, 0x28, 0x50));
		{
			const char * ly[config.lyricex * 2 + 1];
			dword ss[config.lyricex * 2 + 1];
			if(lyric_get_cur_lines(mp3_get_lyric(), config.lyricex, ly, ss))
			{
				int lidx;
				dword cpl = 936 / DISP_FONTSIZE;
				for(lidx = 0; lidx < config.lyricex * 2 + 1; lidx ++)
					if(ly[lidx] != NULL)
					{
						if(ss[lidx] > cpl)
							ss[lidx] = cpl;
						disp_putnstring(6 + (cpl - ss[lidx]) * DISP_FONTSIZE / 4, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) + (DISP_FONTSIZE + 1) * lidx + 1, (lidx == config.lyricex) ? COLOR_WHITE : RGB(0x7F, 0x7F, 0x7F), (const byte *)ly[lidx], ss[lidx], 0, 0, DISP_FONTSIZE, 0);
					}
			}
		}
#endif

		disp_rectangle(5, 262 - DISP_FONTSIZE * 4, 474, 267, COLOR_WHITE);
		disp_fillrect(6, 263 - DISP_FONTSIZE * 4, 473, 266, RGB(0x18, 0x28, 0x50));
		int bitrate, sample, len, tlen;
		char infostr[80];
		if(mp3_get_info(&bitrate, &sample, &len, &tlen))
			sprintf(infostr, "%s   %d kbps   %d Hz   %02d:%02d / %02d:%02d", conf_get_cyclename(config.mp3cycle), bitrate, sample, len / 60, len % 60, tlen / 60, tlen % 60);
		else
			sprintf(infostr, "%s", conf_get_cyclename(config.mp3cycle));
		disp_putstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE, (const byte *)"  SELECT 编辑列表   ←快速后退   →快速前进");
		disp_putstring(6 + DISP_FONTSIZE, 264 - DISP_FONTSIZE * 3, COLOR_WHITE, (const byte *)"○播放/暂停 ×循环 □停止 △曲名编码  L上一首  R下一首");
		disp_putstring(6 + DISP_FONTSIZE, 265 - DISP_FONTSIZE * 2, COLOR_WHITE, (const byte *)infostr);
		disp_putnstring(6 + DISP_FONTSIZE, 266 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)mp3_get_tag(), (468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0, DISP_FONTSIZE, 0);
		disp_flip();
		switch(ctrl_read())
		{
			case (PSP_CTRL_SELECT | PSP_CTRL_START):
				if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
				{
					scene_exit();
					return;
				}
				break;
			case PSP_CTRL_CIRCLE:
				if(mp3_paused())
					mp3_resume();
				else
					mp3_pause();
				break;
			case PSP_CTRL_CROSS:
				if(config.mp3cycle == conf_cycle_random)
					config.mp3cycle = conf_cycle_single;
				else
					config.mp3cycle ++;
				mp3_set_cycle(config.mp3cycle);
				break;
			case PSP_CTRL_SQUARE:
				mp3_restart();
				scene_power_save(true);
				break;
			case PSP_CTRL_TRIANGLE:
				config.mp3encode ++;
				if((dword)config.mp3encode > 3)
					config.mp3encode = 0;
				mp3_set_encode(config.mp3encode);
				break;
			case PSP_CTRL_LTRIGGER:
				mp3_prev();
				break;
			case PSP_CTRL_RTRIGGER:
				mp3_next();
				break;
			case PSP_CTRL_LEFT:
				mp3_backward();
				break;
			case PSP_CTRL_RIGHT:
				mp3_forward();
				break;
			case PSP_CTRL_SELECT:
				if(mp3_list_count() == 0)
					break;
				scene_mp3_list();
				break;
			case PSP_CTRL_START:
				if(saveimage != NULL)
				{
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					free((void *)saveimage);
				}
				else
				{
					disp_waitv();
#ifdef ENABLE_BG
					if(!bg_display())
#endif
						disp_fillvram(0);
					disp_flip();
					disp_duptocache();
				}
				return;
		}
		sceKernelDelayThread(20000);
	}
}
#endif

#ifdef ENABLE_IMAGE
static t_win_menu_op scene_imgkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)"请按对应按键");
		disp_flip();
		dword key, key2;
		SceCtrlData ctl;
		ctrl_waitrelease();
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
		if(config.imgkey[*index] == key || config.imgkey2[*index] == key)
			return win_menu_op_force_redraw;
		int i;
		for(i = 0; i < 12; i ++)
		{
			if(i == *index)
				continue;
			if(config.imgkey[i] == key)
			{
				config.imgkey[i] = config.imgkey2[*index];
				if(config.imgkey[i] == 0)
				{
					config.imgkey[i] = config.imgkey2[i];
					config.imgkey2[i] = 0;
				}
				break;
			}
			if(config.imgkey2[i] == key)
			{
				config.imgkey2[i] = config.imgkey2[*index];
				break;
			}
		}
		config.imgkey2[*index] = config.imgkey[*index];
		config.imgkey[*index] = key;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
		return win_menu_op_force_redraw;
	case PSP_CTRL_TRIANGLE:
		config.imgkey[*index] = config.imgkey2[*index];
		config.imgkey2[*index] = 0;
		return win_menu_op_redraw;
	case PSP_CTRL_SQUARE:
		return win_menu_op_cancel;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_imgkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 144 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 143 + 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"按键设置   △ 删除");
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.imgkey[i], keyname);
		if(config.imgkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.imgkey2[i], keyname2);
			strcat(keyname, " | ");
			strcat(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

static dword scene_imgkey(dword * selidx)
{
	t_win_menuitem item[12];
	strcpy(item[0].name, "上一张图");
	strcpy(item[1].name, "下一张图");
	strcpy(item[2].name, "缩放模式");
	strcpy(item[3].name, "缩小图片");
	strcpy(item[4].name, "放大图片");
	strcpy(item[5].name, "左旋90度");
	strcpy(item[6].name, "右旋90度");
	strcpy(item[7].name, "  信息栏");
	strcpy(item[8].name, "显示信息");
	strcpy(item[9].name, "退出浏览");
	strcpy(item[10].name, "缩放引擎");
	strcpy(item[11].name, "幻灯播放");
	dword i;
	for(i = 0; i < 12; i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, 12, item, 12, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_imgkey_predraw, NULL, scene_imgkey_menucb)) != INVALID);
	return 0;
}
#endif

static t_win_menu_op scene_txtkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)"请按对应按键");
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

static void scene_txtkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 144 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 143 + 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"按键设置   △ 删除");
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.txtkey[i], keyname);
		if(config.txtkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.txtkey2[i], keyname2);
			strcat(keyname, " | ");
			strcat(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

static dword scene_txtkey(dword * selidx)
{
	t_win_menuitem item[12];
	strcpy(item[0].name, "书签菜单");
	strcpy(item[1].name, "  上一页");
	strcpy(item[2].name, "  下一页");
	strcpy(item[3].name, " 前100行");
	strcpy(item[4].name, " 后100行");
	strcpy(item[5].name, " 前500行");
	strcpy(item[6].name, " 后500行");
	strcpy(item[7].name, "  第一页");
	strcpy(item[8].name, "最后一页");
	strcpy(item[9].name, "上一文件");
	strcpy(item[10].name, "下一文件");
	strcpy(item[11].name, "退出阅读");
	dword i;
	for(i = 0; i < 12; i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, 12, item, 12, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_txtkey_predraw, NULL, scene_txtkey_menucb)) != INVALID);
	return 0;
}

static t_win_menu_op scene_flkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)"请按对应按键");
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

static void scene_flkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 139 + DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 138 + DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"按键设置   △ 删除");
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.flkey[i], keyname);
		if(config.flkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.flkey2[i], keyname2);
			strcat(keyname, " | ");
			strcat(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

static dword scene_flkey(dword * selidx)
{
	t_win_menuitem item[7];
	strcpy(item[0].name, "    选定");
	strcpy(item[1].name, "  第一项");
	strcpy(item[2].name, "最后一项");
	strcpy(item[3].name, "上级目录");
	strcpy(item[4].name, "刷新列表");
	strcpy(item[5].name, "文件操作");
	strcpy(item[6].name, "选择文件");
	dword i;
	for(i = 0; i < 7; i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, 7, item, 7, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_flkey_predraw, NULL, scene_flkey_menucb)) != INVALID);
	return 0;
}

static int scene_filelist_compare_ext(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 != ft2)
		return ((int)ft1) - ((int)ft2);
	return stricmp(((p_win_menuitem)data1)->name, ((p_win_menuitem)data2)->name);
}

static int scene_filelist_compare_name(void * data1, void * data2)
{
	t_fs_filetype ft1 = (t_fs_filetype)((p_win_menuitem)data1)->data, ft2 = (t_fs_filetype)((p_win_menuitem)data2)->data;
	if(ft1 == fs_filetype_dir)
	{
		if(ft2 != fs_filetype_dir)
			return -1;
	}
	else if(ft2 == fs_filetype_dir)
		return 1;
	return stricmp(((p_win_menuitem)data1)->name, ((p_win_menuitem)data2)->name);
}

static int scene_filelist_compare_size(void * data1, void * data2)
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

static int scene_filelist_compare_ctime(void * data1, void * data2)
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

static int scene_filelist_compare_mtime(void * data1, void * data2)
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

static qsort_compare compare_func[] = {
	scene_filelist_compare_ext,
	scene_filelist_compare_name,
	scene_filelist_compare_size,
	scene_filelist_compare_ctime,
	scene_filelist_compare_mtime
};

#ifdef ENABLE_IMAGE
static t_win_menu_op scene_ioptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_ioptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[7];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 132 + 2 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"看图选项");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 131 + 2 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.bicubic ? "三次立方" : "两次线性"));
	memset(number, ' ', 4);
	utils_dword2string(config.slideinterval, number, 4);
	strcat(number, "秒");
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_viewposname(config.viewpos));
	memset(number, ' ', 4);
	utils_dword2string(config.imgmvspd, number, 4);
	disp_putstring(242, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_imgpagingname(config.imgpaging));
	utils_dword2string(config.imgpagereserve, number, 4);
	disp_putstring(242, 129, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 130 + DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_thumbname(config.thumb));
}

static dword scene_ioptions(dword * selidx)
{
	t_win_menuitem item[7];
	dword i;
	strcpy(item[0].name, "    缩放算法");
	strcpy(item[1].name, "幻灯播放间隔");
	strcpy(item[2].name, "图片起始位置");
	strcpy(item[3].name, "    卷动速度");
	strcpy(item[4].name, "    翻页模式");
	strcpy(item[5].name, "翻动保留宽度");
	strcpy(item[6].name, "  缩略图查看");
	for(i = 0; i < 7; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	while(win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 7, item, 7, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_ioptions_predraw, NULL, scene_ioptions_menucb) != INVALID);
	return 0;
}
#endif

static t_win_menu_op scene_color_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	dword r, g, b;
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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

static void scene_color_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 120 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 131 + DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 121 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 120 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 121 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"颜色选项");
	disp_line(240 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 130 + DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
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
	disp_putstring(248, 129, COLOR_WHITE, (const byte *)"不支持");
#endif
}

static dword scene_color(dword * selidx)
{
	t_win_menuitem item[7];
	dword i;
	strcpy(item[0].name, "字体颜色：红");
	strcpy(item[1].name, "          绿");
	strcpy(item[2].name, "          蓝");
	strcpy(item[3].name, "背景颜色：红");
#ifdef ENABLE_BG
	strcpy(item[4].name, "(灰度色)  绿");
#else
	strcpy(item[4].name, "          绿");
#endif
	strcpy(item[5].name, "          蓝");
	strcpy(item[6].name, "  背景图灰度");
	for(i = 0; i < 7; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
#ifdef ENABLE_BG
	dword orgbgcolor = config.bgcolor;
	dword orggrayscale = config.grayscale;
#endif
	while(win_menu(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 12, 7, item, 7, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_color_predraw, NULL, scene_color_menucb) != INVALID);
#ifdef ENABLE_BG
	if(orgbgcolor != config.bgcolor || orggrayscale != config.grayscale)
		bg_load(config.bgfile, config.bgcolor, fs_file_get_type(config.bgfile), config.grayscale);
#endif
	return 0;
}

static t_win_menu_op scene_boptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			return win_menu_op_continue;
			scene_exit();
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SELECT:
		return win_menu_op_cancel;
	case PSP_CTRL_LEFT:
		switch (* index)
		{
		case 0:
			if(config.wordspace == 0)
				config.wordspace = 5;
			else
				config.wordspace --;
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
				config.vertread = 2;
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
			if(--config.autopage<0)
				config.autopage = 0;
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_RIGHT:
		switch (* index)
		{
		case 0:
			if(config.wordspace == 5)
				config.wordspace = 0;
			else
				config.wordspace ++;
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
			if(config.vertread == 2)
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
			if(++config.autopage > 1000)
				config.autopage = 1000;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_boptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 137 + 7 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"阅读选项");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 135 + 7 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	memset(number, ' ', 4);
	utils_dword2string(config.wordspace, number, 4);
	disp_putstring(242, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.rowspace, number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.borderspace, number, 4);
	disp_putstring(242, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(242 + DISP_FONTSIZE, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_infobarname(config.infobar));
	disp_putstring(242 + DISP_FONTSIZE, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.rlastrow ? "是" : "否"));
	disp_putstring(242 + DISP_FONTSIZE, 129, COLOR_WHITE, (const byte *)((config.vertread == 2) ? "左向" : (config.vertread == 1 ? "右向" : "无")));
	disp_putstring(242 + DISP_FONTSIZE, 130 + DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_encodename(config.encode));
	disp_putstring(242 + DISP_FONTSIZE, 131 + 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.scrollbar ? "是" : "否"));
	disp_putstring(242 + DISP_FONTSIZE, 132 + 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.autobm ? "是" : "否"));
	disp_putstring(242 + DISP_FONTSIZE, 133 + 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.reordertxt ? "是" : "否"));
	disp_putstring(242 + DISP_FONTSIZE, 134 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.pagetonext ? "下篇文章" : "无动作"));
	if(config.autopage) {
		memset(number, ' ', 4);
		utils_dword2string(config.autopage, number, 4);
		disp_putstring(242 + DISP_FONTSIZE, 134 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	}
	else {
		disp_putstring(242 + DISP_FONTSIZE, 134 + 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"已关闭");
	}
}

static dword scene_boptions(dword * selidx)
{
	t_win_menuitem item[12];
	dword i;
	strcpy(item[0].name, "      字间距");
	strcpy(item[1].name, "      行间距");
	strcpy(item[2].name, "    保留边距");
	strcpy(item[3].name, "      信息栏");
	strcpy(item[4].name, "翻页保留一行");
	strcpy(item[5].name, "    文字竖看");
	strcpy(item[6].name, "    文字编码");
	strcpy(item[7].name, "      滚动条");
	strcpy(item[8].name, "自动保存书签");
	strcpy(item[9].name, "重新编排文本");
	strcpy(item[10].name, "文章末尾翻页");
	strcpy(item[11].name, "自动翻页(秒)");
	for(i = 0; i < 12; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
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
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 12, item, 12, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_boptions_predraw, NULL, scene_boptions_menucb)) != INVALID);
	dword result = 0;
	if(orgibar != config.infobar || orgvert != config.vertread || orgrowspace != config.rowspace || orgborderspace != config.borderspace)
	{
		drperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	}
	if(orgibar != config.infobar || orgvert != config.vertread || orgwordspace != config.wordspace || orgborderspace != config.borderspace || orgscrollbar != config.scrollbar)
	{
		dword orgpixelsperrow = pixelsperrow;
		pixelsperrow = (config.vertread ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		if(orgpixelsperrow != pixelsperrow)
			result = 4;
	}
	if(orgreordertxt != config.reordertxt)
		result = 4;
	if(fs != NULL && fs->ucs == 0 && orgencode != config.encode)
		result = 3;
	return result;
}

static t_win_menu_op scene_ctrlset_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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

static void scene_ctrlset_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 126 - 4 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 121 - 5 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"操作设置");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 125 - 4 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
#ifdef ENABLE_HPRM
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.hprmctrl ? "控制翻页" : "控制音乐"));
#else
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"不支持");
#endif
}

static dword scene_ctrlset(dword * selidx)
{
	t_win_menuitem item[1];
	dword i;
	strcpy(item[0].name, "    线控模式");
	for(i = 0; i < 1; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
#ifdef ENABLE_HPRM
	bool orghprmctrl = config.hprmctrl;
#endif
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 1, item, 1, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_ctrlset_predraw, NULL, scene_ctrlset_menucb)) != INVALID);
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

static t_win_menu_op scene_fontsel_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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
			break;
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_fontsel_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 128 - 2 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 127 - 2 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"字体设置");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 126 - 3 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	memset(number, ' ', 4);
	utils_dword2string(fonts[fontindex].size, number, 4);
	disp_putstring(242, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	memset(number, ' ', 4);
	utils_dword2string(config.usettf ? ttfsize : bookfonts[bookfontindex].size, number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
	disp_putstring(240 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.usettf ? "是" : "否"));
}

static dword scene_fontsel(dword * selidx)
{
	t_win_menuitem item[3];
	dword i;
	strcpy(item[0].name, "菜单字体大小");
	strcpy(item[1].name, "阅读字体大小");
	strcpy(item[2].name, " 使用TTF字体");
	for(i = 0; i < 3; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	int orgttfsize = ttfsize;
	bool orgusettf = config.usettf;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 3, item, 3, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_fontsel_predraw, NULL, scene_fontsel_menucb)) != INVALID);
	if(orgfontindex != fontindex || orgusettf != config.usettf || (!config.usettf && orgbookfontindex != bookfontindex) || (config.usettf && orgttfsize != ttfsize))
	{
		if(orgfontindex != fontindex)
			scene_load_font();
		scene_load_book_font();
		drperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		pixelsperrow = (config.vertread ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		return 2;
	}
	return 0;
}

#ifdef ENABLE_MUSIC
static t_win_menu_op scene_musicopt_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_musicopt_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char number[5];
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 127 - 3 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 4 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"选项  □按键设置");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 126 - 3 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
#ifdef ENABLE_MUSIC
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.autoplay ? "是" : "否"));
#else
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"不支持");
#endif
	memset(number, ' ', 4);
	utils_dword2string(config.lyricex * 2 + 1, number, 4);
	disp_putstring(242, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)number);
}

static dword scene_musicopt(dword * selidx)
{
	t_win_menuitem item[2];
	dword i;
	strcpy(item[0].name, "自动开始播放");
	strcpy(item[1].name, "歌词显示行数");
	for(i = 0; i < 2; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 2, item, 2, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_musicopt_predraw, NULL, scene_musicopt_menucb)) != INVALID);
	return 0;
}
#endif

static t_win_menu_op scene_moptions_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
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
		}
		return win_menu_op_redraw;
	case PSP_CTRL_CIRCLE:
		return win_menu_op_continue;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_moptions_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(239 - DISP_FONTSIZE * 6, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 6, 131 + DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 6, 122 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 4 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 2, 122 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"系统选项");
	disp_line(240 - DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 122 - 5 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(241, 123 - 5 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 6, 130 + DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(242 + DISP_FONTSIZE, 124 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showhidden ? "显示" : "隐藏"));
	disp_putstring(242 + DISP_FONTSIZE, 125 - 4 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showunknown ? "显示" : "隐藏"));
	disp_putstring(242 + DISP_FONTSIZE, 126 - 3 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.showfinfo ? "显示" : "隐藏"));
	disp_putstring(242 + DISP_FONTSIZE, 127 - 2 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)(config.allowdelete ? "是" : "否"));
	disp_putstring(242 + DISP_FONTSIZE, 128 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)conf_get_arrangename(config.arrange));
#ifdef ENABLE_USB
	disp_putstring(242 + DISP_FONTSIZE, 129, COLOR_WHITE, (const byte *)(config.enableusb ? "是" : "否"));
#else
	disp_putstring(242 + DISP_FONTSIZE, 129, COLOR_WHITE, (const byte *)"不支持");
#endif
}

static dword scene_moptions(dword * selidx)
{
	t_win_menuitem item[6];
	dword i;
	strcpy(item[0].name, "    隐藏文件");
	strcpy(item[1].name, "未知类型文件");
	strcpy(item[2].name, "    文件信息");
	strcpy(item[3].name, "    允许删除");
	strcpy(item[4].name, "文件排序方式");
	strcpy(item[5].name, "    USB 连接");
	for(i = 0; i < 6; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	bool orgshowhidden = config.showhidden;
	bool orgshowunknown = config.showunknown;
	int orgfontindex = fontindex;
	int orgbookfontindex = bookfontindex;
	t_conf_arrange orgarrange = config.arrange;
	while((index = win_menu(240 - DISP_FONTSIZE * 6, 123 - 5 * DISP_FONTSIZE, 12, 6, item, 6, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_moptions_predraw, NULL, scene_moptions_menucb)) != INVALID);
#ifdef ENABLE_USB
	if(config.enableusb)
		usb_activate();
	else
		usb_deactivate();
#endif
	if(orgfontindex != fontindex || orgbookfontindex != bookfontindex)
	{
		scene_load_font();
		scene_load_book_font();
		drperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		rowsperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
		pixelsperrow = (config.vertread ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;
		return 2;
	}
	if(orgshowhidden != config.showhidden || orgshowunknown != config.showunknown || orgarrange != config.arrange)
		return 1;
	return 0;
}

static t_win_menu_op scene_locsave_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		if(*index < 10)
		{
			if(location_set(*index, config.path, config.shortpath, filelist[(dword)item[1].data].compname, config.isreading))
			{
				locaval[*index] = true;
				strcpy(item[*index].name, config.path);
				strcat(item[*index].name, filelist[(dword)item[1].data].compname);
				if(strlen(item[*index].name) > 36)
				{
					item[*index].name[36] = item[*index].name[37] = item[*index].name[38] = '.';
					item[*index].name[39] = 0;
				}
				if(config.isreading)
					strcat(item[*index].name, "*");
				item[*index].width = strlen(item[*index].name);
			}
//			win_msg("保存成功！", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
			return win_menu_op_force_redraw;
		}
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_locsave_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(237 - DISP_FONTSIZE * 10, 122 - 5 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 10, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(238 - DISP_FONTSIZE * 10, 123 - 5 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 122 - 4 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(238 - DISP_FONTSIZE * 3, 123 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"保存文件位置");
	disp_line(238 - DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, COLOR_WHITE);
}

static void scene_loc_enum(dword index, char * comppath, char * shortpath, char * compname, bool isreading, void * data)
{
	p_win_menuitem item = (p_win_menuitem)data;
	if(index < 10)
	{
		strcpy(item[index].name, comppath);
		strcat(item[index].name, compname);
		if(strlen(item[index].name) > 36)
		{
			item[index].name[36] = item[index].name[37] = item[index].name[38] = '.';
			item[index].name[39] = 0;
		}
		if(isreading)
			strcat(item[index].name, "*");
		item[index].width = strlen(item[index].name);
	}
}

static dword scene_locsave(dword * selidx)
{
	t_win_menuitem item[10];
	dword i;
	for(i = 0; i < 10; i ++)
	{
		strcpy(item[i].name, "未使用");
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;
	item[1].data = (void *)*selidx;
	index = win_menu(238 - DISP_FONTSIZE * 10, 124 - 4 * DISP_FONTSIZE, 40, 10, item, 10, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_locsave_predraw, NULL, scene_locsave_menucb);
	return 0;
}

static t_win_menu_op scene_locload_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		if(*index < 10)
		{
			char comppath[256] = "", shortpath[256] = "", compname[256] = "";
			if(location_get(*index, comppath, shortpath, compname, &locreading) && comppath[0] != 0 && shortpath[0] != 0 && compname[0] != 0)
			{
				where = scene_in_dir;
				strcpy(config.path, comppath);
				strcpy(config.shortpath, shortpath);
				strcpy(config.lastfile, compname);
				config.isreading = locreading;
				dword plen = strlen(config.path);
				if(plen > 0 && config.path[plen - 1] == '/')
					filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
				else
					switch(fs_file_get_type(config.path))
					{
					case fs_filetype_zip:
					{
						where = scene_in_zip;
						filecount = fs_zip_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
						break;
					}
					case fs_filetype_chm:
					{
						where = scene_in_chm;
						filecount = fs_chm_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
						break;
					}
					case fs_filetype_rar:
					{
						where = scene_in_rar;
						filecount = fs_rar_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
						break;
					}
					default:
						strcpy(config.path, "ms0:/");
						strcpy(config.shortpath, "ms0:/");
						filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
						break;
					}
				if(filecount == 0)
				{
					strcpy(config.path, "ms0:/");
					strcpy(config.shortpath, "ms0:/");
					strcpy(config.lastfile, "");
					filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
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

static void scene_locload_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(237 - DISP_FONTSIZE * 10, 122 - 5 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 10, 136 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(238 - DISP_FONTSIZE * 10, 123 - 5 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 122 - 4 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(238 - DISP_FONTSIZE * 3, 123 - 5 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"读取文件位置");
	disp_line(238 - DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 123 - 4 * DISP_FONTSIZE, COLOR_WHITE);
}

static dword scene_locload(dword * selidx)
{
	t_win_menuitem item[10];
	dword i;
	for(i = 0; i < 10; i ++)
	{
		strcpy(item[i].name, "未使用");
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	location_enum(scene_loc_enum, item);
	dword index;
	item[0].data = (void *)false;
	item[1].data = (void *)*selidx;
	index = win_menu(238 - DISP_FONTSIZE * 10, 124 - 4 * DISP_FONTSIZE, 40, 10, item, 10, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_locload_predraw, NULL, scene_locload_menucb);
	*selidx = (dword)item[1].data;
	return (bool)item[0].data;
}

typedef dword (*t_scene_option_func)(dword * selidx);

static t_scene_option_func scene_option_func[] = {
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
	NULL
};

static t_win_menu_op scene_options_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		if(*index == 12 && win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
			scene_exit();
		if(scene_option_func[*index] != NULL)
		{
			item[0].data = (void *)scene_option_func[*index](item[1].data);
			if(item[0].data != 0)
				return win_menu_op_cancel;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SELECT:
		ctrl_waitreleaseintime(10000);
		return win_menu_op_cancel;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_options_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(237 - DISP_FONTSIZE * 3, 120 - 7 * DISP_FONTSIZE, 241 + DISP_FONTSIZE * 3, 137 + 7 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(238 - DISP_FONTSIZE * 3, 121 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 3, 120 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(238 - DISP_FONTSIZE * 2, 121 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"设置选项");
	disp_line(238 - DISP_FONTSIZE * 3, 121 - 6 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 3, 121 - 6 * DISP_FONTSIZE, COLOR_WHITE);
}

static dword scene_options(dword * selidx)
{
	t_win_menuitem item[13];
	dword i;
	strcpy(item[0].name, "  字体设置");
	strcpy(item[1].name, "  颜色设置");
	strcpy(item[2].name, "  系统选项");
	strcpy(item[3].name, "  系统按键");
	strcpy(item[4].name, "  阅读选项");
	strcpy(item[5].name, "  阅读按键");
#ifdef ENABLE_IMAGE
	strcpy(item[6].name, "  看图设置");
	strcpy(item[7].name, "  看图按键");
#else
	strcpy(item[6].name, "*看图已关闭*");
	strcpy(item[7].name, "*看图已关闭*");
#endif
	strcpy(item[8].name, "  操作设置");
#ifdef ENABLE_MUSIC
	strcpy(item[9].name, "  音乐播放");
#else
	strcpy(item[9].name, "*音乐已关闭*");
#endif
	strcpy(item[10].name, "保存文件位置");
	strcpy(item[11].name, "读取文件位置");
	strcpy(item[12].name, "  退出软件");
	for(i = 0; i < 13; i ++)
	{
		item[i].width = 12;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	item[0].data = (void *)0;
	item[1].data = (void *)selidx;
	dword index = 0;
	while((index = win_menu(238 - DISP_FONTSIZE * 3, 122 - 6 * DISP_FONTSIZE, 12, 13, item, 13, index, 0, RGB(0x10, 0x30, 0x20), true, scene_options_predraw, NULL, scene_options_menucb)) != INVALID);
	return (dword)item[0].data;
}

static t_win_menu_op scene_bookmark_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SELECT:
		bookmark_delete(bm);
		memset(&bm->row[0], 0xFF, 10 * sizeof(dword));
		win_msg("已删除书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		return win_menu_op_cancel;
	case PSP_CTRL_START:
		if(win_msgbox("是否要导出书签？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			char bmfn[256];
			if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
			{
				strcpy(bmfn, config.shortpath);
				strcat(bmfn, fs->filename);
			}
			else
				strcpy(bmfn, fs->filename);
			strcat(bmfn, ".ebm");
			bookmark_export(bm, bmfn);
			win_msg("已导出书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_SQUARE:
		strcpy(item[*index].name, "       ");
		bm->row[(*index) + 1] = *(dword *)item[0].data;
		utils_dword2string(bm->row[(*index) + 1] / 2, item[*index].name, 7);
		bookmark_save(bm);
		return win_menu_op_redraw;
	case PSP_CTRL_TRIANGLE:
		bm->row[(*index) + 1] = INVALID;
		strcpy(item[*index].name, "  NONE ");
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

static void scene_bookmark_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	disp_rectangle(63, 60 - DISP_FONTSIZE, 416, 64 + (1 + DISP_FONTSIZE) * 10, COLOR_WHITE);
	disp_fillrect(64, 61 - DISP_FONTSIZE, 415, 60, RGB(0x30, 0x60, 0x30));
	disp_putstring(75, 61 - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"书签      ○读取  ×取消  □保存  △删除");
	disp_fillrect(68 + 7 * DISP_FONTSIZE / 2, 62, 415, 63 + (1 + DISP_FONTSIZE) * 10, RGB(0x30, 0x60, 0x30));
	disp_line(64, 61, 415, 61, COLOR_WHITE);
	disp_line(64, 64 + (1 + DISP_FONTSIZE) * 9, 415, 64 + (1 + DISP_FONTSIZE) * 9, COLOR_WHITE);
	disp_fillrect(64, 65 + (1 + DISP_FONTSIZE) * 9, 67 + 7 * DISP_FONTSIZE / 2, 63 + (1 + DISP_FONTSIZE) * 10, RGB(0x30, 0x60, 0x30));
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

static bool scene_bookmark(dword * orgp)
{
	char archname[256];
	if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
	{
		strcpy(archname, config.shortpath);
		strcat(archname, fs->filename);
	}
	else
		strcpy(archname, fs->filename);
	bm = bookmark_open(archname);
	if(bm == NULL)
	{
		win_msg("无法打开书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		return 0;
	}
	dword i;
	t_win_menuitem item[9];
	for(i = 0; i < 9; i ++)
	{
		if(bm->row[i + 1] != INVALID)
		{
			strcpy(item[i].name, "       ");
			utils_dword2string(bm->row[i + 1] / 2, item[i].name, 7);
		}
		else
			strcpy(item[i].name, "  NONE ");
		item[i].width = 7;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
	}
	item[0].data = (void *)orgp;
	item[1].data = (void *)false;
	dword index;
	if((index = win_menu(64, 62, 7, 9, item, 9, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_bookmark_predraw, NULL, scene_bookmark_menucb)) != INVALID);
	bookmark_close(bm);
	bm = NULL;
	return (bool)item[1].data;
}

static void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku, dword * kd, dword * kl, dword * kr)
{
	dword i;
	memcpy(ctlkey, config.txtkey, 12 * sizeof(dword));
	memcpy(ctlkey2, config.txtkey2, 12 * sizeof(dword));
	switch(config.vertread)
	{
	case 2:
		for (i = 0; i < 12; i ++)
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
		for (i = 0; i < 12; i ++)
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

static dword scene_readbook(dword selidx)
{
	dword ctlkey[12], ctlkey2[12], ku, kd, kl, kr;
	dword cidx, rrow = INVALID;
	int ticks = 0;
	bool nextpage = false;
	int rowtop = 0;
	char tr[8], * trow = NULL;
	bool needrf = true, needrp = true, needrb = false;
	char filename[256], archname[256];
	scene_mountrbkey(ctlkey, ctlkey2, &ku, &kd, &kl, &kr);
	while(1)
	{
		if(needrf)
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
				return selidx;
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
			needrf = false;
		}
		if(needrp)
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
		if(nextpage) {
			nextpage = false;
			rowtop = 0;
			if(fs->crow >= fs->row_count - 1)
			{
				if(config.pagetonext && key != kr && fs_is_txtbook((t_fs_filetype)filelist[selidx].data))
				{
					dword orgidx = selidx;
					do {
						if(selidx < filecount - 1)
							selidx ++;
						else
							selidx = 0;
					} while(!fs_is_txtbook((t_fs_filetype)filelist[selidx].data));
					if(selidx != orgidx)
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
				continue;
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
			return selidx;
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
			switch(scene_options(&selidx))
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
				return selidx;
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
				if(config.pagetonext && key != kl && fs_is_txtbook((t_fs_filetype)filelist[selidx].data))
				{
					dword orgidx = selidx;
					do {
						if(selidx > 0)
							selidx --;
						else
							selidx = filecount - 1;
					} while(!fs_is_txtbook((t_fs_filetype)filelist[selidx].data));
					if(selidx != orgidx)
					{
						if(config.autobm)
							bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
						needrf = needrp = true;
						needrb = false;
						rrow = INVALID;
					}
				}
				continue;
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
				if(config.pagetonext && key != kr && fs_is_txtbook((t_fs_filetype)filelist[selidx].data))
				{
					dword orgidx = selidx;
					do {
						if(selidx < filecount - 1)
							selidx ++;
						else
							selidx = 0;
					} while(!fs_is_txtbook((t_fs_filetype)filelist[selidx].data));
					if(selidx != orgidx)
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
				continue;
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
			if (fs->row_count >= fs->row_count - 1)
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
			dword orgidx = selidx;
			if(config.autobm)
				bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
			do {
				if(selidx > 0)
					selidx --;
				else
					selidx = filecount - 1;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[selidx].data));
			if(selidx != orgidx)
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
			dword orgidx = selidx;
			do {
				if(selidx < filecount - 1)
					selidx ++;
				else
					selidx = 0;
			} while(!fs_is_txtbook((t_fs_filetype)filelist[selidx].data));
			if(selidx != orgidx)
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
	}
	scene_power_save(false);
	if(config.autobm)
		bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
	text_close(fs);
	fs = NULL;
	disp_duptocachealpha(50);
	return selidx;
}

#ifdef ENABLE_IMAGE
static dword scene_readimage(dword selidx)
{
	dword width, height, w2 = 0, h2 = 0, thumbw = 0, thumbh = 0, paintleft = 0, painttop = 0;
	pixel * imgdata = NULL, * imgshow = NULL;
	pixel bgcolor, thumbimg[128 * 128];
	dword oldangle = 0;
	char filename[256];
	int curtop = 0, curleft = 0, xpos = 0, ypos = 0;
	bool needrf = true, needrc = true, needrp = true, showinfo = false, thumb = false;
	int imgh;
	bool slideshow = false;
	time_t now = 0, lasttime = 0;
	imgreading = true;
	scene_power_save(false);
	if(config.imginfobar)
		imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
	else
		imgh = PSP_SCREEN_HEIGHT;
	while(1) {
		if(needrf)
		{
			if(imgshow != NULL && imgshow != imgdata)
			{
				free((void *)imgshow);
				imgshow = NULL;
			}
			if(imgdata != NULL)
			{
				free((void *)imgdata);
				imgdata = NULL;
			}
			if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
				strcpy(filename, filelist[selidx].compname);
			else
			{
				strcpy(filename, config.shortpath);
				strcat(filename, filelist[selidx].shortname);
			}
			int result;
			switch((t_fs_filetype)filelist[selidx].data)
			{
			case fs_filetype_png:
				switch(where)
				{
				case scene_in_zip:
					result = image_readpng_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readpng_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readpng_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readpng(filename, &width, &height, &imgdata, &bgcolor);
					break;
				}
				break;
			case fs_filetype_gif:
				switch(where)
				{
				case scene_in_zip:
					result = image_readgif_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readgif_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readgif_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readgif(filename, &width, &height, &imgdata, &bgcolor);
					break;
				}
				break;
			case fs_filetype_jpg:
				switch(where)
				{
				case scene_in_zip:
					result = image_readjpg_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readjpg_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readjpg_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readjpg(filename, &width, &height, &imgdata, &bgcolor);
					break;
				}
				break;
			case fs_filetype_bmp:
				switch(where)
				{
				case scene_in_zip:
					result = image_readbmp_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readbmp_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readbmp_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readbmp(filename, &width, &height, &imgdata, &bgcolor);
					break;
				}
				break;
			case fs_filetype_tga:
				switch(where)
				{
				case scene_in_zip:
					result = image_readtga_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readtga_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readtga_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readtga(filename, &width, &height, &imgdata, &bgcolor);
					break;
				}
				break;
			default:
				result = -1;
			}
			if(result != 0)
			{
				imgreading = false;
				scene_power_save(false);
				if(imgshow != NULL && imgshow != imgdata)
				{
					free((void *)imgshow);
					imgshow = NULL;
				}
				if(imgdata != NULL)
				{
					free((void *)imgdata);
					imgdata = NULL;
				}
				disp_duptocachealpha(50);
				return selidx;
			}
			strcpy(config.lastfile, filelist[selidx].compname);
			needrf = false;
			oldangle = 0;
		}
		if(needrc)
		{
			image_rotate(imgdata, &width, &height, oldangle, (dword)config.rotate * 90);
			oldangle = (dword)config.rotate * 90;
			if(config.fit > 0 && (config.fit != conf_fit_custom || config.scale != 100))
			{
				if(imgshow != NULL && imgshow != imgdata)
					free((void *)imgshow);
				imgshow = NULL;
				if(config.fit == conf_fit_custom)
				{
					w2 = width * config.scale / 100;
					h2 = height * config.scale / 100;
				}
				else if(config.fit == conf_fit_width)
				{
					config.scale = PSP_SCREEN_WIDTH / width;
					if(config.scale > 200)
						config.scale = (config.scale / 50) * 50;
					else
					{
						config.scale = (config.scale / 10) * 10;
						if(config.scale < 10)
							config.scale = 10;
					}
					w2 = PSP_SCREEN_WIDTH;
					h2 = height * PSP_SCREEN_WIDTH / width;
				}
				else if(config.fit == conf_fit_dblwidth)
				{
					config.scale = 960 / width;
					if(config.scale > 200)
						config.scale = (config.scale / 50) * 50;
					else
					{
						config.scale = (config.scale / 10) * 10;
						if(config.scale < 10)
							config.scale = 10;
					}
					w2 = 960;
					h2 = height * 960 / width;
				}
				else if(config.fit == conf_fit_dblheight)
				{
					config.scale = imgh / height;
					if(config.scale > 200)
						config.scale = (config.scale / 50) * 50;
					else
					{
						config.scale = (config.scale / 10) * 10;
						if(config.scale < 10)
							config.scale = 10;
					}
					h2 = imgh * 2;
					w2 = width * imgh * 2 / height;
				}
				else
				{
					config.scale = imgh / height;
					if(config.scale > 200)
						config.scale = (config.scale / 50) * 50;
					else
					{
						config.scale = (config.scale / 10) * 10;
						if(config.scale < 10)
							config.scale = 10;
					}
					h2 = imgh;
					w2 = width * imgh / height;
				}
				imgshow = (pixel *)malloc(sizeof(pixel) * w2 * h2);
				if(imgshow != NULL)
				{
					if(config.bicubic)
						image_zoom_bicubic(imgdata, width, height, imgshow, w2, h2);
					else
						image_zoom_bilinear(imgdata, width, height, imgshow, w2, h2);
				}
				else
				{
					imgshow = imgdata;
					w2 = width;
					h2 = height;
				}
			}
			else
			{
				config.scale = 100;
				imgshow = imgdata;
				w2 = width;
				h2 = height;
			}
			curleft = curtop = 0;
			xpos = (int)config.viewpos % 3;
			ypos = (int)config.viewpos / 3;
			if(w2 < PSP_SCREEN_WIDTH)
				paintleft = (PSP_SCREEN_WIDTH - w2) / 2;
			else
			{
				paintleft = 0;
				switch(xpos)
				{
				case 1:
					curleft = (w2 - PSP_SCREEN_WIDTH) / 2;
					break;
				case 2:
					curleft = w2 - PSP_SCREEN_WIDTH;
					break;
				}
			}
			if(h2 < imgh)
				painttop = (imgh - h2) / 2;
			else
			{
				painttop = 0;
				switch(ypos)
				{
				case 1:
					curtop = (h2 - imgh) / 2;
					break;
				case 2:
					curtop = h2 - imgh;
					break;
				}
			}

			if(width > height)
			{
				thumbw = 128;
				thumbh = height * 128 / width;
			}
			else
			{
				thumbh = 128;
				thumbw = width * 128 / height;
			}
			image_zoom_bilinear(imgdata, width, height, thumbimg, thumbw, thumbh);

			needrc = false;
			if(slideshow)
				lasttime = time(NULL);
		}
		if(needrp)
		{
			disp_waitv();
			disp_fillvram(bgcolor);
			disp_putimage(paintleft, painttop, w2, h2, curleft, curtop, imgshow);
			if(config.imginfobar || showinfo)
			{
				char infostr[64];
				if(config.fit == conf_fit_custom)
					sprintf(infostr, "%dx%d  %d%%  旋转角度 %d  %s", (int)w2, (int)h2, (int)config.scale, (int)oldangle, config.bicubic ? "三次立方" : "两次线性");
				else
					sprintf(infostr, "%dx%d  %s  旋转角度 %d  %s", (int)w2, (int)h2, conf_get_fitname(config.fit), (int)oldangle, config.bicubic ? "三次立方" : "两次线性");
				int ilen = strlen(infostr);
				if(config.imginfobar)
				{
					disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
					disp_putnstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)filename, 960 / DISP_FONTSIZE - ilen - 1, 0, 0, DISP_FONTSIZE, 0);
					disp_putnstring(PSP_SCREEN_WIDTH - DISP_FONTSIZE / 2 * ilen, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
				}
				else
				{
					disp_fillrect(11, 11, 10 + DISP_FONTSIZE / 2 * ilen, 10 + DISP_FONTSIZE, 0);
					disp_putnstring(11, 11, COLOR_WHITE, (const byte *)infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
				}
			}
			if(config.thumb == conf_thumb_always || thumb)
			{
				dword top = (PSP_SCREEN_HEIGHT - thumbh) / 2, bottom = top + thumbh;
				dword thumbl = 0, thumbr = 0, thumbt = 0, thumbb = 0;
				if(paintleft > 0)
				{
					thumbl = 0;
					thumbr = thumbw - 1;
				}
				else
				{
					thumbl = curleft * thumbw / w2;
					thumbr = (curleft + 479) * thumbw / w2;
				}
				if(painttop > 0)
				{
					thumbt = 0;
					thumbb = thumbb - 1;
				}
				else
				{
					thumbt = curtop * thumbh / h2;
					thumbb = (curtop + imgh - 1) * thumbh / h2;
				}
				disp_putimage(32, top, thumbw, thumbh, 0, 0, thumbimg);
				disp_line(34, bottom, 32 + thumbw, bottom, 0);
				disp_line(32 + thumbw, top + 2, 32 + thumbw, bottom - 1, 0);
				disp_rectangle(33 + thumbl, top + thumbt + 1, 33 + thumbr, top + thumbb + 1, 0);
				disp_rectangle(32 + thumbl, top + thumbt, 32 + thumbr, top + thumbb, COLOR_WHITE);
			}
			disp_flip();
			needrp = false;
		}
		now = time(NULL);
		dword key = 0;
		if(config.thumb == conf_thumb_scroll && thumb)
		{
			thumb = false;
			needrp = true;
			key = ctrl_read_cont();
		}
		else if(!slideshow || now - lasttime < config.slideinterval)
			key = (showinfo ? ctrl_read_cont() : (slideshow ? ctrl_waittime(config.slideinterval - (now - lasttime)) : ctrl_waitany()));
		if(slideshow)
		{
			scePowerTick(0);
			if(now - lasttime >= config.slideinterval)
			{
				key = CTRL_FORWARD;
				lasttime = now;
			}
		}
		if(showinfo && (key & PSP_CTRL_CIRCLE) == 0)
		{
			needrp = true;
			showinfo = false;
		}
		if(key == 0)
			continue;
		if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
		{
			if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
			{
				scene_exit();
				return win_menu_op_continue;
			}
		}
		else if(key == PSP_CTRL_SELECT)
		{
			needrp = true;
			bool lastbicubic = config.bicubic;
			if(scene_options(&selidx))
			{
				imgreading = false;
				scene_power_save(false);
				if(imgshow != NULL && imgshow != imgdata)
				{
					free((void *)imgshow);
					imgshow = NULL;
				}
				if(imgdata != NULL)
				{
					free((void *)imgdata);
					imgdata = NULL;
				}
				disp_duptocachealpha(50);
				return selidx;
			}
			if(lastbicubic != config.bicubic)
				needrc = true;
			if(config.imginfobar)
				imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
			else
				imgh = PSP_SCREEN_HEIGHT;
		}
#ifdef ENABLE_MUSIC
		else if(key == PSP_CTRL_START)
		{
			scene_mp3bar();
			needrp = true;
		}
#endif
		else if(key == config.imgkey[1] || key == config.imgkey2[1] || key == CTRL_FORWARD)
		{
			switch(config.imgpaging)
			{
			case conf_imgpaging_updown:
				switch(ypos)
				{
				case 0:
				case 1:
					if(curtop + imgh < h2)
					{
						curtop += imgh - config.imgpagereserve;
						if(curtop + imgh > h2)
							curtop = h2 - imgh;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curtop > 0)
					{
						curtop -= imgh - config.imgpagereserve;
						if(curtop < 0)
							curtop = 0;
						needrp = true;
						continue;
					}
					break;
				}
				curtop = 0;
				if(h2 > imgh && ypos == 2)
					curtop = h2 - imgh;
				switch(xpos)
				{
				case 0:
				case 1:
					if(curleft + PSP_SCREEN_WIDTH < w2)
					{
						curleft += PSP_SCREEN_WIDTH;
						if(curleft +PSP_SCREEN_WIDTH > w2)
							curleft = w2 - PSP_SCREEN_WIDTH;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curleft > 0)
					{
						curleft -= PSP_SCREEN_WIDTH;
						if(curleft < 0)
							curleft = 0;
						needrp = true;
						continue;
					}
					break;
				}
				break;
			case conf_imgpaging_leftright:
				switch(xpos)
				{
				case 0:
				case 1:
					if(curleft + PSP_SCREEN_WIDTH < w2)
					{
						curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
						if(curleft + PSP_SCREEN_WIDTH > w2)
							curleft = w2 - PSP_SCREEN_WIDTH;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curleft > 0)
					{
						curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
						if(curleft < 0)
							curleft = 0;
						needrp = true;
						continue;
					}
					break;
				}
				curleft = 0;
				if(w2 > PSP_SCREEN_WIDTH && xpos == 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch(ypos)
				{
				case 0:
				case 1:
					if(curtop + imgh < h2)
					{
						curtop += imgh;
						if(curtop + imgh > h2)
							curtop = h2 - imgh;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curtop > 0)
					{
						curtop -= imgh;
						if(curtop < 0)
							curtop = 0;
						needrp = true;
						continue;
					}
					break;
				}
				break;
			default:;
			}
			dword orgidx = selidx;
			do {
				if(selidx < filecount - 1)
					selidx ++;
				else
					selidx = 0;
			} while(!fs_is_image((t_fs_filetype)filelist[selidx].data));
			if(selidx != orgidx)
				needrf = needrc = needrp = true;
		}
		else if(key == config.imgkey[0] || key == config.imgkey2[0] || key == CTRL_BACK)
		{
			switch(config.imgpaging)
			{
			case conf_imgpaging_updown:
				switch(ypos)
				{
				case 0:
				case 1:
					if(curtop > 0)
					{
						curtop -= imgh - config.imgpagereserve;
						if(curtop < 0)
							curtop = 0;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curtop + imgh < h2)
					{
						curtop += imgh - config.imgpagereserve;
						if(curtop + imgh > h2)
							curtop = h2 - imgh;
						needrp = true;
						continue;
					}
					break;
				}
				curtop = 0;
				if(h2 > imgh && ypos < 2)
					curtop = h2 - imgh;
				switch(xpos)
				{
				case 0:
				case 1:
					if(curleft > 0)
					{
						curleft -= PSP_SCREEN_WIDTH;
						if(curleft < 0)
							curleft = 0;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curleft + PSP_SCREEN_WIDTH < w2)
					{
						curleft += PSP_SCREEN_WIDTH;
						if(curleft + PSP_SCREEN_WIDTH > w2)
							curleft = w2 - PSP_SCREEN_WIDTH;
						needrp = true;
						continue;
					}
					break;
				}
				break;
			case conf_imgpaging_leftright:
				switch(xpos)
				{
				case 0:
				case 1:
					if(curleft > 0)
					{
						curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
						if(curleft < 0)
							curleft = 0;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curleft + PSP_SCREEN_WIDTH < w2)
					{
						curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
						if(curleft + PSP_SCREEN_WIDTH > w2)
							curleft = w2 - PSP_SCREEN_WIDTH;
						needrp = true;
						continue;
					}
					break;
				}
				curleft = 0;
				if(w2 > PSP_SCREEN_WIDTH && xpos < 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch(ypos)
				{
				case 0:
				case 1:
					if(curtop > 0)
					{
						curtop -= imgh;
						if(curtop < 0)
							curtop = 0;
						needrp = true;
						continue;
					}
					break;
				case 2:
					if(curtop + imgh < h2)
					{
						curtop += imgh;
						if(curtop + imgh > h2)
							curtop = h2 - imgh;
						needrp = true;
						continue;
					}
					break;
				}
				break;
			default:;
			}
			dword orgidx = selidx;
			do {
				if(selidx > 0)
					selidx --;
				else
					selidx = filecount - 1;
			} while(!fs_is_image((t_fs_filetype)filelist[selidx].data));
			if(selidx != orgidx)
				needrf = needrc = needrp = true;
		}
#ifdef ENABLE_ANALOG
		else if(key == CTRL_ANALOG)
		{
			int x, y, orgtop = curtop, orgleft = curleft;
			ctrl_analog(&x, &y);
			x = x / 31 * (int)config.imgmvspd / 2;
			y = y / 31 * (int)config.imgmvspd / 2;
			curtop += y;
			if(curtop + imgh > h2)
				curtop = (int)h2 - imgh;
			if(curtop < 0)
				curtop = 0;
			curleft += x;
			if(curleft + PSP_SCREEN_WIDTH > w2)
				curleft = (int)w2 - PSP_SCREEN_WIDTH;
			if(curleft < 0)
				curleft = 0;
			thumb = (config.thumb == conf_thumb_scroll);
			needrp = (thumb || orgtop != curtop || orgleft != curleft);
		}
#endif
		else if(key == PSP_CTRL_LEFT)
		{
			if(curleft > 0)
			{
				curleft -= (int)config.imgmvspd;
				if(curleft < 0)
					curleft = 0;
				needrp = true;
			}
			thumb = (config.thumb == conf_thumb_scroll);
			needrp = thumb | needrp;
		}
		else if(key == PSP_CTRL_UP)
		{
			if(curtop > 0)
			{
				curtop -= (int)config.imgmvspd;
				if(curtop < 0)
					curtop = 0;
				needrp = true;
			}
			thumb = (config.thumb == conf_thumb_scroll);
			needrp = thumb | needrp;
		}
		else if(key == PSP_CTRL_RIGHT)
		{
			if(w2 > PSP_SCREEN_WIDTH && curleft < w2 - PSP_SCREEN_WIDTH)
			{
				curleft += (int)config.imgmvspd;
				if(curleft > w2 - PSP_SCREEN_WIDTH)
					curleft = w2 - PSP_SCREEN_WIDTH;
				needrp = true;
			}
			thumb = (config.thumb == conf_thumb_scroll);
			needrp = thumb | needrp;
		}
		else if(key == PSP_CTRL_DOWN)
		{
			if(h2 > imgh && curtop < h2 - imgh)
			{
				curtop += (int)config.imgmvspd;
				if(curtop > h2 - imgh)
					curtop = h2 - imgh;
				needrp = true;
			}
			thumb = (config.thumb == conf_thumb_scroll);
			needrp = thumb | needrp;
		}
		else if(key == config.imgkey[2] || key == config.imgkey2[2])
		{
			if(config.fit == conf_fit_custom)
				config.fit = conf_fit_none;
			else
				config.fit ++;
			needrc = needrp = true;
		}
		else if(key == config.imgkey[10] || key == config.imgkey2[10])
		{
			config.bicubic = !config.bicubic;
			needrc = needrp = true;
		}
		else if(key == config.imgkey[11] || key == config.imgkey2[11])
		{
			if(!slideshow)
			{
				slideshow = true;
				lasttime = time(NULL);
				ctrl_waitrelease();
			}
			else
			{
				slideshow = false;
				win_msg("幻灯片播放已经停止！", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
			}
		}
		else if(key == config.imgkey[7] || key == config.imgkey2[7])
		{
			config.imginfobar = !config.imginfobar;
			if(config.imginfobar)
				imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
			else
				imgh = PSP_SCREEN_HEIGHT;
			if(h2 > imgh && curtop > h2 - imgh)
				curtop = h2 - imgh;
			needrc = (config.fit == conf_fit_height);
			needrp = true;
		}
		else if(key == config.imgkey[9] || key == config.imgkey2[9] || key == CTRL_PLAYPAUSE)
		{
			if(slideshow)
			{
				slideshow = false;
				win_msg("幻灯片播放已经停止！", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
			}
			else
			{
				imgreading = false;
				scene_power_save(false);
				if(imgshow != NULL && imgshow != imgdata)
				{
					free((void *)imgshow);
					imgshow = NULL;
				}
				if(imgdata != NULL)
				{
					free((void *)imgdata);
					imgdata = NULL;
				}
				disp_duptocachealpha(50);
				return selidx;
			}
		}
		else if(key == config.imgkey[8] || key == config.imgkey2[8])
		{
			if(!showinfo)
			{
				needrp = true;
				showinfo = true;
			}
		}
		else if(key == config.imgkey[5] || key == config.imgkey2[5])
		{
			if(config.rotate == conf_rotate_0)
				config.rotate = conf_rotate_270;
			else
				config.rotate --;
			needrc = needrp = true;
		}
		else if(key == config.imgkey[6] || key == config.imgkey2[6])
		{
			if(config.rotate == conf_rotate_270)
				config.rotate = conf_rotate_0;
			else
				config.rotate ++;
			needrc = needrp = true;
		}
		else if(key == config.imgkey[3] || key == config.imgkey2[3])
		{
			if(config.scale > 200)
				config.scale -= 50;
			else if(config.scale > 10)
				config.scale -= 10;
			else
				continue;
			config.fit = conf_fit_custom;
			needrc = needrp = true;
		}
		else if(key == config.imgkey[4] || key == config.imgkey2[4])
		{
			if(config.scale < 200)
				config.scale += 10;
			else if(config.scale < 1000)
				config.scale += 50;
			else
				continue;
			config.fit = conf_fit_custom;
			needrc = needrp = true;
		}
		else
			needrf = needrc = false;
	}
	imgreading = false;
	scene_power_save(false);
	if(imgshow != NULL && imgshow != imgdata)
	{
		free((void *)imgshow);
		imgshow = NULL;
	}
	if(imgdata != NULL)
	{
		free((void *)imgdata);
		imgdata = NULL;
	}
	disp_duptocachealpha(50);
	return selidx;
}
#endif

static t_win_menu_op scene_filelist_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	dword orgidx = * index;
	t_win_menu_op op = win_menu_op_continue;
	if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
	{
		if(win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	}
	else if(key == config.flkey[3] || key == config.flkey2[3])
	{
		* index = 0;
		if(item[*index].compname[0] == '.')
			return win_menu_op_ok;
		return win_menu_op_force_redraw;
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
		pixel * saveimage = (pixel *)malloc(PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
		if(saveimage)
			disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
		disp_duptocachealpha(50);
		disp_rectangle(240 - DISP_FONTSIZE * 3 - 1, 136 - DISP_FONTSIZE * 3 - 1, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE * 4, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE * 3, 240 + DISP_FONTSIZE * 3 - 1, 136 + DISP_FONTSIZE * 4 - 1, RGB(0x18, 0x28, 0x50));
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
				if(win_msgbox("删除所选文件？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
				{
					char fn[256];
					config.lastfile[0] = 0;
					for(sidx = 0; sidx < filecount; sidx ++) if(item[sidx].selected)
					{
						strcpy(fn, config.shortpath);
						strcat(fn, item[sidx].shortname);
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
								strcpy(config.lastfile, item[idx].compname);
							else if(sidx > 0)
								strcpy(config.lastfile, item[idx - 1].compname);
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
						if(win_msgbox("添加目录内所有歌曲到播放列表？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
						{
							char cfn[256];
							sprintf(cfn, "%s%s/", config.path, item[sidx].compname);
							mp3_list_add_dir(cfn);
							win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
						}
						break;
					case fs_filetype_mp3:
#ifdef ENABLE_WMA
					case fs_filetype_wma:
#endif
						if(win_msgbox("添加歌曲到播放列表？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
						{
							char mp3name[256], mp3longname[256];
							strcpy(mp3name, config.shortpath);
							strcat(mp3name, filelist[sidx].shortname);
							strcpy(mp3longname, config.path);
							strcat(mp3longname, filelist[sidx].compname);
							mp3_list_add(mp3name, mp3longname);
							win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
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
							strcpy(bgfile, config.shortpath);
							strcat(bgfile, item[*index].shortname);
							if(stricmp(bgfile, config.bgfile) == 0)
							{
								if(win_msgbox("是否取消背景图？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
								{
									strcpy(config.bgfile, " ");
									bg_cancel();
									disp_fillvram(0);
									disp_flip();
									disp_fillvram(0);
								}
							}
							else
							{
								if(win_msgbox("是否将当前图片文件设为背景图？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
								{
									strcpy(config.bgfile, bgfile);
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
						if(!win_msgbox("是否要导入书签？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
							break;
					}
					else
					{
						if(!win_msgbox("添加歌曲到播放列表？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
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
								sprintf(cfn, "%s%s/", config.path, item[sidx].compname);
								mp3_list_add_dir(cfn);
							}
							break;
#endif
						case fs_filetype_ebm:
							if(bmcount > 0)
							{
								char bmfn[256];
								strcpy(bmfn, config.path);
								strcat(bmfn, item[sidx].shortname);
								bookmark_import(bmfn);
							}
							break;
#ifdef ENABLE_MUSIC
						case fs_filetype_mp3:
#ifdef ENABLE_WMA
						case fs_filetype_wma:
#endif
							if(bmcount == 0)
							{
								char mp3name[256], mp3longname[256];
								strcpy(mp3name, config.shortpath);
								strcat(mp3name, filelist[sidx].shortname);
								strcpy(mp3longname, config.path);
								strcat(mp3longname, filelist[sidx].compname);
								mp3_list_add(mp3name, mp3longname);
							}
							break;
#endif
						default:
							break;
						}
					}
					if(mp3count + dircount == 0 && bmcount > 0)
						win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
#ifdef ENABLE_MUSIC
					else if(mp3count + dircount > 0)
						win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
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
						if(!win_msgbox("是否要导入书签？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
							break;
					}
#ifdef ENABLE_MUSIC
					else
					{
						if(!win_msgbox("添加歌曲到播放列表？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
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
								sprintf(cfn, "%s%s/", config.path, item[sidx].compname);
								mp3_list_add_dir(cfn);
							}
							break;
#endif
						case fs_filetype_ebm:
							if(mp3count + dircount == 0)
							{
								char bmfn[256];
								strcpy(bmfn, config.path);
								strcat(bmfn, item[sidx].shortname);
								bookmark_import(bmfn);
							}
							break;
#ifdef ENABLE_MUSIC
						case fs_filetype_mp3:
#ifdef ENABLE_WMA
						case fs_filetype_wma:
#endif
							if(mp3count > 0)
							{
								char mp3name[256], mp3longname[256];
								strcpy(mp3name, config.shortpath);
								strcat(mp3name, filelist[sidx].shortname);
								strcpy(mp3longname, config.path);
								strcat(mp3longname, filelist[sidx].compname);
								mp3_list_add(mp3name, mp3longname);
							}
							break;
#endif
						default:
							break;
						}
					}
					if(mp3count + dircount == 0 && bmcount > 0)
						win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
#ifdef ENABLE_MUSIC
					else if(mp3count + dircount > 0)
						win_msg("已添加歌曲到列表!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
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
				strcpy(copydir, config.shortpath);
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
				strcpy(cutdir, config.shortpath);
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
						strcpy(copysrc, copydir);
						strcat(copysrc, copylist[sidx].shortname);
						strcpy(copydest, config.shortpath);
						strcat(copydest, copylist[sidx].shortname);
						if((t_fs_filetype)copylist[sidx].data == fs_filetype_dir)
							copy_dir(copysrc, copydest, NULL, NULL, NULL);
						else
							copy_file(copysrc, copydest, NULL, NULL, NULL);
					}
					strcpy(config.lastfile, item[*index].compname);
					retop = win_menu_op_cancel;
					inop = false;
				}
				else if(cutcount > 0)
				{
					for(sidx = 0; sidx < cutcount; sidx ++)
					{
						char cutsrc[260], cutdest[260];
						strcpy(cutsrc, cutdir);
						strcat(cutsrc, cutlist[sidx].shortname);
						strcpy(cutdest, config.shortpath);
						strcat(cutdest, cutlist[sidx].shortname);
						if((t_fs_filetype)cutlist[sidx].data == fs_filetype_dir)
							move_dir(cutsrc, cutdest, NULL, NULL, NULL);
						else
							move_file(cutsrc, cutdest, NULL, NULL, NULL);
					}
					strcpy(config.lastfile, item[*index].compname);
					retop = win_menu_op_cancel;
					inop = false;
					cutcount = 0;
					free((void *)cutlist);
					cutlist = NULL;
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
#ifdef ENABLE_MUSIC
	else if(key == PSP_CTRL_START)
	{
		scene_mp3bar();
		return win_menu_op_force_redraw;
	}
#endif
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

static void scene_filelist_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
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
	disp_putstring(0, 0, COLOR_WHITE, (const byte *)EREADER_VERSION_STR_LONG);
	disp_line(0, DISP_FONTSIZE, 479, DISP_FONTSIZE, COLOR_WHITE);
	disp_rectangle(239 - WRR * DISP_FONTSIZE, 138 - (HRR + 1) * (DISP_FONTSIZE + 1), 243 + WRR * DISP_FONTSIZE, 141 + HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
	disp_fillrect(240 - WRR * DISP_FONTSIZE, 139 - (HRR + 1) * (DISP_FONTSIZE + 1), 242 + WRR * DISP_FONTSIZE, 137 - HRR * (DISP_FONTSIZE + 1), RGB(0x30, 0x60, 0x30));
	disp_putnstring(240 - WRR * DISP_FONTSIZE, 139 - (HRR + 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)config.path, 40, 0, 0, DISP_FONTSIZE, 0);
	disp_line(240 - WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1), 242 + WRR * DISP_FONTSIZE, 138 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE);
	disp_line(0, 271 - DISP_FONTSIZE, 479, 271 - DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
	disp_putstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)"START 音乐播放控制   SELECT 选项   选项内按□进入按键设置");
}

static void scene_filelist_postdraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	strcpy(config.lastfile, item[index].compname);
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
				sprintf(outstr, "文件大小: %u 字节\n", (unsigned int)item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 3) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				sprintf(outstr, "创建时间: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[0] >> 9) + 1980, (item[index].data2[0] & 0x01FF) >> 5, item[index].data2[0] & 0x01F, item[index].data2[1] >> 11, (item[index].data2[1] & 0x07FF) >> 5, (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 2) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				sprintf(outstr, "最后修改: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[2] >> 9) + 1980, (item[index].data2[2] & 0x01FF) >> 5, item[index].data2[2] & 0x01F, item[index].data2[3] >> 11, (item[index].data2[3] & 0x07FF) >> 5, (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 136 + (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
			else
			{
				disp_rectangle(239 - (WRR - 2) * DISP_FONTSIZE, 141 - HRR * (DISP_FONTSIZE + 1), 243 + (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 3) * (DISP_FONTSIZE + 1), COLOR_WHITE);
				disp_fillrect(240 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), 242 + (WRR - 2) * DISP_FONTSIZE, 141 - (HRR - 3) * (DISP_FONTSIZE + 1), RGB(0x20, 0x20, 0x20));
				char outstr[256];
				sprintf(outstr, "文件大小: %u 字节\n", (unsigned int)item[index].data3);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - HRR * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				sprintf(outstr, "创建时间: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[0] >> 9) + 1980, (item[index].data2[0] & 0x01FF) >> 5, item[index].data2[0] & 0x01F, item[index].data2[1] >> 11, (item[index].data2[1] & 0x07FF) >> 5, (item[index].data2[1] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 1) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
				sprintf(outstr, "最后修改: %04d-%02d-%02d %02d:%02d:%02d\n", (item[index].data2[2] >> 9) + 1980, (item[index].data2[2] & 0x01FF) >> 5, item[index].data2[2] & 0x01F, item[index].data2[3] >> 11, (item[index].data2[3] & 0x07FF) >> 5, (item[index].data2[3] & 0x01F) * 2);
				disp_putstring(242 - (WRR - 2) * DISP_FONTSIZE, 142 - (HRR - 2) * (DISP_FONTSIZE + 1), COLOR_WHITE, (const byte *)outstr);
			}
		}
		else
		{
			char outstr[256];
			sprintf(outstr, "文件大小: %s 字节\n", item[index].shortname);
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

static void scene_filelist()
{
	dword idx = 0;
	where = scene_in_dir;
	if(strlen(config.shortpath) == 0)
	{
		strcpy(config.path, "ms0:/");
		strcpy(config.shortpath, "ms0:/");
	}
	dword plen = strlen(config.path);
	if(plen > 0 && config.path[plen - 1] == '/')
		filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
	else
		switch(fs_file_get_type(config.path))
		{
		case fs_filetype_zip:
		{
			where = scene_in_zip;
			filecount = fs_zip_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			break;
		}
		case fs_filetype_chm:
		{
			where = scene_in_chm;
			filecount = fs_chm_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			break;
		}
		case fs_filetype_rar:
		{
			where = scene_in_rar;
			filecount = fs_rar_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			break;
		}
		default:
			strcpy(config.path, "ms0:/");
			strcpy(config.shortpath, "ms0:/");
			filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
			break;
		}
	if(filecount == 0)
	{
		strcpy(config.path, "ms0:/");
		strcpy(config.shortpath, "ms0:/");
		filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
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
		if(!config.isreading && !locreading)
			idx = win_menu(240 - WRR * DISP_FONTSIZE, 139 - HRR * (DISP_FONTSIZE + 1), WRR * 4, HRR * 2, filelist, filecount, idx, 0, RGB(0x10, 0x30, 0x20), false, scene_filelist_predraw, scene_filelist_postdraw, scene_filelist_menucb);
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
				filecount = fs_zip_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
				break;
			case scene_in_chm:
				filecount = fs_chm_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
				break;
			case scene_in_rar:
				filecount = fs_rar_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
				break;
			default:
				filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
				if(filecount == 0)
				{
					strcpy(config.path, "ms0:/");
					strcpy(config.shortpath, "ms0:/");
					filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
				}
				break;
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
					strcpy(pdir, lps);
					*lps = 0;
				}
				else
					config.path[0] = 0;
			}
			else if(where == scene_in_dir)
			{
				strcat(config.path, filelist[idx].compname);
				strcat(config.path, "/");
			}
			if(config.path[0] == 0)
				filecount = fs_list_device(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			else if(strnicmp(config.path, "ms0:/", 5) == 0)
				filecount = fs_dir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF), config.showhidden, config.showunknown);
			else
				filecount = fs_flashdir_to_menu(config.path, config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
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
		case fs_filetype_zip:
			where = scene_in_zip;
			strcat(config.path, filelist[idx].compname);
			strcat(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_zip_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
		case fs_filetype_chm:
			where = scene_in_chm;
			strcat(config.path, filelist[idx].compname);
			strcat(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_chm_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
		case fs_filetype_rar:
			where = scene_in_rar;
			strcat(config.path, filelist[idx].compname);
			strcat(config.shortpath, filelist[idx].shortname);
			idx = 0;
			filecount = fs_rar_to_menu(config.shortpath, &filelist, RGB(0xDF, 0xDF, 0xDF), RGB(0xFF, 0xFF, 0x40), RGB(0x10, 0x30, 0x20), RGB(0x20, 0x20, 0xDF));
			quicksort(filelist, (filecount > 0 && filelist[0].compname[0] == '.') ? 1 : 0, filecount - 1, sizeof(t_win_menuitem), compare_func[(int)config.arrange]);
			break;
#ifdef ENABLE_PMPAVC
		case fs_filetype_pmp:
		{
			char pmpname[256];
			strcpy(pmpname, config.shortpath);
			strcat(pmpname, filelist[idx].shortname);
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
			if(win_msgbox("是否要导入书签？", "是", "否", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
			{
				char bmfn[256];
				strcpy(bmfn, config.shortpath);
				strcat(bmfn, filelist[idx].shortname);
				bookmark_import(bmfn);
				win_msg("已导入书签!", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
			}
#ifdef ENABLE_MUSIC
		case fs_filetype_mp3:
#ifdef ENABLE_WMA
		case fs_filetype_wma:
#endif
			{
				char fn[256], lfn[256];
				strcpy(fn, config.shortpath);
				strcat(fn, filelist[idx].shortname);
				strcpy(lfn, config.path);
				strcat(lfn, filelist[idx].compname);
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

extern void scene_init()
{
#ifdef ENABLE_USB
	usb_open();
#endif

	char fontzipfile[256], efontfile[256], cfontfile[256], conffile[256], locconf[256], bmfile[256]
#ifdef ENABLE_MUSIC
		, mp3conf[256]
#endif
		;

	getcwd(appdir, 256);
	strcat(appdir, "/");
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
	strcpy(config.path, "ms0:/");
	strcpy(config.shortpath, "ms0:/");

#ifdef _DEBUG
	char logfile[256];
	strcpy(logfile, appdir);
	strcat(logfile, "ereader.log");
	log_open(logfile);
#endif

	strcpy(conffile, appdir);
	strcat(conffile, "ereader.conf");
	conf_set_file(conffile);
	conf_load(&config);

#ifdef ENABLE_BG
	if(config.bgfile[0] == 0)
	{
		strcpy(config.bgfile, appdir);
		strcat(config.bgfile, "bg.png");
	}
	bg_load(config.bgfile, config.bgcolor, fs_file_get_type(config.bgfile), config.grayscale);
#endif

	strcpy(fontzipfile, appdir);
	strcat(fontzipfile, "fonts.zip");
	int _fsize;
	for(_fsize = 12; _fsize <= 32; _fsize += 2)
	{
		sprintf(efontfile, "ASC%d", _fsize);
		sprintf(cfontfile, "GBK%d", _fsize);
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
			sprintf(efontfile, "%sfonts/ASC%d", appdir, _fsize);
			sprintf(cfontfile, "%sfonts/GBK%d", appdir, _fsize);
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
	}
	if(fontcount == 0 || !scene_load_font() || !scene_load_book_font())
	{
		pspDebugScreenInit();
		pspDebugScreenPrintf("Error loading font file! Press any buttun for exit!");
		ctrl_waitany();
		sceKernelExitGame();
	}
	drperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - config.borderspace * 2 + config.rowspace + DISP_BOOK_FONTSIZE * 2 - 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	rowsperpage = ((config.vertread ? PSP_SCREEN_WIDTH : PSP_SCREEN_HEIGHT) - (config.infobar ? DISP_BOOK_FONTSIZE : 0) - config.borderspace * 2) / (config.rowspace + DISP_BOOK_FONTSIZE);
	pixelsperrow = (config.vertread ? (config.scrollbar ? 267 : PSP_SCREEN_HEIGHT) : (config.scrollbar ? 475 : PSP_SCREEN_WIDTH)) - config.borderspace * 2;

	ctrl_init();
#ifdef ENABLE_HPRM
	ctrl_enablehprm(config.hprmctrl);
#endif
	disp_init();
	fat_init();

	strcpy(bmfile, appdir);
	strcat(bmfile, "bookmark.conf");
	bookmark_init(bmfile);
	strcpy(locconf, appdir);
	strcat(locconf, "location.conf");
	location_init(locconf, locaval);

#ifdef ENABLE_MUSIC
	mp3_init();

	strcpy(mp3conf, appdir);
	strcat(mp3conf, "music.lst");
	if(config.confver < 0x00090100 || !mp3_list_load(mp3conf))
		mp3_list_add_dir("ms0:/PSP/MUSIC/");
	mp3_start();
	mp3_set_encode(config.mp3encode);
#ifdef ENABLE_HPRM
	mp3_set_hprm(!config.hprmctrl);
#endif
	scene_power_save(true);
	mp3_set_cycle(config.mp3cycle);
	if(config.autoplay)
		mp3_resume();
#endif

	scene_filelist();
}

extern void scene_exit()
{
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
			strcpy(archname, config.shortpath);
			strcat(archname, fs->filename);
		}
		else
			strcpy(archname, fs->filename);
		if(config.autobm)
			bookmark_autosave(archname, (fs->rows[fs->crow >> 10] + (fs->crow & 0x3FF))->start - fs->buf);
	}
	conf_save(&config);

#ifdef ENABLE_MUSIC
	mp3_end();
	char mp3conf[256];
	strcpy(mp3conf, appdir);
	strcat(mp3conf, "music.lst");
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
#ifdef _DEBUG
	log_close();
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
		power_set_clock(36, 54);
	else if(imgreading && (config.bicubic
#ifdef ENABLE_MUSIC
		|| !mp3_paused()
#endif
		))
	{
		if(sceKernelDevkitVersion() >= 0x02000010)
			power_set_clock(300, 150);
		else
			power_set_clock(333, 166);
	}
	else
		power_set_clock(222, 111);
}

extern void scene_exception()
{
	config.savesucc = false;
}

extern const char * scene_appdir()
{
	return appdir;
}
