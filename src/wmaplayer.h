/*
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

//    xMP3
//    Copyright (C) 2008 Hrimfaxi
//    outmatch@gmail.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

//
//    $Id: wmaplayer.h 65 2008-02-27 17:49:43Z hrimfaxi $
//

#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "wmadec.h"

#define WMA_BUFFER_SIZE WMA_MAX_BUF_SIZE

void WMA_Init(int channel);
int WMA_Load(char *filename);
int WMA_Play();
void WMA_Pause();
int WMA_Stop();
int WMA_EndOfStream();
void WMA_End();
struct fileInfo WMA_GetTagInfoOnly(char *filename);
struct fileInfo WMA_GetInfo();
int WMA_GetPercentage();
int WMA_isFilterSupported();
int WMA_suspend();
int WMA_resume();
int WMA_setMute(int onOff);
void WMA_GetTimeString(char *dest);
void WMA_fadeOut(float seconds);
int WMA_getPlayingSpeed();
int WMA_setPlayingSpeed(int playingSpeed);
void WMA_setVolumeBoostType(char *boostType);
void WMA_setVolumeBoost(int boost);
int WMA_getVolumeBoost();
int WMA_GetStatus();
int WMA_setFilter(double tFilter[32], int copyFilter);
void WMA_enableFilter();
void WMA_disableFilter();
int WMA_isFilterEnabled();
void WMA_Forward(float seconds);
void WMA_Backward(float seconds);
mad_timer_t WMA_getTimer(void);
