/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <malloc.h>
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
#include "dbg.h"

static volatile int secticks = 0;

unsigned int getFreeMemory();

t_win_menu_op scene_mp3_list_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox
				("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE,
				 config.msgbcolor)) {
				scene_exit();
				return win_menu_op_continue;
			}
			return win_menu_op_force_redraw;
		case PSP_CTRL_SQUARE:
#ifdef ENABLE_MUSIC
			if (*index < mp3_list_count() - 1) {
				t_win_menuitem sitem;
				dword mp3i = *index;

				mp3_list_movedown(mp3i);
				memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
				memcpy(&item[mp3i], &item[mp3i + 1], sizeof(t_win_menuitem));
				memcpy(&item[mp3i + 1], &sitem, sizeof(t_win_menuitem));
				(*index)++;
				return win_menu_op_force_redraw;
			}
#endif
			return win_menu_op_continue;
		case PSP_CTRL_TRIANGLE:
#ifdef ENABLE_MUSIC
			if (*index > 0) {
				t_win_menuitem sitem;
				dword mp3i = *index;

				mp3_list_moveup(mp3i);
				memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
				memcpy(&item[mp3i], &item[mp3i - 1], sizeof(t_win_menuitem));
				memcpy(&item[mp3i - 1], &sitem, sizeof(t_win_menuitem));
				(*index)--;
				return win_menu_op_force_redraw;
			}
#endif
			return win_menu_op_continue;
		case PSP_CTRL_CIRCLE:
#ifdef ENABLE_MUSIC
			if (win_msgbox
				("从歌曲列表移除文件?", "是", "否", COLOR_WHITE, COLOR_WHITE,
				 config.msgbcolor)) {
				mp3_list_delete(*index);
				if (*index < *count - 1)
					memmove(&item[*index], &item[*index + 1],
							(*count - *index - 1) * sizeof(t_win_menuitem));
				if (*index == *count - 1)
					(*index)--;
				(*count)--;
			}
#endif
			return win_menu_op_ok;
		case PSP_CTRL_START:
#ifdef ENABLE_MUSIC
			if (mp3_list_get_path(*index) != NULL) {
				mp3_directplay(mp3_list_get_path(*index), NULL);
			}
			return win_menu_op_continue;
#endif
			return win_menu_op_ok;
		case PSP_CTRL_SELECT:
			return win_menu_op_cancel;
	}
	dword orgidx = *index;
	t_win_menu_op op =
		win_menu_defcb(key, item, count, max_height, topindex, index);
	if (*index != orgidx && ((orgidx - *topindex < 6 && *index - *topindex >= 6)
							 || (orgidx - *topindex >= 6
								 && *index - *topindex < 6)))
		return win_menu_op_force_redraw;
	return op;
}

void scene_mp3_list_predraw(p_win_menuitem item, dword index, dword topindex,
							dword max_height)
{
	disp_rectangle(239 - 10 * DISP_FONTSIZE, 128 - 7 * DISP_FONTSIZE,
				   243 + 10 * DISP_FONTSIZE, 145 + 7 * DISP_FONTSIZE,
				   COLOR_WHITE);
	disp_line(240 - 10 * DISP_FONTSIZE, 129 - 6 * DISP_FONTSIZE,
			  242 + 10 * DISP_FONTSIZE, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_line(240 - 10 * DISP_FONTSIZE, 144 + 6 * DISP_FONTSIZE,
			  242 + 10 * DISP_FONTSIZE, 144 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - 10 * DISP_FONTSIZE, 129 - 7 * DISP_FONTSIZE,
				  242 + 10 * DISP_FONTSIZE, 128 - 6 * DISP_FONTSIZE, RGB(0x20,
																		 0x40,
																		 0x30));
	disp_fillrect(240 - 10 * DISP_FONTSIZE, 145 + 6 * DISP_FONTSIZE,
				  242 + 10 * DISP_FONTSIZE, 144 + 7 * DISP_FONTSIZE, RGB(0x20,
																		 0x40,
																		 0x30));
	disp_putstring(240 - 10 * DISP_FONTSIZE, 129 - 7 * DISP_FONTSIZE,
				   COLOR_WHITE,
				   (const byte *) " ○删除 □下移 △上移 ×退出 START播放");
	disp_putstring(240 - 10 * DISP_FONTSIZE, 145 + 6 * DISP_FONTSIZE,
				   COLOR_WHITE,
				   (const byte *) "要添加乐曲请到文件列表选取音乐文件按○");
}

void scene_mp3_list_postdraw(p_win_menuitem item, dword index, dword topindex,
							 dword max_height)
{
	char outstr[256];
	const char *fname;

#ifdef ENABLE_MUSIC
	fname = mp3_list_get(index);
#else
	fname = "音乐已关闭";
#endif

	if (strlen(fname) <= 36)
		STRCPY_S(outstr, fname);
	else {
		mbcsncpy_s((unsigned char *) outstr, 34, (const unsigned char *) fname,
				   -1);
		if (strlen(outstr) < 33) {
			mbcsncpy_s(((unsigned char *) outstr), 36,
					   ((const unsigned char *) fname), -1);
			STRCAT_S(outstr, "..");
		} else
			STRCAT_S(outstr, "...");
	}
	if (index - topindex < 6) {
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 140 + 5 * DISP_FONTSIZE,
					   243 + 9 * DISP_FONTSIZE, 141 + 6 * DISP_FONTSIZE,
					   COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE,
					  242 + 9 * DISP_FONTSIZE, 140 + 6 * DISP_FONTSIZE,
					  RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE,
					   COLOR_WHITE, (const byte *) outstr);
	} else {
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 133 - 6 * DISP_FONTSIZE,
					   243 + 9 * DISP_FONTSIZE, 134 - 5 * DISP_FONTSIZE,
					   COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE,
					  242 + 9 * DISP_FONTSIZE, 133 - 5 * DISP_FONTSIZE,
					  RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE,
					   COLOR_WHITE, (const byte *) outstr);
	}
}

void scene_mp3_list()
{
#ifdef ENABLE_MUSIC
	p_win_menuitem item = win_realloc_items(NULL, 0, mp3_list_count());

	if (item == NULL)
		return;
	dword i;

	for (i = 0; i < mp3_list_count(); i++) {
		char *rname = strrchr(mp3_list_get(i), '/');

		if (rname == NULL)
			rname = (char *) mp3_list_get(i);
		else
			rname++;
		if (strlen(rname) <= 38)
			STRCPY_S(item[i].name, rname);
		else {
			mbcsncpy_s((unsigned char *) item[i].name, 37,
					   (const unsigned char *) rname, -1);
			STRCAT_S(item[i].name, "...");
		}
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = RGB(0x40, 0x40, 0x28);
		item[i].selbcolor = config.selbcolor;
	}
	dword index = 0;

	while ((index =
			win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 40, 12,
					 item, mp3_list_count(), index, 0, RGB(0x40, 0x40, 0x28),
					 true, scene_mp3_list_predraw, scene_mp3_list_postdraw,
					 scene_mp3_list_menucb)) != INVALID)
		if (mp3_list_count() == 0)
			break;
	free((void *) item);
#endif
}

static char *weekStr[] = {
	"星期天",
	"星期一",
	"星期二",
	"星期三",
	"星期四",
	"星期五",
	"星期六",
	"日期错误"
};

void scene_mp3bar()
{
	bool firstdup = true;
	pixel *saveimage = (pixel *) memalign(16,
										  PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT *
										  sizeof(pixel));
	if (saveimage != NULL)
		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
	u64 timer_start, timer_end;

	sceRtcGetCurrentTick(&timer_start);
	while (1) {
		if (firstdup) {
			disp_duptocachealpha(50);
			firstdup = false;
		} else
			disp_duptocache();
		disp_rectangle(5, 5, 474, 8 + DISP_FONTSIZE * 2, COLOR_WHITE);
		disp_fillrect(6, 6, 473, 7 + DISP_FONTSIZE * 2,
					  config.usedyncolor ? GetBGColorByTime() : config.
					  msgbcolor);
		char timestr[80];
		pspTime tm;
		dword cpu, bus;
		dword pos;

		sceRtcGetCurrentClockLocalTime(&tm);
		pos = sceRtcGetDayOfWeek(tm.year, tm.month, tm.day);
		pos = pos >= 7 ? 7 : pos;
		power_get_clock(&cpu, &bus);
		SPRINTF_S(timestr,
				  "%u年%u月%u日  %s  %02u:%02u:%02u   CPU/BUS: %d/%d",
				  tm.year, tm.month, tm.day,
				  weekStr[pos], tm.hour, tm.minutes, tm.seconds, (int) cpu,
				  (int) bus);
		disp_putstring(6 + DISP_FONTSIZE * 2, 6, COLOR_WHITE,
					   (const byte *) timestr);
		int percent, lifetime, tempe, volt;
		char battstr[80];

		power_get_battery(&percent, &lifetime, &tempe, &volt);
		if (percent >= 0) {
			if (config.fontsize <= 12) {
				SPRINTF_S(battstr,
						  "%s  剩余电量: %d%%(%d小时%d分钟)  电池温度: %d℃   剩余内存: %dKB",
						  power_get_battery_charging(), percent, lifetime / 60,
						  lifetime % 60, tempe, getFreeMemory() / 1024);
			} else {
				SPRINTF_S(battstr,
						  "%s  剩余电量: %d%%(%d小时%d分钟)  电池温度: %d℃",
						  power_get_battery_charging(), percent, lifetime / 60,
						  lifetime % 60, tempe);
			}
		} else
			SPRINTF_S(battstr, "[电源供电]   剩余内存: %dKB",
					  getFreeMemory() / 1024);
		disp_putstring(6 + DISP_FONTSIZE, 7 + DISP_FONTSIZE, COLOR_WHITE,
					   (const byte *) battstr);

#ifdef ENABLE_LYRIC
		disp_rectangle(5, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex), 474,
					   136 + (DISP_FONTSIZE + 1) * config.lyricex, COLOR_WHITE);
		disp_fillrect(6, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) + 1,
					  473, 136 + (DISP_FONTSIZE + 1) * config.lyricex - 1,
					  config.usedyncolor ? GetBGColorByTime() : config.
					  msgbcolor);
		{
			const char *ly[config.lyricex * 2 + 1];
			dword ss[config.lyricex * 2 + 1];

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
			if (lyric_get_cur_lines(mp3_get_lyric(), config.lyricex, ly, ss)) {
				int lidx;
				dword cpl = 936 / DISP_FONTSIZE;

				for (lidx = 0; lidx < config.lyricex * 2 + 1; lidx++)
					if (ly[lidx] != NULL) {
						if (ss[lidx] > cpl)
							ss[lidx] = cpl;
						char t[BUFSIZ];

						lyric_decode(ly[lidx], t, &ss[lidx]);
						disp_putnstring(6 +
										(cpl - ss[lidx]) * DISP_FONTSIZE / 4,
										136 - (DISP_FONTSIZE + 1) * (1 +
																	 config.
																	 lyricex) +
										(DISP_FONTSIZE + 1) * lidx + 1,
										(lidx ==
										 config.
										 lyricex) ? COLOR_WHITE : RGB(0x7F,
																	  0x7F,
																	  0x7F),
										(const byte *) t, ss[lidx], 0, 0,
										DISP_FONTSIZE, 0);
					}
			}
#endif
		}
#endif

#ifdef ENABLE_MUSIC
		disp_rectangle(5, 262 - DISP_FONTSIZE * 4, 474, 267, COLOR_WHITE);
		disp_fillrect(6, 263 - DISP_FONTSIZE * 4, 473, 266,
					  config.usedyncolor ? GetBGColorByTime() : config.
					  msgbcolor);
		int bitrate, sample, len, tlen;
		char infostr[80];

		if (mp3_get_info(&bitrate, &sample, &len, &tlen))
			SPRINTF_S(infostr, "%s   %d kbps   %d Hz   %02d:%02d / %02d:%02d",
					  conf_get_cyclename(config.mp3cycle), bitrate, sample,
					  len / 60, len % 60, tlen / 60, tlen % 60);
		else
			SPRINTF_S(infostr, "%s", conf_get_cyclename(config.mp3cycle));
		disp_putstring(6 + DISP_FONTSIZE, 265 - DISP_FONTSIZE * 2, COLOR_WHITE,
					   (const byte *) infostr);
		disp_putstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE,
					   (const byte *)
					   "  SELECT 编辑列表   ←快速后退   →快速前进");
		char lrctaginfo[80];

		SPRINTF_S(lrctaginfo, "%s  LRC  %s  ID3 %s",
				  "  SELECT 编辑列表   ←快速后退   →快速前进",
				  conf_get_encodename(config.lyricencode),
				  conf_get_encodename(config.mp3encode));
		disp_putnstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE,
						(const byte *) lrctaginfo,
						(468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0,
						DISP_FONTSIZE, 0);

		disp_putstring(6 + DISP_FONTSIZE, 264 - DISP_FONTSIZE * 3, COLOR_WHITE,
					   (const byte *)
					   "○播放/暂停 ×循环 □停止 △曲名编码  L上一首  R下一首");
		disp_putnstring(6 + DISP_FONTSIZE, 266 - DISP_FONTSIZE, COLOR_WHITE,
						(const byte *) mp3_get_tag(),
						(468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0,
						DISP_FONTSIZE, 0);
#endif
		disp_flip();
		dword key = ctrl_read();

		if (key != 0) {
			secticks = 0;
		}
		switch (key) {
			case (PSP_CTRL_SELECT | PSP_CTRL_START):
				if (win_msgbox
					("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE,
					 config.msgbcolor)) {
					scene_exit();
					return;
				}
				break;
			case PSP_CTRL_CIRCLE:
#ifdef ENABLE_MUSIC
				if (mp3_paused())
					mp3_resume();
				else
					mp3_pause();
#endif
				break;
			case PSP_CTRL_CROSS:
#ifdef ENABLE_MUSIC
				if (config.mp3cycle == conf_cycle_random)
					config.mp3cycle = conf_cycle_single;
				else
					config.mp3cycle++;
				mp3_set_cycle(config.mp3cycle);
#endif
				break;
			case PSP_CTRL_SQUARE:
#ifdef ENABLE_MUSIC
				mp3_restart();
				scene_power_save(true);
#endif
				break;
			case (PSP_CTRL_LEFT | PSP_CTRL_TRIANGLE):
#ifdef ENABLE_MUSIC
				config.lyricencode++;
				if ((dword) config.lyricencode > 4)
					config.lyricencode = 0;
#endif
				break;
			case (PSP_CTRL_RIGHT | PSP_CTRL_TRIANGLE):
#ifdef ENABLE_MUSIC
				config.mp3encode++;
				if ((dword) config.mp3encode > 4)
					config.mp3encode = 0;
				mp3_set_encode(config.mp3encode);
#endif
				break;
			case PSP_CTRL_LTRIGGER:
#ifdef ENABLE_MUSIC
				mp3_prev();
#endif
				break;
			case PSP_CTRL_RTRIGGER:
#ifdef ENABLE_MUSIC
				mp3_next();
#endif
				break;
			case PSP_CTRL_LEFT:
#ifdef ENABLE_MUSIC
				mp3_backward();
#endif
				break;
			case PSP_CTRL_RIGHT:
#ifdef ENABLE_MUSIC
				mp3_forward();
#endif
				break;
			case PSP_CTRL_SELECT:
#ifdef ENABLE_MUSIC
				if (mp3_list_count() == 0)
					break;
				scene_mp3_list();
#endif
				break;
			case PSP_CTRL_START:
				ctrl_waitrelease();
				if (saveimage != NULL) {
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
								  0, saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
								  0, saveimage);
					free((void *) saveimage);
				} else {
					disp_waitv();
#ifdef ENABLE_BG
					if (!bg_display())
#endif
						disp_fillvram(0);
					disp_flip();
					disp_duptocache();
				}
				return;
		}
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
		sceKernelDelayThread(20000);
	}
}

// 获取PSP剩余内存，单位为Bytes
// 作者:诗诺比
unsigned int getFreeMemory()
{
	void *p[30];
	unsigned int block_size = 0x04000000;	//最大内存:64MB,必需是2的N次方
	unsigned int block_free = 0;
	int i = 0;

	while ((block_size >>= 1) >= 0x0400)	//最小精度:1KB
	{
		if (NULL != (p[i] = malloc(block_size))) {
			block_free |= block_size;
			++i;
		}
	}
	if (NULL != (p[i] = malloc(0x0400)))	//最小精度:1KB
	{
		block_free += 0x0400;
		++i;
	}
	while (i--) {
		free(p[i]);
	}
	return block_free;
}
