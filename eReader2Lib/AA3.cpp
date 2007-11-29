/*
 * $Id: AA3.cpp 80 2007-11-07 17:19:12Z soarchin $

 * Copyright 2007 aeolusc

 * This file is part of eReader2.

 * eReader2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * eReader2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pspkernel.h>
#include <malloc.h>
#include <pspaudiocodec.h>
#include <pspaudio.h>
#include "Thread.h"
#include "Charsets.h"
#include "Music.h"
#include "AA3.h"

extern "C"
{
	int sceAudio_38553111(unsigned short samples, unsigned short freq, char);
	int sceAudio_5C37C0AE(void);
	int sceAudio_E0727056(int volume, void *buffer);
}

AA3::AA3(MusicManager * manager, CONST CHAR * filename): Music(manager, filename)
{
//		m_info.frames.clear();
	memset(&m_info, 0, sizeof(AA3Info)/* - sizeof(m_info.frames)*/);
}

AA3::~AA3()
{
}

/*INT AA3::_CalcFrameSize(BYTE h1, BYTE h2, BYTE h3, INT& mpl, INT& br, INT& sr, INT& chs)
{
	STATIC int _bitrate[9][16] = {
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

	STATIC int _samplerate[3][4] = {
		{44100, 48000, 32000, 0},
		{22050, 24000, 16000, 0},
		{11025, 12000, 8000, 0}
	};
	INT mpv;
	switch((h1 >> 3) & 0x03)
	{
	case 0:
		mpv = 2; // MPEG 2.5
		break;
	case 2:
		mpv = 1; // MPEG 2
		break;
	case 3:
		mpv = 0; // MPEG 1
		break;
	default:
		return 0;
	}
	if(mpl == 0)
	{
		mpl = 3 - ((h1 >> 1) & 0x03);
		if(mpl == 3)
			return 0;
	}
	else if(mpl != 3 - ((h1 >> 1) & 0x03))
		return 0;
	br = _bitrate[mpv * 3 + mpl][h2 >> 4];
	if(sr == 0)
		sr = _samplerate[mpv][(h2 >> 2) & 0x03];
	else if(sr != _samplerate[mpv][(h2 >> 2) & 0x03])
		return 0;
	if(br > 0 && sr > 0)
	{
		if(mpl == 0)
			return (12000 * br / sr + (int)((h2 >> 1) & 0x01)) * 4;
		else
			return 144000 * br / sr + (int)((h2 >> 1) & 0x01);
	}
	chs = ((h3 >> 6) == 3) ? 1 : 2;
	return 0;
}
VOID AA3::_ReadID3Tag(BYTE * buf, WORD * tag, INT tsize)
{
	CHAR * stag = new CHAR[tsize];
	memcpy(stag, &buf[1], tsize - 1);
	stag[tsize - 1] = 0;
	switch(buf[0])
	{
	case 0:
		GBK.ProcessStr(stag, tag);
		break;
	case 1:
		UCS.ProcessStr(stag, tag);
		break;
	case 2:
		UCSBE.ProcessStr(stag, tag);
		break;
	case 3:
		UTF8.ProcessStr(stag, tag);
		break;
	}
	delete[] stag;
}*/
VOID AA3::_ReadInfo()
{
/*		if(m_file == NULL)
		return;
	STATIC BYTE buf[65538];
	m_file->Seek(0, FATFile::FS_BEGIN);
	DWORD off;
	INT size, br = 0, dcount = 0;
	DWORD end;
	m_file->Read(buf, 2);
	off = 0;
	m_info.maxframesize = 0;
	while((end = m_file->Read(&buf[2], 65536)) > 0)
	{
		while(off < end)
		{
			INT brate = 0;
			if(buf[off] == 0xFF && (buf[off + 1] & 0xE0) == 0xE0 && (size = _CalcFrameSize(buf[off + 1], buf[off + 2], buf[off + 3], m_info.level, brate, m_info.samplerate, m_info.channels)) > 0)
			{
				br += brate;
				_MP3FrameInfo info;
				info.frameoff = dcount * 65536 + off;
				info.framesize = size;
				if((DWORD)size > m_info.maxframesize)
					m_info.maxframesize = size;
				m_info.frames.push_back(info);
				off += size;
			}
			else if(buf[off + 1] == 'D' && ((buf[off] == 'I' && buf[off + 2] == '3') || (buf[off] == '3' && buf[off + 2] == 'I')))
			{
				INT orgoff = m_file->Tell();
				off += 6;
				m_file->Seek(off - (INT)end - 2, FATFile::FS_CURRENT);
				BYTE tagsize[4];
				m_file->Read(tagsize, 4);
				off += 4;
				INT id3size = ((DWORD)tagsize[0] << 21) + ((DWORD)tagsize[1] << 14) + ((DWORD)tagsize[2] << 7) + (DWORD)tagsize[3];
				if(m_info.artist[0] == 0 || m_info.title[0] == 0)
				{
					BYTE * id3buf = new BYTE[id3size];
					m_file->Read(id3buf, id3size);
					INT off2 = 0;
					while(off2 < id3size)
					{
						if(id3buf[off2] == 0)
							break;
						off2 += 4;
						if(off2 + 4 > id3size)
							break;
						int tsize = (int)(((DWORD)id3buf[off2] << 21) + ((DWORD)id3buf[off2 + 1] << 14) + ((DWORD)id3buf[off2 + 2] << 7) + (DWORD)id3buf[off2 + 3]);
						if(off2 + 6 + tsize > id3size)
							break;
						if((id3buf[off2 + 4] & 0xC0) > 0)
							off2 += 6;
						else if(m_info.artist[0] == 0 && id3buf[off2 - 4] == 'T' && id3buf[off2 - 3] == 'P' && id3buf[off2 - 2] == 'E' && id3buf[off2 - 1] == '1')
						{
							off2 += 6;
							_ReadID3Tag(&id3buf[off2], (WORD *)m_info.artist, tsize);
						}
						else if(m_info.title[0] == 0 && id3buf[off2 - 4] == 'T' && id3buf[off2 - 3] == 'I' && id3buf[off2 - 2] == 'T' && id3buf[off2 - 1] == '2')
						{
							off2 += 6;
							_ReadID3Tag(&id3buf[off2], (WORD *)m_info.title, tsize);
						}
						else
							off2 += 6;
						off2 += tsize;
					}
					delete[] id3buf;
				}
				m_file->Seek(orgoff, FATFile::FS_BEGIN);
				off += id3size;
			}
			else
				off ++;
		}
		off -= end;
		memmove(buf, &buf[end], 2);
		dcount ++;
	}
	if((dcount > 1 || off >= 128) && (m_info.artist[0] == 0 || m_info.title[0] == 0))
	{
		m_file->Seek(-128, FATFile::FS_END);
		m_file->Read(buf, 128);
		if(buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G')
		{
			CHAR ts[32];
			ts[30] = 0;
			if(m_info.artist[0] == 0)
			{
				memcpy(ts, &buf[3], 30);
				GBK.ProcessStr(ts, (WORD *&)m_info.artist);
			}
			if(m_info.title[0] == 0)
			{
				memcpy(ts, &buf[33], 30);
				GBK.ProcessStr(ts, (WORD *&)m_info.title);
			}
		}
	}
	if(m_info.frames.size() > 0)
	{
		m_file->Seek(m_info.frames[0].frameoff, FATFile::FS_BEGIN);
		m_info.bitrate = (br * 2 + m_info.frames.size()) / m_info.frames.size() / 2;
		if(m_info.level == 0)
		{
			m_info.framelen = 384.0 / (double)m_info.samplerate;
			m_info.length = 384 * m_info.frames.size() / m_info.samplerate;
		}
		else
		{
			m_info.framelen = 1152.0 / (double)m_info.samplerate;
			m_info.length = 1152 * m_info.frames.size() / m_info.samplerate;
		}
	}
	else
		m_file->Seek(0, FATFile::FS_BEGIN);
*/
}

VOID AA3::_FreeInfo()
{
//		m_info.frames.clear();
	memset(&m_info, 0, sizeof(AA3Info)/* - sizeof(m_info.frames)*/);
}

#define TYPE_ATRAC3 0x270
#define TYPE_ATRAC3PLUS 0xFFFE
VOID AA3::_PlayCycle()
{
	u8 ea3_header[0x60];
	m_file->Seek(0xC00, FATFile::FS_BEGIN);
	if(m_file->Read(ea3_header, 0x60) != 0x60)
		return;
	if(*(DWORD *)ea3_header != 0x01334145)
		return;

	BYTE aa3_at3plus_flagdata[2];
	aa3_at3plus_flagdata[0] = ea3_header[0x22];
	aa3_at3plus_flagdata[1] = ea3_header[0x23];

	WORD aa3_type = (ea3_header[0x22] == 0x20) ? TYPE_ATRAC3 : ((ea3_header[0x22] == 0x28) ? TYPE_ATRAC3PLUS : 0x0);

	if ( aa3_type != TYPE_ATRAC3 && aa3_type != TYPE_ATRAC3PLUS )
		return;

	WORD aa3_data_align;
	if ( aa3_type == TYPE_ATRAC3 )
		aa3_data_align = ea3_header[0x23]*8;
	else
		aa3_data_align = (ea3_header[0x23]+1)*8;

	DWORD aa3_data_start = 0x0C60;
	DWORD aa3_data_size = m_file->GetSize() - aa3_data_start;

	if ( aa3_data_size % aa3_data_align != 0 )
		return;

	m_file->Seek(aa3_data_start, FATFile::FS_BEGIN);

	WORD aa3_channel_mode = 0x0;
	BYTE * aa3_data_buffer = NULL;
	DWORD aa3_sample_per_frame;
	if ( aa3_type == TYPE_ATRAC3 ) {
		if ( aa3_data_align == 0xC0 )
			aa3_channel_mode = 0x1;
		aa3_sample_per_frame = 1024;
		aa3_data_buffer = (u8*)memalign(64, 0x180);
		if ( aa3_data_buffer == NULL)
			return;
		m_codec_buffer[26] = 0x20;
		if ( sceAudiocodecCheckNeedMem(m_codec_buffer, 0x1001) < 0 )
			return;
		if ( sceAudiocodecGetEDRAM(m_codec_buffer, 0x1001) < 0 )
			return;
		m_getEDRAM = TRUE;
		m_codec_buffer[10] = 4;
		m_codec_buffer[44] = 2;
		if ( aa3_data_align == 0x130 )
			m_codec_buffer[10] = 6;
		if ( sceAudiocodecInit(m_codec_buffer, 0x1001) < 0 )
			return;
	}
	else if ( aa3_type == TYPE_ATRAC3PLUS ) {
		aa3_sample_per_frame = 2048;
		aa3_data_buffer = (u8*)memalign(64, (aa3_data_align + 8 + 0x3F) & ~0x3F);
		if ( aa3_data_buffer == NULL)
			return;
		m_codec_buffer[5] = 0x1;
		m_codec_buffer[10] = aa3_at3plus_flagdata[1];
		m_codec_buffer[10] = (m_codec_buffer[10] << 8 ) | aa3_at3plus_flagdata[0];
		m_codec_buffer[12] = 0x1;
		m_codec_buffer[14] = 0x1;
		if ( sceAudiocodecCheckNeedMem(m_codec_buffer, 0x1000) < 0 )
			return;
		if ( sceAudiocodecGetEDRAM(m_codec_buffer, 0x1000) < 0 )
			return;
		m_getEDRAM = TRUE;
		if ( sceAudiocodecInit(m_codec_buffer, 0x1000) < 0 )
			return;
	}
	else
		return;

	SHORT * mix_buffer[4];
	for(INT i = 0; i < 4; i ++)
		mix_buffer[i] = (SHORT *)memalign(64, aa3_sample_per_frame * 2 * sizeof(SHORT));
	sceAudio_38553111(aa3_sample_per_frame, 44100, 2);
	INT mixidx = 0;
	while( !GetSkip() ) {
		while(GetPause())
			Thread::Delay(1000000);
		memset(mix_buffer[mixidx], 0, aa3_sample_per_frame * 2 * sizeof(SHORT));
		DWORD decode_type = 0x1001;
		if ( aa3_type == TYPE_ATRAC3 ) {
			memset( aa3_data_buffer, 0, 0x180);
			if (m_file->Read(aa3_data_buffer, aa3_data_align ) != aa3_data_align)
				break;
			if ( aa3_channel_mode )
				memcpy(aa3_data_buffer + aa3_data_align, aa3_data_buffer, aa3_data_align);
			decode_type = 0x1001;
		}
		else {
			memset( aa3_data_buffer, 0, aa3_data_align + 8);
			aa3_data_buffer[0] = 0x0F;
			aa3_data_buffer[1] = 0xD0;
			aa3_data_buffer[2] = aa3_at3plus_flagdata[0];
			aa3_data_buffer[3] = aa3_at3plus_flagdata[1];
			if (m_file->Read( aa3_data_buffer + 8, aa3_data_align ) != aa3_data_align)
				break;
			decode_type = 0x1000;
		}

		m_codec_buffer[6] = (DWORD)aa3_data_buffer;
		m_codec_buffer[8] = (DWORD)mix_buffer[mixidx];

		if ( sceAudiocodecDecode(m_codec_buffer, decode_type ) < 0 )
			break;
		sceAudio_E0727056(0x8000, (CHAR *)mix_buffer[mixidx]);
		mixidx = (mixidx + 1) & 3;
		if(GetStop())
		{
			SetStop(FALSE);
			SetPause(TRUE);
			m_file->Seek(aa3_data_start, FATFile::FS_BEGIN);
		}
	}

	free(aa3_data_buffer);
	for(INT i = 0; i < 4; i ++)
		free(mix_buffer[i]);
	sceAudio_5C37C0AE();
}
