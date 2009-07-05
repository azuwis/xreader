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

#ifndef MUSICINFO_H
#define MUSICINFO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "common/datatype.h"
#include "conf.h"
#include "buffer.h"

	typedef enum
	{
		NONE = 0x0,
		ID3V1 = 0x1,
		ID3V2 = 0x2,
		APETAG = 0x4,
		VORBIS = 0x8,
		WMATAG = 0x10
	} MusicInfoTagType;

	typedef struct
	{
		t_conf_encode encode;
		MusicInfoTagType type;

		char title[80];
		char artist[80];
		char album[80];
		char comment[80];
	} MusicTagInfo;

	typedef struct
	{
		dword filesize;
		int sample_freq;
		int channels;
		int samples;
		double duration;
		double avg_bps;

		MusicTagInfo tag;
	} MusicInfo, *PMusicInfo;

	extern buffer *tag_lyric;

/**
 * ͨ�������ļ���ǩ��ȡ
 *
 * @param music_info ������Ϣ�ṹ��ָ��, ָ���Tag�������ѳ�ʼ����������Ϣ�ṹ��
 * @param spath �ļ���·��, 8.3�ļ�����ʽ
 *
 * @return �ɹ�����0, ���򷵻�-1
 */
	int generic_readtag(MusicInfo * music_info, const char *spath);

#ifdef __cplusplus
}
#endif

#endif
