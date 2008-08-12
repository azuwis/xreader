#include "config.h"

#include <string.h>
#include <psppower.h>
#include <pspsdk.h>
#include "common/utils.h"
#include "xrPrx/xrPrx.h"
#include "power.h"
#include "simple_gettext.h"
#include "conf.h"
#include "mp3.h"
#include "fat.h"
#include "display.h"
#include "scene.h"
#include "display.h"
#include "ttfont.h"

bool use_prx_power_save = false;

extern void power_set_clock(dword cpu, dword bus)
{
	if (use_prx_power_save) {
		xrPlayerSetSpeed(cpu, bus);
		// 15Mhz can't use xrPlayerSetSpeed
		if (cpu <= 15) {
			power_set_clock(33, 16);
			scePowerSetCpuClockFrequency(cpu);
			scePowerSetBusClockFrequency(bus);
		}
	} else {
		if (cpu > 222 || bus > 111)
			scePowerSetClockFrequency(cpu, cpu, bus);
		else {
			scePowerSetClockFrequency(222, 222, 111);
			scePowerSetCpuClockFrequency(cpu);
			scePowerSetBusClockFrequency(bus);
		}
	}
}

extern void power_get_clock(dword * cpu, dword * bus)
{
	*cpu = scePowerGetCpuClockFrequency();
	*bus = scePowerGetBusClockFrequency();
}

extern void power_get_battery(int *percent, int *lifetime, int *tempe,
							  int *volt)
{
	int t;

	t = scePowerGetBatteryLifePercent();
	if (t >= 0)
		*percent = t;
	else
		*percent = 0;
	t = scePowerGetBatteryLifeTime();
	if (t >= 0)
		*lifetime = t;
	else
		*lifetime = 0;
	t = scePowerGetBatteryTemp();
	if (t >= 0)
		*tempe = t;
	else
		*tempe = 0;
	t = scePowerGetBatteryVolt();
	if (t >= 0)
		*volt = scePowerGetBatteryVolt();
	else
		*volt = 0;
}

static int last_status = 0;
static char status_str[256] = "";

extern const char *power_get_battery_charging(void)
{
	int status = scePowerGetBatteryChargingStatus();

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

extern t_conf config;
extern int use_ttf;
extern p_ttf cttf, ettf;

extern void power_down(void)
{
#ifdef ENABLE_TTF
	if (use_ttf && !config.ttf_load_to_memory) {
		ttf_lock();
		disp_ttf_close();
	}
#endif
#ifdef ENABLE_MUSIC
	mp3_powerdown();
#endif
	fat_powerdown();
	scene_power_save(true);
}

extern void power_up(void)
{
	fat_powerup();
#ifdef ENABLE_MUSIC
	mp3_powerup();
#endif
#ifdef ENABLE_TTF
	if (use_ttf && !config.ttf_load_to_memory) {
		disp_ttf_reload();
		ttf_unlock();
	}
#endif
	scene_power_save(true);
}
