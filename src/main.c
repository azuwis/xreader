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
#include <pspsdk.h>
#include <psppower.h>
#include <pspdisplay.h>
#include "fat.h"
#include "conf.h"
#include "scene.h"
#include "conf.h"
#include "display.h"
#include "ctrl.h"
#include "power.h"
#include "xrhal.h"
#include "dbg.h"

PSP_MODULE_INFO("XREADER", 0x0200, 1, 6);
PSP_MAIN_THREAD_PARAMS(45, 256, PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();

static unsigned int lock_count = 0;
static unsigned int intr_flags = 0;

/**
 * malloc lock for malloc-2.6.4 in newlib-0.17,
 * in malloc-2.8.4 newlib port we use our own lock,
 * so these code may be not used at all
 */
void __malloc_lock(struct _reent *ptr)
{
	unsigned int flags = pspSdkDisableInterrupts();

	if (lock_count == 0) {
		intr_flags = flags;
	}

	lock_count++;
}

void __malloc_unlock(struct _reent *ptr)
{
	if (--lock_count == 0) {
		pspSdkEnableInterrupts(intr_flags);
	}
}

static int power_callback(int arg1, int powerInfo, void *arg)
{
	dbg_printf(d, "%s: arg1 0x%08x powerInfo 0x%08x", __func__, arg1, powerInfo);

	if ((powerInfo & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_STANDBY)) > 0) {
		power_down();
	} else if ((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) > 0) {
		xrKernelDelayThread(1500000);
		power_up();
	}

	return 0;
}

static int exit_callback(int arg1, int arg2, void *arg)
{
	extern bool xreader_scene_inited;

	while (xreader_scene_inited == false) {
		xrKernelDelayThread(1);
	}

	scene_exit();
	xrPowerUnregisterCallback(0);
	xrKernelExitGame();

	return 0;
}

static int CallbackThread(unsigned int args, void *argp)
{
	int cbid;

	cbid = xrKernelCreateCallback("Exit Callback", exit_callback, NULL);
	xrKernelRegisterExitCallback(cbid);
	xrPowerUnregisterCallback(0);
	cbid = xrKernelCreateCallback("Power Callback", power_callback, NULL);
	xrPowerRegisterCallback(0, cbid);
	xrKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
static int SetupCallbacks(void)
{
	int thid = xrKernelCreateThread("Callback Thread", CallbackThread, 0x11,
									0x10000,
									0, 0);

	if (thid >= 0) {
		xrKernelStartThread(thid, 0, 0);
	}

	return thid;
}

static int main_thread(unsigned int args, void *argp)
{
	scene_init();
	return 0;
}

int main(int argc, char *argv[])
{
	SetupCallbacks();

	int thid = xrKernelCreateThread("User Thread", main_thread, 45, 0x40000,
									PSP_THREAD_ATTR_USER, NULL);

	if (thid < 0)
		xrKernelSleepThread();

	xrKernelStartThread(thid, 0, NULL);
	xrKernelSleepThread();

	return 0;
}
