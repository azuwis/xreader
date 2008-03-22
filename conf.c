/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include "common/utils.h"
#include "pspscreen.h"
#include "scene.h"
#include "display.h"
#include "version.h"
#include "conf.h"
#include "iniparser.h"
#include "xrPrx/xrPrx.h"
#include "dbg.h"

extern bool scene_load_font();
extern bool scene_load_book_font();

static char conf_filename[PATH_MAX];

extern void conf_set_file(const char *filename)
{
	STRCPY_S(conf_filename, filename);
}

extern void conf_get_keyname(dword key, char *res)
{
	res[0] = 0;
	if ((key & PSP_CTRL_CIRCLE) > 0)
		strcat_s(res, 256, "○");
	if ((key & PSP_CTRL_CROSS) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+×");
		else
			strcat_s(res, 256, "×");
	}
	if ((key & PSP_CTRL_SQUARE) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+□");
		else
			strcat_s(res, 256, "□");
	}
	if ((key & PSP_CTRL_TRIANGLE) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+△");
		else
			strcat_s(res, 256, "△");
	}
	if ((key & PSP_CTRL_LTRIGGER) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+L");
		else
			strcat_s(res, 256, "L");
	}
	if ((key & PSP_CTRL_RTRIGGER) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+R");
		else
			strcat_s(res, 256, "R");
	}
	if ((key & PSP_CTRL_UP) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+↑");
		else
			strcat_s(res, 256, "↑");
	}
	if ((key & PSP_CTRL_DOWN) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+↓");
		else
			strcat_s(res, 256, "↓");
	}
	if ((key & PSP_CTRL_LEFT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+←");
		else
			strcat_s(res, 256, "←");
	}
	if ((key & PSP_CTRL_RIGHT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+→");
		else
			strcat_s(res, 256, "→");
	}
	if ((key & PSP_CTRL_SELECT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+SELECT");
		else
			strcat_s(res, 256, "SELECT");
	}
	if ((key & PSP_CTRL_START) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+START");
		else
			strcat_s(res, 256, "START");
	}
}

static void conf_default(p_conf conf)
{
	char appdir[PATH_MAX];

	getcwd(appdir, PATH_MAX);
	STRCAT_S(appdir, "/");
	memset(conf, 0, sizeof(t_conf));
	STRCPY_S(conf->path, "ms0:/");
	STRCPY_S(conf->shortpath, "ms0:/");
	STRCPY_S(conf->lastfile, "");
	STRCPY_S(conf->bgarch, "");
	STRCPY_S(conf->bgfile, appdir);
	STRCAT_S(conf->bgfile, "bg.png");
	conf->bgwhere = scene_in_zip;
	conf->confver = XREADER_VERSION_NUM;
	conf->forecolor = 0xFFFFFFFF;
	conf->bgcolor = 0;
	conf->have_bg = true;
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
	conf->arrange = conf_arrange_name;
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
	conf->launchtype = 2;

	/*
	   conf->titlecolor = RGB(0x80, 0x10, 0x10);
	   conf->menutextcolor = RGB(0xDF, 0xDF, 0xDF);
	   conf->menubcolor  = RGB(0x40, 0x10, 0x10);
	   conf->selicolor = RGB(0xFF, 0xFF, 0x40);
	   conf->selbcolor = RGB(0x20, 0x20, 0xDF);
	   conf->msgbcolor = RGB(0x18, 0x28, 0x50);
	 */
	conf->titlecolor = RGB(0x30, 0x60, 0x30);
	conf->menutextcolor = RGB(0xDF, 0xDF, 0xDF);
	conf->menubcolor = RGB(0x10, 0x30, 0x20);
	conf->selicolor = RGB(0xFF, 0xFF, 0x40);
	conf->selbcolor = RGB(0x20, 0x20, 0xDF);
	conf->msgbcolor = RGB(0x18, 0x28, 0x50);
	conf->usedyncolor = false;

	STRCPY_S(conf->cttfpath, appdir);
	STRCAT_S(conf->cttfpath, "fonts/gbk.ttf");
	STRCPY_S(conf->ettfpath, appdir);
	STRCAT_S(conf->ettfpath, "fonts/asc.ttf");
	conf->infobar_use_ttf_mode = true;

	conf->cfont_antialias = false;
	conf->cfont_cleartype = true;
	conf->cfont_embolden = false;

	conf->efont_antialias = false;
	conf->efont_cleartype = true;
	conf->efont_embolden = false;

	conf->img_no_repeat = false;
}

static char* hexToString(char* str, int size, unsigned int hex)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "0x%08x", hex);

	return str;
}

static char* booleanToString(char* str, int size, bool b)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%s", b ? "true" : "false");

	return str;
}

static char* intToString(char* str, int size, int i)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%ld", i);

	return str;
}

static char* dwordToString(char* str, int size, dword dw)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%lu", dw);

	return str;
}

extern bool ini_conf_load(const char *inifilename, p_conf conf)
{
	if (conf == NULL || inifilename == NULL) {
		return false;
	}

	dictionary *dict = iniparser_load(inifilename);

	if (dict == NULL)
		return false;

	conf_default(conf);

	STRCPY_S(conf->path, iniparser_getstring(dict, "Global:path", conf->path));
	conf->forecolor = iniparser_getunsigned(dict, "UI:forecolor", conf->forecolor);
	conf->bgcolor = iniparser_getunsigned(dict, "UI:bgcolor", conf->bgcolor);
	conf->have_bg = iniparser_getboolean(dict, "UI:have_bg", conf->have_bg);
	conf->titlecolor = iniparser_getunsigned(dict, "UI:titlecolor", conf->titlecolor);
	conf->menutextcolor = iniparser_getunsigned(dict, "UI:menutextcolor", conf->menutextcolor);
	conf->menubcolor = iniparser_getunsigned(dict, "UI:menubcolor", conf->menubcolor);
	conf->selicolor = iniparser_getunsigned(dict, "UI:selicolor", conf->selicolor);
	conf->selbcolor = iniparser_getunsigned(dict, "UI:selbcolor", conf->selbcolor);
	conf->msgbcolor = iniparser_getunsigned(dict, "UI:msgbcolor", conf->msgbcolor);
	conf->usedyncolor = iniparser_getboolean(dict, "UI:usedyncolor", conf->usedyncolor);
	conf->rowspace = iniparser_getint(dict, "Text:rowspace", conf->rowspace);
	conf->infobar = iniparser_getint(dict, "Text:infobar", conf->infobar);
	conf->rlastrow = iniparser_getboolean(dict, "Text:rlastrow", conf->rlastrow);
	conf->autobm = iniparser_getboolean(dict, "Text:autobm", conf->autobm);
	conf->vertread = iniparser_getint(dict, "Text:vertread", conf->vertread);
	conf->encode = iniparser_getint(dict, "Text:encode", conf->encode);
	conf->fit = iniparser_getint(dict, "Image:fit", conf->fit);
	conf->imginfobar = iniparser_getboolean(dict, "Image:imginfobar", conf->imginfobar);
	conf->scrollbar = iniparser_getboolean(dict, "Text:scrollbar", conf->scrollbar);
	conf->scale = iniparser_getint(dict, "Image:scale", conf->scale);
	conf->rotate = iniparser_getint(dict, "Image:rotate", conf->rotate);
	int i;
	for(i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Text:txtkey1_%02d", i);
		conf->txtkey[i] = iniparser_getint(dict, key, conf->txtkey[i]);
		SPRINTF_S(key, "Image:imgkey1_%02d", i);
		conf->imgkey[i] = iniparser_getint(dict, key, conf->imgkey[i]);
	}
	STRCPY_S(conf->shortpath, iniparser_getstring(dict, "Global:shortpath", conf->shortpath));
	conf->confver = iniparser_getint(dict, "Global:confver", conf->confver);
	conf->bicubic = iniparser_getboolean(dict, "Image:bicubic", conf->bicubic);
	conf->wordspace = iniparser_getint(dict, "Text:wordspace", conf->wordspace);
	STRCPY_S(conf->lastfile, iniparser_getstring(dict, "Global:lastfile", conf->lastfile));
	conf->mp3encode = iniparser_getint(dict, "Music:mp3encode", conf->mp3encode);
	conf->lyricencode = iniparser_getint(dict, "Music:lyricencode", conf->lyricencode);
	conf->mp3cycle = iniparser_getint(dict, "Music:mp3cycle", conf->mp3cycle);
	conf->isreading = iniparser_getboolean(dict, "Global:isreading", conf->isreading);
	STRCPY_S(conf->bgarch, iniparser_getstring(dict, "UI:bgarch", conf->bgarch));
	STRCPY_S(conf->bgfile, iniparser_getstring(dict, "UI:bgfile", conf->bgfile));
	conf->bgwhere = iniparser_getint(dict, "UI:bgwhere", conf->bgwhere);
	conf->slideinterval = iniparser_getint(dict, "Image:slideinterval", conf->slideinterval);
	conf->hprmctrl = iniparser_getboolean(dict, "Music:hprmctrl", conf->hprmctrl);
	conf->grayscale = iniparser_getint(dict, "UI:grayscale", conf->grayscale);
	conf->showhidden = iniparser_getboolean(dict, "Global:showhidden", conf->showhidden);
	conf->showunknown = iniparser_getboolean(dict, "Global:showunknown", conf->showunknown);
	conf->showfinfo = iniparser_getboolean(dict, "Global:showfinfo", conf->showfinfo);
	conf->allowdelete = iniparser_getboolean(dict, "Global:allowdelete", conf->allowdelete);
	conf->arrange = iniparser_getint(dict, "Global:arrange", conf->arrange);
	conf->enableusb = iniparser_getboolean(dict, "Global:enableusb", conf->enableusb);
	conf->viewpos = iniparser_getint(dict, "Image:viewpos", conf->viewpos);
	conf->imgmvspd = iniparser_getint(dict, "Image:imgmvspd", conf->imgmvspd);
	conf->imgpaging = iniparser_getint(dict, "Image:imgpaging", conf->imgpaging);
	for (i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Global:flkey1_%02d", i);
		conf->flkey[i] = iniparser_getint(dict, key, conf->flkey[i]);
	}
	conf->fontsize = iniparser_getint(dict, "UI:fontsize", conf->fontsize);
	conf->reordertxt = iniparser_getboolean(dict, "Text:reordertxt", conf->reordertxt);
	conf->pagetonext = iniparser_getboolean(dict, "Text:pagetonext", conf->pagetonext);
	conf->autopage = iniparser_getint(dict, "Text:autopage", conf->autopage);
	conf->prev_autopage = iniparser_getint(dict, "Text:prev_autopage", conf->prev_autopage);
	conf->autolinedelay = iniparser_getint(dict, "Text:autolinedelay", conf->autolinedelay);
	conf->thumb = iniparser_getboolean(dict, "UI:fontsize", conf->thumb);
	conf->bookfontsize = iniparser_getint(dict, "Text:bookfontsize", conf->bookfontsize);
	conf->enable_analog = iniparser_getboolean(dict, "Text:enable_analog", conf->enable_analog);
	conf->img_enable_analog = iniparser_getboolean(dict, "Image:img_enable_analog", conf->img_enable_analog);
	for (i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Text:txtkey2_%02d", i);
		conf->txtkey2[i] = iniparser_getint(dict, key, conf->txtkey2[i]);
		SPRINTF_S(key, "Image:imgkey2_%02d", i);
		conf->imgkey2[i] = iniparser_getint(dict, key, conf->imgkey2[i]);
		SPRINTF_S(key, "Global:flkey2_%02d", i);
		conf->flkey2[i] = iniparser_getint(dict, key, conf->flkey2[i]);
	}
	conf->imgpagereserve = iniparser_getint(dict, "Image:imgpagereserve", conf->imgpagereserve);
	conf->lyricex = iniparser_getint(dict, "Music:lyricex", conf->lyricex);
	conf->autoplay = iniparser_getboolean(dict, "Music:autoplay", conf->autoplay);
	conf->usettf = iniparser_getboolean(dict, "Text:usettf", conf->usettf);
	STRCPY_S(conf->cttfarch, iniparser_getstring(dict, "Text:cttfarch", conf->cttfarch));
	STRCPY_S(conf->cttfpath, iniparser_getstring(dict, "Text:cttfpath", conf->cttfpath));
	STRCPY_S(conf->ettfarch, iniparser_getstring(dict, "Text:ettfarch", conf->ettfarch));
	STRCPY_S(conf->ettfpath, iniparser_getstring(dict, "Text:ettfpath", conf->ettfpath));
	for (i=0; i<3; ++i) {
		char key[20];
		SPRINTF_S(key, "Global:freqs_%d", i);
		conf->freqs[i] = iniparser_getint(dict, key, conf->freqs[i]);
	}
	conf->imgbrightness = iniparser_getint(dict, "Image:imgbrightness", conf->imgbrightness);
	conf->dis_scrsave = iniparser_getboolean(dict, "Global:dis_scrsave", conf->dis_scrsave);
	conf->autosleep = iniparser_getint(dict, "Global:autosleep", conf->autosleep);
	conf->load_exif = iniparser_getboolean(dict, "Image:load_exif", conf->load_exif);
	conf->brightness = iniparser_getint(dict, "Global:brightness", conf->brightness);
	conf->launchtype = iniparser_getint(dict, "Global:launchtype", conf->launchtype);
	conf->infobar_use_ttf_mode = iniparser_getboolean(dict, "Text:infobar_use_ttf_mode", conf->infobar_use_ttf_mode);

	conf->cfont_antialias = iniparser_getboolean(dict, "Text:cfont_antialias", conf->cfont_antialias);
	conf->cfont_cleartype = iniparser_getboolean(dict, "Text:cfont_cleartype", conf->cfont_cleartype);
	conf->cfont_embolden = iniparser_getboolean(dict, "Text:cfont_embolden", conf->cfont_embolden);

	conf->efont_antialias = iniparser_getboolean(dict, "Text:efont_antialias", conf->efont_antialias);
	conf->efont_cleartype = iniparser_getboolean(dict, "Text:efont_cleartype", conf->efont_cleartype);
	conf->efont_embolden = iniparser_getboolean(dict, "Text:efont_embolden", conf->efont_embolden);

	conf->img_no_repeat = iniparser_getboolean(dict, "Image:no_repeat", conf->img_no_repeat);

	dictionary_del(dict);

	return true;
}

extern bool ini_conf_save(p_conf conf)
{
	if (conf == NULL) {
		return false;
	}

	dictionary *dict = dictionary_new(0);

	if (dict == NULL)
		return false;

	FILE *fp = fopen(conf_filename, "w");
	if (fp == NULL) {
		return false;
	}

	if (iniparser_setstring(dict, "Global", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "UI", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Text", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Image", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Music", NULL) != 0)
		goto error;

	char buf[PATH_MAX];

	iniparser_setstring(dict, "Global:path", conf->path);
	iniparser_setstring(dict, "UI:forecolor", hexToString(buf, sizeof(buf), conf->forecolor));
	iniparser_setstring(dict, "UI:bgcolor", hexToString(buf, sizeof(buf), conf->bgcolor));
	iniparser_setstring(dict, "UI:have_bg", booleanToString(buf, sizeof(buf), conf->have_bg));
	iniparser_setstring(dict, "UI:titlecolor", hexToString(buf, sizeof(buf), conf->titlecolor));
	iniparser_setstring(dict, "UI:menutextcolor", hexToString(buf, sizeof(buf), conf->menutextcolor));
	iniparser_setstring(dict, "UI:menubcolor", hexToString(buf, sizeof(buf), conf->menubcolor));
	iniparser_setstring(dict, "UI:selicolor", hexToString(buf, sizeof(buf), conf->selicolor));
	iniparser_setstring(dict, "UI:selbcolor", hexToString(buf, sizeof(buf), conf->selbcolor));
	iniparser_setstring(dict, "UI:msgbcolor", hexToString(buf, sizeof(buf), conf->msgbcolor));
	iniparser_setstring(dict, "UI:usedyncolor", booleanToString(buf, sizeof(buf), conf->usedyncolor));
	iniparser_setstring(dict, "Text:rowspace", dwordToString(buf, sizeof(buf), conf->rowspace));
	iniparser_setstring(dict, "Text:infobar", dwordToString(buf, sizeof(buf), conf->infobar));
	iniparser_setstring(dict, "Text:rlastrow", booleanToString(buf, sizeof(buf), conf->rlastrow));
	iniparser_setstring(dict, "Text:autobm", booleanToString(buf, sizeof(buf), conf->autobm));
	iniparser_setstring(dict, "Text:vertread", dwordToString(buf, sizeof(buf), conf->vertread));
	iniparser_setstring(dict, "Text:encode", dwordToString(buf, sizeof(buf), conf->encode));
	iniparser_setstring(dict, "Image:fit", dwordToString(buf, sizeof(buf), conf->fit));
	iniparser_setstring(dict, "Image:imginfobar", booleanToString(buf, sizeof(buf), conf->imginfobar));
	iniparser_setstring(dict, "Text:scrollbar", booleanToString(buf, sizeof(buf), conf->scrollbar));
	iniparser_setstring(dict, "Image:scale", dwordToString(buf, sizeof(buf), conf->scale));
	iniparser_setstring(dict, "Image:rotate", dwordToString(buf, sizeof(buf), conf->rotate));
	int i;
	for(i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Text:txtkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->txtkey[i]));
		SPRINTF_S(key, "Image:imgkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->imgkey[i]));
	}
	iniparser_setstring(dict, "Global:shortpath", conf->shortpath);
	iniparser_setstring(dict, "Global:confver", hexToString(buf, sizeof(buf), conf->confver));
	iniparser_setstring(dict, "Image:bicubic", booleanToString(buf, sizeof(buf), conf->bicubic));
	iniparser_setstring(dict, "Text:wordspace", dwordToString(buf, sizeof(buf), conf->wordspace));
	iniparser_setstring(dict, "Text:borderspace", dwordToString(buf, sizeof(buf), conf->borderspace));
	iniparser_setstring(dict, "Global:lastfile", conf->lastfile);
	iniparser_setstring(dict, "Music:mp3encode", dwordToString(buf, sizeof(buf), conf->mp3encode));
	iniparser_setstring(dict, "Music:lyricencode", dwordToString(buf, sizeof(buf), conf->lyricencode));
	iniparser_setstring(dict, "Music:mp3cycle", dwordToString(buf, sizeof(buf), conf->mp3cycle));
	iniparser_setstring(dict, "Global:isreading", booleanToString(buf, sizeof(buf), conf->isreading));
	iniparser_setstring(dict, "UI:bgarch", conf->bgarch);
	iniparser_setstring(dict, "UI:bgfile", conf->bgfile);
	iniparser_setstring(dict, "UI:bgwhere", intToString(buf, sizeof(buf), conf->bgwhere));
	iniparser_setstring(dict, "Image:slideinterval", dwordToString(buf, sizeof(buf), conf->slideinterval));
	iniparser_setstring(dict, "Music:hprmctrl", booleanToString(buf, sizeof(buf), conf->hprmctrl));
	iniparser_setstring(dict, "UI:grayscale", intToString(buf, sizeof(buf), conf->grayscale));
	iniparser_setstring(dict, "Global:showhidden", booleanToString(buf, sizeof(buf), conf->showhidden));
	iniparser_setstring(dict, "Global:showunknown", booleanToString(buf, sizeof(buf), conf->showunknown));
	iniparser_setstring(dict, "Global:showfinfo", booleanToString(buf, sizeof(buf), conf->showfinfo));
	iniparser_setstring(dict, "Global:allowdelete", booleanToString(buf, sizeof(buf), conf->allowdelete));
	iniparser_setstring(dict, "Global:arrange", dwordToString(buf, sizeof(buf), conf->arrange));
	iniparser_setstring(dict, "Global:enableusb", booleanToString(buf, sizeof(buf), conf->enableusb));
	iniparser_setstring(dict, "Image:viewpos", dwordToString(buf, sizeof(buf), conf->viewpos));
	iniparser_setstring(dict, "Image:imgmvspd", dwordToString(buf, sizeof(buf), conf->imgmvspd));
	iniparser_setstring(dict, "Image:imgpaging", dwordToString(buf, sizeof(buf), conf->imgpaging));
	for (i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Text:txtkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->txtkey[i]));
	}
	iniparser_setstring(dict, "UI:fontsize", intToString(buf, sizeof(buf), conf->fontsize));
	iniparser_setstring(dict, "Text:reordertxt", booleanToString(buf, sizeof(buf), conf->reordertxt));
	iniparser_setstring(dict, "Text:pagetonext", booleanToString(buf, sizeof(buf), conf->pagetonext));
	iniparser_setstring(dict, "Text:autopage", intToString(buf, sizeof(buf), conf->autopage));
	iniparser_setstring(dict, "Text:prev_autopage", intToString(buf, sizeof(buf), conf->prev_autopage));
	iniparser_setstring(dict, "Text:autopagetype", intToString(buf, sizeof(buf), conf->autopagetype));
	iniparser_setstring(dict, "Text:autolinedelay", intToString(buf, sizeof(buf), conf->autolinedelay));
	iniparser_setstring(dict, "Image:thumb", booleanToString(buf, sizeof(buf), conf->thumb));
	iniparser_setstring(dict, "Text:bookfontsize", intToString(buf, sizeof(buf), conf->bookfontsize));
	iniparser_setstring(dict, "Text:enable_analog", booleanToString(buf, sizeof(buf), conf->enable_analog));
	iniparser_setstring(dict, "Image:img_enable_analog", booleanToString(buf, sizeof(buf), conf->img_enable_analog));
	for (i=0; i<20; ++i) {
		char key[20];
		SPRINTF_S(key, "Text:txtkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->txtkey2[i]));
		SPRINTF_S(key, "Image:imgkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->imgkey2[i]));
		SPRINTF_S(key, "Global:flkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->flkey2[i]));
	}
	iniparser_setstring(dict, "Image:imgpagereserve", dwordToString(buf, sizeof(buf), conf->imgpagereserve));
	iniparser_setstring(dict, "Music:lyricex", dwordToString(buf, sizeof(buf), conf->lyricex));
	iniparser_setstring(dict, "Music:autoplay", booleanToString(buf, sizeof(buf), conf->autoplay));
	iniparser_setstring(dict, "Text:usettf", booleanToString(buf, sizeof(buf), conf->usettf));
	iniparser_setstring(dict, "Text:cttfarch", conf->cttfarch);
	iniparser_setstring(dict, "Text:cttfpath", conf->cttfpath);
	iniparser_setstring(dict, "Text:ettfarch", conf->ettfarch);
	iniparser_setstring(dict, "Text:ettfpath", conf->ettfpath);
	for (i=0; i<3; ++i) {
		char key[20];
		SPRINTF_S(key, "Global:freqs_%d", i);
		iniparser_setstring(dict, key, intToString(buf, sizeof(buf), conf->freqs[i]));
	}
	iniparser_setstring(dict, "Image:imgbrightness", intToString(buf, sizeof(buf), conf->imgbrightness));
	iniparser_setstring(dict, "Global:dis_scrsave", booleanToString(buf, sizeof(buf), conf->dis_scrsave));
	iniparser_setstring(dict, "Global:autosleep", intToString(buf, sizeof(buf), conf->autosleep));
	iniparser_setstring(dict, "Image:load_exif", booleanToString(buf, sizeof(buf), conf->load_exif));
	iniparser_setstring(dict, "Global:brightness", intToString(buf, sizeof(buf), conf->brightness));
	iniparser_setstring(dict, "Global:launchtype", intToString(buf, sizeof(buf), conf->launchtype));
	iniparser_setstring(dict, "Text:infobar_use_ttf_mode", booleanToString(buf, sizeof(buf), conf->infobar_use_ttf_mode));

	iniparser_setstring(dict, "Text:cfont_antialias", booleanToString(buf, sizeof(buf), conf->cfont_antialias));
	iniparser_setstring(dict, "Text:cfont_cleartype", booleanToString(buf, sizeof(buf), conf->cfont_cleartype));
	iniparser_setstring(dict, "Text:cfont_embolden", booleanToString(buf, sizeof(buf), conf->cfont_embolden));

	iniparser_setstring(dict, "Text:efont_antialias", booleanToString(buf, sizeof(buf), conf->efont_antialias));
	iniparser_setstring(dict, "Text:efont_cleartype", booleanToString(buf, sizeof(buf), conf->efont_cleartype));
	iniparser_setstring(dict, "Text:efont_embolden", booleanToString(buf, sizeof(buf), conf->efont_embolden));

	iniparser_setstring(dict, "Image:no_repeat", booleanToString(buf, sizeof(buf), conf->img_no_repeat));

	iniparser_dump_ini(dict, fp);

	fclose(fp);

	dictionary_del(dict);

	return true;
error:
	if (fp != NULL)
		fclose(fp);
	return false;
}

extern bool conf_load(p_conf conf)
{
	conf_default(conf);
	if (ini_conf_load(conf_filename, conf) == false)
		return false;

	if (conf->confver != XREADER_VERSION_NUM)
		conf_default(conf);
#ifndef ENABLE_USB
	conf->enableusb = false;
#endif
#ifndef ENABLE_BG
	STRCPY_S(conf->bgfile, "");
	STRCPY_S(conf->bgarch, "");
	conf->bgwhere = scene_in_dir;
#endif
#ifndef ENABLE_MUSIC
	conf->autoplay = false;
	conf->hprmctrl = true;
#else
#ifndef ENABLE_LYRIC
	conf->lyricex = 0;
#endif
#endif

	ini_conf_save(conf);

	return true;
}

extern bool conf_save(p_conf conf)
{
	extern bool prx_loaded;

	if (prx_loaded)
		conf->brightness = xrGetBrightness();

	ini_conf_save(conf);
	return true;
}

extern const char *conf_get_encodename(t_conf_encode encode)
{
	switch (encode) {
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

extern const char *conf_get_fitname(t_conf_fit fit)
{
	switch (fit) {
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

extern const char *conf_get_cyclename(t_conf_cycle cycle)
{
	switch (cycle) {
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

extern const char *conf_get_arrangename(t_conf_arrange arrange)
{
	switch (arrange) {
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

extern const char *conf_get_viewposname(t_conf_viewpos viewpos)
{
	switch (viewpos) {
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

extern const char *conf_get_imgpagingname(t_conf_imgpaging imgpaging)
{
	switch (imgpaging) {
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

extern const char *conf_get_thumbname(t_conf_thumb thumb)
{
	switch (thumb) {
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

extern const char *conf_get_infobarname(t_conf_infobar infobar)
{
	switch (infobar) {
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
