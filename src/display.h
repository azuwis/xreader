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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <pspgu.h>
#include <pspdisplay.h>
#include "common/datatype.h"

typedef dword pixel;

#include "ttfont.h"

extern int DISP_FONTSIZE, DISP_BOOK_FONTSIZE, HRR, WRR;
extern byte disp_ewidth[0x80];

// R,G,B color to word value color

#define PIXEL_BYTES 4
#define COLOR_MAX 255
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0x0

#define RGB(r,g,b) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|0xFF000000)
#define RGB2(r,g,b) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|0xFF000000)
#define RGBA(r,g,b,a) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|(((pixel)(a))<<24))
#define RGB_R(c) ((c) & 0xFF)
#define RGB_G(c) (((c) >> 8) & 0xFF)
#define RGB_B(c) (((c) >> 16) & 0xFF)
#define RGB_16to32(c) RGBA((((c)&0x1F)*255/31),((((c)>>5)&0x1F)*255/31),((((c)>>10)&0x1F)*255/31),((c&0x8000)?0xFF:0))

#define disp_grayscale(c,r,g,b,gs) RGB2(((r)*(gs)+RGB_R(c)*(100-(gs)))/100,((g)*(gs)+RGB_G(c)*(100-(gs)))/100,((b)*(gs)+RGB_B(c)*(100-(gs)))/100)

extern pixel *vram_draw;

// sceDisplayWaitVblankStart function alias name, define is faster than function call (even at most time this is inline linked)
#define disp_waitv() sceDisplayWaitVblankStart()

#define disp_get_vaddr(x, y) (vram_draw + (x) + ((y) << 9))

/**
 * ����
 *
 * @param x
 * @param y
 * @param color
 */
extern void disp_putpixel(int x, int y, pixel color);

/**
 * ��ʼ����ʾ
 *
 * @note ������ʾ���桢ҳ�桢ģʽ
 *
 */
extern void disp_init(void);

/**
 * ��ʼ��Graphics Unit
 */
extern void init_gu(void);

/**
 * ����ϵͳ�˵������С
 *
 * @param fontsize
 */
extern void disp_set_fontsize(int fontsize);

/**
 * �����ı���ʾ�����С
 *
 * @note ��������С<=10�����Զ����ּ������Ϊ1�����ַ���ʾ����
 *
 * @param fontsize
 */
extern void disp_set_book_fontsize(int fontsize);

/**
 * ���ZIP�ļ����Ƿ��ж�Ӧ������
 *
 * @param zipfile
 * @param efont
 * @param cfont
 *
 * @return
 */
extern bool disp_has_zipped_font(const char *zipfile, const char *efont,
								 const char *cfont);

/**
 * ��ZIP�ļ���װ��ϵͳ��������
 *
 * @param zipfile
 * @param efont
 * @param cfont
 *
 * @return
 */
extern bool disp_load_zipped_font(const char *zipfile, const char *efont,
								  const char *cfont);

/**
 * ��ZIP�ļ���װ���ı���������
 *
 * @param zipfile
 * @param efont
 * @param cfont
 *
 * @return
 */
extern bool disp_load_zipped_book_font(const char *zipfile, const char *efont,
									   const char *cfont);

/**
 * ��ZIP�����ж�ȡ�ļ���Ϊ�ı�TTF����
 *
 * @note ���ͬ�ϴ�װ�ص��ļ���ͬ�Ͳ��ظ�װ�أ�ֻ�ı������С
 *
 * @param ezipfile
 * @param czipfile
 * @param ettffile
 * @param cttffile
 * @param size
 *
 * @return
 */
extern bool disp_load_zipped_truetype_book_font(const char *ezipfile,
												const char *czipfile,
												const char *ettffile,
												const char *cttffile, int size);

/**
 * �����Ӣ�������ļ��Ƿ񶼴���
 *
 * @param efont
 * @param cfont
 *
 * @return 
 */
extern bool disp_has_font(const char *efont, const char *cfont);

/**
 * װ����Ӣ��ϵͳ���������ļ�
 *
 * @note װ�غ��ı��������屻����Ϊ��ϵͳ�������干��ͬһ��ַ
 *
 * @param efont
 * @param cfont
 *
 * @return 
 */
extern bool disp_load_font(const char *efont, const char *cfont);

/**
 * װ����Ӣ���ı����������ļ�
 *
 * @note 
 *
 * @param efont
 * @param cfont
 *
 * @return 
 */
extern bool disp_load_book_font(const char *efont, const char *cfont);

extern void disp_assign_book_font(void);
extern void disp_free_font(void);
extern void disp_flip(void);
extern void disp_getimage(dword x, dword y, dword w, dword h, pixel * buf);
extern void disp_getimage_draw(dword x, dword y, dword w, dword h, pixel * buf);

extern void disp_newputimage(int x, int y, int w, int h, int bufw, int startx,
							 int starty, int ow, int oh, pixel * buf,
							 bool swizzled);
extern void disp_putimage(dword x, dword y, dword w, dword h, dword startx,
						  dword starty, pixel * buf);
extern void disp_duptocache(void);
extern void disp_duptocachealpha(int percent);
extern void disp_rectduptocache(dword x1, dword y1, dword x2, dword y2);
extern void disp_rectduptocachealpha(dword x1, dword y1, dword x2, dword y2,
									 int percent);

extern void disp_putnstring(int x, int y, pixel color, const byte * str,
							int count, dword wordspace, int top, int height,
							int bot);
#define disp_putstring(x,y,color,str) disp_putnstring((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_FONTSIZE,PSP_SCREEN_HEIGHT)

extern void disp_putnstringreversal(int x, int y, pixel color, const byte * str,
									int count, dword wordspace, int top,
									int height, int bot);
#define disp_putstringreversal(x,y,color,str) disp_putnstringreversal((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringhorz(int x, int y, pixel color, const byte * str,
								int count, dword wordspace, int top, int height,
								int bot);
#define disp_putstringhorz(x,y,color,str) disp_putnstringhorz((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringlvert(int x, int y, pixel color, const byte * str,
								 int count, dword wordspace, int top,
								 int height, int bot);
#define disp_putstringlvert(x,y,color,str) disp_putnstringlvert((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringrvert(int x, int y, pixel color, const byte * str,
								 int count, dword wordspace, int top,
								 int height, int bot);
#define disp_putstringrvert(x,y,color,str) disp_putnstringrvert((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringreversal_sys(int x, int y, pixel color,
										const byte * str, int count,
										dword wordspace, int top, int height,
										int bot);
#define disp_putstringreversal_sys(x,y,color,str) disp_putnstringreversal_sys((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringhorz_sys(int x, int y, pixel color, const byte * str,
									int count, dword wordspace, int top,
									int height, int bot);
#define disp_putstringhorz_sys(x,y,color,str) disp_putnstringhorz_sys((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringlvert_sys(int x, int y, pixel color,
									 const byte * str, int count,
									 dword wordspace, int top, int height,
									 int bot);
#define disp_putstringlvert_sys(x,y,color,str) disp_putnstringlvert_sys((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringrvert_sys(int x, int y, pixel color,
									 const byte * str, int count,
									 dword wordspace, int top, int height,
									 int bot);
#define disp_putstringrvert_sys(x,y,color,str) disp_putnstringrvert_sys((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringreversal_sys(int x, int y, pixel color,
										const byte * str, int count,
										dword wordspace, int top, int height,
										int bot);
#define disp_putstringreversal_sys(x,y,color,str) disp_putnstringreversal_sys((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_fillvram(pixel color);
extern void disp_fillrect(dword x1, dword y1, dword x2, dword y2, pixel color);
extern void disp_rectangle(dword x1, dword y1, dword x2, dword y2, pixel color);
extern void disp_line(dword x1, dword y1, dword x2, dword y2, pixel color);
extern bool check_range(int x, int y);
extern void disp_fix_osk(void *buffer);
pixel *disp_swizzle_image(pixel * buf, int width, int height);

#ifdef ENABLE_TTF
extern void disp_ttf_close(void);
#endif

#endif
