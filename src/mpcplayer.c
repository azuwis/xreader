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
//    $Id: mpcplayer.c 66 2008-02-29 05:22:39Z hrimfaxi $
//

#include <string.h>
#include <stdio.h>
#include <mpcdec/mpcdec.h>
#include <assert.h>

#include "player.h"
#include "id3.h"
#include "mpcplayer.h"
#include "id3tag.h"
#include "strsafe.h"
#include "musicmgr.h"
#include "musicdrv.h"

#define THREAD_PRIORITY 0x12

static MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];
static signed short OutputBuffer[4][MPC_DECODER_BUFFER_LENGTH];
static signed short InputBuffer[MPC_DECODER_BUFFER_LENGTH];
static int OutputBuffer_Index = 0;
static int OutputBuffer_Pos = 0;
static int MPC_audio_channel = -1;
static int MPC_volume = PSP_AUDIO_VOLUME_MAX;
static volatile int MPC_isPlaying = FALSE;
static struct fileInfo MPC_info;

static int MPC_thid = -1;
static volatile int MPC_threadActive = FALSE;
static volatile int MPC_threadExited = FALSE;
static volatile int MPC_eof = FALSE;

static int MPC_defaultCPUClock = 80;
static char MPC_fileName[256] = { 0 };
static bool MPC_Loaded = false;

/*
 * @note
 * - -1 backward
 * - 0  normal playback
 * -  1 forward
 */
static volatile int MPC_jump = 0;
static volatile float g_seek_seconds = 0.0;

/*
  The data bundle we pass around with our reader to store file
  position and size etc. 
*/
typedef struct reader_data_t
{
	SceUID fd;
	long size;
	mpc_bool_t seekable;
} reader_data;

static reader_data data;
static mpc_decoder decoder;
static mpc_reader reader;
static mpc_streaminfo info;

/*
  Our implementations of the mpc_reader callback functions.
*/
mpc_int32_t read_impl(void *data, void *ptr, mpc_int32_t size)
{
	reader_data *d = (reader_data *) data;

	return sceIoRead(d->fd, ptr, size);
}

mpc_bool_t seek_impl(void *data, mpc_int32_t offset)
{
	reader_data *d = (reader_data *) data;

	return d->seekable ? (sceIoLseek(d->fd, offset, SEEK_SET), 1) : 0;
}

mpc_int32_t tell_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return sceIoLseek(d->fd, 0, SEEK_CUR);
}

mpc_int32_t get_size_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return d->size;
}

mpc_bool_t canseek_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return d->seekable;
}

#ifdef MPC_FIXED_POINT
static int shift_signed(MPC_SAMPLE_FORMAT val, int shift)
{
	if (shift > 0)
		val <<= shift;
	else if (shift < 0)
		val >>= -shift;
	return (int) val;
}
#endif

void OutputAudio(signed short *buf, size_t size, mad_timer_t incr)
{
	int p = 0;

	while (size > 0) {
		if (OutputBuffer_Pos + size >= MPC_DECODER_BUFFER_LENGTH) {
			memcpy(&OutputBuffer[OutputBuffer_Index]
				   [OutputBuffer_Pos / 2], &buf[p],
				   MPC_DECODER_BUFFER_LENGTH - OutputBuffer_Pos);
			sceAudioOutputPannedBlocking(MPC_audio_channel, MPC_volume,
										 MPC_volume,
										 OutputBuffer[OutputBuffer_Index]);
			mad_timer_add(&MPC_info.timer, incr);
			OutputBuffer_Index = (OutputBuffer_Index + 1) & 3;
			memset(&OutputBuffer[OutputBuffer_Index][0], 0,
				   MPC_DECODER_BUFFER_LENGTH);
			p += MPC_DECODER_BUFFER_LENGTH - OutputBuffer_Pos;
			size -= MPC_DECODER_BUFFER_LENGTH - OutputBuffer_Pos;
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

/**
 * @param p_buffer MPC Sample buffer pointer
 * @param p_size   MPC Sample count
 */
static mpc_bool_t OutputSamples(const MPC_SAMPLE_FORMAT * p_buffer,
								unsigned p_size)
{
	mad_timer_t incr;
	double tt =
		(double) (MPC_DECODER_BUFFER_LENGTH / 2 / info.channels) /
		info.sample_freq;
	incr.seconds = (long) tt;
	incr.fraction = (unsigned long) ((tt - (long) tt) * MAD_TIMER_RESOLUTION);

	unsigned n;
	unsigned m_bps = 16;
	signed short *p = InputBuffer;
	int clip_min = -1 << (m_bps - 1), clip_max = (1 << (m_bps - 1)) - 1;

#ifndef MPC_FIXED_POINT
	int float_scale = 1 << (m_bps - 1);
#endif
	for (n = 0; n < p_size; n++) {
		int val;

#ifdef MPC_FIXED_POINT
		val = shift_signed(p_buffer[n], m_bps - MPC_FIXED_POINT_SCALE_SHIFT);
#else
		val = (int) (p_buffer[n] * float_scale);
#endif
		if (val < clip_min)
			val = clip_min;
		else if (val > clip_max)
			val = clip_max;
		*p++ = (signed short) val;
	}

	OutputAudio(InputBuffer, MPC_DECODER_BUFFER_LENGTH, incr);

	return 1;
}

static void clearOutputSampleBuffer(void)
{
	memset(&OutputBuffer[0][0], 0, sizeof(OutputBuffer));
	memset(&InputBuffer[0], 0, sizeof(InputBuffer));
}

static int _end(void)
{
	if (data.fd >= 0) {
		sceIoClose(data.fd);
		data.fd = -1;
	}
	if (MPC_thid >= 0) {
		sceKernelDeleteThread(MPC_thid);
		MPC_thid = -1;
	}
	if (MPC_audio_channel >= 0) {
		sceAudioChRelease(MPC_audio_channel);
		MPC_audio_channel = -1;
	}

	MPC_Loaded = false;

	return 0;
}

int MPC_decodeThread(SceSize args, void *argp)
{
	MPC_threadActive = TRUE;
	MPC_threadExited = FALSE;

	MPC_jump = 0;

	while (MPC_threadActive) {
		while (MPC_eof == FALSE && MPC_isPlaying) {
			if (MPC_jump < 0) {
				float s = mad_timer_count(MPC_info.timer, MAD_UNITS_SECONDS);

				s -= g_seek_seconds;
				if (s < 0.0) {
					s = 0.0;
				}
				mpc_decoder_seek_seconds(&decoder, s);
				MPC_info.timer.seconds = (long) s;
				MPC_info.timer.fraction =
					(unsigned long) ((s - (long) s) * MAD_TIMER_RESOLUTION);
				clearOutputSampleBuffer();
				MPC_jump = 0;
			}
			if (MPC_jump > 0) {
				float s = mad_timer_count(MPC_info.timer, MAD_UNITS_SECONDS);

				s += g_seek_seconds;
				if (s >= MPC_info.length) {
					MPC_jump = 0;
					MPC_isPlaying = 0;
					MPC_threadActive = 0;
					MPC_eof = TRUE;
					break;
				}
				mpc_decoder_seek_seconds(&decoder, s);
				MPC_info.timer.seconds = (long) s;
				MPC_info.timer.fraction =
					(unsigned long) ((s - (long) s) * MAD_TIMER_RESOLUTION);
				clearOutputSampleBuffer();
				MPC_jump = 0;
			}
			unsigned status = mpc_decoder_decode(&decoder, sample_buffer, 0, 0);

			if (status == (unsigned) (-1) || status == 0) {
				// MPC playback end...
				MPC_jump = 0;
				MPC_isPlaying = 0;
				MPC_threadActive = 0;
				MPC_eof = TRUE;
				break;
			}
			OutputSamples(sample_buffer, status * 2);
		}
		sceKernelDelayThread(10000);
	}

	MPC_threadExited = TRUE;
	MPC_jump = 0;

	if (data.fd >= 0) {
		sceIoClose(data.fd);
		data.fd = -1;
	}
	if (MPC_audio_channel >= 0) {
		sceAudioChRelease(MPC_audio_channel);
		MPC_audio_channel = -1;
	}
	MPC_Loaded = false;

	sceKernelExitDeleteThread(0);

	return 0;
}

//TODO
void getMPCTagInfo(char *filename, struct fileInfo *targetInfo)
{
	initFileInfo(&MPC_info);
	SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		return;
	}

	TagInfo_t taginfo;

	if (Read_ID3V1_Tags(fd, &taginfo) || Read_APE_Tags(fd, &taginfo)) {
		STRCPY_S(targetInfo->album, taginfo.Album);
		STRCPY_S(targetInfo->title, taginfo.Title);
		STRCPY_S(targetInfo->artist, taginfo.Artist);
		STRCPY_S(targetInfo->genre, taginfo.Genre);
		STRCPY_S(targetInfo->year, taginfo.Year);
		STRCPY_S(targetInfo->trackNumber, taginfo.Track);
	}

	sceIoClose(fd);
}

struct fileInfo MPC_GetTagInfoOnly(char *filename)
{
	if (strcmp(filename, MPC_fileName) != 0) {
		getMPCTagInfo(filename, &MPC_info);
	}
	return MPC_info;
}

void MPC_Init(int channel)
{
	MPC_audio_channel = channel;
	MPC_volume = PSP_AUDIO_VOLUME_MAX;
	mad_timer_reset(&MPC_info.timer);
	initFileInfo(&MPC_info);
}

int MPC_Load(char *filename)
{
	if (!filename)
		return ERROR_OPENING;

	STRCPY_S(MPC_fileName, filename);

	data.fd = sceIoOpen(MPC_fileName, PSP_O_RDONLY, 0777);

	if (data.fd < 0) {
		_end();
		return ERROR_OPENING;
	}

	data.seekable = TRUE;

	data.size = sceIoLseek(data.fd, 0, PSP_SEEK_END);
	sceIoLseek(data.fd, 0, PSP_SEEK_SET);

	reader.read = read_impl;
	reader.seek = seek_impl;
	reader.tell = tell_impl;
	reader.get_size = get_size_impl;
	reader.canseek = canseek_impl;
	reader.data = &data;

	mpc_streaminfo_init(&info);
	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		return ERROR_OPENING;
	}

	mpc_decoder_setup(&decoder, &reader);

	mpc_decoder_set_seeking(&decoder, &info, 0);

	if (!mpc_decoder_initialize(&decoder, &info)) {
		_end();
		return ERROR_OPENING;
	}

	MPC_audio_channel =
		sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
						  MPC_DECODER_BUFFER_LENGTH / 2 / 2, 0);
	if (MPC_audio_channel < 0) {
		_end();
		return ERROR_CREATE_THREAD;
	}

	initFileInfo(&MPC_info);
	getMPCTagInfo(filename, &MPC_info);

	MPC_isPlaying = FALSE;
	MPC_jump = 0;
	MPC_thid = -1;
	MPC_eof = FALSE;
	MPC_thid =
		sceKernelCreateThread("MPC_decodeThread", MPC_decodeThread,
							  THREAD_PRIORITY, 0x10000, PSP_THREAD_ATTR_USER,
							  NULL);
	if (MPC_thid < 0) {
		_end();
		return ERROR_CREATE_THREAD;
	}

	mad_timer_reset(&MPC_info.timer);

	// clear the buffer
	memset(&sample_buffer[0], 0, sizeof(sample_buffer));
	memset(&OutputBuffer[0][0], 0, sizeof(OutputBuffer));
	memset(&InputBuffer[0], 0, sizeof(InputBuffer));

	OutputBuffer_Index = 0;
	OutputBuffer_Pos = 0;

	int ret = sceKernelStartThread(MPC_thid, 0, NULL);

	if (ret < 0) {
		_end();
		return ERROR_CREATE_THREAD;
	}

	MPC_Loaded = true;

	return OPENING_OK;
}

int MPC_Play()
{
	if (MPC_isPlaying) {
		return FALSE;
	}

	MPC_isPlaying = !MPC_isPlaying;
	return TRUE;
}

void MPC_Pause()
{
	MPC_isPlaying = !MPC_isPlaying;
}

int MPC_Stop()
{
	MPC_isPlaying = FALSE;
	MPC_threadActive = FALSE;
	if (MPC_thid >= 0)
		while (MPC_threadExited == FALSE)
			sceKernelDelayThread(10000);
	_end();
	return 0;
}

int MPC_EndOfStream()
{
	return MPC_eof;
}

void MPC_End()
{
	MPC_Stop();
	MPC_fileName[0] = '\0';
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

	snprintf_s(str, size, "%2.2i:%2.2i:%2.2i", hh, mm, ss);
}

struct fileInfo MPC_GetInfo()
{
	MPC_info.instantBitrate = info.average_bitrate;
	MPC_info.hz = info.sample_freq;
	if (info.average_bitrate != 0) {
		MPC_info.length = info.total_file_length * 8 / info.average_bitrate;
	} else {
		MPC_info.length = 0;
	}
	getTimeStrBySecond(MPC_info.strLength, sizeof(MPC_info.strLength),
					   MPC_info.length);

	MPC_info.fileType = MPC_TYPE;
	MPC_info.defaultCPUClock = MPC_defaultCPUClock;
	MPC_info.needsME = 0;
	MPC_info.framesDecoded = 0;
	MPC_info.layer[0] = '\0';
	MPC_info.kbit = info.average_bitrate / 1000;
	SPRINTF_S(MPC_info.mode,
			  "SV %lu.%lu, Profile %s (%s)", info.stream_version & 15,
			  info.stream_version >> 4, info.profile_name, info.encoder);
	MPC_info.emphasis[0] = '\0';

	return MPC_info;
}

int MPC_GetPercentage()
{
	float perc;

	if (MPC_info.length != 0) {
		perc =
			(float) (mad_timer_count(MPC_info.timer, MAD_UNITS_SECONDS)) *
			100.0 / MPC_info.length;
	} else {
		perc = 0.0;
	}

	return ((int) perc);
}

int MPC_isFilterSupported()
{
	return FALSE;
}

int MPC_suspend()
{
	return 0;
}

int MPC_resume()
{
	return 0;
}

int MPC_setMute(int onOff)
{
	if (onOff)
		MPC_volume = MUTED_VOLUME;
	else
		MPC_volume = PSP_AUDIO_VOLUME_MAX;

	return 0;
}

void MPC_GetTimeString(char *dest)
{
	if (!dest)
		return;

	mad_timer_string(MPC_info.timer, dest, "%02lu:%02u:%02u", MAD_UNITS_HOURS,
					 MAD_UNITS_MILLISECONDS, 0);
}

void MPC_fadeOut(float seconds)
{
}

int MPC_getPlayingSpeed()
{
	return 0;
}

int MPC_setPlayingSpeed(int playingSpeed)
{
	return -1;
}

void MPC_setVolumeBoostType(char *boostType)
{
}

void MPC_setVolumeBoost(int boost)
{
}

int MPC_getVolumeBoost()
{
	return -1;
}

int MPC_GetStatus()
{
	return 0;
}

int MPC_setFilter(double tFilter[32], int copyFilter)
{
	return FALSE;
}

void MPC_enableFilter()
{
}

void MPC_disableFilter()
{
}

int MPC_isFilterEnabled()
{
	return 0;
}

void MPC_Forward(float seconds)
{
	if (MPC_isPlaying == FALSE)
		return;
	MPC_jump = 1;
	g_seek_seconds = seconds;
}

void MPC_Backward(float seconds)
{
	if (MPC_isPlaying == FALSE)
		return;
	MPC_jump = -1;
	g_seek_seconds = seconds;
}

mad_timer_t MPC_getTimer()
{
	return MPC_info.timer;
}

int mpc_load(const char *spath, const char *lpath)
{
	MPC_Init(0);
	if (MPC_Load((char *) spath) != OPENING_OK)
		return -1;

	MPC_GetInfo();
	return 0;
}

int mpc_play(void)
{
	return MPC_Play();
}

int mpc_pause(void)
{
	MPC_isPlaying = 0;

	return 0;
}

int mpc_fforward(int sec)
{
	MPC_Forward(sec);

	return 0;
}

int mpc_fbackward(int sec)
{
	MPC_Backward(sec);

	return 0;
}

int mpc_get_status(void)
{
	if (MPC_isPlaying)
		return ST_PLAYING;
	if (!MPC_isPlaying && MPC_threadActive && !MPC_eof)
		return ST_PAUSED;

	if (MPC_Loaded)
		return ST_LOADED;

	return ST_STOPPED;
}

int mpc_suspend(void)
{
	return MPC_Stop();
}

int mpc_resume(const char *filename)
{
	return 0;
}

int mpc_get_info(struct music_info *info)
{
	if (info->type & MD_GET_TITLE) {
		STRCPY_S(info->title, MPC_info.title);
	}
	if (info->type & MD_GET_ARTIST) {
		STRCPY_S(info->artist, MPC_info.artist);
	}
	if (info->type & MD_GET_COMMENT) {
		STRCPY_S(info->comment, "");
	}
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = mad_timer_count(MPC_info.timer, MAD_UNITS_SECONDS);
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = MPC_info.length;
	}
	if (info->type & MD_GET_CPUFREQ) {
		info->psp_freq[0] = 80;
		info->psp_freq[1] = 111;
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = MPC_info.hz;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = 2;
	}
	if (info->type & MD_GET_DECODERNAME) {
		STRCPY_S(info->decoder_name, "");
	}
	if (info->type & MD_GET_ENCODEMSG) {
		STRCPY_S(info->encode_msg, "");
	}
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = MPC_info.kbit;
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = MPC_info.instantBitrate;
	}
	if (info->type & MD_GET_FILEFD) {
		info->file_handle = -1;
	}
	if (info->type & MD_GET_SNDCHL) {
		info->channel_handle = -1;
	}

	return 0;
}

int mpc_end(void)
{
	MPC_End();
	return 0;
}

static int mpc_set_opt(const char *key, const char *value)
{
	return 0;
}

static struct music_ops mpc_ops = {
	.name = "musepack",
	.set_opt = mpc_set_opt,
	.load = mpc_load,
	.play = mpc_play,
	.pause = mpc_pause,
	.end = mpc_end,
	.get_status = mpc_get_status,
	.fforward = mpc_fforward,
	.fbackward = mpc_fbackward,
	.suspend = mpc_suspend,
	.resume = mpc_resume,
	.get_info = mpc_get_info,
	.next = NULL
};

int mpc_init(void)
{
	return register_musicdrv(&mpc_ops);
}
