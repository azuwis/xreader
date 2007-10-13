#ifndef _WMADEC_H
#define _WMADEC_H

#define WMA_MAX_BUF_SIZE 131072

typedef struct _WmaFile {
	void * h;
	unsigned char * outbuf, * inbuf;
	int insize;
	int wma_idx;
	char title[512];
	char author[512];
	int duration;
	int channels;
	int bitrate;
	int samplerate;
} WmaFile;

extern void wma_init();
extern WmaFile * wma_open(const char * filename);
extern void wma_close(WmaFile * wma);
extern void wma_seek(WmaFile * wma, long long seekpos);
extern unsigned char * wma_decode_frame(WmaFile * wma, int * outsize);
extern int wma_check(char *filename);

#endif
