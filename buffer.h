/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdlib.h>
#include <sys/types.h>

#define BUFFER_MAX_REUSE_SIZE  (4 * 1024)

typedef struct
{
	char *ptr;

	size_t used;
	size_t size;
} buffer;

typedef struct
{
	buffer **ptr;

	size_t used;
	size_t size;
} buffer_array;

typedef struct
{
	char *ptr;

	size_t offset;				/* input-pointer */

	size_t used;				/* output-pointer */
	size_t size;
} read_buffer;

buffer_array *buffer_array_init(void);
void buffer_array_free(buffer_array * b);
void buffer_array_reset(buffer_array * b);
buffer *buffer_array_append_get_buffer(buffer_array * b);

buffer *buffer_init(void);
buffer *buffer_init_buffer(buffer * b);
buffer *buffer_init_string(const char *str);
void buffer_free(buffer * b);
char *buffer_free_weak(buffer * b);
void buffer_reset(buffer * b);

int buffer_prepare_copy(buffer * b, size_t size);
int buffer_prepare_append(buffer * b, size_t size);

int buffer_copy_string(buffer * b, const char *s);
int buffer_copy_string_len(buffer * b, const char *s, size_t s_len);
int buffer_copy_string_buffer(buffer * b, const buffer * src);

int buffer_copy_memory(buffer * b, const char *s, size_t s_len);

int buffer_append_string(buffer * b, const char *s);
int buffer_append_string_len(buffer * b, const char *s, size_t s_len);
int buffer_append_string_buffer(buffer * b, const buffer * src);

int buffer_append_memory(buffer * b, const char *s, size_t s_len);

char *buffer_search_string_len(buffer * b, const char *needle, size_t len);

int buffer_is_empty(buffer * b);
int buffer_is_equal(buffer * a, buffer * b);
int buffer_is_equal_right_len(buffer * a, buffer * b, size_t len);
int buffer_is_equal_string(buffer * a, const char *s, size_t b_len);
int buffer_caseless_compare(const char *a, size_t a_len, const char *b,
							size_t b_len);

int buffer_to_lower(buffer * b);
int buffer_to_upper(buffer * b);

#endif
