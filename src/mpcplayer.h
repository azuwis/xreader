//    Copyright (C) 2008 hrimfaxi
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

//
//    $Id: mpcplayer.h 64 2008-02-27 17:32:07Z hrimfaxi $
//

#pragma once

#include <stdio.h>
#include <stdlib.h>

void MPC_Init(int channel);
int MPC_Load(char *filename);
int MPC_Play();
void MPC_Pause();
int MPC_Stop();
int MPC_EndOfStream();
void MPC_End();
struct fileInfo MPC_GetTagInfoOnly(char *filename);
struct fileInfo MPC_GetInfo();
int MPC_GetPercentage();
int MPC_isFilterSupported();
int MPC_suspend();
int MPC_resume();
int MPC_setMute(int onOff);
void MPC_GetTimeString(char *dest);
void MPC_fadeOut(float seconds);
int MPC_getPlayingSpeed();
int MPC_setPlayingSpeed(int playingSpeed);
void MPC_setVolumeBoostType(char *boostType);
void MPC_setVolumeBoost(int boost);
int MPC_getVolumeBoost();
int MPC_GetStatus();
int MPC_setFilter(double tFilter[32], int copyFilter);
void MPC_enableFilter();
void MPC_disableFilter();
int MPC_isFilterEnabled();
void MPC_Forward(float seconds);
void MPC_Backward(float seconds);

mad_timer_t MPC_getTimer();
int mpc_init(void);
