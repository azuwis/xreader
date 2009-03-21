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

#include "common/datatype.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdlib.h>
#include "strsafe.h"
#include "musicdrv.h"
#include "mp3info.h"
#include "common/utils.h"
#include "crc16.h"
#include "charsets.h"
#include "dbg.h"

#define ID3v2_HEADER_SIZE 10

typedef int64_t offset_t;

typedef struct MPADecodeContext
{
	//    DECLARE_ALIGNED_8(uint8_t, last_buf[2*BACKSTEP_SIZE + EXTRABYTES]);
	int last_buf_size;
	int frame_size;
	/* next header (used in free format parsing) */
	uint32_t free_format_next_header;
	int error_protection;
	int layer;
	int sample_rate;
	int sample_rate_index;		/* between 0 and 8 */
	int bit_rate;
	//    GetBitContext gb;
	//    GetBitContext in_gb;
	int nb_channels;
	int mode;
	int mode_ext;
	int lsf;
	//    DECLARE_ALIGNED_16(MPA_INT, synth_buf[MPA_MAX_CHANNELS][512 * 2]);
	//    int synth_buf_offset[MPA_MAX_CHANNELS];
	//    DECLARE_ALIGNED_16(int32_t, sb_samples[MPA_MAX_CHANNELS][36][SBLIMIT]);
	//    int32_t mdct_buf[MPA_MAX_CHANNELS][SBLIMIT * 18]; /* previous samples, for layer 3 MDCT */
#ifdef DEBUG
	int frame_count;
#endif
	//    void (*compute_antialias)(struct MPADecodeContext *s, struct GranuleDef *g);
	int adu_mode;
	int dither_state;
	int error_recognition;
	//    AVCodecContext* avctx;
} MPADecodeContext;

#define ID3v1_GENRE_MAX 125

static const char *const id3v1_genre_str[ID3v1_GENRE_MAX + 1] = {
	[0] = "Blues",
	[1] = "Classic Rock",
	[2] = "Country",
	[3] = "Dance",
	[4] = "Disco",
	[5] = "Funk",
	[6] = "Grunge",
	[7] = "Hip-Hop",
	[8] = "Jazz",
	[9] = "Metal",
	[10] = "New Age",
	[11] = "Oldies",
	[12] = "Other",
	[13] = "Pop",
	[14] = "R&B",
	[15] = "Rap",
	[16] = "Reggae",
	[17] = "Rock",
	[18] = "Techno",
	[19] = "Industrial",
	[20] = "Alternative",
	[21] = "Ska",
	[22] = "Death Metal",
	[23] = "Pranks",
	[24] = "Soundtrack",
	[25] = "Euro-Techno",
	[26] = "Ambient",
	[27] = "Trip-Hop",
	[28] = "Vocal",
	[29] = "Jazz+Funk",
	[30] = "Fusion",
	[31] = "Trance",
	[32] = "Classical",
	[33] = "Instrumental",
	[34] = "Acid",
	[35] = "House",
	[36] = "Game",
	[37] = "Sound Clip",
	[38] = "Gospel",
	[39] = "Noise",
	[40] = "AlternRock",
	[41] = "Bass",
	[42] = "Soul",
	[43] = "Punk",
	[44] = "Space",
	[45] = "Meditative",
	[46] = "Instrumental Pop",
	[47] = "Instrumental Rock",
	[48] = "Ethnic",
	[49] = "Gothic",
	[50] = "Darkwave",
	[51] = "Techno-Industrial",
	[52] = "Electronic",
	[53] = "Pop-Folk",
	[54] = "Eurodance",
	[55] = "Dream",
	[56] = "Southern Rock",
	[57] = "Comedy",
	[58] = "Cult",
	[59] = "Gangsta",
	[60] = "Top 40",
	[61] = "Christian Rap",
	[62] = "Pop/Funk",
	[63] = "Jungle",
	[64] = "Native American",
	[65] = "Cabaret",
	[66] = "New Wave",
	[67] = "Psychadelic",
	[68] = "Rave",
	[69] = "Showtunes",
	[70] = "Trailer",
	[71] = "Lo-Fi",
	[72] = "Tribal",
	[73] = "Acid Punk",
	[74] = "Acid Jazz",
	[75] = "Polka",
	[76] = "Retro",
	[77] = "Musical",
	[78] = "Rock & Roll",
	[79] = "Hard Rock",
	[80] = "Folk",
	[81] = "Folk-Rock",
	[82] = "National Folk",
	[83] = "Swing",
	[84] = "Fast Fusion",
	[85] = "Bebob",
	[86] = "Latin",
	[87] = "Revival",
	[88] = "Celtic",
	[89] = "Bluegrass",
	[90] = "Avantgarde",
	[91] = "Gothic Rock",
	[92] = "Progressive Rock",
	[93] = "Psychedelic Rock",
	[94] = "Symphonic Rock",
	[95] = "Slow Rock",
	[96] = "Big Band",
	[97] = "Chorus",
	[98] = "Easy Listening",
	[99] = "Acoustic",
	[100] = "Humour",
	[101] = "Speech",
	[102] = "Chanson",
	[103] = "Opera",
	[104] = "Chamber Music",
	[105] = "Sonata",
	[106] = "Symphony",
	[107] = "Booty Bass",
	[108] = "Primus",
	[109] = "Porn Groove",
	[110] = "Satire",
	[111] = "Slow Jam",
	[112] = "Club",
	[113] = "Tango",
	[114] = "Samba",
	[115] = "Folklore",
	[116] = "Ballad",
	[117] = "Power Ballad",
	[118] = "Rhythmic Soul",
	[119] = "Freestyle",
	[120] = "Duet",
	[121] = "Punk Rock",
	[122] = "Drum Solo",
	[123] = "A capella",
	[124] = "Euro-House",
	[125] = "Dance Hall",
};

static const uint16_t ff_mpa_freq_tab[3] = { 44100, 48000, 32000 };

static const uint16_t ff_mpa_bitrate_tab[2][3][15] = {
	{{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
	 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
	 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
	{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
	 }
};

static inline void av_log()
{
}

/* fast header check for resync */
static inline int ff_mpa_check_header(uint32_t header)
{
	/* header */
	if ((header & 0xffe00000) != 0xffe00000)
		return -1;
	/* layer check */
	if ((header & (3 << 17)) == 0)
		return -1;
	/* bit rate */
	if ((header & (0xf << 12)) == 0xf << 12)
		return -1;
	/* frequency */
	if ((header & (3 << 10)) == 3 << 10)
		return -1;
	return 0;
}

static int ff_mpegaudio_decode_header(MPADecodeContext * s, uint32_t header)
{
	int sample_rate, frame_size, mpeg25, padding;
	int sample_rate_index, bitrate_index;

	if (header & (1 << 20)) {
		s->lsf = (header & (1 << 19)) ? 0 : 1;
		mpeg25 = 0;
	} else {
		s->lsf = 1;
		mpeg25 = 1;
	}

	s->layer = 4 - ((header >> 17) & 3);
	/* extract frequency */
	sample_rate_index = (header >> 10) & 3;
	sample_rate = ff_mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
	sample_rate_index += 3 * (s->lsf + mpeg25);
	s->sample_rate_index = sample_rate_index;
	s->error_protection = ((header >> 16) & 1) ^ 1;
	s->sample_rate = sample_rate;

	bitrate_index = (header >> 12) & 0xf;
	padding = (header >> 9) & 1;
	//extension = (header >> 8) & 1;
	s->mode = (header >> 6) & 3;
	s->mode_ext = (header >> 4) & 3;
	//copyright = (header >> 3) & 1;
	//original = (header >> 2) & 1;
	//emphasis = header & 3;

	if (s->mode == MPA_MONO)
		s->nb_channels = 1;
	else
		s->nb_channels = 2;

	if (bitrate_index != 0) {
		frame_size = ff_mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
		s->bit_rate = frame_size * 1000;
		switch (s->layer) {
			case 1:
				frame_size = (frame_size * 12000) / sample_rate;
				frame_size = (frame_size + padding) * 4;
				break;
			case 2:
				frame_size = (frame_size * 144000) / sample_rate;
				frame_size += padding;
				break;
			default:
			case 3:
				frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
				frame_size += padding;
				break;
		}
		s->frame_size = frame_size;
	} else {
		/* if no frame size computed, signal it */
		return 1;
	}

#if 0
	printf("layer%d, %d Hz, %d kbits/s, ",
		   s->layer, s->sample_rate, s->bit_rate);
	if (s->nb_channels == 2) {
		if (s->layer == 3) {
			if (s->mode_ext & MODE_EXT_MS_STEREO)
				printf("ms-");
			if (s->mode_ext & MODE_EXT_I_STEREO)
				printf("i-");
		}
		printf("stereo");
	} else {
		printf("mono");
	}
	printf("\n");
#endif
	return 0;
}

static int mp3_parse_vbr_tags(mp3_reader_data * data, struct MP3Info *info,
							  uint32_t off)
{
	const uint32_t xing_offtbl[2][2] = { {32, 17}, {17, 9} };
	uint32_t b, frames;
	MPADecodeContext ctx;

	frames = -1;

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}

	b = LB_CONV(b);

	if (ff_mpa_check_header(b) < 0)
		return -1;

	ff_mpegaudio_decode_header(&ctx, b);

	if (ctx.layer != 3) {
		info->is_mpeg1or2 = true;
		return 0;
	}
	if (ctx.layer != 3)
		return -1;

	info->channels = ctx.nb_channels;
	info->sample_freq = ctx.sample_rate;

	buffered_reader_seek(data->r,
						 buffered_reader_position(data->r) +
						 xing_offtbl[ctx.lsf == 1][ctx.nb_channels == 1]);

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}
	b = LB_CONV(b);

	if (b == MKBETAG('X', 'i', 'n', 'g') || b == MKBETAG('I', 'n', 'f', 'o')) {
		if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
			return -1;
		}
		b = LB_CONV(b);
		if (b & 0x1) {
			if (buffered_reader_read(data->r, &frames, sizeof(frames)) !=
				sizeof(frames)) {
				return -1;
			}
			frames = LB_CONV(frames);
		}
	}

	/* Check for VBRI tag (always 32 bytes after end of mpegaudio header) */
	buffered_reader_seek(data->r, off + 4 + 32);
	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}
	b = LB_CONV(b);
	if (b == MKBETAG('V', 'B', 'R', 'I')) {
		uint16_t t;

		/* Check tag version */
		if (buffered_reader_read(data->r, &t, sizeof(t)) != sizeof(t)) {
			return -1;
		}
		t = ((t & 0xff) << 8) | (t >> 8);

		if (t == 1) {
			/* skip delay, quality and total bytes */
			buffered_reader_seek(data->r,
								 8 + buffered_reader_position(data->r));
			if (buffered_reader_read(data->r, &frames, sizeof(frames)) !=
				sizeof(frames)) {
				return -1;
			}
			frames = LB_CONV(frames);
		}
	}

	if ((int) frames < 0)
		return -1;

	info->spf = ctx.lsf ? 576 : 1152;	/* Samples per frame, layer 3 */
	info->duration = (double) frames *info->spf / info->sample_freq;

	info->frames = frames;
	info->average_bitrate = (double) data->size * 8 / info->duration;

	return frames;
}

static int id3v2_match(const uint8_t * buf)
{
	return buf[0] == 'I' &&
		buf[1] == 'D' &&
		buf[2] == '3' &&
		buf[3] != 0xff &&
		buf[4] != 0xff &&
		(buf[6] & 0x80) == 0 &&
		(buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 && (buf[9] & 0x80) == 0;
}

static const uint8_t ff_log2_tab[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7
};

static inline int av_log2(unsigned int v)
{
	int n = 0;

	if (v & 0xffff0000) {
		v >>= 16;
		n += 16;
	}
	if (v & 0xff00) {
		v >>= 8;
		n += 8;
	}
	n += ff_log2_tab[v];

	return n;
}

static void id3v2_read_ttag(mp3_reader_data * data, int taglen, char *dst,
							int dstlen, struct MP3Info *info)
{
	int len;

	if (dstlen > 0)
		dst[0] = 0;
	if (taglen < 1)
		return;

	taglen--;					/* account for encoding type byte */
	dstlen--;					/* Leave space for zero terminator */

	uint8_t b;

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return;
	}

	switch (b) {				/* encoding type */
			// TODO: non-standard hack...
		case 0:				/* ISO-8859-1 (0 - 255 maps directly into unicode) */
			info->tag.encode = config.mp3encode;
			{
				char *p = malloc(taglen);

				if (p == NULL) {
					return;
				}

				if (buffered_reader_read(data->r, p, taglen) != taglen) {
					free(p);
					return;
				}

				strncpy_s(dst, dstlen, p, taglen);
				free(p);
			}

			break;
		case 1:
			/*
			   UTF-16 [UTF-16] encoded Unicode [UNICODE] with BOM. All
			   strings in the same frame SHALL have the same byteorder.
			   Terminated with $00 00.
			 */
			{
				uint8_t *buf = malloc(taglen);

				if (buf == NULL)
					return;

				if (buffered_reader_read(data->r, buf, taglen) != taglen) {
					free(buf);
					return;
				}

				if (buf[0] != 0xff || buf[1] != 0xfe) {
					free(buf);
					return;
				}

				info->tag.encode = conf_encode_gbk;
				len = min(taglen - 2, dstlen - 1);
				memcpy(dst, buf + 2, len);
				dst[len] = 0;
				dbg_hexdump(d, (const uint8_t *) dst, len);
				charsets_ucs_conv((const byte *) dst, len, (byte *) dst, len);
				dbg_hexdump(d, (const uint8_t *) dst, len);
				free(buf);
				break;
			}
		case 3:				/* UTF-8 */
			info->tag.encode = conf_encode_utf8;
			len = min(taglen, dstlen - 1);
			if (buffered_reader_read(data->r, dst, len) < 0) {
				return;
			}
			dst[len] = 0;
			break;
	}
}

static unsigned int id3v2_get_size(mp3_reader_data * data, int len)
{
	int v = 0;
	uint8_t b;

	while (len--) {
		buffered_reader_read(data->r, &b, sizeof(b));
		v = (v << 8) + (b);
	}

	return v;
}

static unsigned int get_be16(mp3_reader_data * data)
{
	uint16_t val;

	buffered_reader_read(data->r, &val, sizeof(val));
	val = ((val & 0xff) << 8) | (val >> 8);
	return val;
}

static unsigned int get_be32(mp3_reader_data * data)
{
	uint32_t val;

	buffered_reader_read(data->r, &val, sizeof(val));
	val = LB_CONV(val);
	return val;
}

static uint8_t get_byte(mp3_reader_data * data)
{
	uint8_t val;

	buffered_reader_read(data->r, &val, sizeof(val));
	return val;
}

static unsigned int get_be24(mp3_reader_data * data)
{
	uint32_t val;

	val = get_be16(data) << 8;
	val |= get_byte(data);
	return val;
}

static void id3v2_parse(mp3_reader_data * data, struct MP3Info *info,
						int len, uint8_t version, uint8_t flags)
{
	int isv34, tlen;
	uint32_t tag;
	offset_t next;
	char tmp[16];
	int taghdrlen;
	const char *reason;

	switch (version) {
		case 2:
			if (flags & 0x40) {
				reason = "compression";
				goto error;
			}
			isv34 = 0;
			taghdrlen = 6;
			break;

		case 3:
		case 4:
			isv34 = 1;
			taghdrlen = 10;
			break;

		default:
			reason = "version";
			goto error;
	}

#if 0
	if (flags & 0x80) {
		reason = "unsynchronization";
		goto error;
	}
#endif

	if (isv34 && flags & 0x40) {	/* Extended header present, just skip over it */
		buffered_reader_seek(data->r,
							 id3v2_get_size(data,
											4) +
							 buffered_reader_position(data->r));
	}

	while (len >= taghdrlen) {
		if (isv34) {
			tag = get_be32(data);
			tlen = id3v2_get_size(data, 4);
			get_be16(data);		/* flags */
		} else {
			tag = get_be24(data);
			tlen = id3v2_get_size(data, 3);
		}
		len -= taghdrlen + tlen;

		if (len < 0)
			break;

		next = buffered_reader_position(data->r) + tlen;

		switch (tag) {
			case MKBETAG('T', 'I', 'T', '2'):
			case MKBETAG(0, 'T', 'T', '2'):
				id3v2_read_ttag(data, tlen, info->tag.title,
								sizeof(info->tag.title), info);
				break;
			case MKBETAG('T', 'E', 'N', 'C'):
			case MKBETAG(0, 'T', 'E', 'N'):
				id3v2_read_ttag(data, tlen, info->tag.encoder,
								sizeof(info->tag.encoder), info);
				break;
			case MKBETAG('T', 'P', 'E', '1'):
			case MKBETAG(0, 'T', 'P', '1'):
				id3v2_read_ttag(data, tlen, info->tag.author,
								sizeof(info->tag.author), info);
				break;
			case MKBETAG('T', 'A', 'L', 'B'):
			case MKBETAG(0, 'T', 'A', 'L'):
				id3v2_read_ttag(data, tlen, info->tag.album,
								sizeof(info->tag.album), info);
				break;
			case MKBETAG('C', 'O', 'M', 'M'):
				id3v2_read_ttag(data, tlen, info->tag.comment,
								sizeof(info->tag.comment), info);
				break;
			case MKBETAG('T', 'C', 'O', 'N'):
			case MKBETAG(0, 'T', 'C', 'O'):
				id3v2_read_ttag(data, tlen, info->tag.genre,
								sizeof(info->tag.genre), info);
				break;
			case MKBETAG('T', 'C', 'O', 'P'):
			case MKBETAG(0, 'T', 'C', 'R'):
				id3v2_read_ttag(data, tlen, info->tag.copyright,
								sizeof(info->tag.copyright), info);
				break;
			case MKBETAG('T', 'R', 'C', 'K'):
			case MKBETAG(0, 'T', 'R', 'K'):
				id3v2_read_ttag(data, tlen, tmp, sizeof(tmp), info);
				info->tag.track = atoi(tmp);
				break;
			case MKBETAG('T', 'D', 'R', 'C'):
			case MKBETAG('T', 'Y', 'E', 'R'):
				id3v2_read_ttag(data, tlen, tmp, sizeof(tmp), info);
				info->tag.year = atoi(tmp);
				break;
			case 0:
				/* padding, skip to end */
				buffered_reader_seek(data->r,
									 len + buffered_reader_position(data->r));
				len = 0;
				continue;
		}
		/* Skip to end of tag */
		buffered_reader_seek(data->r, next);
	}

	info->tag.type = ID3V2;

	if (version == 4 && flags & 0x10) {	/* Footer preset, always 10 bytes, skip over it */
		buffered_reader_seek(data->r, 10 + buffered_reader_position(data->r));
	}
	return;

  error:
	buffered_reader_seek(data->r, len + buffered_reader_position(data->r));
}

#define ID3v1_TAG_SIZE 128

static void id3v1_get_string(char *str, int str_size,
							 const uint8_t * buf, int buf_size)
{
	int i, c;
	char *q;

	q = str;

	for (i = 0; i < buf_size; i++) {
		c = buf[i];
		if (c == '\0')
			break;
		if ((q - str) >= str_size - 1)
			break;
		*q++ = c;
	}

	*q = '\0';
}

static int id3v1_parse_tag(mp3_reader_data * data, struct MP3Info *info,
						   const uint8_t * buf)
{
	char str[5];
	int genre;

	if (!(buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G'))
		return -1;

	info->tag.type = ID3V1;

	id3v1_get_string(info->tag.title, sizeof(info->tag.title), buf + 3, 30);
	id3v1_get_string(info->tag.author, sizeof(info->tag.author), buf + 33, 30);
	id3v1_get_string(info->tag.album, sizeof(info->tag.album), buf + 63, 30);
	id3v1_get_string(str, sizeof(str), buf + 93, 4);
	info->tag.year = atoi(str);
	id3v1_get_string(info->tag.comment, sizeof(info->tag.comment), buf + 97,
					 30);

	if (buf[125] == 0 && buf[126] != 0)
		info->tag.track = buf[126];

	genre = buf[127];

	if (genre <= ID3v1_GENRE_MAX)
		STRCPY_S(info->tag.genre, id3v1_genre_str[genre]);

	info->tag.encode = config.mp3encode;

	return 0;
}

static int _bitrate[9][16] = {
	{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,
	 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
	{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
};

static int _sample_freq[3][4] = {
	{44100, 48000, 32000, 0},
	{22050, 24000, 16000, 0},
	{11025, 12000, 8000, 0}
};

static int check_bc_combination(int bitrate, uint8_t channel_mode)
{
	if (bitrate == 64 || bitrate == 96 || bitrate == 112 || bitrate == 128
		|| bitrate == 160 || bitrate == 192)
		return 0;

	if (channel_mode == 0 || channel_mode == 1 || channel_mode == 2) {
		if (bitrate == 224 || bitrate == 256 || bitrate == 320
			|| bitrate == 384)
			return 0;
	}

	if (channel_mode == 3) {
		if (bitrate == 32 || bitrate == 48 || bitrate == 56 || bitrate == 80)
			return 0;
	}

	return -1;
}

static int calc_crc(buffered_reader_t * reader, size_t nbytes,
					uint16_t * crc_value)
{
	uint8_t *bytes;

	bytes = malloc(nbytes);

	if (bytes == NULL)
		return -1;

	if (buffered_reader_read(reader, bytes, nbytes) != nbytes) {
		free(bytes);
		return -1;
	}

	crc16(bytes, nbytes, crc_value);
	free(bytes);

	return 0;
}

static inline int parse_frame(uint8_t * h, int *lv, int *br,
							  struct MP3Info *info, mp3_reader_data * data,
							  offset_t start)
{
	uint8_t mp3_version, mp3_level;
	uint8_t layer;
	uint8_t crc;
	uint8_t bitrate_bit;
	uint8_t pad;
	uint8_t channel_mode;
	uint32_t framelenbyte;
	int bitrate;
	int freq;
	uint16_t mp3_samples_per_frames[9] =
		{ 384, 1152, 1152, 384, 1152, 576, 384, 1152, 576 };

	if (h[0] != 0xff)
		return -1;

	switch ((h[1] >> 3) & 0x03) {
		case 0:
			mp3_version = 2;	// MPEG 2.5
			break;
		case 2:
			mp3_version = 1;	// MPEG 2
			break;
		case 3:
			mp3_version = 0;	// MPEG 1
			break;
		default:
			mp3_version = 0;
			break;
	}

	mp3_level = 3 - ((h[1] >> 1) & 0x03);
	if (mp3_level == 3)
		mp3_level--;

	info->spf = mp3_samples_per_frames[mp3_version * 3 + mp3_level];

	layer = 4 - ((h[1] >> 1) & 3);

	if (layer < 1 || layer > 3)
		return -1;

	crc = !(h[1] & 1);
	bitrate_bit = (h[2] >> 4) & 0xf;
	bitrate = _bitrate[mp3_version * 3 + mp3_level][bitrate_bit];

	if (bitrate == 0)
		return -1;

	freq = _sample_freq[mp3_version][(h[2] >> 2) & 0x03];

	if (freq == 0) {
		return -1;
	}

	pad = (h[2] >> 1) & 1;
	channel_mode = (h[3] >> 6) & 3;

	if (layer == 2 && check_bc_combination(bitrate, channel_mode) != 0)
		return -1;

	if (info->check_crc && crc) {
		uint16_t crc_value, crc_frame;
		offset_t offset;

		crc_value = 0xffff;
		offset = buffered_reader_position(data->r);
		buffered_reader_seek(data->r, start);
		crc16((uint8_t *) & h[2], 2, &crc_value);
		buffered_reader_seek(data->r, 4 + buffered_reader_position(data->r));

		if (buffered_reader_read(data->r, &crc_frame, sizeof(crc_frame)) !=
			sizeof(crc_frame)) {
			buffered_reader_seek(data->r, offset);
			return -1;
		}

		crc_frame = crc_frame >> 8 | crc_frame << 8;

		switch (layer) {
			case 1:
				if (channel_mode == 3) {
					if (calc_crc(data->r, 128 / 8, &crc_value) < 0) {
						goto failed;
					}
				} else {
					if (calc_crc(data->r, 256 / 8, &crc_value) < 0) {
						goto failed;
					}
				}
				break;
			case 2:
				// TODO
				goto failed;
				break;
			case 3:
				if (channel_mode == 3) {
					if (calc_crc(data->r, 136 / 8, &crc_value) < 0) {
						goto failed;
					}
				} else {
					if (calc_crc(data->r, 256 / 8, &crc_value) < 0) {
						goto failed;
					}
				}
				break;
		}

		if (crc_value != crc_frame) {
			dbg_printf(d, "Checking crc failed: 0x%04x 0x%04x", crc_value,
					   crc_frame);
		  failed:
			buffered_reader_seek(data->r, offset);
			return -1;
		}

		buffered_reader_seek(data->r, offset);
		info->have_crc = true;
	}

	if (*lv == 0)
		*lv = layer;

	*br = bitrate;

	if (info->sample_freq == 0)
		info->sample_freq = freq;

	if (info->channels == 0) {
		if (channel_mode == 3)
			info->channels = 1;
		else
			info->channels = 2;
	}

	if (mp3_level == 0)
		framelenbyte = ((info->spf / 8 * 1000) * bitrate / freq + pad) << 2;
	else
		framelenbyte = (info->spf / 8 * 1000) * bitrate / freq + pad;

	return framelenbyte;
}

int skip_id3v2_tag_buffered(mp3_reader_data * data)
{
	int len;
	uint8_t buf[ID3v2_HEADER_SIZE];

	if (buffered_reader_read(data->r, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	if (id3v2_match(buf)) {
		struct MP3Info info;

		memset(&info, 0, sizeof(info));
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) |
			((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);
		id3v2_parse(data, &info, len, buf[3], buf[5]);
	} else {
		buffered_reader_seek(data->r, 0);
	}

	return 0;
}

int search_valid_frame_me_buffered(mp3_reader_data * data, int *brate)
{
	uint32_t off, start, dcount = 0;
	int size = 0;
	int end;
	int level;
	uint8_t buf[1024 + 4];
	struct MP3Info inf, *info;

	info = &inf;
	memset(&inf, 0, sizeof(inf));

	level = info->sample_freq = info->channels = info->frames = 0;
	off = buffered_reader_position(data->r);

	if (buffered_reader_read(data->r, buf, 4) != 4) {
		return -1;
	}

	start = 0;
	while ((end = buffered_reader_read(data->r, &buf[4], sizeof(buf) - 4)) > 0) {
		while (start < end) {
			size =
				parse_frame(buf + start, &level, brate, info, data,
							dcount * (sizeof(buf) - 4) + off + start);

			if (size > 0) {
				goto found;
				break;
			}

			start++;
		}

		start -= end;
		memmove(buf, &buf[end], 4);
		dcount++;
	}

	if (end <= 0) {
		return -1;
	}

  found:
	buffered_reader_seek(data->r, dcount * (sizeof(buf) - 4) + off + start);

	return size;
}

int read_mp3_info_brute_buffered(struct MP3Info *info, mp3_reader_data * data)
{
	uint32_t off;
	int size, br = 0, dcount = 0;
	int end;
	int level;

	buffered_reader_seek(data->r, 0);

	if (skip_id3v2_tag_buffered(data) == -1)
		return -1;

	static uint8_t *buf;

	buf = malloc(65536 + 4);

	if (!buf)
		return -1;

	off = buffered_reader_position(data->r);
	buffered_reader_seek(data->r, 0);

	if (buffered_reader_read(data->r, buf, 4) != 4) {
		free(buf);
		return -1;
	}

	uint32_t first_frame = (uint32_t) - 1;

	level = info->sample_freq = info->channels = info->frames = 0;
	info->frameoff = NULL;

	while ((end = buffered_reader_read(data->r, &buf[4], 65536)) > 0) {
		while (off < end) {
			int brate = 0;

			if ((size =
				 parse_frame(&buf[off], &level, &brate, info, data,
							 dcount * 65536 + off)) > 0) {
				br += brate;
				if (first_frame == (uint32_t) - 1)
					first_frame = dcount * 65536 + off;

				if (info->frames >= 0) {
					if (info->frames == 0)
						info->frameoff = malloc(sizeof(dword) * 1024);
					else
						info->frameoff =
							safe_realloc(info->frameoff,
										 sizeof(dword) * (info->frames + 1024));
					if (info->frameoff == NULL)
						info->frames = -1;
					else
						info->frameoff[info->frames++] = dcount * 65536 + off;
				}
				off += size;
			} else
				off++;
		}
		off -= end;
		memmove(buf, &buf[end], 4);
		dcount++;
	}

	if (info->frames) {
		info->duration = 1.0 * info->spf * info->frames / info->sample_freq;
		info->average_bitrate = (double) data->size * 8 / info->duration;
	}

	buffered_reader_seek(data->r, first_frame);
	free(buf);

	return 0;
}

int read_id3v2_tag_buffered(buffered_reader_t * reader, struct MP3Info *info)
{
	uint8_t buf[ID3v2_HEADER_SIZE];
	int len;
	mp3_reader_data data;

	data.r = reader;

	if (buffered_reader_read(data.r, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	if (id3v2_match(buf)) {
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) |
			((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);
		id3v2_parse(&data, info, len, buf[3], buf[5]);
	} else {
		buffered_reader_seek(data.r, 0);
	}

	return 0;
}

int read_mp3_info_buffered(struct MP3Info *info, mp3_reader_data * data)
{
	int ret;
	uint32_t off;

	info->frameoff = NULL;

	if (data->size > 128) {
		uint8_t buf[ID3v1_TAG_SIZE];

		buffered_reader_seek(data->r, data->size - 128);
		ret = buffered_reader_read(data->r, buf, ID3v1_TAG_SIZE);
		if (ret == ID3v1_TAG_SIZE) {
			id3v1_parse_tag(data, info, buf);
		}
	}

	buffered_reader_seek(data->r, 0);

	if (read_id3v2_tag_buffered(data->r, info) != 0) {
		return -1;
	}

	off = buffered_reader_position(data->r);

	if (mp3_parse_vbr_tags(data, info, off) < 0) {
		dbg_printf(d, "%s: No Xing header found, use brute force search method",
				   __func__);
		return read_mp3_info_brute_buffered(info, data);
	}

	buffered_reader_seek(data->r, off);

	return 0;
}

int free_mp3_info_buffered(struct MP3Info *info)
{
	if (info->frameoff) {
		free(info->frameoff);
		info->frameoff = NULL;
	}

	return 0;
}