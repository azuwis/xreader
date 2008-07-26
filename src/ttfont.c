#include "config.h"

#ifdef ENABLE_TTF

#include <pspkernel.h>
#include <malloc.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_SYNTHESIS_H
#include "freetype/ftlcdfil.h"
#include "charsets.h"
#include "ttfont.h"
#include "display.h"
#include "charsets.h"
#include "pspscreen.h"
#include "conf.h"
#include "dbg.h"
#include "scene.h"
#include "strsafe.h"
#include "power.h"

extern t_conf config;

static SceUID ttf_sema = -1;

static p_ttf ttf_open_file_to_memory(const char *filename, int size,
									 const char *ttfname)
{
	p_ttf ttf;
	byte *buf;
	int fd, fileSize;

	if (filename == NULL || size == 0)
		return NULL;

	fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		return NULL;
	}

	fileSize = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	buf = malloc(fileSize);

	if (buf == NULL) {
		sceIoClose(fd);
		return NULL;
	}

	sceIoRead(fd, buf, fileSize);
	sceIoClose(fd);

	ttf = ttf_open_buffer(buf, fileSize, size, ttfname);

	if (ttf == NULL) {
		free(buf);
	}

	return ttf;
}

/**
 * ��TTF����
 * @param filename TTF�ļ���
 * @param size Ԥ��������С
 * @param load2mem �Ƿ���ص��ڴ�
 * @return ����TTF��ָ��
 * - NULL ʧ��
 */
extern p_ttf ttf_open(const char *filename, int size, bool load2mem)
{
	p_ttf ttf;

	if (filename == NULL || size == 0)
		return NULL;

	if (load2mem)
		ttf = ttf_open_file_to_memory(filename, size, filename);
	else
		ttf = ttf_open_file(filename, size, filename);

	return ttf;
}

/**
 * ��TTF��������
 * @param ttfpath TTF����·��
 * @param pixelSize Ԥ��������С
 * @param ttfName TTF������
 * @return ����TTF��ָ��
 * - NULL ʧ��
 */
extern p_ttf ttf_open_file(const char *ttfpath, int pixelSize,
						   const char *ttfName)
{
	int i;
	p_ttf ttf;

	if (ttfpath == NULL || ttfName == NULL)
		return NULL;

	if ((ttf = calloc(1, sizeof(*ttf))) == NULL) {
		return NULL;
	}

	memset(ttf, 0, sizeof(t_ttf));

	if (strrchr(ttfName, '/') == NULL)
		ttf->fontName = strdup(ttfName);
	else {
		ttf->fontName = strdup(strrchr(ttfName, '/') + 1);
	}

	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf);
		return NULL;
	}

	STRCPY_S(ttf->fnpath, ttfpath);
	if (FT_New_Face(ttf->library, ttf->fnpath, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf);
		return NULL;
	}

	for (i = 0; i < SBIT_HASH_SIZE; ++i) {
		memset(&ttf->sbitHashRoot[i], 0, sizeof(SBit_HashItem));
	}

	ttf->cacheSize = 0;
	ttf->cachePop = 0;
	ttf_set_pixel_size(ttf, pixelSize);

	return ttf;
}

/**
 * ��TTF��������
 * @param ttfBuf TTF��������
 * @param ttfLength TTF�������ݴ�С�����ֽڼ�
 * @param pixelSize Ԥ��������С
 * @param ttfName TTF������
 * @return ����TTF��ָ��
 * - NULL ʧ��
 */
extern p_ttf ttf_open_buffer(void *ttfBuf, size_t ttfLength, int pixelSize,
							 const char *ttfName)
{
	int i;
	p_ttf ttf;

	if (ttfBuf == NULL || ttfLength == 0 || ttfName == NULL)
		return NULL;

	if ((ttf = calloc(1, sizeof(*ttf))) == NULL) {
		return NULL;
	}

	memset(ttf, 0, sizeof(t_ttf));

	STRCPY_S(ttf->fnpath, "");
	ttf->fileBuffer = ttfBuf;
	ttf->fileSize = ttfLength;

	if (strrchr(ttfName, '/') == NULL)
		ttf->fontName = strdup(ttfName);
	else {
		ttf->fontName = strdup(strrchr(ttfName, '/') + 1);
	}

	if (FT_Init_FreeType(&ttf->library) != 0) {
		free(ttf);
		return NULL;
	}

	if (FT_New_Memory_Face
		(ttf->library, ttf->fileBuffer, ttf->fileSize, 0, &ttf->face)
		!= 0) {
		FT_Done_FreeType(ttf->library);
		free(ttf);
		return NULL;
	}

	for (i = 0; i < SBIT_HASH_SIZE; ++i) {
		memset(&ttf->sbitHashRoot[i], 0, sizeof(SBit_HashItem));
	}
	ttf->cacheSize = 0;
	ttf->cachePop = 0;

	ttf_set_pixel_size(ttf, pixelSize);

	return ttf;
}

/**
 * �ر�TTF�ṹ
 * @param ttf ttfָ��
 */
extern void ttf_close(p_ttf ttf)
{
	int i;

	if (ttf == NULL)
		return;

	if (ttf->fontName != NULL) {
		free(ttf->fontName);
		ttf->fontName = NULL;
	}

	if (ttf->fileBuffer != NULL) {
		free(ttf->fileBuffer);
		ttf->fileBuffer = NULL;
	}

	for (i = 0; i < SBIT_HASH_SIZE; ++i) {
		if (ttf->sbitHashRoot[i].bitmap.buffer) {
			free(ttf->sbitHashRoot[i].bitmap.buffer);
			ttf->sbitHashRoot[i].bitmap.buffer = NULL;
		}
	}

	FT_Done_Face(ttf->face);
	FT_Done_FreeType(ttf->library);

	free(ttf);
}

/**
 * ����TTF�����С
 * @param ttf ttfָ��
 * @param size �����С
 * @return �Ƿ�ɹ�
 */
extern bool ttf_set_pixel_size(p_ttf ttf, int size)
{
	if (ttf == NULL)
		return false;

	if (FT_Set_Pixel_Sizes(ttf->face, 0, size) != 0) {
		return false;
	}

	ttf->pixelSize = size;
	return true;
}

/**
 * ����TTF�Ƿ����ÿ����Ч��
 * @param ttf ttfָ��
 * @param aa �Ƿ����ÿ����Ч��
 */
extern void ttf_set_anti_alias(p_ttf ttf, bool aa)
{
	if (ttf == NULL)
		return;

	ttf->antiAlias = aa;
}

/**
 * ����TTF�Ƿ�����ClearType(Sub-Pixel LCD�Ż�Ч��)
 * @param ttf ttfָ��
 * @param cleartype �Ƿ�����cleartype
 */
extern void ttf_set_cleartype(p_ttf ttf, bool cleartype)
{
	if (ttf == NULL)
		return;

	if (cleartype)
		FT_Library_SetLcdFilter(ttf->library, FT_LCD_FILTER_DEFAULT);
	ttf->cleartype = cleartype;
}

/**
 * ����TTF�Ƿ���������Ӵ�
 * @param ttf ttfָ��
 * @param embolden �Ƿ���������Ӵ�
 */
extern void ttf_set_embolden(p_ttf ttf, bool embolden)
{
	if (ttf == NULL)
		return;

	ttf->embolden = embolden;
}

/**
 * ������ε�ttf���ͻ���
 */
static void sbitCacheAdd(p_ttf ttf, unsigned long ucsCode, int glyphIndex,
						 FT_Bitmap * bitmap, int left, int top, int xadvance,
						 int yadvance)
{
	int addIndex = 0;

	if (ttf->cacheSize < SBIT_HASH_SIZE) {
		addIndex = ttf->cacheSize++;
	} else {
		addIndex = ttf->cachePop++;
		if (ttf->cachePop == SBIT_HASH_SIZE)
			ttf->cachePop = 0;
	}

	SBit_HashItem *item = &ttf->sbitHashRoot[addIndex];

	if (item->bitmap.buffer) {
		free(item->bitmap.buffer);
		item->bitmap.buffer = 0;
	}

	item->ucs_code = ucsCode;
	item->glyph_index = glyphIndex;
	item->size = ttf->pixelSize;
	item->anti_alias = ttf->antiAlias;
	item->cleartype = ttf->cleartype;
	item->embolden = ttf->embolden;
	item->xadvance = xadvance;
	item->yadvance = yadvance;
	item->bitmap.width = bitmap->width;
	item->bitmap.height = bitmap->rows;
	item->bitmap.left = left;
	item->bitmap.top = top;
	item->bitmap.pitch = bitmap->pitch;
	item->bitmap.format = bitmap->pixel_mode;
	item->bitmap.max_grays = (bitmap->num_grays - 1);

	int pitch = abs(bitmap->pitch);

	if (pitch * bitmap->rows > 0) {
		item->bitmap.buffer =
			(unsigned char *) memalign(64, pitch * bitmap->rows);
		if (item->bitmap.buffer) {
			memcpy(item->bitmap.buffer, bitmap->buffer, pitch * bitmap->rows);
		}
	}
}

/**
 * ��TTF���λ����в������� 
 */
static SBit_HashItem *sbitCacheFind(p_ttf ttf, unsigned long ucsCode,
									int format)
{
	int i;

	for (i = 0; i < ttf->cacheSize; i++) {
		if ((ttf->sbitHashRoot[i].ucs_code == ucsCode) &&
			(ttf->sbitHashRoot[i].size == ttf->pixelSize) &&
			(ttf->sbitHashRoot[i].anti_alias == ttf->antiAlias) &&
			(ttf->sbitHashRoot[i].cleartype == ttf->cleartype) &&
			(ttf->sbitHashRoot[i].embolden == ttf->embolden) &&
			(ttf->sbitHashRoot[i].bitmap.format == format)
			)
			return (&ttf->sbitHashRoot[i]);
	}
	return NULL;
}

static FT_Render_Mode get_render_mode(p_ttf ttf, bool isVertical)
{
	if (ttf->cleartype && isVertical)
		return FT_RENDER_MODE_LCD_V;
	if (ttf->cleartype && !isVertical)
		return FT_RENDER_MODE_LCD;
	if (ttf->antiAlias)
		return FT_RENDER_MODE_NORMAL;

	return FT_RENDER_MODE_MONO;
}

static FT_Pixel_Mode get_pixel_mode(FT_Render_Mode mode)
{
	switch (mode) {
		case FT_RENDER_MODE_LCD:
			return FT_PIXEL_MODE_LCD;
		case FT_RENDER_MODE_LCD_V:
			return FT_PIXEL_MODE_LCD_V;
		case FT_RENDER_MODE_NORMAL:
			return FT_PIXEL_MODE_GRAY;
		case FT_RENDER_MODE_MONO:
			return FT_PIXEL_MODE_MONO;
		default:
			break;
	}

	return FT_PIXEL_MODE_NONE;
}

/**
 * 8λ��ͨ��alpha����㷨
 * @param wpSrc Դ��ɫָ��
 * @param wpDes Ŀ����ɫָ��
 * @param wAlpha alphaֵ(0-255)
 * @note Ŀ����ɫ = Ŀ����ɫ * alpha + ( 1 - alpha ) * Դ��ɫ
 */
static inline void MakeAlpha(byte * wpSrc, byte * wpDes, byte wAlpha)
{
	word result;

	if (*wpDes == *wpSrc && wAlpha == 255)
		return;

	if (wAlpha == 0)
		*wpDes = *wpSrc;

	result = wAlpha * (*wpDes) + (255 - wAlpha) * (*wpSrc);

	*wpDes = result / 255;
}

/** 
 * ����TTF���͵���Ļ��ˮƽ�汾
 * @param buffer ����λͼ����
 * @param format ��ʽ
 * @param width λͼ���
 * @param height λͼ�߶�
 * @param pitch λͼ��λ�����ֽڼǣ�
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param scr_width ��ͼ�������
 * @param scr_height ��ͼ�����߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void _drawBitmap_horz(byte * buffer, int format, int width, int height,
							 int pitch, FT_Int x, FT_Int y,
							 int scr_width, int scr_height, pixel color)
{
	FT_Int i, j;
	pixel pix, grey;
	byte ra, ga, ba;
	byte rd, gd, bd;
	byte rs, gs, bs;

	if (!buffer)
		return;
//  dbg_printf(d, "%s: %d, %d, %d", __func__, x, y, scr_height);
	if (format == FT_PIXEL_MODE_MONO) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (i + x < 0 || i + x >= scr_width || j + y < 0
					|| j + y >= scr_height)
					continue;
				if (buffer[j * pitch + i / 8] & (0x80 >> (i % 8)))
					*(pixel *) disp_get_vaddr((i + x), (j + y)) = (color);
			}
	} else if (format == FT_PIXEL_MODE_GRAY) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (i + x < 0 || i + x >= scr_width || j + y < 0
					|| j + y >= scr_height)
					continue;
				grey = buffer[j * pitch + i];

				if (grey) {
					pix =
						disp_grayscale(*disp_get_vaddr
									   ((i + x), (j + y)),
									   RGB_R(color),
									   RGB_G(color),
									   RGB_B(color), grey * 100 / 255);
					*(pixel *) disp_get_vaddr((i + x), (j + y)) = (pix);
				}
			}
	} else if (format == FT_PIXEL_MODE_LCD) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width / 3; i++) {
				if (i + x < 0 || i + x >= scr_width || j + y < 0
					|| j + y >= scr_height)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((i + x), (j + y));

				ra = buffer[j * pitch + i * 3];
				ga = buffer[j * pitch + i * 3 + 1];
				ba = buffer[j * pitch + i * 3 + 2];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((i + x), (j + y)) = (RGB(rd, gd, bd));
			}
	} else if (format == FT_PIXEL_MODE_LCD_V) {
		for (j = 0; j < height / 3; j++)
			for (i = 0; i < width; i++) {
				if (i + x < 0 || i + x >= scr_width || j + y < 0
					|| j + y >= scr_height)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((i + x), (j + y));

				ra = buffer[j * 3 * pitch + i];
				ga = buffer[(j * 3 + 1) * pitch + i];
				ba = buffer[(j * 3 + 2) * pitch + i];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((i + x), (j + y)) = (RGB(rd, gd, bd));
			}
	}
}

/** 
 * ����TTF���ͻ��浽��Ļ��ˮƽ�汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void drawCachedBitmap_horz(Cache_Bitmap * sbt, FT_Int x, FT_Int y,
								  int width, int height, pixel color)
{
	_drawBitmap_horz(sbt->buffer, sbt->format, sbt->width, sbt->height,
					 sbt->pitch, x, y, width, height, color);
}

/** 
 * ����TTF���͵���Ļ��ˮƽ�汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void drawBitmap_horz(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
							int width, int height, pixel color)
{
	if (!bitmap->buffer)
		return;

	_drawBitmap_horz(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
					 bitmap->rows, bitmap->pitch, x, y, width, height, color);
}

#define DISP_RSPAN 0

/**
 * ��*str��ȡ��һ���֣�����/Ӣ����ĸ�����л��ƣ�ˮƽ�汾
 * @param ttf ʹ�õ�TTF����
 * @param *x ָ��X�����ָ��
 * @param *y ָ��Y�����ָ�� 
 * @param color ������ɫ
 * @param str �ַ���ָ��ָ��
 * @param count ����ָ��
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @param top ��������Ŀ�ʼ�У�����������Ͳ�������Ļ���⣬��Ϊ��0ֵ
 * @param height ��������ĸ߶ȣ�����������β�������Ļ���⣬
 * 				 �򲻵���DISP_BOOK_FONTSIZE�������ڱ��ü���ĸ߶�
 * @param bot �����Ƹ߶�
 * @param previous ָ����һ��ͬ���ַ�ָ��
 * @param is_hanzi �Ƿ�Ϊ����
 * @note ���������һ�����ͣ���*x, *y, *count, *str���ᱻ����
 * <br> ������ڻ��Ƶ�һ�У�bot = 0������ڻ��������У�bot = �����Ƹ߶�
 */
static void ttf_disp_putnstring_horz(p_ttf ttf, int *x, int *y, pixel color,
									 const byte ** str, int *count,
									 dword wordspace, int top, int height,
									 int bot, FT_UInt * previous, bool is_hanzi)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;

	FT_Bool useKerning;

	useKerning = FT_HAS_KERNING(ttf->face);
	word ucs = charsets_gbk_to_ucs(*str);
	SBit_HashItem *cache =
		sbitCacheFind(ttf, ucs, get_pixel_mode(get_render_mode(ttf, false)));

	if (cache) {
		if (useKerning && *previous && cache->glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, cache->glyph_index,
						   FT_KERNING_DEFAULT, &delta);
			*x += delta.x >> 6;
		}
		drawCachedBitmap_horz(&cache->bitmap, *x + cache->bitmap.left,
							  bot ? *y + DISP_BOOK_FONTSIZE -
							  cache->bitmap.top : *y + height -
							  cache->bitmap.top,
							  PSP_SCREEN_WIDTH,
							  bot ? bot : PSP_SCREEN_HEIGHT, color);
		*x += cache->xadvance >> 6;
		*previous = cache->glyph_index;
	} else {
		glyphIndex = FT_Get_Char_Index(ttf->face, ucs);
		// disable hinting when loading chinese characters
		if (is_hanzi)
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_NO_HINTING);
		else
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_DEFAULT);
		if (error)
			return;
		if (ttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			error =
				FT_Render_Glyph(ttf->face->glyph, get_render_mode(ttf, false));
			if (error) {
				return;
			}
		}
		slot = ttf->face->glyph;

		if (ttf->embolden)
			FT_GlyphSlot_Embolden(slot);

		if (useKerning && *previous && glyphIndex) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, glyphIndex,
						   FT_KERNING_DEFAULT, &delta);
			*x += delta.x >> 6;
		}
		drawBitmap_horz(&slot->bitmap, *x + slot->bitmap_left,
						bot ? *y + DISP_BOOK_FONTSIZE -
						slot->bitmap_top : *y + height -
						slot->bitmap_top, PSP_SCREEN_WIDTH,
						bot ? bot : PSP_SCREEN_HEIGHT, color);
		*x += slot->advance.x >> 6;
		*previous = glyphIndex;

		sbitCacheAdd(ttf, ucs, glyphIndex,
					 &slot->bitmap, slot->bitmap_left, slot->bitmap_top,
					 slot->advance.x, slot->advance.y);
	}
	if (is_hanzi) {
		(*str) += 2;
		*count -= 2;
		*x += wordspace * 2;
	} else {
		(*str)++;
		(*count)--;
		*x += wordspace;
	}
}

extern bool bytetable[256];

/**
 * �õ��ַ���������ʾ��maxpixels�еĳ���
 * @param cttf ����TTF����
 * @param ettf Ӣ��TTF����
 * @param str �ַ���
 * @param maxpixels ��󳤶�
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @note ����������У����ַ�������ֹͣ�ۼӡ�
 * <br>  ����ַ������Ƴ��ȣ�maxpixels�����ַ�������ֹͣ�ۼӡ�
 * <br>  ����汾�ٶȼ�������Ҫʹ��
 */
extern int ttf_get_string_width_hard(p_ttf cttf, p_ttf ettf, const byte * str,
									 dword maxpixels, dword wordspace)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;
	FT_Bool useKerning;
	FT_UInt cprevious = 0, eprevious = 0;
	int x = 0, count = 0;

	if (str == NULL || maxpixels == 0 || cttf == NULL || ettf == NULL)
		return 0;

	while (*str != 0 && x < maxpixels && bytetable[*(byte *) str] != 1) {
		if (*str > 0x80) {
			useKerning = FT_HAS_KERNING(cttf->face);
			word ucs = charsets_gbk_to_ucs(str);
			SBit_HashItem *cache = sbitCacheFind(cttf, ucs,
												 get_pixel_mode(get_render_mode
																(cttf, false)));
			if (cache) {
				if (useKerning && cprevious && cache->glyph_index) {
					FT_Vector delta;

					FT_Get_Kerning(cttf->face, cprevious,
								   cache->glyph_index,
								   FT_KERNING_DEFAULT, &delta);
					x += delta.x >> 6;
					if (x > maxpixels)
						break;
				}
				x += cache->xadvance >> 6;
				if (x > maxpixels)
					break;
				cprevious = cache->glyph_index;
			} else {
				glyphIndex = FT_Get_Char_Index(cttf->face, ucs);
				// disable hinting when loading chinese characters
				error =
					FT_Load_Glyph(cttf->face, glyphIndex, FT_LOAD_NO_HINTING);
				if (error) {
					return count;
				}
				if (cttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
					if (cttf->cleartype) {
						error =
							FT_Render_Glyph(cttf->face->
											glyph, FT_RENDER_MODE_LCD);
					} else if (cttf->antiAlias) {
						error =
							FT_Render_Glyph(cttf->face->
											glyph, FT_RENDER_MODE_NORMAL);
					} else
						error =
							FT_Render_Glyph(cttf->face->
											glyph, FT_RENDER_MODE_MONO);
					if (error) {
						return count;
					}
				}
				slot = cttf->face->glyph;

				if (cttf->embolden)
					FT_GlyphSlot_Embolden(slot);

				if (useKerning && cprevious && glyphIndex) {
					FT_Vector delta;

					FT_Get_Kerning(cttf->face, cprevious,
								   glyphIndex, FT_KERNING_DEFAULT, &delta);
					x += delta.x >> 6;
					if (x > maxpixels)
						break;
				}
				x += slot->advance.x >> 6;
				if (x > maxpixels)
					break;
				cprevious = glyphIndex;

				sbitCacheAdd(cttf, ucs, glyphIndex,
							 &slot->bitmap, slot->bitmap_left,
							 slot->bitmap_top, slot->advance.x,
							 slot->advance.y);
			}
			if (x > maxpixels)
				break;
			x += wordspace * 2;
			(str) += 2;
			(count) += 2;
		} else if (*str > 0x1F) {
			useKerning = FT_HAS_KERNING(ettf->face);
			word ucs = charsets_gbk_to_ucs(str);
			SBit_HashItem *cache = sbitCacheFind(ettf, ucs,
												 get_pixel_mode(get_render_mode
																(ettf, false)));

			if (cache) {
				if (useKerning && eprevious && cache->glyph_index) {
					FT_Vector delta;

					FT_Get_Kerning(ettf->face, eprevious,
								   cache->glyph_index,
								   FT_KERNING_DEFAULT, &delta);
					x += delta.x >> 6;
					if (x > maxpixels)
						break;
				}
				x += cache->xadvance >> 6;
				if (x > maxpixels)
					break;
				eprevious = cache->glyph_index;
			} else {
				glyphIndex = FT_Get_Char_Index(ettf->face, ucs);
				// disable hinting when loading chinese characters
				error = FT_Load_Glyph(ettf->face, glyphIndex, FT_LOAD_DEFAULT);
				if (error) {
					return count;
				}
				if (ettf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
					if (ettf->cleartype) {
						error =
							FT_Render_Glyph(ettf->face->
											glyph, FT_RENDER_MODE_LCD);
					} else if (ettf->antiAlias) {
						error =
							FT_Render_Glyph(ettf->face->
											glyph, FT_RENDER_MODE_NORMAL);
					} else
						error =
							FT_Render_Glyph(ettf->face->
											glyph, FT_RENDER_MODE_MONO);
					if (error) {
						return count;
					}
				}
				slot = ettf->face->glyph;

				if (ettf->embolden)
					FT_GlyphSlot_Embolden(slot);

				if (useKerning && eprevious && glyphIndex) {
					FT_Vector delta;

					FT_Get_Kerning(ettf->face, eprevious,
								   glyphIndex, FT_KERNING_DEFAULT, &delta);
					x += delta.x >> 6;
					if (x > maxpixels)
						break;
				}
				x += slot->advance.x >> 6;
				if (x > maxpixels)
					break;
				eprevious = glyphIndex;

				sbitCacheAdd(ettf, ucs, glyphIndex,
							 &slot->bitmap, slot->bitmap_left,
							 slot->bitmap_top, slot->advance.x,
							 slot->advance.y);
			}
			if (x > maxpixels)
				break;
			x += wordspace;
			(str)++;
			(count)++;
		} else {
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x += DISP_BOOK_FONTSIZE / 2 + wordspace;
			if (x > maxpixels)
				break;
			str++;
			(count)++;
		}
	}
	if (bytetable[*(byte *) str] == 1) {
		if (*str == '\r' && *(str + 1) == '\n') {
			count += 2;
		} else {
			count++;
		}
	}

	return count;
}

static int ttf_get_char_width(p_ttf cttf, const byte * str)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;
	FT_Bool useKerning;
	int x = 0;

	if (str == NULL)
		return 0;
	if (cttf == NULL)
		return DISP_BOOK_FONTSIZE;

	useKerning = FT_HAS_KERNING(cttf->face);
	word ucs = charsets_gbk_to_ucs(str);

	glyphIndex = FT_Get_Char_Index(cttf->face, ucs);
	// disable hinting when loading chinese characters
	error = FT_Load_Glyph(cttf->face, glyphIndex, FT_LOAD_NO_HINTING);
	if (error)
		return x;
	if (cttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
		if (cttf->cleartype) {
			error = FT_Render_Glyph(cttf->face->glyph, FT_RENDER_MODE_LCD);
		} else if (cttf->antiAlias) {
			error = FT_Render_Glyph(cttf->face->glyph, FT_RENDER_MODE_NORMAL);
		} else
			error = FT_Render_Glyph(cttf->face->glyph, FT_RENDER_MODE_MONO);
		if (error) {
			return x;
		}
	}
	slot = cttf->face->glyph;

	if (cttf->embolden)
		FT_GlyphSlot_Embolden(slot);

	x += slot->advance.x >> 6;

	return x;
}

/**
 * �õ��ַ���������ʾ��maxpixels�еĳ���
 * @param cttf ����TTF����
 * @param ettf Ӣ��TTF����
 * @param str �ַ���
 * @param maxpixels ������س���
 * @param maxbytes ����ַ����ȣ����ֽڼ�
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @return �ַ����������������ֽڼ�
 * @note ����������У����ַ�������ֹͣ�ۼӡ�
 * <br>  ����ַ������Ƴ��ȣ�maxpixels�����ַ�������ֹͣ�ۼӡ�
 * <br>  ����汾�ٶȿ죬������Ӣ����ĸ�����г�������
 */
extern int ttf_get_string_width(p_ttf cttf, p_ttf ettf, const byte * str,
								dword maxpixels, dword maxbytes,
								dword wordspace)
{
	dword width = 0;
	const byte *ostr = str;
	static int hanzi_len, hanzi_size = 0;
	dword bytes = 0;

	if (hanzi_len == 0 || hanzi_size != DISP_BOOK_FONTSIZE) {
		hanzi_len = ttf_get_char_width(cttf, (const byte *) "��");
		hanzi_size = DISP_BOOK_FONTSIZE;
	}

	while (*str != 0 && width <= maxpixels && bytes < maxbytes
		   && bytetable[*str] != 1) {
		if (*str > 0x80) {
			width += hanzi_len;
			if (width > maxpixels)
				break;
			width += wordspace * 2;
			str += 2;
		} else if (*str == 0x20) {
			width += DISP_BOOK_FONTSIZE / 2;
			if (width > maxpixels)
				break;
			width += wordspace;
			str++;
		} else {
			if (*str == 0x09) {
				int j;

				for (j = 0; j < config.tabstop; ++j)
					width += DISP_BOOK_FONTSIZE / 2;
			} else
				width += disp_ewidth[*str];
			if (width > maxpixels)
				break;
			width += wordspace;
			str++;
		}
		bytes++;
	}

	return str - ostr;
}

extern void disp_putnstring_horz_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										  pixel color, const byte * str,
										  int count, dword wordspace, int top,
										  int height, int bot)
{
	if (cttf == NULL || ettf == NULL)
		return;

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
//      dbg_printf(d, "%s: bot now: height: %d bot %d", __func__, height, bot);
	}

	FT_UInt cprevious, eprevious;

	cprevious = eprevious = 0;

	dword cpu, bus;

	power_get_clock(&cpu, &bus);
	if (cpu < 222)
		power_set_clock(222, 111);
	ttf_set_pixel_size(ettf, height);
	ttf_set_pixel_size(cttf, height);
	while (*str != 0 && count > 0) {
		if (!check_range(x, y)) {
			scene_power_save(true);
			return;
		}
		if (*str > 0x80) {
			ttf_disp_putnstring_horz(cttf, &x, &y, color, &str,
									 &count, wordspace, top, height,
									 bot, &cprevious, true);
		} else if (*str > 0x1F && *str != 0x20) {
			ttf_disp_putnstring_horz(ettf, &x, &y, color, &str,
									 &count, wordspace, top, height,
									 bot, &eprevious, false);
		} else {
			if (x > PSP_SCREEN_WIDTH - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
#if 0
				x = 0;
				y += DISP_BOOK_FONTSIZE;
#endif
			}
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x += DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
	scene_power_save(true);
}

/**
 * �õ�Ӣ����ĸ�����Ϣ
 */
extern void ttf_load_ewidth(p_ttf ttf, byte * ewidth, int size)
{
	if (ttf == NULL || ewidth == NULL || size == 0)
		return;

	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;
	FT_Bool useKerning;
	FT_UInt eprevious = 0;
	byte width;
	word ucs;

	for (ucs = 0; ucs < size; ++ucs) {
		width = 0;
		useKerning = FT_HAS_KERNING(ttf->face);
		glyphIndex = FT_Get_Char_Index(ttf->face, ucs);
		error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_DEFAULT);
		if (error) {
			return;
		}
		if (ttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			if (ttf->cleartype) {
				error = FT_Render_Glyph(ttf->face->glyph, FT_RENDER_MODE_LCD);
			} else if (ttf->antiAlias) {
				error =
					FT_Render_Glyph(ttf->face->glyph, FT_RENDER_MODE_NORMAL);
			} else
				error = FT_Render_Glyph(ttf->face->glyph, FT_RENDER_MODE_MONO);
			if (error) {
				return;
			}
		}
		slot = ttf->face->glyph;

		if (ttf->embolden)
			FT_GlyphSlot_Embolden(slot);

		if (useKerning && eprevious && glyphIndex) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, eprevious, glyphIndex,
						   FT_KERNING_DEFAULT, &delta);
			width += delta.x >> 6;
		}
		width += slot->advance.x >> 6;
		eprevious = glyphIndex;

		// Add width to length
		ewidth[ucs] = width;
	}

	dbg_printf(d, "%s: OK. 'i' width is %d, 'M' width is %d", __func__,
			   ewidth['i'], ewidth['M']);
}

/** 
 * ����TTF���͵���Ļ���ߵ��汾
 * @param buffer ����λͼ����
 * @param format ��ʽ
 * @param width λͼ���
 * @param height λͼ�߶�
 * @param pitch λͼ��λ�����ֽڼǣ�
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param scr_width ��ͼ�������
 * @param scr_height ��ͼ�����߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void _drawBitmap_reversal(byte * buffer, int format, int width,
								 int height, int pitch, FT_Int x, FT_Int y,
								 int scr_width, int scr_height, pixel color)
{
	FT_Int i, j;
	pixel pix, grey;
	byte ra, ga, ba;
	byte rd, gd, bd;
	byte rs, gs, bs;

	if (!buffer)
		return;
	if (format == FT_PIXEL_MODE_MONO) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (x - i < 0 || x - i >= scr_width
					|| y - j < PSP_SCREEN_HEIGHT - scr_height
					|| y - j >= PSP_SCREEN_HEIGHT)
					continue;
				if (buffer[j * pitch + i / 8] & (0x80 >> (i % 8)))
					*(pixel *) disp_get_vaddr((x - i), (y - j)) = (color);
			}
	} else if (format == FT_PIXEL_MODE_GRAY) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (x - i < 0 || x - i >= scr_width
					|| y - j < PSP_SCREEN_HEIGHT - scr_height
					|| y - j >= PSP_SCREEN_HEIGHT)
					continue;
				grey = buffer[j * pitch + i];

				if (grey) {
					pix =
						disp_grayscale(*disp_get_vaddr
									   ((x - i), (y - j)),
									   RGB_R(color),
									   RGB_G(color),
									   RGB_B(color), grey * 100 / 255);
					*(pixel *) disp_get_vaddr((x - i), (y - j)) = (pix);
				}
			}
	} else if (format == FT_PIXEL_MODE_LCD) {
//      dbg_printf(d, "%s: %d, %d, %d", __func__, x, y, scr_height);
		for (j = 0; j < height; j++)
			for (i = 0; i < width / 3; i++) {
				if (x - i < 0 || x - i >= scr_width
					|| y - j < PSP_SCREEN_HEIGHT - scr_height
					|| y - j >= PSP_SCREEN_HEIGHT)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x - i), (y - j));

				ra = buffer[j * pitch + i * 3];
				ga = buffer[j * pitch + i * 3 + 1];
				ba = buffer[j * pitch + i * 3 + 2];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x - i), (y - j)) = (RGB(rd, gd, bd));
			}
	} else if (format == FT_PIXEL_MODE_LCD_V) {
		for (j = 0; j < height / 3; j++)
			for (i = 0; i < width; i++) {
				if (x - i < 0 || x - i >= scr_width
					|| y - j < PSP_SCREEN_HEIGHT - scr_height
					|| y - j >= PSP_SCREEN_HEIGHT)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x - i), (y - j));

				ra = buffer[j * 3 * pitch + i];
				ga = buffer[(j * 3 + 1) * pitch + i];
				ba = buffer[(j * 3 + 2) * pitch + i];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x - i), (y - j)) = (RGB(rd, gd, bd));
			}
	}
}

/** 
 * ����TTF���ͻ��浽��Ļ���ߵ��汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void drawCachedBitmap_reversal(Cache_Bitmap * sbt, FT_Int x, FT_Int y,
									  int width, int height, pixel color)
{
	_drawBitmap_reversal(sbt->buffer, sbt->format, sbt->width, sbt->height,
						 sbt->pitch, x, y, width, height, color);
}

/** 
 * ����TTF���͵���Ļ���ߵ��汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void drawBitmap_reversal(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
								int width, int height, pixel color)
{
	if (!bitmap->buffer)
		return;

	_drawBitmap_reversal(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
						 bitmap->rows, bitmap->pitch, x, y, width, height,
						 color);
}

/**
 * ��*str��ȡ��һ���֣�����/Ӣ����ĸ�����л��ƣ��ߵ��汾
 * @param ttf ʹ�õ�TTF����
 * @param *x ָ��X�����ָ��
 * @param *y ָ��Y�����ָ�� 
 * @param color ������ɫ
 * @param str �ַ���ָ��ָ��
 * @param count ����ָ��
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @param top ��������Ŀ�ʼ�У�����������Ͳ�������Ļ���⣬��Ϊ��0ֵ
 * @param height ��������ĸ߶ȣ�����������β�������Ļ���⣬
 * 				 �򲻵���DISP_BOOK_FONTSIZE�������ڱ��ü���ĸ߶�
 * @param bot �����Ƹ߶�
 * @param previous ָ����һ��ͬ���ַ�ָ��
 * @param is_hanzi �Ƿ�Ϊ����
 * @note ���������һ�����ͣ���*x, *y, *count, *str���ᱻ����
 * <br> ������ڻ��Ƶ�һ�У�bot = 0������ڻ��������У�bot = �����Ƹ߶�
 */
static void ttf_disp_putnstring_reversal(p_ttf ttf, int *x, int *y, pixel color,
										 const byte ** str, int *count,
										 dword wordspace, int top, int height,
										 int bot, FT_UInt * previous,
										 bool is_hanzi)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;

	FT_Bool useKerning;

	useKerning = FT_HAS_KERNING(ttf->face);
	word ucs = charsets_gbk_to_ucs(*str);
	SBit_HashItem *cache =
		sbitCacheFind(ttf, ucs, get_pixel_mode(get_render_mode(ttf, false)));

	if (cache) {
		if (useKerning && *previous && cache->glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, cache->glyph_index,
						   FT_KERNING_DEFAULT, &delta);
			*x -= delta.x >> 6;
		}
		drawCachedBitmap_reversal(&cache->bitmap,
								  *x - cache->bitmap.left,
								  bot ? *y - DISP_BOOK_FONTSIZE +
								  cache->bitmap.top : *y - height +
								  cache->bitmap.top, PSP_SCREEN_WIDTH,
								  bot ? bot : PSP_SCREEN_HEIGHT, color);
		*x -= cache->xadvance >> 6;
		*previous = cache->glyph_index;
	} else {
		glyphIndex = FT_Get_Char_Index(ttf->face, ucs);
		// disable hinting when loading chinese characters
		if (is_hanzi)
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_NO_HINTING);
		else
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_DEFAULT);
		if (error)
			return;
		if (ttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			error =
				FT_Render_Glyph(ttf->face->glyph, get_render_mode(ttf, false));
			if (error) {
				return;
			}
		}
		slot = ttf->face->glyph;

		if (ttf->embolden)
			FT_GlyphSlot_Embolden(slot);

		if (useKerning && *previous && glyphIndex) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, glyphIndex,
						   FT_KERNING_DEFAULT, &delta);
			*x -= delta.x >> 6;
		}
		drawBitmap_reversal(&slot->bitmap, *x - slot->bitmap_left,
							bot ? *y - DISP_BOOK_FONTSIZE +
							slot->bitmap_top : *y - height +
							slot->bitmap_top, PSP_SCREEN_WIDTH,
							bot ? bot : PSP_SCREEN_HEIGHT, color);
		*x -= slot->advance.x >> 6;
		*previous = glyphIndex;

		sbitCacheAdd(ttf, ucs, glyphIndex,
					 &slot->bitmap, slot->bitmap_left, slot->bitmap_top,
					 slot->advance.x, slot->advance.y);
	}
	if (is_hanzi) {
		(*str) += 2;
		*count -= 2;
		*x -= wordspace * 2;
	} else {
		(*str)++;
		(*count)--;
		*x -= wordspace;
	}
}

extern void disp_putnstring_reversal_truetype(p_ttf cttf, p_ttf ettf, int x,
											  int y, pixel color,
											  const byte * str, int count,
											  dword wordspace, int top,
											  int height, int bot)
{
	if (cttf == NULL || ettf == NULL)
		return;

	if (bot) {
		if (y >= bot)
			return;
		if (y + height > bot)
			height = bot - y;
//      dbg_printf(d, "%s: bot now: height: %d bot %d", __func__, height, bot);
	}

	x = PSP_SCREEN_WIDTH - x - 1, y = PSP_SCREEN_HEIGHT - y - 1;

	FT_UInt cprevious, eprevious;

	cprevious = eprevious = 0;

	dword cpu, bus;

	power_get_clock(&cpu, &bus);
	if (cpu < 222)
		power_set_clock(222, 111);
	ttf_set_pixel_size(ettf, height);
	ttf_set_pixel_size(cttf, height);
	while (*str != 0 && count > 0) {
		if (!check_range(x, y)) {
			scene_power_save(true);
			return;
		}
		if (x < 0)
			break;
		if (*str > 0x80) {
			ttf_disp_putnstring_reversal(cttf, &x, &y, color, &str,
										 &count, wordspace, top,
										 height, bot, &cprevious, true);
		} else if (*str > 0x1F && *str != 0x20) {
			ttf_disp_putnstring_reversal(ettf, &x, &y, color, &str,
										 &count, wordspace, top,
										 height, bot, &eprevious, false);
		} else {
			if (x < 0) {
				break;
#if 0
				x = 0;
				y += DISP_BOOK_FONTSIZE;
#endif
			}
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				x -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
	scene_power_save(true);
}

/** 
 * ����TTF���͵���Ļ������汾
 * @param buffer ����λͼ����
 * @param format ��ʽ
 * @param width λͼ���
 * @param height λͼ�߶�
 * @param pitch λͼ��λ�����ֽڼǣ�
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param scr_width ��ͼ�������
 * @param scr_height ��ͼ�����߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void _drawBitmap_lvert(byte * buffer, int format, int width, int height,
							  int pitch, FT_Int x, FT_Int y, int scr_width,
							  int scr_height, pixel color)
{
	FT_Int i, j;
	pixel pix, grey;
	byte ra, ga, ba;
	byte rd, gd, bd;
	byte rs, gs, bs;

	if (!buffer)
		return;
	if (format == FT_PIXEL_MODE_MONO) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (y - i < 0 || y - i >= scr_height
					|| x + j < 0 || x + j >= scr_width)
					continue;
				if (buffer[j * pitch + i / 8] & (0x80 >> (i % 8)))
					*(pixel *) disp_get_vaddr((x + j), (y - i)) = (color);
			}
	} else if (format == FT_PIXEL_MODE_GRAY) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (y - i < 0 || y - i >= scr_height
					|| x + j < 0 || x + j >= scr_width)
					continue;
				grey = buffer[j * pitch + i];

				if (grey) {
					pix =
						disp_grayscale(*disp_get_vaddr
									   ((x + j), (y - i)),
									   RGB_R(color),
									   RGB_G(color),
									   RGB_B(color), grey * 100 / 255);
					*(pixel *) disp_get_vaddr((x + j), (y - i)) = (pix);
				}
			}
	} else if (format == FT_PIXEL_MODE_LCD) {
//      dbg_printf(d, "%s: %d, %d, %d", __func__, x, y, scr_height);
		for (j = 0; j < height; j++)
			for (i = 0; i < width / 3; i++) {
				if (y - i < 0 || y - i >= scr_height
					|| x + j < 0 || x + j >= scr_width)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x + j), (y - i));

				ra = buffer[j * pitch + i * 3];
				ga = buffer[j * pitch + i * 3 + 1];
				ba = buffer[j * pitch + i * 3 + 2];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x + j), (y - i)) = (RGB(rd, gd, bd));
			}
	} else if (format == FT_PIXEL_MODE_LCD_V) {
		for (j = 0; j < height / 3; j++)
			for (i = 0; i < width; i++) {
				if (y - i < 0 || y - i >= scr_height
					|| x + j < 0 || x + j >= scr_width)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x + j), (y - i));

				ra = buffer[j * 3 * pitch + i];
				ga = buffer[(j * 3 + 1) * pitch + i];
				ba = buffer[(j * 3 + 2) * pitch + i];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x + j), (y - i)) = (RGB(rd, gd, bd));
			}
	}
}

/** 
 * ����TTF���ͻ��浽��Ļ������汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static inline void drawCachedBitmap_lvert(Cache_Bitmap * sbt, FT_Int x,
										  FT_Int y, int width, int height,
										  pixel color)
{
	_drawBitmap_lvert(sbt->buffer, sbt->format, sbt->width, sbt->height,
					  sbt->pitch, x, y, width, height, color);
}

/** 
 * ����TTF���͵���Ļ������汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static inline void drawBitmap_lvert(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
									int width, int height, pixel color)
{
	_drawBitmap_lvert(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
					  bitmap->rows, bitmap->pitch, x, y, width, height, color);
}

/**
 * ��*str��ȡ��һ���֣�����/Ӣ����ĸ�����л��ƣ�����汾
 * @param ttf ʹ�õ�TTF����
 * @param *x ָ��X�����ָ��
 * @param *y ָ��Y�����ָ�� 
 * @param color ������ɫ
 * @param str �ַ���ָ��ָ��
 * @param count ����ָ��
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @param top ��������Ŀ�ʼ�У�����������Ͳ�������Ļ���⣬��Ϊ��0ֵ
 * @param height ��������ĸ߶ȣ�����������β�������Ļ���⣬
 * 				 �򲻵���DISP_BOOK_FONTSIZE�������ڱ��ü���ĸ߶�
 * @param bot �����Ƹ߶�
 * @param previous ָ����һ��ͬ���ַ�ָ��
 * @param is_hanzi �Ƿ�Ϊ����
 * @note ���������һ�����ͣ���*x, *y, *count, *str���ᱻ����
 * <br> ������ڻ��Ƶ�һ�У�bot = 0������ڻ��������У�bot = �����Ƹ߶�
 */
static void ttf_disp_putnstring_lvert(p_ttf ttf, int *x, int *y, pixel color,
									  const byte ** str, int *count,
									  dword wordspace, int top, int height,
									  int bot, FT_UInt * previous,
									  bool is_hanzi)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;

	FT_Bool useKerning;

	useKerning = FT_HAS_KERNING(ttf->face);
	word ucs = charsets_gbk_to_ucs(*str);
	SBit_HashItem *cache =
		sbitCacheFind(ttf, ucs, get_pixel_mode(get_render_mode(ttf, true)));

	if (cache) {
		if (useKerning && *previous && cache->glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, cache->glyph_index,
						   FT_KERNING_DEFAULT, &delta);
			*y -= delta.x >> 6;
		}
		drawCachedBitmap_lvert(&cache->bitmap,
							   bot ? *x + DISP_BOOK_FONTSIZE -
							   cache->bitmap.top : *x + height -
							   cache->bitmap.top,
							   *y - cache->bitmap.left,
							   bot ? bot : PSP_SCREEN_WIDTH,
							   PSP_SCREEN_HEIGHT, color);
		*y -= cache->xadvance >> 6;
		*previous = cache->glyph_index;
	} else {
		glyphIndex = FT_Get_Char_Index(ttf->face, ucs);
		// disable hinting when loading chinese characters
		if (is_hanzi)
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_NO_HINTING);
		else
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_DEFAULT);
		if (error)
			return;
		if (ttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			error =
				FT_Render_Glyph(ttf->face->glyph, get_render_mode(ttf, true));
			if (error) {
				return;
			}
		}
		slot = ttf->face->glyph;

		if (ttf->embolden)
			FT_GlyphSlot_Embolden(slot);

		if (useKerning && *previous && glyphIndex) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, glyphIndex,
						   FT_KERNING_DEFAULT, &delta);
			*y -= delta.x >> 6;
		}
		drawBitmap_lvert(&slot->bitmap,
						 bot ? *x + DISP_BOOK_FONTSIZE -
						 slot->bitmap_top : *x + height -
						 slot->bitmap_top, *y - slot->bitmap_left,
						 bot ? bot : PSP_SCREEN_WIDTH,
						 PSP_SCREEN_HEIGHT, color);
		*y -= slot->advance.x >> 6;
		*previous = glyphIndex;

		sbitCacheAdd(ttf, ucs, glyphIndex,
					 &slot->bitmap, slot->bitmap_left, slot->bitmap_top,
					 slot->advance.x, slot->advance.y);
	}
	if (is_hanzi) {
		(*str) += 2;
		*count -= 2;
		*y -= wordspace * 2;
	} else {
		(*str)++;
		(*count)--;
		*y -= wordspace;
	}
}

extern void disp_putnstring_lvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const byte * str,
										   int count, dword wordspace, int top,
										   int height, int bot)
{
	if (cttf == NULL || ettf == NULL)
		return;

	if (bot) {
		if (x >= bot)
			return;
		if (x + height > bot)
			height = bot - x;
//      dbg_printf(d, "%s: bot now: height: %d bot %d", __func__, height, bot);
	}

	FT_UInt cprevious, eprevious;

	cprevious = eprevious = 0;

	dword cpu, bus;

	power_get_clock(&cpu, &bus);
	if (cpu < 222)
		power_set_clock(222, 111);
	ttf_set_pixel_size(ettf, height);
	ttf_set_pixel_size(cttf, height);
	while (*str != 0 && count > 0) {
		if (!check_range(x, y)) {
			scene_power_save(true);
			return;
		}
		if (*str > 0x80) {
			if (y < DISP_RSPAN + DISP_BOOK_FONTSIZE - 1)
				break;
			ttf_disp_putnstring_lvert(cttf, &x, &y, color, &str,
									  &count, wordspace, top,
									  height, bot, &cprevious, true);
		} else if (*str > 0x1F && *str != 0x20) {
			if (y < DISP_RSPAN + disp_ewidth[*str] - 1)
				break;
			ttf_disp_putnstring_lvert(ettf, &x, &y, color, &str,
									  &count, wordspace, top,
									  height, bot, &eprevious, false);
		} else {
			if (y < DISP_RSPAN + DISP_BOOK_FONTSIZE - 1) {
				break;
			}
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				y -= DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
	scene_power_save(true);
}

/** 
 * ����TTF���͵���Ļ������汾
 * @param buffer ����λͼ����
 * @param format ��ʽ
 * @param width λͼ���
 * @param height λͼ�߶�
 * @param pitch λͼ��λ�����ֽڼǣ�
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param scr_width ��ͼ�������
 * @param scr_height ��ͼ�����߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static void _drawBitmap_rvert(byte * buffer, int format, int width, int height,
							  int pitch, FT_Int x, FT_Int y, int scr_width,
							  int scr_height, pixel color)
{
	FT_Int i, j;
	pixel pix, grey;
	byte ra, ga, ba;
	byte rd, gd, bd;
	byte rs, gs, bs;

	if (!buffer)
		return;
	if (format == FT_PIXEL_MODE_MONO) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (y + i < 0 || y + i >= scr_height
					|| x - j < PSP_SCREEN_WIDTH - scr_width
					|| x - j >= PSP_SCREEN_WIDTH)
					continue;
				if (buffer[j * pitch + i / 8] & (0x80 >> (i % 8)))
					*(pixel *) disp_get_vaddr((x - j), (y + i)) = (color);
			}
	} else if (format == FT_PIXEL_MODE_GRAY) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width; i++) {
				if (y + i < 0 || y + i >= scr_height
					|| x - j < PSP_SCREEN_WIDTH - scr_width
					|| x - j >= PSP_SCREEN_WIDTH)
					continue;
				grey = buffer[j * pitch + i];

				if (grey) {
					pix =
						disp_grayscale(*disp_get_vaddr
									   ((x - j), (y + i)),
									   RGB_R(color),
									   RGB_G(color),
									   RGB_B(color), grey * 100 / 255);
					*(pixel *) disp_get_vaddr((x - j), (y + i)) = (pix);
				}
			}
	} else if (format == FT_PIXEL_MODE_LCD) {
		for (j = 0; j < height; j++)
			for (i = 0; i < width / 3; i++) {
				if (y + i < 0 || y + i >= scr_height
					|| x - j < PSP_SCREEN_WIDTH - scr_width
					|| x - j >= PSP_SCREEN_WIDTH)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x - j), (y + i));

				ra = buffer[j * pitch + i * 3];
				ga = buffer[j * pitch + i * 3 + 1];
				ba = buffer[j * pitch + i * 3 + 2];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x - j), (y + i)) = (RGB(rd, gd, bd));
			}
	} else if (format == FT_PIXEL_MODE_LCD_V) {
		for (j = 0; j < height / 3; j++)
			for (i = 0; i < width; i++) {
				if (y + i < 0 || y + i >= scr_height
					|| x - j < PSP_SCREEN_WIDTH - scr_width
					|| x - j >= PSP_SCREEN_WIDTH)
					continue;
				// RGB or BGR ?
				pixel origcolor = *disp_get_vaddr((x - j), (y + i));

				ra = buffer[j * 3 * pitch + i];
				ga = buffer[(j * 3 + 1) * pitch + i];
				ba = buffer[(j * 3 + 2) * pitch + i];

				rs = RGB_R(origcolor);
				gs = RGB_G(origcolor);
				bs = RGB_B(origcolor);

				rd = RGB_R(color);
				gd = RGB_G(color);
				bd = RGB_B(color);

				MakeAlpha(&rs, &rd, ra);
				MakeAlpha(&gs, &gd, ga);
				MakeAlpha(&bs, &bd, ba);

				*(pixel *) disp_get_vaddr((x - j), (y + i)) = (RGB(rd, gd, bd));
			}
	}
}

/** 
 * ����TTF���ͻ��浽��Ļ������汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static inline void drawCachedBitmap_rvert(Cache_Bitmap * sbt, FT_Int x,
										  FT_Int y, int width, int height,
										  pixel color)
{
	_drawBitmap_rvert(sbt->buffer, sbt->format, sbt->width, sbt->height,
					  sbt->pitch, x, y, width, height, color);
}

/** 
 * ����TTF���͵���Ļ������汾
 * @param bitmap ����λͼ
 * @param x ��Ļx����
 * @param y ��Ļy����
 * @param width �����
 * @param height ���߶�
 * @param color ��ɫ
 * @note ����(x,y)Ϊ������ʾ�����Ͻǣ�������������"�հ�"����
 */
static inline void drawBitmap_rvert(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
									int width, int height, pixel color,
									int space)
{
	_drawBitmap_rvert(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
					  bitmap->rows, bitmap->pitch, x, y, width, height, color);
}

/**
 * ��*str��ȡ��һ���֣�����/Ӣ����ĸ�����л��ƣ�����汾
 * @param ttf ʹ�õ�TTF����
 * @param *x ָ��X�����ָ��
 * @param *y ָ��Y�����ָ�� 
 * @param color ������ɫ
 * @param str �ַ���ָ��ָ��
 * @param count ����ָ��
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @param top ��������Ŀ�ʼ�У�����������Ͳ�������Ļ���⣬��Ϊ��0ֵ
 * @param height ��������ĸ߶ȣ�����������β�������Ļ���⣬
 * 				 �򲻵���DISP_BOOK_FONTSIZE�������ڱ��ü���ĸ߶�
 * @param bot �����Ƹ߶�
 * @param previous ָ����һ��ͬ���ַ�ָ��
 * @param is_hanzi �Ƿ�Ϊ����
 * @note ���������һ�����ͣ���*x, *y, *count, *str���ᱻ����
 * <br> ������ڻ��Ƶ�һ�У�bot = 0������ڻ��������У�bot = �����Ƹ߶�
 */
static void ttf_disp_putnstring_rvert(p_ttf ttf, int *x, int *y, pixel color,
									  const byte ** str, int *count,
									  dword wordspace, int top, int height,
									  int bot, FT_UInt * previous,
									  bool is_hanzi)
{
	FT_Error error;
	FT_GlyphSlot slot;
	FT_UInt glyphIndex;

	FT_Bool useKerning;

	useKerning = FT_HAS_KERNING(ttf->face);
	word ucs = charsets_gbk_to_ucs(*str);
	SBit_HashItem *cache =
		sbitCacheFind(ttf, ucs, get_pixel_mode(get_render_mode(ttf, true)));

	if (cache) {
		if (useKerning && *previous && cache->glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, cache->glyph_index,
						   FT_KERNING_DEFAULT, &delta);
			*y += delta.x >> 6;
		}
		drawCachedBitmap_rvert(&cache->bitmap,
							   bot ? *x - DISP_BOOK_FONTSIZE +
							   cache->bitmap.top : *x - height +
							   cache->bitmap.top,
							   *y + cache->bitmap.left,
							   bot ? PSP_SCREEN_WIDTH -
							   bot : PSP_SCREEN_WIDTH,
							   PSP_SCREEN_HEIGHT, color);
		*y += cache->xadvance >> 6;
		*previous = cache->glyph_index;
	} else {
		glyphIndex = FT_Get_Char_Index(ttf->face, ucs);
		// disable hinting when loading chinese characters
		if (is_hanzi)
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_NO_HINTING);
		else
			error = FT_Load_Glyph(ttf->face, glyphIndex, FT_LOAD_DEFAULT);
		if (error)
			return;
		if (ttf->face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			error =
				FT_Render_Glyph(ttf->face->glyph, get_render_mode(ttf, true));
			if (error) {
				return;
			}
		}
		slot = ttf->face->glyph;

		if (ttf->embolden)
			FT_GlyphSlot_Embolden(slot);

		if (useKerning && *previous && glyphIndex) {
			FT_Vector delta;

			FT_Get_Kerning(ttf->face, *previous, glyphIndex,
						   FT_KERNING_DEFAULT, &delta);
			*y += delta.x >> 6;
		}
		drawBitmap_rvert(&slot->bitmap,
						 bot ? *x - DISP_BOOK_FONTSIZE +
						 slot->bitmap_top : *x - height +
						 slot->bitmap_top, *y + slot->bitmap_left,
						 bot ? PSP_SCREEN_WIDTH -
						 bot : PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
						 color, DISP_BOOK_FONTSIZE - slot->bitmap_top);
		*y += slot->advance.x >> 6;
		*previous = glyphIndex;

		sbitCacheAdd(ttf, ucs, glyphIndex,
					 &slot->bitmap, slot->bitmap_left, slot->bitmap_top,
					 slot->advance.x, slot->advance.y);
	}
	if (is_hanzi) {
		(*str) += 2;
		*count -= 2;
		*y += wordspace * 2;
	} else {
		(*str)++;
		(*count)--;
		*y += wordspace;
	}
}

extern void disp_putnstring_rvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const byte * str,
										   int count, dword wordspace, int top,
										   int height, int bot)
{
	if (cttf == NULL || ettf == NULL)
		return;

	if (x < bot)
		return;
	if (x + 1 - height < bot)
		height = x + 1 - bot;

	FT_UInt cprevious, eprevious;

	cprevious = eprevious = 0;

	dword cpu, bus;

	power_get_clock(&cpu, &bus);
	if (cpu < 222)
		power_set_clock(222, 111);
	ttf_set_pixel_size(ettf, height);
	ttf_set_pixel_size(cttf, height);
	while (*str != 0 && count > 0) {
		if (!check_range(x, y)) {
			scene_power_save(true);
			return;
		}
		if (*str > 0x80) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE)
				break;
			ttf_disp_putnstring_rvert(cttf, &x, &y, color, &str,
									  &count, wordspace, top,
									  height, bot, &cprevious, true);
		} else if (*str > 0x1F && *str != 0x20) {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - disp_ewidth[*str])
				break;
			ttf_disp_putnstring_rvert(ettf, &x, &y, color, &str,
									  &count, wordspace, top,
									  height, bot, &eprevious, false);
		} else {
			if (y > PSP_SCREEN_HEIGHT - DISP_RSPAN - DISP_BOOK_FONTSIZE / 2) {
				break;
			}
			int j;

			for (j = 0; j < (*str == 0x09 ? config.tabstop : 1); ++j)
				y += DISP_BOOK_FONTSIZE / 2 + wordspace;
			str++;
			count--;
		}
	}
	scene_power_save(true);
}

#endif

extern void ttf_lock(void)
{
#ifdef ENABLE_TTF
	if (ttf_sema >= 0)
		sceKernelWaitSema(ttf_sema, 1, NULL);
#endif
}

extern void ttf_unlock(void)
{
#ifdef ENABLE_TTF
	if (ttf_sema >= 0)
		sceKernelSignalSema(ttf_sema, 1);
#endif
}

extern void ttf_init(void)
{
#ifdef ENABLE_TTF
	ttf_sema = sceKernelCreateSema("TTF Sema", 0, 1, 1, NULL);
#endif
}

extern void ttf_free(void)
{
#ifdef ENABLE_TTF
	if (ttf_sema >= 0)
		sceKernelDeleteSema(ttf_sema);
#endif
}
