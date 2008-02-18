/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#ifdef ENABLE_BG

#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "image.h"
#include "bg.h"
#include "pspscreen.h"
#include "config.h"

extern t_conf config;

static pixel *bg_start =
	(pixel *) (0x44000000 + 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES * 2);
static dword imgwidth, imgheight;

static void bg_set_color(pixel* bg_addr, dword bgcolor)
{
	pixel* bg_buf, *bg_end;
	for (bg_buf = bg_addr, bg_end = bg_addr + 0x22000; bg_buf < bg_end;
		 bg_buf++)
		*bg_buf = bgcolor;
}

extern void bg_load(const char *filename, pixel bgcolor, t_fs_filetype ft,
					dword grayscale)
{
	pixel *imgdata, *imgshow =
		NULL, *img_buf, *bg_buf, *bg_line, *bg_end, *bg_lineend;
	dword width, height, w2, h2, left, top;
	pixel bgc;
	int result;

	switch (ft) {
		case fs_filetype_png:
			result = image_readpng(filename, &width, &height, &imgdata, &bgc);
			break;
		case fs_filetype_gif:
			result = image_readgif(filename, &width, &height, &imgdata, &bgc);
			break;
		case fs_filetype_jpg:
			result = image_readjpg(filename, &width, &height, &imgdata, &bgc);
			break;
		case fs_filetype_tga:
			result = image_readtga(filename, &width, &height, &imgdata, &bgc);
			break;
		case fs_filetype_bmp:
			result = image_readbmp(filename, &width, &height, &imgdata, &bgc);
			break;
		default:
			result = -1;
	}

	if (result != 0) {
		// if load bg image fail, continue to set bgcolor
		bg_set_color(bg_start, bgcolor);
		config.have_bg = true;
		return;
	}
	if (width > PSP_SCREEN_WIDTH) {
		h2 = height * PSP_SCREEN_WIDTH / width;
		if (h2 > PSP_SCREEN_HEIGHT) {
			h2 = PSP_SCREEN_HEIGHT;
			w2 = width * PSP_SCREEN_HEIGHT / height;
		} else
			w2 = PSP_SCREEN_WIDTH;
	} else if (height > PSP_SCREEN_HEIGHT) {
		h2 = PSP_SCREEN_HEIGHT;
		w2 = width * PSP_SCREEN_HEIGHT / height;
	} else {
		h2 = height;
		w2 = width;
	}
	if (width != w2 || height != h2) {
		imgshow = (pixel *) malloc(sizeof(pixel) * w2 * h2);
		if (imgshow == NULL) {
			free((void *) imgdata);
			return;
		}
		image_zoom_bicubic(imgdata, width, height, imgshow, w2, h2);
	} else
		imgshow = imgdata;

	if (w2 < PSP_SCREEN_WIDTH)
		left = (PSP_SCREEN_WIDTH - w2) / 2;
	else
		left = 0;
	if (h2 < PSP_SCREEN_HEIGHT)
		top = (PSP_SCREEN_HEIGHT - h2) / 2;
	else
		top = 0;

	bg_set_color(bg_start, bgcolor);

	img_buf = imgshow;
	dword r = RGB_R(bgcolor), g = RGB_G(bgcolor), b = RGB_B(bgcolor);

	for (bg_buf = bg_start + top * 512 + left, bg_end = bg_buf + h2 * 512;
			bg_buf < bg_end; bg_buf += 512)
		for (bg_line = bg_buf, bg_lineend = bg_buf + w2; bg_line < bg_lineend;
				bg_line++) {
			*bg_line = disp_grayscale(*img_buf, r, g, b, grayscale);
			img_buf++;
		}

	config.have_bg = true;

	if (imgshow != imgdata)
		free((void *) imgshow);
	imgwidth = w2;
	imgheight = h2;
	free((void *) imgdata);
}

extern bool bg_display()
{
	if (config.have_bg) {
		memcpy(vram_start, bg_start, 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
		return true;
	}
	return false;
}

extern void bg_cancel()
{
	config.have_bg = false;
}

static byte *_cache = NULL;

extern void bg_cache()
{
	if (!config.have_bg)
		return;
	if (_cache != NULL)
		free((void *) _cache);
	_cache = malloc(512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	if (_cache == NULL)
		return;
	memcpy(_cache, bg_start, 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
}

extern void bg_restore()
{
	if (!config.have_bg)
		return;
	if (_cache == NULL)
		return;
	memcpy(bg_start, _cache, 512 * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	free((void *) _cache);
	_cache = NULL;
}

#endif
