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
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"
#include "simple_gettext.h"
#include "dbg.h"
#include "xrPrx/xrPrx.h"

extern win_menu_predraw_data g_predraw;

static volatile int secticks = 0;

// ��ȡPSPʣ���ڴ棬��λΪBytes
// ����:ʫŵ��
static unsigned int get_free_mem(void)
{
	void *p[30];
	unsigned int block_size = 0x04000000;	//����ڴ�:64MB,������2��N�η�
	unsigned int block_free = 0;
	int i = 0;

	while ((block_size >>= 1) >= 0x0400)	//��С����:1KB
	{
		if (NULL != (p[i] = malloc(block_size))) {
			block_free |= block_size;
			++i;
		}
	}
	if (NULL != (p[i] = malloc(0x0400)))	//��С����:1KB
	{
		block_free += 0x0400;
		++i;
	}
	while (i--) {
		free(p[i]);
	}
	return block_free;
}

t_win_menu_op scene_mp3_list_menucb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox
				("�Ƿ��˳����?", "��", "��", COLOR_WHITE, COLOR_WHITE,
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
				(_("�Ӹ����б��Ƴ��ļ�?"), _("��"), _("��"), COLOR_WHITE,
				 COLOR_WHITE, config.msgbcolor)) {
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
	int left, right, upper, bottom;

	default_predraw(&g_predraw, _(" ��ɾ�� ������ ������ ���˳� START����"),
					max_height, &left, &right, &upper, &bottom, 4);
	int pos =
		get_center_pos(0, 480, _("Ҫ��������뵽�ļ��б�ѡȡ�����ļ�����"));
	int posend =
		pos + 1 +
		strlen(_("Ҫ��������뵽�ļ��б�ѡȡ�����ļ�����")) * DISP_FONTSIZE / 2;
	upper = bottom + 1;
	bottom = bottom + 1 + 1 * DISP_FONTSIZE;

	disp_rectangle(pos - 2, upper - 1, posend + 1, bottom + 1, COLOR_WHITE);
	disp_fillrect(pos - 1, upper, posend, bottom, RGB(0x20, 0x40, 0x30));
	disp_putstring(pos, upper, COLOR_WHITE,
				   (const byte *) _("Ҫ��������뵽�ļ��б�ѡȡ�����ļ�����"));
}

void scene_mp3_list_postdraw(p_win_menuitem item, dword index, dword topindex,
							 dword max_height)
{
	char outstr[256];
	const char *fname;

#ifdef ENABLE_MUSIC
	fname = mp3_list_get(index);
#else
	fname = _("�����ѹر�");
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
	t_win_menuitem item[mp3_list_count()];

	if (item == NULL)
		return;

	win_menu_predraw_data prev;

	memcpy(&prev, &g_predraw, sizeof(prev));

	dword i;

	if (strcmp(simple_textdomain(NULL), "zh_CN") == 0)
		g_predraw.max_item_len = WRR * 4;
	else
		g_predraw.max_item_len = WRR * 5;

	for (i = 0; i < mp3_list_count(); i++) {
		char *rname = strrchr(mp3_list_get(i), '/');

		if (rname == NULL)
			rname = (char *) mp3_list_get(i);
		else
			rname++;
		if (strlen(rname) <= g_predraw.max_item_len - 2)
			STRCPY_S(item[i].name, rname);
		else {
			mbcsncpy_s((unsigned char *) item[i].name,
					   g_predraw.max_item_len - 3,
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

	g_predraw.item_count = 12;
	g_predraw.x = 240;
	g_predraw.y = 130;
	g_predraw.left =
		g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2 / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	dword index = 0;

	while ((index =
			win_menu(g_predraw.left, g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, mp3_list_count(), index, 0,
					 RGB(0x40, 0x40, 0x28), true, scene_mp3_list_predraw,
					 scene_mp3_list_postdraw,
					 scene_mp3_list_menucb)) != INVALID)
		if (mp3_list_count() == 0)
			break;
	memcpy(&g_predraw, &prev, sizeof(g_predraw));
#endif
}

const char *get_week_str(int day)
{
	static char *week_str[] = {
		"������",
		"����һ",
		"���ڶ�",
		"������",
		"������",
		"������",
		"������",
		"���ڴ���"
	};

	if (day < 0 || day > 7) {
		day = 7;
	}

	return _(week_str[day]);
}

static void scene_mp3bar_delay_action()
{
	extern bool prx_loaded;

	if (config.dis_scrsave)
		scePowerTick(0);
	if (prx_loaded) {
		xrSetBrightness(config.brightness);
	}
}

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
static void draw_lyric_string(const char **ly, dword lidx, dword * ss,
							  dword cpl)
{
	char t[BUFSIZ];

	if (ly[lidx] != NULL)
		return;
	if (ss[lidx] > cpl)
		ss[lidx] = cpl;

	lyric_decode(ly[lidx], t, &ss[lidx]);
	disp_putnstring(6 + (cpl - ss[lidx]) * DISP_FONTSIZE / 4,
					136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) +
					(DISP_FONTSIZE + 1) * lidx + 1,
					(lidx == config.lyricex) ? COLOR_WHITE : RGB(0x7F, 0x7F,
																 0x7F),
					(const byte *) t, ss[lidx], 0, 0, DISP_FONTSIZE, 0);
}
#endif

static void scene_draw_lyric(void)
{
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	disp_rectangle(5, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex), 474,
				   136 + (DISP_FONTSIZE + 1) * config.lyricex, COLOR_WHITE);
	disp_fillrect(6, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) + 1,
				  473, 136 + (DISP_FONTSIZE + 1) * config.lyricex - 1,
				  config.usedyncolor ? get_bgcolor_by_time() : config.
				  msgbcolor);
	const char *ly[config.lyricex * 2 + 1];
	dword ss[config.lyricex * 2 + 1];

	if (lyric_get_cur_lines(mp3_get_lyric(), config.lyricex, ly, ss)) {
		int lidx;
		dword cpl = 936 / DISP_FONTSIZE;

		for (lidx = 0; lidx < config.lyricex * 2 + 1; lidx++) {
			draw_lyric_string(ly, lidx, ss, cpl);
		}
	}
#endif
}

#ifdef ENABLE_MUSIC
static void scene_draw_mp3bar_music_staff(void)
{
	int bitrate, sample, len, tlen;
	char infostr[80];

	disp_rectangle(5, 262 - DISP_FONTSIZE * 4, 474, 267, COLOR_WHITE);
	disp_fillrect(6, 263 - DISP_FONTSIZE * 4, 473, 266,
				  config.usedyncolor ? get_bgcolor_by_time() : config.
				  msgbcolor);
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
				   _("  SELECT �༭�б�   �����ٺ���   ������ǰ��"));
	SPRINTF_S(infostr, "%s  LRC  %s  ID3v1 %s",
			  _("  SELECT �༭�б�   �����ٺ���   ������ǰ��"),
			  conf_get_encodename(config.lyricencode),
			  conf_get_encodename(config.mp3encode));
	disp_putnstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE,
					(const byte *) infostr,
					(468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0,
					DISP_FONTSIZE, 0);

	disp_putstring(6 + DISP_FONTSIZE, 264 - DISP_FONTSIZE * 3, COLOR_WHITE,
				   (const byte *)
				   _("�𲥷�/��ͣ ��ѭ�� ��ֹͣ ����������  L��һ��  R��һ��"));
	disp_putnstring(6 + DISP_FONTSIZE, 266 - DISP_FONTSIZE, COLOR_WHITE,
					(const byte *) mp3_get_tag(),
					(468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0,
					DISP_FONTSIZE, 0);
}
#endif

static void scene_draw_mp3bar(bool * firstdup)
{
	if (*firstdup) {
		disp_duptocachealpha(50);
		*firstdup = false;
	} else
		disp_duptocache();

	char infostr[80];
	pspTime tm;
	dword cpu, bus;
	dword pos;
	int percent, lifetime, tempe, volt;

	disp_rectangle(5, 5, 474, 8 + DISP_FONTSIZE * 2, COLOR_WHITE);
	disp_fillrect(6, 6, 473, 7 + DISP_FONTSIZE * 2,
				  config.usedyncolor ? get_bgcolor_by_time() : config.
				  msgbcolor);

	sceRtcGetCurrentClockLocalTime(&tm);
	pos = sceRtcGetDayOfWeek(tm.year, tm.month, tm.day);
	pos = pos >= 7 ? 7 : pos;
	power_get_clock(&cpu, &bus);
	SPRINTF_S(infostr,
			  _("%u��%u��%u��  %s  %02u:%02u:%02u   CPU/BUS: %d/%d"),
			  tm.year, tm.month, tm.day,
			  get_week_str(pos), tm.hour, tm.minutes, tm.seconds, (int) cpu,
			  (int) bus);
	disp_putstring(6 + DISP_FONTSIZE * 2, 6, COLOR_WHITE,
				   (const byte *) infostr);
	power_get_battery(&percent, &lifetime, &tempe, &volt);
	if (percent >= 0) {
		if (config.fontsize <= 12) {
			SPRINTF_S(infostr,
					  _
					  ("%s  ʣ�����: %d%%(%dСʱ%d����)  ����¶�: %d��   ʣ���ڴ�: %dKB"),
					  power_get_battery_charging(), percent, lifetime / 60,
					  lifetime % 60, tempe, get_free_mem() / 1024);
		} else {
			SPRINTF_S(infostr,
					  _("%s  ʣ�����: %d%%(%dСʱ%d����)  ����¶�: %d��"),
					  power_get_battery_charging(), percent, lifetime / 60,
					  lifetime % 60, tempe);
		}
	} else
		SPRINTF_S(infostr, _("[��Դ����]   ʣ���ڴ�: %dKB"),
				  get_free_mem() / 1024);
	if (strcmp(simple_textdomain(NULL), "en_US") == 0)
		disp_putstring(6, 7 + DISP_FONTSIZE, COLOR_WHITE,
					   (const byte *) infostr);
	else
		disp_putstring(6 + DISP_FONTSIZE, 7 + DISP_FONTSIZE, COLOR_WHITE,
					   (const byte *) infostr);

	scene_draw_lyric();

#ifdef ENABLE_MUSIC
	scene_draw_mp3bar_music_staff();
#endif
}

static int scene_mp3bar_handle_input(dword key, pixel ** saveimage)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox
				(_("�Ƿ��˳����?"), _("��"), _("��"), COLOR_WHITE,
				 COLOR_WHITE, config.msgbcolor)) {
				scene_exit();
				return 1;
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
			if (*saveimage != NULL) {
				disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
							  0, *saveimage);
				disp_flip();
				disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0,
							  0, *saveimage);
				free(*saveimage);
				*saveimage = NULL;
			} else {
				disp_waitv();
#ifdef ENABLE_BG
				if (!bg_display())
#endif
					disp_fillvram(0);
				disp_flip();
				disp_duptocache();
			}
			return 1;
	}

	return 0;
}

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
		scene_draw_mp3bar(&firstdup);
		disp_flip();
		dword key = ctrl_read();

		if (key != 0) {
			secticks = 0;
		}

		if (scene_mp3bar_handle_input(key, &saveimage) == 1)
			return;

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
		scene_mp3bar_delay_action();
	}
}
