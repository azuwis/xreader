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

#include <string.h>
#include <psppower.h>
#include <pspsdk.h>
#include "common/utils.h"
#include "xrPrx/xrPrx.h"
#include "power.h"
#include "simple_gettext.h"
#include "conf.h"
#include "musicmgr.h"
#include "fat.h"
#include "display.h"
#include "scene.h"
#include "display.h"
#include "ttfont.h"
#include "xrhal.h"

bool use_prx_power_save = false;

extern void power_set_clock(dword cpu, dword bus)
{
	if (use_prx_power_save) {
		xrPlayerSetSpeed(cpu, bus);
		// 15Mhz can't use xrPlayerSetSpeed
		if (cpu <= 15) {
			power_set_clock(33, 16);
			xrPowerSetCpuClockFrequency(cpu);
			xrPowerSetBusClockFrequency(bus);
		}
	} else {
		if (cpu > 222 || bus > 111)
			xrPowerSetClockFrequency(cpu, cpu, bus);
		else {
			xrPowerSetClockFrequency(222, 222, 111);
			xrPowerSetCpuClockFrequency(cpu);
			xrPowerSetBusClockFrequency(bus);
		}
	}
}

extern void power_get_clock(dword * cpu, dword * bus)
{
	*cpu = xrPowerGetCpuClockFrequency();
	*bus = xrPowerGetBusClockFrequency();
}

extern void power_get_battery(int *percent, int *lifetime, int *tempe,
							  int *volt)
{
	int t;

	t = xrPowerGetBatteryLifePercent();
	if (t >= 0)
		*percent = t;
	else
		*percent = 0;
	t = xrPowerGetBatteryLifeTime();
	if (t >= 0)
		*lifetime = t;
	else
		*lifetime = 0;
	t = xrPowerGetBatteryTemp();
	if (t >= 0)
		*tempe = t;
	else
		*tempe = 0;
	t = xrPowerGetBatteryVolt();
	if (t >= 0)
		*volt = xrPowerGetBatteryVolt();
	else
		*volt = 0;
}

static int last_status = 0;
static char status_str[256] = "";

extern const char *power_get_battery_charging(void)
{
	int status = xrPowerGetBatteryChargingStatus();

	if (last_status != status) {
		status_str[0] = 0;
		if ((status & PSP_POWER_CB_BATTPOWER) > 0)
			STRCAT_S(status_str, _("[电源充电]"));
		else if ((status & PSP_POWER_CB_AC_POWER) > 0)
			STRCAT_S(status_str, _("[电源供电]"));
		else if ((status & PSP_POWER_CB_BATTERY_LOW) > 0)
			STRCAT_S(status_str, _("[电量不足]"));
	}
	return status_str;
}

extern int use_ttf;
extern p_ttf cttf, ettf;

static volatile bool has_run_suspend = false;

extern void power_down(void)
{
	if (!has_run_suspend)
		has_run_suspend = true;
	else
		return;

#ifdef ENABLE_TTF
	if (use_ttf && !config.ttf_load_to_memory) {
		ttf_lock();
		disp_ttf_close();
	}
#endif
#ifdef ENABLE_MUSIC
	music_request_suspend();
#endif
	fat_powerdown();
}

extern void power_up(void)
{
	if (has_run_suspend)
		has_run_suspend = false;
	else
		return;

	fat_powerup();
#ifdef ENABLE_MUSIC
	music_request_resume();
#endif
#ifdef ENABLE_TTF
	if (use_ttf && !config.ttf_load_to_memory) {
		disp_ttf_reload();
		ttf_unlock();
	}
#endif
}
