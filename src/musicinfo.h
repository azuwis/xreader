#ifndef MUSICINFO_H
#define MUSICINFO_H

#include "common/datatype.h"
#include "conf.h"

typedef enum {
	NONE   =  0x0,
	ID3V1  =  0x1,
	ID3V2  =  0x2,
	APETAG =  0x4
} MusicInfoTagType;

typedef struct {
	t_conf_encode encode;
	MusicInfoTagType type;

	char title[80];
	char artist[80];
	char album[80];
	char comment[80];
} MusicTagInfo;

typedef struct {
	dword filesize;
	int sample_freq;
	int channels;
	int samples;
	double duration;
	double avg_bps;
	bool is_seekable;

	MusicTagInfo tag;
} MusicInfo, *PMusicInfo;

#endif
