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
//    $Id: mp3player.c 62 2008-02-22 11:49:49Z hrimfaxi $
//

#include <pspiofilemgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#include "id3.h"
#include "player.h"
#include "mp3player.h"
#include "xmp3audiolib.h"
#include "pspaudio_kernel.h"
#include "mp3info.h"
#include "strsafe.h"

#define FALSE 0
#define TRUE !FALSE

/* This table represents the subband-domain filter characteristics. It
* is initialized by the ParseArgs() function and is used as
* coefficients against each subband samples when DoFilter is non-nul.
*/
mad_fixed_t Filter[32];
double filterDouble[32] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0
};

/* DoFilter is non-nul when the Filter table defines a filter bank to
* be applied to the decoded audio subbands.
*/
int DoFilter = 0;

struct mad_stream Stream;
struct mad_header Header;
struct mad_frame Frame;
struct mad_synth Synth;
int i;

///  The following variables are maintained and updated by the tracker during playback
static int isPlaying;			// Set to true when a mod is being played

typedef struct
{
	short left;
	short right;
} Sample;

static int myChannel;
static int eos;

struct fileInfo MP3_info;

static t_mp3info g_mp3_info;

#define BOOST_OLD 0
#define BOOST_NEW 1

char MP3_fileName[264];
int MP3_volume_boost_type = BOOST_NEW;
double MP3_volume_boost = 0.0;
unsigned int MP3_volume_boost_old = 0;
double DB_forBoost = 1.0;
int MP3_playingSpeed = 0;		// 0 = normal
int MP3_defaultCPUClock = 70;
int MP3_fd = -1;

#define INPUT_BUFFER_SIZE 2048
unsigned char fileBuffer[INPUT_BUFFER_SIZE];
unsigned int samplesRead;
unsigned int filePos;
double fileSize;

///  Applies a frequency-domain filter to audio data in the subband-domain.
static void ApplyFilter(struct mad_frame *Frame)
{
	int Channel, Sample, Samples, SubBand;

	/* There is two application loops, each optimized for the number
	 * of audio channels to process. The first alternative is for
	 * two-channel frames, the second is for mono-audio.
	 */
	Samples = MAD_NSBSAMPLES(&Frame->header);
	if (Frame->header.mode != MAD_MODE_SINGLE_CHANNEL)
		for (Channel = 0; Channel < 2; Channel++)
			for (Sample = 0; Sample < Samples; Sample++)
				for (SubBand = 0; SubBand < 32; SubBand++)
					Frame->sbsample[Channel][Sample][SubBand] =
						mad_f_mul(Frame->sbsample[Channel][Sample][SubBand],
								  Filter[SubBand]);
	else
		for (Sample = 0; Sample < Samples; Sample++)
			for (SubBand = 0; SubBand < 32; SubBand++)
				Frame->sbsample[0][Sample][SubBand] =
					mad_f_mul(Frame->sbsample[0][Sample][SubBand],
							  Filter[SubBand]);
}

///  Converts a sample from libmad's fixed point number format to a signed
///  short (16 bits).
short convertSample(mad_fixed_t sample)
{
	// round
	sample += (1L << (MAD_F_FRACBITS - 16));

	// clip
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	// quantize
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/// Streaming functions (adapted from Ghoti's MusiceEngine.c):
int fillFileBuffer()
{
	// Find out how much to keep and how much to fill.
	const unsigned int bytesToKeep = Stream.bufend - Stream.next_frame;
	unsigned int bytesToFill = sizeof(fileBuffer) - bytesToKeep;

	// Want to keep any bytes?
	if (bytesToKeep)
		memmove(fileBuffer, fileBuffer + sizeof(fileBuffer) - bytesToKeep,
				bytesToKeep);

	// Read into the rest of the file buffer.
	unsigned char *bufferPos = fileBuffer + bytesToKeep;

	while (bytesToFill > 0) {
		const unsigned int bytesRead =
			sceIoRead(MP3_fd, bufferPos, bytesToFill);

		// EOF?
		if (bytesRead == 0)
			return 2;

		// Adjust where we're writing to.
		bytesToFill -= bytesRead;
		bufferPos += bytesRead;
		filePos += bytesRead;
	}
	return 0;
}

void decode()
{
	while ((mad_frame_decode(&Frame, &Stream) == -1)
		   && ((Stream.error == MAD_ERROR_BUFLEN)
			   || (Stream.error == MAD_ERROR_BUFPTR))) {
		int tmp;

		tmp = fillFileBuffer();
		if (tmp == 2)
			eos = 1;
		mad_stream_buffer(&Stream, fileBuffer, sizeof(fileBuffer));
	}
	//Equalizers and volume boost (NEW METHOD):
	if (DoFilter || MP3_volume_boost)
		ApplyFilter(&Frame);

	mad_timer_add(&MP3_info.timer, Frame.header.duration);
	mad_synth_frame(&Synth, &Frame);
}

void convertLeftSamples(Sample * first, Sample * last, const mad_fixed_t * src)
{
	Sample *dst;

	for (dst = first; dst != last; ++dst) {
		dst->left = convertSample(*src++);
		//Volume Boost (OLD METHOD):
		if (MP3_volume_boost_old)
			dst->left = volume_boost(&dst->left, &MP3_volume_boost_old);
	}
}

void convertRightSamples(Sample * first, Sample * last, const mad_fixed_t * src)
{
	Sample *dst;

	for (dst = first; dst != last; ++dst) {
		dst->right = convertSample(*src++);
		//Volume Boost (OLD METHOD):
		if (MP3_volume_boost_old)
			dst->right = volume_boost(&dst->right, &MP3_volume_boost_old);
	}
}

/// MP3 Callback for audio:
static void MP3Callback(void *buffer, unsigned int samplesToWrite, void *pdata)
{
	Sample *destination = (Sample *) buffer;

	if (isPlaying == TRUE) {	//  Playing , so mix up a buffer
		while (samplesToWrite > 0) {
			const unsigned int samplesAvailable =
				Synth.pcm.length - samplesRead;
			if (samplesAvailable > samplesToWrite) {
				convertLeftSamples(destination, destination + samplesToWrite,
								   &Synth.pcm.samples[0][samplesRead]);
				convertRightSamples(destination, destination + samplesToWrite,
									&Synth.pcm.samples[1][samplesRead]);

				samplesRead += samplesToWrite;
				samplesToWrite = 0;

				//Check for playing speed:
				if (MP3_playingSpeed) {
					if (sceIoLseek32
						(MP3_fd, INPUT_BUFFER_SIZE * MP3_playingSpeed,
						 PSP_SEEK_CUR) != filePos)
						filePos += INPUT_BUFFER_SIZE * MP3_playingSpeed;
					else
						MP3_setPlayingSpeed(0);
				}
			} else {
				convertLeftSamples(destination, destination + samplesAvailable,
								   &Synth.pcm.samples[0][samplesRead]);
				convertRightSamples(destination, destination + samplesAvailable,
									&Synth.pcm.samples[1][samplesRead]);

				samplesRead = 0;
				decode();

				destination += samplesAvailable;
				samplesToWrite -= samplesAvailable;
			}
		}
	} else {					//  Not Playing , so clear buffer
		int count;

		for (count = 0; count < samplesToWrite; count++) {
			destination[count].left = 0;
			destination[count].right = 0;
		}
	}
}

/// Init:
void MP3_Init(int channel)
{
	initAudioLib();
	myChannel = channel;
	isPlaying = FALSE;
	MP3_playingSpeed = 0;
	MP3_volume_boost = 0;
	MP3_volume_boost_old = 0;

	xMP3AudioSetChannelCallback(myChannel, MP3Callback, 0);

	MIN_PLAYING_SPEED = -10;
	MAX_PLAYING_SPEED = 9;

	/* First the structures used by libmad must be initialized. */
	mad_stream_init(&Stream);
	mad_header_init(&Header);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_timer_reset(&MP3_info.timer);
}

/// Free tune
void MP3_FreeTune()
{
	sceIoClose(MP3_fd);
	MP3_fd = -1;
	/* Mad is no longer used, the structures that were initialized must
	 * now be cleared.
	 */
	mad_synth_finish(&Synth);
	mad_header_finish(&Header);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);
}

/// Recupero le informazioni sul file:
void getMP3TagInfo(char *filename, struct fileInfo *targetInfo)
{
	/*
	   //ID3:
	   struct ID3Tag ID3;

	   strcpy(MP3_fileName, filename);
	   ID3 = ParseID3(filename);
	   strcpy(targetInfo->title, ID3.ID3Title);
	   strcpy(targetInfo->artist, ID3.ID3Artist);
	   strcpy(targetInfo->album, ID3.ID3Album);
	   strcpy(targetInfo->year, ID3.ID3Year);
	   strcpy(targetInfo->genre, ID3.ID3GenreText);
	   strcpy(targetInfo->trackNumber, ID3.ID3TrackText);
	 */

	STRCPY_S(targetInfo->title, g_mp3_info.title);
	STRCPY_S(targetInfo->artist, g_mp3_info.artist);
}

/// Seek next valid frame
/// NOTE: this function comes from Music prx 0.55 source
///       all credits goes to joek2100.
int SeekNextFrameMP3(SceUID fd)
{
	int offset = 0;
	unsigned char buf[1024];
	unsigned char *pBuffer;
	int i;
	int size = 0;

	offset = sceIoLseek32(fd, 0, PSP_SEEK_CUR);
	sceIoRead(fd, buf, sizeof(buf));
	if (!strncmp((char *) buf, "ID3", 3) || !strncmp((char *) buf, "ea3", 3))	//skip past id3v2 header, which can cause a false sync to be found
	{
		//get the real size from the syncsafe int
		size = buf[6];
		size = (size << 7) | buf[7];
		size = (size << 7) | buf[8];
		size = (size << 7) | buf[9];

		size += 10;

		if (buf[5] & 0x10)		//has footer
			size += 10;
	}

	sceIoLseek32(fd, offset, PSP_SEEK_SET);	//now seek for a sync
	while (1) {
		offset = sceIoLseek32(fd, 0, PSP_SEEK_CUR);
		size = sceIoRead(fd, buf, sizeof(buf));

		if (size <= 2)			//at end of file
			return -1;

		if (!strncmp((char *) buf, "EA3", 3))	//oma mp3 files have non-safe ints in the EA3 header
		{
			sceIoLseek32(fd, (buf[4] << 8) + buf[5], PSP_SEEK_CUR);
			continue;
		}

		pBuffer = buf;
		for (i = 0; i < size; i++) {
			//if this is a valid frame sync (0xe0 is for mpeg version 2.5,2+1)
			if ((pBuffer[i] == 0xff) && ((pBuffer[i + 1] & 0xE0) == 0xE0)) {
				offset += i;
				sceIoLseek32(fd, offset, PSP_SEEK_SET);
				return offset;
			}
		}
		//go back two bytes to catch any syncs that on the boundary
		sceIoLseek32(fd, -2, PSP_SEEK_CUR);
	}
}

int MP3getInfo()
{
	unsigned long FrameCount = 0;
	int fd;
	int bufferSize = 1024 * 500;
	u8 *localBuffer;
	long singleDataRed = 0;
	struct mad_stream stream;
	struct mad_header header;
	int timeFromID3 = 0;
	float mediumBitrate = 0;

	getMP3TagInfo(MP3_fileName, &MP3_info);

	mad_stream_init(&stream);
	mad_header_init(&header);

	fd = sceIoOpen(MP3_fileName, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return -1;

	long size = sceIoLseek(fd, 0, PSP_SEEK_END);

	sceIoLseek(fd, 0, PSP_SEEK_SET);

	double startPos = ID3v2TagSize(MP3_fileName);

	sceIoLseek32(fd, startPos, PSP_SEEK_SET);
	startPos = SeekNextFrameMP3(fd);
	size -= startPos;

	if (size < bufferSize * 3)
		bufferSize = size;
	localBuffer = (unsigned char *) malloc(bufferSize);

	MP3_info.fileType = MP3_TYPE;
	MP3_info.defaultCPUClock = MP3_defaultCPUClock;
	MP3_info.needsME = 0;
	MP3_info.fileSize = size;
	MP3_info.framesDecoded = 0;

	double totalBitrate = 0;
	int i = 0;

	for (i = 0; i < 3; i++) {
		memset(localBuffer, 0, bufferSize);
		singleDataRed = sceIoRead(fd, localBuffer, bufferSize);
		mad_stream_buffer(&stream, localBuffer, singleDataRed);

		while (1) {
			if (mad_header_decode(&header, &stream) == -1) {
				if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN)
					break;
				else if (MAD_RECOVERABLE(stream.error)) {
					continue;
				} else {
					break;
				}
			}
			//Informazioni solo dal primo frame:
			if (FrameCount++ == 0) {
				switch (header.layer) {
					case MAD_LAYER_I:
						strcpy(MP3_info.layer, "I");
						break;
					case MAD_LAYER_II:
						strcpy(MP3_info.layer, "II");
						break;
					case MAD_LAYER_III:
						strcpy(MP3_info.layer, "III");
						break;
					default:
						strcpy(MP3_info.layer, "unknown");
						break;
				}

				MP3_info.kbit = header.bitrate / 1000;
				MP3_info.instantBitrate = header.bitrate;
				MP3_info.hz = header.samplerate;
				switch (header.mode) {
					case MAD_MODE_SINGLE_CHANNEL:
						strcpy(MP3_info.mode, "single channel");
						break;
					case MAD_MODE_DUAL_CHANNEL:
						strcpy(MP3_info.mode, "dual channel");
						break;
					case MAD_MODE_JOINT_STEREO:
						strcpy(MP3_info.mode, "joint (MS/intensity) stereo");
						break;
					case MAD_MODE_STEREO:
						strcpy(MP3_info.mode, "normal LR stereo");
						break;
					default:
						strcpy(MP3_info.mode, "unknown");
						break;
				}

				switch (header.emphasis) {
					case MAD_EMPHASIS_NONE:
						strcpy(MP3_info.emphasis, "no");
						break;
					case MAD_EMPHASIS_50_15_US:
						strcpy(MP3_info.emphasis, "50/15 us");
						break;
					case MAD_EMPHASIS_CCITT_J_17:
						strcpy(MP3_info.emphasis, "CCITT J.17");
						break;
					case MAD_EMPHASIS_RESERVED:
						strcpy(MP3_info.emphasis, "reserved(!)");
						break;
					default:
						strcpy(MP3_info.emphasis, "unknown");
						break;
				}

				//Check if lenght found in tag info:
				if (MP3_info.length > 0) {
					timeFromID3 = 1;
					break;
				}
			}
			//Controllo il cambio di sample rate (ma non dovrebbe succedere)
			if (header.samplerate > MP3_info.hz)
				MP3_info.hz = header.samplerate;

			totalBitrate += header.bitrate;
			if (size == bufferSize)
				break;
			else if (i == 0)
				sceIoLseek(fd, startPos + size / 3, PSP_SEEK_SET);
			else if (i == 1)
				sceIoLseek(fd, startPos + 2 * size / 3, PSP_SEEK_SET);
		}
	}
	mad_header_finish(&header);
	mad_stream_finish(&stream);
	if (localBuffer)
		free(localBuffer);
	sceIoClose(fd);

	mediumBitrate = totalBitrate / (float) FrameCount;
	int secs = 0;

	if (!MP3_info.length) {
		secs = size * 8 / mediumBitrate;
		MP3_info.length = secs;
	} else {
		secs = MP3_info.length;
	}

	//Formatto in stringa la durata totale:
	int h = secs / 3600;
	int m = (secs - h * 3600) / 60;
	int s = secs - h * 3600 - m * 60;

	snprintf(MP3_info.strLength, sizeof(MP3_info.strLength),
			 "%2.2i:%2.2i:%2.2i", h, m, s);

	return 0;
}

/// MP3_End
void MP3_End()
{
	MP3_Stop();
	xMP3AudioSetChannelCallback(myChannel, 0, 0);
	MP3_FreeTune();
	endAudioLib();
}

/// Load mp3 into memory:
int MP3_Load(char *filename)
{
	eos = 0;
	filePos = 0;
	fileSize = 0;
	samplesRead = 0;
	initFileInfo(&MP3_info);
	MP3_fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if (MP3_fd < 0)
		return ERROR_OPENING;

	extern int g_libid3tag_found_id3v2;
	extern bool g_bID3v1Hack;

	g_bID3v1Hack = false;

	readAPETag(&g_mp3_info, filename);
	if (g_mp3_info.found_apetag == false)
		readID3Tag(&g_mp3_info, filename);
	else
		g_bID3v1Hack = true;

	if (g_libid3tag_found_id3v2) {
		g_bID3v1Hack = true;
	}

	if (mp3info_read(&g_mp3_info, fd) == false) {
		STRCPY_S(filename, "");
		return ERROR_OPENING;
	}

	fileSize = sceIoLseek32(MP3_fd, 0, PSP_SEEK_END);
	sceIoLseek32(MP3_fd, 0, PSP_SEEK_SET);
	int startPos = ID3v2TagSize(filename);

	sceIoLseek32(MP3_fd, startPos, PSP_SEEK_SET);
	startPos = SeekNextFrameMP3(MP3_fd);

	isPlaying = FALSE;

	strcpy(MP3_fileName, filename);
	if (MP3getInfo() != 0) {
		strcpy(MP3_fileName, "");
		sceIoClose(MP3_fd);
		MP3_fd = -1;
		return ERROR_OPENING;
	}
	//Controllo il sample rate:
	if (xMP3AudioSetFrequency(MP3_info.hz) < 0)
		return ERROR_INVALID_SAMPLE_RATE;
	return OPENING_OK;
}

///  This function initialises for playing, and starts
int MP3_Play()
{
	//Azzero il timer:
	if (eos == 1) {
		mad_timer_reset(&MP3_info.timer);
	}
	// See if I'm already playing
	if (isPlaying)
		return FALSE;

	isPlaying = TRUE;

	return TRUE;
}

///  Pause:
void MP3_Pause()
{
	isPlaying = !isPlaying;
}

///  Stop:
int MP3_Stop()
{
	//stop playing
	isPlaying = FALSE;

	return TRUE;
}

/// Get time string
void MP3_GetTimeString(char *dest)
{
	mad_timer_string(MP3_info.timer, dest, "%02lu:%02u:%02u", MAD_UNITS_HOURS,
					 MAD_UNITS_MILLISECONDS, 0);
}

///  Get Percentage
int MP3_GetPercentage()
{
	//Calcolo posizione in %:
	float perc;

	if (fileSize > 0) {
		perc = (float) filePos / (float) fileSize *100.0;
	} else {
		perc = 0;
	}
	return ((int) perc);
}

/// Check EOS
int MP3_EndOfStream()
{
	if (eos == 1)
		return 1;
	return 0;
}

/// Get info on file:
struct fileInfo MP3_GetInfo()
{
	return MP3_info;
}

/// Get only tag info from a file:
struct fileInfo MP3_GetTagInfoOnly(char *filename)
{
	struct fileInfo tempInfo;

	initFileInfo(&tempInfo);
	getMP3TagInfo(filename, &tempInfo);
	return tempInfo;
}

/// Set volume boost type:
/// NOTE: to be launched only once BEFORE setting boost volume or filter
void MP3_setVolumeBoostType(char *boostType)
{
	if (strcmp(boostType, "OLD") == 0) {
		MP3_volume_boost_type = BOOST_OLD;
		MAX_VOLUME_BOOST = 4;
		MIN_VOLUME_BOOST = 0;
	} else {
		MAX_VOLUME_BOOST = 15;
		MIN_VOLUME_BOOST = -MAX_VOLUME_BOOST;
		MP3_volume_boost_type = BOOST_NEW;
	}
	MP3_volume_boost_old = 0;
	MP3_volume_boost = 0;
}

/// Set volume boost:
void MP3_setVolumeBoost(int boost)
{
	if (MP3_volume_boost_type == BOOST_NEW) {
		MP3_volume_boost_old = 0;
		MP3_volume_boost = boost;
	} else {
		MP3_volume_boost_old = boost;
		MP3_volume_boost = 0;
	}
	//Reapply the filter:
	MP3_setFilter(filterDouble, 0);
}

/// Get actual volume boost:
int MP3_getVolumeBoost()
{
	if (MP3_volume_boost_type == BOOST_NEW) {
		return (MP3_volume_boost);
	} else {
		return (MP3_volume_boost_old);
	}
}

/// Set Filter:
int MP3_setFilter(double tFilter[32], int copyFilter)
{
	//Converto i db:
	double AmpFactor;
	int i;

	for (i = 0; i < 32; i++) {
		//Check for volume boost:
		if (MP3_volume_boost) {
			AmpFactor =
				pow(10., (tFilter[i] + MP3_volume_boost * DB_forBoost) / 20);
		} else {
			AmpFactor = pow(10., tFilter[i] / 20);
		}
		if (AmpFactor > mad_f_todouble(MAD_F_MAX)) {
			DoFilter = 0;
			return (0);
		} else {
			Filter[i] = mad_f_tofixed(AmpFactor);
		}
		if (copyFilter) {
			filterDouble[i] = tFilter[i];
		}
	}
	return (1);
}

int MP3_isFilterSupported()
{
	return 1;
}

/// Check if filter is enabled:
int MP3_isFilterEnabled()
{
	return DoFilter;
}

/// Enable filter:
void MP3_enableFilter()
{
	DoFilter = 1;
}

/// Disable filter:
void MP3_disableFilter()
{
	DoFilter = 0;
}

/// Get playing speed:
int MP3_getPlayingSpeed()
{
	return MP3_playingSpeed;
}

/// Set playing speed:
int MP3_setPlayingSpeed(int playingSpeed)
{
	if (playingSpeed >= MIN_PLAYING_SPEED && playingSpeed <= MAX_PLAYING_SPEED) {
		MP3_playingSpeed = playingSpeed;
		if (playingSpeed == 0)
			setVolume(myChannel, 0x8000);
		else
			setVolume(myChannel, FASTFORWARD_VOLUME);
		return 0;
	} else {
		return -1;
	}
}

/// Set mute:
int MP3_setMute(int onOff)
{
	return setMute(myChannel, onOff);
}

/// Fade out:
void MP3_fadeOut(float seconds)
{
	fadeOut(myChannel, seconds);
}

/// Manage suspend:
int MP3_suspend()
{
	return 0;
}

int MP3_resume()
{
	return 0;
}

mad_timer_t MP3_getTimer()
{
	return MP3_info.timer;
}
