//    player.c
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
//    CREDITS:
//    This file contains functions to play aa3 files through the PSP's Media Engine.
//    This code is based upon this sample code from ps2dev.org
//    http://forums.ps2dev.org/viewtopic.php?t=8469
//    and the source code of Music prx by joek2100

//
//    $Id: player.c 68 2008-02-29 05:28:42Z hrimfaxi $
//

#include <pspkernel.h>
#include <pspsdk.h>
#include <string.h>
#include <psputility_avmodules.h>
#include <pspaudio.h>
#include "player.h"

/// shared global vars
int MUTED_VOLUME = 800;
int MAX_VOLUME_BOOST = 15;
int MIN_VOLUME_BOOST = -15;
int MIN_PLAYING_SPEED = 0;
int MAX_PLAYING_SPEED = 9;
int currentVolume = 0;

/// shared global vars for ME
int HW_ModulesInit = 0;
SceUID fd;
u16 data_align;
u32 sample_per_frame;
u16 channel_mode;
u32 samplerate;
long data_start;
long data_size;
u8 getEDRAM;
u32 channels;
SceUID data_memid;
volatile int OutputBuffer_flip;

/// shared between at3+aa3
u16 at3_type;
u8 *at3_data_buffer;
u8 at3_at3plus_flagdata[2];
unsigned char AT3_OutputBuffer[2][AT3_OUTPUT_BUFFER_SIZE]
	__attribute__ ((aligned(64))), *AT3_OutputPtr = AT3_OutputBuffer[0];

/// Load and start needed modules:
int initMEAudioModules()
{
	if (!HW_ModulesInit) {
		if (sceKernelDevkitVersion() == 0x01050001) {
			LoadStartAudioModule("flash0:/kd/me_for_vsh.prx",
								 PSP_MEMORY_PARTITION_KERNEL);
			LoadStartAudioModule("flash0:/kd/audiocodec.prx",
								 PSP_MEMORY_PARTITION_KERNEL);
		} else {
			sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
		}
		HW_ModulesInit = 1;
	}
	return 0;
}

int GetID3TagSize(char *fname)
{
	SceUID fd;
	char header[10];
	int size = 0;

	fd = sceIoOpen(fname, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return 0;

	sceIoRead(fd, header, sizeof(header));
	sceIoClose(fd);

	if (!strncmp((char *) header, "ea3", 3)
		|| !strncmp((char *) header, "EA3", 3)
		|| !strncmp((char *) header, "ID3", 3)) {
		// get the real size from the syncsafe int
		size = header[6];
		size = (size << 7) | header[7];
		size = (size << 7) | header[8];
		size = (size << 7) | header[9];

		size += 10;
		//has footer
		if (header[5] & 0x10)
			size += 10;
		return size;
	}
	return 0;
}

/// Init a file info structure:
void initFileInfo(struct fileInfo *info)
{
	info->fileType = 0;
	info->defaultCPUClock = 0;
	info->needsME = 0;
	info->fileSize = 0;
	strcpy(info->layer, "");
	info->kbit = 0;
	info->instantBitrate = 0;
	info->hz = 0;
	strcpy(info->mode, "");
	strcpy(info->emphasis, "");
	info->length = 0;
	strcpy(info->strLength, "");
	info->frames = 0;
	info->framesDecoded = 0;
	info->encapsulatedPictureOffset = 0;

	strcpy(info->album, "");
	strcpy(info->title, "");
	strcpy(info->artist, "");
	strcpy(info->genre, "");
	strcpy(info->year, "");
	strcpy(info->trackNumber, "");
}

/// In the 3.xx firmwares you have to wait until sceAudioOutput2GetRestSample
/// returns 0 before calling sceAudioSRCChRelease. Also sceAudioOutput2OutputBlocking 
/// only calls sceAudioSRCOutputBlocking, sceAudioOutput2Release only calls sceAudioSRCChRelease and 
/// buggy, may struck here
int releaseAudio(void)
{
	while (sceAudioOutput2GetRestSample() > 0);
	return sceAudioSRCChRelease();
}

/// Set frequency for output:
int setAudioFrequency(unsigned short samples, unsigned short freq, char car)
{
	return sceAudioSRCChReserve(samples, freq, car);
}

/// Functions for ME
/// Open audio for player
int openAudio(int channel, int samplecount)
{
	int audio_channel =
		sceAudioChReserve(channel, samplecount, PSP_AUDIO_FORMAT_STEREO);
	if (audio_channel < 0)
		audio_channel =
			sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, samplecount,
							  PSP_AUDIO_FORMAT_STEREO);
	return audio_channel;
}

unsigned char volume_boost_char(unsigned char *Sample, unsigned int *boost)
{
	int intSample = (int) *Sample * (*boost + 1);

	if (intSample > 255)
		return 255;
	else if (intSample < 0)
		return 0;
	else
		return (unsigned char) intSample;
}

/// Set volume:
int setVolume(int channel, int volume)
{
	xMP3AudioSetVolume(channel, volume, volume);
	currentVolume = volume;
	return 0;
}

/// Set mute:
int setMute(int channel, int onOff)
{
	if (onOff)
		setVolume(channel, MUTED_VOLUME);
	else
		setVolume(channel, PSP_AUDIO_VOLUME_MAX);
	return 0;
}

/// Fade out:
void fadeOut(int channel, float seconds)
{
	int i = 0;
	long timeToWait = (long) ((seconds * 1000.0) / (float) currentVolume);

	for (i = currentVolume; i >= 0; i--) {
		xMP3AudioSetVolume(channel, i, i);
		sceKernelDelayThread(timeToWait);
	}
}

/*
void MusicDriver_fadeOut(float seconds)
*/

/// Volume boost for a single sample:
/// @note OLD METHOD
short volume_boost(short *Sample, unsigned int *boost)
{
	int intSample = *Sample * (*boost + 1);

	if (intSample > 32767)
		return 32767;
	else if (intSample < -32768)
		return -32768;
	else
		return intSample;
}

/*unsigned long volume_boost_long(unsigned long *Sample, unsigned int *boost){
	long intSample = *Sample * (*boost + 1);
	if (intSample > 2147483647)
		return 2147483647;
	else if (intSample < -2147483648)
		return -2147483648;
	else
    	return intSample;
}*/

/// Release audio:
int sceAudioOutput2GetRestSample();

/// Audio output:
int sceAudioSRCOutputBlocking(int volume, void *buffer);
int audioOutput(int volume, void *buffer)
{
	return sceAudioSRCOutputBlocking(volume, buffer);
}

/// Init pspaudiolib:
int initAudioLib()
{
	xMP3AudioInit();
	return 0;
}

/// End pspaudiolib:
int endAudioLib()
{
	xMP3AudioEnd();
	return 0;
}

/// Load a module:
SceUID LoadStartAudioModule(char *modname, int partition)
{
	SceKernelLMOption option;
	SceUID modid;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.mpidtext = partition;
	option.mpiddata = partition;
	option.position = 0;
	option.access = 1;

	modid = sceKernelLoadModule(modname, 0, &option);
	if (modid < 0)
		return modid;

	return sceKernelStartModule(modid, 0, NULL, NULL, NULL);
}

