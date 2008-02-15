/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _TTFONT_H_
#define _TTFONT_H_

#include "./common/datatype.h"

extern void *ttf_open(const char *filename, int size, byte ** cbuffer);
extern void *ttf_reopen(void *ttfh, int size, byte ** cbuffer);
extern void *ttf_open_buffer(const byte * buf, int len, int size,
							 byte ** cbuffer);
extern void *ttf_open_ascii(const char *filename, int size, byte ** ebuffer);
extern void *ttf_open_ascii_buffer(const byte * buf, int len, int size,
								   byte ** ebuffer);
extern void ttf_close(void *ttfh);
extern void ttf_cache(void *ttfh, const byte * s, byte * zslot);
extern bool ttf_cache_ascii(const char *filename, int size, byte ** ebuffer,
							byte * ewidth);
extern bool ttf_cache_ascii_buffer(const byte * buffer, int len, int size,
								   byte ** ebuffer, byte * ewidth);

#endif
