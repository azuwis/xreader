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
#include <pspaudio.h>
#include <pspaudiocodec.h>
#include <pspmpeg.h>
#include <limits.h>
#include "config.h"
#include "ssv.h"
#include "strsafe.h"
#include "musicdrv.h"
#include "xmp3audiolib.h"
#include "dbg.h"
#include "scene.h"
#include "mp3info.h"
#include "apetaglib/APETag.h"
#include "genericplayer.h"

#define MP3_FRAME_SIZE 2889

#define LB_CONV(x)	\
    (((x) & 0xff)<<24) |  \
    (((x>>8) & 0xff)<<16) |  \
    (((x>>16) & 0xff)<< 8) |  \
    (((x>>24) & 0xff)    )

#define UNUSED(x) ((void)(x))
#define BUFF_SIZE	8*1152

static mp3_reader_data data;

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
static bool use_me = false;

/**
 * Media Engine buffer缓存
 */
unsigned long mp3_codec_buffer[65] __attribute__ ((aligned(64)));

bool mp3_getEDRAM = false;

static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);
	return ((signed short) (Fixed >> (MAD_F_FRACBITS - 15)));
}

/**
 * 清空声音缓冲区
 *
 * @param buf 声音缓冲区指针
 * @param frames 帧数大小
 */
static void clear_snd_buf(void *buf, int frames)
{
	memset(buf, 0, frames * 2 * 2);
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
	if (frames <= 0)
		return;

	memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
}

static int madmp3_seek_seconds_offset_brute(double offset)
{
	int pos, ret;

	pos =
		(int) ((double) mp3info.frames * (g_play_time + offset) /
			   mp3info.duration);

	if (pos < 0) {
		pos = 0;
	}

	dbg_printf(d, "%s: jumping to %d frame, offset %08x", __func__, pos,
			   (int) mp3info.frameoff[pos]);
	dbg_printf(d, "%s: frame range (0~%u)", __func__,
			   (unsigned) mp3info.frames);

	if (pos >= mp3info.frames) {
		__end();
		return -1;
	}

	ret = sceIoLseek(data.fd, mp3info.frameoff[pos], PSP_SEEK_SET);

	if (ret < 0)
		return -1;

	mad_stream_finish(&stream);
	mad_stream_init(&stream);

	if (pos <= 0)
		g_play_time = 0.0;
	else
		g_play_time += offset;

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

unsigned long id3_parse_uint(id3_byte_t const **ptr, unsigned int bytes)
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

unsigned long id3_parse_syncsafe(id3_byte_t const **ptr, unsigned int bytes)
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

signed long id3_tag_query(id3_byte_t const *data, id3_length_t length)
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

			bufsize = sceIoRead(data.fd, read_start, read_size);

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

static int madmp3_seek_seconds_offset(double offset)
{
	uint32_t target_frame = abs(offset) * mp3info.average_bitrate / 8;
	int is_forward = offset > 0 ? 1 : -1;
	int orig_pos = sceIoLseek(data.fd, 0, PSP_SEEK_CUR);
	int new_pos = orig_pos + is_forward * (int) target_frame;
	int ret;

	if (new_pos < 0) {
		new_pos = 0;
	}

	ret = sceIoLseek(data.fd, new_pos, PSP_SEEK_SET);

	mad_stream_finish(&stream);
	mad_stream_init(&stream);

	if (ret >= 0) {
		if (seek_valid_frame() == -1) {
			__end();
			return -1;
		}

		if (data.size > 0) {
			long cur_pos;

			cur_pos = sceIoLseek(data.fd, 0, PSP_SEEK_CUR);
			g_play_time = mp3info.duration * cur_pos / data.size;
		} else {
			g_play_time += offset;
		}

		if (g_play_time < 0)
			g_play_time = 0;

		return 0;
	}

	__end();
	return -1;
}

static int madmp3_seek_seconds(double npt)
{
	free_bitrate(&g_inst_br);

	if (mp3info.frameoff && mp3info.frames > 0) {
		return madmp3_seek_seconds_offset_brute(npt - g_play_time);
	} else {
		return madmp3_seek_seconds_offset(npt - g_play_time);
	}
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
static int madmp3_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFOWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time - g_seek_seconds);
		}
		clear_snd_buf(buf, snd_buf_frame_size);
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
				bufsize = sceIoRead(data.fd, read_start, read_size);
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

	return sceAudiocodecDecode(mp3_codec_buffer, 0x1002);
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
		if (g_status == ST_FFOWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time - g_seek_seconds);
		}
		clear_snd_buf(buf, snd_buf_frame_size);
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
				if ((frame_size = search_valid_frame_me(&data, &brate)) < 0) {
					__end();
					return -1;
				}

				ret = sceIoRead(data.fd, memp3_input_buf, frame_size);

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
	g_status_sema = sceKernelCreateSema("wave Sema", 0, 1, 1, NULL);

	generic_lock();
	g_status = ST_UNKNOWN;
	generic_unlock();

	memset(&g_inst_br, 0, sizeof(g_inst_br));
	memset(g_buff, 0, sizeof(g_buff));
	memset(g_input_buff, 0, sizeof(g_input_buff));
	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	mp3info.duration = g_play_time = 0.;
	memset(&mp3info, 0, sizeof(mp3info));

	return 0;
}

static bool me_prx_loaded = false;

static int load_me_prx(void)
{
	int result;

	if (me_prx_loaded)
		return 0;

#if (PSP_FW_VERSION >= 300)
	result = sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
#else
	result =
		pspSdkLoadStartModule("flash0:/kd/avcodec.prx",
							  PSP_MEMORY_PARTITION_KERNEL);
#endif

	if (result < 0)
		return -1;

#if (PSP_FW_VERSION >= 300)
	result = sceUtilityLoadAvModule(PSP_AV_MODULE_ATRAC3PLUS);
	if (result < 0)
		return -2;
#endif

	me_prx_loaded = true;

	return 0;
}

static int me_init()
{
	int ret;

	ret = load_me_prx();

	if (ret < 0)
		return ret;

	memset(mp3_codec_buffer, 0, sizeof(mp3_codec_buffer));
	ret = sceAudiocodecCheckNeedMem(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = false;
	ret = sceAudiocodecGetEDRAM(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = true;

	ret = sceAudiocodecInit(mp3_codec_buffer, 0x1002);

	if (ret < 0) {
		sceAudiocodecReleaseEDRAM(mp3_codec_buffer);
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
	APETag *tag = loadAPETag(spath);

	if (tag != NULL) {
		char *title = APETag_SimpleGet(tag, "Title");
		char *artist = APETag_SimpleGet(tag, "Artist");
		char *album = APETag_SimpleGet(tag, "Album");

		if (title) {
			STRCPY_S(mp3info.tag.title, title);
			free(title);
			title = NULL;
		}
		if (artist) {
			STRCPY_S(mp3info.tag.author, artist);
			free(artist);
			artist = NULL;
		} else {
			artist = APETag_SimpleGet(tag, "Album artist");
			if (artist) {
				STRCPY_S(mp3info.tag.author, artist);
				free(artist);
				artist = NULL;
			}
		}
		if (album) {
			STRCPY_S(mp3info.tag.album, album);
			free(album);
			album = NULL;
		}

		freeAPETag(tag);
		mp3info.tag.encode = conf_encode_utf8;
	}
}

static int madmp3_load(const char *spath, const char *lpath)
{
	int ret;

	__init();

	dbg_printf(d, "%s: loading %s", __func__, spath);
	g_status = ST_UNKNOWN;
	data.fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (data.fd < 0) {
		return data.fd;
	}

	data.size = sceIoLseek(data.fd, 0, PSP_SEEK_END);
	if (data.size < 0)
		return data.size;
	sceIoLseek(data.fd, 0, PSP_SEEK_SET);

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
		// read ID3 tag first
		read_mp3_info(&mp3info, &data);

		if (read_mp3_info_brute(&mp3info, &data) < 0) {
			__end();
			return -1;
		}
	} else {
		if (read_mp3_info(&mp3info, &data) < 0) {
			__end();
			return -1;
		}
	}

	if (config.apetagorder || mp3info.tag.title[0] == '\0')
		readMP3ApeTag(spath);

	dbg_printf(d, "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d%sframes %d%s]",
			   mp3info.channels, mp3info.sample_freq,
			   mp3info.average_bitrate / 1000,
			   (int) (mp3info.duration / 60), (int) mp3info.duration % 60,
			   mp3info.frameoff != NULL ? ", frame table, " : ", ",
			   mp3info.frames, mp3info.have_crc ? ", crc passed" : "");
	ret = xMP3AudioInit();

	if (ret < 0) {
		__end();
		return -1;
	}

	ret = xMP3AudioSetFrequency(mp3info.sample_freq);
	if (ret < 0) {
		__end();
		return -1;
	}
	// ME can't handle mono channel now
	if (use_me)
		xMP3AudioSetChannelCallback(0, memp3_audiocallback, NULL);
	else
		xMP3AudioSetChannelCallback(0, madmp3_audiocallback, NULL);

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	return 0;
}

static void init_default_option(void)
{
	use_me = false;
	check_crc = false;
}

static int madmp3_set_opt(const char *unused, const char *values)
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

static int madmp3_get_info(struct music_info *info)
{
	if (g_status == ST_UNKNOWN) {
		return -1;
	}

	if (info->type & MD_GET_TITLE) {
		info->encode = mp3info.tag.encode;
		STRCPY_S(info->title, mp3info.tag.title);
	}
	if (info->type & MD_GET_ALBUM) {
		info->encode = mp3info.tag.encode;
		STRCPY_S(info->album, mp3info.tag.album);
	}
	if (info->type & MD_GET_ARTIST) {
		info->encode = mp3info.tag.encode;
		STRCPY_S(info->artist, mp3info.tag.author);
	}
	if (info->type & MD_GET_COMMENT) {
		info->encode = mp3info.tag.encode;
		STRCPY_S(info->comment, mp3info.tag.comment);
	}
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = mp3info.duration;
	}
	if (info->type & MD_GET_CPUFREQ) {
		if (use_me) {
			info->psp_freq[0] = 33;
			info->psp_freq[1] = 16;
		} else {
			info->psp_freq[0] =
				66 + (133 - 66) * mp3info.average_bitrate / 1000 / 320;
			info->psp_freq[1] = 111;
		}
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = mp3info.sample_freq;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = mp3info.channels;
	}
	if (info->type & MD_GET_DECODERNAME) {
		if (use_me) {
			STRCPY_S(info->decoder_name, "mp3");
		} else {
			STRCPY_S(info->decoder_name, "madmp3");
		}
	}
	if (info->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			STRCPY_S(info->encode_msg, mp3info.tag.encoder);
		} else {
			info->encode_msg[0] = '\0';
		}
	}
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = mp3info.average_bitrate / 1000;
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}

	return 0;
}

/**
 * 停止MP3音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int madmp3_end(void)
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
			sceAudiocodecReleaseEDRAM(mp3_codec_buffer);
	}

	free_mp3_info(&mp3info);
	free_bitrate(&g_inst_br);

	if (data.fd >= 0) {
		sceIoClose(data.fd);
		data.fd = -1;
	}

	return 0;
}

/**
 * PSP准备休眠时MP3的操作
 *
 * @return 成功时返回0
 */
static int madmp3_suspend(void)
{
	dbg_printf(d, "%s", __func__);

	generic_suspend();
	madmp3_end();

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
static int madmp3_resume(const char *spath, const char *lpath)
{
	int ret;

	dbg_printf(d, "%s", __func__);
	ret = madmp3_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: madmp3_load failed %d", __func__, ret);
		return -1;
	}

	madmp3_seek_seconds(g_suspend_playing_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

static struct music_ops madmp3_ops = {
	.name = "madmp3",
	.set_opt = madmp3_set_opt,
	.load = madmp3_load,
	.play = NULL,
	.pause = NULL,
	.end = madmp3_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = madmp3_suspend,
	.resume = madmp3_resume,
	.get_info = madmp3_get_info,
	.next = NULL,
};

int madmp3_init()
{
	return register_musicdrv(&madmp3_ops);
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
