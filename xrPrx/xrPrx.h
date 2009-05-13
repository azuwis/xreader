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

#ifndef XR_PRX
#define XR_PRX

#ifdef __cplusplus
extern "C" {
#endif

#include <psploadexec_kernel.h>

void xrPlayerSetSpeed(int cpu, int bus);

/**
  * Set audio sampling frequency
  *
  * @param frequency - Sampling frequency to set audio output to - either 44100 or 48000.
  *
  * @returns 0 on success, an error if less than 0.
  */
int cooleyesAudioSetFrequency(int devkitVersion, int frequency);

int cooleyesMeBootStart(int devkitVersion, int mebooterType);

#ifdef __cplusplus
}
#endif

#endif
