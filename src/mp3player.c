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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <mad.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspaudio.h>
#include <psprtc.h>
#include <pspaudiocodec.h>
#include <limits.h>
#include "config.h"
#include "ssv.h"
#include "strsafe.h"
#include "musicdrv.h"
#include "xmp3audiolib.h"
#include "dbg.h"
#include "scene.h"
#include "apetaglib/APETag.h"
#include "genericplayer.h"
#include "mp3info.h"
#include "musicinfo.h"
#include "common/utils.h"
#include "xrhal.h"
#include "mediaengine.h"
#ifdef _DEBUG
#define DMALLOC 1
#include "dmalloc.h"
#endif

#ifdef ENABLE_MP3

#define MP3_FRAME_SIZE 2889

#define LB_CONV(x)	\
    (((x) & 0xff)<<24) |  \
    (((x>>8) & 0xff)<<16) |  \
    (((x>>16) & 0xff)<< 8) |  \
    (((x>>24) & 0xff)    )

#define UNUSED(x) ((void)(x))
#define BUFF_SIZE	8*1152

static mp3_reader_data mp3_data;

static int __end(void);

static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;

/**
 * MP3音乐播放缓冲
 */
static uint16_t g_buff[BUFF_SIZE / 2];

/**
 * MP3音乐解码缓冲
 */
static uint8_t g_input_buff[BUFF_SIZE + MAD_BUFFER_GUARD];

/**
 * MP3音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * MP3音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * MP3文件信息
 */
static struct MP3Info mp3info;

/*
 * 使用暴力
 */
static bool use_brute_method = false;

/**
 * 检查CRC
 */
static bool check_crc = false;

/**
 * 使用ME
 */
static bool use_me = true;

/**
 * Media Engine buffer缓存
 */
static unsigned long mp3_codec_buffer[65] __attribute__ ((aligned(64)));

static bool mp3_getEDRAM = false;

static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);
	return ((signed short) (Fixed >> (MAD_F_FRACBITS - 15)));
}

/**
 * 复制数据到声音缓冲区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param srcbuf 解码数据缓冲区指针
 * @param frames 复制帧数
 * @param channels 声道数
 */
static void send_to_sndbuf(void *buf, uint16_t * srcbuf, int frames,
						   int channels)
{
	int n;
	signed short *p = (signed short *) buf;

	if (frames <= 0)
		return;

	if (channels == 2) {
		memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
	} else {
		for (n = 0; n < frames * channels; n++) {
			*p++ = srcbuf[n];
			*p++ = srcbuf[n];
		}
	}

}

static int mp3_seek_seconds_offset_brute(double npt)
{
	int pos;

	pos = (int) ((double) g_info.samples * (npt) / g_info.duration);

	if (pos < 0) {
		pos = 0;
	}

	dbg_printf(d, "%s: jumping to %d frame, offset %08x", __func__, pos,
			   (int) mp3info.frameoff[pos]);
	dbg_printf(d, "%s: frame range (0~%u)", __func__,
			   (unsigned) g_info.samples);

	if (pos >= g_info.samples) {
		__end();
		return -1;
	}

	if (mp3_data.use_buffer)
		buffered_reader_seek(mp3_data.r, mp3info.frameoff[pos]);
	else
		xrIoLseek(mp3_data.fd, mp3info.frameoff[pos], PSP_SEEK_SET);

	mad_stream_finish(&stream);
	mad_stream_init(&stream);

	if (pos <= 0)
		g_play_time = 0.0;
	else
		g_play_time = npt;

	return 0;
}

/* Steal from libid3tag */

enum tagtype
{
	TAGTYPE_NONE = 0,
	TAGTYPE_ID3V1,
	TAGTYPE_ID3V2,
	TAGTYPE_ID3V2_FOOTER
};

enum
{
	ID3_TAG_FLAG_UNSYNCHRONISATION = 0x80,
	ID3_TAG_FLAG_EXTENDEDHEADER = 0x40,
	ID3_TAG_FLAG_EXPERIMENTALINDICATOR = 0x20,
	ID3_TAG_FLAG_FOOTERPRESENT = 0x10,
	ID3_TAG_FLAG_KNOWNFLAGS = 0xf0
};

typedef uint8_t id3_byte_t;
typedef int id3_length_t;

static enum tagtype tagtype(id3_byte_t const *data, id3_length_t length)
{
	if (length >= 3 && data[0] == 'T' && data[1] == 'A' && data[2] == 'G')
		return TAGTYPE_ID3V1;

	if (length >= 10 &&
		((data[0] == 'I' && data[1] == 'D' && data[2] == '3') ||
		 (data[0] == '3' && data[1] == 'D' && data[2] == 'I')) &&
		data[3] < 0xff && data[4] < 0xff &&
		data[6] < 0x80 && data[7] < 0x80 && data[8] < 0x80 && data[9] < 0x80)
		return data[0] == 'I' ? TAGTYPE_ID3V2 : TAGTYPE_ID3V2_FOOTER;

	return TAGTYPE_NONE;
}

static unsigned long id3_parse_uint(id3_byte_t const **ptr, unsigned int bytes)
{
	unsigned long value = 0;

	assert(bytes >= 1 && bytes <= 4);

	switch (bytes) {
		case 4:
			value = (value << 8) | *(*ptr)++;
		case 3:
			value = (value << 8) | *(*ptr)++;
		case 2:
			value = (value << 8) | *(*ptr)++;
		case 1:
			value = (value << 8) | *(*ptr)++;
	}

	return value;
}

static unsigned long id3_parse_syncsafe(id3_byte_t const **ptr,
										unsigned int bytes)
{
	unsigned long value = 0;

	assert(bytes == 4 || bytes == 5);

	switch (bytes) {
		case 5:
			value = (value << 4) | (*(*ptr)++ & 0x0f);
		case 4:
			value = (value << 7) | (*(*ptr)++ & 0x7f);
			value = (value << 7) | (*(*ptr)++ & 0x7f);
			value = (value << 7) | (*(*ptr)++ & 0x7f);
			value = (value << 7) | (*(*ptr)++ & 0x7f);
	}

	return value;
}

static void parse_header(id3_byte_t const **ptr, unsigned int *version,
						 int *flags, id3_length_t * size)
{
	*ptr += 3;

	*version = id3_parse_uint(ptr, 2);
	*flags = id3_parse_uint(ptr, 1);
	*size = id3_parse_syncsafe(ptr, 4);
}

static signed long id3_tag_query(id3_byte_t const *data, id3_length_t length)
{
	unsigned int version;
	int flags;
	id3_length_t size;

	assert(data);

	switch (tagtype(data, length)) {
		case TAGTYPE_ID3V1:
			return 128;

		case TAGTYPE_ID3V2:
			parse_header(&data, &version, &flags, &size);

			if (flags & ID3_TAG_FLAG_FOOTERPRESENT)
				size += 10;

			return 10 + size;

		case TAGTYPE_ID3V2_FOOTER:
			parse_header(&data, &version, &flags, &size);
			return -size - 10;

		case TAGTYPE_NONE:
			break;
	}

	return 0;
}

/**
 * 搜索下一个有效MP3 frame
 *
 * @return
 * - <0 失败
 * - 0 成功
 */
static int seek_valid_frame(void)
{
	int cnt = 0;
	int ret;

	mad_stream_finish(&stream);
	mad_stream_init(&stream);

	do {
		cnt++;
		if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN) {
			size_t read_size, remaining = 0;
			uint8_t *read_start;
			int bufsize;

			if (stream.next_frame != NULL) {
				remaining = stream.bufend - stream.next_frame;
				memmove(g_input_buff, stream.next_frame, remaining);
				read_start = g_input_buff + remaining;
				read_size = BUFF_SIZE - remaining;
			} else {
				read_size = BUFF_SIZE;
				read_start = g_input_buff;
				remaining = 0;
			}

			if (mp3_data.use_buffer)
				bufsize =
					buffered_reader_read(mp3_data.r, read_start, read_size);
			else
				bufsize = xrIoRead(mp3_data.fd, read_start, read_size);

			if (bufsize <= 0) {
				return -1;
			}

			if (bufsize < read_size) {
				uint8_t *guard = read_start + read_size;

				memset(guard, 0, MAD_BUFFER_GUARD);
				read_size += MAD_BUFFER_GUARD;
			}

			mad_stream_buffer(&stream, g_input_buff, read_size + remaining);
			stream.error = 0;
		}

		if ((ret = mad_header_decode(&frame.header, &stream)) == -1) {
			if (!MAD_RECOVERABLE(stream.error)) {
				return -1;
			}
		} else {
			ret = 0;
			stream.error = 0;
		}
	} while (!(ret == 0 && stream.sync == 1));
	dbg_printf(d, "%s: tried %d times", __func__, cnt);

	return 0;
}

static int mp3_seek_seconds_offset(double npt)
{
	int new_pos;

	if (npt < 0) {
		npt = 0;
	} else if (npt > g_info.duration) {
		__end();
		return -1;
	}

	new_pos = npt * mp3info.average_bitrate / 8;

	if (new_pos < 0) {
		new_pos = 0;
	}

	if (mp3_data.use_buffer)
		buffered_reader_seek(mp3_data.r, new_pos);
	else
		xrIoLseek(mp3_data.fd, new_pos, PSP_SEEK_SET);

	mad_stream_finish(&stream);
	mad_stream_init(&stream);

	if (seek_valid_frame() == -1) {
		__end();
		return -1;
	}

	if (g_info.filesize > 0) {
		long cur_pos;

		if (mp3_data.use_buffer)
			cur_pos = buffered_reader_position(mp3_data.r);
		else
			cur_pos = xrIoLseek(mp3_data.fd, 0, PSP_SEEK_CUR);
		g_play_time = g_info.duration * cur_pos / g_info.filesize;
	} else {
		g_play_time = npt;
	}

	if (g_play_time < 0)
		g_play_time = 0;

	return 0;
}

static int mp3_seek_seconds(double npt)
{
	int ret;

	free_bitrate(&g_inst_br);

	if (mp3info.frameoff && g_info.samples > 0) {
		ret = mp3_seek_seconds_offset_brute(npt);
	} else {
		ret = mp3_seek_seconds_offset(npt);
	}

	return ret;
}

/**
 * 处理快进快退
 *
 * @return
 * - -1 should exit
 * - 0 OK
 */
static int handle_seek(void)
{
	if (g_status == ST_FFORWARD) {
		generic_lock();
		g_status = ST_PLAYING;
		generic_unlock();
		generic_set_playback(true);
		mp3_seek_seconds(g_play_time + g_seek_seconds);
	} else if (g_status == ST_FBACKWARD) {
		generic_lock();
		g_status = ST_PLAYING;
		generic_unlock();
		generic_set_playback(true);
		mp3_seek_seconds(g_play_time - g_seek_seconds);
	}

	return 0;
}

/**
 * MP3音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int mp3_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() == -1) {
			__end();
			return -1;
		}

		xMP3ClearSndBuf(buf, snd_buf_frame_size);
		xrKernelDelayThread(100000);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2],
						   snd_buf_frame_size, 2);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN) {
				size_t read_size, remaining = 0;
				uint8_t *read_start;
				int bufsize;

				if (stream.next_frame != NULL) {
					remaining = stream.bufend - stream.next_frame;
					memmove(g_input_buff, stream.next_frame, remaining);
					read_start = g_input_buff + remaining;
					read_size = BUFF_SIZE - remaining;
				} else {
					read_size = BUFF_SIZE;
					read_start = g_input_buff;
					remaining = 0;
				}

				if (mp3_data.use_buffer)
					bufsize =
						buffered_reader_read(mp3_data.r, read_start, read_size);
				else
					bufsize = xrIoRead(mp3_data.fd, read_start, read_size);

				if (bufsize <= 0) {
					__end();
					return -1;
				}
				if (bufsize < read_size) {
					uint8_t *guard = read_start + read_size;

					memset(guard, 0, MAD_BUFFER_GUARD);
					read_size += MAD_BUFFER_GUARD;
				}
				mad_stream_buffer(&stream, g_input_buff, read_size + remaining);
				stream.error = 0;
			}

			ret = mad_frame_decode(&frame, &stream);

			if (ret == -1) {
				if (MAD_RECOVERABLE(stream.error)
					|| stream.error == MAD_ERROR_BUFLEN) {
					if (stream.error == MAD_ERROR_LOSTSYNC) {
						long tagsize = id3_tag_query(stream.this_frame,
													 stream.bufend -
													 stream.this_frame);

						if (tagsize > 0) {
							mad_stream_skip(&stream, tagsize);
						}

						if (mad_header_decode(&frame.header, &stream) == -1) {
							if (stream.error != MAD_ERROR_BUFLEN) {
								if (!MAD_RECOVERABLE(stream.error)) {
									__end();
									return -1;
								}
							}
						} else {
							stream.error = MAD_ERROR_NONE;
						}
					}

					g_buff_frame_size = 0;
					g_buff_frame_start = 0;
					continue;
				} else {
					__end();
					return -1;
				}
			}

			unsigned i;
			uint16_t *output = &g_buff[0];

			if (stream.error != MAD_ERROR_NONE) {
				continue;
			}

			mad_synth_frame(&synth, &frame);
			for (i = 0; i < synth.pcm.length; i++) {
				signed short sample;

				if (MAD_NCHANNELS(&frame.header) == 2) {
					/* Left channel */
					sample = MadFixedToSshort(synth.pcm.samples[0][i]);
					*(output++) = sample;
					sample = MadFixedToSshort(synth.pcm.samples[1][i]);
					*(output++) = sample;
				} else {
					sample = MadFixedToSshort(synth.pcm.samples[0][i]);
					*(output++) = sample;
					*(output++) = sample;
				}
			}

			g_buff_frame_size = synth.pcm.length;
			g_buff_frame_start = 0;
			incr = frame.header.duration.seconds;
			incr +=
				mad_timer_fraction(frame.header.duration,
								   MAD_UNITS_MILLISECONDS) / 1000.0;
			g_play_time += incr;
			add_bitrate(&g_inst_br, frame.header.bitrate, incr);
		}
	}

	return 0;
}

int memp3_decode(void *data, dword data_len, void *pcm_data)
{
	mp3_codec_buffer[6] = (uint32_t) data;
	mp3_codec_buffer[8] = (uint32_t) pcm_data;
	mp3_codec_buffer[7] = data_len;
	mp3_codec_buffer[10] = data_len;
	mp3_codec_buffer[9] = mp3info.spf << 2;

	return xrAudiocodecDecode(mp3_codec_buffer, 0x1002);
}

static uint8_t memp3_input_buf[2889] __attribute__ ((aligned(64)));
static uint8_t memp3_decoded_buf[2048 * 4] __attribute__ ((aligned(64)));
static dword this_frame, buf_end;
static bool need_data;

/**
 * MP3音乐播放回调函数，ME版本
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int memp3_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	signed short *audio_buf = buf;
	double incr;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() == -1) {
			__end();
			return -1;
		}

		xMP3ClearSndBuf(buf, snd_buf_frame_size);
		xrKernelDelayThread(100000);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2],
						   snd_buf_frame_size, 2);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			int frame_size, ret, brate = 0;

			do {
				if (mp3_data.use_buffer) {
					if ((frame_size =
						 search_valid_frame_me_buffered(&mp3_data,
														&brate)) < 0) {
						__end();
						return -1;
					}
				} else {
					if ((frame_size =
						 search_valid_frame_me(&mp3_data, &brate)) < 0) {
						__end();
						return -1;
					}
				}

				if (mp3_data.use_buffer)
					ret =
						buffered_reader_read(mp3_data.r, memp3_input_buf,
											 frame_size);
				else
					ret = xrIoRead(mp3_data.fd, memp3_input_buf, frame_size);

				if (ret < 0) {
					__end();
					return -1;
				}

				ret = memp3_decode(memp3_input_buf, ret, memp3_decoded_buf);
			} while (ret < 0);

			uint16_t *output = &g_buff[0];

			memcpy(output, memp3_decoded_buf, mp3info.spf * 4);
			g_buff_frame_size = mp3info.spf;
			g_buff_frame_start = 0;
			incr = (double) mp3info.spf / mp3info.sample_freq;
			g_play_time += incr;

			add_bitrate(&g_inst_br, brate * 1000, incr);
		}
	}

	return 0;
}

/**
 * 初始化驱动变量资源等
 *
 * @return 成功时返回0
 */
static int __init(void)
{
	generic_init();

	generic_lock();
	g_status = ST_UNKNOWN;
	generic_unlock();

	memset(&g_inst_br, 0, sizeof(g_inst_br));
	memset(g_buff, 0, sizeof(g_buff));
	memset(g_input_buff, 0, sizeof(g_input_buff));
	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_play_time = 0.;
	memset(&mp3info, 0, sizeof(mp3info));
	memset(&g_info, 0, sizeof(g_info));

	return 0;
}

static int me_init()
{
	int ret;

	ret = load_me_prx();

	if (ret < 0)
		return ret;

	memset(mp3_codec_buffer, 0, sizeof(mp3_codec_buffer));
	ret = xrAudiocodecCheckNeedMem(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = false;
	ret = xrAudiocodecGetEDRAM(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = true;

	ret = xrAudiocodecInit(mp3_codec_buffer, 0x1002);

	if (ret < 0) {
		xrAudiocodecReleaseEDRAM(mp3_codec_buffer);
		return ret;
	}

	memset(memp3_input_buf, 0, sizeof(memp3_input_buf));
	memset(memp3_decoded_buf, 0, sizeof(memp3_decoded_buf));
	this_frame = buf_end = 0;
	need_data = true;

	return 0;
}

void readMP3ApeTag(const char *spath)
{
	APETag *tag = apetag_load(spath);

	if (tag != NULL) {
		char *title = apetag_get(tag, "Title");
		char *artist = apetag_get(tag, "Artist");
		char *album = apetag_get(tag, "Album");

		if (title) {
			STRCPY_S(g_info.tag.title, title);
			free(title);
			title = NULL;
		}
		if (artist) {
			STRCPY_S(g_info.tag.artist, artist);
			free(artist);
			artist = NULL;
		} else {
			artist = apetag_get(tag, "Album artist");
			if (artist) {
				STRCPY_S(g_info.tag.artist, artist);
				free(artist);
				artist = NULL;
			}
		}
		if (album) {
			STRCPY_S(g_info.tag.album, album);
			free(album);
			album = NULL;
		}

		apetag_free(tag);
		g_info.tag.encode = conf_encode_utf8;
	}
}

static int mp3_load(const char *spath, const char *lpath)
{
	int ret;

	__init();

	dbg_printf(d, "%s: loading %s", __func__, spath);
	g_status = ST_UNKNOWN;

	mp3_data.use_buffer = true;

	mp3_data.fd = xrIoOpen(spath, PSP_O_RDONLY, 0777);

	if (mp3_data.fd < 0)
		return -1;

	g_info.filesize = xrIoLseek(mp3_data.fd, 0, PSP_SEEK_END);
	xrIoLseek(mp3_data.fd, 0, PSP_SEEK_SET);
	mp3_data.size = g_info.filesize;

	if (g_info.filesize < 0)
		return g_info.filesize;

	xrIoLseek(mp3_data.fd, 0, PSP_SEEK_SET);

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);

	if (use_me) {
		if ((ret = me_init()) < 0) {
			dbg_printf(d, "me_init failed: %d", ret);
			use_me = false;
		}
	}

	mp3info.check_crc = check_crc;
	mp3info.have_crc = false;

	if (use_brute_method) {
		if (read_mp3_info_brute(&mp3info, &mp3_data) < 0) {
			__end();
			return -1;
		}
	} else {
		if (read_mp3_info(&mp3info, &mp3_data) < 0) {
			__end();
			return -1;
		}
	}

	g_info.channels = mp3info.channels;
	g_info.sample_freq = mp3info.sample_freq;
	g_info.avg_bps = mp3info.average_bitrate;
	g_info.samples = mp3info.frames;
	g_info.duration = mp3info.duration;

	generic_readtag(&g_info, spath);

	if (mp3_data.use_buffer) {
		SceOff cur = xrIoLseek(mp3_data.fd, 0, PSP_SEEK_CUR);

		xrIoClose(mp3_data.fd);
		mp3_data.fd = -1;
		mp3_data.r = buffered_reader_open(spath, g_io_buffer_size, 1);
		buffered_reader_seek(mp3_data.r, cur);
	}

	dbg_printf(d, "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d%sframes %d%s]",
			   g_info.channels, g_info.sample_freq,
			   g_info.avg_bps / 1000,
			   (int) (g_info.duration / 60), (int) g_info.duration % 60,
			   mp3info.frameoff != NULL ? ", frame table, " : ", ",
			   g_info.samples, mp3info.have_crc ? ", crc passed" : "");

#ifdef _DEBUG
	if (mp3info.lame_encoded) {
		char lame_method[80];
		char encode_msg[80];

		switch (mp3info.lame_mode) {
			case ABR:
				STRCPY_S(lame_method, "ABR");
				break;
			case CBR:
				STRCPY_S(lame_method, "CBR");
				break;
			case VBR:
				SPRINTF_S(lame_method, "VBR V%1d", mp3info.lame_vbr_quality);
				break;
			default:
				break;
		}

		if (mp3info.lame_str[strlen(mp3info.lame_str) - 1] == ' ')
			SPRINTF_S(encode_msg, "%s%s", mp3info.lame_str, lame_method);
		else
			SPRINTF_S(encode_msg, "%s %s", mp3info.lame_str, lame_method);
		dbg_printf(d, "[ %s ]", encode_msg);
	}
#endif

	ret = xMP3AudioInit();

	if (ret < 0) {
		__end();
		return -1;
	}

	ret = xMP3AudioSetFrequency(g_info.sample_freq);
	if (ret < 0) {
		__end();
		return -1;
	}
	// ME can't handle mono channel now
	if (use_me)
		xMP3AudioSetChannelCallback(0, memp3_audiocallback, NULL);
	else
		xMP3AudioSetChannelCallback(0, mp3_audiocallback, NULL);

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	return 0;
}

static void init_default_option(void)
{
	use_me = false;
	check_crc = false;
	g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;
}

static int mp3_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	init_default_option();

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp
			(argv[i], "mp3_brute_mode", sizeof("mp3_brute_mode") - 1)) {
			if (opt_is_on(argv[i])) {
				use_brute_method = true;
			} else {
				use_brute_method = false;
			}
		} else
			if (!strncasecmp(argv[i], "mp3_use_me", sizeof("mp3_use_me") - 1)) {
			if (opt_is_on(argv[i])) {
				use_me = true;
			} else {
				use_me = false;
			}
		} else if (!strncasecmp
				   (argv[i], "mp3_check_crc", sizeof("mp3_check_crc") - 1)) {
			if (opt_is_on(argv[i])) {
				check_crc = true;
			} else {
				check_crc = false;
			}
		} else if (!strncasecmp
				   (argv[i], "mp3_buffer_size",
					sizeof("mp3_buffer_size") - 1)) {
			const char *p = argv[i];

			if ((p = strrchr(p, '=')) != NULL) {
				p++;

				g_io_buffer_size = atoi(p);

				if (g_io_buffer_size < 8192) {
					g_io_buffer_size = 8192;
				}
			}
		} else if (!strncasecmp
				   (argv[i], "show_encoder_msg",
					sizeof("show_encoder_msg") - 1)) {
			if (opt_is_on(argv[i])) {
				show_encoder_msg = true;
			} else {
				show_encoder_msg = false;
			}
		}
	}

	clean_args(argc, argv);

	return 0;
}

static int mp3_get_info(struct music_info *info)
{
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_CPUFREQ) {
		if (use_me) {
			if (mp3_data.use_buffer)
				info->psp_freq[0] = 49;
			else
				info->psp_freq[0] = 33;

			info->psp_freq[1] = 16;
		} else {
			info->psp_freq[0] = 66 + (133 - 66) * g_info.avg_bps / 1000 / 320;
			info->psp_freq[1] = 111;
		}
	}
	if (info->type & MD_GET_DECODERNAME) {
		if (use_me) {
			STRCPY_S(info->decoder_name, "mp3");
		} else {
			STRCPY_S(info->decoder_name, "madmp3");
		}
	}
	if (info->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg && mp3info.lame_encoded) {
			char lame_method[80];

			switch (mp3info.lame_mode) {
				case ABR:
					STRCPY_S(lame_method, "ABR");
					break;
				case CBR:
					STRCPY_S(lame_method, "CBR");
					break;
				case VBR:
					SPRINTF_S(lame_method, "VBR V%1d",
							  mp3info.lame_vbr_quality);
					break;
				default:
					break;
			}

			if (mp3info.lame_str[strlen(mp3info.lame_str) - 1] == ' ')
				SPRINTF_S(info->encode_msg,
						  "%s%s", mp3info.lame_str, lame_method);
			else
				SPRINTF_S(info->encode_msg,
						  "%s %s", mp3info.lame_str, lame_method);
		} else {
			info->encode_msg[0] = '\0';
		}
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}

	return generic_get_info(info);
}

/**
 * 停止MP3音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int mp3_end(void)
{
	dbg_printf(d, "%s", __func__);

	__end();

	xMP3AudioEnd();

	g_status = ST_STOPPED;

	mad_stream_finish(&stream);
	mad_synth_finish(&synth);
	mad_frame_finish(&frame);

	if (use_me) {
		if (mp3_getEDRAM)
			xrAudiocodecReleaseEDRAM(mp3_codec_buffer);
	}

	free_mp3_info(&mp3info);
	free_bitrate(&g_inst_br);

	if (mp3_data.use_buffer) {
		if (mp3_data.r != NULL) {
			buffered_reader_close(mp3_data.r);
			mp3_data.r = NULL;
		}
	} else {
		if (mp3_data.fd >= 0) {
			xrIoClose(mp3_data.fd);
			mp3_data.fd = -1;
		}
	}

	generic_end();

	return 0;
}

/**
 * PSP准备休眠时MP3的操作
 *
 * @return 成功时返回0
 */
static int mp3_suspend(void)
{
	dbg_printf(d, "%s", __func__);

	generic_suspend();
	mp3_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的MP3的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件名形式
 *
 * @return 成功时返回0
 */
static int mp3_resume(const char *spath, const char *lpath)
{
	int ret;

	dbg_printf(d, "%s", __func__);
	ret = mp3_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: mp3_load failed %d", __func__, ret);
		return -1;
	}

	mp3_seek_seconds(g_suspend_playing_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 检测是否为MP3文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是MP3文件返回1，否则返回0
 */
static int mp3_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "mp1") == 0) {
			return 1;
		}
		if (stricmp(p, "mp2") == 0) {
			return 1;
		}
		if (stricmp(p, "mp3") == 0) {
			return 1;
		}
		if (stricmp(p, "mpa") == 0) {
			return 1;
		}
		if (stricmp(p, "mpeg") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops mp3_ops = {
	.name = "mp3",
	.set_opt = mp3_set_opt,
	.load = mp3_load,
	.play = NULL,
	.pause = NULL,
	.end = mp3_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = mp3_suspend,
	.resume = mp3_resume,
	.get_info = mp3_get_info,
	.probe = mp3_probe,
	.next = NULL,
};

int mp3_init()
{
	return register_musicdrv(&mp3_ops);
}

/**
 * 停止MP3音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	g_play_time = 0.;
	generic_lock();
	g_status = ST_STOPPED;
	generic_unlock();

	return 0;
}

#endif
