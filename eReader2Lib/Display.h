/*
 * $Id: Display.h 74 2007-10-20 10:46:39Z soarchin $

 * Copyright 2007 aeolusc

 * This file is part of eReader2.

 * eReader2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * eReader2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <pspdisplay.h>
#include "Charsets.h"
#include "Datatype.h"


#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)

#ifdef DISPLAY_16BIT_DEPTH

typedef WORD PIXEL;
#define PIXEL_BYTES (2)
#define COLOR_MAX (15)
#define COLOR_WHITE (0xFFFF)
#define COLOR_BLACK (0xF000)

#define RGB(r,g,b) ((((PIXEL)(b)>>4)<<8)|(((PIXEL)(g)>>4)<<4)|((PIXEL)(r)>>4)|0xF000)
#define RGB2(r,g,b) ((((PIXEL)(b))<<8)|(((PIXEL)(g))<<4)|((PIXEL)(r))|0xF000)
#define RGBA(r,g,b,a) ((((PIXEL)(b)>>4)<<8)|(((PIXEL)(g)>>4)<<4)|((PIXEL)(r)>>4)|(((PIXEL)(a)>>4)<<12))
#define RGBA2(r,g,b,a) ((((PIXEL)(b))<<8)|(((PIXEL)(g))<<4)|((PIXEL)(r))|(((PIXEL)(a))<<12))
#define RGB_R(c) ((c) & 0x0F)
#define RGB_G(c) (((c) >> 4) & 0x0F)
#define RGB_B(c) (((c) >> 8) & 0x0F)
#define RGB_16to32(c) (c)
#define	PSM_FORMAT GU_PSM_4444
#define COLOR_FORMAT GU_COLOR_4444
#define ALPHA_MASK 0xF000
#define BLUE_MASK 0x0F00
#define GREEN_MASK 0x00F0
#define RED_MASK 0x000F

#else

typedef DWORD PIXEL;
#define PIXEL_BYTES (4)
#define COLOR_MAX (255)
#define COLOR_WHITE (0xFFFFFFFF)
#define COLOR_BLACK (0xFF000000)

#define RGB(r,g,b) ((((PIXEL)(b))<<16)|(((PIXEL)(g))<<8)|((PIXEL)(r))|0xFF000000)
#define RGB2(r,g,b) ((((PIXEL)(b))<<16)|(((PIXEL)(g))<<8)|((PIXEL)(r))|0xFF000000)
#define RGBA(r,g,b,a) ((((PIXEL)(b))<<16)|(((PIXEL)(g))<<8)|((PIXEL)(r))|(((PIXEL)(a))<<24))
#define RGBA2(r,g,b,a) ((((PIXEL)(b))<<16)|(((PIXEL)(g))<<8)|((PIXEL)(r))|(((PIXEL)(a))<<24))
#define RGB_R(c) ((c) & 0xFF)
#define RGB_G(c) (((c) >> 8) & 0xFF)
#define RGB_B(c) (((c) >> 16) & 0xFF)
// R,G,B color to WORD value color
#define RGB_16to32(c) RGBA((((c)&0x0F)*17),((((c)>>4)&0x0F)*17),((((c)>>8)&0x0F)*17),((((c)>>12)&0x0F)*17))
#define	PSM_FORMAT GU_PSM_8888
#define COLOR_FORMAT GU_COLOR_8888
#define ALPHA_MASK 0xFF000000
#define BLUE_MASK 0x00FF0000
#define GREEN_MASK 0x0000FF00
#define RED_MASK 0x000000FF

#endif

#define FRAME_BUFFER_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_BYTES)

class Display
{
public:
#ifdef DISPLAY_16BIT_DEPTH
	typedef WORD PIXEL;
#else
	typedef DWORD PIXEL;
#endif
private:
	PIXEL m_foreColor;
	PIXEL m_backColor;
	BYTE * m_FontBuffer;
	PIXEL * VRAMStart, * VRAMDisp;
	PIXEL * DrawPointer, * DrawStart;
	INT m_FontSize, m_FontBits;
	DWORD m_FontAlpha[256];
	BYTE * AllocTexBuffer(DWORD size);
	BYTE m_charwidth[65536];
public:
	Display();
	~Display();
	VOID BeginPaint(BOOL copy = FALSE);
	VOID EndPaint();
	INLINE VOID WaitVBlank() {sceDisplayWaitVblankStart();}

	BOOL HasZippedFont(CONST CHAR * zipfile, CONST CHAR * filename);
	BOOL LoadZippedFont(CONST CHAR * zipfile, CONST CHAR * filename);
	BOOL HasFont(CONST CHAR * filename);
	BOOL LoadFont(CONST CHAR * filename);
	VOID FreeFont();
	inline INT GetFontSize() CONST {return m_FontSize;}

	VOID GetImage(INT x, INT y, INT w, INT h, PIXEL * buf);
	VOID PutImage(INT x, INT y, INT w, INT h, INT bufw, INT startx, INT starty, INT ow, INT oh, PIXEL * buf, BOOL swizzled = FALSE);

	INLINE VOID SetForeColor(INT forecolor) {m_foreColor = forecolor;}
	INLINE DWORD GetForeColor() {return m_foreColor;}
	INLINE VOID SetBackColor(INT backcolor) {m_backColor = backcolor;}
	INLINE DWORD GetBackColor() {return m_backColor;}
	INLINE VOID SetColor(INT forecolor, INT backcolor) {m_foreColor = forecolor; m_backColor = backcolor;}
	INLINE VOID PutString(INT x, INT y, CONST CHAR *str, Charsets& Encode = GBK, DWORD wordspace = 0, INT count = 0x7FFFFFFF)
	{
		PutStringLimit(x, y, str, 0, m_FontSize - 1, 0, SCR_WIDTH - x, Encode, wordspace, count);
	}
	VOID PutStringLimit(INT x, INT y, CONST CHAR *str, INT top, INT bottom, INT left, INT right, Charsets& Encode = GBK, DWORD wordspace = 0, INT count = 0x7FFFFFFF);
	INLINE VOID PutStringVert(INT x, INT y, CONST CHAR *str, Charsets& Encode = GBK, DWORD wordspace = 0, INT count = 0x7FFFFFFF)
	{
		PutStringVertLimit(x, y, str, 0, m_FontSize - 1, 0, SCR_WIDTH - y, Encode, wordspace, count);
	}
	VOID PutStringVertLimit(INT x, INT y, CONST CHAR *str, INT top, INT bottom, INT left, INT right, Charsets& Encode = GBK, DWORD wordspace = 0, INT count = 0x7FFFFFFF);
	DWORD GetStringWidth(CONST CHAR *str, Charsets& Encode = GBK, DWORD wordspace = 0, INT count = 0x7FFFFFFF);
	INLINE BYTE GetCharWidth(WORD ch) {return m_charwidth[ch];}
	VOID FillVRAM();
	VOID FillRect(INT x1, INT y1, INT x2, INT y2);
	VOID PutPixel(INT x, INT y);
	VOID Rectangle(INT x1, INT y1, INT x2, INT y2);
	VOID Line(INT x1, INT y1, INT x2, INT y2);

	PIXEL * GetVRAMStart() CONST;
};

extern Display g_Display;
