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

MusicDriver g_MusicDriver;

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

char GetOMGFileType(char *fname)
{
	SceUID fd;
	int size;
	char ea3_header[0x60];

	size = GetID3TagSize(fname);

	fd = sceIoOpen(fname, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return UNK_TYPE;

	sceIoLseek32(fd, size, PSP_SEEK_SET);

	if (sceIoRead(fd, ea3_header, 0x60) != 0x60) {
		sceIoClose(fd);
		return UNK_TYPE;
	}

	sceIoClose(fd);

	if (strncmp(ea3_header, "EA3", 3) != 0) {
		return UNK_TYPE;
	}

	switch (ea3_header[3]) {
		case 1:
		case 3:
			return AT3_TYPE;
			break;
		case 2:
			return MP3_TYPE;
			break;
		default:
			return UNK_TYPE;
			break;
	}
}

const char *getFileExt(const char *filename)
{
	size_t len = strlen(filename);
	const char *p = filename + len;

	while (p > filename && *p != '.' && *p != '/')
		p--;
	if (*p == '.')
		return p + 1;
	else
		return NULL;
}

/**
 * Set up Music Driver by filename and useME flag
 * @param filename the path of playing file
 * @param useME whether to use media engine
 */
void setAudioFunctions(char *filename, int useME_MP3)
{
	const char *ext = getFileExt(filename);

	if (ext == NULL) {
		return;
	}

	if (!stricmp(ext, "ogg")) {
		//OGG Vorbis
		g_MusicDriver.initFunct = OGG_Init;
		g_MusicDriver.loadFunct = OGG_Load;
		g_MusicDriver.playFunct = OGG_Play;
		g_MusicDriver.pauseFunct = OGG_Pause;
		g_MusicDriver.endFunct = OGG_End;
		g_MusicDriver.setVolumeBoostTypeFunct = OGG_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = OGG_setVolumeBoost;
		g_MusicDriver.getInfoFunct = OGG_GetInfo;
		g_MusicDriver.getTagInfoFunct = OGG_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = OGG_GetTimeString;
		g_MusicDriver.getPercentageFunct = OGG_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = OGG_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = OGG_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = OGG_EndOfStream;
		g_MusicDriver.setMuteFunct = OGG_setMute;
		g_MusicDriver.setFilterFunct = OGG_setFilter;
		g_MusicDriver.enableFilterFunct = OGG_enableFilter;
		g_MusicDriver.disableFilterFunct = OGG_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = OGG_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = OGG_isFilterSupported;

		g_MusicDriver.suspendFunct = OGG_suspend;
		g_MusicDriver.resumeFunct = OGG_resume;
		g_MusicDriver.fadeOutFunct = OGG_fadeOut;
		g_MusicDriver.getTimer = OGG_getTimer;
	} else if (!stricmp(ext, "mp3") && useME_MP3) {
		//MP3 via Media Engine
		g_MusicDriver.initFunct = MP3ME_Init;
		g_MusicDriver.loadFunct = MP3ME_Load;
		g_MusicDriver.playFunct = MP3ME_Play;
		g_MusicDriver.pauseFunct = MP3ME_Pause;
		g_MusicDriver.endFunct = MP3ME_End;
		g_MusicDriver.setVolumeBoostTypeFunct = MP3ME_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = MP3ME_setVolumeBoost;
		g_MusicDriver.getInfoFunct = MP3ME_GetInfo;
		g_MusicDriver.getTagInfoFunct = MP3ME_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = MP3ME_GetTimeString;
		g_MusicDriver.getPercentageFunct = MP3ME_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = MP3ME_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = MP3ME_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = MP3ME_EndOfStream;

		g_MusicDriver.setMuteFunct = MP3ME_setMute;
		g_MusicDriver.setFilterFunct = MP3ME_setFilter;
		g_MusicDriver.enableFilterFunct = MP3ME_enableFilter;
		g_MusicDriver.disableFilterFunct = MP3ME_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = MP3ME_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = MP3ME_isFilterSupported;

		g_MusicDriver.suspendFunct = MP3ME_suspend;
		g_MusicDriver.resumeFunct = MP3ME_resume;
		g_MusicDriver.fadeOutFunct = MP3ME_fadeOut;

		g_MusicDriver.backwardSeekFunct = MP3ME_Backward;
		g_MusicDriver.forwardSeekFunct = MP3ME_Forward;
		g_MusicDriver.getTimer = MP3ME_getTimer;

		return;
	} else if (!stricmp(ext, "mp3")) {
		/// MP3 via LibMad
		g_MusicDriver.initFunct = MP3_Init;
		g_MusicDriver.loadFunct = MP3_Load;
		g_MusicDriver.playFunct = MP3_Play;
		g_MusicDriver.pauseFunct = MP3_Pause;
		g_MusicDriver.endFunct = MP3_End;
		g_MusicDriver.setVolumeBoostTypeFunct = MP3_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = MP3_setVolumeBoost;
		g_MusicDriver.getInfoFunct = MP3_GetInfo;
		g_MusicDriver.getTagInfoFunct = MP3_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = MP3_GetTimeString;
		g_MusicDriver.getPercentageFunct = MP3_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = MP3_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = MP3_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = MP3_EndOfStream;

		g_MusicDriver.setMuteFunct = MP3_setMute;
		g_MusicDriver.setFilterFunct = MP3_setFilter;
		g_MusicDriver.enableFilterFunct = MP3_enableFilter;
		g_MusicDriver.disableFilterFunct = MP3_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = MP3_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = MP3_isFilterSupported;

		g_MusicDriver.suspendFunct = MP3_suspend;
		g_MusicDriver.resumeFunct = MP3_resume;
		g_MusicDriver.fadeOutFunct = MP3_fadeOut;

		g_MusicDriver.getTimer = MP3_getTimer;
	} else if (!stricmp(ext, "aa3") || !stricmp(ext, "oma")
			   || !stricmp(ext, "omg")) {
		/// AA3
		g_MusicDriver.initFunct = AA3ME_Init;
		g_MusicDriver.loadFunct = AA3ME_Load;
		g_MusicDriver.playFunct = AA3ME_Play;
		g_MusicDriver.pauseFunct = AA3ME_Pause;
		g_MusicDriver.endFunct = AA3ME_End;
		g_MusicDriver.setVolumeBoostTypeFunct = AA3ME_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = AA3ME_setVolumeBoost;
		g_MusicDriver.getInfoFunct = AA3ME_GetInfo;
		g_MusicDriver.getTagInfoFunct = AA3ME_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = AA3ME_GetTimeString;
		g_MusicDriver.getPercentageFunct = AA3ME_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = AA3ME_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = AA3ME_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = AA3ME_EndOfStream;

		g_MusicDriver.setMuteFunct = AA3ME_setMute;
		g_MusicDriver.setFilterFunct = AA3ME_setFilter;
		g_MusicDriver.enableFilterFunct = AA3ME_enableFilter;
		g_MusicDriver.disableFilterFunct = AA3ME_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = AA3ME_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = AA3ME_isFilterSupported;

		g_MusicDriver.suspendFunct = AA3ME_suspend;
		g_MusicDriver.resumeFunct = AA3ME_resume;
		g_MusicDriver.fadeOutFunct = AA3ME_fadeOut;
		g_MusicDriver.getTimer = AA3ME_getTimer;
	} else if (!stricmp(ext, "flac")) {
#if 0
		/// FLAC
		g_MusicDriver.initFunct = FLAC_Init;
		g_MusicDriver.loadFunct = FLAC_Load;
		g_MusicDriver.playFunct = FLAC_Play;
		g_MusicDriver.pauseFunct = FLAC_Pause;
		g_MusicDriver.endFunct = FLAC_End;
		g_MusicDriver.setVolumeBoostTypeFunct = FLAC_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = FLAC_setVolumeBoost;
		g_MusicDriver.getInfoFunct = FLAC_GetInfo;
		g_MusicDriver.getTagInfoFunct = FLAC_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = FLAC_GetTimeString;
		g_MusicDriver.getPercentageFunct = FLAC_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = FLAC_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = FLAC_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = FLAC_EndOfStream;

		g_MusicDriver.setMuteFunct = FLAC_setMute;
		g_MusicDriver.setFilterFunct = FLAC_setFilter;
		g_MusicDriver.enableFilterFunct = FLAC_enableFilter;
		g_MusicDriver.disableFilterFunct = FLAC_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = FLAC_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = FLAC_isFilterSupported;

		g_MusicDriver.suspendFunct = FLAC_suspend;
		g_MusicDriver.resumeFunct = FLAC_resume;
		g_MusicDriver.fadeOutFunct = FLAC_fadeOut;
#endif
	} else if (!stricmp(ext, "wma")) {
#if 0
		/// WMA
		g_MusicDriver.initFunct = WMA_Init;
		g_MusicDriver.loadFunct = WMA_Load;
		g_MusicDriver.playFunct = WMA_Play;
		g_MusicDriver.pauseFunct = WMA_Pause;
		g_MusicDriver.endFunct = WMA_End;
		g_MusicDriver.setVolumeBoostTypeFunct = WMA_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = WMA_setVolumeBoost;
		g_MusicDriver.getInfoFunct = WMA_GetInfo;
		g_MusicDriver.getTagInfoFunct = WMA_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = WMA_GetTimeString;
		g_MusicDriver.getPercentageFunct = WMA_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = WMA_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = WMA_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = WMA_EndOfStream;

		g_MusicDriver.setMuteFunct = WMA_setMute;
		g_MusicDriver.setFilterFunct = WMA_setFilter;
		g_MusicDriver.enableFilterFunct = WMA_enableFilter;
		g_MusicDriver.disableFilterFunct = WMA_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = WMA_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = WMA_isFilterSupported;

		g_MusicDriver.suspendFunct = WMA_suspend;
		g_MusicDriver.resumeFunct = WMA_resume;
		g_MusicDriver.fadeOutFunct = WMA_fadeOut;

		g_MusicDriver.backwardSeekFunct = WMA_Backward;
		g_MusicDriver.forwardSeekFunct = WMA_Forward;

		g_MusicDriver.getTimer = WMA_getTimer;
		return;
#endif
	} else if (!stricmp(ext, "mpc")) {
		/// MPC
		g_MusicDriver.initFunct = MPC_Init;
		g_MusicDriver.loadFunct = MPC_Load;
		g_MusicDriver.playFunct = MPC_Play;
		g_MusicDriver.pauseFunct = MPC_Pause;
		g_MusicDriver.endFunct = MPC_End;
		g_MusicDriver.setVolumeBoostTypeFunct = MPC_setVolumeBoostType;
		g_MusicDriver.setVolumeBoostFunct = MPC_setVolumeBoost;
		g_MusicDriver.getInfoFunct = MPC_GetInfo;
		g_MusicDriver.getTagInfoFunct = MPC_GetTagInfoOnly;
		g_MusicDriver.getTimeStringFunct = MPC_GetTimeString;
		g_MusicDriver.getPercentageFunct = MPC_GetPercentage;
		g_MusicDriver.getPlayingSpeedFunct = MPC_getPlayingSpeed;
		g_MusicDriver.setPlayingSpeedFunct = MPC_setPlayingSpeed;
		g_MusicDriver.endOfStreamFunct = MPC_EndOfStream;

		g_MusicDriver.setMuteFunct = MPC_setMute;
		g_MusicDriver.setFilterFunct = MPC_setFilter;
		g_MusicDriver.enableFilterFunct = MPC_enableFilter;
		g_MusicDriver.disableFilterFunct = MPC_disableFilter;
		g_MusicDriver.isFilterEnabledFunct = MPC_isFilterEnabled;
		g_MusicDriver.isFilterSupportedFunct = MPC_isFilterSupported;

		g_MusicDriver.suspendFunct = MPC_suspend;
		g_MusicDriver.resumeFunct = MPC_resume;
		g_MusicDriver.fadeOutFunct = MPC_fadeOut;

		g_MusicDriver.backwardSeekFunct = MPC_Backward;
		g_MusicDriver.forwardSeekFunct = MPC_Forward;
		g_MusicDriver.getTimer = MPC_getTimer;
		return;
	}

	g_MusicDriver.backwardSeekFunct = NULL;
	g_MusicDriver.forwardSeekFunct = NULL;
}

/// Unset MusicDriver
void unsetAudioFunctions()
{
	g_MusicDriver.initFunct = NULL;
	g_MusicDriver.loadFunct = NULL;
	g_MusicDriver.playFunct = NULL;
	g_MusicDriver.pauseFunct = NULL;
	g_MusicDriver.endFunct = NULL;
	g_MusicDriver.setVolumeBoostTypeFunct = NULL;
	g_MusicDriver.setVolumeBoostFunct = NULL;
	g_MusicDriver.getInfoFunct = NULL;
	g_MusicDriver.getTagInfoFunct = NULL;
	g_MusicDriver.getTimeStringFunct = NULL;
	g_MusicDriver.getPercentageFunct = NULL;
	g_MusicDriver.getPlayingSpeedFunct = NULL;
	g_MusicDriver.setPlayingSpeedFunct = NULL;
	g_MusicDriver.endOfStreamFunct = NULL;

	g_MusicDriver.setMuteFunct = NULL;
	g_MusicDriver.setFilterFunct = NULL;
	g_MusicDriver.enableFilterFunct = NULL;
	g_MusicDriver.disableFilterFunct = NULL;
	g_MusicDriver.isFilterEnabledFunct = NULL;
	g_MusicDriver.isFilterSupportedFunct = NULL;

	g_MusicDriver.suspendFunct = NULL;
	g_MusicDriver.resumeFunct = NULL;

	g_MusicDriver.backwardSeekFunct = NULL;
	g_MusicDriver.forwardSeekFunct = NULL;
}

/**
 * Initial current driver
 * @param channel channel number for pspAudioSetChannelCallback
 */
void MusicDriver_initial(int channel)
{
	if (g_MusicDriver.initFunct != NULL) {
		(*g_MusicDriver.initFunct) (channel);
	}
}

/**
 * Ask current driver for loading file
 * @param filename path to the music 
 * @return state flag
 * - OPENING_OK OK
 * - ERROR_OPENING Can't open file
 * - ERROR_INVALID_SAMPLE_RATE Can't set PSP audio sample rate to that value
 * - ERROR_CREATE_THREAD Can't create the decode thread
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_load(char *filename)
{
	if (g_MusicDriver.loadFunct == NULL)
		return ERROR_MUSIC_DRIVER;

	return (*g_MusicDriver.loadFunct) (filename);
}

/**
 * Set music to be played
 * @return
 * - TRUE Now music is playing
 * - FALSE Music is already playing
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_play()
{
	if (g_MusicDriver.playFunct == NULL)
		return ERROR_MUSIC_DRIVER;

	return (*g_MusicDriver.playFunct) ();
}

/**
 * Set music to be paused
 */
void MusicDriver_pause()
{
	if (g_MusicDriver.pauseFunct == NULL)
		return;

	return (*g_MusicDriver.pauseFunct) ();
}

/**
 * Set music to be end, including stop, release audio channel, and release resources
 */
void MusicDriver_end()
{
	if (g_MusicDriver.endFunct == NULL)
		return;

	return (*g_MusicDriver.endFunct) ();
}

/**
 * Set music volume boost type
 * @param type type string
 * - "OLD" old boost algorithm
 * - "NEW" new boost algorithm
 */
void MusicDriver_setVolumeBoostType(char *type)
{
	if (g_MusicDriver.setVolumeBoostTypeFunct == NULL)
		return;

	return (*g_MusicDriver.setVolumeBoostTypeFunct) (type);
}

/**
 * Set music volume boost value
 * @param boost boost value
 */
void MusicDriver_setVolumeBoost(int boost)
{
	if (g_MusicDriver.setVolumeBoostFunct == NULL)
		return;

	return (*g_MusicDriver.setVolumeBoostFunct) (boost);
}

/**
 * Get current music information
 * @return a struct fileInfo conatins the information of the music
 */
struct fileInfo MusicDriver_getInfo()
{
	if (g_MusicDriver.getInfoFunct == NULL) {
		struct fileInfo f;

		memset(&f, 0, sizeof(f));
		return f;
	}

	return (*g_MusicDriver.getInfoFunct) ();
}

/**
 * Get current music tag information
 * @return a struct fileInfo conatins the information of the music
 */
struct fileInfo MusicDriver_getTagInfo()
{
	if (g_MusicDriver.getTagInfoFunct == NULL) {
		struct fileInfo f;

		memset(&f, 0, sizeof(f));
		return f;
	}

	return (*g_MusicDriver.getTagInfoFunct) ();
}

/** 
 * Get current playback time string
 * @param dest buffer to receive string
 * @note  dest must be enough length
 */
void MusicDriver_getTimeString(char *dest)
{
	if (g_MusicDriver.getTimeStringFunct == NULL) {
		return;
	}

	return (*g_MusicDriver.getTimeStringFunct) (dest);
}

/**
 * Get current playback music percent, in deical number 0-100
 * @return percent number in 0 - 100, or
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_getPercentage()
{
	if (g_MusicDriver.getPercentageFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.getPercentageFunct) ();
}

/**
 * Get current music playing speed ratio
 * @return playing speed ratio, or
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_getPlayingSpeed()
{
	if (g_MusicDriver.getPlayingSpeedFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.getPlayingSpeedFunct) ();
}

/**
 * Set current music playing speed ratio
 * @param speed new playing speed ratio
 * @return
 * - 0 Success
 * - -1 Bad argument
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_setPlayingSpeed(int speed)
{
	if (g_MusicDriver.setPlayingSpeedFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.setPlayingSpeedFunct) (speed);
}

/**
 * Get whether is reached the end of the music stream
 * @return
 * - TRUE The end is reached
 * - FALSE The is is not reached
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_endOfStream()
{
	if (g_MusicDriver.endOfStreamFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.endOfStreamFunct) ();
}

/**
 * Set whether is mute the audio channel
 * @param mute
 * - TRUE Set the audio channel off
 * - FALSE Set the audio channel on
 * @return
 * - 0 This operation is OK
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_setMute(int mute)
{
	if (g_MusicDriver.setMuteFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.setMuteFunct) (mute);
}

/**
 * Set the audio filter ( equalizer ) value
 * @param filter the equalizer filter array
 * @param copyFilter whether music driver copy to its own store
 * @return
 * - 0 One of filter array value is set too high
 * - 1 This operation is OK
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_setFilter(double filter[32], int copyFilter)
{
	if (g_MusicDriver.setFilterFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.setFilterFunct) (filter, copyFilter);
}

/**
 * Enable the use of filter ( equalizer )
 */
void MusicDriver_enableFilter()
{
	if (g_MusicDriver.enableFilterFunct == NULL) {
		return;
	}

	(*g_MusicDriver.enableFilterFunct) ();
}

/**
 * Disable the use of filter ( equalizer )
 */
void MusicDriver_disableFilter()
{
	if (g_MusicDriver.disableFilterFunct == NULL) {
		return;
	}

	(*g_MusicDriver.disableFilterFunct) ();
}

/**
 * Get whether the filter is enable
 * @return
 * - TRUE The filter is enable.
 * - FALSE The filter is disable.
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_isFilterEnable()
{
	if (g_MusicDriver.isFilterEnabledFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.isFilterEnabledFunct) ();
}

/**
 * Get whether the filter is supported
 * @return
 * - TRUE The filter is supported.
 * - FALSE The filter is supported.
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 */
int MusicDriver_isFilterSupported()
{
	if (g_MusicDriver.isFilterSupportedFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.isFilterSupportedFunct) ();
}

/**
 * Suspend the playback of the music
 * @return
 * - 0 This operation is OK
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 * @note mp3/mp3me didn't support this function
 */
int MusicDriver_suspend()
{
	if (g_MusicDriver.suspendFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.suspendFunct) ();
}

/**
 * Resume the playback of the music
 * @return
 * - 0 This operation is OK
 * - ERROR_MUSIC_DRIVER If music driver didn't setup completely
 * @note mp3/mp3me didn't support this function
 */
int MusicDriver_resume()
{
	if (g_MusicDriver.resumeFunct == NULL) {
		return ERROR_MUSIC_DRIVER;
	}

	return (*g_MusicDriver.resumeFunct) ();
}

/**
 * Set fade out effect duartion seconds
 * @param seconds fade out effect duartion time in seconds
 */
void MusicDriver_fadeOut(float seconds)
{
	if (g_MusicDriver.fadeOutFunct == NULL) {
		return;
	}

	(*g_MusicDriver.fadeOutFunct) (seconds);
}

/// Backward seek the music by 5 seconds
extern void MusicDriver_backward(void)
{
	if (g_MusicDriver.backwardSeekFunct == NULL) {
		return;
	}

	(*g_MusicDriver.backwardSeekFunct) (5.0);
}

/// Forward seek the music by 5 seconds
extern void MusicDriver_forward(void)
{
	if (g_MusicDriver.forwardSeekFunct == NULL) {
		return;
	}

	(*g_MusicDriver.forwardSeekFunct) (5.0);
}

/// Get playback Timer
extern mad_timer_t MusicDriver_getTimer(void)
{
	if (g_MusicDriver.getTimer == NULL) {
		return mad_timer_zero;
	}

	return (*g_MusicDriver.getTimer) ();
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

/// Set frequency for output:
int setAudioFrequency(unsigned short samples, unsigned short freq, char car)
{
	return sceAudioSRCChReserve(samples, freq, car);
}

/// Release audio:
int sceAudioOutput2GetRestSample();

/// In the 3.xx firmwares you have to wait until sceAudioOutput2GetRestSample
/// returns 0 before calling sceAudioSRCChRelease. Also sceAudioOutput2OutputBlocking 
/// only calls sceAudioSRCOutputBlocking, sceAudioOutput2Release only calls sceAudioSRCChRelease and 
/// buggy, may struck here
int releaseAudio(void)
{
	while (sceAudioOutput2GetRestSample() > 0);
	return sceAudioSRCChRelease();
}

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
