/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _POWER_H_
#define _POWER_H_

#include "common/datatype.h"

extern void power_set_clock(dword cpu, dword bus);
extern void power_get_clock(dword * cpu, dword * bus);
extern void power_get_battery(int * percent, int * lifetime, int * tempe, int * volt);
extern const char * power_get_battery_charging();

#endif
