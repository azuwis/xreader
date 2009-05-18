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
#include "rar_speed_test.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "commons.h"
#include "image_queue.h"
#include "pspscreen.h"
#include "ctrl.h"
#include "xrhal.h"

extern p_win_menuitem filelist;

#define MAX_CACHE_IMAGE 10
#define MAX_MEMORY_USAGE ( 40 * 1024 * 1024L )

enum
{
	CACHE_INIT = 0,
	CACHE_OK = 1,
	CACHE_FAILED = 2
};

static cache_image_t *caches = NULL;
static size_t caches_cap = 0, caches_size = 0;
static SceUID cache_lock_uid = -1;
static SceUID thid = -1;
static volatile bool cache_switch = false;
static volatile bool cache_forward = true;
static volatile dword memory_usage = 0;
static bool first_run = true;
volatile bool selidx_moved = false;
static volatile bool cacher_should_exit = false;
static volatile bool cacher_exited = false;
static volatile bool cacher_cleared = true;
static dword cache_img_cnt = 0;
static int cache_memory_usage = 0;

enum
{
	CACHE_EVENT_DELETED = 1,
	CACHE_EVENT_UNDELETED = 2
};

static SceUID cache_del_event = -1;

#if 0
/// Add lock to malloc

#ifndef F_mlock
#define F_mlock
// by default newlib-PSP doesn't have a lock with malloc() / free()
static unsigned int lock_count = 0;
static unsigned int intr_flags = 0;

void __malloc_lock(struct _reent *ptr)
{
	unsigned int flags = pspSdkDisableInterrupts();

	if (lock_count == 0) {
		intr_flags = flags;
	}

	lock_count++;
}

void __malloc_unlock(struct _reent *ptr)
{
	if (--lock_count == 0) {
		pspSdkEnableInterrupts(intr_flags);
	}
}
#endif
#endif

static unsigned get_avail_memory(void)
{
	int memory;
	extern unsigned int get_free_mem(void);

	memory = get_free_mem();

	if (memory >= 1024 * 1024L) {
		// reverse for 1MB
		memory -= 1024 * 1024L;
	} else {
		memory = 0;
	}

	memory = min(MAX_MEMORY_USAGE, memory);

	return (cache_memory_usage = memory);
}

static inline int cache_lock(void)
{
	dbg_printf(d, "%s", __func__);
	SceUInt tm_out;

	tm_out = 5 * 10000000L;
	return xrKernelWaitSema(cache_lock_uid, 1, &tm_out);
}

static inline int cache_unlock(void)
{
	dbg_printf(d, "%s", __func__);
	return xrKernelSignalSema(cache_lock_uid, 1);
}

void cache_on(bool on)
{
	if (on) {
		cache_img_cnt = 0;
		cache_memory_usage = 0;
		selidx_moved = true;
		first_run = true;
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);
	}

	cache_switch = on;
}

// Ask cacher for next image
void cache_next_image(void)
{
	selidx_moved = true;
}

static void cache_clear_without_lock()
{
	int i;

	for (i = 0; i < caches_size; ++i) {
		if (caches[i].data != NULL) {
			free(caches[i].data);
		}

		if (caches[i].status == CACHE_OK) {
			memory_usage -= caches[i].width * caches[i].height * sizeof(pixel);
		}
	}

	caches_size = 0;
}

static void cache_clear(void)
{
	if (cache_lock() == 0) {
		cache_clear_without_lock();
		cache_unlock();
	}
}

void cache_set_forward(bool forward)
{
	if (cache_lock() == 0) {

		if (cache_forward != forward) {
			cache_clear_without_lock();
			first_run = true;
		}

		cache_forward = forward;
		cache_unlock();
	}
}

bool user_ask_for_exit = false;
bool user_ask_for_next_image = false;
bool user_ask_for_prev_image = false;

extern dword filecount;

void parse_input()
{
	dword key = ctrl_read();

	if (key == PSP_CTRL_LTRIGGER) {
		dbg_printf(d, "user has asked for prev image");
		user_ask_for_prev_image = true;
	}
	if (key == PSP_CTRL_RTRIGGER) {
		dbg_printf(d, "user has asked for next image");
		user_ask_for_next_image = true;
	}
	if (key == PSP_CTRL_CROSS) {
		dbg_printf(d, "user has asked for exit");
		user_ask_for_exit = true;
	}
	if (key == PSP_CTRL_START)
		dbg_dump_cache();

//  ctrl_waitrelease();
}

/**
 * 删除缓存, 并释放资源
 */
static int cache_delete_without_lock(cache_image_t * p)
{
	size_t pos;

	if (p == NULL)
		return -1;

	if (p->data != NULL) {
		free(p->data);
		p->data = NULL;
	}

	pos = p - caches;

	if (p->status == CACHE_OK) {
		memory_usage -= p->width * p->height * sizeof(pixel);
	}

	memmove(&caches[pos], &caches[pos + 1],
			(caches_size - pos - 1) * sizeof(caches[0]));
//  caches = safe_realloc(caches, sizeof(caches[0]) * (caches_size - 1));
	caches_size--;

	return 0;
}

int cache_delete_first(void)
{
	if (cache_lock() == 0) {

		if (caches_size != 0 && caches != NULL) {
			int ret;

			ret = cache_delete_without_lock(&caches[0]);
			xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);
			cache_unlock();

			return ret;
		}

		cache_unlock();
	}
	return -1;
}

int get_image(dword selidx)
{
	cache_image_t *img;
	u64 start, now;
	int ret;

	xrRtcGetCurrentTick(&start);

//  if (!first_run) {
//      xrKernelWaitEventFlag (cache_del_event, CACHE_EVENT_UNDELETED, PSP_EVENT_WAITAND, NULL, NULL);
//  }

	while (caches_size == 0) {
		xrKernelDelayThread(10000);
	}

	img = &caches[0];

	DBG_ASSERT(d, "CLIENT: img->selidx == selidx", img->selidx == selidx);

	while (img->status == CACHE_INIT) {
//      dbg_printf(d, "CLIENT: Wait image %u %s load finish", (unsigned) selidx, filename);
		xrKernelDelayThread(10000);
	}

	xrRtcGetCurrentTick(&now);

	if (img->status == CACHE_OK && img->result == 0) {
		dbg_printf(d, "CLIENT: Image %u load OK in %.3f s, %ux%u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start),
				   (unsigned) img->width, (unsigned) img->height);
		ret = 0;
	} else {
		dbg_printf(d,
				   "CLIENT: Image %u load FAILED in %.3f s, %ux%u, status %u, result %u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start),
				   (unsigned) img->width, (unsigned) img->height, img->status,
				   img->result);
		ret = -1;
	}

	// Draw image
	if (img->data) {
		disp_flip();
		disp_putimage(0, 0, img->width, img->height, 0, 0, img->data);
		disp_flip();
	}
//  xrKernelDelayThread(1000000);

	return ret;
}

void update_selidx(volatile dword * selidx, dword count, bool is_forward)
{
	if (is_forward) {
		do {
			if (*selidx < count - 1) {
				(*selidx)++;
			} else {
				*selidx = 0;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
	} else {
		do {
			if (*selidx > 0) {
				(*selidx)--;
			} else {
				*selidx = count - 1;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
	}
}

dword selidx = 0;

int test_cache()
{
	bool is_forward = true, is_needrf = true;

	selidx = 0;

	while (!fs_is_image((t_fs_filetype) filelist[selidx].data)
		   && selidx < filecount) {
		selidx++;
	}

	if (selidx == filecount)
		return -1;

	ctrl_init();
	disp_init();
	cache_set_forward(true);
	// enable cache now
	cache_on(true);

	user_ask_for_exit = false;

	while (!user_ask_for_exit) {
		if (user_ask_for_next_image) {
			is_forward = true;
			is_needrf = true;
			cache_set_forward(is_forward);
			user_ask_for_next_image = false;
			update_selidx(&selidx, filecount, is_forward);
			cache_next_image();
		}

		if (user_ask_for_prev_image) {
			is_forward = false;
			is_needrf = true;
			cache_set_forward(is_forward);
			user_ask_for_prev_image = false;
			update_selidx(&selidx, filecount, is_forward);
			cache_next_image();
		}

		if (is_needrf) {
			cache_delete_first();

			if (get_image(selidx) != 0) {
				user_ask_for_exit = true;
			}

			is_needrf = false;
		}

		parse_input();
	}

	// disable cache now
	cache_on(false);

	return 0;
}

/**
 * 根据路径添加缓存
 * 
 * @param archname 可能的档案名
 * @param filename 文件名
 * @param where 文件位置类型
 *
 */
int cache_add_by_path(const char *archname, const char *filename, int where,
					  dword selidx)
{
	t_fs_filetype type;
	cache_image_t img;

	type = fs_file_get_type(filename);

	if (!fs_is_image(type)) {
		return -1;
	}

	if (cache_get(archname, filename) != NULL) {
		dbg_printf(d, "SERVER: %s: Image %s duplicate load, FIXME", __func__,
				   filename);

		return -1;
	}

	if (cache_lock() < 0) {
		return -1;
	}

	memset(&img, 0, sizeof(img));
	img.archname = archname;
	img.filename = filename;
	img.where = where;
	img.status = CACHE_INIT;
	img.selidx = selidx;

	// add to cache table
#if 0
	caches = safe_realloc(caches, sizeof(caches[0]) * (caches_size + 1));

	if (caches == NULL) {
		cache_unlock();
		return -1;
	}
#endif

	if (caches_size < caches_cap) {
		caches[caches_size] = img;
		caches_size++;
	} else {
		dbg_printf(d, "SERVER: cannot add cache any more: size %u cap %u",
				   caches_size, caches_cap);
		cache_unlock();

		return -1;
	}
	cache_unlock();

	return 0;
}

void dbg_dump_cache(void)
{
	if (cache_lock() < 0) {
		return;
	}

	cache_image_t *p = caches;
	dword c;

	for (c = 0; p != caches + caches_size; ++p) {
		if (p->status == CACHE_OK || p->status == CACHE_FAILED)
			c++;
	}

	dbg_printf(d, "CLIENT: Dumping cache[%u] %u/%ukb, %u finished", caches_size,
			   (unsigned) memory_usage / 1024,
			   (unsigned) get_avail_memory() / 1024,
			   (unsigned) c);

	for (p = caches; p != caches + caches_size; ++p) {
		dbg_printf(d, "%d: %u st %u res %d mem %lukb", p - caches,
				   (unsigned) p->selidx, p->status, p->result,
				   p->width * p->height * sizeof(pixel) / 1024L);
	}

	cache_unlock();
}

int caching(void)
{
	cache_image_t *p = NULL;
	cache_image_t tmp;
	t_fs_filetype ft;
	static bool dealarm = false;

	if (memory_usage > get_avail_memory()) {
		if (!dealarm)
			dbg_printf(d, "SERVER: %s: Memory usage %uKB, refuse to cache",
					   __func__, (unsigned) memory_usage / 1024);

		dealarm = true;

		return -1;
	}

	if (dealarm)
		dbg_printf(d, "SERVER: %s: Memory usage %uKB, OK to go now", __func__,
				   (unsigned) memory_usage / 1024);

	dealarm = false;

	if (cache_lock() < 0) {
		return -1;
	}

	for (p = caches; p != caches + caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == caches + caches_size || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	memcpy(&tmp, p, sizeof(tmp));
	cache_unlock();

	ft = fs_file_get_type(tmp.filename);
#if 1
	if (tmp.status == CACHE_INIT && (tmp.result == 4 || tmp.result == 5)) {
		if (memory_usage + (sizeof(pixel) * tmp.width * tmp.height) >
			get_avail_memory()) {
//          dbg_printf(d, "SERVER: Cannot enter now because of out of memory ");

			return -1;
		}
	}

	dbg_switch(d, 0);
	int fid = freq_enter_hotzone();

	tmp.result =
		image_open_archive(tmp.filename, tmp.archname, ft, &tmp.width,
						   &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	freq_leave(fid);
	dbg_switch(d, 1);

	if (tmp.result == 0) {
//      dbg_printf(d, "SERVER: Image %u finished loading", (unsigned)tmp.selidx);
		memory_usage += tmp.width * tmp.height * sizeof(pixel);
//      dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) memory_usage / 1024);
		tmp.status = CACHE_OK;
	} else if (tmp.result == 4 || tmp.result == 5) {
		// out of memory

		// is memory completely out of memory?
		if (memory_usage == 0) {
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), giving up", (unsigned)tmp.selidx, tmp.result);
			tmp.status = CACHE_FAILED;
		} else {
			// retry later
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), retring", (unsigned)tmp.selidx, tmp.result);
//          dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) memory_usage / 1024);
		}

		if (tmp.data != NULL) {
			free(tmp.data);
			tmp.data = NULL;
		}
	} else {
//      dbg_printf(d, "SERVER: Image %u finished failed(%u)", (unsigned)tmp.selidx, tmp.result);
		tmp.status = CACHE_FAILED;
	}

#else
	tmp.status = CACHE_FAILED;
#endif

	if (cache_lock() < 0) {
		return -1;
	}

	for (p = caches; p != caches + caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == caches + caches_size || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	memcpy(p, &tmp, sizeof(tmp));
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

static int start_cache(void)
{
	int re;
	dword pos;
	dword size;

	if (cache_lock() < 0) {
		return -1;
	}

	if (first_run) {
		first_run = false;
	} else {
		xrKernelWaitEventFlag(cache_del_event, CACHE_EVENT_DELETED,
							   PSP_EVENT_WAITAND, NULL, NULL);
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_UNDELETED);
	}

	pos = selidx;
	size = caches_size;

	while (size-- > 0) {
		pos = cache_get_next_image(pos, cache_forward);
	}

	re = min(MAX_CACHE_IMAGE, count_img()) - caches_size;
	dbg_printf(d, "SERVER: start pos %u selidx %u caches_size %u re %u",
			   (unsigned) pos, (unsigned) selidx, (unsigned) caches_size,
			   (unsigned) re);
	cache_unlock();

	if (re == 0) {
		return 0;
	}
	// dbg_printf(d, "SERVER: Wait for new selidx: memory usage %uKB", (unsigned) memory_usage / 1024);
	// dbg_printf(d, "SERVER: %d images to cache, selidx %u, caches_size %u", re, (unsigned)selidx, (unsigned)caches_size);

	while (re-- > 0) {
		dbg_printf(d, "SERVER: add cache image %u", (unsigned) pos);
		cache_add_by_path(config.shortpath, filelist[pos].compname->ptr, where,
						  pos);
		pos = cache_get_next_image(pos, cache_forward);
	}

	return re;
}

static int thread_cacher(SceSize args, void *argp)
{
	while (!cacher_should_exit) {
		if (cache_switch) {
			cacher_cleared = false;

			while (selidx_moved == false && !cacher_should_exit && cache_switch) {
				// select a image cache and start caching
				caching();
				xrKernelDelayThread(1000000);
			}

			if (cacher_should_exit)
				break;

			if (!cache_switch) {
				continue;
			}

			start_cache();
			selidx_moved = false;
		} else {
			// clean up all remain cache now
			cache_clear();
			cacher_cleared = true;
		}

		xrKernelDelayThread(100000);
	}

	cacher_exited = true;
	xrKernelExitDeleteThread(0);

	return 0;
}

int cache_init(void)
{
	int ret;

	dbg_printf(d, "set max memory usage to %ukb",
			   (unsigned) get_avail_memory() / 1024);

	cache_lock_uid = xrKernelCreateSema("Cache Mutex", 0, 1, 1, 0);
	thid =
		xrKernelCreateThread("thread_cacher", thread_cacher, 90, 0x10000, 0,
							  NULL);

	if (thid < 0) {
		dbg_printf(d, "create thread failed: 0x%08x", (unsigned) thid);

		return -1;
	}

	cacher_cleared = true;
	cacher_should_exit = false;
	cacher_exited = false;

	caches_size = 0;
	caches_cap = MAX_CACHE_IMAGE;
	caches = calloc(caches_cap, sizeof(caches[0]));

	cache_del_event = xrKernelCreateEventFlag("cache_del_event", 0, 0, 0);

	if ((ret = xrKernelStartThread(thid, 0, NULL)) < 0) {
		dbg_printf(d, "start thread failed: 0x%08x", (unsigned) ret);

		return -1;
	}

	return 0;
}

void cache_free(void)
{
	cache_on(false);

	while (!cacher_cleared) {
		xrKernelDelayThread(100000);
	}

	if (thid >= 0) {
		cacher_should_exit = true;

		while (!cacher_exited) {
			xrKernelDelayThread(100000);
		}

		thid = -1;
	}

	if (cache_lock_uid >= 0) {
		xrKernelDeleteSema(cache_lock_uid);
		cache_lock_uid = -1;
	}

	if (cache_del_event >= 0) {
		xrKernelDeleteEventFlag(cache_del_event);
		cache_del_event = -1;
	}

	if (caches != NULL) {
		free(caches);
		caches = NULL;
	}

	caches_size = caches_cap = 0;
}

/**
 * 得到缓存信息
 */
cache_image_t *cache_get(const char *archname, const char *filename)
{
	cache_image_t *p;

	if (caches == NULL || caches_size == 0 || filename == NULL) {
		return NULL;
	}

	for (p = caches; p != caches + caches_size; ++p) {
		if (((archname == p->archname && archname == NULL)
			 || !strcmp(p->archname, archname))
			&& !strcmp(p->filename, filename)) {
			return p;
		}
	}

	return NULL;
}

/*
extern unsigned int get_free_mem(void);

static inline bool test_free_memory(void)
{
	// Remember malloc/free is not thread-safe for now
	return get_free_mem() >= MIN_FREE_MEMORY ? true : false;
}
*/

int image_queue_test(void)
{
	int fid;
	static int i = 0;

	dbg_printf(d, "Start image queue test");
	cache_init();

	fid = freq_enter_hotzone();

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	where = scene_in_rar;

#if 1
	if (i == 0)
		SPRINTF_S(config.shortpath, "ms0:/PIC.rar");
	else
		SPRINTF_S(config.shortpath, "ms0:/PIC%d.rar", i + 1);

	++i;
#else
	SPRINTF_S(config.shortpath, "ms0:/PIC%d.rar", 3);
#endif
	filecount = fs_rar_to_menu(config.shortpath, &filelist, 0, 0, 0, 0);

	freq_leave(fid);

	if (filelist == NULL) {
		dbg_printf(d, "Cannot open %s", config.shortpath);
		cache_free();

		return -1;
	}

	test_cache();

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	cache_free();

	return 0;
}
