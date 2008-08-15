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
//    $Id: oggplayer.c 57 2008-02-16 08:53:13Z hrimfaxi $
//

#include <string.h>

#include "tremor/ivorbiscodec.h"
#include "tremor/ivorbisfile.h"
#include "player.h"
#include "oggplayer.h"

int OGG_audio_channel;
char OGG_fileName[262];
int OGG_file = 0;
OggVorbis_File OGG_VorbisFile;
int OGG_eos = 0;
struct fileInfo OGG_info;
int isPlaying = 0;
unsigned int OGG_volume_boost = 0.0;
double OGG_milliSeconds = 0.0;
int OGG_playingSpeed = 0;		// 0 = normal
int OGG_playingDelta = 0;
int outputInProgress = 0;
long OGG_suspendPosition = -1;
long OGG_suspendIsPlaying = 0;
int OGG_defaultCPUClock = 50;

/// Audio callback
static void oggDecodeThread(void *_buf2, unsigned int numSamples, void *pdata)
{
	short *_buf = (short *) _buf2;
	static short tempmixbuf[PSP_NUM_AUDIO_SAMPLES * 2 * 2]
		__attribute__ ((aligned(64)));
	static unsigned long tempmixleft = 0;
	int current_section;

	if (isPlaying) {
		// Playing , so mix up a buffer
		outputInProgress = 1;
		while (tempmixleft < numSamples) {
			//  Not enough in buffer, so we must mix more
			unsigned long bytesRequired = (numSamples - tempmixleft) * 4;

			// 2channels, 16bit = 4 bytes per sample
			unsigned long ret =
				ov_read(&OGG_VorbisFile, (char *) &tempmixbuf[tempmixleft * 2],
						bytesRequired, &current_section);

			if (!ret) {
				// EOF
				isPlaying = 0;
				OGG_eos = 1;
				outputInProgress = 0;
				return;
			} else if (ret < 0) {
				if (ret == OV_HOLE)
					continue;
				isPlaying = 0;
				OGG_eos = 1;
				outputInProgress = 0;
				return;
			}
			// back down to sample num
			tempmixleft += ret / 4;
		}
		OGG_info.instantBitrate = ov_bitrate_instant(&OGG_VorbisFile);
		OGG_milliSeconds = ov_time_tell(&OGG_VorbisFile);

		// Check for playing speed:
		if (OGG_playingSpeed) {
			if (ov_raw_seek
				(&OGG_VorbisFile,
				 ov_raw_tell(&OGG_VorbisFile) + OGG_playingDelta) != 0)
				OGG_setPlayingSpeed(0);
		}

		if (tempmixleft >= numSamples) {
			//  Buffer has enough, so copy across
			int count, count2;
			short *_buf2;

			for (count = 0; count < numSamples; count++) {
				count2 = count + count;
				_buf2 = _buf + count2;
				// Volume boost:
				if (OGG_volume_boost) {
					*(_buf2) =
						volume_boost(&tempmixbuf[count2], &OGG_volume_boost);
					*(_buf2 + 1) =
						volume_boost(&tempmixbuf[count2 + 1],
									 &OGG_volume_boost);
				} else {
					// Double up for stereo
					*(_buf2) = tempmixbuf[count2];
					*(_buf2 + 1) = tempmixbuf[count2 + 1];
				}
			}
			//  Move the pointers
			tempmixleft -= numSamples;
			//  Now shuffle the buffer along
			for (count = 0; count < tempmixleft; count++)
				tempmixbuf[count] = tempmixbuf[numSamples + count];

		}
		outputInProgress = 0;
	} else {
		//  Not Playing , so clear buffer
		int count;

		for (count = 0; count < numSamples * 2; count++)
			*(_buf + count) = 0;
	}
}

/// Callback for vorbis
size_t ogg_callback_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return sceIoRead(*(int *) datasource, ptr, size * nmemb);
}

int ogg_callback_seek(void *datasource, ogg_int64_t offset, int whence)
{
	return sceIoLseek32(*(int *) datasource, (unsigned int) offset, whence);
}

long ogg_callback_tell(void *datasource)
{
	return sceIoLseek32(*(int *) datasource, 0, SEEK_CUR);
}

int ogg_callback_close(void *datasource)
{
	return sceIoClose(*(int *) datasource);
}

void splitComment(char *comment, char *name, char *value)
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

void getOGGTagInfo(OggVorbis_File * inVorbisFile, struct fileInfo *targetInfo)
{
	int i;
	char name[31];
	char value[257];

	vorbis_comment *comment = ov_comment(inVorbisFile, -1);

	for (i = 0; i < comment->comments; i++) {
		splitComment(comment->user_comments[i], name, value);
		if (!strcmp(name, "TITLE"))
			strcpy(targetInfo->title, value);
		else if (!strcmp(name, "ALBUM"))
			strcpy(targetInfo->album, value);
		else if (!strcmp(name, "ARTIST"))
			strcpy(targetInfo->artist, value);
		else if (!strcmp(name, "GENRE"))
			strcpy(targetInfo->genre, value);
		else if (!strcmp(name, "DATE"))
			strcpy(targetInfo->year, value);
		else if (!strcmp(name, "TRACKNUMBER"))
			strcpy(targetInfo->trackNumber, value);
	}
}

void OGGgetInfo()
{
	// Estraggo le informazioni:
	OGG_info.fileType = OGG_TYPE;
	OGG_info.defaultCPUClock = OGG_defaultCPUClock;
	OGG_info.needsME = 0;

	vorbis_info *vi = ov_info(&OGG_VorbisFile, -1);

	OGG_info.kbit = vi->bitrate_nominal / 1000;
	OGG_info.instantBitrate = vi->bitrate_nominal;
	OGG_info.hz = vi->rate;
	OGG_info.length = (long) ov_time_total(&OGG_VorbisFile, -1) / 1000;
	if (vi->channels == 1)
		strcpy(OGG_info.mode, "single channel");
	else if (vi->channels == 2)
		strcpy(OGG_info.mode, "normal LR stereo");
	strcpy(OGG_info.emphasis, "no");

	int h = 0;
	int m = 0;
	int s = 0;
	long secs = OGG_info.length;

	h = secs / 3600;
	m = (secs - h * 3600) / 60;
	s = secs - h * 3600 - m * 60;
	snprintf(OGG_info.strLength, sizeof(OGG_info.strLength),
			 "%2.2i:%2.2i:%2.2i", h, m, s);

	getOGGTagInfo(&OGG_VorbisFile, &OGG_info);
}

void OGG_Init(int channel)
{
	initAudioLib();
	MIN_PLAYING_SPEED = -10;
	MAX_PLAYING_SPEED = 9;
	OGG_audio_channel = channel;
	xMP3AudioSetChannelCallback(OGG_audio_channel, oggDecodeThread, NULL);
}

int OGG_Load(char *filename)
{
	isPlaying = 0;
	OGG_milliSeconds = 0;
	OGG_eos = 0;
	OGG_playingSpeed = 0;
	OGG_playingDelta = 0;
	strcpy(OGG_fileName, filename);
	// Apro il file OGG:
	initFileInfo(&OGG_info);
	OGG_file = sceIoOpen(OGG_fileName, PSP_O_RDONLY, 0777);
	if (OGG_file >= 0) {
		OGG_info.fileSize = sceIoLseek(OGG_file, 0, PSP_SEEK_END);
		sceIoLseek(OGG_file, 0, PSP_SEEK_SET);
		ov_callbacks ogg_callbacks;

		ogg_callbacks.read_func = ogg_callback_read;
		ogg_callbacks.seek_func = ogg_callback_seek;
		ogg_callbacks.close_func = ogg_callback_close;
		ogg_callbacks.tell_func = ogg_callback_tell;
		if (ov_open_callbacks
			(&OGG_file, &OGG_VorbisFile, NULL, 0, ogg_callbacks) < 0) {
			sceIoClose(OGG_file);
			return ERROR_OPENING;
		}
	} else {
		return ERROR_OPENING;
	}

	OGGgetInfo();
	// Controllo il sample rate:
	if (xMP3AudioSetFrequency(OGG_info.hz) < 0) {
		OGG_FreeTune();
		return ERROR_INVALID_SAMPLE_RATE;
	}
	return OPENING_OK;
}

int OGG_Play()
{
	isPlaying = 1;
	return 0;
}

void OGG_Pause()
{
	isPlaying = !isPlaying;
}

int OGG_Stop()
{
	isPlaying = 0;
	// This is to be sure that oggDecodeThread isn't messing with &OGG_VorbisFile
	while (outputInProgress == 1)
		sceKernelDelayThread(100000);
	return 0;
}

void OGG_FreeTune()
{
	ov_clear(&OGG_VorbisFile);
	if (OGG_file >= 0)
		sceIoClose(OGG_file);
}

void OGG_GetTimeString(char *dest)
{
	char timeString[9];
	long secs = (long) OGG_milliSeconds / 1000;
	int h = secs / 3600;
	int m = (secs - h * 3600) / 60;
	int s = secs - h * 3600 - m * 60;

	snprintf(timeString, sizeof(timeString), "%2.2i:%2.2i:%2.2i", h, m, s);
	strcpy(dest, timeString);
}

int OGG_EndOfStream()
{
	return OGG_eos;
}

struct fileInfo OGG_GetInfo()
{
	return OGG_info;
}

struct fileInfo OGG_GetTagInfoOnly(char *filename)
{
	int tempFile = 0;
	OggVorbis_File vf;
	struct fileInfo tempInfo;

	initFileInfo(&tempInfo);
	// Apro il file OGG:
	tempFile = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if (tempFile >= 0) {
		//sceIoLseek(tempFile, 0, PSP_SEEK_SET);
		ov_callbacks ogg_callbacks;

		ogg_callbacks.read_func = ogg_callback_read;
		ogg_callbacks.seek_func = ogg_callback_seek;
		ogg_callbacks.close_func = ogg_callback_close;
		ogg_callbacks.tell_func = ogg_callback_tell;

		if (ov_open_callbacks(&tempFile, &vf, NULL, 0, ogg_callbacks) < 0) {
			sceIoClose(tempFile);
			return tempInfo;
		}
		getOGGTagInfo(&vf, &tempInfo);
		ov_clear(&vf);
		if (tempFile >= 0)
			sceIoClose(tempFile);
	}
	return tempInfo;
}

int OGG_GetPercentage()
{
	return (int) (OGG_milliSeconds / 1000.0 / (double) OGG_info.length * 100.0);
}

void OGG_End()
{
	OGG_Stop();
	xMP3AudioSetChannelCallback(OGG_audio_channel, 0, 0);
	OGG_FreeTune();
	endAudioLib();
}

int OGG_setMute(int onOff)
{
	return setMute(OGG_audio_channel, onOff);
}

void OGG_fadeOut(float seconds)
{
	fadeOut(OGG_audio_channel, seconds);
}

void OGG_setVolumeBoost(int boost)
{
	OGG_volume_boost = boost;
}

int OGG_getVolumeBoost()
{
	return OGG_volume_boost;
}

int OGG_setPlayingSpeed(int playingSpeed)
{
	if (playingSpeed >= MIN_PLAYING_SPEED && playingSpeed <= MAX_PLAYING_SPEED) {
		OGG_playingSpeed = playingSpeed;
		if (playingSpeed == 0)
			setVolume(OGG_audio_channel, 0x8000);
		else
			setVolume(OGG_audio_channel, FASTFORWARD_VOLUME);
		OGG_playingDelta = PSP_NUM_AUDIO_SAMPLES * 4 * OGG_playingSpeed;
		return 0;
	} else {
		return -1;
	}
}

int OGG_getPlayingSpeed()
{
	return OGG_playingSpeed;
}

int OGG_GetStatus()
{
	return 0;
}

void OGG_setVolumeBoostType(char *boostType)
{
	// Only old method supported
	MAX_VOLUME_BOOST = 4;
	MIN_VOLUME_BOOST = 0;
}

/// Functions for filter (equalizer): 
int OGG_setFilter(double tFilter[32], int copyFilter)
{
	return 0;
}

void OGG_enableFilter()
{
}

void OGG_disableFilter()
{
}

int OGG_isFilterSupported()
{
	return 0;
}

int OGG_isFilterEnabled()
{
	return 0;
}

int OGG_suspend()
{
	OGG_suspendPosition = ov_raw_tell(&OGG_VorbisFile);
	OGG_suspendIsPlaying = isPlaying;
	OGG_Stop();
	OGG_FreeTune();
	return 0;
}

int OGG_resume()
{
	if (OGG_suspendPosition >= 0) {
		OGG_Load(OGG_fileName);
		if (ov_raw_seek(&OGG_VorbisFile, OGG_suspendPosition)) {
			if (OGG_suspendIsPlaying)
				OGG_Play();
		}
		OGG_suspendPosition = -1;
	}
	return 0;
}

mad_timer_t OGG_getTimer()
{
	float sec = OGG_milliSeconds / 1000.0;
	mad_timer_t t;

	t.seconds = sec;
	t.fraction = 0;
	return t;
}
