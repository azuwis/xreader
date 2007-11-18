#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#ifdef ENABLE_GE 
#include <pspgu.h>
#endif
#include <pspdisplay.h>
#include "common/datatype.h"

extern int DISP_FONTSIZE, DISP_BOOK_FONTSIZE, HRR, WRR;
extern byte disp_ewidth[0x80];

// R,G,B color to word value color
#ifdef COLOR16BIT
typedef word pixel;
#define PIXEL_BYTES 2
#define COLOR_MAX 31
#define COLOR_WHITE 0xFFFF

#define RGB(r,g,b) ((((pixel)(b)*31/255)<<10)|(((pixel)(g)*31/255)<<5)|((pixel)(r)*31/255)|0x8000)
#define RGB2(r,g,b) ((((pixel)(b))<<10)|(((pixel)(g))<<5)|((pixel)(r))|0x8000)
#define RGBA(r,g,b,a) ((((pixel)(b))<<10)|(((pixel)(g))<<5)|((pixel)(r))|((a)?0x8000:0))
#define RGB_R(c) ((c) & 0x1F)
#define RGB_G(c) (((c) >> 5) & 0x1F)
#define RGB_B(c) (((c) >> 10) & 0x1F)
#define RGB_16to32(c) (c)

#else

typedef dword pixel;
#define PIXEL_BYTES 4
#define COLOR_MAX 255
#define COLOR_WHITE 0xFFFFFFFF

#define RGB(r,g,b) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|0xFF000000)
#define RGB2(r,g,b) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|0xFF000000)
#define RGBA(r,g,b,a) ((((pixel)(b))<<16)|(((pixel)(g))<<8)|((pixel)(r))|(((pixel)(a))<<24))
#define RGB_R(c) ((c) & 0xFF)
#define RGB_G(c) (((c) >> 8) & 0xFF)
#define RGB_B(c) (((c) >> 16) & 0xFF)
#define RGB_16to32(c) RGBA((((c)&0x1F)*255/31),((((c)>>5)&0x1F)*255/31),((((c)>>10)&0x1F)*255/31),((c&0x8000)?0xFF:0))

#endif

#define disp_grayscale(c,r,g,b,gs) RGB2(((r)*(gs)+RGB_R(c)*(100-(gs)))/100,((g)*(gs)+RGB_G(c)*(100-(gs)))/100,((b)*(gs)+RGB_B(c)*(100-(gs)))/100)

extern pixel * vram_start;

// sceDisplayWaitVblankStart function alias name, define is faster than function call (even at most time this is inline linked)
#define disp_waitv() sceDisplayWaitVblankStart()

#define disp_get_vaddr(x, y) (vram_start + (x) + ((y) << 9))

extern void disp_putpixel(int x, int y, pixel color);
extern void disp_init();
extern void init_gu(void);
extern void disp_set_fontsize(int fontsize);
extern void disp_set_book_fontsize(int fontsize);
extern bool disp_has_zipped_font(const char * zipfile, const char * efont, const char * cfont);
extern bool disp_load_zipped_font(const char * zipfile, const char * efont, const char * cfont);
extern bool disp_load_zipped_book_font(const char * zipfile, const char * efont, const char * cfont);
extern bool disp_load_truetype_book_font(const char * ettffile, const char *cttffile, int size);
extern bool disp_load_zipped_truetype_book_font(const char * zipfile, const char * ettffile, const char *cttffile, int size);
extern bool disp_has_font(const char * efont, const char * cfont);
extern bool disp_load_font(const char * efont, const char * cfont);
extern bool disp_load_book_font(const char * efont, const char * cfont);
extern void disp_assign_book_font();
extern void disp_free_font();
extern void disp_flip();
extern void disp_getimage(dword x, dword y, dword w, dword h, pixel * buf);
#ifdef ENABLE_GE
extern void disp_newputimage(int x, int y, int w, int h, int bufw, int startx, int starty, int ow, int oh, pixel * buf, bool swizzled);
#endif
extern void disp_putimage(dword x, dword y, dword w, dword h, dword startx, dword starty, pixel * buf);
extern void disp_duptocache();
extern void disp_duptocachealpha(int percent);
extern void disp_rectduptocache(dword x1, dword y1, dword x2, dword y2);
extern void disp_rectduptocachealpha(dword x1, dword y1, dword x2, dword y2, int percent);

extern void disp_putnstring(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot);
#define disp_putstring(x,y,color,str) disp_putnstring((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_FONTSIZE,0)

extern void disp_putnstringreversal(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot);
#define disp_putstringreversal(x,y,color,str) disp_putnstringreversal((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringhorz(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot);
#define disp_putstringhorz(x,y,color,str) disp_putnstringhorz((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringlvert(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot);
#define disp_putstringlvert(x,y,color,str) disp_putnstringlvert((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_putnstringrvert(int x, int y, pixel color, const byte *str, int count, dword wordspace, int top, int height, int bot);
#define disp_putstringrvert(x,y,color,str) disp_putnstringrvert((x),(y),(color),(str),0x7FFFFFFF,0,0,DISP_BOOK_FONTSIZE,0)

extern void disp_fillvram(pixel color);
extern void disp_fillrect(dword x1, dword y1, dword x2, dword y2, pixel color);
extern void disp_rectangle(dword x1, dword y1, dword x2, dword y2, pixel color);
extern void disp_line(dword x1, dword y1, dword x2, dword y2, pixel color);

#endif
