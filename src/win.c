/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include <pspdebug.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "common/utils.h"
#include "display.h"
#include "ctrl.h"
#include "win.h"
#include "pspscreen.h"
#include <stdio.h>
#include "dbg.h"
#include "conf.h"
#include "text.h"
#include "power.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static volatile int secticks = 0;

extern t_win_menu_op win_menu_defcb(dword key, p_win_menuitem item,
									dword * count, dword max_height,
									dword * topindex, dword * index)
{
	switch (key) {
		case PSP_CTRL_UP:
			if (*index == 0)
				*index = *count - 1;
			else
				(*index)--;
			return win_menu_op_redraw;
		case PSP_CTRL_DOWN:
			if (*index == *count - 1)
				*index = 0;
			else
				(*index)++;
			return win_menu_op_redraw;
		case PSP_CTRL_LEFT:
			if (*index < max_height - 1)
				*index = 0;
			else
				*index -= max_height - 1;
			return win_menu_op_redraw;
		case PSP_CTRL_RIGHT:
			if (*index + (max_height - 1) >= *count)
				*index = *count - 1;
			else
				*index += max_height - 1;
			return win_menu_op_redraw;
		case PSP_CTRL_LTRIGGER:
			*index = 0;
			return win_menu_op_redraw;
		case PSP_CTRL_RTRIGGER:
			*index = *count - 1;
			return win_menu_op_redraw;
		case PSP_CTRL_CIRCLE:
			ctrl_waitrelease();
			return win_menu_op_ok;
		case PSP_CTRL_CROSS:
			ctrl_waitrelease();
			return win_menu_op_cancel;
	}
	return win_menu_op_continue;
}

static void win_menu_delay_action(void)
{
	if (config.dis_scrsave)
		xrPowerTick(0);
}

extern dword win_menu(dword x, dword y, dword max_width, dword max_height,
					  p_win_menuitem item, dword count, dword initindex,
					  dword linespace, pixel bgcolor, bool redraw,
					  t_win_menu_draw predraw, t_win_menu_draw postdraw,
					  t_win_menu_callback cb)
{
	dword i, index = initindex, topindex, botindex, lastsel = index;
	bool needrp = true;
	bool firstdup = true;
	pixel *saveimage = NULL;
	u64 timer_start, timer_end;

	secticks = 0;

	if (cb == NULL)
		cb = win_menu_defcb;

	if (redraw) {
		saveimage =
			(pixel *) memalign(16,
							   PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT *
							   sizeof(pixel));
		if (saveimage)
			disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
		disp_duptocachealpha(50);
	}

	topindex = (index >= max_height) ? (index - max_height + 1) : 0;
	botindex =
		(topindex + max_height >
		 count) ? (count - 1) : (topindex + max_height - 1);

	xrRtcGetCurrentTick(&timer_start);
	while (1) {
		t_win_menu_op op;
		dword key;

		disp_waitv();
		if (predraw != NULL)
			predraw(item, index, topindex, max_height);
		if (needrp) {
			disp_fillrect(x, y,
						  x + 2 + max_width * (DISP_FONTSIZE / 2),
						  y + 2 + DISP_FONTSIZE + (max_height -
												   1) * (1 +
														 DISP_FONTSIZE
														 + linespace), bgcolor);
			for (i = topindex; i <= botindex; i++)
				if (i != index)
					disp_putstring(x + 1,
								   y + 1 + (i -
											topindex) *
								   (DISP_FONTSIZE + 1 +
									linespace),
								   item[i].selected ? item[i].selicolor :
								   item[i].icolor, (const byte *) item[i].name);
			if (max_height < count) {
				dword sbh =
					2 + DISP_FONTSIZE + (max_height - 1) * (1 +
															DISP_FONTSIZE
															+ linespace);
				disp_line(x - 1 +
						  max_width * (DISP_FONTSIZE / 2), y,
						  x - 1 +
						  max_width * (DISP_FONTSIZE / 2),
						  y + sbh, COLOR_WHITE);
				disp_fillrect(x +
							  max_width * (DISP_FONTSIZE / 2),
							  y + sbh * topindex / count,
							  x + 2 +
							  max_width * (DISP_FONTSIZE / 2),
							  y + sbh * (botindex + 1) / count, COLOR_WHITE);
			}
			needrp = false;
		} else {
			disp_rectduptocache(x, y,
								x + 2 +
								max_width * (DISP_FONTSIZE / 2),
								y + 2 + DISP_FONTSIZE +
								(max_height - 1) * (DISP_FONTSIZE +
													1 + linespace));
			if (item[lastsel].selrcolor != bgcolor
				|| item[lastsel].selbcolor != bgcolor)
				disp_fillrect(x,
							  y + (lastsel -
								   topindex) * (DISP_FONTSIZE +
												1 + linespace),
							  x + 1 +
							  DISP_FONTSIZE / 2 *
							  item[lastsel].width,
							  y + DISP_FONTSIZE + 1 + (lastsel - topindex)
							  * (DISP_FONTSIZE + 1 + linespace), bgcolor);
			disp_putstring(x + 1,
						   y + 1 + (lastsel -
									topindex) * (DISP_FONTSIZE + 1 +
												 linespace),
						   item[lastsel].selected ? item[lastsel].
						   selicolor : item[lastsel].icolor,
						   (const byte *) item[lastsel].name);
		}
		if (item[index].selrcolor != bgcolor)
			disp_rectangle(x,
						   y + (index - topindex) * (DISP_FONTSIZE +
													 1 + linespace),
						   x + 1 +
						   DISP_FONTSIZE / 2 * item[index].width,
						   y + DISP_FONTSIZE + 1 + (index -
													topindex) *
						   (DISP_FONTSIZE + 1 + linespace),
						   item[index].selrcolor);
		if (item[index].selbcolor != bgcolor)
			disp_fillrect(x + 1,
						  y + 1 + (index -
								   topindex) * (DISP_FONTSIZE + 1 +
												linespace),
						  x + DISP_FONTSIZE / 2 * item[index].width,
						  y + DISP_FONTSIZE + (index -
											   topindex) *
						  (DISP_FONTSIZE + 1 + linespace),
						  item[index].selbcolor);
		disp_putstring(x + 1,
					   y + 1 + (index - topindex) * (DISP_FONTSIZE + 1 +
													 linespace),
					   item[index].selected ? item[index].
					   selicolor : item[index].icolor,
					   (const byte *) item[index].name);
		if (postdraw != NULL)
			postdraw(item, index, topindex, max_height);
		disp_flip();

		if (firstdup) {
			disp_duptocache();
			firstdup = false;
		}

		lastsel = index;

		while ((key = ctrl_read()) == 0) {
			xrRtcGetCurrentTick(&timer_end);
			if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
				xrRtcGetCurrentTick(&timer_start);
				secticks++;
			}
			if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
				power_down();
				xrPowerRequestSuspend();
				secticks = 0;
			}
			xrKernelDelayThread(20000);
			win_menu_delay_action();
		}
		if (key != 0) {
			secticks = 0;
		}
		while ((op =
				cb(key, item, &count, max_height, &topindex,
				   &index)) == win_menu_op_continue) {
			while ((key = ctrl_read()) == 0) {
				xrRtcGetCurrentTick(&timer_end);
				if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
					xrRtcGetCurrentTick(&timer_start);
					secticks++;
				}
				if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
					power_down();
					xrPowerRequestSuspend();
					secticks = 0;
				}
				xrKernelDelayThread(20000);
				win_menu_delay_action();
			}
		}
		switch (op) {
			case win_menu_op_ok:
				if (saveimage) {
					disp_putimage(0, 0, PSP_SCREEN_WIDTH,
								  PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH,
								  PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					free(saveimage);
				}
				return index;
			case win_menu_op_cancel:
				if (saveimage) {
					disp_putimage(0, 0, PSP_SCREEN_WIDTH,
								  PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH,
								  PSP_SCREEN_HEIGHT, 0, 0, saveimage);
					free(saveimage);
				}
				return INVALID;
			case win_menu_op_force_redraw:
				needrp = true;
			default:;
		}
		if (index > botindex) {
			topindex = index - max_height + 1;
			botindex = index;
			needrp = true;
		} else if (index < topindex) {
			topindex = index;
			botindex = topindex + max_height - 1;
			needrp = true;
		}
	}
	free(saveimage);
	return INVALID;
}

extern bool win_msgbox(const char *prompt, const char *yesstr,
					   const char *nostr, pixel fontcolor, pixel bordercolor,
					   pixel bgcolor)
{
	dword width = strlen(prompt) * DISP_FONTSIZE / 4;
	dword yeswidth = strlen(yesstr) * (DISP_FONTSIZE / 2);
	pixel *saveimage = (pixel *) memalign(16,
										  PSP_SCREEN_WIDTH *
										  PSP_SCREEN_HEIGHT * sizeof(pixel));
	bool result;

	if (saveimage)
		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
	disp_duptocachealpha(50);
	disp_rectangle(219 - width, 99, 260 + width, 173, bordercolor);
	disp_fillrect(220 - width, 100, 259 + width, 172, bgcolor);
	disp_putstring(240 - width, 115, fontcolor, (const byte *) prompt);
	disp_putstring(214 - yeswidth, 141, fontcolor, (const byte *) "��");
	disp_putstring(230 - yeswidth, 141, fontcolor, (const byte *) yesstr);
	disp_putstring(250, 141, fontcolor, (const byte *) "��");
	disp_putstring(266, 141, fontcolor, (const byte *) nostr);
	disp_flip();
	disp_duptocache();
	disp_rectduptocachealpha(219 - width, 99, 260 + width, 173, 50);
	result =
		(ctrl_waitmask(PSP_CTRL_CIRCLE | PSP_CTRL_CROSS) == PSP_CTRL_CIRCLE);
	if (saveimage) {
		disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
					  saveimage);
		disp_flip();
		disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
					  saveimage);
		free(saveimage);
	}
	return result;
}

extern void win_msg(const char *prompt, pixel fontcolor, pixel bordercolor,
					pixel bgcolor)
{
	dword width = strlen(prompt) * DISP_FONTSIZE / 4;
	pixel *saveimage = (pixel *) memalign(16,
										  PSP_SCREEN_WIDTH *
										  PSP_SCREEN_HEIGHT * sizeof(pixel));
	if (saveimage)
		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);
	disp_duptocachealpha(50);
	disp_rectangle(219 - width, 118, 260 + width, 154, bordercolor);
	disp_fillrect(220 - width, 119, 259 + width, 153, bgcolor);
	disp_putstring(240 - width, 132, fontcolor, (const byte *) prompt);
	disp_flip();
	disp_duptocache();
	disp_rectduptocachealpha(219 - width, 118, 260 + width, 154, 50);
	ctrl_waitany();
	if (saveimage) {
		disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
					  saveimage);
		disp_flip();
		disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0,
					  saveimage);
		free(saveimage);
	}
}

extern p_win_menuitem win_realloc_items(p_win_menuitem item, int orgsize,
										int newsize)
{
	int i;
	p_win_menuitem p;

	if (orgsize > newsize) {
		for (i = newsize; i < orgsize; ++i) {
			buffer_free(item[i].compname);
			buffer_free(item[i].shortname);
		}

		item = safe_realloc(item, sizeof(*item) * newsize);

		return item;
	}
	// Cannot use safe_realloc here
	p = realloc(item, sizeof(*item) * newsize);

	if (p == NULL) {
		for (i = 0; i < orgsize; ++i) {
			buffer_free(item[i].compname);
			buffer_free(item[i].shortname);
		}

		free(item);

		return p;
	}

	item = p;

	for (i = orgsize; i < newsize; ++i) {
		item[i].compname = buffer_init();
		item[i].shortname = buffer_init();
	}

	return item;
}

extern void win_item_destroy(p_win_menuitem * item, dword * size)
{
	int i;
	p_win_menuitem p = *item;

	if (item == NULL || *item == NULL || size == 0) {
		return;
	}

	for (i = 0; i < *size; ++i) {
		buffer_free(p[i].compname);
		buffer_free(p[i].shortname);
	}

	free(*item);
	*size = 0;
	*item = NULL;
}

extern p_win_menuitem win_copy_item(p_win_menuitem dst,
									const p_win_menuitem src)
{
	size_t i;

	if (dst == NULL || src == NULL)
		return NULL;

	buffer_copy_string(dst->compname, src->compname->ptr);
	buffer_copy_string(dst->shortname, src->shortname->ptr);
	STRCPY_S(dst->name, src->name);
	dst->width = src->width;
	dst->icolor = src->icolor;
	dst->selicolor = src->selicolor;
	dst->selrcolor = src->selrcolor;
	dst->selbcolor = src->selbcolor;
	dst->selected = src->selected;
	dst->data = src->data;

	for (i = 0; i < 4; ++i) {
		dst->data2[i] = src->data2[i];
	}
	dst->data3 = src->data3;

	return dst;
}

extern int win_get_max_length(const p_win_menuitem pItem, int size)
{
	int i, max = 0;

	if (pItem == NULL || size == 0)
		return 0;

	for (i = 0; i < size; ++i) {
		const char *str = pItem[i].name;
		int t = strlen(str);

		max = max > t ? max : t;
	}

	return max;
}

extern int win_get_max_pixel_width(const p_win_menuitem pItem, int size)
{
	int i, max = 0;

	if (pItem == NULL || size == 0)
		return 0;

	for (i = 0; i < size; ++i) {
		const char *str = pItem[i].name;
		int t = text_get_string_width_sys((const byte *) str, strlen(str),
										  1);

		max = max > t ? max : t;
	}

	return max;
}
