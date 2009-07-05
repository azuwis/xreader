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

#include "config.h"

#include <stdio.h>
#include <win.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include <unzip.h>
#include "conf.h"
#include "ttfont.h"
#include "display.h"
#include "pspscreen.h"
#include "iniparser.h"
#include "strsafe.h"
#include "fs.h"
#include "archive.h"
#include "dbg.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static bool auto_inc_wordspace_on_small_font = false;
static pixel *vram_disp = NULL;
pixel *vram_draw = NULL;
static bool vram_page = 0;
static byte *cfont_buffer = NULL, *book_cfont_buffer = NULL, *efont_buffer =
	NULL, *book_efont_buffer = NULL;
int DISP_FONTSIZE = 16, DISP_BOOK_FONTSIZE = 16, HRR = 6, WRR = 15;
static int DISP_EFONTSIZE, DISP_CFONTSIZE, DISP_CROWSIZE, DISP_EROWSIZE,
	fbits_last = 0, febits_last =
	0, DISP_BOOK_EFONTSIZE, DISP_BOOK_CFONTSIZE, DISP_BOOK_EROWSIZE,
	DISP_BOOK_CROWSIZE, fbits_book_last = 0, febits_book_last = 0;;
byte disp_ewidth[0x80];

bool using_ttf = false;

#ifdef ENABLE_TTF
static bool g_ttf_share_buffer = false;
#endif

#ifdef ENABLE_TTF
//static byte *cache = NULL;
//static void *ttfh = NULL;
p_ttf ettf = NULL, cttf = NULL, cttfinfo = NULL, ettfinfo = NULL;
#endif

typedef struct _VertexColor
{
	pixel color;
	u16 x, y, z;
} VertexColor;

typedef struct _Vertex
{
	u16 u, v;
	pixel color;
	u16 x, y, z;
} Vertex;

static inline void setVertex(VertexColor * vertex, u16 x, u16 y, u16 z,
							 u32 color)
{
	vertex->x = x, vertex->y = y, vertex->z = z;
	vertex->color = color;
}

static inline void setVertexUV(Vertex * vertex, u16 x, u16 y, u16 z, u32 color,
							   u16 u, u16 v)
{
	vertex->x = x, vertex->y = y, vertex->z = z;
	vertex->color = color;
	vertex->u = u;
	vertex->v = v;
}

#define DISP_RSPAN 0

extern void disp_init(void)
{
	xrDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	vram_page = 0;
	vram_disp = (pixel *) 0x04000000;
	vram_draw = (pixel *) (0x44000000 + PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	xrDisplaySetFrameBuf(vram_disp, PSP_SCREEN_SCANLINE, PSP_DISPLAY_PIXEL_FORMAT_8888,
						 PSP_DISPLAY_SETBUF_NEXTFRAME);
}

unsigned int __attribute__ ((aligned(16))) list[262144];

extern void init_gu(void)
{
	void *vram = NULL;

	xrGuInit();

	xrGuStart(GU_DIRECT, list);
	xrGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, vram, PSP_SCREEN_SCANLINE);
	vram += PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES;
	xrGuDrawBuffer(GU_PSM_8888, vram, PSP_SCREEN_SCANLINE);
	vram += PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES * 2;
	xrGuDepthBuffer(vram, PSP_SCREEN_SCANLINE);
	xrGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
	xrGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	xrGuDepthRange(65535, 0);
	xrGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	xrGuEnable(GU_SCISSOR_TEST);
	xrGuFrontFace(GU_CW);
	xrGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	xrGuEnable(GU_TEXTURE_2D);
	xrGuFinish();
	xrGuSync(0, 0);

	xrDisplayWaitVblankStart();
	xrGuDisplay(1);
}

extern void disp_putpixel(int x, int y, pixel color)
{
	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}
	*(pixel *) disp_get_vaddr((x), (y)) = (color);
}

extern void disp_set_fontsize(int fontsize)
{
	if (!using_ttf)
		memset(disp_ewidth, 0, 0x80);
	DISP_FONTSIZE = fontsize;
	if (fontsize <= 16) {
		DISP_CROWSIZE = 2;
		DISP_EROWSIZE = 1;
		fbits_last = (1 << (16 - fontsize)) / 2;
		febits_last = (1 << (8 - fontsize / 2)) / 2;
	} else if (fontsize <= 24) {
		DISP_CROWSIZE = 3;
		DISP_EROWSIZE = 2;
		fbits_last = (1 << (24 - fontsize)) / 2;
		febits_last = (1 << (16 - fontsize / 2)) / 2;
	} else {
		DISP_CROWSIZE = 4;
		DISP_EROWSIZE = 2;
		fbits_last = (1 << (32 - fontsize)) / 2;
		febits_last = (1 << (16 - fontsize / 2)) / 2;
	}
	DISP_CFONTSIZE = DISP_FONTSIZE * DISP_CROWSIZE;
	DISP_EFONTSIZE = DISP_FONTSIZE * DISP_EROWSIZE;
	HRR = 100 / DISP_FONTSIZE;
	WRR = config.filelistwidth / DISP_FONTSIZE;

	extern int MAX_ITEM_NAME_LEN;

	MAX_ITEM_NAME_LEN = WRR * 4 - 1;
}

extern void disp_set_book_fontsize(int fontsize)
{
	DISP_BOOK_FONTSIZE = fontsize;
	if (fontsize <= 16) {
		DISP_BOOK_CROWSIZE = 2;
		DISP_BOOK_EROWSIZE = 1;
		fbits_book_last = (1 << (16 - fontsize)) / 2;
		febits_book_last = (1 << (8 - fontsize / 2)) / 2;
	} else if (fontsize <= 24) {
		DISP_BOOK_CROWSIZE = 3;
		DISP_BOOK_EROWSIZE = 2;
		fbits_book_last = (1 << (24 - fontsize)) / 2;
		febits_book_last = (1 << (16 - fontsize / 2)) / 2;
	} else {
		DISP_BOOK_CROWSIZE = 4;
		DISP_BOOK_EROWSIZE = 2;
		fbits_book_last = (1 << (32 - fontsize)) / 2;
		febits_book_last = (1 << (16 - fontsize / 2)) / 2;
	}
	DISP_BOOK_CFONTSIZE = DISP_BOOK_FONTSIZE * DISP_BOOK_CROWSIZE;
	DISP_BOOK_EFONTSIZE = DISP_BOOK_FONTSIZE * DISP_BOOK_EROWSIZE;

	// if set font size to very small one, set config.wordspace to 1
	if (using_ttf == false && fontsize <= 10 && config.wordspace == 0) {
		config.wordspace = 1;
		auto_inc_wordspace_on_small_font = true;
	}
	// if previous have auto increased wordspace on small font, restore config.wordspace to 0
	if (using_ttf == false && fontsize >= 12
		&& auto_inc_wordspace_on_small_font && config.wordspace == 1) {
		config.wordspace = 0;
		auto_inc_wordspace_on_small_font = false;
	}
}

extern bool disp_has_zipped_font(const char *zipfile, const char *efont,
								 const char *cfont)
{
	unzFile unzf = unzOpen(zipfile);

	if (unzf == NULL)
		return false;

	if (unzLocateFile(unzf, efont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	unzCloseCurrentFile(unzf);

	if (unzLocateFile(unzf, cfont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	unzCloseCurrentFile(unzf);

	unzClose(unzf);
	return true;
}

extern bool disp_has_font(const char *efont, const char *cfont)
{
	int fd = xrIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	xrIoClose(fd);

	fd = xrIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return false;
	xrIoClose(fd);
	return true;
}

#ifdef ENABLE_TTF
static void ttf_clear_cache(p_ttf ttf)
{
	size_t i;

	for (i = 0; i < SBIT_HASH_SIZE; ++i) {
		memset(&ttf->sbitHashRoot[i], 0, sizeof(SBit_HashItem));
	}
}

static void clean_ttf_cache(p_ttf ttf)
{
	size_t i;

	for (i = 0; i < SBIT_HASH_SIZE; ++i) {
		if (ttf->sbitHashRoot[i].bitmap.buffer) {
			free(ttf->sbitHashRoot[i].bitmap.buffer);
			ttf->sbitHashRoot[i].bitmap.buffer = NULL;
		}
	}
}
#endif

#ifdef ENABLE_TTF
extern void disp_ttf_close(void)
{
	if (ettf != NULL) {
		if (g_ttf_share_buffer) {
			ettf->fileBuffer = NULL;
			g_ttf_share_buffer = false;
		}

		ttf_close(ettf);
		ettf = NULL;
	}

	if (cttf != NULL) {
		ttf_close(cttf);
		cttf = NULL;
	}

	if (cttfinfo != NULL) {
		clean_ttf_cache(cttfinfo);
		free(cttfinfo);
		cttfinfo = NULL;
	}

	if (ettfinfo != NULL) {
		clean_ttf_cache(ettfinfo);
		free(ettfinfo);
		ettfinfo = NULL;
	}

	using_ttf = false;
}
#endif

extern void disp_assign_book_font(void)
{
	using_ttf = false;
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free(book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free(book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
#ifdef ENABLE_TTF
	disp_ttf_close();
#endif
	book_efont_buffer = efont_buffer;
	book_cfont_buffer = cfont_buffer;
}

extern bool disp_load_zipped_font(const char *zipfile, const char *efont,
								  const char *cfont)
{
	disp_free_font();
	unzFile unzf = unzOpen(zipfile);
	unz_file_info info;
	dword size;

	if (unzf == NULL)
		return false;

	if (unzLocateFile(unzf, efont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	if (unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) {
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if ((efont_buffer = malloc(size)) == NULL) {
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, efont_buffer, size);
	unzCloseCurrentFile(unzf);
	book_efont_buffer = efont_buffer;

	if (unzLocateFile(unzf, cfont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	if (unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) {
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if ((cfont_buffer = malloc(size)) == NULL) {
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, cfont_buffer, size);
	unzCloseCurrentFile(unzf);
	book_cfont_buffer = cfont_buffer;

	unzClose(unzf);

	return true;
}

#ifdef ENABLE_TTF
static p_ttf load_archieve_truetype_book_font(const char *zipfile,
											  const char *zippath, int size,
											  bool cjkmode)
{
	p_ttf ttf = NULL;

	if (ttf == NULL && zipfile[0] != '\0') {
		buffer *b = NULL;

		extract_archive_file_into_buffer(&b, zipfile, zippath,
										 fs_file_get_type(zipfile));

		if (b == NULL) {
			return false;
		}

		if ((ttf =
			 ttf_open_buffer(b->ptr, b->used, size, zippath,
							 cjkmode)) == NULL) {
			buffer_free_weak(b);
			return NULL;
		}
		buffer_free_weak(b);
	} else {
		ttf_set_pixel_size(ttf, size);
	}

	using_ttf = true;

	if (ttf) {
		ttf->cjkmode = cjkmode;
	}

	return ttf;
}
#endif

extern bool disp_ttf_reload(int size)
{
	dbg_printf(d, "%s", __func__);

#ifdef ENABLE_TTF
	static char prev_ettfpath[PATH_MAX] = "", prev_ettfarch[PATH_MAX] = "";
	static char prev_cttfpath[PATH_MAX] = "", prev_cttfarch[PATH_MAX] = "";

	using_ttf = false;
	memset(disp_ewidth, size / 2, 0x80);

	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free(book_efont_buffer);
		book_efont_buffer = NULL;
	}

	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free(book_cfont_buffer);
		book_cfont_buffer = NULL;
	}

	if (cttf != NULL && strcmp(prev_cttfarch, config.cttfarch) == 0
		&& strcmp(prev_cttfpath, config.cttfpath) == 0) {
		ttf_set_pixel_size(cttf, size);
	} else {
		if (cttf != NULL) {
			if (g_ttf_share_buffer) {
				cttf->fileBuffer = NULL;
				g_ttf_share_buffer = false;
			}

			ttf_close(cttf);
			cttf = NULL;
		}

		if (config.cttfarch[0] != '\0') {
			cttf =
				load_archieve_truetype_book_font(config.cttfarch,
												 config.cttfpath,
												 config.bookfontsize, true);
		} else {
			cttf =
				ttf_open(config.cttfpath, config.bookfontsize,
						 config.ttf_load_to_memory, true);
		}
	}

	if (ettf != NULL && strcmp(prev_ettfarch, config.ettfarch) == 0
		&& strcmp(prev_ettfpath, config.ettfpath) == 0) {
		ttf_set_pixel_size(ettf, size);
	} else {
		if (ettf != NULL) {
			if (g_ttf_share_buffer) {
				ettf->fileBuffer = NULL;
				g_ttf_share_buffer = false;
			}

			ttf_close(ettf);
			ettf = NULL;
		}

		if (!strcmp(config.ettfarch, config.cttfarch)
			&& !strcmp(config.ettfpath, config.cttfpath)) {
			if (cttf != NULL && cttf->fileBuffer != NULL) {
				ettf =
					ttf_open_buffer(cttf->fileBuffer, cttf->fileSize, size,
									cttf->fontName, false);

				if (ettf) {
					g_ttf_share_buffer = true;
				}
			} else {
				ettf =
					ttf_open(config.ettfpath, config.bookfontsize,
							 config.ttf_load_to_memory, false);
			}
		} else if (config.ettfarch[0] != '\0') {
			ettf =
				load_archieve_truetype_book_font(config.ettfarch,
												 config.ettfpath,
												 config.bookfontsize, false);
		} else {
			ettf =
				ttf_open(config.ettfpath, config.bookfontsize,
						 config.ttf_load_to_memory, false);
		}
	}

	if (cttf == NULL && ettf == NULL)
		return false;

	if (cttf == NULL) {
		STRCPY_S(config.cttfarch, config.ettfarch);
		STRCPY_S(config.cttfpath, config.ettfpath);

		if (ettf != NULL && ettf->fileBuffer != NULL) {
			cttf =
				ttf_open_buffer(ettf->fileBuffer, ettf->fileSize, size,
								ettf->fontName, true);

			if (cttf) {
				g_ttf_share_buffer = true;
			}
		} else {
			cttf =
				ttf_open(config.cttfpath, config.bookfontsize,
						 config.ttf_load_to_memory, true);
		}
	}

	if (ettf == NULL) {
		STRCPY_S(config.ettfarch, config.cttfarch);
		STRCPY_S(config.ettfpath, config.cttfpath);

		if (cttf != NULL && cttf->fileBuffer != NULL) {
			ettf =
				ttf_open_buffer(cttf->fileBuffer, cttf->fileSize, size,
								cttf->fontName, false);

			if (ettf) {
				g_ttf_share_buffer = true;
			}
		} else {
			ettf =
				ttf_open(config.ettfpath, config.bookfontsize,
						 config.ttf_load_to_memory, false);
		}
	}

	if (cttf == NULL && ettf == NULL)
		return false;

	if (cttf == NULL) {
		cttf =
			ttf_open_buffer(ettf->fileBuffer, ettf->fileSize, size,
					ettf->fontName, true);

		if (cttf) {
			g_ttf_share_two_font = true;
		}
	}

	if (ettf == NULL) {
		ettf =
			ttf_open_buffer(cttf->fileBuffer, cttf->fileSize, size,
					cttf->fontName, false);

		if (ettf) {
			g_ttf_share_two_font = true;
		}
	}

	if (cttf == NULL || ettf == NULL)
		return false;

	if (cttfinfo != NULL) {
		clean_ttf_cache(cttfinfo);
		free(cttfinfo);
		cttfinfo = NULL;
	}

	cttfinfo = malloc(sizeof(*cttfinfo));

	if (cttfinfo != NULL) {
		memcpy(cttfinfo, cttf, sizeof(*cttfinfo));
		cttfinfo->config.embolden = 0;
		cttfinfo->pixelSize = cttfinfo->config.pixelsize =
			config.infobar_fontsize;
	}

	ttf_clear_cache(cttfinfo);

	if (ettfinfo != NULL) {
		clean_ttf_cache(ettfinfo);
		free(ettfinfo);
		ettfinfo = NULL;
	}

	ettfinfo = malloc(sizeof(*ettfinfo));

	if (ettfinfo != NULL) {
		memcpy(ettfinfo, ettf, sizeof(*ettfinfo));
		ettfinfo->config.embolden = 0;
		ettfinfo->pixelSize = ettfinfo->config.pixelsize =
			config.infobar_fontsize;
	}

	ttf_clear_cache(ettfinfo);

	ttf_load_ewidth(ettf, disp_ewidth, 0x80);

	STRCPY_S(prev_cttfarch, config.cttfarch);
	STRCPY_S(prev_cttfpath, config.cttfpath);
	STRCPY_S(prev_ettfarch, config.ettfarch);
	STRCPY_S(prev_ettfpath, config.ettfpath);

	using_ttf = true;
	return true;
#else
	return false;
#endif
}

extern bool disp_load_font(const char *efont, const char *cfont)
{
	disp_free_font();
	int size;
	int fd = xrIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	size = xrIoLseek32(fd, 0, PSP_SEEK_END);
	if ((efont_buffer = calloc(1, size)) == NULL) {
		xrIoClose(fd);
		return false;
	}
	xrIoLseek32(fd, 0, PSP_SEEK_SET);
	xrIoRead(fd, efont_buffer, size);
	xrIoClose(fd);
	book_efont_buffer = efont_buffer;

	fd = xrIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		disp_free_font();
		return false;
	}
	size = xrIoLseek32(fd, 0, PSP_SEEK_END);
	if ((cfont_buffer = calloc(1, size)) == NULL) {
		disp_free_font();
		xrIoClose(fd);
		return false;
	}
	xrIoLseek32(fd, 0, PSP_SEEK_SET);
	xrIoRead(fd, cfont_buffer, size);
	xrIoClose(fd);
	book_cfont_buffer = cfont_buffer;

	return true;
}

extern bool disp_load_zipped_book_font(const char *zipfile, const char *efont,
									   const char *cfont)
{
	using_ttf = false;
#ifdef ENABLE_TTF
	disp_ttf_close();
#endif
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free(book_efont_buffer);
		book_efont_buffer = NULL;
	}
	unzFile unzf = unzOpen(zipfile);
	unz_file_info info;
	dword size;

	if (unzf == NULL)
		return false;

	if (unzLocateFile(unzf, efont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	if (unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) {
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if ((book_efont_buffer = calloc(1, size)) == NULL) {
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, book_efont_buffer, size);
	unzCloseCurrentFile(unzf);

	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free(book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if (unzLocateFile(unzf, cfont, 0) != UNZ_OK
		|| unzOpenCurrentFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		return false;
	}
	if (unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) {
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if ((book_cfont_buffer = calloc(1, size)) == NULL) {
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, book_cfont_buffer, size);
	unzCloseCurrentFile(unzf);

	unzClose(unzf);
	return true;
}

extern bool disp_load_book_font(const char *efont, const char *cfont)
{
	using_ttf = false;
#ifdef ENABLE_TTF
	disp_ttf_close();
#endif
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free(book_efont_buffer);
		book_efont_buffer = NULL;
	}
	int size;
	int fd = xrIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	size = xrIoLseek32(fd, 0, PSP_SEEK_END);
	if ((book_efont_buffer = calloc(1, size)) == NULL) {
		xrIoClose(fd);
		return false;
	}
	xrIoLseek32(fd, 0, PSP_SEEK_SET);
	xrIoRead(fd, book_efont_buffer, size);
	xrIoClose(fd);

	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free(book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	fd = xrIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		disp_free_font();
		return false;
	}
	size = xrIoLseek32(fd, 0, PSP_SEEK_END);
	if ((book_cfont_buffer = calloc(1, size)) == NULL) {
		disp_free_font();
		xrIoClose(fd);
		return false;
	}
	xrIoLseek32(fd, 0, PSP_SEEK_SET);
	xrIoRead(fd, book_cfont_buffer, size);
	xrIoClose(fd);

	return true;
}

extern void disp_free_font(void)
{
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free(book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free(book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if (efont_buffer != NULL) {
		free(efont_buffer);
		efont_buffer = NULL;
	}
	if (cfont_buffer != NULL) {
		free(cfont_buffer);
		cfont_buffer = NULL;
	}
#ifdef ENABLE_TTF
	memset(disp_ewidth, 0, 0x80);
	disp_ttf_close();
#endif
}

void *framebuffer = 0;

extern void disp_flip(void)
{
	vram_page ^= 1;
	vram_disp =
		(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
	vram_draw =
		(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	disp_waitv();
	xrDisplaySetFrameBuf(vram_disp, 512, PSP_DISPLAY_PIXEL_FORMAT_8888,
						 PSP_DISPLAY_SETBUF_IMMEDIATE);
	framebuffer = xrGuSwapBuffers();
}

extern void disp_getimage_draw(dword x, dword y, dword w, dword h, pixel * buf)
{
	pixel *lines = disp_get_vaddr(x, y), *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h) << 9);
	dword rw = min(512 - x, w) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(buf, lines, rw);
		buf += w;
	}
}

extern void disp_getimage(dword x, dword y, dword w, dword h, pixel * buf)
{
	pixel *lines =
		(vram_disp + (x) + ((y) << 9)) + 0x40000000 / PIXEL_BYTES, *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h) << 9);
	dword rw = min(512 - x, w) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(buf, lines, rw);
		buf += w;
	}
}

/**
 * x: x ����
 * y: y ����
 * w: ͼ���
 * h: ͼ�߶�
 * bufw: ��ͼ�߿��
 * startx: ��ʼx����
 * starty: ��ʼy���� 
 * ow: ?
 * oh: ?
 * buf: ��ͼ����
 * swizzled: �Ƿ�ΪΪ����ͼ
 */
extern void disp_newputimage(int x, int y, int w, int h, int bufw, int startx,
							 int starty, int ow, int oh, pixel * buf,
							 bool swizzled)
{
	xrGuStart(GU_DIRECT, list);
	xrGuTexFilter(GU_LINEAR, GU_LINEAR);
	xrGuShadeModel(GU_SMOOTH);
	xrGuAmbientColor(0xFFFFFFFF);
	Vertex *vertices = (Vertex *) xrGuGetMemory(2 * sizeof(Vertex));

	xrGuTexMode(GU_PSM_8888, 0, 0, swizzled ? 1 : 0);
	xrGuTexImage(0, 512, 512, bufw, buf);
	xrGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	xrGuTexFilter(GU_LINEAR, GU_LINEAR);
	vertices[0].u = startx;
	vertices[0].v = starty;
	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = 0;
	vertices[0].color = 0;
	vertices[1].u = startx + ((ow == 0) ? w : ow);
	vertices[1].v = starty + ((oh == 0) ? h : oh);
	vertices[1].x = x + w;
	vertices[1].y = y + h;
	vertices[1].z = 0;
	vertices[1].color = 0;
	xrGuDrawArray(GU_SPRITES,
				  GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT |
				  GU_TRANSFORM_2D, 2, 0, vertices);
	xrGuFinish();
	xrGuSync(0, 0);
}

/**
 * ����ͼ����Ļ
 * @param x Ŀ��x�����ַ
 * @param y Ŀ��y�����ַ
 * @param w ����ͼ��Ŀ��
 * @param h ����ͼ��ĸ߶�
 * @param startx ����ͼ���Դx�����ַ
 * @param starty ����ͼ���Դy�����ַ
 * @param buf ͼ������
 * @note TODO should use GE to copy image
 */
extern void disp_putimage(dword x, dword y, dword w, dword h, dword startx,
						  dword starty, pixel * buf)
{
	if (x < 0) {
		w += x;
		startx -= x;
		x = 0;
		if (w < 0)
			return;
	} else if (x >= PSP_SCREEN_WIDTH) {
		return;
	}
	if (y < 0) {
		h += y;
		starty -= y;
		y = 0;
		if (h < 0)
			return;
	} else if (y >= PSP_SCREEN_HEIGHT) {
		return;
	}

	pixel *lines = disp_get_vaddr(x, y), *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h - starty) << 9);
	buf = buf + starty * w + startx;
	dword rw = min(512 - x, w - startx) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(lines, buf, rw);
		buf += w;
	}
}

extern void disp_duptocache(void)
{
	memmove(vram_draw, ((byte *) vram_disp) + 0x40000000,
			512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
}

extern void disp_duptocachealpha(int percent)
{
	pixel *vsrc = (pixel *) (((byte *) vram_disp) + 0x40000000), *vdest =
		vram_draw;
	int i, j;

	for (i = 0; i < PSP_SCREEN_HEIGHT; i++) {
		pixel *vput = vdest, *vget = vsrc;

		for (j = 0; j < PSP_SCREEN_WIDTH; j++) {
			*vput++ = disp_grayscale(*vget, 0, 0, 0, percent);
			vget++;
		}
		vsrc += 512;
		vdest += 512;
	}
}

/**
 * fix osk buffer mismatch by checking swap buffer
 */
extern void disp_fix_osk(void *buffer)
{
	if (buffer) {
		vram_page = 0;
		vram_disp =
			(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
		vram_draw =
			(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	} else {
		vram_page = 1;
		vram_disp =
			(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
		vram_draw =
			(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	}
	xrDisplaySetFrameBuf(vram_disp, 512, PSP_DISPLAY_PIXEL_FORMAT_8888,
						 PSP_DISPLAY_SETBUF_IMMEDIATE);
}

extern void disp_rectduptocache(dword x1, dword y1, dword x2, dword y2)
{
	pixel *lines = disp_get_vaddr(x1, y1), *linesend =
		disp_get_vaddr(x1, y2), *lined =
		vram_disp + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
	dword w = (x2 - x1 + 1) * PIXEL_BYTES;

	for (; lines <= linesend; lines += 512, lined += 512)
		memcpy(lines, lined, w);
}

extern void disp_rectduptocachealpha(dword x1, dword y1, dword x2, dword y2,
									 int percent)
{
	pixel *lines = disp_get_vaddr(x1, y1), *linesend =
		disp_get_vaddr(x1, y2), *lined =
		vram_disp + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
	dword w = x2 - x1 + 1;

	for (; lines <= linesend; lines += 512, lined += 512) {
		pixel *vput = lines, *vget = lined;
		dword i;

		for (i = 0; i < w; i++) {
			*vput++ = disp_grayscale(*vget, 0, 0, 0, percent);
			vget++;
		}
	}
}

bool check_range(int x, int y)
{
	return x >= 0 && x < PSP_SCREEN_WIDTH && y >= 0 && y < PSP_SCREEN_HEIGHT;
}

enum
{
	HORZ,
	LVERT,
	RVERT,
	REVERSAL
};

static inline void next_col(int direction, pixel ** vpoint)
{
	switch (direction) {
		case HORZ:
			(*vpoint)++;
			break;
		case LVERT:
			(*vpoint) -= 512;
			break;
		case RVERT:
			(*vpoint) += 512;
			break;
		case REVERSAL:
			(*vpoint)--;
			break;
	}
}

static inline void next_row(int direction, pixel ** vaddr)
{
	switch (direction) {
		case HORZ:
			(*vaddr) += 512;
			break;
		case LVERT:
			(*vaddr)++;
			break;
		case RVERT:
			(*vaddr)--;
			break;
		case REVERSAL:
			(*vaddr) -= 512;
			break;
	}
}

typedef struct
{
	dword x;
	dword y;
	int top;
	int height;
	const byte *str;
	pixel color;
	dword count;
	dword wordspace;
	bool is_system;
	int direction;
} disp_draw_string_inf;

static inline int putnstring_hanzi(disp_draw_string_inf * inf)
{
	if (inf == NULL)
		return 0;
	if (!check_range(inf->x, inf->y))
		return 0;
	pixel *vaddr = disp_get_vaddr(inf->x, inf->y);
	const byte *ccur, *cend;

	if (inf->is_system)
		ccur =
			cfont_buffer + (((dword) (*inf->str - 0x81)) * 0xBF +
							((dword) (*(inf->str + 1) - 0x40))) *
			DISP_CFONTSIZE + inf->top * DISP_CROWSIZE;
	else
		ccur =
			book_cfont_buffer + (((dword) (*inf->str - 0x81)) * 0xBF +
								 ((dword) (*(inf->str + 1) - 0x40))) *
			DISP_BOOK_CFONTSIZE + inf->top * DISP_BOOK_CROWSIZE;

	if (inf->is_system)
		cend = ccur + inf->height * DISP_CROWSIZE;
	else
		cend = ccur + inf->height * DISP_BOOK_CROWSIZE;
	for (; ccur < cend; ccur++) {
		int b;
		pixel *vpoint = vaddr;
		int bitsleft;

		if (inf->is_system)
			bitsleft = DISP_FONTSIZE - 8;
		else
			bitsleft = DISP_BOOK_FONTSIZE - 8;

		while (bitsleft > 0) {
			for (b = 0x80; b > 0; b >>= 1) {
				if (((*ccur) & b) != 0)
					*vpoint = inf->color;
				next_col(inf->direction, &vpoint);
			}
			++ccur;
			bitsleft -= 8;
		}
		int t = inf->is_system ? fbits_last : fbits_book_last;

		for (b = 0x80; b > t; b >>= 1) {
			if (((*ccur) & b) != 0)
				*vpoint = inf->color;
			next_col(inf->direction, &vpoint);
		}
		next_row(inf->direction, &vaddr);
	}
	switch (inf->direction) {
		case LVERT:
			vaddr++;
			break;
		case RVERT:
			vaddr--;
			break;
	}

	inf->str += 2;
	inf->count -= 2;

	int d =
		inf->is_system ? DISP_FONTSIZE +
		inf->wordspace * 2 : DISP_BOOK_FONTSIZE + inf->wordspace * 2;

	switch (inf->direction) {
		case HORZ:
			inf->x += d;
			break;
		case REVERSAL:
			inf->x -= d;
			break;
		case LVERT:
			inf->y -= d;
			break;
		case RVERT:
			inf->y += d;
			break;
	}

	return 1;
}

static inline int putnstring_ascii(disp_draw_string_inf * inf)
{
	if (!check_range(inf->x, inf->y))
		return 0;
	pixel *vaddr = disp_get_vaddr(inf->x, inf->y);
	const byte *ccur, *cend;

	if (inf->is_system)
		ccur =
			efont_buffer + ((dword) * inf->str) * DISP_EFONTSIZE +
			inf->top * DISP_EROWSIZE;
	else
		ccur =
			book_efont_buffer + ((dword) * inf->str) * DISP_BOOK_EFONTSIZE +
			inf->top * DISP_BOOK_EROWSIZE;

	if (inf->is_system)
		cend = ccur + inf->height * DISP_EROWSIZE;
	else
		cend = ccur + inf->height * DISP_BOOK_EROWSIZE;
	for (; ccur < cend; ccur++) {
		int b;
		pixel *vpoint = vaddr;
		int bitsleft;

		if (inf->is_system)
			bitsleft = DISP_FONTSIZE / 2 - 8;
		else
			bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;

		while (bitsleft > 0) {
			for (b = 0x80; b > 0; b >>= 1) {
				if (((*ccur) & b) != 0)
					*vpoint = inf->color;
				next_col(inf->direction, &vpoint);
			}
			++ccur;
			bitsleft -= 8;
		}
		int t = inf->is_system ? febits_last : febits_book_last;

		for (b = 0x80; b > t; b >>= 1) {
			if (((*ccur) & b) != 0)
				*vpoint = inf->color;
			next_col(inf->direction, &vpoint);
		}
		next_row(inf->direction, &vaddr);
	}
	switch (inf->direction) {
		case LVERT:
			vaddr++;
			break;
		case RVERT:
			vaddr--;
			break;
	}

	inf->str++;
	inf->count--;

	int d =
		inf->is_system ? DISP_FONTSIZE / 2 +
		inf->wordspace : DISP_BOOK_FONTSIZE / 2 + inf->wordspace;

	switch (inf->direction) {
		case HORZ:
			inf->x += d;
			break;
		case REVERSAL:
			inf->x -= d;
			break;
		case LVERT:
			inf->y -= d;
			break;
		case RVERT:
			inf->y += d;
			break;
	}

	return 1;
}

static inline void disp_to_draw_string_inf(disp_draw_string_inf * inf, int x,
										   int y, pixel color, const byte * str,
										   int count, dword wordspace, int top,
										   int height, bool is_system,
										   int direction)
{
	inf->x = x;
	inf->y = y;
	inf->color = color;
	inf->str = str;
	inf->count = count;
	inf->wordspace = wordspace;
	inf->top = top;
	inf->height = height;
	inf->is_system = is_system;
	inf->direction = direction;
}

static inline void disp_from_draw_string_inf(disp_draw_string_inf * inf, int *x,
											 int *y, pixel * color,
											 const byte ** str, int *count,
											 dword * wordspace, int *top,
											 int *height, int *direction)
{
	if (x)
		*x = inf->x;
	if (y)
		*y = inf->y;
	if (color)
		*color = inf->color;
	if (str)
		*str = inf->str;
	if (count)
		*count = inf->count;
	if (wordspace)
		*wordspace = inf->wordspace;
	if (top)
		*top = inf->top;
	if (height)
		*height = inf->height;
	if (direction)
		*direction = inf->direction;
}

extern void disp_putnstring(int x, int y, pixel color, const byte * str,
							int count, dword wordspace, int top, int height,
							int bot)
{
	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE) {
				break;
			}

			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, HORZ);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, HORZ);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x += DISP_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
}

extern void disp_putnstringreversal_sys(int x, int y, pixel color,
										const byte * str, int count,
										dword wordspace, int top, int height,
										int bot)
{
	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	if (x < 0 || y < 0)
		return;

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x < 0) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, REVERSAL);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (x < 0) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, REVERSAL);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (x < 0) {
				break;
			}
			if (!check_range(x, y))
				return;
			str++;
			count--;
			x -= DISP_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringreversal(int x, int y, pixel color, const byte * str,
									int count, dword wordspace, int top,
									int height, int bot)
{
	pixel *vaddr;
	const byte *ccur, *cend;

#ifdef ENABLE_TTF
	if (using_ttf) {
		disp_putnstring_reversal_truetype(cttf, ettf, x, y, color, str,
										  count, wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	if (x < 0 || y < 0)
		return;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x < 0) {
				break;
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);
			dword pos =
				(((dword) (*str - 0x81)) * 0xBF +
				 ((dword) (*(str + 1) - 0x40)));
			ccur =
				book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE +
				top * DISP_BOOK_CROWSIZE;

			for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur++) {
				int b;
				pixel *vpoint = vaddr;
				int bitsleft = DISP_BOOK_FONTSIZE - 8;

				while (bitsleft > 0) {
					for (b = 0x80; b > 0; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint--;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint--;
				}
				vaddr -= 512;
			}
			str += 2;
			count -= 2;
			x -= DISP_BOOK_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (x < 0) {
				break;
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					book_efont_buffer +
					((dword) * str) * DISP_BOOK_EFONTSIZE +
					top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE;
					 ccur < cend; ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint--;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint--;
					}
					vaddr -= 512;
				}
				x -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (x < 0) {
				break;
			}
			if (!check_range(x, y))
				return;
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
}

extern void disp_putnstringhorz_sys(int x, int y, pixel color, const byte * str,
									int count, dword wordspace, int top,
									int height, int bot)
{
	pixel *vaddr;
	const byte *ccur, *cend;

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE) {
				break;
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);
			dword pos =
				(((dword) (*str - 0x81)) * 0xBF +
				 ((dword) (*(str + 1) - 0x40)));
			ccur = cfont_buffer + pos * DISP_CFONTSIZE + top * DISP_CROWSIZE;

			for (cend = ccur + height * DISP_CROWSIZE; ccur < cend; ccur++) {
				int b;
				pixel *vpoint = vaddr;
				int bitsleft = DISP_FONTSIZE - 8;

				while (bitsleft > 0) {
					for (b = 0x80; b > 0; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint++;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint++;
				}
				vaddr += 512;
			}
			str += 2;
			count -= 2;
			x += DISP_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					efont_buffer +
					((dword) * str) * DISP_EFONTSIZE + top * DISP_EROWSIZE;
				for (cend = ccur + height * DISP_EROWSIZE; ccur < cend; ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint++;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint++;
					}
					vaddr += 512;
				}
				x += DISP_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			str++;
			count--;
			x += DISP_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringhorz(int x, int y, pixel color, const byte * str,
								int count, dword wordspace, int top, int height,
								int bot)
{
#ifdef ENABLE_TTF
	if (using_ttf) {
		disp_putnstring_horz_truetype(cttf, ettf, x, y, color, str,
									  count, wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, HORZ);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, HORZ);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x += DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
}

extern void disp_putnstringlvert_sys(int x, int y, pixel color,
									 const byte * str, int count,
									 dword wordspace, int top, int height,
									 int bot)
{
	if (bot) {
		if (x >= bot)
			return;
		if (x + height > bot)
			height = bot - x;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y < DISP_BOOK_FONTSIZE - 1) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, LVERT);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (y < DISP_BOOK_FONTSIZE / 2 - 1) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, LVERT);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (y < DISP_RSPAN + DISP_FONTSIZE - 1) {
				break;
			}
			if (!check_range(x, y))
				return;
			str++;
			count--;
			y -= DISP_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringlvert(int x, int y, pixel color, const byte * str,
								 int count, dword wordspace, int top,
								 int height, int bot)
{
#ifdef ENABLE_TTF
	if (using_ttf) {
		disp_putnstring_lvert_truetype(cttf, ettf, x, y, color, str,
									   count, wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (x >= bot)
			return;
		if (x + height > bot)
			height = bot - x;
	}

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y < DISP_BOOK_FONTSIZE - 1) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, LVERT);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (y < DISP_BOOK_FONTSIZE / 2 - 1) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, LVERT);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (y < DISP_BOOK_FONTSIZE / 2 - 1) {
				break;
			}
			if (!check_range(x, y))
				return;
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				y -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
}

extern void disp_putnstringrvert_sys(int x, int y, pixel color,
									 const byte * str, int count,
									 dword wordspace, int top, int height,
									 int bot)
{
	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	if (x < bot)
		return;
	if (x + 1 - height < bot)
		height = x + 1 - bot;

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, RVERT);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, true, RVERT);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			str++;
			count--;
			y += DISP_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringrvert(int x, int y, pixel color, const byte * str,
								 int count, dword wordspace, int top,
								 int height, int bot)
{
#ifdef ENABLE_TTF
	if (using_ttf) {
		disp_putnstring_rvert_truetype(cttf, ettf, x, y, color, str,
									   count, wordspace, top, height, bot);
		return;
	}
#endif

	if (str == NULL) {
		dbg_printf(d, "%s: %d/%d output null string", __func__, x, y);
		return;
	}

	if (x < 0 || x >= PSP_SCREEN_WIDTH || y < 0 || y >= PSP_SCREEN_HEIGHT) {
		dbg_printf(d, "%s: axis out of screen %d %d", __func__, x, y);
		return;
	}

	if (x < bot)
		return;
	if (x + 1 - height < bot)
		height = x + 1 - bot;

	disp_draw_string_inf inf;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, RVERT);
			if (putnstring_hanzi(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else if (*str > 0x1F) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
			}
			disp_to_draw_string_inf(&inf, x, y, color, str, count, wordspace,
									top, height, false, RVERT);
			if (putnstring_ascii(&inf) == 0) {
				return;
			}
			disp_from_draw_string_inf(&inf, &x, &y, &color, &str, &count,
									  &wordspace, &top, &height, NULL);
		} else {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
			}
			if (!check_range(x, y))
				return;
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				y += DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
}

extern void disp_fillvram(pixel color)
{
	xrGuStart(GU_DIRECT, list);
	xrGuClearColor(color);
	xrGuClear(GU_COLOR_BUFFER_BIT);
	xrGuFinish();
	xrGuSync(0, 0);
}

extern void disp_fillrect(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	xrGuStart(GU_DIRECT, list);
	VertexColor *vertices = xrGuGetMemory(2 * sizeof(*vertices));

	setVertex(&vertices[0], x1, y1, 0, color);
	setVertex(&vertices[1], x2 + 1, y2 + 1, 0, color);

	xrGuDisable(GU_TEXTURE_2D);
	xrGuDrawArray(GU_SPRITES,
				  GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0,
				  vertices);
	xrGuEnable(GU_TEXTURE_2D);

	xrGuFinish();
	xrGuSync(0, 0);
}

extern void disp_rectangle(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	xrGuStart(GU_DIRECT, list);
	VertexColor *vertices = xrGuGetMemory(5 * sizeof(*vertices));

	setVertex(&vertices[0], x1, y1, 0, color);
	setVertex(&vertices[1], x2, y1, 0, color);
	setVertex(&vertices[2], x2, y2, 0, color);
	setVertex(&vertices[3], x1, y2, 0, color);
	setVertex(&vertices[4], x1, y1, 0, color);

	xrGuDisable(GU_TEXTURE_2D);
	xrGuDrawArray(GU_LINE_STRIP,
				  GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 5, 0,
				  vertices);
	xrGuEnable(GU_TEXTURE_2D);

	xrGuFinish();
	xrGuSync(0, 0);
}

extern void disp_line(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	xrGuStart(GU_DIRECT, list);
	VertexColor *vertices = xrGuGetMemory(2 * sizeof(*vertices));

	setVertex(&vertices[0], x1, y1, 0, color);
	setVertex(&vertices[1], x2, y2, 0, color);

	xrGuDisable(GU_TEXTURE_2D);
	xrGuDrawArray(GU_LINES,
				  GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0,
				  vertices);
	xrGuEnable(GU_TEXTURE_2D);

	xrGuFinish();
	xrGuSync(0, 0);
}

pixel *disp_swizzle_image(pixel * buf, int width, int height)
{
	int pitch = (PIXEL_BYTES * width + 15) & ~0xF;

	pixel *out;

	if ((out = (pixel *) memalign(16, pitch * ((height + 7) & ~0x7))) == NULL)
		return out;
	unsigned blockx, blocky;
	unsigned j;

	unsigned width_blocks = pitch / 16;
	unsigned height_blocks = (height + 7) / 8;

	unsigned src_pitch = (pitch - 16) / 4;
	unsigned src_row = pitch * 8;

	const byte *ysrc = (const byte *) buf;
	u32 *dst = (u32 *) out;

	for (blocky = 0; blocky < height_blocks; ++blocky) {
		const byte *xsrc = ysrc;

		for (blockx = 0; blockx < width_blocks; ++blockx) {
			const u32 *src = (u32 *) xsrc;

			for (j = 0; j < 8; ++j) {
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				src += src_pitch;
			}
			xsrc += 16;
		}
		ysrc += src_row;
	}

	return out;
}
