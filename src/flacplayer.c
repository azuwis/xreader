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
//    CREDITS:
//    All credits for this goes to JLF65

//
//    $Id: flacplayer.c 67 2008-02-29 05:28:12Z hrimfaxi $
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <utime.h>

#include <FLAC/stream_decoder.h>
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#include "player.h"
#include "flacplayer.h"

int bufferThid = -1;
int FLAC_audio_channel;
char FLAC_fileName[262];
FILE *FLAC_file = 0;
int FLAC_eos = 0;
struct fileInfo FLAC_info;
static int isPlaying = 0;
unsigned int FLAC_volume_boost = 0;
int FLAC_playingSpeed = 0;		// 0 = normal
int FLAC_playingDelta = 0;
static int outputInProgress = 0;
static long suspendPosition = -1;
static long suspendIsPlaying = 0;
int FLAC_defaultCPUClock = 166;

int kill_flac_thread;
int bufferLow;

#define MIX_BUF_SIZE (PSP_NUM_AUDIO_SAMPLES * 2)
short FLAC_mixBuffer[MIX_BUF_SIZE * 4] __attribute__ ((aligned(64)));

long FLAC_tempmixleft = 0;
long samples_played = 0;

FLAC__StreamDecoder *decoder = 0;

int chmod(const char *path, mode_t mode)
{
	return 0;
}

int chown(const char *path, uid_t owner, gid_t group)
{
	return 0;
}

int utime(const char *path, const struct utimbuf *times)
{
	return 0;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *
											  decoder,
											  const FLAC__Frame * frame,
											  const FLAC__int32 *
											  const buffer[], void *client_data)
{
	int i;

	(void) decoder, (void) client_data;

	if (FLAC_tempmixleft + frame->header.blocksize > MIX_BUF_SIZE)
		sceKernelWaitSema(bufferLow, 1, 0);	// wait for buffer to get low

	if (kill_flac_thread)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	/* copy decoded PCM samples to buffer */
	for (i = 0; i < frame->header.blocksize; i++) {
		int j = (FLAC_tempmixleft + i) << 1;

		FLAC_mixBuffer[j] = (short) buffer[0][i];
		FLAC_mixBuffer[j + 1] = (short) buffer[1][i];
	}
	FLAC_tempmixleft += frame->header.blocksize;	// increment # samples reported in buffer

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;	// keep going until buffer is full or get killed
}

void metadata_callback(const FLAC__StreamDecoder * decoder,
					   const FLAC__StreamMetadata * metadata, void *client_data)
{
	(void) decoder, (void) metadata, (void) client_data;
}

void error_callback(const FLAC__StreamDecoder * decoder,
					FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void) decoder, (void) client_data;

	//printf(" Error: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

int flacThread(SceSize args, void *argp)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoderInitStatus init_status;

	if ((decoder = FLAC__stream_decoder_new()) == NULL)
		sceKernelExitDeleteThread(0);

	(void) FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status =
		FLAC__stream_decoder_init_file(decoder, FLAC_fileName, write_callback,
									   metadata_callback, error_callback,
									   FLAC_file);
	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		FLAC_eos = 1;
		sceKernelExitDeleteThread(0);
	}

	FLAC_tempmixleft = 0;
	FLAC_eos = 0;
	sceKernelSignalSema(bufferLow, 1);	// so it fills the buffer to start

	kill_flac_thread = 0;
	ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
	//printf(" decoding: %s\n", ok ? "succeeded" : "FAILED");
	//printf("    state: %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);
	FLAC_eos = 1;

	FLAC__stream_decoder_delete(decoder);

	sceKernelExitDeleteThread(0);
	return 0;
}

static void audioCallback(void *_buf2, unsigned int numSamples, void *pdata)
{
	short *_buf = (short *) _buf2;

	if (isPlaying) {			// Playing , so mix up a buffer
		outputInProgress = 1;

		while (FLAC_tempmixleft < numSamples) {	//  Not enough in buffer, so we must mix more
			sceKernelSignalSema(bufferLow, 1);
			sceKernelDelayThread(10);	// allow buffer filling thread to run
			if (FLAC_eos) {		//EOF
				isPlaying = 0;
				outputInProgress = 0;
				break;
			}
		}
		//FLAC_info.instantBitrate = ???;
		if (FLAC_tempmixleft >= numSamples) {	//  Buffer has enough, so copy across
			int count, count2;
			short *_buf2;

			for (count = 0; count < numSamples; count++) {
				count2 = count + count;
				_buf2 = _buf + count2;
				//Volume boost:
				if (FLAC_volume_boost) {
					*(_buf2) =
						volume_boost(&FLAC_mixBuffer[count2],
									 &FLAC_volume_boost);
					*(_buf2 + 1) =
						volume_boost(&FLAC_mixBuffer[count2 + 1],
									 &FLAC_volume_boost);
				} else {
					*(_buf2) = FLAC_mixBuffer[count2];
					*(_buf2 + 1) = FLAC_mixBuffer[count2 + 1];
				}
			}

			//  Move the pointers
			FLAC_tempmixleft -= numSamples;
			//  Now shuffle the buffer along
			for (count = 0; count < FLAC_tempmixleft; count++) {
				int j = count << 1;

				FLAC_mixBuffer[j] = FLAC_mixBuffer[(numSamples << 1) + j];
				FLAC_mixBuffer[j + 1] =
					FLAC_mixBuffer[(numSamples << 1) + j + 1];
			}
			//Check for playing speed:
			if (FLAC_playingSpeed) {
				FLAC__uint64 sample =
					(FLAC__uint64) (samples_played + FLAC_playingDelta);
				if (sample < 0
					|| !FLAC__stream_decoder_seek_absolute(decoder, sample)) {
					FLAC_setPlayingSpeed(0);
				} else {
					samples_played += FLAC_playingDelta;
					//FLAC_tempmixleft = 0; // clear buffer of stale samples
				}
				FLAC__stream_decoder_flush(decoder);
			}
		}
		samples_played += numSamples;
		outputInProgress = 0;
	} else {					//  Not Playing , so clear buffer
		int count;

		for (count = 0; count < numSamples * 2; count++)
			*(_buf + count) = 0;
	}
}

static void splitComment(char *comment, char *name, char *value)
{
	char *result = NULL;

	result = strtok(comment, "=");
	int count = 0;

	while (result != NULL && count < 2) {
		if (strlen(result) > 0) {
			switch (count) {
				case 0:
					strncpy(name, result, 30);
					name[30] = '\0';
					break;
				case 1:
					strncpy(value, result, 256);
					value[256] = '\0';
					break;
			}
			count++;
		}
		result = strtok(NULL, "=");
	}
}

void getFLACTagInfo(char *filename, struct fileInfo *targetInfo)
{
	int i;
	char name[31];
	char value[260];
	FLAC__StreamMetadata *info = 0;

	strcpy(FLAC_fileName, filename);
	if (FLAC__metadata_get_tags(filename, &info)) {
		if (info->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			for (i = 0; i < info->data.vorbis_comment.num_comments; ++i) {
				splitComment((char *) info->data.vorbis_comment.comments[i].
							 entry, name, value);
				if (!strcmp(name, "TITLE"))
					strcpy(targetInfo->title, value);
				else if (!strcmp(name, "ALBUM"))
					strcpy(targetInfo->album, value);
				else if (!strcmp(name, "ARTIST"))
					strcpy(targetInfo->artist, value);
				else if (!strcmp(name, "GENRE"))
					strcpy(targetInfo->genre, value);
				else if (!strcmp(name, "DATE")) {
					strncpy(targetInfo->year, value, 4);
					targetInfo->year[4] = '\0';
				} else if (!strcmp(name, "TRACKNUMBER"))
					strcpy(targetInfo->trackNumber, value);
				else if (!strcmp(name, "COVERART_UUENCODED")) {
					//COVER ART
				}
			}
		}
		FLAC__metadata_object_delete(info);
	}
	if (!strlen(targetInfo->title))
		strcpy(targetInfo->title, FLAC_fileName);
}

void FLACgetInfo(char *filename)
{
	FLAC__StreamMetadata streaminfo;

	if (FLAC__metadata_get_streaminfo(filename, &streaminfo)) {
		FLAC_info.fileType = FLAC_TYPE;
		FLAC_info.defaultCPUClock = FLAC_defaultCPUClock;
		FLAC_info.kbit =
			streaminfo.data.stream_info.sample_rate *
			streaminfo.data.stream_info.bits_per_sample *
			streaminfo.data.stream_info.channels / 1000;
		FLAC_info.instantBitrate =
			streaminfo.data.stream_info.sample_rate *
			streaminfo.data.stream_info.bits_per_sample *
			streaminfo.data.stream_info.channels;
		FLAC_info.hz = streaminfo.data.stream_info.sample_rate;
		FLAC_info.length =
			(long) (streaminfo.data.stream_info.total_samples /
					streaminfo.data.stream_info.sample_rate);
		FLAC_info.needsME = 0;
		if (streaminfo.data.stream_info.channels == 1)
			strcpy(FLAC_info.mode, "single channel");
		else if (streaminfo.data.stream_info.channels == 2)
			strcpy(FLAC_info.mode, "normal LR stereo");
		strcpy(FLAC_info.emphasis, "no");

		int h = 0;
		int m = 0;
		int s = 0;
		long secs = FLAC_info.length;

		h = secs / 3600;
		m = (secs - h * 3600) / 60;
		s = secs - h * 3600 - m * 60;
		snprintf(FLAC_info.strLength, sizeof(FLAC_info.strLength),
				 "%2.2i:%2.2i:%2.2i", h, m, s);
	}

	getFLACTagInfo(FLAC_fileName, &FLAC_info);
}

void FLAC_Init(int channel)
{
	initAudioLib();
	MIN_PLAYING_SPEED = -10;
	MAX_PLAYING_SPEED = 9;
	FLAC_audio_channel = channel;
	samples_played = 0;
	bufferLow = sceKernelCreateSema("bufferLow", 0, 1, 1, 0);
	memset(FLAC_mixBuffer, 0, sizeof(FLAC_mixBuffer));
	FLAC_tempmixleft = 0;
	xMP3AudioSetChannelCallback(FLAC_audio_channel, audioCallback, NULL);
}

int FLAC_Load(char *filename)
{
	int file = -1;

	samples_played = 0;
	isPlaying = 0;
	FLAC_eos = 0;
	FLAC_playingSpeed = 0;
	initFileInfo(&FLAC_info);
	strcpy(FLAC_fileName, filename);

	FLACgetInfo(FLAC_fileName);

	file = sceIoOpen(FLAC_fileName, PSP_O_RDONLY, 0777);
	if (file >= 0) {
		FLAC_info.fileSize = sceIoLseek(file, 0, PSP_SEEK_END);
		sceIoClose(file);
	} else {
		FLAC_Stop();
		return ERROR_OPENING;
	}

	//Start buffer filling thread:
	bufferThid = -1;
	bufferThid =
		sceKernelCreateThread("bufferFilling", flacThread, 0x11, 0x10000,
							  PSP_THREAD_ATTR_USER, NULL);
	if (bufferThid < 0) {
		FLAC_Stop();
		return ERROR_CREATE_THREAD;
	}
	sceKernelStartThread(bufferThid, 0, NULL);

	//Controllo il sample rate:
	if (xMP3AudioSetFrequency(FLAC_info.hz) < 0) {
		FLAC_FreeTune();
		FLAC_Stop();
		return ERROR_INVALID_SAMPLE_RATE;
	}
	return OPENING_OK;
}

int FLAC_Play()
{
	isPlaying = 1;
	return 0;
}

void FLAC_Pause()
{
	isPlaying = !isPlaying;
}

int FLAC_Stop()
{
	isPlaying = 0;
	while (outputInProgress == 1)
		sceKernelDelayThread(100000);
	sceKernelDeleteThread(bufferThid);
	return 0;
}

void FLAC_FreeTune()
{
	kill_flac_thread = 1;
	sceKernelSignalSema(bufferLow, 1);
	sceKernelDelayThread(100 * 1000);
	sceKernelDeleteSema(bufferLow);
	if (FLAC_file)
		fclose(FLAC_file);
	memset(FLAC_mixBuffer, 0, sizeof(FLAC_mixBuffer));
	FLAC_tempmixleft = 0;
}

void FLAC_GetTimeString(char *dest)
{
	char timeString[9];
	long secs = 0;

	if (FLAC_info.hz)
		secs = samples_played / FLAC_info.hz;
	int h = secs / 3600;
	int m = (secs - h * 3600) / 60;
	int s = secs - h * 3600 - m * 60;

	snprintf(timeString, sizeof(timeString), "%2.2i:%2.2i:%2.2i", h, m, s);
	strcpy(dest, timeString);
}

int FLAC_EndOfStream()
{
	return FLAC_eos;
}

struct fileInfo FLAC_GetInfo()
{
	return FLAC_info;
}

struct fileInfo FLAC_GetTagInfoOnly(char *filename)
{
	struct fileInfo tempInfo;

	initFileInfo(&tempInfo);
	getFLACTagInfo(filename, &tempInfo);
	return tempInfo;
}

int FLAC_GetPercentage()
{
	return (int) ((float) (samples_played / FLAC_info.hz) /
				  (float) FLAC_info.length * 100.0);
}

void FLAC_End()
{
	FLAC_Stop();
	xMP3AudioSetChannelCallback(FLAC_audio_channel, 0, 0);
	FLAC_FreeTune();
	endAudioLib();
}

int FLAC_setMute(int onOff)
{
	return setMute(FLAC_audio_channel, onOff);
}

void FLAC_fadeOut(float seconds)
{
	fadeOut(FLAC_audio_channel, seconds);
}

void FLAC_setVolumeBoost(int boost)
{
	FLAC_volume_boost = boost;
}

int FLAC_getVolumeBoost()
{
	return FLAC_volume_boost;
}

int FLAC_setPlayingSpeed(int playingSpeed)
{
	if (playingSpeed >= MIN_PLAYING_SPEED && playingSpeed <= MAX_PLAYING_SPEED) {
		FLAC_playingSpeed = playingSpeed;
		if (playingSpeed == 0)
			setVolume(FLAC_audio_channel, 0x8000);
		else
			setVolume(FLAC_audio_channel, FASTFORWARD_VOLUME);
		FLAC_playingDelta = PSP_NUM_AUDIO_SAMPLES * 2 * FLAC_playingSpeed;
		return 0;
	} else {
		return -1;
	}
}

int FLAC_getPlayingSpeed()
{
	return FLAC_playingSpeed;
}

int FLAC_GetStatus()
{
	return 0;
}

void FLAC_setVolumeBoostType(char *boostType)
{
	//Only old method supported
	MAX_VOLUME_BOOST = 4;
	MIN_VOLUME_BOOST = 0;
}

/// Functions for filter (equalizer):
int FLAC_setFilter(double tFilter[32], int copyFilter)
{
	return 0;
}

void FLAC_enableFilter()
{
}

void FLAC_disableFilter()
{
}

int FLAC_isFilterSupported()
{
	return 0;
}

int FLAC_isFilterEnabled()
{
	return 0;
}

int FLAC_suspend()
{
	suspendPosition = samples_played;
	suspendIsPlaying = isPlaying;
	FLAC_End();
	return 0;
}

int FLAC_resume()
{
	if (suspendPosition >= 0) {
		FLAC__uint64 sample = (FLAC__uint64) suspendPosition;

		FLAC_Load(FLAC_fileName);
		if (FLAC__stream_decoder_seek_absolute(decoder, sample)) {
			samples_played = suspendPosition;
			if (suspendIsPlaying)
				FLAC_Play();
		}
		suspendPosition = -1;
	}
	return 0;
}

mad_timer_t FLAC_getTimer(void)
{
	mad_timer_t t;
	long secs;

	if (FLAC_info.hz != 0)
		secs = samples_played / FLAC_info.hz;
	else
		secs = 0;

	t.seconds = secs;
	t.fraction = 0;

	return t;
}
