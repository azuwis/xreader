/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <pspkernel.h>
#include <psppower.h>
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
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "pspscreen.h"
#include "dbg.h"

#ifdef ENABLE_IMAGE

dword width, height, w2 = 0, h2 = 0, thumbw = 0, thumbh = 0, paintleft =
	0, painttop = 0;
pixel *imgdata = NULL, *imgshow = NULL;
pixel bgcolor = 0, thumbimg[128 * 128];
dword oldangle = 0;
char filename[PATH_MAX];
int curtop = 0, curleft = 0, xpos = 0, ypos = 0;
bool img_needrf = true, img_needrc = true, img_needrp = true;
static bool showinfo = false, thumb = false;
int imgh;
bool slideshow = false;
time_t now = 0, lasttime = 0;
extern void *framebuffer;

static volatile int secticks = 0;

int open_image(dword selidx)
{
	if (exif_array) {
		buffer_array_reset(exif_array);
	}
	// if imgshow share memory with imgdata, and loads image fail (imgdata = 0), we must reset imgshow.
	bool shareimg = (imgshow == imgdata) ? true : false;
	int result;

	result =
		image_open_archive(filename, config.shortpath,
						   (t_fs_filetype) filelist[selidx].data, &width,
						   &height, &imgdata, &bgcolor, where);
	if (imgdata == NULL && shareimg) {
		imgshow = NULL;
	}
	return result;
}

void reset_image_ptr(void)
{
	if (imgshow != NULL && imgshow != imgdata) {
		free((void *) imgshow);
		imgshow = NULL;
	}
	if (imgdata != NULL) {
		if (imgshow == imgdata)
			imgshow = NULL;
		free((void *) imgdata);
		imgdata = NULL;
	}
}

void report_image_error(int status)
{
	char infomsg[80];
	const char *errstr;

	switch (status) {
		case 1:
		case 2:
		case 3:
			errstr = "格式错误";
			break;
		case 4:
		case 5:
			errstr = "内存不足";
			break;
		default:
			errstr = "不明";
			break;
	}
	SPRINTF_S(infomsg, "图像无法装载, 原因: %s", errstr);
	win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
	dbg_printf(d,
			   "图像无法装载，原因: %s where = %d config.path %s filename %s",
			   errstr, where, config.path, filename);
	imgreading = false;
	reset_image_ptr();
	scene_power_save(true);
}

// TODO: use GU to improve speed...
void recalc_brightness()
{
	int i;

	if (imgdata) {
		pixel *t = imgdata;
		short b = 100 - config.imgbrightness;

		for (i = 0; i < height * width; i++) {
			*t = disp_grayscale(*t, 0, 0, 0, b);
			t++;
		}
	}
}

int scene_reloadimage(dword selidx)
{
	scene_power_save(false);
	reset_image_ptr();
	if (where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
		STRCPY_S(filename, filelist[selidx].compname);
	else {
		STRCPY_S(filename, config.shortpath);
		STRCAT_S(filename, filelist[selidx].shortname);
	}
	int result = open_image(selidx);

	if (result != 0) {
		report_image_error(result);
		return -1;
	}
	if (config.imgbrightness != 100) {
		recalc_brightness();
	}
	STRCPY_S(config.lastfile, filelist[selidx].compname);
	oldangle = 0;
	scene_power_save(true);
	return 0;
}

dword scene_rotateimage()
{
	scene_power_save(false);
	image_rotate(imgdata, &width, &height, oldangle,
				 (dword) config.rotate * 90);
	oldangle = (dword) config.rotate * 90;
	if (config.fit > 0
		&& (config.fit != conf_fit_custom || config.scale != 100)) {
		if (imgshow != NULL && imgshow != imgdata)
			free((void *) imgshow);
		imgshow = NULL;
		if (config.fit == conf_fit_custom) {
			w2 = width * config.scale / 100;
			h2 = height * config.scale / 100;
		} else if (config.fit == conf_fit_width) {
			config.scale = PSP_SCREEN_WIDTH / width;
			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}
			w2 = PSP_SCREEN_WIDTH;
			h2 = height * PSP_SCREEN_WIDTH / width;
		} else if (config.fit == conf_fit_dblwidth) {
			config.scale = 960 / width;
			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}
			w2 = 960;
			h2 = height * 960 / width;
		} else if (config.fit == conf_fit_dblheight) {
			config.scale = imgh / height;
			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}
			h2 = imgh * 2;
			w2 = width * imgh * 2 / height;
		} else {
			config.scale = imgh / height;
			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}
			h2 = imgh;
			w2 = width * imgh / height;
		}
		// sceGuCopyImage need the length to be block aligned...
		imgshow = (pixel *) memalign(16, sizeof(pixel) * w2 * h2);
		if (imgshow != NULL) {
			if (config.bicubic)
				image_zoom_bicubic(imgdata, width, height, imgshow, w2, h2);
			else
				image_zoom_bilinear(imgdata, width, height, imgshow, w2, h2);
		} else {
			imgshow = imgdata;
			w2 = width;
			h2 = height;
		}
	} else {
		config.scale = 100;
		imgshow = imgdata;
		w2 = width;
		h2 = height;
	}
	curleft = curtop = 0;
	xpos = (int) config.viewpos % 3;
	ypos = (int) config.viewpos / 3;
	if (w2 < PSP_SCREEN_WIDTH)
		paintleft = (PSP_SCREEN_WIDTH - w2) / 2;
	else {
		paintleft = 0;
		switch (xpos) {
			case 1:
				curleft = (w2 - PSP_SCREEN_WIDTH) / 2;
				break;
			case 2:
				curleft = w2 - PSP_SCREEN_WIDTH;
				break;
		}
	}
	if (h2 < imgh)
		painttop = (imgh - h2) / 2;
	else {
		painttop = 0;
		switch (ypos) {
			case 1:
				curtop = (h2 - imgh) / 2;
				break;
			case 2:
				curtop = h2 - imgh;
				break;
		}
	}

	if (width > height) {
		thumbw = 128;
		thumbh = height * 128 / width;
	} else {
		thumbh = 128;
		thumbw = width * 128 / height;
	}
	image_zoom_bilinear(imgdata, width, height, thumbimg, thumbw, thumbh);

	if (slideshow)
		lasttime = time(NULL);
	scene_power_save(true);
	return 0;
}

float get_text_len(const unsigned char *str)
{
	if (!str)
		return 0.0;

	float f = 0.0;

	while (*str != '\0') {
		if (*str >= 0x80 && *(str + 1) != '\0') {
			f++;
			str += 2;
		} else {
			f += 0.5;
			str++;
		}
	}

	return f;
}

float get_max_height(void)
{
	if (exif_array == NULL)
		return 0;
	int i;

	float max_h = -1.0;

	int height = PSP_SCREEN_HEIGHT / DISP_FONTSIZE - 1;
	int line_num = exif_array->used <= height ? exif_array->used : height;

	for (i = 0; i < line_num; ++i) {
		float len =
			get_text_len((const unsigned char *) exif_array->ptr[i]->ptr);
		max_h = max_h > len ? max_h : len;
	}

	return max_h;
}

int scene_printimage(int selidx)
{
	disp_waitv();
	disp_fillvram(bgcolor);
	disp_putimage(paintleft, painttop, w2, h2, curleft, curtop, imgshow);
	if (config.imginfobar || showinfo) {
		char infostr[64];

		if (config.fit == conf_fit_custom)
			SPRINTF_S(infostr, "%dx%d  %d%%  旋转角度 %d  %s", (int) w2,
					  (int) h2, (int) config.scale, (int) oldangle,
					  config.bicubic ? "三次立方" : "两次线性");
		else
			SPRINTF_S(infostr, "%dx%d  %s  旋转角度 %d  %s", (int) w2, (int) h2,
					  conf_get_fitname(config.fit), (int) oldangle,
					  config.bicubic ? "三次立方" : "两次线性");
		int ilen = strlen(infostr);

		if (config.imginfobar) {
			if (config.load_exif && exif_array && exif_array->used > 0) {
				int width = get_max_height() * DISP_FONTSIZE + 10;

				width =
					width >
					PSP_SCREEN_WIDTH - 10 ? PSP_SCREEN_WIDTH - 10 : width;
				int height = PSP_SCREEN_HEIGHT / DISP_FONTSIZE - 1;
				int line_num =
					exif_array->used <= height ? exif_array->used : height;
				int top =
					(PSP_SCREEN_HEIGHT - (1 + height) * DISP_FONTSIZE) / 2 >
					1 ? (PSP_SCREEN_HEIGHT -
						 (1 + height) * DISP_FONTSIZE) / 2 : 1;
				int left =
					(PSP_SCREEN_WIDTH - width) / 4 - 10 <
					1 ? 1 : (PSP_SCREEN_WIDTH - width) / 4 - 10;
				int right =
					(PSP_SCREEN_WIDTH + 3 * width) / 4 >=
					PSP_SCREEN_WIDTH - 1 ? PSP_SCREEN_WIDTH -
					2 : (PSP_SCREEN_WIDTH + 3 * width) / 4;
				disp_fillrect(left, top, right, top + DISP_FONTSIZE * line_num,
							  0);
				disp_rectangle(left - 1, top - 1, right + 1,
							   top + DISP_FONTSIZE * line_num + 1, COLOR_WHITE);
				int i;

				for (i = 0; i < line_num; ++i) {
					const char *teststr = exif_array->ptr[i]->ptr;

					disp_putnstring((PSP_SCREEN_WIDTH - width) / 4,
									top + i * DISP_FONTSIZE, COLOR_WHITE,
									(const byte *) teststr, strlen(teststr), 0,
									0, DISP_FONTSIZE, 0);
				}
			}

			disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
			disp_putnstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE,
							(const byte *) filelist[selidx].name,
							960 / DISP_FONTSIZE - ilen - 1, 0, 0, DISP_FONTSIZE,
							0);
			disp_putnstring(PSP_SCREEN_WIDTH - DISP_FONTSIZE / 2 * ilen,
							PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE,
							(const byte *) infostr, ilen, 0, 0, DISP_FONTSIZE,
							0);
		} else {
			disp_fillrect(11, 11, 10 + DISP_FONTSIZE / 2 * ilen,
						  10 + DISP_FONTSIZE, 0);
			disp_putnstring(11, 11, COLOR_WHITE, (const byte *) infostr, ilen,
							0, 0, DISP_FONTSIZE, 0);
		}
	}
	if ((config.thumb == conf_thumb_always || thumb)) {
		if (!config.imginfobar
			|| !(config.load_exif && exif_array && exif_array->used > 0)) {
			dword top = (PSP_SCREEN_HEIGHT - thumbh) / 2, bottom = top + thumbh;
			dword thumbl = 0, thumbr = 0, thumbt = 0, thumbb = 0;

			if (paintleft > 0) {
				thumbl = 0;
				thumbr = thumbw - 1;
			} else {
				thumbl = curleft * thumbw / w2;
				thumbr = (curleft + 479) * thumbw / w2;
			}
			if (painttop > 0) {
				thumbt = 0;
				thumbb = thumbb - 1;
			} else {
				thumbt = curtop * thumbh / h2;
				thumbb = (curtop + imgh - 1) * thumbh / h2;
			}
			disp_putimage(32, top, thumbw, thumbh, 0, 0, thumbimg);
			disp_line(34, bottom, 32 + thumbw, bottom, 0);
			disp_line(32 + thumbw, top + 2, 32 + thumbw, bottom - 1, 0);
			disp_rectangle(33 + thumbl, top + thumbt + 1, 33 + thumbr,
						   top + thumbb + 1, 0);
			short b =
				75 - config.imgbrightness > 0 ? 75 - config.imgbrightness : 0;

			disp_rectangle(32 + thumbl, top + thumbt, 32 + thumbr, top + thumbb,
						   disp_grayscale(COLOR_WHITE, 0, 0, 0, b));
		}
	}
	disp_flip();
	return 0;
}

int ctrl_waitreleasekey(int key)
{
	SceCtrlData pad;

	sceCtrlReadBufferPositive(&pad, 1);
	while (pad.Buttons == key) {
		sceKernelDelayThread(20000);
		sceCtrlReadBufferPositive(&pad, 1);
	}
	return 0;
}

void image_up(void)
{
	if (curtop > 0) {
		curtop -= (int) config.imgmvspd * 2;
		if (curtop < 0)
			curtop = 0;
		img_needrp = true;
	}
}

void image_down(void)
{
	if (h2 > imgh && curtop < h2 - imgh) {
		curtop += (int) config.imgmvspd * 2;
		if (curtop > h2 - imgh)
			curtop = h2 - imgh;
		img_needrp = true;
	}
}

void image_left(void)
{
	if (curleft > 0) {
		curleft -= (int) config.imgmvspd * 2;
		if (curleft < 0)
			curleft = 0;
		img_needrp = true;
	}
}

void image_right(void)
{
	if (w2 > PSP_SCREEN_WIDTH && curleft < w2 - PSP_SCREEN_WIDTH) {
		curleft += (int) config.imgmvspd * 2;
		if (curleft > w2 - PSP_SCREEN_WIDTH)
			curleft = w2 - PSP_SCREEN_WIDTH;
		img_needrp = true;
	}
}

void image_move(dword key)
{
	if (key & PSP_CTRL_LEFT && !(key & PSP_CTRL_RIGHT)) {
		image_left();
	}
	if (key & PSP_CTRL_RIGHT && !(key & PSP_CTRL_LEFT)) {
		image_right();
	}
	if (key & PSP_CTRL_UP && !(key & PSP_CTRL_DOWN)) {
		image_up();
	}
	if (key & PSP_CTRL_DOWN && !(key & PSP_CTRL_UP)) {
		image_down();
	}
	thumb = (config.thumb == conf_thumb_scroll);
	img_needrp = thumb | img_needrp;
}

int image_handle_input(dword * selidx, dword key)
{
	if (key == 0)
		goto next;
	secticks = 0;
	if (key == (PSP_CTRL_SELECT | PSP_CTRL_START)) {
		return exit_confirm();
	} else if (key == PSP_CTRL_SELECT) {
		img_needrp = true;
		bool lastbicubic = config.bicubic;

		if (scene_options(selidx)) {
			imgreading = false;
			scene_power_save(true);
			if (imgshow != NULL && imgshow != imgdata) {
				free((void *) imgshow);
				imgshow = NULL;
			}
			if (imgdata != NULL) {
				free((void *) imgdata);
				imgdata = NULL;
			}
			disp_duptocachealpha(50);
			return *selidx;
		}
		if (lastbicubic != config.bicubic)
			img_needrc = true;
		if (config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;
	} else if (key == PSP_CTRL_START) {
		scene_mp3bar();
		img_needrp = true;
	} else if (key == config.imgkey[1] || key == config.imgkey2[1]
			   || key == CTRL_FORWARD) {
		if (config.imgpaging > conf_imgpaging_direct)
			sceKernelDelayThread(200000);
		switch (config.imgpaging) {
			case conf_imgpaging_updown:
				switch (ypos) {
					case 0:
					case 1:
						if (curtop + imgh < h2) {
							curtop += imgh - config.imgpagereserve;
							if (curtop + imgh > h2)
								curtop = h2 - imgh;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curtop > 0) {
							curtop -= imgh - config.imgpagereserve;
							if (curtop < 0)
								curtop = 0;
							img_needrp = true;
							goto next;
						}
						break;
				}
				curtop = 0;
				if (h2 > imgh && ypos == 2)
					curtop = h2 - imgh;
				switch (xpos) {
					case 0:
					case 1:
						if (curleft + PSP_SCREEN_WIDTH < w2) {
							curleft += PSP_SCREEN_WIDTH;
							if (curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curleft > 0) {
							curleft -= PSP_SCREEN_WIDTH;
							if (curleft < 0)
								curleft = 0;
							img_needrp = true;
							goto next;
						}
						break;
				}
				break;
			case conf_imgpaging_leftright:
				switch (xpos) {
					case 0:
					case 1:
						if (curleft + PSP_SCREEN_WIDTH < w2) {
							curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
							if (curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curleft > 0) {
							curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
							if (curleft < 0)
								curleft = 0;
							img_needrp = true;
							goto next;
						}
						break;
				}
				curleft = 0;
				if (w2 > PSP_SCREEN_WIDTH && xpos == 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch (ypos) {
					case 0:
					case 1:
						if (curtop + imgh < h2) {
							curtop += imgh;
							if (curtop + imgh > h2)
								curtop = h2 - imgh;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curtop > 0) {
							curtop -= imgh;
							if (curtop < 0)
								curtop = 0;
							img_needrp = true;
							goto next;
						}
						break;
				}
				break;
			default:;
		}
		dword orgidx = *selidx;

		do {
			if (*selidx < filecount - 1)
				(*selidx)++;
			else {
				if (config.img_no_repeat == false) {
					*selidx = 0;
				} else {
					return *selidx;
				}
			}
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
		if (*selidx != orgidx)
			img_needrf = img_needrc = img_needrp = true;
	} else if (key == config.imgkey[0] || key == config.imgkey2[0]
			   || key == CTRL_BACK) {
		if (config.imgpaging > conf_imgpaging_direct)
			sceKernelDelayThread(200000);
		switch (config.imgpaging) {
			case conf_imgpaging_updown:
				switch (ypos) {
					case 0:
					case 1:
						if (curtop > 0) {
							curtop -= imgh - config.imgpagereserve;
							if (curtop < 0)
								curtop = 0;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curtop + imgh < h2) {
							curtop += imgh - config.imgpagereserve;
							if (curtop + imgh > h2)
								curtop = h2 - imgh;
							img_needrp = true;
							goto next;
						}
						break;
				}
				curtop = 0;
				if (h2 > imgh && ypos < 2)
					curtop = h2 - imgh;
				switch (xpos) {
					case 0:
					case 1:
						if (curleft > 0) {
							curleft -= PSP_SCREEN_WIDTH;
							if (curleft < 0)
								curleft = 0;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curleft + PSP_SCREEN_WIDTH < w2) {
							curleft += PSP_SCREEN_WIDTH;
							if (curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							img_needrp = true;
							goto next;
						}
						break;
				}
				break;
			case conf_imgpaging_leftright:
				switch (xpos) {
					case 0:
					case 1:
						if (curleft > 0) {
							curleft -= PSP_SCREEN_WIDTH - config.imgpagereserve;
							if (curleft < 0)
								curleft = 0;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curleft + PSP_SCREEN_WIDTH < w2) {
							curleft += PSP_SCREEN_WIDTH - config.imgpagereserve;
							if (curleft + PSP_SCREEN_WIDTH > w2)
								curleft = w2 - PSP_SCREEN_WIDTH;
							img_needrp = true;
							goto next;
						}
						break;
				}
				curleft = 0;
				if (w2 > PSP_SCREEN_WIDTH && xpos < 2)
					curleft = w2 - PSP_SCREEN_WIDTH;
				switch (ypos) {
					case 0:
					case 1:
						if (curtop > 0) {
							curtop -= imgh;
							if (curtop < 0)
								curtop = 0;
							img_needrp = true;
							goto next;
						}
						break;
					case 2:
						if (curtop + imgh < h2) {
							curtop += imgh;
							if (curtop + imgh > h2)
								curtop = h2 - imgh;
							img_needrp = true;
							goto next;
						}
						break;
				}
				break;
			default:;
		}
		dword orgidx = *selidx;

		do {
			if (*selidx > 0)
				(*selidx)--;
			else
				*selidx = filecount - 1;
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
		if (*selidx != orgidx)
			img_needrf = img_needrc = img_needrp = true;
	}
#ifdef ENABLE_ANALOG
	else if (key == CTRL_ANALOG && config.img_enable_analog) {
		int x, y, orgtop = curtop, orgleft = curleft;

		ctrl_analog(&x, &y);
		x = x / 31 * (int) config.imgmvspd / 2;
		y = y / 31 * (int) config.imgmvspd / 2;
		curtop += y;
		if (curtop + imgh > h2)
			curtop = (int) h2 - imgh;
		if (curtop < 0)
			curtop = 0;
		curleft += x;
		if (curleft + PSP_SCREEN_WIDTH > w2)
			curleft = (int) w2 - PSP_SCREEN_WIDTH;
		if (curleft < 0)
			curleft = 0;
		thumb = (config.thumb == conf_thumb_scroll);
		img_needrp = (thumb || orgtop != curtop || orgleft != curleft);
	}
#endif
	else if (key &
			 (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN)
			 && !(key &
				  ~(PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP |
					PSP_CTRL_DOWN))) {
		image_move(key);
	} else if (key == config.imgkey[2] || key == config.imgkey2[2]) {
		ctrl_waitrelease();
		if (config.fit == conf_fit_custom)
			config.fit = conf_fit_none;
		else
			config.fit++;
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[10] || key == config.imgkey2[10]) {
		config.bicubic = !config.bicubic;
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[11] || key == config.imgkey2[11]) {
		if (!slideshow) {
			slideshow = true;
			lasttime = time(NULL);
			ctrl_waitrelease();
		} else {
			slideshow = false;
			win_msg("幻灯片播放已经停止！", COLOR_WHITE, COLOR_WHITE,
					config.msgbcolor);
		}
	} else if (key == config.imgkey[7] || key == config.imgkey2[7]) {
		config.imginfobar = !config.imginfobar;
		if (config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;
		if (h2 > imgh && curtop > h2 - imgh)
			curtop = h2 - imgh;
		img_needrc = (config.fit == conf_fit_height);
		img_needrp = true;
		SceCtrlData ctl;
		int t = 0;

		do {
			sceCtrlReadBufferPositive(&ctl, 1);
			sceKernelDelayThread(10000);
			t += 10000;
		} while (ctl.Buttons != 0 && t <= 500000);
	} else if (key == config.imgkey[9] || key == config.imgkey2[9]
			   || key == CTRL_PLAYPAUSE) {
		if (slideshow) {
			slideshow = false;
			win_msg("幻灯片播放已经停止！", COLOR_WHITE, COLOR_WHITE,
					config.msgbcolor);
		} else {
			imgreading = false;
			scene_power_save(true);
			if (imgshow != NULL && imgshow != imgdata) {
				free((void *) imgshow);
				imgshow = NULL;
			}
			if (imgdata != NULL) {
				free((void *) imgdata);
				imgdata = NULL;
			}
			disp_duptocachealpha(50);
			return *selidx;
		}
	} else if (key == config.imgkey[8] || key == config.imgkey2[8]) {
		if (!showinfo) {
			img_needrp = true;
			showinfo = true;
		}
	} else if (key == config.imgkey[5] || key == config.imgkey2[5]) {
		if (config.rotate == conf_rotate_0)
			config.rotate = conf_rotate_270;
		else
			config.rotate--;
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[6] || key == config.imgkey2[6]) {
		if (config.rotate == conf_rotate_270)
			config.rotate = conf_rotate_0;
		else
			config.rotate++;
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[3] || key == config.imgkey2[3]) {
		if (config.scale > 200)
			config.scale -= 50;
		else if (config.scale > 10)
			config.scale -= 10;
		else
			goto next;
		config.fit = conf_fit_custom;
		img_needrc = img_needrp = true;
		ctrl_waitreleasekey(config.imgkey[3]);
	} else if (key == config.imgkey[4] || key == config.imgkey2[4]) {
		if (config.scale < 200)
			config.scale += 10;
		else if (config.scale < 1000)
			config.scale += 50;
		else
			goto next;
		config.fit = conf_fit_custom;
		img_needrc = img_needrp = true;
		ctrl_waitreleasekey(config.imgkey[4]);
	} else
		img_needrf = img_needrc = false;
  next:
	return -1;
}

dword scene_readimage(dword selidx)
{
	w2 = 0, h2 = 0, thumbw = 0, thumbh = 0, paintleft = 0, painttop = 0;
	imgdata = NULL, imgshow = NULL;
	oldangle = 0;
	curtop = 0, curleft = 0, xpos = 0, ypos = 0;
	img_needrf = true, img_needrc = true, img_needrp = true, showinfo =
		false, thumb = false;
	slideshow = false;
	now = 0, lasttime = 0;
	imgreading = true;
	if (config.imginfobar)
		imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
	else
		imgh = PSP_SCREEN_HEIGHT;
	u64 timer_start, timer_end;

	sceRtcGetCurrentTick(&timer_start);
	while (1) {
		u64 dbgnow, dbglasttick;

		if (img_needrf) {
			sceRtcGetCurrentTick(&dbglasttick);
			dword ret = scene_reloadimage(selidx);

			if (ret == -1)
				return selidx;
			img_needrf = false;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, "装载图像时间: %.2f秒",
					   pspDiffTime(&dbgnow, &dbglasttick));
		}
		if (img_needrc) {
			sceRtcGetCurrentTick(&dbglasttick);
			scene_rotateimage();
			img_needrc = false;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, "旋转图像时间: %.2f秒",
					   pspDiffTime(&dbgnow, &dbglasttick));
		}
		if (img_needrp) {
			scene_printimage(selidx);
			img_needrp = false;
		}
		now = time(NULL);
		dword key = 0;

		if (config.thumb == conf_thumb_scroll && thumb) {
			thumb = false;
			img_needrp = true;
			key = ctrl_read_cont();
		} else if (!slideshow || now - lasttime < config.slideinterval) {
			key = ctrl_read_cont();
		}
		if (slideshow) {
			scePowerTick(0);
			if (now - lasttime >= config.slideinterval) {
				key = CTRL_FORWARD;
				lasttime = now;
			}
		}
		if (showinfo && (key & PSP_CTRL_CIRCLE) == 0) {
			img_needrp = true;
			showinfo = false;
		}
		int ret = image_handle_input(&selidx, key);

		sceRtcGetCurrentTick(&timer_end);
		if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
			sceRtcGetCurrentTick(&timer_start);
			secticks++;
		}
		if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
#ifdef ENABLE_MUSIC
			mp3_powerdown();
#endif
			fat_powerdown();
			scePowerRequestSuspend();
			secticks = 0;
		}
		if (ret != -1)
			return ret;
		if (config.dis_scrsave)
			scePowerTick(0);
	}
	imgreading = false;
	scene_power_save(false);
	if (imgshow != NULL && imgshow != imgdata) {
		free((void *) imgshow);
		imgshow = NULL;
	}
	if (imgdata != NULL) {
		free((void *) imgdata);
		imgdata = NULL;
	}
	disp_duptocachealpha(50);
	return selidx;
}

t_win_menu_op scene_imgkey_menucb(dword key, p_win_menuitem item, dword * count,
								  dword max_height, dword * topindex,
								  dword * index)
{
	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_CIRCLE:
			disp_duptocache();
			disp_waitv();
			disp_rectangle(239 - DISP_FONTSIZE * 3, 135 - DISP_FONTSIZE / 2,
						   240 + DISP_FONTSIZE * 3, 136 + DISP_FONTSIZE / 2,
						   COLOR_WHITE);
			disp_fillrect(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2,
						  239 + DISP_FONTSIZE * 3, 135 + DISP_FONTSIZE / 2,
						  RGB(0x8, 0x18, 0x10));
			disp_putstring(240 - DISP_FONTSIZE * 3, 136 - DISP_FONTSIZE / 2,
						   COLOR_WHITE, (const byte *) "请按对应按键");
			disp_flip();
			dword key, key2;
			SceCtrlData ctl;

			ctrl_waitrelease();
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
				key = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			} while ((key &
					  ~(PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT |
						PSP_CTRL_RIGHT)) == 0);
			key2 = key;
			while ((key2 & key) == key) {
				key = key2;
				sceCtrlReadBufferPositive(&ctl, 1);
				key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			}
			if (config.imgkey[*index] == key || config.imgkey2[*index] == key)
				return win_menu_op_force_redraw;
			int i;

			for (i = 0; i < 12; i++) {
				if (i == *index)
					continue;
				if (config.imgkey[i] == key) {
					config.imgkey[i] = config.imgkey2[*index];
					if (config.imgkey[i] == 0) {
						config.imgkey[i] = config.imgkey2[i];
						config.imgkey2[i] = 0;
					}
					break;
				}
				if (config.imgkey2[i] == key) {
					config.imgkey2[i] = config.imgkey2[*index];
					break;
				}
			}
			config.imgkey2[*index] = config.imgkey[*index];
			config.imgkey[*index] = key;
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
			} while (ctl.Buttons != 0);
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

void scene_imgkey_predraw(p_win_menuitem item, dword index, dword topindex,
						  dword max_height)
{
	char keyname[256];

	disp_rectangle(239 - DISP_FONTSIZE * 10, 128 - 7 * DISP_FONTSIZE,
				   240 + DISP_FONTSIZE * 10, 144 + 6 * DISP_FONTSIZE,
				   COLOR_WHITE);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 129 - 7 * DISP_FONTSIZE,
				  239 + DISP_FONTSIZE * 10, 128 - 6 * DISP_FONTSIZE,
				  config.menubcolor);
	disp_fillrect(240 - DISP_FONTSIZE * 10, 127 - 6 * DISP_FONTSIZE,
				  239 + DISP_FONTSIZE * 10, 143 + 6 * DISP_FONTSIZE,
				  config.menubcolor);
	disp_putstring(240 - DISP_FONTSIZE * 5, 129 - 7 * DISP_FONTSIZE,
				   COLOR_WHITE, (const byte *) "按键设置   △ 删除");
	disp_line(240 - DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE,
			  239 + DISP_FONTSIZE * 10, 129 - 6 * DISP_FONTSIZE, COLOR_WHITE);
	dword i;

	for (i = topindex; i < topindex + max_height; i++) {
		conf_get_keyname(config.imgkey[i], keyname);
		if (config.imgkey2[i] != 0) {
			char keyname2[256];

			conf_get_keyname(config.imgkey2[i], keyname2);
			STRCAT_S(keyname, " | ");
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(238 - DISP_FONTSIZE * 5,
					   131 - 6 * DISP_FONTSIZE + (1 + DISP_FONTSIZE) * i,
					   COLOR_WHITE, (const byte *) keyname);
	}
}

dword scene_imgkey(dword * selidx)
{
	t_win_menuitem item[12];

	STRCPY_S(item[0].name, "上一张图");
	STRCPY_S(item[1].name, "下一张图");
	STRCPY_S(item[2].name, "缩放模式");
	STRCPY_S(item[3].name, "缩小图片");
	STRCPY_S(item[4].name, "放大图片");
	STRCPY_S(item[5].name, "左旋90度");
	STRCPY_S(item[6].name, "右旋90度");
	STRCPY_S(item[7].name, "  信息栏");
	STRCPY_S(item[8].name, "显示信息");
	STRCPY_S(item[9].name, "退出浏览");
	STRCPY_S(item[10].name, "缩放引擎");
	STRCPY_S(item[11].name, "幻灯播放");
	dword i;

	for (i = 0; i < 12; i++) {
		item[i].width = 8;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}
	dword index;

	while ((index =
			win_menu(240 - DISP_FONTSIZE * 10, 130 - 6 * DISP_FONTSIZE, 8, 12,
					 item, 12, 0, 0, config.menubcolor, true,
					 scene_imgkey_predraw, NULL,
					 scene_imgkey_menucb)) != INVALID);
	return 0;
}
#endif
