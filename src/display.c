/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

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
#include "dbg.h"

extern t_conf config;

static bool auto_inc_wordspace_on_small_font = false;
static pixel *vram_base = NULL;
pixel *vram_start = NULL;
static bool vram_page = 0;
static byte *cfont_buffer = NULL, *book_cfont_buffer = NULL, *efont_buffer =
	NULL, *book_efont_buffer = NULL;
int DISP_FONTSIZE = 16, DISP_BOOK_FONTSIZE = 16, HRR = 6, WRR = 15;
int use_ttf = 0;
static int DISP_EFONTSIZE, DISP_CFONTSIZE, DISP_CROWSIZE, DISP_EROWSIZE,
	fbits_last = 0, febits_last =
	0, DISP_BOOK_EFONTSIZE, DISP_BOOK_CFONTSIZE, DISP_BOOK_EROWSIZE,
	DISP_BOOK_CROWSIZE, fbits_book_last = 0, febits_book_last = 0;;
byte disp_ewidth[0x80];

#ifdef ENABLE_TTF
//static byte *cache = NULL;
//static void *ttfh = NULL;
p_ttf ettf = NULL, cttf = NULL;
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

#define DISP_RSPAN 0

#define CHECK_DISPLAY_BORDER

#ifdef CHECK_DISPLAY_BORDER
#define CHECK_AND_VALID(x, y) \
{\
	x = (x < 0) ? 0 : x; \
	y = (y < 0) ? 0 : y; \
	x = (x >= PSP_SCREEN_WIDTH )? PSP_SCREEN_WIDTH - 1: x;\
	y = (y >= PSP_SCREEN_HEIGHT )? PSP_SCREEN_HEIGHT - 1: y;\
}

#define CHECK_AND_VALID_4(x1, y1, x2, y2) \
{\
	CHECK_AND_VALID(x1, y1);\
	CHECK_AND_VALID(x2, y2);\
}

#define CHECK_AND_VALID_WH(x, y, w, h) \
{\
	CHECK_AND_VALID(x, y);\
	w = x + w > PSP_SCREEN_WIDTH ? PSP_SCREEN_WIDTH - x : w; \
	h = y + h > PSP_SCREEN_HEIGHT ? PSP_SCREEN_HEIGHT - y : h; \
}
#else
#define CHECK_AND_VALID(x, y)
#define CHECK_AND_VALID_4(x1, y1, x2, y2)
#define CHECK_AND_VALID_WH(x, y, w, h)
#endif

extern void disp_init()
{
	sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	vram_page = 0;
	vram_base = (pixel *) 0x04000000;
	vram_start = (pixel *) (0x44000000 + 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
#ifdef COLOR16BIT
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_5551,
						  PSP_DISPLAY_SETBUF_NEXTFRAME);
#else
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_NEXTFRAME);
#endif
}

unsigned int __attribute__ ((aligned(16))) list[262144];

#ifdef ENABLE_GE
extern void init_gu(void)
{
	sceGuInit();

	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888,
					(void *) 0 + 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES, 512);
	sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, (void *) 0, 512);
	sceGuDepthBuffer((void *) 0 + (u32) 4 * 512 * PSP_SCREEN_HEIGHT +
					 (u32) 2 * 512 * PSP_SCREEN_HEIGHT, 512);
	sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
	sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuDepthRange(65535, 0);
	sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}
#endif

extern void disp_putpixel(int x, int y, pixel color)
{
	CHECK_AND_VALID(x, y);
#ifndef ENABLE_GE
	*(pixel *) disp_get_vaddr((x), (y)) = (color);
#else
	sceGuStart(GU_DIRECT, list);
	VertexColor *vertices = (VertexColor *) sceGuGetMemory(sizeof(VertexColor));

	vertices[0].color = color;
	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = 0;
	sceGuDrawArray(GU_POINTS, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
				   1, 0, vertices);
	sceGuFinish();
	sceGuSync(0, 0);
#endif
}

extern void disp_set_fontsize(int fontsize)
{
	if (!use_ttf)
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
	if (use_ttf == false && fontsize <= 10 && config.wordspace == 0) {
		config.wordspace = 1;
		auto_inc_wordspace_on_small_font = true;
	}
	// if previous have auto increased wordspace on small font, restore config.wordspace to 0
	if (use_ttf == false && fontsize >= 12 && auto_inc_wordspace_on_small_font
		&& config.wordspace == 1) {
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
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	sceIoClose(fd);

	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return false;
	sceIoClose(fd);
	return true;
}

extern void disp_assign_book_font()
{
	use_ttf = 0;
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
#ifdef ENABLE_TTF
	if (ettf != NULL) {
		ttf_close(ettf);
		ettf = NULL;
	}
	if (cttf != NULL) {
		ttf_close(cttf);
		cttf = NULL;
	}
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
	if ((efont_buffer = (byte *) malloc(size)) == NULL) {
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
	if ((cfont_buffer = (byte *) malloc(size)) == NULL) {
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
static bool load_ttf_config(void)
{
	ttf_set_anti_alias(cttf, config.cfont_antialias);
	ttf_set_anti_alias(ettf, config.efont_antialias);
	ttf_set_cleartype(cttf, config.cfont_cleartype);
	ttf_set_cleartype(ettf, config.efont_cleartype);
	ttf_set_embolden(cttf, config.cfont_embolden);
	ttf_set_embolden(ettf, config.efont_embolden);

	return true;
}
#endif

extern bool disp_load_truetype_book_font(const char *ettffile,
										 const char *cttffile, int size)
{
#ifdef ENABLE_TTF
	use_ttf = 0;
	memset(disp_ewidth, size / 2, 0x80);
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if (ettf == NULL) {
		if ((ettf = ttf_open(ettffile, size)) == NULL) {
			return false;
		}
	} else {
		ttf_set_pixel_size(ettf, size);
	}
	if (cttf == NULL) {
		if ((cttf = ttf_open(cttffile, size)) == NULL) {
			return false;
		}
	} else {
		ttf_set_pixel_size(cttf, size);
	}

	load_ttf_config();

	ttf_load_ewidth(ettf, disp_ewidth, 0x80);
	use_ttf = 1;
	return true;
#else
	return false;
#endif
}

#ifdef ENABLE_TTF
static p_ttf load_archieve_truetype_book_font(const char *zipfile,
											  const char *zippath, int size)
{
	p_ttf ttf = NULL;

	if (ttf == NULL && zipfile[0] != '\0') {
		buffer *b = NULL;

		extract_archive_file_into_buffer(&b, zipfile, zippath,
										 fs_file_get_type(zipfile));

		if (b == NULL) {
			return false;
		}

		if ((ttf = ttf_open_buffer(b->ptr, b->used, size, zippath)) == NULL) {
			buffer_free_weak(b);
			return false;
		}
		buffer_free_weak(b);
	} else {
		ttf_set_pixel_size(ttf, size);
	}

	use_ttf = 1;
	return ttf;
}
#endif

extern bool disp_load_zipped_truetype_book_font(const char *ezipfile,
												const char *czipfile,
												const char *ettffile,
												const char *cttffile, int size)
{
#ifdef ENABLE_TTF
	static char prev_ettfpath[PATH_MAX] = "", prev_ettfarch[PATH_MAX] = "";
	static char prev_cttfpath[PATH_MAX] = "", prev_cttfarch[PATH_MAX] = "";

	use_ttf = 0;
	memset(disp_ewidth, size / 2, 0x80);
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if (ettf != NULL && strcmp(prev_ettfarch, config.ettfarch) == 0
		&& strcmp(prev_ettfpath, config.ettfpath) == 0) {
		ttf_set_pixel_size(ettf, size);
	} else {
		ttf_close(ettf);
		ettf = NULL;
		if (config.ettfarch[0] != '\0') {
			ettf =
				load_archieve_truetype_book_font(config.ettfarch,
												 config.ettfpath,
												 config.bookfontsize);
		} else {
			ettf = ttf_open(config.ettfpath, config.bookfontsize);
		}
		STRCPY_S(prev_ettfarch, config.ettfarch);
		STRCPY_S(prev_ettfpath, config.ettfpath);
	}
	if (cttf != NULL && strcmp(prev_cttfarch, config.cttfarch) == 0
		&& strcmp(prev_cttfpath, config.cttfpath) == 0) {
		ttf_set_pixel_size(cttf, size);
	} else {
		if (cttf != NULL) {
			ttf_close(cttf);
			cttf = NULL;
		}
		if (config.cttfarch[0] != '\0') {
			cttf =
				load_archieve_truetype_book_font(config.cttfarch,
												 config.cttfpath,
												 config.bookfontsize);
		} else {
			cttf = ttf_open(config.cttfpath, config.bookfontsize);
		}
		STRCPY_S(prev_cttfarch, config.cttfarch);
		STRCPY_S(prev_cttfpath, config.cttfpath);
	}

	if (cttf == NULL || ettf == NULL)
		return false;

	load_ttf_config();

	ttf_load_ewidth(ettf, disp_ewidth, 0x80);
	use_ttf = 1;
	return true;
#else
	return false;
#endif
}

extern bool disp_load_font(const char *efont, const char *cfont)
{
	disp_free_font();
	int size;
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if ((efont_buffer = (byte *) calloc(1, size)) == NULL) {
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, efont_buffer, size);
	sceIoClose(fd);
	book_efont_buffer = efont_buffer;

	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		disp_free_font();
		return false;
	}
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if ((cfont_buffer = (byte *) calloc(1, size)) == NULL) {
		disp_free_font();
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, cfont_buffer, size);
	sceIoClose(fd);
	book_cfont_buffer = cfont_buffer;

	return true;
}

extern bool disp_load_zipped_book_font(const char *zipfile, const char *efont,
									   const char *cfont)
{
	use_ttf = 0;
#ifdef ENABLE_TTF
	if (ettf != NULL) {
		ttf_close(ettf);
		ettf = NULL;
	}
	if (cttf != NULL) {
		ttf_close(cttf);
		cttf = NULL;
	}
#endif
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
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
	if ((book_efont_buffer = (byte *) calloc(1, size)) == NULL) {
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, book_efont_buffer, size);
	unzCloseCurrentFile(unzf);

	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
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
	if ((book_cfont_buffer = (byte *) calloc(1, size)) == NULL) {
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
	use_ttf = 0;
#ifdef ENABLE_TTF
	if (ettf != NULL) {
		ttf_close(ettf);
		ettf = NULL;
	}
	if (cttf != NULL) {
		ttf_close(cttf);
		cttf = NULL;
	}
#endif
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
		book_efont_buffer = NULL;
	}
	int size;
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if ((book_efont_buffer = (byte *) calloc(1, size)) == NULL) {
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, book_efont_buffer, size);
	sceIoClose(fd);

	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		disp_free_font();
		return false;
	}
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if ((book_cfont_buffer = (byte *) calloc(1, size)) == NULL) {
		disp_free_font();
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, book_cfont_buffer, size);
	sceIoClose(fd);

	return true;
}

extern void disp_free_font()
{
	if (book_efont_buffer != NULL && efont_buffer != book_efont_buffer) {
		free((void *) book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if (book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer) {
		free((void *) book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if (efont_buffer != NULL) {
		free((void *) efont_buffer);
		efont_buffer = NULL;
	}
	if (cfont_buffer != NULL) {
		free((void *) cfont_buffer);
		cfont_buffer = NULL;
	}
	use_ttf = 0;
#ifdef ENABLE_TTF
	memset(disp_ewidth, 0, 0x80);
	if (ettf != NULL) {
		ttf_close(ettf);
		ettf = NULL;
	}
	if (cttf != NULL) {
		ttf_close(cttf);
		cttf = NULL;
	}
#endif
}

#ifdef ENABLE_GE
void *framebuffer = 0;
#endif

extern void disp_flip()
{
	vram_page ^= 1;
	vram_base =
		(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
	vram_start =
		(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	disp_waitv();
#ifdef COLOR16BIT
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_5551,
						  PSP_DISPLAY_SETBUF_IMMEDIATE);
#else
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_IMMEDIATE);
#endif
#ifdef ENABLE_GE
	framebuffer = sceGuSwapBuffers();
#endif
}

extern void disp_getimage(dword x, dword y, dword w, dword h, pixel * buf)
{
	CHECK_AND_VALID_WH(x, y, w, h);
	pixel *lines = vram_base + 0x40000000 / PIXEL_BYTES, *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h) << 9);
	dword rw = min(512 - x, w) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(buf, lines, rw);
		buf += w;
	}
}

#ifndef ENABLE_GE
extern void disp_newputimage(int x, int y, int w, int h, int bufw, int startx,
							 int starty, int ow, int oh, pixel * buf,
							 bool swizzled)
{
	int pitch = (w + 15) & ~15;
	pixel *lines = disp_get_vaddr(x, y), *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h - starty) << 9);
	buf = buf + starty * pitch + startx;
	dword rw = min(512 - x, pitch - startx) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(lines, buf, rw);
		buf += w;
	}
}
#else
/**
 * x: x 坐标
 * y: y 坐标
 * w: 图宽度
 * h: 图高度
 * bufw: 贴图线宽度
 * startx: 开始x坐标
 * starty: 开始y坐标 
 * ow: ?
 * oh: ?
 * buf: 贴图缓存
 * swizzled: 是否为为碎贴图
 */
extern void disp_newputimage(int x, int y, int w, int h, int bufw, int startx,
							 int starty, int ow, int oh, pixel * buf,
							 bool swizzled)
{
	sceGuStart(GU_DIRECT, list);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuShadeModel(GU_SMOOTH);
	sceGuAmbientColor(0xFFFFFFFF);
	Vertex *vertices = (Vertex *) sceGuGetMemory(2 * sizeof(Vertex));

	sceGuTexMode(GU_PSM_8888, 0, 0, swizzled ? 1 : 0);
	sceGuTexImage(0, 512, 512, bufw, buf);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
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
	sceGuDrawArray(GU_SPRITES,
				   GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT |
				   GU_TRANSFORM_2D, 2, 0, vertices);
	sceGuFinish();
	sceGuSync(0, 0);
}
#endif

// TODO: use Gu CopyImage
// buf: 图像数据: 现在大小为(16-8对齐)
// w: 要复制的宽度, 为图像
// h: 要复制的高度
extern void disp_putimage(dword x, dword y, dword w, dword h, dword startx,
						  dword starty, pixel * buf)
{
	CHECK_AND_VALID(x, y);
	pixel *lines = disp_get_vaddr(x, y), *linesend =
		lines + (min(PSP_SCREEN_HEIGHT - y, h - starty) << 9);
	buf = buf + starty * w + startx;
	dword rw = min(512 - x, w - startx) * PIXEL_BYTES;

	for (; lines < linesend; lines += 512) {
		memcpy(lines, buf, rw);
		buf += w;
	}
	return;
}

extern void disp_duptocache()
{
	memmove(vram_start, ((byte *) vram_base) + 0x40000000,
			512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
}

extern void disp_duptocachealpha(int percent)
{
	pixel *vsrc = (pixel *) (((byte *) vram_base) + 0x40000000), *vdest =
		vram_start;
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

extern void disp_fix_osk(void *buffer)
{
	if (buffer) {
		vram_page = 0;
		vram_base =
			(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
		vram_start =
			(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	} else {
		vram_page = 1;
		vram_base =
			(pixel *) 0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
		vram_start =
			(pixel *) 0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	}
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_IMMEDIATE);
}

extern void disp_rectduptocache(dword x1, dword y1, dword x2, dword y2)
{
	CHECK_AND_VALID_4(x1, y1, x2, y2);
	pixel *lines = disp_get_vaddr(x1, y1), *linesend =
		disp_get_vaddr(x1, y2), *lined =
		vram_base + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
	dword w = (x2 - x1 + 1) * PIXEL_BYTES;

	for (; lines <= linesend; lines += 512, lined += 512)
		memcpy(lines, lined, w);
}

extern void disp_rectduptocachealpha(dword x1, dword y1, dword x2, dword y2,
									 int percent)
{
	CHECK_AND_VALID_4(x1, y1, x2, y2);
	pixel *lines = disp_get_vaddr(x1, y1), *linesend =
		disp_get_vaddr(x1, y2), *lined =
		vram_base + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
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

extern void disp_putnstring(int x, int y, pixel color, const byte * str,
							int count, dword wordspace, int top, int height,
							int bot)
{
	pixel *vaddr;
	const byte *ccur, *cend;

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	CHECK_AND_VALID(x, y);

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE) {
				break;
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);
			ccur =
				cfont_buffer + (((dword) (*str - 0x81)) * 0xBF +
								((dword) (*(str + 1) - 0x40))) *
				DISP_CFONTSIZE + top * DISP_CROWSIZE;

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
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);
			ccur =
				efont_buffer + ((dword) * str) * DISP_EFONTSIZE +
				top * DISP_EROWSIZE;

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
			str++;
			count--;
			x += DISP_FONTSIZE / 2 + wordspace;
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	CHECK_AND_VALID(x, y);

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	if (x < 0 || y < 0)
		return;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x < 0) {
				break;
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE;
				y -= DISP_FONTSIZE;
#endif
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
						vpoint--;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint--;
				}
				vaddr -= 512;
			}
			str += 2;
			count -= 2;
			x -= DISP_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (x < 0) {
				break;
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2;
				y -= DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					efont_buffer + ((dword) * str) * DISP_EFONTSIZE +
					top * DISP_EROWSIZE;
				for (cend = ccur + height * DISP_EROWSIZE; ccur < cend; ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint--;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint--;
					}
					vaddr -= 512;
				}
				x -= DISP_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (x < 0) {
				break;
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2;
				y -= DISP_FONTSIZE;
#endif
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
	if (use_ttf) {
		disp_putnstring_reversal_truetype(cttf, ettf, x, y, color, str, count,
										  wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	CHECK_AND_VALID(x, y);

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	if (x < 0 || y < 0)
		return;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x < 0) {
				break;
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE;
				y -= DISP_BOOK_FONTSIZE;
#endif
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
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2;
				y -= DISP_BOOK_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					book_efont_buffer + ((dword) * str) * DISP_BOOK_EFONTSIZE +
					top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend;
					 ccur++) {
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
#if 0
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2;
				y -= DISP_BOOK_FONTSIZE;
#endif
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

	CHECK_AND_VALID(x, y);

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE) {
				break;
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
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
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					efont_buffer + ((dword) * str) * DISP_EFONTSIZE +
					top * DISP_EROWSIZE;
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
#if 0
				x = 0;
				y += DISP_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

#ifdef ENABLE_TTF
	if (use_ttf) {
		disp_putnstring_horz_truetype(cttf, ettf, x, y, color, str, count,
									  wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
	}

	CHECK_AND_VALID(x, y);

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE) {
				break;
#if 0
				x = 0;
				y += DISP_BOOK_FONTSIZE;
#endif
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
						vpoint++;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint++;
				}
				vaddr += 512;
			}
			str += 2;
			count -= 2;
			x += DISP_BOOK_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
#if 0
				x = 0;
				y += DISP_BOOK_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					book_efont_buffer + ((dword) * str) * DISP_BOOK_EFONTSIZE +
					top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend;
					 ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint++;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint++;
					}
					vaddr += 512;
				}
				x += DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
#if 0
				x = 0;
				y += DISP_BOOK_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

	if (bot) {
		if (x >= bot)
			return;
		if (x + height > bot)
			height = bot - x;
	}

	CHECK_AND_VALID(x, y);

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y < DISP_RSPAN + DISP_FONTSIZE - 1) {
				break;
#if 0
				y = 271;
				x += DISP_FONTSIZE;
#endif
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
						vpoint -= 512;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint -= 512;
				}
				vaddr++;
			}
			str += 2;
			count -= 2;
			y -= DISP_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (y < DISP_RSPAN + DISP_FONTSIZE - 1) {
				break;
#if 0
				y = 271;
				x += DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					efont_buffer + ((dword) * str) * DISP_EFONTSIZE +
					top * DISP_EROWSIZE;
				for (cend = ccur + height * DISP_EROWSIZE; ccur < cend; ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint -= 512;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint -= 512;
					}
					vaddr++;
				}
				y -= DISP_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (y < DISP_RSPAN + DISP_FONTSIZE - 1) {
				break;
#if 0
				y = 271;
				x += DISP_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

#ifdef ENABLE_TTF
	if (use_ttf) {
		disp_putnstring_lvert_truetype(cttf, ettf, x, y, color, str, count,
									   wordspace, top, height, bot);
		return;
	}
#endif

	if (bot) {
		if (x >= bot)
			return;
		if (x + height > bot)
			height = bot - x;
	}

	CHECK_AND_VALID(x, y);

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y < DISP_BOOK_FONTSIZE - 1) {
				break;
#if 0
				y = 271;
				x += DISP_BOOK_FONTSIZE;
#endif
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
						vpoint -= 512;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint -= 512;
				}
				vaddr++;
			}
			str += 2;
			count -= 2;
			y -= DISP_BOOK_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (y < DISP_BOOK_FONTSIZE / 2 - 1) {
				break;
#if 0
				y = 271;
				x += DISP_BOOK_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					book_efont_buffer + ((dword) * str) * DISP_BOOK_EFONTSIZE +
					top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend;
					 ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint -= 512;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint -= 512;
					}
					vaddr++;
				}
				y -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (y < DISP_BOOK_FONTSIZE / 2 - 1) {
				break;
#if 0
				y = 271;
				x += DISP_BOOK_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

	CHECK_AND_VALID(x, y);

	if (x < bot)
		return;
	if (x + 1 - height < bot)
		height = x + 1 - bot;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE) {
				break;
#if 0
				y = 0;
				x -= DISP_FONTSIZE;
#endif
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
						vpoint += 512;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint += 512;
				}
				vaddr--;
			}
			str += 2;
			count -= 2;
			y += DISP_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
#if 0
				y = 0;
				x -= DISP_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					efont_buffer + ((dword) * str) * DISP_EFONTSIZE +
					top * DISP_EROWSIZE;
				for (cend = ccur + height * DISP_EROWSIZE; ccur < cend; ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint += 512;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint += 512;
					}
					vaddr--;
				}
				y += DISP_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_FONTSIZE / 2) {
				break;
#if 0
				y = 0;
				x -= DISP_FONTSIZE;
#endif
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
	pixel *vaddr;
	const byte *ccur, *cend;

#ifdef ENABLE_TTF
	if (use_ttf) {
		disp_putnstring_rvert_truetype(cttf, ettf, x, y, color, str, count,
									   wordspace, top, height, bot);
		return;
	}
#endif

	CHECK_AND_VALID(x, y);

	if (x < bot)
		return;
	if (x + 1 - height < bot)
		height = x + 1 - bot;

	while (*str != 0 && count > 0) {
		if (*str > 0x80) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE) {
				break;
#if 0
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
#endif
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
						vpoint += 512;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						*vpoint = color;
					vpoint += 512;
				}
				vaddr--;
			}
			str += 2;
			count -= 2;
			y += DISP_BOOK_FONTSIZE + wordspace * 2;
		} else if (*str > 0x1F) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
#if 0
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
#endif
			}
			if (!check_range(x, y))
				return;
			vaddr = disp_get_vaddr(x, y);

			{
				ccur =
					book_efont_buffer + ((dword) * str) * DISP_BOOK_EFONTSIZE +
					top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend;
					 ccur++) {
					int b;
					pixel *vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;

					while (bitsleft > 0) {
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								*vpoint = color;
							vpoint += 512;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							*vpoint = color;
						vpoint += 512;
					}
					vaddr--;
				}
				y += DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str++;
			count--;
		} else {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
#if 0
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
#endif
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
#ifndef ENABLE_GE
	pixel *vram, *vram_end;

	if (color == 0 || color == (pixel) - 1)
		memset(vram_start, (color & 0xFF),
			   512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	else
		for (vram = vram_start, vram_end = vram + 0x22000; vram < vram_end;
			 vram++)
			*vram = color;
#else
	sceGuStart(GU_DIRECT, list);
	sceGuClearColor(color);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(0, 0);
#endif
}

extern void disp_fillrect(dword x1, dword y1, dword x2, dword y2, pixel color)
{
#ifndef ENABLE_GE
	CHECK_AND_VALID_4(x1, y1, x2, y2);
	pixel *vsram, *vsram_end, *vram, *vram_end;
	dword wdwords;

	if (x1 > x2) {
		dword t = x1;

		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		dword t = y1;

		y1 = y2;
		y2 = t;
	}
	vsram = disp_get_vaddr(x1, y1);
	vsram_end = vsram + 512 * (y2 - y1);
	wdwords = (x2 - x1);
	for (; vsram <= vsram_end; vsram += 512)
		for (vram = vsram, vram_end = vram + wdwords; vram <= vram_end; vram++)
			*vram = color;
#else
	sceGuStart(GU_DIRECT, list);
	VertexColor *vertices =
		(VertexColor *) sceGuGetMemory(2 * sizeof(VertexColor));

	vertices[0].color = color;
	vertices[0].x = x1;
	vertices[0].y = y1;
	vertices[0].z = 0;

	vertices[1].color = color;
	vertices[1].x = x2 + 1;
	vertices[1].y = y2 + 1;
	vertices[1].z = 0;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDrawArray(GU_SPRITES,
				   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0,
				   vertices);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0, 0);
#endif
}

extern void disp_rectangle(dword x1, dword y1, dword x2, dword y2, pixel color)
{
#ifndef ENABLE_GE
	CHECK_AND_VALID_4(x1, y1, x2, y2);
	pixel *vsram, *vram, *vram_end;

	if (x1 > x2) {
		dword t = x1;

		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		dword t = y1;

		y1 = y2;
		y2 = t;
	}
	vsram = disp_get_vaddr(x1, y1);
	for (vram = vsram, vram_end = vram + (x2 - x1); vram < vram_end; vram++)
		*vram = color;
	for (vram_end = vram + 512 * (y2 - y1); vram < vram_end; vram += 512)
		*vram = color;
	for (vram = vsram, vram_end = vram + 512 * (y2 - y1); vram < vram_end;
		 vram += 512)
		*vram = color;
	for (vram_end = vram + (x2 - x1); vram < vram_end; vram++)
		*vram = color;
	*vram = color;
#else
	sceGuStart(GU_DIRECT, list);
	VertexColor *vertices =
		(VertexColor *) sceGuGetMemory(5 * sizeof(VertexColor));

	vertices[0].color = color;
	vertices[0].x = x1;
	vertices[0].y = y1;
	vertices[0].z = 0;

	vertices[1].color = color;
	vertices[1].x = x2;
	vertices[1].y = y1;
	vertices[1].z = 0;

	vertices[2].color = color;
	vertices[2].x = x2;
	vertices[2].y = y2;
	vertices[2].z = 0;

	vertices[3].color = color;
	vertices[3].x = x1;
	vertices[3].y = y2;
	vertices[3].z = 0;

	vertices[4].color = color;
	vertices[4].x = x1;
	vertices[4].y = y1;
	vertices[4].z = 0;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDrawArray(GU_LINE_STRIP,
				   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 5, 0,
				   vertices);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0, 0);
#endif
}

extern void disp_line(dword x1, dword y1, dword x2, dword y2, pixel color)
{
#ifndef ENABLE_GE
	CHECK_AND_VALID_4(x1, y1, x2, y2);
	pixel *vram;
	int dy, dx, x, y, d;

	dx = (int) x2 - (int) x1;
	dy = (int) y2 - (int) y1;
	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;
	d = -dx;
	x = (int) x1;
	y = (int) y1;
	if (dx >= dy) {
		if (y2 < y1) {
			dword t = x1;

			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}
		vram = disp_get_vaddr(x1, y1);
		if (x1 < x2) {
			for (x = x1; x <= x2; x++) {
				if (d > 0) {
					y++;
					vram += 512;
					d -= 2 * dx;
				}
				d += 2 * dy;
				*vram = color;
				vram++;
			}
		} else {
			for (x = x1; x >= x2; x--) {
				if (d > 0) {
					y++;
					vram += 512;
					d -= 2 * dx;
				}
				d += 2 * dy;
				*vram = color;
				vram--;
			}
		}
	} else {
		if (x2 < x1) {
			dword t = x1;

			x1 = x2;
			x2 = t;
			t = y1;
			y1 = y2;
			y2 = t;
		}
		vram = disp_get_vaddr(x1, y1);
		if (y1 < y2) {
			for (y = y1; y <= y2; y++) {
				if (d > 0) {
					x++;
					vram++;
					d -= 2 * dy;
				}
				d += 2 * dx;
				*vram = color;
				vram += 512;
			}
		} else {
			for (y = y1; y >= y2; y--) {
				if (d > 0) {
					x++;
					vram++;
					d -= 2 * dy;
				}
				d += 2 * dx;
				*vram = color;
				vram -= 512;
			}
		}
	}
#else
	sceGuStart(GU_DIRECT, list);
	VertexColor *vertices =
		(VertexColor *) sceGuGetMemory(2 * sizeof(VertexColor));

	vertices[0].color = color;
	vertices[0].x = x1;
	vertices[0].y = y1;
	vertices[0].z = 0;

	vertices[1].color = color;
	vertices[1].x = x2;
	vertices[1].y = y2;
	vertices[1].z = 0;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
				   2, 0, vertices);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(0, 0);
#endif
}
