/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#ifdef ENABLE_MUSIC

#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>
#include "charsets.h"
#include "mp3info.h"

static int _bitrate[9][16] = {
	{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
	{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
};

static int _samplerate[3][4] = {
	{44100, 48000, 32000, 0},
	{22050, 24000, 16000, 0},
	{11025, 12000, 8000, 0}
};

__inline int calc_framesize(byte h1, byte h2, int *mpl, int *br, int *sr)
{
	int mpv;

	switch ((h1 >> 3) & 0x03) {
		case 0:
			mpv = 2;			// MPEG 2.5
			break;
		case 2:
			mpv = 1;			// MPEG 2
			break;
		case 3:
			mpv = 0;			// MPEG 1
			break;
		default:
			return 0;
	}
	if (*mpl == 0) {
		*mpl = 3 - ((h1 >> 1) & 0x03);
		if (*mpl == 3)
			return 0;
	} else if (*mpl != 3 - ((h1 >> 1) & 0x03))
		return 0;
	*br = _bitrate[mpv * 3 + *mpl][h2 >> 4];
	if (*sr == 0)
		*sr = _samplerate[mpv][(h2 >> 2) & 0x03];
	else if (*sr != _samplerate[mpv][(h2 >> 2) & 0x03])
		return 0;
	if (*br > 0 && *sr > 0) {
		if (*mpl == 0)
			return (12000 * *br / *sr + (int) ((h2 >> 1) & 0x01)) * 4;
		else
			return 144000 * *br / *sr + (int) ((h2 >> 1) & 0x01);
	}
	return 0;
}

__inline void read_id3tag(byte * buf, byte * tag, int tsize)
{
	switch (buf[0]) {
		case 0:
			memcpy(tag, &buf[1], tsize - 1);
			tag[tsize - 1] = 0;
			break;
		case 1:
			memcpy(tag, &buf[3], tsize - 3);
			tag[tsize - 1] = 0;
			charsets_utf16_conv(tag, tag);
			break;
		case 2:
			memcpy(tag, &buf[1], tsize - 1);
			tag[tsize - 1] = 0;
			charsets_utf16be_conv(tag, tag);
			break;
		case 3:
			memcpy(tag, &buf[1], tsize - 1);
			tag[tsize - 1] = 0;
			charsets_utf8_conv(tag, tag);
			break;
	}
}

extern void mp3info_init(p_mp3info info)
{
	memset(info, 0, sizeof(t_mp3info));
}

extern bool mp3info_read(p_mp3info info, int fd)
{
	if (fd < 0)
		return false;
	static byte buf[65538];

	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	dword off;
	int size, br = 0, dcount = 0;
	int end;

	sceIoRead(fd, buf, 2);
	off = 0;
	while ((end = sceIoRead(fd, &buf[2], 65536)) > 0) {
		while (off < end) {
			int brate = 0;

			if (buf[off] == 0xFF && (buf[off + 1] & 0xE0) == 0xE0
				&& (size =
					calc_framesize(buf[off + 1], buf[off + 2], &info->level,
								   &brate, &info->samplerate)) > 0) {
				br += brate;
				if (info->framecount >= 0) {
					if (info->framecount == 0)
						info->frameoff = malloc(sizeof(dword) * 1024);
					else
						info->frameoff =
							realloc(info->frameoff,
									sizeof(dword) * (info->framecount + 1024));
					if (info->frameoff == NULL)
						info->framecount = -1;
					else
						info->frameoff[info->framecount++] =
							dcount * 65536 + off;
				}
				off += size;
			} else if (buf[off + 1] == 'D'
					   && ((buf[off] == 'I' && buf[off + 2] == '3')
						   || (buf[off] == '3' && buf[off + 2] == 'I'))) {
				// TODO: ID3v2 part parsing
				off += 6;
				int id3size =
					((dword) buf[off] << 21) + ((dword) buf[off + 1] << 14) +
					((dword) buf[off + 2] << 7) + (dword) buf[off + 3] + 4;
				if (info->artist[0] == 0 || info->title[0] == 0) {
					int off2 = off + 4, end2 = min(end, off + id3size);

					while (off2 < end2) {
						if (buf[off2] == 0)
							break;
						off2 += 4;
						if (off2 + 4 < end2)
							break;
						int tsize =
							(int) (((dword) buf[off2] << 21) +
								   ((dword) buf[off2 + 1] << 14) +
								   ((dword) buf[off2 + 2] << 7) +
								   (dword) buf[off2 + 3]);
						if (off2 + 6 + tsize < end2)
							break;
						if ((buf[off2 + 4] & 0x60) > 0)
							off2 += 6;
						else if (info->artist[0] == 0 && buf[off2 - 4] == 'T'
								 && buf[off2 - 3] == 'P' && buf[off2 - 2] == 'E'
								 && buf[off2 - 1] == '1') {
							off2 += 6;
							read_id3tag(&buf[off2], (byte *) info->artist,
										tsize);
						} else if (info->title[0] == 0 && buf[off2 - 4] == 'T'
								   && buf[off2 - 3] == 'I'
								   && buf[off2 - 2] == 'T'
								   && buf[off2 - 1] == '2') {
							off2 += 6;
							read_id3tag(&buf[off2], (byte *) info->title,
										tsize);
						} else
							off2 += 6;
						off2 += tsize;
					}
				}
				off += id3size;
			} else
				off++;
		}
		off -= end;
		memmove(buf, &buf[end], 2);
		dcount++;
	}
	if ((dcount > 1 || off >= 128)
		&& (info->artist[0] == 0 || info->title[0] == 0)) {
		sceIoLseek32(fd, -128, PSP_SEEK_END);
		sceIoRead(fd, buf, 128);
		if (buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G') {
			if (info->artist[0] == 0) {
				memcpy(info->artist, &buf[3], 30);
			}
			if (info->title[0] == 0) {
				memcpy(info->title, &buf[33], 30);
			}
		}
	}
	if (info->framecount > 0) {
		sceIoLseek32(fd, info->frameoff[0], PSP_SEEK_SET);
		info->bitrate = (br * 2 + info->framecount) / info->framecount / 2;
		if (info->level == 0) {
			info->framelen = 384.0 / (double) info->samplerate;
			info->length = 384 * info->framecount / info->samplerate;
		} else {
			info->framelen = 1152.0 / (double) info->samplerate;
			info->length = 1152 * info->framecount / info->samplerate;
		}
	} else
		sceIoLseek32(fd, 0, PSP_SEEK_SET);
	return true;
}

extern void mp3info_free(p_mp3info info)
{
	if (info->frameoff != NULL) {
		free((void *) info->frameoff);
		info->frameoff = NULL;
	}
	memset(info, 0, sizeof(t_mp3info));
}

#endif
