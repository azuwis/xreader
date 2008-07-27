#ifndef _POWER_H_
#define _POWER_H_

#include "common/datatype.h"

extern void power_set_clock(dword cpu, dword bus);
extern void power_get_clock(dword * cpu, dword * bus);
extern void power_get_battery(int *percent, int *lifetime, int *tempe,
							  int *volt);
extern const char *power_get_battery_charging(void);
extern void power_down(void);
extern void power_up(void);

#endif
