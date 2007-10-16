#ifndef _CONF_H_
#define _CONF_H_

#include "common/datatype.h"

typedef enum {
	conf_encode_gbk = 0,
	conf_encode_big5 = 1,
	conf_encode_sjis = 2,
	conf_encode_ucs = 3,
	conf_encode_utf8 = 4
} t_conf_encode;

typedef enum {
	conf_fit_none = 0,
	conf_fit_width = 1,
	conf_fit_dblwidth = 2,
	conf_fit_height = 3,
	conf_fit_dblheight = 4,
	conf_fit_custom = 5
} t_conf_fit;

typedef enum {
	conf_rotate_0 = 0,
	conf_rotate_90 = 1,
	conf_rotate_180 = 2,
	conf_rotate_270 = 3,
} t_conf_rotate;

typedef enum {
	conf_cycle_single = 0,
	conf_cycle_repeat = 1,
	conf_cycle_repeat_one = 2,
	conf_cycle_random = 3
} t_conf_cycle;

typedef enum {
	conf_arrange_ext = 0,
	conf_arrange_name = 1,
	conf_arrange_size = 2,
	conf_arrange_ctime = 3,
	conf_arrange_mtime = 4
} t_conf_arrange;

typedef enum {
	conf_viewpos_leftup = 0,
	conf_viewpos_midup = 1,
	conf_viewpos_rightup = 2,
	conf_viewpos_leftmid = 3,
	conf_viewpos_midmid = 4,
	conf_viewpos_rightmid = 5,
	conf_viewpos_leftdown = 6,
	conf_viewpos_middown = 7,
	conf_viewpos_rightdown = 8,
} t_conf_viewpos;

typedef enum {
	conf_imgpaging_direct = 0,
	conf_imgpaging_updown = 1,
	conf_imgpaging_leftright = 2,
} t_conf_imgpaging;

typedef enum {
	conf_thumb_none = 0,
	conf_thumb_scroll = 1,
	conf_thumb_always = 2
} t_conf_thumb;

typedef enum {
	conf_infobar_none = 0,
	conf_infobar_info = 1,
	conf_infobar_lyric = 2
} t_conf_infobar;

typedef struct {
	char path[256];
	dword forecolor;
	dword bgcolor;
	dword rowspace;
	t_conf_infobar infobar;
	bool rlastrow;
	bool autobm;
	dword vertread;
	t_conf_encode encode;
	t_conf_fit fit;
	bool imginfobar;
	bool scrollbar;
	dword scale;
	t_conf_rotate rotate;
	dword txtkey[20];
	dword imgkey[20];
	char shortpath[256];
	dword confver;
	bool bicubic;
	dword wordspace;
	dword borderspace;
	char lastfile[256];
	t_conf_encode mp3encode;
	t_conf_cycle mp3cycle;
	bool isreading;
	char bgfile[256];
	dword slideinterval;
	bool hprmctrl;
	dword grayscale;
	bool showhidden;
	bool showunknown;
	bool showfinfo;
	bool allowdelete;
	t_conf_arrange arrange;
	bool enableusb;
	t_conf_viewpos viewpos;
	dword imgmvspd;
	t_conf_imgpaging imgpaging;
	dword flkey[20];
	int fontsize;
	bool reordertxt;
	bool pagetonext;
	int  autopage;
	bool thumb;
	int bookfontsize;
	dword txtkey2[20];
	dword imgkey2[20];
	dword flkey2[20];
	dword imgpagereserve;
	dword lyricex;
	bool savesucc;
	bool autoplay;
	bool usettf;
} t_conf, * p_conf;

/* txt key:
	0 - bookmark;
	1 - prevpage;
	2 - nextpage;
	3 - prev100;
	4 - next100;
	5 - prev500;
	6 - next500;
	7 - firstpage;
	8 - lastpage;
	9 - prevbook;
	10 - nextbook;
	11 - exitbook;
	above 11 - reversed for future version

   image key:
	0 - imageprev;
	1 - imagenext;
	2 - imagescaletype;
	3 - imagescalesmaller;
	4 - imagescalelarger;
	5 - imagerotateleft;
	6 - imagerotateright;
	7 - imagebar;
	8 - imageinfo;
	9 - imageexit;
	above 9 - reversed for future version
*/

extern void conf_set_file(const char * filename);
extern void conf_get_keyname(dword key, char * res);
extern bool conf_load(p_conf conf);
extern bool conf_save(p_conf conf);
extern const char * conf_get_encodename(t_conf_encode encode);
extern const char * conf_get_fitname(t_conf_fit fit);
extern const char * conf_get_cyclename(t_conf_cycle cycle);
extern const char * conf_get_arrangename(t_conf_arrange arrange);
extern const char * conf_get_viewposname(t_conf_viewpos viewpos);
extern const char * conf_get_imgpagingname(t_conf_imgpaging imgpaging);
extern const char * conf_get_thumbname(t_conf_thumb thumb);
extern const char * conf_get_infobarname(t_conf_infobar infobar);

#endif
