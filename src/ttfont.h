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

#ifndef _TTFONT_H_
#define _TTFONT_H_

#include <pspkernel.h>
#include <malloc.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "./common/datatype.h"
#include "fontconfig.h"

/** Truetype���λ����С */
#define SBIT_HASH_SIZE (1024)

/** ��������λͼ�ṹ */
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

/** �������νṹ */
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

/** Truetype����ṹ */
typedef struct _ttf
{
	FT_Library library;
	FT_Face face;

	const char *fontName;
	int pixelSize;

	SBit_HashItem sbitHashRoot[SBIT_HASH_SIZE];
	int cacheSize;
	int cachePop;

	char fnpath[PATH_MAX];
	int fileSize;
	byte *fileBuffer;
	bool cjkmode;

	font_config config;
} t_ttf, *p_ttf;

/**
 * ��TTF����
 *
 * @param filename TTF�ļ���
 * @param size Ԥ��������С
 * @param load2mem �Ƿ���ص��ڴ�
 *
 * @return ����TTF��ָ��
 * - NULL ʧ��
 */
extern p_ttf ttf_open(const char *filename, int size, bool load2mem, bool cjkmode);

/**
 * ��ָ�������д�TTF����
 *
 * @param ttfBuf TTF��������
 * @param ttfLength TTF�������ݴ�С�����ֽڼ�
 * @param pixelSize Ԥ��������С
 * @param ttfName TTF������
 *
 * @return ����TTF��ָ��
 * - NULL ʧ��
 */
extern p_ttf ttf_open_buffer(void *ttfBuf, size_t ttfLength, int pixelSize,
							 const char *ttfName);

/**
 * �ͷ�TTF����
 *
 * @param ttf ttfָ��
 */
extern void ttf_close(p_ttf ttf);

/**
 * ����TTF�����С
 *
 * @param ttf ttfָ��
 * @param size �����С
 *
 * @return �Ƿ�ɹ�
 */
extern bool ttf_set_pixel_size(p_ttf ttf, int size);

/**
 * ����TTF�Ƿ����ÿ����Ч��
 *
 * @param ttf ttfָ��
 * @param aa �Ƿ����ÿ����Ч��
 */
extern void ttf_set_anti_alias(p_ttf ttf, bool aa);

/**
 * ����TTF�Ƿ�����ClearType(Sub-Pixel LCD�Ż�Ч��)
 *
 * @param ttf ttfָ��
 * @param cleartype �Ƿ�����cleartype
 */
extern void ttf_set_cleartype(p_ttf ttf, bool cleartype);

/**
 * ����TTF�Ƿ���������Ӵ�
 *
 * @param ttf ttfָ��
 * @param embolden �Ƿ���������Ӵ�
 */
extern void ttf_set_embolden(p_ttf ttf, bool embolden);

/**
 * �õ��ַ���������ʾ��maxpixels�еĳ���
 *
 * @note ����������У����ַ�������ֹͣ�ۼӡ�
 * <br>  ����ַ������Ƴ��ȣ�maxpixels�����ַ�������ֹͣ�ۼӡ�
 * <br>  ����汾�ٶȿ죬������Ӣ����ĸ�����г�������
 * <br>  ����汾ΪӢ�Ļ��ư汾
 * 
 * @param cttf ����TTF����
 * @param ettf Ӣ��TTF����
 * @param str �ַ���
 * @param maxpixels ������س���
 * @param maxbytes ����ַ����ȣ����ֽڼ�
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 *
 * @return �ַ����������������ֽڼ�
 */
extern int ttf_get_string_width_english(p_ttf cttf, p_ttf ettf,
										const byte * str, dword maxpixels,
										dword maxbytes, dword wordspace);
/**
 * �õ��ַ���������ʾ��maxpixels�еĳ���
 *
 * @note ����������У����ַ�������ֹͣ�ۼӡ�
 * <br>  ����ַ������Ƴ��ȣ�maxpixels�����ַ�������ֹͣ�ۼӡ�
 * <br>  ����汾�ٶȿ죬������Ӣ����ĸ�����г�������
 * 
 * @param cttf ����TTF����
 * @param ettf Ӣ��TTF����
 * @param str �ַ���
 * @param maxpixels ������س���
 * @param maxbytes ����ַ����ȣ����ֽڼ�
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 * @param pwidth �����ַ�������ָ�루�����ص�ƣ�
 *
 * @return �ַ����������������ֽڼ�
 */
extern int ttf_get_string_width(p_ttf cttf, p_ttf ettf, const byte * str,
								dword maxpixels, dword maxbytes,
								dword wordspace, dword * pwidth);

/**
 * �õ��ַ���������ʾ��maxpixels�еĳ���
 *
 * @note ����������У����ַ�������ֹͣ�ۼӡ�
 * <br>  ����ַ������Ƴ��ȣ�maxpixels�����ַ�������ֹͣ�ۼӡ�
 * <br>  ����汾�ٶȼ�������Ҫʹ��
 * 
 * @param cttf ����TTF����
 * @param ettf Ӣ��TTF����
 * @param str �ַ���
 * @param maxpixels ��󳤶�
 * @param wordspace �ּ�ࣨ�����ص�ƣ�
 */
extern int ttf_get_string_width_hard(p_ttf cttf, p_ttf ettf, const byte * str,
									 dword maxpixels, dword wordspace);

/**
 * �õ�Ӣ����ĸ�����Ϣ
 * 
 * @param ttf
 * @param ewidth
 * @param size
 */
extern void ttf_load_ewidth(p_ttf ttf, byte * ewidth, int size);

/**
 * ����ˮƽTTF���庺�ֵ���Ļ
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_horz_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										  pixel color, const byte * str,
										  int count, dword wordspace, int top,
										  int height, int bot);

/**
 * ����ˮƽ�ߵ�TTF���庺�ֵ���Ļ
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_reversal_truetype(p_ttf cttf, p_ttf ettf, int x,
											  int y, pixel color,
											  const byte * str, int count,
											  dword wordspace, int top,
											  int height, int bot);

/**
 * ��������TTF���庺�ֵ���Ļ
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_lvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const byte * str,
										   int count, dword wordspace, int top,
										   int height, int bot);

/**
 * ��������TTF���庺�ֵ���Ļ
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_rvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const byte * str,
										   int count, dword wordspace, int top,
										   int height, int bot);

/** TTF���� */
extern void ttf_lock(void);

/** TTF���� */
extern void ttf_unlock(void);

/** ��ʼ��TTFģ�� */
extern void ttf_init(void);

/** �ͷ�TTFģ�� */
extern void ttf_free(void);

#endif
