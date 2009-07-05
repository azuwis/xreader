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

#include <pspkernel.h>
#include <time.h>
#include "display.h"
#include "conf.h"
#ifdef ENABLE_MUSIC
#include "musicmgr.h"
#ifdef ENABLE_LYRIC
#include "lyric.h"
#endif
#endif
#include "ctrl.h"
#include "xrhal.h"
#include "thread_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define CTRL_REPEAT_TIME 0x40000
static unsigned int last_btn = 0;
static unsigned int last_tick = 0;

#ifdef ENABLE_HPRM
static bool hprmenable = false;
static u32 lasthprmkey = 0;
static u32 lastkhprmkey = 0;
static struct psp_mutex_t hprm_l;
#endif

extern void ctrl_init(void)
{
	xrCtrlSetSamplingCycle(0);
#ifdef ENABLE_ANALOG
	xrCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#else
	xrCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
#endif
#ifdef ENABLE_HPRM
	xr_lock_init(&hprm_l);
#endif
}

extern void ctrl_destroy(void)
{
#ifdef ENABLE_HPRM
	xr_lock_destroy(&hprm_l);
#endif
}

#ifdef ENABLE_ANALOG
extern bool ctrl_analog(int *x, int *y)
{
	SceCtrlData ctl;

	xrCtrlReadBufferPositive(&ctl, 1);

	if (ctl.Buttons & PSP_CTRL_HOLD) {
		return false;
	}

	*x = ((int) ctl.Lx) - 128;
	*y = ((int) ctl.Ly) - 128;

	if (*x < -63 || *x > 63 || *y < -63 || *y > 63) {
		return true;
	}

	return false;
}
#endif

extern dword ctrl_read_cont(void)
{
	SceCtrlData ctl;

	xrCtrlReadBufferPositive(&ctl, 1);

#ifdef ENABLE_HPRM
	if (hprmenable && xrHprmIsRemoteExist()) {
		u32 key;

		if (xr_lock(&hprm_l) >= 0) {
			xrHprmPeekCurrentKey(&key);
			xr_unlock(&hprm_l);

			if (key > 0) {
				switch (key) {
					case PSP_HPRM_FORWARD:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_FORWARD;
					case PSP_HPRM_BACK:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_BACK;
					case PSP_HPRM_PLAYPAUSE:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_PLAYPAUSE;
				}
			} else
				lastkhprmkey = 0;
		}
	}
#endif

	last_btn = ctl.Buttons;
	last_tick = ctl.TimeStamp;

	return last_btn;
}

extern dword ctrl_read(void)
{
	SceCtrlData ctl;

#ifdef ENABLE_HPRM
	if (hprmenable && xrHprmIsRemoteExist()) {
		u32 key;

		xrHprmPeekCurrentKey(&key);

		if (key > 0) {
			switch (key) {
				case PSP_HPRM_FORWARD:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_FORWARD;
				case PSP_HPRM_BACK:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_BACK;
				case PSP_HPRM_PLAYPAUSE:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_PLAYPAUSE;
			}
		} else
			lastkhprmkey = 0;
	}
#endif

	xrCtrlReadBufferPositive(&ctl, 1);

	if (ctl.Buttons == last_btn) {
		if (ctl.TimeStamp - last_tick < CTRL_REPEAT_TIME)
			return 0;
		return last_btn;
	}

	last_btn = ctl.Buttons;
	last_tick = ctl.TimeStamp;
	return last_btn;
}

extern void ctrl_waitreleaseintime(int i)
{
	SceCtrlData ctl;

	do {
		xrCtrlReadBufferPositive(&ctl, 1);
		xrKernelDelayThread(i);
	} while (ctl.Buttons != 0);
}

extern void ctrl_waitrelease(void)
{
	return ctrl_waitreleaseintime(20000);
}

extern int ctrl_waitreleasekey(dword key)
{
	SceCtrlData pad;

	xrCtrlReadBufferPositive(&pad, 1);
	while (pad.Buttons == key) {
		xrKernelDelayThread(50000);
		xrCtrlReadBufferPositive(&pad, 1);
	}

	return 0;
}

extern dword ctrl_waitany(void)
{
	dword key;

	while ((key = ctrl_read()) == 0) {
		xrKernelDelayThread(50000);
	}
	return key;
}

extern dword ctrl_waitkey(dword keymask)
{
	dword key;

	while ((key = ctrl_read()) != key) {
		xrKernelDelayThread(50000);
	}
	return key;
}

extern dword ctrl_waitmask(dword keymask)
{
	dword key;

	while (((key = ctrl_read()) & keymask) == 0) {
		xrKernelDelayThread(50000);
	}
	return key;
}

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
extern dword ctrl_waitlyric(void)
{
	dword key;

	while ((key = ctrl_read()) == 0) {
		xrKernelDelayThread(50000);
		if (lyric_check_changed(music_get_lyric()))
			break;
	}
	return key;
}
#endif

#ifdef ENABLE_HPRM
extern dword ctrl_hprm(void)
{
	u32 key = ctrl_hprm_raw();

	if (key == lasthprmkey)
		return 0;

	lasthprmkey = key;
	return (dword) key;
}

extern dword ctrl_hprm_raw(void)
{
	u32 key = 0;

/*	if(xrKernelDevkitVersion() >= 0x02000010)
		return 0;*/
	if (!xrHprmIsRemoteExist())
		return 0;

	if (xr_lock(&hprm_l) >= 0) {
		xrHprmPeekCurrentKey(&key);
		xr_unlock(&hprm_l);
	}

	return (dword) key;
}
#endif

extern dword ctrl_waittime(dword t)
{
	dword key;
	time_t t1 = time(NULL);

	while ((key = ctrl_read()) == 0) {
		xrKernelDelayThread(50000);
		if (time(NULL) - t1 >= t)
			return 0;
	}
	return key;
}

#ifdef ENABLE_HPRM
extern void ctrl_enablehprm(bool enable)
{
	hprmenable = /*(xrKernelDevkitVersion() < 0x02000010) && */ enable;
}
#endif
