#ifndef IMAGE_QUEUE_H
#define IMAGE_QUEUE_H

typedef struct _cache_image_t
{
	const char *archname;
	const char *filename;
	int where;
	int status;
	pixel *data;
	pixel bgc;
	dword width;
	dword height;
	int result;
	dword selidx;
} cache_image_t;

int cache_init(void);
void cache_free(void);
void dbg_dump_cache(void);
int cache_get_size();
void cache_set_forward(bool forward);
void cache_next_image(void);
void cache_on(bool on);
int cache_wait_avail();
int cache_delete_first(void);

int image_queue_test(void);

#endif
