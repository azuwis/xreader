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

/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * pspaudiolib.h - Audio library build on top of sceAudio, but to provide
 *                 multiple thread usage and callbacks.
 *
 * Copyright (c) 2005 Adresd
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 *
 * $Id: xmp3audiolib.h 58 2008-02-16 08:53:48Z hrimfaxi $
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//I need just one channel, more CPU free:
#define PSP_NUM_AUDIO_CHANNELS 1
//#define PSP_NUM_AUDIO_CHANNELS 4

/** This is the number of frames you can update per callback, a frame being
 * 1 sample for mono, 2 samples for stereo etc. */
#define PSP_NUM_AUDIO_SAMPLES 1024*4
#define PSP_VOLUME_MAX 0x8000

	typedef void (*xMP3AudioCallback_t) (void *buf, unsigned int reqn,
										 void *pdata);

	typedef struct
	{
		int threadhandle;
		int handle;
		int volumeleft;
		int volumeright;
		xMP3AudioCallback_t callback;
		void *pdata;
		int threadactive;
	} psp_audio_channelinfo;

	typedef int (*xMP3AudioThreadfunc_t) (int args, void *argp);

	int xMP3AudioInit();
	void xMP3AudioEndPre();
	void xMP3AudioEnd();

	void xMP3AudioSetVolume(int channel, int left, int right);
	void xMP3AudioChannelThreadCallback(int channel, void *buf,
										unsigned int reqn);
	void xMP3AudioSetChannelCallback(int channel, xMP3AudioCallback_t callback,
									 void *pdata);
	int xMP3AudioOutBlocking(unsigned int channel, unsigned int vol1,
							 unsigned int vol2, void *buf);
	int xMP3AudioSetFrequency(unsigned short freq);

#ifdef __cplusplus
}
#endif
