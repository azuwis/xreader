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

//
//    $Id: wmaplayer.c 65 2008-02-27 17:49:43Z hrimfaxi $
//

#include <string.h>
#include <stdio.h>

#include "player.h"
#include "id3.h"
#include "wmaplayer.h"

#define THREAD_PRIORITY 0x12

enum
{
	FALSE = 0,
	TRUE = 1
};

static signed short OutputBuffer[4][WMA_MAX_BUF_SIZE];
static int OutputBuffer_Index = 0;
static int OutputBuffer_Pos = 0;
static WmaFile *wma = NULL;
static int WMA_audio_channel = -1;
static int WMA_volume = PSP_AUDIO_VOLUME_MAX;
static int WMA_isPlaying = FALSE;
static struct fileInfo WMA_info;

static int WMA_thid = -1;
static int WMA_threadActive = FALSE;
static int WMA_threadExited = FALSE;
static volatile int WMA_eof = FALSE;

static int WMA_defaultCPUClock = 222;
static char WMA_fileName[256] = { 0 };

/*
 * @note
 * - -1 backward
 * - 0  normal playback
 * -  1 forward
 */
static volatile int WMA_jump = 0;
static volatile float g_seek_seconds = 0.0;

int WMA_decodeThread(SceSize args, void *argp)
{
	WMA_threadActive = TRUE;
	WMA_threadExited = FALSE;

	WMA_jump = 0;

	while (WMA_threadActive) {
		while (WMA_eof == FALSE && WMA_isPlaying) {
			if (WMA_jump < 0) {
				if (WMA_info.timer.seconds > 5) {
					wma_seek(wma, WMA_info.timer.seconds - 5);
					WMA_info.timer.seconds -= 5;
					WMA_info.timer.fraction = 0;
				} else {
					wma_seek(wma, 0);
					mad_timer_reset(&WMA_info.timer);
				}
				WMA_jump = 0;
			}
			if (WMA_jump > 0) {
				int newp = WMA_info.timer.seconds + 5;

				if (newp < wma->duration) {
					wma_seek(wma, newp);
					WMA_info.timer.seconds = newp;
					WMA_info.timer.fraction = 0;
				} else {
					// wma playback end...
					WMA_eof = TRUE;
					break;
				}
				WMA_jump = 0;
			}

			int size = 0, p = 0;
			unsigned char *buf;
			mad_timer_t incr;
			double tt =
				(double) (WMA_MAX_BUF_SIZE / 2 / wma->channels) /
				wma->samplerate;
			incr.seconds = (long) tt;
			incr.fraction =
				(unsigned long) ((tt - (long) tt) * MAD_TIMER_RESOLUTION);

			buf = (unsigned char *) wma_decode_frame(wma, &size);

			if (buf == NULL) {
				// wma playback end...
				WMA_eof = TRUE;
				break;
			}

			while (size > 0) {
				if (OutputBuffer_Pos + size >= WMA_MAX_BUF_SIZE) {
					memcpy(&OutputBuffer[OutputBuffer_Index]
						   [OutputBuffer_Pos / 2], &buf[p],
						   WMA_MAX_BUF_SIZE - OutputBuffer_Pos);
					sceAudioOutputPannedBlocking(WMA_audio_channel, WMA_volume,
												 WMA_volume,
												 OutputBuffer
												 [OutputBuffer_Index]);
					mad_timer_add(&WMA_info.timer, incr);
					OutputBuffer_Index = (OutputBuffer_Index + 1) & 3;
					memset(&OutputBuffer[OutputBuffer_Index][0], 0,
						   WMA_MAX_BUF_SIZE);
					p += WMA_MAX_BUF_SIZE - OutputBuffer_Pos;
					size -= WMA_MAX_BUF_SIZE - OutputBuffer_Pos;
					OutputBuffer_Pos = 0;
				} else {
					memcpy(&OutputBuffer[OutputBuffer_Index]
						   [OutputBuffer_Pos / 2], &buf[p], size);
					OutputBuffer_Pos += size;
					p += size;
					size = 0;
				}
			}
		}
		sceKernelDelayThread(10000);
	}

	WMA_threadExited = TRUE;

	sceKernelExitThread(0);

	return 0;
}

void getWMATagInfo(char *filename, struct fileInfo *targetInfo)
{
	//  charsets_ucs_conv((const byte *)wma->title, (byte *)wma->title);
	//  charsets_ucs_conv((const byte *)wma->author, (byte *)wma->author);

	if (wma == NULL || strcmp(filename, WMA_fileName)) {
		wma_init();
		WmaFile *w = wma_open(filename);

		if (w) {
			if (w->title[0] != 0) {
				strncpy(targetInfo->title, w->title, 256);
				targetInfo->title[255] = '\0';
			}
			if (w->author[0] != 0) {
				strncpy(targetInfo->artist, w->author, 256);
				targetInfo->artist[255] = '\0';
			}

			wma_close(w);
		}
		return;
	}
	if (wma->title[0] != 0) {
		strncpy(targetInfo->title, wma->title, 256);
		targetInfo->title[255] = '\0';
	}
	if (wma->author[0] != 0) {
		strncpy(targetInfo->artist, wma->author, 256);
		targetInfo->artist[255] = '\0';
	}
}

struct fileInfo WMA_GetTagInfoOnly(char *filename)
{
	struct fileInfo tempInfo;

	initFileInfo(&tempInfo);
	getWMATagInfo(filename, &tempInfo);
	return tempInfo;
}

void WMA_Init(int channel)
{
	wma_init();
	WMA_audio_channel = channel;
	WMA_volume = PSP_AUDIO_VOLUME_MAX;
	mad_timer_reset(&WMA_info.timer);
}

int WMA_Load(char *filename)
{
	if (!filename)
		return ERROR_OPENING;

	strncpy(WMA_fileName, filename, 256);
	WMA_fileName[255] = '\0';

	SceUID fd = sceIoOpen(WMA_fileName, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return ERROR_OPENING;

	WMA_info.fileSize = sceIoLseek(fd, 0, PSP_SEEK_END);
	sceIoLseek(fd, 0, PSP_SEEK_SET);
	sceIoClose(fd);

	wma = wma_open(filename);
	if (wma == NULL)
		return ERROR_OPENING;

	WMA_audio_channel =
		sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
						  WMA_MAX_BUF_SIZE / 2 / wma->channels,
						  (wma->channels == 2) ? 0 : 1);
	if (WMA_audio_channel < 0) {
		wma_close(wma);
		return ERROR_CREATE_THREAD;
	}

	WMA_isPlaying = FALSE;

	WMA_thid = -1;
	WMA_eof = FALSE;
	WMA_thid =
		sceKernelCreateThread("WMA_decodeThread", WMA_decodeThread,
							  THREAD_PRIORITY, 0x10000, PSP_THREAD_ATTR_USER,
							  NULL);
	if (WMA_thid < 0) {
		wma_close(wma);
		sceAudioChRelease(WMA_audio_channel);
		return ERROR_CREATE_THREAD;
	}

	sceKernelStartThread(WMA_thid, 0, NULL);

	mad_timer_reset(&WMA_info.timer);

	// clear the buffer
	memset(&OutputBuffer[0][0], 0, 8 * WMA_MAX_BUF_SIZE);
	OutputBuffer_Index = 0;
	OutputBuffer_Pos = 0;

	return OPENING_OK;
}

int WMA_Play()
{
	if (WMA_isPlaying) {
		return FALSE;
	}

	WMA_isPlaying = !WMA_isPlaying;
	return TRUE;
}

void WMA_Pause()
{
	WMA_isPlaying = !WMA_isPlaying;
}

int WMA_Stop()
{
	WMA_isPlaying = FALSE;
	WMA_threadActive = FALSE;
	while (WMA_threadExited == FALSE)
		sceKernelDelayThread(10000);
	sceKernelDeleteThread(WMA_thid);
	WMA_thid = -1;
	sceAudioChRelease(WMA_audio_channel);
	WMA_audio_channel = -1;
	return 0;
}

int WMA_EndOfStream()
{
	return WMA_eof;
}

void WMA_End()
{
	WMA_Stop();
	if (wma)
		wma_close(wma);
	wma = NULL;
	WMA_fileName[0] = '\0';
}

static void getTimeStrBySecond(char *str, size_t size, float seconds)
{
	if (!str || size == 0) {
		return;
	}

	int secs = (int) seconds;
	int hh = secs / 3600;
	int mm = (secs - hh * 3600) / 60;
	int ss = secs - hh * 3600 - mm * 60;

	snprintf(str, size, "%2.2i:%2.2i:%2.2i", hh, mm, ss);
	str[size - 1] = '\0';
}

struct fileInfo WMA_GetInfo()
{
	if (wma) {
		WMA_info.hz = wma->samplerate;
		WMA_info.instantBitrate = wma->bitrate;
		WMA_info.length = wma->duration / 1000;
		getTimeStrBySecond(WMA_info.strLength, sizeof(WMA_info.strLength),
						   WMA_info.length);
	} else {
		WMA_info.hz = 0;
		WMA_info.instantBitrate = 0;
	}

	WMA_info.fileType = WMA_TYPE;
	WMA_info.defaultCPUClock = WMA_defaultCPUClock;
	WMA_info.needsME = 0;
	WMA_info.framesDecoded = 0;
	WMA_info.layer[0] = '\0';
	WMA_info.kbit = WMA_info.instantBitrate / 1000;
	WMA_info.mode[0] = '\0';
	WMA_info.emphasis[0] = '\0';

	WMA_GetTagInfoOnly(WMA_fileName);

	return WMA_info;
}

int WMA_GetPercentage()
{
	float perc;

	if (WMA_info.length != 0) {
		perc =
			(float) (mad_timer_count(WMA_info.timer, MAD_UNITS_SECONDS)) *
			100.0 / WMA_info.length;
	} else {
		perc = 0.0;
	}

	return ((int) perc);
}

int WMA_isFilterSupported()
{
	return FALSE;
}

int WMA_suspend()
{
	return 0;
}

int WMA_resume()
{
	return 0;
}

int WMA_setMute(int onOff)
{
	if (onOff)
		WMA_volume = MUTED_VOLUME;
	else
		WMA_volume = PSP_AUDIO_VOLUME_MAX;

	return 0;
}

void WMA_GetTimeString(char *dest)
{
	if (!dest)
		return;

	mad_timer_string(WMA_info.timer, dest, "%02lu:%02u:%02u", MAD_UNITS_HOURS,
					 MAD_UNITS_MILLISECONDS, 0);
}

void WMA_fadeOut(float seconds)
{
}

int WMA_getPlayingSpeed()
{
	return 0;
}

int WMA_setPlayingSpeed(int playingSpeed)
{
	return -1;
}

void WMA_setVolumeBoostType(char *boostType)
{
}

void WMA_setVolumeBoost(int boost)
{
}

int WMA_getVolumeBoost()
{
	return -1;
}

int WMA_GetStatus()
{
	return 0;
}

int WMA_setFilter(double tFilter[32], int copyFilter)
{
	return FALSE;
}

void WMA_enableFilter()
{
}

void WMA_disableFilter()
{
}

int WMA_isFilterEnabled()
{
	return 0;
}

void WMA_Forward(float seconds)
{
	if (WMA_isPlaying == FALSE)
		return;
	WMA_jump = 1;
	g_seek_seconds = seconds;
}

void WMA_Backward(float seconds)
{
	if (WMA_isPlaying == FALSE)
		return;
	WMA_jump = -1;
	g_seek_seconds = seconds;
}

mad_timer_t WMA_getTimer(void)
{
	return WMA_info.timer;
}
