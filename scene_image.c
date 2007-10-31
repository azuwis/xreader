#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include "display.h"
#include "win.h"
#include "ctrl.h"
#include "fs.h"
#include "image.h"
#ifdef ENABLE_USB
#include "usb.h"
#endif
#include "power.h"
#include "bookmark.h"
#include "conf.h"
#include "charsets.h"
#include "fat.h"
#include "location.h"
#ifdef ENABLE_MUSIC
#include "mp3.h"
#ifdef ENABLE_LYRIC
#include "lyric.h"
#endif
#endif
#include "text.h"
#include "bg.h"
#include "copy.h"
#ifdef ENABLE_PMPAVC
#include "avc.h"
#endif
#include "version.h"
#include "common/log.h"
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"

#ifdef ENABLE_IMAGE

dword width, height, w2 = 0, h2 = 0, thumbw = 0, thumbh = 0, paintleft = 0, painttop = 0;
pixel * imgdata = NULL, * imgshow = NULL;
pixel bgcolor = 0, thumbimg[128 * 128];
dword oldangle = 0;
char filename[256];
int curtop = 0, curleft = 0, xpos = 0, ypos = 0;
static bool needrf = true, needrc = true, needrp = true, showinfo = false, thumb = false;
int imgh;
bool slideshow = false;
time_t now = 0, lasttime = 0;

int scene_reloadimage(dword selidx)
{
	scene_power_save(false);
	if(imgshow != NULL && imgshow != imgdata)
	{
		free((void *)imgshow);
		imgshow = NULL;
	}
	if(imgdata != NULL)
	{
		free((void *)imgdata);
		imgdata = NULL;
	}
	if(where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
		strcpy(filename, filelist[selidx].compname);
	else
	{
		strcpy(filename, config.shortpath);
		strcat(filename, filelist[selidx].shortname);
	}
	int result;
	switch((t_fs_filetype)filelist[selidx].data)
	{
		case fs_filetype_png:
			switch(where)
			{
				case scene_in_zip:
					result = image_readpng_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readpng_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readpng_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readpng(filename, &width, &height, &imgdata, &bgcolor);
					break;
			}
			break;
		case fs_filetype_gif:
			switch(where)
			{
				case scene_in_zip:
					result = image_readgif_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readgif_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readgif_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readgif(filename, &width, &height, &imgdata, &bgcolor);
					break;
			}
			break;
		case fs_filetype_jpg:
			switch(where)
			{
				case scene_in_zip:
					result = image_readjpg_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readjpg_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readjpg_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readjpg(filename, &width, &height, &imgdata, &bgcolor);
					break;
			}
			break;
		case fs_filetype_bmp:
			switch(where)
			{
				case scene_in_zip:
					result = image_readbmp_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readbmp_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readbmp_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readbmp(filename, &width, &height, &imgdata, &bgcolor);
					break;
			}
			break;
		case fs_filetype_tga:
			switch(where)
			{
				case scene_in_zip:
					result = image_readtga_in_zip(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_chm:
					result = image_readtga_in_chm(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				case scene_in_rar:
					result = image_readtga_in_rar(config.shortpath, filename, &width, &height, &imgdata, &bgcolor);
					break;
				default:
					result = image_readtga(filename, &width, &height, &imgdata, &bgcolor);
					break;
			}
			break;
		default:
			result = -1;
	}
	if(result != 0)
	{
		char infomsg[80];
		const char *errstr;
		switch(result) {
			case 1:
			case 2:
			case 3:
				errstr = "��ʽ����";
				break;
			case 4:
			case 5:
				errstr = "�ڴ治��";
				break;
			default:
				errstr = "����";
				break;
		}
		sprintf(infomsg, "ͼ���޷�װ��, ԭ��: %s", errstr);
		win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		imgreading = false;
		scene_power_save(true);
		// imgshow double freed bug fix
		if(imgshow != NULL && imgdata != NULL && imgshow != imgdata)
		{
			free((void *)imgshow);
			imgshow = NULL;
		}
		if(imgdata != NULL)
		{
			free((void *)imgdata);
			imgdata = NULL;
		}
		disp_duptocachealpha(50);
		return -1;
	}
	strcpy(config.lastfile, filelist[selidx].compname);
	oldangle = 0;
	scene_power_save(true);	
	return 0;
}

dword scene_rotateimage()
{
	image_rotate(imgdata, &width, &height, oldangle, (dword)config.rotate * 90);
	oldangle = (dword)config.rotate * 90;
	if(config.fit > 0 && (config.fit != conf_fit_custom || config.scale != 100))
	{
		if(imgshow != NULL && imgshow != imgdata)
			free((void *)imgshow);
		imgshow = NULL;
		if(config.fit == conf_fit_custom)
		{
			w2 = width * config.scale / 100;
			h2 = height * config.scale / 100;
		}
		else if(config.fit == conf_fit_width)
		{
			config.scale = PSP_SCREEN_WIDTH / width;
			if(config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else
			{
				config.scale = (config.scale / 10) * 10;
				if(config.scale < 10)
					config.scale = 10;
			}
			w2 = PSP_SCREEN_WIDTH;
			h2 = height * PSP_SCREEN_WIDTH / width;
		}
		else if(config.fit == conf_fit_dblwidth)
		{
			config.scale = 960 / width;
			if(config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else
			{
				config.scale = (config.scale / 10) * 10;
				if(config.scale < 10)
					config.scale = 10;
			}
			w2 = 960;
			h2 = height * 960 / width;
		}
		else if(config.fit == conf_fit_dblheight)
		{
			config.scale = imgh / height;
			if(config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else
			{
				config.scale = (config.scale / 10) * 10;
				if(config.scale < 10)
					config.scale = 10;
			}
			h2 = imgh * 2;
			w2 = width * imgh * 2 / height;
		}
		else
		{
			config.scale = imgh / height;
			if(config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else
			{
				config.scale = (config.scale / 10) * 10;
				if(config.scale < 10)
					config.scale = 10;
			}
			h2 = imgh;
			w2 = width * imgh / height;
		}
		imgshow = (pixel *)malloc(sizeof(pixel) * w2 * h2);
		if(imgshow != NULL)
		{
			if(config.bicubic)
				image_zoom_bicubic(imgdata, width, height, imgshow, w2, h2);
			else
				image_zoom_bilinear(imgdata, width, height, imgshow, w2, h2);
		}
		else
		{
			imgshow = imgdata;
			w2 = width;
			h2 = height;
		}
	}
	else
	{
		config.scale = 100;
		imgshow = imgdata;
		w2 = width;
		h2 = height;
	}
	curleft = curtop = 0;
	xpos = (int)config.viewpos % 3;
	ypos = (int)config.viewpos / 3;
	if(w2 < PSP_SCREEN_WIDTH)
		paintleft = (PSP_SCREEN_WIDTH - w2) / 2;
	else
	{
		paintleft = 0;
		switch(xpos)
		{
			case 1:
				curleft = (w2 - PSP_SCREEN_WIDTH) / 2;
				break;
			case 2:
				curleft = w2 - PSP_SCREEN_WIDTH;
				break;
		}
	}
	if(h2 < imgh)
		painttop = (imgh - h2) / 2;
	else
	{
		painttop = 0;
		switch(ypos)
		{
			case 1:
				curtop = (h2 - imgh) / 2;
				break;
			case 2:
				curtop = h2 - imgh;
				break;
		}
	}

	if(width > height)
	{
		thumbw = 128;
		thumbh = height * 128 / width;
	}
	else
	{
		thumbh = 128;
		thumbw = width * 128 / height;
	}
	image_zoom_bilinear(imgdata, width, height, thumbimg, thumbw, thumbh);

	if(slideshow)
		lasttime = time(NULL);
	return 0;
}

int scene_printimage()
{
	disp_waitv();
	disp_fillvram(bgcolor);
	disp_putimage(paintleft, painttop, w2, h2, curleft, curtop, imgshow);
	if(config.imginfobar || showinfo)
	{
		char infostr[64];
		if(config.fit == conf_fit_custom)
			sprintf(infostr, "%dx%d  %d%%  ��ת�Ƕ� %d  %s", (int)w2, (int)h2, (int)config.scale, (int)oldangle, config.bicubic ? "��������" : "��������");
		else
			sprintf(infostr, "%dx%d  %s  ��ת�Ƕ� %d  %s", (int)w2, (int)h2, conf_get_fitname(config.fit), (int)oldangle, config.bicubic ? "��������" : "��������");
		int ilen = strlen(infostr);
		if(config.imginfobar)
		{
			disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
			disp_putnstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)filename, 960 / DISP_FONTSIZE - ilen - 1, 0, 0, DISP_FONTSIZE, 0);
			disp_putnstring(PSP_SCREEN_WIDTH - DISP_FONTSIZE / 2 * ilen, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const byte *)infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
		}
		else
		{
			disp_fillrect(11, 11, 10 + DISP_FONTSIZE / 2 * ilen, 10 + DISP_FONTSIZE, 0);
			disp_putnstring(11, 11, COLOR_WHITE, (const byte *)infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
		}
	}
	if(config.thumb == conf_thumb_always || thumb)
	{
		dword top = (PSP_SCREEN_HEIGHT - thumbh) / 2, bottom = top + thumbh;
		dword thumbl = 0, thumbr = 0, thumbt = 0, thumbb = 0;
		if(paintleft > 0)
		{
			thumbl = 0;
			thumbr = thumbw - 1;
		}
		else
		{
			thumbl = curleft * thumbw / w2;
			thumbr = (curleft + 479) * thumbw / w2;
		}
		if(painttop > 0)
		{
			thumbt = 0;
			thumbb = thumbb - 1;
		}
		else
		{
			thumbt = curtop * thumbh / h2;
			thumbb = (curtop + imgh - 1) * thumbh / h2;
		}
		disp_putimage(32, top, thumbw, thumbh, 0, 0, thumbimg);
		disp_line(34, bottom, 32 + thumbw, bottom, 0);
		disp_line(32 + thumbw, top + 2, 32 + thumbw, bottom - 1, 0);
		disp_rectangle(33 + thumbl, top + thumbt + 1, 33 + thumbr, top + thumbb + 1, 0);
		disp_rectangle(32 + thumbl, top + thumbt, 32 + thumbr, top + thumbb, COLOR_WHITE);
	}
	disp_flip();
	return 0;
}

int image_handle_input(dword *selidx, dword key)
{
	if(key == 0)
		goto next;
	if(key == (PSP_CTRL_SELECT | PSP_CTRL_START))
	{
		if(win_msgbox("�Ƿ��˳�����?", "��", "��", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
	}
	else if(key == PSP_CTRL_SELECT)
	{
		needrp = true;
		bool lastbicubic = config.bicubic;
		if(scene_options(selidx))
		{
			imgreading = false;
			scene_power_save(true);
			if(imgshow != NULL && imgshow != imgdata)
			{
				free((void *)imgshow);
				imgshow = NULL;
			}
			if(imgdata != NULL)
			{
				free((void *)imgdata);
				imgdata = NULL;
			}
			disp_duptocachealpha(50);
			return *selidx;
		}
		if(lastbicubic != config.bicubic)
			needrc = true;
		if(config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;
	}
#ifdef ENABLE_MUSIC
	else if(key == PSP_CTRL_START)
	{
		scene_mp3bar();
		needrp = true;
	}
#endif
	else if(key == config.imgkey[1] || key == config.imgkey2[1] || key == CTRL_FORWARD)
	{
		switch(config.imgpaging)
		{
			case conf_imgpaging_updown:
				switch(ypos)
				{
					case 0:
					case 1:
						if(curtop + imgh < h2)
						{
							curtop += imgh - config.imgpagereserve;
							if(curtop + imgh > h2)
								curtop = h2 - imgh;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curtop > 0)
						{
							curtop -= imgh - config.imgpagereserve;
							if(curtop < 0)
								curtop = 0;
							needrp = true;
							goto next;
						}
						break;
				}
				curtop = 0;
				if(h2 > imgh && ypos == 2)
					curtop = h2 - imgh;
				switch(xpos)
				{
					case 0:
					case 1:
						if(curleft + PSP_SCREEN_WIDTH < w2)
						{
							curleft += PSP_SCREEN_WIDTH;
							if(curleft +PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curleft > 0)
						{
							curleft -= PSP_SCREEN_WIDTH;
							if(curleft < 0)
								curleft = 0;
							needrp = true;
							goto next;
						}
						break;
				}
				break;
			case conf_imgpaging_leftright:
				switch(xpos)
				{
					case 0:
					case 1:
						if(curleft + PSP_SCREEN_WIDTH < w2)
						{
							curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
							if(curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curleft > 0)
						{
							curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
							if(curleft < 0)
								curleft = 0;
							needrp = true;
							goto next;
						}
						break;
				}
				curleft = 0;
				if(w2 > PSP_SCREEN_WIDTH && xpos == 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch(ypos)
				{
					case 0:
					case 1:
						if(curtop + imgh < h2)
						{
							curtop += imgh;
							if(curtop + imgh > h2)
								curtop = h2 - imgh;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curtop > 0)
						{
							curtop -= imgh;
							if(curtop < 0)
								curtop = 0;
							needrp = true;
							goto next;
						}
						break;
				}
				break;
			default:;
		}
		dword orgidx = *selidx;
		do {
			if(*selidx < filecount - 1)
				(*selidx) ++;
			else
				*selidx = 0;
		} while(!fs_is_image((t_fs_filetype)filelist[*selidx].data));
		if(*selidx != orgidx)
			needrf = needrc = needrp = true;
	}
	else if(key == config.imgkey[0] || key == config.imgkey2[0] || key == CTRL_BACK)
	{
		switch(config.imgpaging)
		{
			case conf_imgpaging_updown:
				switch(ypos)
				{
					case 0:
					case 1:
						if(curtop > 0)
						{
							curtop -= imgh - config.imgpagereserve;
							if(curtop < 0)
								curtop = 0;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curtop + imgh < h2)
						{
							curtop += imgh - config.imgpagereserve;
							if(curtop + imgh > h2)
								curtop = h2 - imgh;
							needrp = true;
							goto next;
						}
						break;
				}
				curtop = 0;
				if(h2 > imgh && ypos < 2)
					curtop = h2 - imgh;
				switch(xpos)
				{
					case 0:
					case 1:
						if(curleft > 0)
						{
							curleft -= PSP_SCREEN_WIDTH;
							if(curleft < 0)
								curleft = 0;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curleft + PSP_SCREEN_WIDTH < w2)
						{
							curleft += PSP_SCREEN_WIDTH;
							if(curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							needrp = true;
							goto next;
						}
						break;
				}
				break;
			case conf_imgpaging_leftright:
				switch(xpos)
				{
					case 0:
					case 1:
						if(curleft > 0)
						{
							curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
							if(curleft < 0)
								curleft = 0;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curleft + PSP_SCREEN_WIDTH < w2)
						{
							curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
							if(curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							needrp = true;
							goto next;
						}
						break;
				}
				curleft = 0;
				if(w2 > PSP_SCREEN_WIDTH && xpos < 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch(ypos)
				{
					case 0:
					case 1:
						if(curtop > 0)
						{
							curtop -= imgh;
							if(curtop < 0)
								curtop = 0;
							needrp = true;
							goto next;
						}
						break;
					case 2:
						if(curtop + imgh < h2)
						{
							curtop += imgh;
							if(curtop + imgh > h2)
								curtop = h2 - imgh;
							needrp = true;
							goto next;
						}
						break;
				}
				break;
			default:;
		}
		dword orgidx = *selidx;
		do {
			if(*selidx > 0)
				(*selidx) --;
			else
				*selidx = filecount - 1;
		} while(!fs_is_image((t_fs_filetype)filelist[*selidx].data));
		if(*selidx != orgidx)
			needrf = needrc = needrp = true;
	}
#ifdef ENABLE_ANALOG
	else if(key == CTRL_ANALOG)
	{
		int x, y, orgtop = curtop, orgleft = curleft;
		ctrl_analog(&x, &y);
		x = x / 31 * (int)config.imgmvspd / 2;
		y = y / 31 * (int)config.imgmvspd / 2;
		curtop += y;
		if(curtop + imgh > h2)
			curtop = (int)h2 - imgh;
		if(curtop < 0)
			curtop = 0;
		curleft += x;
		if(curleft + PSP_SCREEN_WIDTH > w2)
			curleft = (int)w2 - PSP_SCREEN_WIDTH;
		if(curleft < 0)
			curleft = 0;
		thumb = (config.thumb == conf_thumb_scroll);
		needrp = (thumb || orgtop != curtop || orgleft != curleft);
	}
#endif
	else if(key == PSP_CTRL_LEFT)
	{
		if(curleft > 0)
		{
			curleft -= (int)config.imgmvspd;
			if(curleft < 0)
				curleft = 0;
			needrp = true;
		}
		thumb = (config.thumb == conf_thumb_scroll);
		needrp = thumb | needrp;
	}
	else if(key == PSP_CTRL_UP)
	{
		if(curtop > 0)
		{
			curtop -= (int)config.imgmvspd;
			if(curtop < 0)
				curtop = 0;
			needrp = true;
		}
		thumb = (config.thumb == conf_thumb_scroll);
		needrp = thumb | needrp;
	}
	else if(key == PSP_CTRL_RIGHT)
	{
		if(w2 > PSP_SCREEN_WIDTH && curleft < w2 - PSP_SCREEN_WIDTH)
		{
			curleft += (int)config.imgmvspd;
			if(curleft > w2 - PSP_SCREEN_WIDTH)
				curleft = w2 - PSP_SCREEN_WIDTH;
			needrp = true;
		}
		thumb = (config.thumb == conf_thumb_scroll);
		needrp = thumb | needrp;
	}
	else if(key == PSP_CTRL_DOWN)
	{
		if(h2 > imgh && curtop < h2 - imgh)
		{
			curtop += (int)config.imgmvspd;
			if(curtop > h2 - imgh)
				curtop = h2 - imgh;
			needrp = true;
		}
		thumb = (config.thumb == conf_thumb_scroll);
		needrp = thumb | needrp;
	}
	else if(key == config.imgkey[2] || key == config.imgkey2[2])
	{
		if(config.fit == conf_fit_custom)
			config.fit = conf_fit_none;
		else
			config.fit ++;
		needrc = needrp = true;
	}
	else if(key == config.imgkey[10] || key == config.imgkey2[10])
	{
		config.bicubic = !config.bicubic;
		needrc = needrp = true;
	}
	else if(key == config.imgkey[11] || key == config.imgkey2[11])
	{
		if(!slideshow)
		{
			slideshow = true;
			lasttime = time(NULL);
			ctrl_waitrelease();
		}
		else
		{
			slideshow = false;
			win_msg("�õ�Ƭ�����Ѿ�ֹͣ��", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		}
	}
	else if(key == config.imgkey[7] || key == config.imgkey2[7])
	{
		config.imginfobar = !config.imginfobar;
		if(config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;
		if(h2 > imgh && curtop > h2 - imgh)
			curtop = h2 - imgh;
		needrc = (config.fit == conf_fit_height);
		needrp = true;
	}
	else if(key == config.imgkey[9] || key == config.imgkey2[9] || key == CTRL_PLAYPAUSE)
	{
		if(slideshow)
		{
			slideshow = false;
			win_msg("�õ�Ƭ�����Ѿ�ֹͣ��", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50));
		}
		else
		{
			imgreading = false;
			scene_power_save(true);
			if(imgshow != NULL && imgshow != imgdata)
			{
				free((void *)imgshow);
				imgshow = NULL;
			}
			if(imgdata != NULL)
			{
				free((void *)imgdata);
				imgdata = NULL;
			}
			disp_duptocachealpha(50);
			return *selidx;
		}
	}
	else if(key == config.imgkey[8] || key == config.imgkey2[8])
	{
		if(!showinfo)
		{
			needrp = true;
			showinfo = true;
		}
	}
	else if(key == config.imgkey[5] || key == config.imgkey2[5])
	{
		if(config.rotate == conf_rotate_0)
			config.rotate = conf_rotate_270;
		else
			config.rotate --;
		needrc = needrp = true;
	}
	else if(key == config.imgkey[6] || key == config.imgkey2[6])
	{
		if(config.rotate == conf_rotate_270)
			config.rotate = conf_rotate_0;
		else
			config.rotate ++;
		needrc = needrp = true;
	}
	else if(key == config.imgkey[3] || key == config.imgkey2[3])
	{
		if(config.scale > 200)
			config.scale -= 50;
		else if(config.scale > 10)
			config.scale -= 10;
		else
			goto next;
		config.fit = conf_fit_custom;
		needrc = needrp = true;
	}
	else if(key == config.imgkey[4] || key == config.imgkey2[4])
	{
		if(config.scale < 200)
			config.scale += 10;
		else if(config.scale < 1000)
			config.scale += 50;
		else
			goto next;
		config.fit = conf_fit_custom;
		needrc = needrp = true;
	}
	else
		needrf = needrc = false;
next:
	return -1;
}

dword scene_readimage(dword selidx)
{
	w2 = 0, h2 = 0, thumbw = 0, thumbh = 0, paintleft = 0, painttop = 0;
	imgdata = NULL, imgshow = NULL;
	oldangle = 0;
	curtop = 0, curleft = 0, xpos = 0, ypos = 0;
	needrf = true, needrc = true, needrp = true, showinfo = false, thumb = false;
	slideshow = false;
	now = 0, lasttime = 0;
	imgreading = true;
	if(config.imginfobar)
		imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
	else
		imgh = PSP_SCREEN_HEIGHT;
	while(1) {
		if(needrf)
		{
			dword ret = scene_reloadimage(selidx);
			if(ret == -1)
				return selidx;
			needrf = false;
		}
		if(needrc)
		{
			scene_rotateimage();
			needrc = false;
		}
		if(needrp)
		{
			scene_printimage();
			needrp = false;
		}
		now = time(NULL);
		dword key = 0;
		if(config.thumb == conf_thumb_scroll && thumb)
		{
			thumb = false;
			needrp = true;
			key = ctrl_read_cont();
		}
		else if(!slideshow || now - lasttime < config.slideinterval)
			key = (showinfo ? ctrl_read_cont() : (slideshow ? ctrl_waittime(config.slideinterval - (now - lasttime)) : ctrl_waitany()));
		if(slideshow)
		{
			scePowerTick(0);
			if(now - lasttime >= config.slideinterval)
			{
				key = CTRL_FORWARD;
				lasttime = now;
			}
		}
		if(showinfo && (key & PSP_CTRL_CIRCLE) == 0)
		{
			needrp = true;
			showinfo = false;
		}
		int ret = image_handle_input(&selidx, key);
		if(ret != -1)
			return ret;
	}
	imgreading = false;
	scene_power_save(false);
	if(imgshow != NULL && imgshow != imgdata)
	{
		free((void *)imgshow);
		imgshow = NULL;
	}
	if(imgdata != NULL)
	{
		free((void *)imgdata);
		imgdata = NULL;
	}
	disp_duptocachealpha(50);
	return selidx;
}

t_win_menu_op scene_imgkey_menucb(dword key, p_win_menuitem item, dword * count, dword max_height, dword * topindex, dword * index)
{
	switch(key)
	{
	case (PSP_CTRL_SELECT | PSP_CTRL_START):
		if(win_msgbox("�Ƿ��˳�����?", "��", "��", COLOR_WHITE, COLOR_WHITE, RGB(0x18, 0x28, 0x50)))
		{
			scene_exit();
			return win_menu_op_continue;
		}
		return win_menu_op_force_redraw;
	case PSP_CTRL_CIRCLE:
		disp_duptocache();
		disp_waitv();
		disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2, 240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2, COLOR_WHITE);
		disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, 239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2, RGB(0x8, 0x18, 0x10));
		disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2, COLOR_WHITE, (const byte *)"�밴��Ӧ����");
		disp_flip();
		dword key, key2;
		SceCtrlData ctl;
		ctrl_waitrelease();
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
			key = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
		} while((key & ~(PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT))== 0);
		key2 = key;
		while((key2 & key) == key)
		{
			key = key2;
			sceCtrlReadBufferPositive(&ctl, 1);
			key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
		}
		if(config.imgkey[*index] == key || config.imgkey2[*index] == key)
			return win_menu_op_force_redraw;
		int i;
		for(i = 0; i < 12; i ++)
		{
			if(i == *index)
				continue;
			if(config.imgkey[i] == key)
			{
				config.imgkey[i] = config.imgkey2[*index];
				if(config.imgkey[i] == 0)
				{
					config.imgkey[i] = config.imgkey2[i];
					config.imgkey2[i] = 0;
				}
				break;
			}
			if(config.imgkey2[i] == key)
			{
				config.imgkey2[i] = config.imgkey2[*index];
				break;
			}
		}
		config.imgkey2[*index] = config.imgkey[*index];
		config.imgkey[*index] = key;
		do {
			sceCtrlReadBufferPositive(&ctl, 1);
		} while(ctl.Buttons != 0);
		return win_menu_op_force_redraw;
	case PSP_CTRL_TRIANGLE:
		config.imgkey[*index] = config.imgkey2[*index];
		config.imgkey2[*index] = 0;
		return win_menu_op_redraw;
	case PSP_CTRL_SQUARE:
		return win_menu_op_cancel;
	default:;
	}
	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

void scene_imgkey_predraw(p_win_menuitem item, dword index, dword topindex, dword max_height)
{
	char keyname[256];
	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE, 240 + DISP_FONTSIZE * 10, 144 + 6 * DISP_FONTSIZE, COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 143 + 6 * DISP_FONTSIZE, RGB(0x10, 0x30, 0x20));
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE, COLOR_WHITE, (const byte *)"��������   �� ɾ��");
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, 239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;
	for (i = topindex; i < topindex + max_height; i ++)
	{
		conf_get_keyname(config.imgkey[i], keyname);
		if(config.imgkey2[i] != 0)
		{
			char keyname2[256];
			conf_get_keyname(config.imgkey2[i], keyname2);
			strcat(keyname, " | ");
			strcat(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5, 131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i, COLOR_WHITE, (const byte *)keyname);
	}
}

dword scene_imgkey(dword * selidx)
{
	t_win_menuitem item[12];
	strcpy(item[0].name, "��һ��ͼ");
	strcpy(item[1].name, "��һ��ͼ");
	strcpy(item[2].name, "����ģʽ");
	strcpy(item[3].name, "��СͼƬ");
	strcpy(item[4].name, "�Ŵ�ͼƬ");
	strcpy(item[5].name, "����90��");
	strcpy(item[6].name, "����90��");
	strcpy(item[7].name, "  ��Ϣ��");
	strcpy(item[8].name, "��ʾ��Ϣ");
	strcpy(item[9].name, "�˳����");
	strcpy(item[10].name, "��������");
	strcpy(item[11].name, "�õƲ���");
	dword i;
	for(i = 0; i < 12; i ++)
	{
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = RGB(0xDF, 0xDF, 0xDF);
		item[i].selicolor = RGB(0xFF, 0xFF, 0x40);
		item[i].selrcolor = RGB(0x10, 0x30, 0x20);
		item[i].selbcolor = RGB(0x20, 0x20, 0xDF);
		item[i].data = NULL;
	}
	dword index;
	while((index = win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, 12, item, 12, 0, 0, RGB(0x10, 0x30, 0x20), true, scene_imgkey_predraw, NULL, scene_imgkey_menucb)) != INVALID);
	return 0;
}
#endif