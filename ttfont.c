/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#ifdef ENABLE_TTF

#include <pspkernel.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "charsets.h"
#include "ttfont.h"

typedef struct _ttf
{
	FT_Library library;
	FT_Face face;
	int bpr;
	int size;
	byte *buf;
	int buflen;
} t_ttf, *p_ttf;

extern void *ttf_open(const char *filename, int size, byte ** cbuffer)
{
	p_ttf ttf;

	if ((ttf = (p_ttf) calloc(1, sizeof(t_ttf))) == NULL) {
		return NULL;
	}
	int fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		free(ttf);
		return NULL;
	}
	ttf->buflen = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	ttf->buf = (byte *) malloc(ttf->buflen);
	if (ttf->buf == NULL) {
		free(ttf);
		sceIoClose(fd);
		return NULL;
	}
	sceIoRead(fd, ttf->buf, ttf->buflen);
	sceIoClose(fd);
	ttf->size = size;
	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf->buf);
		free(ttf);
		return NULL;
	}
	if (FT_New_Memory_Face(ttf->library, ttf->buf, ttf->buflen, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf->buf);
		free(ttf);
		return NULL;
	}

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		ttf_close(ttf);
		return NULL;
	}

	ttf->bpr = (size + 7) / 8;
	*cbuffer = (byte *) calloc(1, 0x5E02 * ttf->bpr * size);
	if (*cbuffer == NULL) {
		ttf_close(ttf);
		return NULL;
	}
	return ttf;
}

extern void *ttf_reopen(void *ttfh, int size, byte ** cbuffer)
{
	p_ttf ttf = (p_ttf) ttfh;

	ttf->size = size;

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		ttf_close(ttf);
		return NULL;
	}

	ttf->bpr = (size + 7) / 8;
	*cbuffer = (byte *) calloc(1, 0x5E02 * ttf->bpr * size);
	if (*cbuffer == NULL) {
		ttf_close(ttf);
		return NULL;
	}
	return ttf;
}

extern void *ttf_open_buffer(const byte * buf, int len, int size,
							 byte ** cbuffer)
{
	p_ttf ttf;

	if ((ttf = (p_ttf) calloc(1, sizeof(t_ttf))) == NULL) {
		return NULL;
	}
	ttf->buflen = len;
	ttf->buf = (byte *) buf;
	ttf->size = size;
	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf->buf);
		free(ttf);
		return NULL;
	}
	if (FT_New_Memory_Face(ttf->library, ttf->buf, ttf->buflen, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf->buf);
		free(ttf);
		return NULL;
	}

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		ttf_close(ttf);
		return NULL;
	}

	ttf->bpr = (size + 7) / 8;
	*cbuffer = (byte *) calloc(1, 0x5E02 * ttf->bpr * size);
	if (*cbuffer == NULL) {
		ttf_close(ttf);
		return NULL;
	}
	return ttf;
}

extern void ttf_close(void *ttfh)
{
	p_ttf ttf = (p_ttf) ttfh;

	if (ttf->buf != NULL)
		free(ttf->buf);
	FT_Done_Face(ttf->face);
	FT_Done_FreeType(ttf->library);
	free(ttf);
}

extern void *ttf_open_ascii(const char *filename, int size, byte ** ebuffer)
{
	p_ttf ttf;

	if ((ttf = (p_ttf) calloc(1, sizeof(t_ttf))) == NULL) {
		return NULL;
	}
	int fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		free(ttf);
		return NULL;
	}
	ttf->buflen = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	ttf->buf = (byte *) malloc(ttf->buflen);
	if (ttf->buf == NULL) {
		free(ttf);
		sceIoClose(fd);
		return NULL;
	}
	sceIoRead(fd, ttf->buf, ttf->buflen);
	sceIoClose(fd);
	ttf->size = size;
	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf->buf);
		free(ttf);
		return NULL;
	}
	if (FT_New_Memory_Face(ttf->library, ttf->buf, ttf->buflen, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf->buf);
		free(ttf);
		return NULL;
	}

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		ttf_close(ttf);
		return NULL;
	}

	ttf->bpr = (size + 7) / 8;
	*ebuffer = (byte *) calloc(1, 0x80 * ttf->bpr * size);
	if (*ebuffer == NULL) {
		ttf_close(ttf);
		return NULL;
	}
	return ttf;
}

extern void *ttf_open_ascii_buffer(const byte * buf, int len, int size,
								   byte ** ebuffer)
{
	p_ttf ttf;

	if ((ttf = (p_ttf) calloc(1, sizeof(t_ttf))) == NULL) {
		return NULL;
	}
	ttf->buflen = len;
	ttf->buf = (byte *) buf;
	ttf->size = size;
	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf->buf);
		free(ttf);
		return NULL;
	}
	if (FT_New_Memory_Face(ttf->library, ttf->buf, ttf->buflen, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf->buf);
		free(ttf);
		return NULL;
	}

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		ttf_close(ttf);
		return NULL;
	}

	ttf->bpr = (size + 7) / 8;
	*ebuffer = (byte *) calloc(1, 0x80 * ttf->bpr * size);
	if (*ebuffer == NULL) {
		ttf_close(ttf);
		return NULL;
	}
	return ttf;
}

extern void ttf_cache(void *ttfh, const byte * s, byte * zslot)
{
	p_ttf ttf = (p_ttf) ttfh;
	word u = charsets_gbk_to_ucs(s);

	if (FT_Load_Char(ttf->face, u, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) == 0
		&& ttf->face->glyph->bitmap.buffer != NULL) {
		memset(zslot, 0, ttf->bpr * ttf->size);
		int sz = ttf->face->glyph->bitmap.rows;
		int bpr2 = min(ttf->face->glyph->bitmap.pitch,
					   ttf->bpr * 8 - ttf->face->glyph->bitmap_left);
		int m, n, y =
			ttf->size - ttf->face->glyph->bitmap_top - (ttf->size + 8) / 9;
		if (y + sz > ttf->size)
			y = ttf->size - sz;
		if (y < 0)
			y = 0;
		byte *p = ttf->face->glyph->bitmap.buffer;
		int sx = ttf->face->glyph->bitmap_left;

		if (sx < 0)
			sx = 0;
		int maxx = sx + ttf->face->glyph->bitmap.width;

		if (maxx > ttf->size) {
			maxx = ttf->size;
			sx = 0;
		}
		for (m = 0; m < sz; m++) {
			int x = sx;

			for (n = 0; n < bpr2; n++) {
				byte e;

				for (e = 0x80; e > 0 && x < maxx; e >>= 1) {
					if (((*p) & e) > 0)
						zslot[y * ttf->bpr + (x >> 3)] |= (0x80 >> (x & 0x07));
					x++;
				}
				p++;
			}
			y++;
		}
	}
}

static void _cache_ascii(void *ttfh, word u, byte * zslot, byte * width)
{
	p_ttf ttf = (p_ttf) ttfh;

	if (FT_Load_Char(ttf->face, u, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) == 0
		&& ttf->face->glyph->bitmap.buffer != NULL) {
		memset(zslot, 0, ttf->bpr * ttf->size);
		int sz = ttf->face->glyph->bitmap.rows;
		int bpr2 = min(ttf->face->glyph->bitmap.pitch,
					   ttf->bpr * 8 - ttf->face->glyph->bitmap_left);
		int m, n, y =
			ttf->size - ttf->face->glyph->bitmap_top - (ttf->size + 8) / 9;
		if (y + sz > ttf->size)
			y = ttf->size - sz;
		if (y < 0)
			y = 0;
		byte *p = ttf->face->glyph->bitmap.buffer;
		int sx = ttf->face->glyph->bitmap_left;

		if (sx < 0)
			sx = 0;
		int maxx = sx + ttf->face->glyph->bitmap.width;

		if (maxx > ttf->size) {
			maxx = ttf->size;
			sx = 0;
		}
		*width = maxx;
		for (m = 0; m < sz; m++) {
			int x = 0;

			for (n = 0; n < bpr2; n++) {
				byte e;

				for (e = 0x80; e > 0 && x < maxx; e >>= 1) {
					if (((*p) & e) > 0)
						zslot[y * ttf->bpr + ((x + sx) >> 3)] |=
							(0x80 >> ((x + sx) & 0x07));
					x++;
					if (x >= ttf->face->glyph->bitmap.width)
						break;
				}
				p++;
			}
			y++;
		}
	}
}

extern bool ttf_cache_ascii(const char *filename, int size, byte ** ebuffer,
							byte * ewidth)
{
	p_ttf ttf = (p_ttf) ttf_open_ascii(filename, size, ebuffer);

	if (ttf == NULL)
		return false;
	word u;
	byte *buf = *ebuffer;

	for (u = 0x00; u < 0x80; u++) {
		_cache_ascii(ttf, u, buf, ewidth + u);
		buf += ttf->bpr * ttf->size;
	}
	ewidth[0x20] = size / 2;
	ttf_close(ttf);
	return true;
}

extern bool ttf_cache_ascii_buffer(const byte * buffer, int len, int size,
								   byte ** ebuffer, byte * ewidth)
{
	p_ttf ttf = (p_ttf) ttf_open_ascii_buffer(buffer, len, size, ebuffer);

	if (ttf == NULL)
		return false;
	word u;
	byte *buf = *ebuffer;

	for (u = 0x00; u < 0x80; u++) {
		_cache_ascii(ttf, u, buf, ewidth + u);
		buf += ttf->bpr * ttf->size;
	}
	ewidth[0x20] = size / 2;
	ttf_close(ttf);
	return true;
}

#endif
