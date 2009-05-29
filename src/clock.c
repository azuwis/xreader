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

#include <pspkernel.h>
#include <psppower.h>
#include <pspsdk.h>
#include "scene.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

int getCpuClock()
{
	return xrPowerGetCpuClockFrequency();
}

void setBusClock(int bus)
{
	if (bus >= 54 && bus <= 111 && xrKernelDevkitVersion() < 0x03070110)
		xrPowerSetBusClockFrequency(bus);
}

void xrSetCpuClock(int cpu, int bus)
{
	if (xrKernelDevkitVersion() < 0x03070110) {
		xrPowerSetCpuClockFrequency(cpu);
		if (xrPowerGetCpuClockFrequency() < cpu)
			xrPowerSetCpuClockFrequency(++cpu);
	} else {
		xrPowerSetClockFrequency(cpu, cpu, cpu / 2);
		if (xrPowerGetCpuClockFrequency() < cpu) {
			cpu++;
			xrPowerSetClockFrequency(cpu, cpu, cpu / 2);
		}
	}
}
