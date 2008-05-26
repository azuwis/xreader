#ifndef _WIN_H_
#define _WIN_H_

#include "common/datatype.h"
#include "display.h"
#include "buffer.h"

// Menu item structure
typedef struct
{
	buffer *compname;
	buffer *shortname;
	char name[128];				// item name
	dword width;				// display width in bytes (for align/span)
	pixel icolor;				// font color
	pixel selicolor;			// selected font color
	pixel selrcolor;			// selected rectangle line color
	pixel selbcolor;			// selected background filling color
	bool selected;				// item selected
	void *data;					// custom data for user processing
	word data2[4];
	dword data3;
} t_win_menuitem, *p_win_menuitem;

typedef struct _win_menu_predraw_data
{
	int max_item_len;
	int item_count;
	int x, y;
	int left, upper;
	int linespace;
} win_menu_predraw_data;

// Menu callback
typedef enum
{
	win_menu_op_continue = 0,
	win_menu_op_redraw,
	win_menu_op_ok,
	win_menu_op_cancel,
	win_menu_op_force_redraw
} t_win_menu_op;
typedef t_win_menu_op(*t_win_menu_callback) (dword key, p_win_menuitem item,
											 dword * count, dword page_count,
											 dword * topindex, dword * index);
typedef void (*t_win_menu_draw) (p_win_menuitem item, dword index,
								 dword topindex, dword max_height);

// Default callback for menu
extern t_win_menu_op win_menu_defcb(dword key, p_win_menuitem item,
									dword * count, dword page_count,
									dword * topindex, dword * index);

// Menu show & wait for input with callback supported
extern dword win_menu(dword x, dword y, dword max_width, dword max_height,
					  p_win_menuitem item, dword count, dword initindex,
					  dword linespace, pixel bgcolor, bool redraw,
					  t_win_menu_draw predraw, t_win_menu_draw postdraw,
					  t_win_menu_callback cb);

// Messagebox with yes/no
extern bool win_msgbox(const char *prompt, const char *yesstr,
					   const char *nostr, pixel fontcolor, pixel bordercolor,
					   pixel bgcolor);

// Message with any key pressed to close
extern void win_msg(const char *prompt, pixel fontcolor, pixel bordercolor,
					pixel bgcolor);

extern void win_item_destroy(p_win_menuitem * item, dword * size);
extern p_win_menuitem win_realloc_items(p_win_menuitem item, int orgsize,
										int newsize);
extern p_win_menuitem win_copy_item(p_win_menuitem dst,
									const p_win_menuitem src);

extern int win_get_max_length(const p_win_menuitem pItem, int size);
extern int win_get_max_pixel_width(const p_win_menuitem pItem, int size);

#endif
