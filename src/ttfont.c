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
 * 打开TTF字体
 * @param filename TTF文件名
 * @param size 预设的字体大小
 * @param load2mem 是否加载到内存
 * @return 描述TTF的指针
 * - NULL 失败
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
 * 打开TTF字体数据
 * @param ttfpath TTF字体路径
 * @param pixelSize 预设的字体大小
 * @param ttfName TTF字体名
 * @return 描述TTF的指针
 * - NULL 失败
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
 * 打开TTF字体数据
 * @param ttfBuf TTF字体数据
 * @param ttfLength TTF字体数据大小，以字节计
 * @param pixelSize 预设的字体大小
 * @param ttfName TTF字体名
 * @return 描述TTF的指针
 * - NULL 失败
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
 * 关闭TTF结构
 * @param ttf ttf指针
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
 * 设置TTF字体大小
 * @param ttf ttf指针
 * @param size 字体大小
 * @return 是否成功
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
 * 设置TTF是否启用抗锯齿效果
 * @param ttf ttf指针
 * @param aa 是否启用抗锯齿效果
 */
extern void ttf_set_anti_alias(p_ttf ttf, bool aa)
{
	if (ttf == NULL)
		return;

	ttf->antiAlias = aa;
}

/**
 * 设置TTF是否启用ClearType(Sub-Pixel LCD优化效果)
 * @param ttf ttf指针
 * @param cleartype 是否启用cleartype
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
 * 设置TTF是否启用字体加粗
 * @param ttf ttf指针
 * @param embolden 是否启用字体加粗
 */
extern void ttf_set_embolden(p_ttf ttf, bool embolden)
{
	if (ttf == NULL)
		return;

	ttf->embolden = embolden;
}

/**
 * 添加字形到ttf字型缓存
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
 * 在TTF字形缓存中查找字形 
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
 * 8位单通道alpha混合算法
 * @param wpSrc 源颜色指针
 * @param wpDes 目的颜色指针
 * @param wAlpha alpha值(0-255)
 * @note 目的颜色 = 目的颜色 * alpha + ( 1 - alpha ) * 源颜色
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
 * 绘制TTF字型到屏幕，水平版本
 * @param buffer 字型位图数据
 * @param format 格式
 * @param width 位图宽度
 * @param height 位图高度
 * @param pitch 位图行位数（字节记）
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param scr_width 绘图区最大宽度
 * @param scr_height 绘图区最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 绘制TTF字型缓存到屏幕，水平版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static void drawCachedBitmap_horz(Cache_Bitmap * sbt, FT_Int x, FT_Int y,
								  int width, int height, pixel color)
{
	_drawBitmap_horz(sbt->buffer, sbt->format, sbt->width, sbt->height,
					 sbt->pitch, x, y, width, height, color);
}

/** 
 * 绘制TTF字型到屏幕，水平版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 从*str中取出一个字（汉字/英文字母）进行绘制，水平版本
 * @param ttf 使用的TTF字体
 * @param *x 指向X坐标的指针
 * @param *y 指向Y坐标的指针 
 * @param color 字体颜色
 * @param str 字符串指针指针
 * @param count 字数指针
 * @param wordspace 字间距（以像素点计）
 * @param top 绘制字体的开始行，如果所绘字型部分在屏幕以外，则为非0值
 * @param height 绘制字体的高度，如果所绘字形部分在屏幕以外，
 * 				 则不等于DISP_BOOK_FONTSIZE，而等于被裁剪后的高度
 * @param bot 最大绘制高度
 * @param previous 指向上一个同类字符指针
 * @param is_hanzi 是否为汉字
 * @note 如果绘制了一个字型，则*x, *y, *count, *str都会被更新
 * <br> 如果是在绘制第一行，bot = 0，如果在绘制其它行，bot = 最大绘制高度
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
 * 得到字符串所能显示在maxpixels中的长度
 * @param cttf 中文TTF字体
 * @param ettf 英文TTF字体
 * @param str 字符串
 * @param maxpixels 最大长度
 * @param wordspace 字间距（以像素点计）
 * @note 如果遇到换行，则字符串计数停止累加。
 * <br>  如果字符串绘制长度＞maxpixels，则字符串计数停止累加。
 * <br>  这个版本速度极慢，不要使用
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
 * 得到字符串所能显示在maxpixels中的长度
 * @param cttf 中文TTF字体
 * @param ettf 英文TTF字体
 * @param str 字符串
 * @param maxpixels 最大象素长度
 * @param maxbytes 最大字符长度，以字节计
 * @param wordspace 字间距（以像素点计）
 * @return 字符串个数计数，以字节计
 * @note 如果遇到换行，则字符串计数停止累加。
 * <br>  如果字符串绘制长度＞maxpixels，则字符串计数停止累加。
 * <br>  这个版本速度快，但对于英文字母可能有出界问题
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
		hanzi_len = ttf_get_char_width(cttf, (const byte *) "字");
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
 * 得到英文字母宽度信息
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
 * 绘制TTF字型到屏幕，颠倒版本
 * @param buffer 字型位图数据
 * @param format 格式
 * @param width 位图宽度
 * @param height 位图高度
 * @param pitch 位图行位数（字节记）
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param scr_width 绘图区最大宽度
 * @param scr_height 绘图区最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 绘制TTF字型缓存到屏幕，颠倒版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static void drawCachedBitmap_reversal(Cache_Bitmap * sbt, FT_Int x, FT_Int y,
									  int width, int height, pixel color)
{
	_drawBitmap_reversal(sbt->buffer, sbt->format, sbt->width, sbt->height,
						 sbt->pitch, x, y, width, height, color);
}

/** 
 * 绘制TTF字型到屏幕，颠倒版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 从*str中取出一个字（汉字/英文字母）进行绘制，颠倒版本
 * @param ttf 使用的TTF字体
 * @param *x 指向X坐标的指针
 * @param *y 指向Y坐标的指针 
 * @param color 字体颜色
 * @param str 字符串指针指针
 * @param count 字数指针
 * @param wordspace 字间距（以像素点计）
 * @param top 绘制字体的开始行，如果所绘字型部分在屏幕以外，则为非0值
 * @param height 绘制字体的高度，如果所绘字形部分在屏幕以外，
 * 				 则不等于DISP_BOOK_FONTSIZE，而等于被裁剪后的高度
 * @param bot 最大绘制高度
 * @param previous 指向上一个同类字符指针
 * @param is_hanzi 是否为汉字
 * @note 如果绘制了一个字型，则*x, *y, *count, *str都会被更新
 * <br> 如果是在绘制第一行，bot = 0，如果在绘制其它行，bot = 最大绘制高度
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
 * 绘制TTF字型到屏幕，左向版本
 * @param buffer 字型位图数据
 * @param format 格式
 * @param width 位图宽度
 * @param height 位图高度
 * @param pitch 位图行位数（字节记）
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param scr_width 绘图区最大宽度
 * @param scr_height 绘图区最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 绘制TTF字型缓存到屏幕，左向版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static inline void drawCachedBitmap_lvert(Cache_Bitmap * sbt, FT_Int x,
										  FT_Int y, int width, int height,
										  pixel color)
{
	_drawBitmap_lvert(sbt->buffer, sbt->format, sbt->width, sbt->height,
					  sbt->pitch, x, y, width, height, color);
}

/** 
 * 绘制TTF字型到屏幕，左向版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static inline void drawBitmap_lvert(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
									int width, int height, pixel color)
{
	_drawBitmap_lvert(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
					  bitmap->rows, bitmap->pitch, x, y, width, height, color);
}

/**
 * 从*str中取出一个字（汉字/英文字母）进行绘制，左向版本
 * @param ttf 使用的TTF字体
 * @param *x 指向X坐标的指针
 * @param *y 指向Y坐标的指针 
 * @param color 字体颜色
 * @param str 字符串指针指针
 * @param count 字数指针
 * @param wordspace 字间距（以像素点计）
 * @param top 绘制字体的开始行，如果所绘字型部分在屏幕以外，则为非0值
 * @param height 绘制字体的高度，如果所绘字形部分在屏幕以外，
 * 				 则不等于DISP_BOOK_FONTSIZE，而等于被裁剪后的高度
 * @param bot 最大绘制高度
 * @param previous 指向上一个同类字符指针
 * @param is_hanzi 是否为汉字
 * @note 如果绘制了一个字型，则*x, *y, *count, *str都会被更新
 * <br> 如果是在绘制第一行，bot = 0，如果在绘制其它行，bot = 最大绘制高度
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
 * 绘制TTF字型到屏幕，右向版本
 * @param buffer 字型位图数据
 * @param format 格式
 * @param width 位图宽度
 * @param height 位图高度
 * @param pitch 位图行位数（字节记）
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param scr_width 绘图区最大宽度
 * @param scr_height 绘图区最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
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
 * 绘制TTF字型缓存到屏幕，右向版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static inline void drawCachedBitmap_rvert(Cache_Bitmap * sbt, FT_Int x,
										  FT_Int y, int width, int height,
										  pixel color)
{
	_drawBitmap_rvert(sbt->buffer, sbt->format, sbt->width, sbt->height,
					  sbt->pitch, x, y, width, height, color);
}

/** 
 * 绘制TTF字型到屏幕，右向版本
 * @param bitmap 字型位图
 * @param x 屏幕x坐标
 * @param y 屏幕y坐标
 * @param width 最大宽度
 * @param height 最大高度
 * @param color 颜色
 * @note 坐标(x,y)为字体显示的左上角，不包括字形中"空白"部分
 */
static inline void drawBitmap_rvert(FT_Bitmap * bitmap, FT_Int x, FT_Int y,
									int width, int height, pixel color,
									int space)
{
	_drawBitmap_rvert(bitmap->buffer, bitmap->pixel_mode, bitmap->width,
					  bitmap->rows, bitmap->pitch, x, y, width, height, color);
}

/**
 * 从*str中取出一个字（汉字/英文字母）进行绘制，右向版本
 * @param ttf 使用的TTF字体
 * @param *x 指向X坐标的指针
 * @param *y 指向Y坐标的指针 
 * @param color 字体颜色
 * @param str 字符串指针指针
 * @param count 字数指针
 * @param wordspace 字间距（以像素点计）
 * @param top 绘制字体的开始行，如果所绘字型部分在屏幕以外，则为非0值
 * @param height 绘制字体的高度，如果所绘字形部分在屏幕以外，
 * 				 则不等于DISP_BOOK_FONTSIZE，而等于被裁剪后的高度
 * @param bot 最大绘制高度
 * @param previous 指向上一个同类字符指针
 * @param is_hanzi 是否为汉字
 * @note 如果绘制了一个字型，则*x, *y, *count, *str都会被更新
 * <br> 如果是在绘制第一行，bot = 0，如果在绘制其它行，bot = 最大绘制高度
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
