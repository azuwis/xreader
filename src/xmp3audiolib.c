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
 * pspaudiolib.c - Audio library build on top of xrAudio, but to provide
 *                 multiple thread usage and callbacks.
 *
 * Copyright (c) 2005 Adresd
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 *
 * $Id: xmp3audiolib.c 58 2008-02-16 08:53:48Z hrimfaxi $
 */

#include <stdlib.h>
#include <string.h>
#include <pspthreadman.h>
#include <pspaudio.h>
#include <pspaudio_kernel.h>

#include "xmp3audiolib.h"
#include "xrhal.h"
#include "common/datatype.h"
#include <stdio.h>
#include "dbg.h"

#define THREAD_STACK_SIZE (128 * 1024)
#define WMA_AUDIO_SAMPLE_MAX (30720)

static bool g_useAudioChReserve = false;
static int g_AudioCh = -1;

int setFrequency(unsigned short samples, unsigned short freq, char car)
{
	if (!g_useAudioChReserve)
		return xrAudioSRCChReserve(samples, freq, car);
	else {
		// Only support 44100 now
		if (freq != 44100) {
			return -1;
		}

		if (g_AudioCh >= 0) {
			xrAudioChRelease(g_AudioCh);
		}

		g_AudioCh = xrAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, WMA_AUDIO_SAMPLE_MAX, PSP_AUDIO_FORMAT_STEREO);

		if (g_AudioCh < 0) {
			dbg_printf(d, "%s: xrAudioChReserve failed 0x%08x", __func__, g_AudioCh);
		}
		
		return g_AudioCh;
	}
}

int xMP3SetUseAudioChReserve(int use)
{
	int prev = (int)g_useAudioChReserve;

	g_useAudioChReserve = (bool)use;

	return prev;
}

int xMP3ReleaseAudio(void)
{
	if (!g_useAudioChReserve) {
		while (xrAudioOutput2GetRestSample() > 0);
		return xrAudioSRCChRelease();
	} else {
		int ret = 0;

		if (g_AudioCh >= 0) {
//			while (xrAudioGetChannelRestLength(g_AudioCh) > 0);

			ret = xrAudioChRelease(g_AudioCh);

			if (ret == 0) {
				g_AudioCh = -1;
			}
		}

		return ret;
	}
}

int audioOutpuBlocking(int volume, void *buffer)
{
	if (!g_useAudioChReserve) {
		return xrAudioSRCOutputBlocking(volume, buffer);
	} else {
		return xrAudioOutputPannedBlocking(g_AudioCh, volume, volume, buffer);
	}
}

static int audio_ready = 0;
static short audio_sndbuf[PSP_NUM_AUDIO_CHANNELS][2][WMA_AUDIO_SAMPLE_MAX][2];
static psp_audio_channelinfo AudioStatus[PSP_NUM_AUDIO_CHANNELS];
static volatile int audio_terminate = 0;

void xMP3AudioSetVolume(int channel, int left, int right)
{
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;
	AudioStatus[channel].volumeright = right;
	AudioStatus[channel].volumeleft = left;
}

void xMP3AudioChannelThreadCallback(int channel, void *buf, unsigned int reqn)
{
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;
	xMP3AudioCallback_t callback;

	callback = AudioStatus[channel].callback;
}

void xMP3AudioSetChannelCallback(int channel, xMP3AudioCallback_t callback,
								 void *pdata)
{
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;
	volatile psp_audio_channelinfo *pci = &AudioStatus[channel];

	pci->callback = 0;
	pci->pdata = pdata;
	pci->callback = callback;
}

int xMP3AudioOutBlocking(unsigned int channel, unsigned int vol1,
						 unsigned int vol2, void *buf)
{
	if (!audio_ready)
		return -1;
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return -1;
	if (vol1 > PSP_VOLUME_MAX)
		vol1 = PSP_VOLUME_MAX;
	if (vol2 > PSP_VOLUME_MAX)
		vol2 = PSP_VOLUME_MAX;
	return audioOutpuBlocking(vol1, buf);
}

static SceUID play_sema = -1;

static int AudioChannelThread(int args, void *argp)
{
	volatile int bufidx = 0;
	int channel = *(int *) argp;

	AudioStatus[channel].threadactive = 1;
	while (audio_terminate == 0) {
		void *bufptr = &audio_sndbuf[channel][bufidx];
		xMP3AudioCallback_t callback;
		int size;

		size = g_useAudioChReserve ? WMA_AUDIO_SAMPLE_MAX : PSP_NUM_AUDIO_SAMPLES;

		callback = AudioStatus[channel].callback;
		if (callback) {

			int ret = callback(bufptr, size,
							   AudioStatus[channel].pdata);

			if (ret != 0) {
				break;
			}
		} else {
			unsigned int *ptr = bufptr;
			int i;

			for (i = 0; i < size; ++i)
				*(ptr++) = 0;
		}
		//xMP3AudioOutBlocking(channel,AudioStatus[channel].volumeleft,AudioStatus[channel].volumeright,bufptr);
		xrKernelWaitSema(play_sema, 1, 0);
		audioOutpuBlocking(AudioStatus[0].volumeright, bufptr);
		xrKernelSignalSema(play_sema, 1);
		bufidx = (bufidx ? 0 : 1);
	}
	AudioStatus[channel].threadactive = 0;
	xrKernelExitThread(0);
	return 0;
}

int xMP3AudioSetFrequency(unsigned short freq)
{
	int ret = 0;

	switch (freq) {
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 32000:
		case 48000:
		case 11025:
		case 22050:
		case 44100:
			break;
		default:
			return -1;
	}
	xrKernelWaitSema(play_sema, 1, 0);
	xMP3ReleaseAudio();
	if (setFrequency(PSP_NUM_AUDIO_SAMPLES, freq, 2) < 0)
		ret = -1;
	xrKernelSignalSema(play_sema, 1);
	return ret;
}

int xMP3AudioInit()
{
	int i, ret;
	int failed = 0;
	char str[32];

	xMP3ReleaseAudio();
	audio_terminate = 0;
	audio_ready = 0;
	memset(audio_sndbuf, 0, sizeof(audio_sndbuf));

	if (play_sema < 0) {
		play_sema = xrKernelCreateSema("play_sema", 6, 1, 1, 0);
	}
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		AudioStatus[i].handle = -1;
		AudioStatus[i].threadhandle = -1;
		AudioStatus[i].threadactive = 0;
		AudioStatus[i].volumeright = PSP_VOLUME_MAX;
		AudioStatus[i].volumeleft = PSP_VOLUME_MAX;
		AudioStatus[i].callback = 0;
		AudioStatus[i].pdata = 0;
	}
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (xMP3AudioSetFrequency(44100) < 0)
			failed = 1;
		else
			AudioStatus[i].handle = 0;
	}
	if (failed) {
		for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
			if (AudioStatus[i].handle != -1)
				xMP3ReleaseAudio();
			AudioStatus[i].handle = -1;
		}
		return -1;
	}
	audio_ready = 1;
	strcpy(str, "audiot0");
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		str[6] = '0' + i;
		//AudioStatus[i].threadhandle = xrKernelCreateThread(str,(void*)&AudioChannelThread,0x12,0x10000,0,NULL);
		//xrAudioSetChannelDataLen(i, PSP_NUM_AUDIO_SAMPLES);
		AudioStatus[i].threadhandle =
			xrKernelCreateThread(str, (void *) &AudioChannelThread, 0x12,
								 THREAD_STACK_SIZE, PSP_THREAD_ATTR_USER, NULL);
		if (AudioStatus[i].threadhandle < 0) {
			AudioStatus[i].threadhandle = -1;
			failed = 1;
			break;
		}
		ret = xrKernelStartThread(AudioStatus[i].threadhandle, sizeof(i), &i);
		if (ret != 0) {
			failed = 1;
			break;
		}
	}
	if (failed) {
		audio_terminate = 1;
		for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
			if (AudioStatus[i].threadhandle != -1) {
				//xrKernelWaitThreadEnd(AudioStatus[i].threadhandle,NULL);
				while (AudioStatus[i].threadactive)
					xrKernelDelayThread(100000);
				xrKernelDeleteThread(AudioStatus[i].threadhandle);
			}
			AudioStatus[i].threadhandle = -1;
		}
		audio_ready = 0;
		return -1;
	}
	return 0;
}

void xMP3AudioEndPre()
{
	audio_ready = 0;
	audio_terminate = 1;
}

void xMP3AudioEnd()
{
	int i;

	audio_ready = 0;
	audio_terminate = 1;

	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (AudioStatus[i].threadhandle != -1) {
			xMP3ReleaseAudio();
			//xrKernelWaitThreadEnd(AudioStatus[i].threadhandle,NULL);
			while (AudioStatus[i].threadactive)
				xrKernelDelayThread(100000);
			xrKernelDeleteThread(AudioStatus[i].threadhandle);
		}
		AudioStatus[i].threadhandle = -1;
	}

	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (AudioStatus[i].handle != -1) {
			xMP3ReleaseAudio();
			AudioStatus[i].handle = -1;
		}
	}
	if (play_sema >= 0) {
		xrKernelDeleteSema(play_sema);
		play_sema = -1;
	}

	g_useAudioChReserve = false;
}

/**
 * 清空声音缓冲区
 *
 * @param buf 声音缓冲区指针
 * @param frames 帧数大小
 */
void xMP3ClearSndBuf(void *buf, int frames)
{
	memset(buf, 0, frames * 2 * 2);
}
