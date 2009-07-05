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

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "win.h"
#include "common/datatype.h"
#include "common/utils.h"
#include "fs.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "image_queue.h"
#include "pspscreen.h"
#include "ctrl.h"
#include "xrhal.h"
#include "kubridge.h"
#include "common/utils.h"
#include "thread_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

extern p_win_menuitem filelist;
extern dword filecount;
volatile dword *cache_selidx = NULL;

volatile cacher_context ccacher;

static volatile bool cacher_cleared = true;
static dword cache_img_cnt = 0;
static uint16_t curr_times = 0, avoid_times = 0;

static struct psp_mutex_t cacher_locker;

enum
{
	CACHE_EVENT_DELETED = 1,
	CACHE_EVENT_UNDELETED = 2
};

static SceUID cache_del_event = -1;

static inline int cache_lock(void)
{
//  dbg_printf(d, "%s", __func__);
	return xr_lock(&cacher_locker);
}

static inline int cache_unlock(void)
{
//  dbg_printf(d, "%s", __func__);
	return xr_unlock(&cacher_locker);
}

void cache_on(bool on)
{
	if (on) {
		cache_img_cnt = 0;
		cache_next_image();
		ccacher.first_run = true;
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);

		ccacher.on = on;
	} else {
		ccacher.on = on;

		while (!cacher_cleared) {
			xrKernelDelayThread(100000);
		}

		if (ccacher.caches != NULL) {
			free(ccacher.caches);
			ccacher.caches = NULL;
		}

		ccacher.caches_size = ccacher.caches_cap = 0;
	}
}

// Ask cacher for next image
void cache_next_image(void)
{
	ccacher.selidx_moved = true;
	curr_times = avoid_times = 0;
}

static void cache_clear(void)
{
	int i;

	cache_lock();

	for (i = 0; i < ccacher.caches_size; ++i) {
		if (ccacher.caches[i].data != NULL) {
			dbg_printf(d, "%s: %d data 0x%08x", __func__, i,
					   (unsigned) ccacher.caches[i].data);
			free(ccacher.caches[i].data);
			ccacher.caches[i].data = NULL;
		}

		if (ccacher.caches[i].status == CACHE_OK) {
			ccacher.memory_usage -=
				ccacher.caches[i].width * ccacher.caches[i].height *
				sizeof(pixel);
		}
	}

	ccacher.caches_size = 0;
	cacher_cleared = true;
	cache_unlock();
}

void cache_set_forward(bool forward)
{
	cache_lock();

	if (ccacher.isforward != forward) {
		cache_clear();
		ccacher.first_run = true;
	}

	ccacher.isforward = forward;
	cache_unlock();
}

/**
 * 删除缓存, 并释放资源
 */
static int cache_delete(size_t pos)
{
	cache_lock();
	cache_image_t *p = &ccacher.caches[pos];

	if (p->data != NULL) {
		dbg_printf(d, "%s: data 0x%08x", __func__, (unsigned) p->data);
		free(p->data);
		p->data = NULL;
	}

	if (p->status == CACHE_OK) {
		ccacher.memory_usage -= p->width * p->height * sizeof(pixel);
	}

	memmove(&ccacher.caches[pos], &ccacher.caches[pos + 1],
			(ccacher.caches_size - pos - 1) * sizeof(ccacher.caches[0]));
	ccacher.caches_size--;
	cache_unlock();

	return 0;
}

int cache_delete_first(void)
{
	cache_lock();

	if (ccacher.caches_size != 0 && ccacher.caches != NULL) {
		int ret;

		ret = cache_delete(0);
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);
		cache_unlock();

		return ret;
	}

	cache_unlock();
	return -1;
}

static cache_image_t *cache_get(const char *archname, const char *filename);

/**
 * @param selidx filelist中的文件位置
 * @param where 文件位置类型
 *
 */
static int cache_add_by_selidx(dword selidx, int where)
{
	t_fs_filetype type;
	cache_image_t img;
	const char *archname;
	const char *filename;
	dword filesize;

	archname = config.shortpath;
	filename = filelist[selidx].compname->ptr;
	filesize = filelist[selidx].data3;

	type = fs_file_get_type(filename);

	if (!fs_is_image(type)) {
		return -1;
	}

	if (cache_get(archname, filename) != NULL) {
		dbg_printf(d, "SERVER: %s: Image %s duplicate load, FIXME", __func__,
				   filename);

		return -1;
	}

	cache_lock();

	memset(&img, 0, sizeof(img));
	img.archname = archname;
	img.filename = filename;
	img.where = where;
	img.status = CACHE_INIT;
	img.selidx = selidx;
	img.filesize = filesize;

	if (ccacher.caches_size < ccacher.caches_cap) {
		ccacher.caches[ccacher.caches_size] = img;
		ccacher.caches_size++;
		cacher_cleared = false;
	} else {
		dbg_printf(d, "SERVER: cannot add cache any more: size %u cap %u",
				   ccacher.caches_size, ccacher.caches_cap);
		cache_unlock();

		return -1;
	}
	cache_unlock();

	return 0;
}

void dbg_dump_cache(void)
{
	cache_lock();

	cache_image_t *p = ccacher.caches;
	dword c;

	for (c = 0; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_OK || p->status == CACHE_FAILED)
			c++;
	}

	dbg_printf(d, "CLIENT: Dumping cache[%u] %u/%ukb, %u finished",
			   ccacher.caches_size, (unsigned) ccacher.memory_usage / 1024,
			   (unsigned) get_free_mem() / 1024, (unsigned) c);

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		dbg_printf(d, "%d: %u st %u res %d mem %lukb", p - ccacher.caches,
				   (unsigned) p->selidx, p->status, p->result,
				   p->width * p->height * sizeof(pixel) / 1024L);
	}

	cache_unlock();
}

static void copy_cache_image(cache_image_t * dst, const cache_image_t * src)
{
	memcpy(dst, src, sizeof(*src));
}

static void free_cache_image(cache_image_t * p)
{
	if (p == NULL)
		return;

	if (p->data != NULL) {
		free(p->data);
		p->data = NULL;
	}

	if (p->exif_array != NULL) {
		buffer_array_free(p->exif_array);
		p->exif_array = NULL;
	}
}

int start_cache_next_image(void)
{
	cache_image_t *p = NULL;
	cache_image_t tmp;
	t_fs_filetype ft;
	dword free_memory;

	if (avoid_times && curr_times++ < avoid_times) {
//      dbg_printf(d, "%s: curr_times %d avoid time %d", __func__, curr_times, avoid_times);
		return -1;
	}

	free_memory = get_free_mem();

	if (config.scale >= 100) {
		if (free_memory < 8 * 1024 * 1024) {
			return -1;
		}
	} else if (free_memory < 1024 * 1024) {
		return -1;
	}

	cache_lock();

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == ccacher.caches + ccacher.caches_size || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	copy_cache_image(&tmp, p);
	cache_unlock();

	ft = fs_file_get_type(tmp.filename);
	int fid = freq_enter_hotzone();

	if (tmp.where == scene_in_dir) {
		char fullpath[PATH_MAX];

		STRCPY_S(fullpath, tmp.archname);
		STRCAT_S(fullpath, tmp.filename);
		tmp.result =
			image_open_archive(fullpath, tmp.archname, ft, &tmp.width,
							   &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	} else {
		tmp.result =
			image_open_archive(tmp.filename, tmp.archname, ft, &tmp.width,
							   &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	}

	if (tmp.result == 0 && tmp.data != NULL && config.imgbrightness != 100) {
		pixel *t = tmp.data;
		short b = 100 - config.imgbrightness;
		dword i;

		for (i = 0; i < tmp.height * tmp.width; i++) {
			*t = disp_grayscale(*t, 0, 0, 0, b);
			t++;
		}
	}

	freq_leave(fid);

	cache_lock();

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// recheck the first unloaded (and not failed) image, for we haven't locked cache for a while
	if (p == ccacher.caches + ccacher.caches_size || p->status == CACHE_FAILED) {
		free_cache_image(&tmp);
		cache_unlock();
		return 0;
	}

	if (tmp.result == 0) {
		dword memory_used;

		memory_used = tmp.width * tmp.height * sizeof(pixel);

//      dbg_printf(d, "SERVER: Image %u finished loading", (unsigned)tmp.selidx);
//      dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
		ccacher.memory_usage += memory_used;
		cacher_cleared = false;
		tmp.status = CACHE_OK;
		copy_cache_image(p, &tmp);
		tmp.data = NULL;
		tmp.exif_array = NULL;
		free_cache_image(&tmp);
		curr_times = avoid_times = 0;
	} else if ((tmp.result == 4 || tmp.result == 5)
			   || (tmp.where == scene_in_rar && tmp.result == 6)) {
		// out of memory
		// if unrar throwed a bad_cast exception when run out of memory, result can be 6 also.

		// is memory completely out of memory?
		if (ccacher.memory_usage == 0) {
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), giving up", (unsigned)tmp.selidx, tmp.result);
			tmp.status = CACHE_FAILED;
			copy_cache_image(p, &tmp);
			p->data = NULL;
		} else {
			// retry later
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), retring", (unsigned)tmp.selidx, tmp.result);
//          dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
			if (avoid_times) {
				avoid_times *= 2;
			} else {
				avoid_times = 1;
			}

			avoid_times = min(avoid_times, 32767);
			curr_times = 0;
		}

		free_cache_image(&tmp);
	} else {
//      dbg_printf(d, "SERVER: Image %u finished failed(%u)", (unsigned)tmp.selidx, tmp.result);
		tmp.status = CACHE_FAILED;
		copy_cache_image(p, &tmp);
		p->data = NULL;
		free_cache_image(&tmp);
	}

	cache_unlock();

	return 0;
}

static dword cache_get_next_image(dword pos, bool forward)
{
	if (forward) {
		do {
			if (pos < filecount - 1) {
				pos++;
			} else {
				pos = 0;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[pos].data));
	} else {
		do {
			if (pos > 0) {
				pos--;
			} else {
				pos = filecount - 1;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[pos].data));
	}

	return pos;
}

/* Count how many img in archive */
static dword count_img(void)
{
	if (filecount == 0 || filelist == NULL)
		return 0;

	dword i = 0;

	if (cache_img_cnt != 0)
		return cache_img_cnt;

	for (i = 0; i < filecount; ++i) {
		if (fs_is_image((t_fs_filetype) filelist[i].data))
			cache_img_cnt++;
	}

	return cache_img_cnt;
}

static int start_cache(dword selidx)
{
	int re;
	dword pos;
	dword size;

	if (ccacher.first_run) {
		ccacher.first_run = false;
	} else {
		SceUInt timeout = 10000;

		// wait until user notify cache delete
		re = xrKernelWaitEventFlag(cache_del_event, CACHE_EVENT_DELETED,
								   PSP_EVENT_WAITAND, NULL, &timeout);

		if (re == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			return 0;
		}

		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_UNDELETED);
	}

	cache_lock();

	pos = selidx;
	size = ccacher.caches_size;

	while (size-- > 0) {
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	re = min(ccacher.caches_cap, count_img()) - ccacher.caches_size;
	dbg_printf(d, "SERVER: start pos %u selidx %u caches_size %u re %u",
			   (unsigned) pos, (unsigned) selidx,
			   (unsigned) ccacher.caches_size, (unsigned) re);

	if (re == 0) {
		cache_unlock();
		return 0;
	}
	// dbg_printf(d, "SERVER: Wait for new selidx: memory usage %uKB", (unsigned) ccacher.memory_usage / 1024);
	// dbg_printf(d, "SERVER: %d images to cache, selidx %u, caches_size %u", re, (unsigned)selidx, (unsigned)ccacher.caches_size);

	while (re-- > 0) {
		dbg_printf(d, "SERVER: add cache image %u", (unsigned) pos);
		cache_add_by_selidx(pos, where);
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	cache_unlock();

	return re;
}

int cache_routine(void)
{
	if (ccacher.on) {
		if (!ccacher.selidx_moved && ccacher.on) {
			// cache next image
			start_cache_next_image();

			return 0;
		}

		if (!ccacher.on) {
			return 0;
		}

		start_cache(*cache_selidx);
		ccacher.selidx_moved = false;
	} else {
		// clean up all remain cache now
		cache_clear();
	}

	xrKernelDelayThread(100000);

	return 0;
}

int cache_init(void)
{
	xr_lock_init(&cacher_locker);
	ccacher.caches_size = 0;
	ccacher.caches_cap = 0;

	cache_del_event = xrKernelCreateEventFlag("cache_del_event", 0, 0, 0);
	ccacher.caches = NULL;

	return 0;
}

int cache_setup(unsigned max_cache_img, dword * c_selidx)
{
	cache_selidx = c_selidx;
	ccacher.caches_cap = max_cache_img;

	ccacher.caches =
		safe_realloc(ccacher.caches,
					 ccacher.caches_cap * sizeof(ccacher.caches[0]));

	cacher_cleared = true;

	return 0;
}

void cache_free(void)
{
	cache_on(false);

	xr_lock_destroy(&cacher_locker);

	if (cache_del_event >= 0) {
		xrKernelDeleteEventFlag(cache_del_event);
		cache_del_event = -1;
	}
}

/**
 * 得到缓存信息
 */
static cache_image_t *cache_get(const char *archname, const char *filename)
{
	cache_image_t *p;

	if (ccacher.caches == NULL || ccacher.caches_size == 0 || filename == NULL) {
		return NULL;
	}

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (((archname == p->archname && archname == NULL)
			 || !strcmp(p->archname, archname))
			&& !strcmp(p->filename, filename)) {
			return p;
		}
	}

	return NULL;
}

int cache_get_loaded_size()
{
	cache_image_t *p = ccacher.caches;
	int c;

	for (c = 0; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_OK)
			c++;
	}

	return c;
}

void cache_reload_all(void)
{
	cache_image_t *p;
	int i;

	cache_lock();

	for (i = 0; i < ccacher.caches_size; ++i) {
		p = &ccacher.caches[i];

		if (p->status == CACHE_OK) {
			free_cache_image(p);
			ccacher.memory_usage -= p->width * p->height * sizeof(pixel);
		}

		p->status = CACHE_INIT;
	}

	cache_unlock();
}
