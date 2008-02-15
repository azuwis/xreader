/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <string.h>
#include <psppower.h>
#include "common/utils.h"
#include "xrPrx/xrPrx.h"
#include "power.h"

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
	*percent = scePowerGetBatteryLifePercent();
	*lifetime = scePowerGetBatteryLifeTime();
	*tempe = scePowerGetBatteryTemp();
	*volt = scePowerGetBatteryVolt();
}

static int last_status = 0;
static char status_str[256] = "";

extern const char *power_get_battery_charging()
{
	int status = scePowerGetBatteryChargingStatus();

	if (last_status != status) {
		status_str[0] = 0;
		if ((status & PSP_POWER_CB_BATTPOWER) > 0)
			STRCAT_S(status_str, "[电源充电]");
		else if ((status & PSP_POWER_CB_AC_POWER) > 0)
			STRCAT_S(status_str, "[电源供电]");
		else if ((status & PSP_POWER_CB_BATTERY_LOW) > 0)
			STRCAT_S(status_str, "[电量不足]");
	}
	return status_str;
}
