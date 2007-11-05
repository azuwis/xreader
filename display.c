#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include <unzip.h>
#include "conf.h"
#include "ttfont.h"
#include "display.h"
#include "pspscreen.h"

extern t_conf config;

static bool auto_inc_wordspace_on_small_font = false;
static pixel * vram_base = NULL;
pixel * vram_start = NULL;
static bool vram_page = 0;
static byte * cfont_buffer = NULL, * book_cfont_buffer = NULL, * efont_buffer = NULL, * book_efont_buffer = NULL;
int DISP_FONTSIZE = 16, DISP_BOOK_FONTSIZE = 16, HRR = 6, WRR = 10;
static int use_ttf = 0, DISP_EFONTSIZE, DISP_CFONTSIZE, DISP_CROWSIZE, DISP_EROWSIZE, fbits_last = 0, febits_last = 0, DISP_BOOK_EFONTSIZE, DISP_BOOK_CFONTSIZE, DISP_BOOK_EROWSIZE, DISP_BOOK_CROWSIZE, fbits_book_last = 0, febits_book_last = 0;;
byte disp_ewidth[0x80];
#ifdef ENABLE_TTF
static byte * cache = NULL;
static void * ttfh = NULL;
#endif

#define DISP_RSPAN 0

extern void disp_init()
{
	sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	vram_page = 0;
	vram_base = (pixel *)0x04000000;
	vram_start = (pixel *)(0x44000000 + 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
#ifdef COLOR16BIT
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_NEXTFRAME);
#else
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
#endif
}

extern void disp_set_fontsize(int fontsize)
{
	if(!use_ttf)
		memset(disp_ewidth, fontsize / 2, 0x80);
	DISP_FONTSIZE = fontsize;
	if(fontsize <= 16)
	{
		DISP_CROWSIZE = 2;
		DISP_EROWSIZE = 1;
		fbits_last = (1 << (16 - fontsize)) / 2;
		febits_last = (1 << (8 - fontsize / 2)) / 2;
	}
	else if(fontsize <= 24)
	{
		DISP_CROWSIZE = 3;
		DISP_EROWSIZE = 2;
		fbits_last = (1 << (24 - fontsize)) / 2;
		febits_last = (1 << (16 - fontsize / 2)) / 2;
	}
	else
	{
		DISP_CROWSIZE = 4;
		DISP_EROWSIZE = 2;
		fbits_last = (1 << (32 - fontsize)) / 2;
		febits_last = (1 << (16 - fontsize / 2)) / 2;
	}
	DISP_CFONTSIZE = DISP_FONTSIZE * DISP_CROWSIZE;
	DISP_EFONTSIZE = DISP_FONTSIZE * DISP_EROWSIZE;
	HRR = 100 / DISP_FONTSIZE;
	WRR = 160 / DISP_FONTSIZE;

	extern int MAX_ITEM_NAME_LEN;
	MAX_ITEM_NAME_LEN = WRR * 4 - 1;
}

extern void disp_set_book_fontsize(int fontsize)
{
	DISP_BOOK_FONTSIZE = fontsize;
	if(fontsize <= 16)
	{
		DISP_BOOK_CROWSIZE = 2;
		DISP_BOOK_EROWSIZE = 1;
		fbits_book_last = (1 << (16 - fontsize)) / 2;
		febits_book_last = (1 << (8 - fontsize / 2)) / 2;
	}
	else if(fontsize <= 24)
	{
		DISP_BOOK_CROWSIZE = 3;
		DISP_BOOK_EROWSIZE = 2;
		fbits_book_last = (1 << (24 - fontsize)) / 2;
		febits_book_last = (1 << (16 - fontsize / 2)) / 2;
	}
	else
	{
		DISP_BOOK_CROWSIZE = 4;
		DISP_BOOK_EROWSIZE = 2;
		fbits_book_last = (1 << (32 - fontsize)) / 2;
		febits_book_last = (1 << (16 - fontsize / 2)) / 2;
	}
	DISP_BOOK_CFONTSIZE = DISP_BOOK_FONTSIZE * DISP_BOOK_CROWSIZE;
	DISP_BOOK_EFONTSIZE = DISP_BOOK_FONTSIZE * DISP_BOOK_EROWSIZE;

	// if set font size to very small one, set config.wordspace to 1
	if(fontsize <= 10 && config.wordspace == 0) {
		config.wordspace = 1;
		auto_inc_wordspace_on_small_font = true;
	}

	// if previous have auto increased wordspace on small font, restore config.wordspace to 0
	if(fontsize >= 12 && auto_inc_wordspace_on_small_font && config.wordspace == 1) {
		config.wordspace = 0;
		auto_inc_wordspace_on_small_font = false;
	}
}

extern bool disp_has_zipped_font(const char * zipfile, const char * efont, const char * cfont)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return false;

	if(unzLocateFile(unzf, efont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	unzCloseCurrentFile(unzf);

	if(unzLocateFile(unzf, cfont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	unzCloseCurrentFile(unzf);

	unzClose(unzf);
	return true;
}

extern bool disp_has_font(const char * efont, const char * cfont)
{
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	sceIoClose(fd);

	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	sceIoClose(fd);
	return true;
}

extern void disp_assign_book_font()
{
	use_ttf = 0;
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
#ifdef ENABLE_TTF
	if(ttfh != NULL)
	{
		ttf_close(ttfh);
		ttfh = NULL;
	}
	if(cache != NULL)
	{
		free(cache);
		cache = NULL;
	}
#endif
	book_efont_buffer = efont_buffer;
	book_cfont_buffer = cfont_buffer;
}

extern bool disp_load_zipped_font(const char * zipfile, const char * efont, const char * cfont)
{
	disp_free_font();
	unzFile unzf = unzOpen(zipfile);
	unz_file_info info;
	dword size;

	if(unzf == NULL)
		return false;

	if(unzLocateFile(unzf, efont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if((efont_buffer = (byte *)malloc(size)) == NULL)
	{
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, efont_buffer, size);
	unzCloseCurrentFile(unzf);
	book_efont_buffer = efont_buffer;

	if(unzLocateFile(unzf, cfont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if((cfont_buffer = (byte *)malloc(size)) == NULL)
	{
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

extern bool disp_load_truetype_book_font(const char * ettffile, const char *cttffile, int size)
{
#ifdef ENABLE_TTF
	use_ttf = 0;
	memset(disp_ewidth, 0, 0x80);
	if(cache != NULL)
	{
		free((void *)cache);
		cache = NULL;
	}
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if(!ttf_cache_ascii(ettffile, size, &book_efont_buffer, disp_ewidth))
		return false;
	if(ttfh == NULL)
	{
		if((ttfh = ttf_open(cttffile, size, &book_cfont_buffer)) == NULL)
		{
			free((void *)book_efont_buffer);
			book_efont_buffer = NULL;
			return false;
		}
	}
	else
	{
		if((ttfh = ttf_reopen(ttfh, size, &book_cfont_buffer)) == NULL)
		{
			free((void *)book_efont_buffer);
			book_efont_buffer = NULL;
			return false;
		}
	}
	cache = (byte *)calloc(1, 0x5E02);
	use_ttf = 1;
	return true;
#else
	return false;
#endif
}

extern bool disp_load_zipped_truetype_book_font(const char * zipfile, const char * ettffile, const char *cttffile, int size)
{
#ifdef ENABLE_TTF
	use_ttf = 0;
	memset(disp_ewidth, 0, 0x80);
	if(cache != NULL)
	{
		free((void *)cache);
		cache = NULL;
	}
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	unzFile unzf = unzOpen(zipfile);
	unz_file_info info;
	dword usize;
	byte * buf;

	if(unzf == NULL)
		return false;

	if(unzLocateFile(unzf, ettffile, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	usize = info.uncompressed_size;
	if((buf = (byte *)calloc(1, usize)) == NULL)
	{
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, buf, usize);
	unzCloseCurrentFile(unzf);
	if(!ttf_cache_ascii_buffer(buf, usize, size, &book_efont_buffer, disp_ewidth))
	{
		unzClose(unzf);
		return false;
	}

	if(ttfh == NULL)
	{
		if(unzLocateFile(unzf, cttffile, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
		{
			unzClose(unzf);
			return false;
		}
		if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
		{
			unzCloseCurrentFile(unzf);
			unzClose(unzf);
			return false;
		}
		usize = info.uncompressed_size;
		if((buf = (byte *)calloc(1, usize)) == NULL)
		{
			disp_free_font();
			unzCloseCurrentFile(unzf);
			unzClose(unzf);
			return false;
		}
		unzReadCurrentFile(unzf, buf, usize);
		unzCloseCurrentFile(unzf);
		if((ttfh = ttf_open_buffer(buf, usize, size, &book_cfont_buffer)) == NULL)
		{
			free((void *)book_efont_buffer);
			book_efont_buffer = NULL;
			return false;
		}
	}
	else
	{
		if((ttfh = ttf_reopen(ttfh, size, &book_cfont_buffer)) == NULL)
		{
			free((void *)book_efont_buffer);
			book_efont_buffer = NULL;
			return false;
		}
	}
	cache = (byte *)calloc(1, 0x5E02);
	use_ttf = 1;
	unzClose(unzf);
	return true;
#else
	return false;
#endif
}

extern bool disp_load_font(const char * efont, const char * cfont)
{
	disp_free_font();
	int size;
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if((efont_buffer = (byte *)calloc(1, size)) == NULL)
	{
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, efont_buffer, size);
	sceIoClose(fd);
	book_efont_buffer = efont_buffer;

	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if(fd < 0)
	{
		disp_free_font();
		return false;
	}
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if((cfont_buffer = (byte *)calloc(1, size)) == NULL)
	{
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

extern bool disp_load_zipped_book_font(const char * zipfile, const char * efont, const char * cfont)
{
	use_ttf = 0;
#ifdef ENABLE_TTF
	if(ttfh != NULL)
	{
		ttf_close(ttfh);
		ttfh = NULL;
	}
	if(cache != NULL)
	{
		free(cache);
		cache = NULL;
	}
#endif
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	unzFile unzf = unzOpen(zipfile);
	unz_file_info info;
	dword size;

	if(unzf == NULL)
		return false;

	if(unzLocateFile(unzf, efont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if((book_efont_buffer = (byte *)calloc(1, size)) == NULL)
	{
		disp_free_font();
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	unzReadCurrentFile(unzf, book_efont_buffer, size);
	unzCloseCurrentFile(unzf);

	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if(unzLocateFile(unzf, cfont, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return false;
	}
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return false;
	}
	size = info.uncompressed_size;
	if((book_cfont_buffer = (byte *)calloc(1, size)) == NULL)
	{
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

extern bool disp_load_book_font(const char * efont, const char * cfont)
{
	use_ttf = 0;
#ifdef ENABLE_TTF
	if(ttfh != NULL)
	{
		ttf_close(ttfh);
		ttfh = NULL;
	}
	if(cache != NULL)
	{
		free(cache);
		cache = NULL;
	}
#endif
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	int size;
	int fd = sceIoOpen(efont, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if((book_efont_buffer = (byte *)calloc(1, size)) == NULL)
	{
		sceIoClose(fd);
		return false;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, book_efont_buffer, size);
	sceIoClose(fd);

	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	fd = sceIoOpen(cfont, PSP_O_RDONLY, 0777);
	if(fd < 0)
	{
		disp_free_font();
		return false;
	}
	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if((book_cfont_buffer = (byte *)calloc(1, size)) == NULL)
	{
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
	if(book_efont_buffer != NULL && efont_buffer != book_efont_buffer)
	{
		free((void *)book_efont_buffer);
		book_efont_buffer = NULL;
	}
	if(book_cfont_buffer != NULL && cfont_buffer != book_cfont_buffer)
	{
		free((void *)book_cfont_buffer);
		book_cfont_buffer = NULL;
	}
	if(efont_buffer != NULL)
	{
		free((void *)efont_buffer);
		efont_buffer = NULL;
	}
	if(cfont_buffer != NULL)
	{
		free((void *)cfont_buffer);
		cfont_buffer = NULL;
	}
	use_ttf = 0;
#ifdef ENABLE_TTF
	memset(disp_ewidth, 0, 0x80);
	if(ttfh != NULL)
	{
		ttf_close(ttfh);
		ttfh = NULL;
	}
	if(cache != NULL)
	{
		free((void *)cache);
		cache = NULL;
	}
#endif
}

extern void disp_flip()
{
	vram_page ^= 1;
	vram_base = (pixel *)0x04000000 + (vram_page ? (512 * PSP_SCREEN_HEIGHT) : 0);
	vram_start = (pixel *)0x44000000 + (vram_page ? 0 : (512 * PSP_SCREEN_HEIGHT));
	disp_waitv();
#ifdef COLOR16BIT
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_IMMEDIATE);
#else
	sceDisplaySetFrameBuf(vram_base, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
#endif
}

extern void disp_getimage(dword x, dword y, dword w, dword h, pixel * buf)
{
	pixel * lines = vram_base + 0x40000000 / PIXEL_BYTES, * linesend = lines + (min(PSP_SCREEN_HEIGHT - y, h) << 9);
	dword rw = min(512 - x, w) * PIXEL_BYTES;
	for(;lines < linesend; lines += 512)
	{
		memcpy(buf, lines, rw);
		buf += w;
	}
}

extern void disp_putimage(dword x, dword y, dword w, dword h, dword startx, dword starty, pixel * buf)
{
	pixel * lines = disp_get_vaddr(x, y), * linesend = lines + (min(PSP_SCREEN_HEIGHT - y, h - starty) << 9);
	buf = buf + starty * w + startx;
	dword rw = min(512 - x, w - startx) * PIXEL_BYTES;
	for(;lines < linesend; lines += 512)
	{
		memcpy(lines, buf, rw);
		buf += w;
	}
}

extern void disp_duptocache()
{
	memmove(vram_start, ((byte *)vram_base) + 0x40000000, 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
}

extern void disp_duptocachealpha(int percent)
{
	pixel * vsrc = (pixel *)(((byte *)vram_base) + 0x40000000), * vdest = vram_start;
	int i, j;
	for(i = 0; i < PSP_SCREEN_HEIGHT; i ++)
	{
		pixel * vput = vdest, * vget = vsrc;
		for(j = 0; j < PSP_SCREEN_WIDTH; j ++)
		{
			*vput ++ = disp_grayscale(*vget, 0, 0, 0, percent);
			vget ++;
		}
		vsrc += 512;
		vdest += 512;
	}
}

extern void disp_rectduptocache(dword x1, dword y1, dword x2, dword y2)
{
	pixel * lines = disp_get_vaddr(x1, y1), * linesend = disp_get_vaddr(x1, y2), * lined = vram_base + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
	dword w = (x2 - x1 + 1) * PIXEL_BYTES;
	for(;lines <= linesend; lines += 512, lined += 512)
		memcpy(lines, lined, w);
}

extern void disp_rectduptocachealpha(dword x1, dword y1, dword x2, dword y2, int percent)
{
	pixel * lines = disp_get_vaddr(x1, y1), * linesend = disp_get_vaddr(x1, y2), * lined = vram_base + 0x40000000 / PIXEL_BYTES + x1 + (y1 << 9);
	dword w = x2 - x1 + 1;
	for(;lines <= linesend; lines += 512, lined += 512)
	{
		pixel * vput = lines, * vget = lined;
		dword i;
		for(i = 0; i < w; i ++)
		{
			*vput ++ = disp_grayscale(*vget, 0, 0, 0, percent);
			vget ++;
		}
	}
}

extern void disp_putnstring(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot)
{
	pixel * vaddr;
	const byte * ccur, * cend;

	if(bot)
	{
		if(y >= bot)
			return;
		if(y + height > bot)
			height = bot - y;
	}

	while(*str != 0 && count > 0)
	{
		if(*str > 0x80)
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE)
			{
				x = 0;
				y += DISP_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			ccur = cfont_buffer + (((dword)(*str - 0x81)) * 0xBF + ((dword)(*(str + 1) - 0x40))) * DISP_CFONTSIZE + top * DISP_CROWSIZE;
		
			for (cend = ccur + height * DISP_CROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_FONTSIZE - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint ++;
					}
					++ ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_last; b >>= 1) {
					if (((* ccur) & b) != 0)
						* vpoint = color;
					vpoint ++;
				}
				vaddr += 512;
			}
			str += 2;
			count -= 2;
			x += DISP_FONTSIZE + wordspace * 2;
		}
		else if(*str > 0x1F)
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2)
			{
				x = 0;
				y += DISP_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			ccur = efont_buffer + ((dword)*str) * DISP_EFONTSIZE + top * DISP_EROWSIZE;

			for (cend = ccur + height * DISP_EROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_FONTSIZE / 2 - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((*ccur) & b) != 0)
							* vpoint = color;
						vpoint ++;
					}
					++ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > febits_last; b >>= 1) {
					if (((*ccur) & b) != 0)
						* vpoint = color;
					vpoint ++;
				}
				vaddr += 512;
			}
			str ++;
			count --;
			x += DISP_FONTSIZE / 2 + wordspace;
		}
		else
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_FONTSIZE / 2)
			{
				x = 0;
				y += DISP_FONTSIZE;
			}
			str ++;
			count --;
			x += DISP_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringreversal(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot)
{
	pixel * vaddr;
	const byte * ccur, * cend;

	if(bot)
	{
		if(y >= bot)
			return;
		if(y + height > bot)
			height = bot - y;
	}

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	if(x < 0 || y < 0)
		return;

	while(*str != 0 && count > 0)
	{
		if(*str > 0x80)
		{
			if(x < 0)
			{
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE;
				y -= DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			dword pos = (((dword)(*str - 0x81)) * 0xBF + ((dword)(*(str + 1) - 0x40)));
#ifdef ENABLE_TTF
			if(use_ttf && !cache[pos])
			{
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE;
				ttf_cache(ttfh, (const byte *)str, (byte *)ccur);
				cache[pos] = 1;
				ccur += top * DISP_BOOK_CROWSIZE;
			}
			else
#endif
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CROWSIZE;
		
			for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_BOOK_FONTSIZE - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint --;
					}
					++ ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((* ccur) & b) != 0)
						* vpoint = color;
					vpoint --;
				}
				vaddr -= 512;
			}
			str += 2;
			count -= 2;
			x -= DISP_BOOK_FONTSIZE + wordspace * 2;
		}
		else if(*str > 0x1F)
		{
			if(x < 0)
			{
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2;
				y -= DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);

#ifdef ENABLE_TTF
			if(use_ttf)
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CFONTSIZE;
				for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((* ccur) & b) != 0)
								* vpoint = color;
							vpoint --;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > fbits_book_last; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint --;
					}
					vaddr -= 512;
				}
				x -= disp_ewidth[*str] + wordspace;
			}
			else
#endif
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_EFONTSIZE + top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								* vpoint = color;
							vpoint --;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							* vpoint = color;
						vpoint --;
					}
					vaddr -= 512;
				}
				x -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str ++;
			count --;
		}
		else
		{
			if(x < 0)
			{
				x = PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2;
				y -= DISP_BOOK_FONTSIZE;
			}
			str ++;
			count --;
			x -= DISP_BOOK_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringhorz(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot)
{
	pixel * vaddr;
	const byte * ccur, * cend;

	if(bot)
	{
		if(y >= bot)
			return;
		if(y + height > bot)
			height = bot - y;
	}

	while(*str != 0 && count > 0)
	{
		if(*str > 0x80)
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE)
			{
				x = 0;
				y += DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			dword pos = (((dword)(*str - 0x81)) * 0xBF + ((dword)(*(str + 1) - 0x40)));
#ifdef ENABLE_TTF
			if(use_ttf && !cache[pos])
			{
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE;
				ttf_cache(ttfh, (const byte *)str, (byte *)ccur);
				cache[pos] = 1;
				ccur += top * DISP_BOOK_CROWSIZE;
			}
			else
#endif
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CROWSIZE;
		
			for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_BOOK_FONTSIZE - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint ++;
					}
					++ ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((* ccur) & b) != 0)
						* vpoint = color;
					vpoint ++;
				}
				vaddr += 512;
			}
			str += 2;
			count -= 2;
			x += DISP_BOOK_FONTSIZE + wordspace * 2;
		}
		else if(*str > 0x1F)
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2)
			{
				x = 0;
				y += DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);

#ifdef ENABLE_TTF
			if(use_ttf)
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CFONTSIZE;
				for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((* ccur) & b) != 0)
								* vpoint = color;
							vpoint ++;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > fbits_book_last; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint ++;
					}
					vaddr += 512;
				}
				x += disp_ewidth[*str] + wordspace;
			}
			else
#endif
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_EFONTSIZE + top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								* vpoint = color;
							vpoint ++;
						}
						++ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							* vpoint = color;
						vpoint ++;
					}
					vaddr += 512;
				}
				x += DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str ++;
			count --;
		}
		else
		{
			if(x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2)
			{
				x = 0;
				y += DISP_BOOK_FONTSIZE;
			}
			str ++;
			count --;
			x += DISP_BOOK_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringlvert(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot)
{
	pixel * vaddr;
	const byte * ccur, * cend;

	if(bot)
	{
		if(x >= bot)
			return;
		if(x + height > bot)
			height = bot - x;
	}

	while(*str != 0 && count > 0)
	{
		if(*str > 0x80)
		{
			if(y < DISP_RSPAN + DISP_BOOK_FONTSIZE - 1)
			{
				y = 271;
				x += DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			dword pos = (((dword)(*str - 0x81)) * 0xBF + ((dword)(*(str + 1) - 0x40)));
#ifdef ENABLE_TTF
			if(use_ttf && !cache[pos])
			{
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE;
				ttf_cache(ttfh, (const byte *)str, (byte *)ccur);
				cache[pos] = 1;
				ccur += top * DISP_BOOK_CROWSIZE;
			}
			else
#endif
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CROWSIZE;
		
			for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_BOOK_FONTSIZE - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint -= 512;
					}
					++ ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((* ccur) & b) != 0)
						* vpoint = color;
					vpoint -= 512;
				}
				vaddr ++;
			}
			str += 2;
			count -= 2;
			y -= DISP_BOOK_FONTSIZE + wordspace * 2;
		}
		else if(*str > 0x1F)
		{
			if(y < DISP_RSPAN + DISP_BOOK_FONTSIZE - 1)
			{
				y = 271;
				x += DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);

#ifdef ENABLE_TTF
			if(use_ttf)
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CFONTSIZE;
				for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((* ccur) & b) != 0)
								* vpoint = color;
							vpoint -= 512;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > fbits_book_last; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint -= 512;
					}
					vaddr ++;
				}
				y -= disp_ewidth[*str] + wordspace;
			}
			else
#endif
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_EFONTSIZE + top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								* vpoint = color;
							vpoint -= 512;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							* vpoint = color;
						vpoint -= 512;
					}
					vaddr ++;
				}
				y -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str ++;
			count --;
		}
		else
		{
			if(y < DISP_RSPAN + DISP_BOOK_FONTSIZE - 1)
			{
				y = 271;
				x += DISP_BOOK_FONTSIZE;
			}
			str ++;
			count --;
			y -= DISP_BOOK_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_putnstringrvert(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot)
{
	pixel * vaddr;
	const byte * ccur, * cend;

	if(x < bot)
		return;
	if(x + 1 - height < bot)
		height = x + 1 - bot;

	while(*str != 0 && count > 0)
	{
		if(*str > 0x80)
		{
			if(y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE)
			{
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);
			dword pos = (((dword)(*str - 0x81)) * 0xBF + ((dword)(*(str + 1) - 0x40)));
#ifdef ENABLE_TTF
			if(use_ttf && !cache[pos])
			{
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE;
				ttf_cache(ttfh, (const byte *)str, (byte *)ccur);
				cache[pos] = 1;
				ccur += top * DISP_BOOK_CROWSIZE;
			}
			else
#endif
				ccur = book_cfont_buffer + pos * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CROWSIZE;
		
			for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
				int b;
				pixel * vpoint = vaddr;
				int bitsleft = DISP_BOOK_FONTSIZE - 8;
				while(bitsleft > 0)
				{
					for (b = 0x80; b > 0; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint += 512;
					}
					++ ccur;
					bitsleft -= 8;
				}
				for (b = 0x80; b > fbits_book_last; b >>= 1) {
					if (((* ccur) & b) != 0)
						* vpoint = color;
					vpoint += 512;
				}
				vaddr --;
			}
			str += 2;
			count -= 2;
			y += DISP_BOOK_FONTSIZE + wordspace * 2;
		}
		else if(*str > 0x1F)
		{
			if(y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2)
			{
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
			}
			vaddr = disp_get_vaddr(x, y);

#ifdef ENABLE_TTF
			if(use_ttf)
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_CFONTSIZE + top * DISP_BOOK_CFONTSIZE;
				for (cend = ccur + height * DISP_BOOK_CROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((* ccur) & b) != 0)
								* vpoint = color;
							vpoint += 512;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > fbits_book_last; b >>= 1) {
						if (((* ccur) & b) != 0)
							* vpoint = color;
						vpoint += 512;
					}
					vaddr --;
				}
				y += disp_ewidth[*str] + wordspace;
			}
			else
#endif
			{
				ccur = book_efont_buffer + ((dword)*str) * DISP_BOOK_EFONTSIZE + top * DISP_BOOK_EROWSIZE;
				for (cend = ccur + height * DISP_BOOK_EROWSIZE; ccur < cend; ccur ++) {
					int b;
					pixel * vpoint = vaddr;
					int bitsleft = DISP_BOOK_FONTSIZE / 2 - 8;
					while(bitsleft > 0)
					{
						for (b = 0x80; b > 0; b >>= 1) {
							if (((*ccur) & b) != 0)
								* vpoint = color;
							vpoint += 512;
						}
						++ ccur;
						bitsleft -= 8;
					}
					for (b = 0x80; b > febits_book_last; b >>= 1) {
						if (((*ccur) & b) != 0)
							* vpoint = color;
						vpoint += 512;
					}
					vaddr --;
				}
				y += DISP_BOOK_FONTSIZE / 2 + wordspace;
			}
			str ++;
			count --;
		}
		else
		{
			if(y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2)
			{
				y = 0;
				x -= DISP_BOOK_FONTSIZE;
			}
			str ++;
			count --;
			y += DISP_BOOK_FONTSIZE / 2 + wordspace;
		}
	}
}

extern void disp_fillvram(pixel color)
{
	pixel *vram, *vram_end;

	if(color == 0 || color == (pixel)-1)
		memset(vram_start, (color & 0xFF), 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	else
		for (vram = vram_start, vram_end = vram + 0x22000; vram < vram_end; vram ++)
			* vram = color;
}

extern void disp_fillrect(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	pixel * vsram, * vsram_end, * vram, * vram_end;
	dword wdwords;
	if(x1 > x2)
	{
		dword t = x1; x1 = x2; x2 = t;
	}
	if(y1 > y2)
	{
		dword t = y1; y1 = y2; y2 = t;
	}
	vsram = disp_get_vaddr(x1, y1);
	vsram_end = vsram + 512 * (y2 - y1);
	wdwords = (x2 - x1);
	for(;vsram <= vsram_end; vsram += 512)
		for(vram = vsram, vram_end = vram + wdwords; vram <= vram_end; vram ++)
			* vram = color;
}

extern void disp_rectangle(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	pixel *vsram, * vram, * vram_end;
	if(x1 > x2)
	{
		dword t = x1; x1 = x2; x2 = t;
	}
	if(y1 > y2)
	{
		dword t = y1; y1 = y2; y2 = t;
	}
	vsram = disp_get_vaddr(x1, y1);
	for(vram = vsram, vram_end = vram + (x2 - x1); vram < vram_end; vram ++)
		* vram = color;
	for(vram_end = vram + 512 * (y2 - y1); vram < vram_end; vram += 512)
		* vram = color;
	for(vram = vsram, vram_end = vram + 512 * (y2 - y1); vram < vram_end; vram += 512)
		* vram = color;
	for(vram_end = vram + (x2 - x1); vram < vram_end; vram ++)
		* vram = color;
	* vram = color;
}

extern void disp_line(dword x1, dword y1, dword x2, dword y2, pixel color)
{
	pixel * vram;
	int dy, dx, x, y, d;
	dx = (int)x2 - (int)x1;
	dy = (int)y2 - (int)y1;
	if(dx < 0)
		dx = -dx;
	if(dy < 0)
		dy = -dy;
	d = -dx;
	x = (int)x1;
	y = (int)y1;
	if(dx >= dy)
	{
		if(y2 < y1)
		{
			dword t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t;
		}
		vram = disp_get_vaddr(x1, y1);
		if(x1 < x2)
		{
			for(x = x1; x <= x2; x ++)
			{
				if(d > 0)
				{
					y ++;
					vram += 512;
					d -= 2 * dx;
				}
				d += 2 * dy;
				* vram = color;
				vram ++;
			}
		}
		else
		{
			for(x = x1; x >= x2; x --)
			{
				if(d > 0)
				{
					y ++;
					vram += 512;
					d -= 2 * dx;
				}
				d += 2 * dy;
				* vram = color;
				vram --;
			}
		}
	}
	else
	{
		if(x2 < x1)
		{
			dword t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t;
		}
		vram = disp_get_vaddr(x1, y1);
		if(y1 < y2)
		{
			for(y = y1; y <= y2; y ++)
			{
				if(d > 0)
				{
					x ++;
					vram ++;
					d -= 2 * dy;
				}
				d += 2 * dx;
				* vram = color;
				vram += 512;
			}
		}
		else
		{
			for(y = y1; y >= y2; y --)
			{
				if(d > 0)
				{
					x ++;
					vram ++;
					d -= 2 * dy;
				}
				d += 2 * dx;
				* vram = color;
				vram -= 512;
			}
		}
	}
}
