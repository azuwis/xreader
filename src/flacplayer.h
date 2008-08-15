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
//    $Id: flacplayer.h 52 2008-02-16 06:33:43Z hrimfaxi $
//

#pragma once

extern int FLAC_defaultCPUClock;

/// private functions
void FLAC_Init(int channel);
int FLAC_Play();
void FLAC_Pause();
int FLAC_Stop();
void FLAC_End();
void FLAC_FreeTune();
int FLAC_Load(char *filename);
void FLAC_GetTimeString(char *dest);
int FLAC_EndOfStream();
struct fileInfo FLAC_GetInfo();
struct fileInfo FLAC_GetTagInfoOnly(char *filename);
int FLAC_GetStatus();
int FLAC_GetPercentage();
void FLAC_setVolumeBoostType(char *boostType);
void FLAC_setVolumeBoost(int boost);
int FLAC_getVolumeBoost();
int FLAC_getPlayingSpeed();
int FLAC_setPlayingSpeed(int playingSpeed);
int FLAC_setMute(int onOff);
void FLAC_fadeOut(float seconds);

/// Functions for filter (equalizer):
int FLAC_setFilter(double tFilter[32], int copyFilter);
void FLAC_enableFilter();
void FLAC_disableFilter();
int FLAC_isFilterEnabled();
int FLAC_isFilterSupported();

/// Manage suspend:
int FLAC_suspend();
int FLAC_resume();
mad_timer_t FLAC_getTimer(void);
