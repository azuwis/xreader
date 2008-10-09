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
//    $Id: oggplayer.h 52 2008-02-16 06:33:43Z hrimfaxi $
//

#pragma once

extern int OGG_defaultCPUClock;

/// private functions
void OGG_Init(int channel);
int OGG_Play();
void OGG_Pause();
int OGG_Stop();
void OGG_End();
void OGG_FreeTune();
int OGG_Load(char *filename);
void OGG_GetTimeString(char *dest);
int OGG_EndOfStream();
struct fileInfo OGG_GetInfo();
struct fileInfo OGG_GetTagInfoOnly(char *filename);
int OGG_GetStatus();
int OGG_GetPercentage();
void OGG_setVolumeBoostType(char *boostType);
void OGG_setVolumeBoost(int boost);
int OGG_getVolumeBoost();
int OGG_getPlayingSpeed();
int OGG_setPlayingSpeed(int playingSpeed);
int OGG_setMute(int onOff);
void OGG_fadeOut(float seconds);

/// Functions for filter (equalizer): 
int OGG_setFilter(double tFilter[32], int copyFilter);
void OGG_enableFilter();
void OGG_disableFilter();
int OGG_isFilterEnabled();
int OGG_isFilterSupported();

/// Manage suspend:
int OGG_suspend();
int OGG_resume();
mad_timer_t OGG_getTimer();
