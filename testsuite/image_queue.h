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
void cache_set_fierce(bool fierce);
void cache_free(void);
void dbg_dump_cache(void);

int image_queue_test(void);
cache_image_t *cache_get(const char *archname, const char *filename);
void cache_on(bool on);

#endif
