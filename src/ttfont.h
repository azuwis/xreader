#ifndef _TTFONT_H_
#define _TTFONT_H_

#include <pspkernel.h>
#include <malloc.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "./common/datatype.h"

#define SBIT_HASH_SIZE (1024)

typedef struct Cache_Bitmap_
{
	int width;
	int height;
	int left;
	int top;
	char format;
	short max_grays;
	int pitch;
	unsigned char *buffer;
} Cache_Bitmap;

typedef struct SBit_HashItem_
{
	unsigned long ucs_code;
	int glyph_index;
	int size;
	bool anti_alias;
	bool cleartype;
	bool embolden;
	int xadvance;
	int yadvance;
	Cache_Bitmap bitmap;
} SBit_HashItem;

typedef struct _ttf
{
	FT_Library library;
	FT_Face face;

	char *fontName;
	bool antiAlias;
	bool cleartype;
	bool embolden;
	int pixelSize;

	SBit_HashItem sbitHashRoot[SBIT_HASH_SIZE];
	int cacheSize;
	int cachePop;

	char fnpath[PATH_MAX];
	int fileSize;
	byte *fileBuffer;
} t_ttf, *p_ttf;

extern p_ttf ttf_open(const char *filename, int size, bool load2mem);
extern p_ttf ttf_open_buffer(void *buf, size_t length, int pixelsize,
							 const char *ttfname);
extern p_ttf ttf_open_file(const char *filename, int pixelsize,
						   const char *ttfname);
extern void ttf_close(p_ttf ttf);
extern bool ttf_set_pixel_size(p_ttf ttf, int size);
extern void ttf_set_anti_alias(p_ttf ttf, bool cleartype);
extern void ttf_set_cleartype(p_ttf ttf, bool aa);
extern void ttf_set_embolden(p_ttf ttf, bool embolden);

extern int ttf_get_string_width(p_ttf cttf, p_ttf ettf, const byte * str,
								dword maxpixels, dword maxbytes,
								dword wordspace);
extern int ttf_get_string_width_hard(p_ttf cttf, p_ttf ettf, const byte * str,
									 dword maxpixels, dword wordspace);
extern void ttf_load_ewidth(p_ttf ttf, byte * ewidth, int size);
#endif
