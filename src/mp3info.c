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

#include "config.h"

#ifdef ENABLE_MUSIC

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/utils.h"
#include "charsets.h"
#include "mp3info.h"
#include "libid3tag/id3tag.h"
#include "apetaglib/APETag.h"
#include "dbg.h"

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

static int _samplerate[3][4] = {
	{44100, 48000, 32000, 0},
	{22050, 24000, 16000, 0},
	{11025, 12000, 8000, 0}
};

int g_libid3tag_found_id3v2 = 0;

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

extern void mp3info_init(p_mp3info info)
{
	memset(info, 0, sizeof(t_mp3info));
}

void GetTagInfo(struct id3_tag *pTag, const char *key, char *dest, int size)
{
	struct id3_frame *frame = id3_tag_findframe(pTag, key, 0);

	if (frame == 0) {
//      dbg_printf(d, "%s: Frame \"%s\" not found!", __func__, key);
		return;
	}

	/*
	   dbg_printf(d, "%s: id: %s desc: %s flags: 0x%08x group_id: %d "
	   "encryption_method: %d",
	   __func__, frame->id, frame->description, frame->flags,
	   frame->group_id, frame->encryption_method);
	 */

	int i;
	union id3_field *field;

	for (i = 0; i < frame->nfields; i++) {
//      dbg_printf(d, "field %d in frame", i);
		field = id3_frame_field(frame, i);
		if (field == NULL) {
//          dbg_printf(d, "continue1");
			continue;
		}
//      dbg_printf(d, "Type: %u", field->type);
		id3_ucs4_t const *str = id3_field_getstrings(field, 0);

		if (str == NULL) {
//          dbg_printf(d, "continue2");
			continue;
		}
		id3_utf8_t *pUTF8Str = id3_ucs4_utf8duplicate(str);

		if (pUTF8Str == NULL) {
//          dbg_printf(d, "continue3");
			continue;
		}
//      dbg_printf(d, "%s: Get UTF8 String: %s", __func__, pUTF8Str);
		strcpy_s(dest, size, (const char *) pUTF8Str);
		//          char *pChinese = ConvertUTF8toGB2312(pUTF8Str, -1);
		//          dbg_printf(d, "标题：%s", pChinese);
		//          free(pChinese);
		free(pUTF8Str);
	}
}

/// 使用ID3Taglib读取艺术家名和音乐名到mp3info.artist, mp3info.title
/// 设置字ftruncate符编码为UTF8
/// 如果ID3原始编码为LATIN1，使用config.mp3encode设置值进行编码
extern void readID3Tag(p_mp3info pInfo, const char *filename)
{
	if (filename == NULL)
		return;

	STRCPY_S(pInfo->title, "");
	STRCPY_S(pInfo->artist, "");

	g_libid3tag_found_id3v2 = 0;

	struct id3_file *pFile = id3_file_open(filename, ID3_FILE_MODE_READONLY);

	if (pFile == NULL) {
		return;
	}

	struct id3_tag *pTag = id3_file_tag(pFile);

	if (pTag == NULL) {
		id3_file_close(pFile);
		return;
	}

	/*
	   dbg_printf(d,
	   "%s: version: 0x%08x flags: 0x%08x extendedflags: 0x%08x restrictions: 0x%08x options: 0x%08x",
	   __func__, pTag->version, pTag->flags, pTag->extendedflags,
	   pTag->restrictions, pTag->options);
	 */
	if (g_libid3tag_found_id3v2 == 0) {
		/*
		   dbg_printf(d,
		   "no ID3tagv2 found, don't use libid3tag to decode, aborting");
		 */
		id3_file_close(pFile);
		return;
	}

	GetTagInfo(pTag, ID3_FRAME_TITLE, pInfo->title, sizeof(pInfo->title));
	GetTagInfo(pTag, ID3_FRAME_ARTIST, pInfo->artist, sizeof(pInfo->title));

	id3_file_close(pFile);

	pInfo->found_id3v2 = true;
}

extern void readAPETag(p_mp3info pInfo, const char *filename)
{
	APETag *tag = loadAPETag(filename);

	if (tag == NULL) {
		pInfo->found_apetag = false;
	} else {
		char *sTitle = APETag_SimpleGet(tag, "Title");
		char *sArtist = APETag_SimpleGet(tag, "Artist");
		char *sArtist2 = APETag_SimpleGet(tag, "Album artist");

		STRCPY_S(pInfo->title, sTitle);
		if (sArtist)
			STRCPY_S(pInfo->artist, sArtist);
		else if (sArtist2)
			STRCPY_S(pInfo->artist, sArtist2);

		if (sTitle)
			free(sTitle);
		if (sArtist)
			free(sArtist);
		if (sArtist2)
			free(sArtist2);

		freeAPETag(tag);
		pInfo->found_apetag = true;
	}
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
					calc_framesize(buf[off + 1], buf[off + 2],
								   &info->level, &brate,
								   &info->samplerate)) > 0) {
				br += brate;
				if (info->framecount >= 0) {
					if (info->framecount == 0)
						info->frameoff = malloc(sizeof(dword) * 1024);
					else
						info->frameoff =
							safe_realloc(info->frameoff,
										 sizeof(dword) *
										 (info->framecount + 1024));
					if (info->frameoff == NULL)
						info->framecount = -1;
					else
						info->frameoff[info->
									   framecount++] = dcount * 65536 + off;
				}
				off += size;
			} else
				off++;
		}
		off -= end;
		memmove(buf, &buf[end], 2);
		dcount++;
	}
	if (!info->found_id3v2 && !info->found_apetag) {
		if ((dcount > 1 || off >= 128)
			&& (info->artist[0] == 0 || info->title[0] == 0)) {
			sceIoLseek32(fd, -128, PSP_SEEK_END);
			sceIoRead(fd, buf, 128);
			if (buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G') {
				if (info->artist[0] == 0) {
					memcpy(info->artist, &buf[33], 30);
				}
				if (info->title[0] == 0) {
					memcpy(info->title, &buf[3], 30);
				}
				info->found_id3v1 = true;
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
		free(info->frameoff);
		info->frameoff = NULL;
	}
	memset(info, 0, sizeof(t_mp3info));
}

#endif
