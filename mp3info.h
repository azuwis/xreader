/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _MP3INFO_H_
#define _MP3INFO_H_

#include "common/datatype.h"
#include <mad.h>

typedef struct
{
	char artist[256];
	char title[256];
	int level;
	int bitrate;
	int samplerate;
	dword length;
	double framelen;
	int framecount;
	dword *frameoff;
	bool found_apetag;
	bool found_id3v2;
	bool found_id3v1;
} t_mp3info, *p_mp3info;

extern void mp3info_init(p_mp3info info);
extern bool mp3info_read(p_mp3info info, int fd);
extern void mp3info_free(p_mp3info info);
extern void readAPETag(p_mp3info pInfo, const char *filename);
extern void readID3Tag(p_mp3info pInfo, const char *filename);

#endif
