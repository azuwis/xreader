/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <string.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include "pspscreen.h"
#include "scene.h"
#include "display.h"
#include "version.h"
#include "conf.h"
#include "xrPrx/xrPrx.h"

extern t_fonts fonts[5], bookfonts[21];
extern int fontindex, fontcount, bookfontcount, bookfontindex;
extern bool scene_load_font();
extern bool scene_load_book_font();

static char conf_filename[256];

extern void conf_set_file(const char * filename)
{
	strcpy(conf_filename, filename);
}

extern void conf_get_keyname(dword key, char * res)
{
	res[0] = 0;
	if((key & PSP_CTRL_CIRCLE) > 0)
		strcat(res, "○");
	if((key & PSP_CTRL_CROSS) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+×");
		else
			strcat(res, "×");
	}
	if((key & PSP_CTRL_SQUARE) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+□");
		else
			strcat(res, "□");
	}
	if((key & PSP_CTRL_TRIANGLE) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+△");
		else
			strcat(res, "△");
	}
	if((key & PSP_CTRL_LTRIGGER) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+L");
		else
			strcat(res, "L");
	}
	if((key & PSP_CTRL_RTRIGGER) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+R");
		else
			strcat(res, "R");
	}
	if((key & PSP_CTRL_UP) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+↑");
		else
			strcat(res, "↑");
	}
	if((key & PSP_CTRL_DOWN) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+↓");
		else
			strcat(res, "↓");
	}
	if((key & PSP_CTRL_LEFT) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+←");
		else
			strcat(res, "←");
	}
	if((key & PSP_CTRL_RIGHT) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+→");
		else
			strcat(res, "→");
	}
	if((key & PSP_CTRL_SELECT) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+SELECT");
		else
			strcat(res, "SELECT");
	}
	if((key & PSP_CTRL_START) > 0)
	{
		if(res[0] != 0)
			strcat(res, "+START");
		else
			strcat(res, "START");
	}
}

static void conf_default(p_conf conf)
{
	memset(conf, 0, sizeof(t_conf));
	conf->forecolor = 0xFFFFFFFF;
	conf->bgcolor = 0;
	conf->rowspace = 2;
	conf->wordspace = 0;
	conf->borderspace = 0;
	conf->vertread = 0;
	conf->infobar = true;
	conf->rlastrow = false;
	conf->autobm = true;
	conf->encode = conf_encode_gbk;
	conf->fit = conf_fit_none;
	conf->imginfobar = false;
	conf->scrollbar = false;
	conf->scale = 0;
	conf->rotate = conf_rotate_0;
	conf->enable_analog = true;
	conf->img_enable_analog = true;
	conf->txtkey[0] = PSP_CTRL_SQUARE;
	conf->txtkey[1] = PSP_CTRL_LTRIGGER;
	conf->txtkey[2] = PSP_CTRL_RTRIGGER;
	conf->txtkey[3] = PSP_CTRL_UP | PSP_CTRL_CIRCLE;
	conf->txtkey[4] = PSP_CTRL_DOWN | PSP_CTRL_CIRCLE;
	conf->txtkey[5] = PSP_CTRL_LEFT | PSP_CTRL_CIRCLE;
	conf->txtkey[6] = PSP_CTRL_RIGHT | PSP_CTRL_CIRCLE;
	conf->txtkey[7] = PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE;
	conf->txtkey[8] = PSP_CTRL_RTRIGGER | PSP_CTRL_CIRCLE;
	conf->txtkey[9] = 0;
	conf->txtkey[10] = 0;
	conf->txtkey[11] = PSP_CTRL_CROSS;
	conf->txtkey[12] = PSP_CTRL_TRIANGLE;
	conf->imgkey[0] = PSP_CTRL_LTRIGGER;
	conf->imgkey[1] = PSP_CTRL_RTRIGGER;
	conf->imgkey[2] = PSP_CTRL_TRIANGLE;
	conf->imgkey[3] = PSP_CTRL_UP | PSP_CTRL_CIRCLE;
	conf->imgkey[4] = PSP_CTRL_DOWN | PSP_CTRL_CIRCLE;
	conf->imgkey[5] = PSP_CTRL_LEFT | PSP_CTRL_CIRCLE;
	conf->imgkey[6] = PSP_CTRL_RIGHT | PSP_CTRL_CIRCLE;
	conf->imgkey[7] = PSP_CTRL_SQUARE;
	conf->imgkey[8] = PSP_CTRL_CIRCLE;
	conf->imgkey[9] = PSP_CTRL_CROSS;
	conf->imgkey[10] = PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE;
	conf->flkey[0] = PSP_CTRL_CIRCLE;
	conf->flkey[1] = PSP_CTRL_LTRIGGER;
	conf->flkey[2] = PSP_CTRL_RTRIGGER;
	conf->flkey[3] = PSP_CTRL_CROSS;
	conf->flkey[4] = 0;
	conf->flkey[5] = PSP_CTRL_TRIANGLE;
	conf->flkey[6] = PSP_CTRL_SQUARE;
	conf->bicubic = false;
	conf->mp3encode = conf_encode_gbk;
	conf->lyricencode = conf_encode_gbk;
	conf->mp3cycle = conf_cycle_repeat;
	conf->isreading = false;
	conf->slideinterval = 5;
	conf->hprmctrl = false;
	conf->grayscale = 30;
	conf->showhidden = true;
	conf->showunknown = true;
	conf->showfinfo = true;
	conf->allowdelete = true;
	conf->arrange = conf_arrange_ext;
	conf->enableusb = false;
	conf->viewpos = conf_viewpos_leftup;
	conf->imgmvspd = 8;
	conf->imgpaging = conf_imgpaging_direct;
	conf->fontsize = 12;
	conf->bookfontsize = 12;
	conf->reordertxt = false;
	conf->pagetonext = false;
	conf->autopage = 0;
	conf->autopagetype = 2;
	conf->autolinedelay = 0;
	conf->thumb = conf_thumb_scroll;
	conf->imgpagereserve = 0;
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	conf->lyricex = 1;
#else
	conf->lyricex = 0;
#endif
	conf->savesucc = false;
	conf->autoplay = false;
	conf->usettf = 0;
	conf->freqs[0] = 1;
	conf->freqs[1] = 5;
	conf->freqs[2] = 8;
	conf->imgbrightness = 100;
	conf->dis_scrsave = false;
	conf->autosleep = 0;
	conf->load_exif = true;
	conf->brightness = 50;
	conf->prev_autopage = 2;
}

extern bool conf_load(p_conf conf)
{
	conf_default(conf);
	int fd = sceIoOpen(conf_filename, PSP_O_RDWR, 0777);
	if(fd < 0)
		return false;
	sceIoRead(fd, conf, sizeof(t_conf));
#ifndef ENABLE_USB
	conf->enableusb = false;
#endif
#ifndef ENABLE_BG
	strcpy(conf->bgfile, "");
#endif
#ifndef ENABLE_MUSIC
	conf->autoplay = false;
	conf->hprmctrl = true;
	conf->lyricex = 0;
#else
#ifndef ENABLE_LYRIC
	conf->lyricex = 0;
#endif
#endif
	if(!conf->savesucc)
	{
		strcpy(conf->path, "ms0:/");
		strcpy(conf->shortpath, "ms0:/");
		strcpy(conf->lastfile, "");
		strcpy(conf->bgfile, "");
		conf->isreading = false;
	}
	else
		conf->savesucc = false;
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoWrite(fd, conf, sizeof(t_conf));
	sceIoClose(fd);
	conf->savesucc = true;

	int i=0;
	for(i=0; i<fontcount; ++i) {
		if(fonts[i].size == conf->fontsize)
		{
			break;
		}
	}
	if(i != fontcount) {
		fontindex = i;
		scene_load_font();
	}
	
	for(i=0; i<bookfontcount; ++i) {
		if(bookfonts[i].size == conf->bookfontsize)
		{
			break;
		}
	}
	if(i != bookfontcount) {
		bookfontindex = i;
		scene_load_book_font();
	}

	return true;
}

extern bool conf_save(p_conf conf)
{
	extern bool prx_loaded;
	if(prx_loaded)
		conf->brightness = xrGetBrightness();
	conf->fontsize = fonts[fontindex].size;

	int fd = sceIoOpen(conf_filename, PSP_O_CREAT | PSP_O_RDWR, 0777);
	if(fd < 0)
		return false;
	sceIoWrite(fd, conf, sizeof(t_conf));
	sceIoClose(fd);
	return true;
}

extern const char * conf_get_encodename(t_conf_encode encode)
{
	switch(encode)
	{
	case conf_encode_gbk:
		return "GBK ";
	case conf_encode_big5:
		return "BIG5";
	case conf_encode_sjis:
		return "SJIS";
	case conf_encode_ucs:
		return "UCS ";
	case conf_encode_utf8:
		return "UTF-8";
	default:
		return "";
	}
}

extern const char * conf_get_fitname(t_conf_fit fit)
{
	switch(fit)
	{
	case conf_fit_none:
		return "原始大小";
		break;
	case conf_fit_width:
		return "适应宽度";
		break;
	case conf_fit_dblwidth:
		return "适应两倍宽度";
		break;
	case conf_fit_height:
		return "适应高度";
		break;
	case conf_fit_dblheight:
		return "适应两倍高度";
		break;
	case conf_fit_custom:
		return "";
		break;
	default:
		return "";
		break;
	}
}

extern const char * conf_get_cyclename(t_conf_cycle cycle)
{
	switch(cycle)
	{
	case conf_cycle_single:
		return "顺序播放";
	case conf_cycle_repeat:
		return "循环播放";
	case conf_cycle_repeat_one:
		return "循环单曲";
	case conf_cycle_random:
		return "随机播放";
	default:
		return "";
	}
}

extern const char * conf_get_arrangename(t_conf_arrange arrange)
{
	switch(arrange)
	{
	case conf_arrange_ext:
		return "按扩展名";
		break;
	case conf_arrange_name:
		return "按文件名";
		break;
	case conf_arrange_size:
		return "按文件大小";
		break;
	case conf_arrange_ctime:
		return "按创建时间";
		break;
	case conf_arrange_mtime:
		return "按修改时间";
		break;
	default:
		return "";
	}
}

extern const char * conf_get_viewposname(t_conf_viewpos viewpos)
{
	switch(viewpos)
	{
	case conf_viewpos_leftup:
		return "左上";
	case conf_viewpos_midup:
		return "中上";
	case conf_viewpos_rightup:
		return "右上";
	case conf_viewpos_leftmid:
		return "左中";
	case conf_viewpos_midmid:
		return "正中";
	case conf_viewpos_rightmid:
		return "右中";
	case conf_viewpos_leftdown:
		return "左下";
	case conf_viewpos_middown:
		return "中下";
	case conf_viewpos_rightdown:
		return "右下";
	default:
		return "";
	}
}

extern const char * conf_get_imgpagingname(t_conf_imgpaging imgpaging)
{
	switch(imgpaging)
	{
	case conf_imgpaging_direct:
		return "直接翻页";
	case conf_imgpaging_updown:
		return "先上下卷动";
	case conf_imgpaging_leftright:
		return "先左右卷动";
	default:
		return "";
	}
}

extern const char * conf_get_thumbname(t_conf_thumb thumb)
{
	switch(thumb)
	{
	case conf_thumb_none:
		return "不显示";
	case conf_thumb_scroll:
		return "卷动时显示";
	case conf_thumb_always:
		return "总是显示";
	default:
		return "";
	}
}

extern const char * conf_get_infobarname(t_conf_infobar infobar)
{
	switch(infobar)
	{
	case conf_infobar_none:
		return "不显示";
	case conf_infobar_info:
		return "阅读信息";
#ifdef ENABLE_LYRIC
	case conf_infobar_lyric:
		return "歌词";
#endif
	default:
		return "";
	}
}
