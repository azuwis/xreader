/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <ctype.h>

#include "common/utils.h"
#include "buffer.h"

static const char hex_chars[] = "0123456789abcdef";

/**
 * init the buffer
 *
 */

buffer *buffer_init(void)
{
	buffer *b;

	b = malloc(sizeof(*b));

	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}

buffer *buffer_init_buffer(buffer * src)
{
	buffer *b = buffer_init();

	buffer_copy_string_buffer(b, src);
	return b;
}

/**
 * free the buffer
 *
 */

void buffer_free(buffer * b)
{
	if (!b)
		return;

	free(b->ptr);
	free(b);
}

char *buffer_free_weak(buffer * b)
{
	if (!b)
		return NULL;

	char *ptr = b->ptr;

	free(b);

	return ptr;
}

void buffer_reset(buffer * b)
{
	if (!b)
		return;

	/* limit don't reuse buffer larger than ... bytes */
	if (b->size > BUFFER_MAX_REUSE_SIZE) {
		free(b->ptr);
		b->ptr = NULL;
		b->size = 0;
	}

	b->used = 0;
}

/**
 *
 * allocate (if neccessary) enough space for 'size' bytes and
 * set the 'used' counter to 0
 *
 */

#define BUFFER_PIECE_SIZE 64

int buffer_prepare_copy(buffer * b, size_t size)
{
	if (!b)
		return -1;

	if ((0 == b->size) || (size > b->size)) {
		if (b->size)
			free(b->ptr);

		b->size = size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
	}
	b->used = 0;
	return 0;
}

/**
 *
 * increase the internal buffer (if neccessary) to append another 'size' byte
 * ->used isn't changed
 *
 */

int buffer_prepare_append(buffer * b, size_t size)
{
	if (!b)
		return -1;

	if (0 == b->size) {
		b->size = size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		b->used = 0;
	} else if (b->used + size > b->size) {
		b->size += size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		char *ptr = realloc_free_when_fail(b->ptr, b->size);

		if (ptr == NULL)
			return -1;
		b->ptr = ptr;
	}
	return 0;
}

int buffer_copy_string(buffer * b, const char *s)
{
	size_t s_len;

	if (!s || !b)
		return -1;

	s_len = strlen(s) + 1;
	buffer_prepare_copy(b, s_len);

	memcpy(b->ptr, s, s_len);
	b->used = s_len;

	return 0;
}

int buffer_copy_string_len(buffer * b, const char *s, size_t s_len)
{
	if (!s || !b)
		return -1;
#if 0
	/* removed optimization as we have to keep the empty string
	 * in some cases for the config handling
	 *
	 * url.access-deny = ( "" )
	 */
	if (s_len == 0)
		return 0;
#endif
	buffer_prepare_copy(b, s_len + 1);

	memcpy(b->ptr, s, s_len);
	b->ptr[s_len] = '\0';
	b->used = s_len + 1;

	return 0;
}

int buffer_copy_string_buffer(buffer * b, const buffer * src)
{
	if (!src)
		return -1;

	if (src->used == 0) {
		b->used = 0;
		return 0;
	}
	return buffer_copy_string_len(b, src->ptr, src->used - 1);
}

int buffer_append_string(buffer * b, const char *s)
{
	size_t s_len;

	if (!s || !b)
		return -1;

	s_len = strlen(s);
	buffer_prepare_append(b, s_len + 1);
	if (b->used == 0)
		b->used++;

	memcpy(b->ptr + b->used - 1, s, s_len + 1);
	b->used += s_len;

	return 0;
}

/**
 * append a string to the end of the buffer
 *
 * the resulting buffer is terminated with a '\0'
 * s is treated as a un-terminated string (a \0 is handled a normal character)
 *
 * @param b a buffer
 * @param s the string
 * @param s_len size of the string (without the terminating \0)
 */

int buffer_append_string_len(buffer * b, const char *s, size_t s_len)
{
	if (!s || !b)
		return -1;
	if (s_len == 0)
		return 0;

	buffer_prepare_append(b, s_len + 1);
	if (b->used == 0)
		b->used++;

	memcpy(b->ptr + b->used - 1, s, s_len);
	b->used += s_len;
	b->ptr[b->used - 1] = '\0';

	return 0;
}

int buffer_append_string_buffer(buffer * b, const buffer * src)
{
	if (!src)
		return -1;
	if (src->used == 0)
		return 0;

	return buffer_append_string_len(b, src->ptr, src->used - 1);
}

int buffer_append_memory(buffer * b, const char *s, size_t s_len)
{
	if (!s || !b)
		return -1;
	if (s_len == 0)
		return 0;

	if (buffer_prepare_append(b, s_len) == 0) {
		memcpy(b->ptr + b->used, s, s_len);
		b->used += s_len;
	} else {
		return -1;
	}

	return 0;
}

int buffer_copy_memory(buffer * b, const char *s, size_t s_len)
{
	if (!s || !b)
		return -1;

	b->used = 0;

	return buffer_append_memory(b, s, s_len);
}

/**
 * init the buffer
 *
 */

buffer_array *buffer_array_init(void)
{
	buffer_array *b;

	b = malloc(sizeof(*b));

	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}

void buffer_array_reset(buffer_array * b)
{
	size_t i;

	if (!b)
		return;

	/* if they are too large, reduce them */
	for (i = 0; i < b->used; i++) {
		buffer_reset(b->ptr[i]);
	}

	b->used = 0;
}

/**
 * free the buffer_array
 *
 */

void buffer_array_free(buffer_array * b)
{
	size_t i;

	if (!b)
		return;

	for (i = 0; i < b->size; i++) {
		if (b->ptr[i])
			buffer_free(b->ptr[i]);
	}
	free(b->ptr);
	free(b);
}

buffer *buffer_array_append_get_buffer(buffer_array * b)
{
	size_t i;

	if (b->size == 0) {
		b->size = 16;
		b->ptr = malloc(sizeof(*b->ptr) * b->size);
		for (i = 0; i < b->size; i++) {
			b->ptr[i] = NULL;
		}
	} else if (b->size == b->used) {
		b->size += 16;
		b->ptr = realloc_free_when_fail(b->ptr, sizeof(*b->ptr) * b->size);
		for (i = b->used; i < b->size; i++) {
			b->ptr[i] = NULL;
		}
	}

	if (b->ptr[b->used] == NULL) {
		b->ptr[b->used] = buffer_init();
	}

	b->ptr[b->used]->used = 0;

	return b->ptr[b->used++];
}

char *buffer_search_string_len(buffer * b, const char *needle, size_t len)
{
	size_t i;

	if (len == 0)
		return NULL;
	if (needle == NULL)
		return NULL;

	if (b->used < len)
		return NULL;

	for (i = 0; i < b->used - len; i++) {
		if (0 == memcmp(b->ptr + i, needle, len)) {
			return b->ptr + i;
		}
	}

	return NULL;
}

buffer *buffer_init_string(const char *str)
{
	buffer *b = buffer_init();

	buffer_copy_string(b, str);

	return b;
}

int buffer_is_empty(buffer * b)
{
	if (!b)
		return 1;
	return (b->used == 0);
}

/**
 * check if two buffer contain the same data
 *
 * HISTORY: this function was pretty much optimized, but didn't handled
 * alignment properly.
 */

int buffer_is_equal(buffer * a, buffer * b)
{
	if (a->used != b->used)
		return 0;
	if (a->used == 0)
		return 1;

	return (0 == strcmp(a->ptr, b->ptr));
}

int buffer_is_equal_string(buffer * a, const char *s, size_t b_len)
{
	buffer b;

	b.ptr = (char *) s;
	b.used = b_len + 1;

	return buffer_is_equal(a, &b);
}

/* simple-assumption:
 *
 * most parts are equal and doing a case conversion needs time
 *
 */
int buffer_caseless_compare(const char *a, size_t a_len, const char *b,
							size_t b_len)
{
	size_t ndx = 0, max_ndx;
	size_t *al, *bl;
	size_t mask = sizeof(*al) - 1;

	al = (size_t *) a;
	bl = (size_t *) b;

	/* is the alignment correct ? */
	if (((size_t) al & mask) == 0 && ((size_t) bl & mask) == 0) {

		max_ndx = ((a_len < b_len) ? a_len : b_len) & ~mask;

		for (; ndx < max_ndx; ndx += sizeof(*al)) {
			if (*al != *bl)
				break;
			al++;
			bl++;

		}

	}

	a = (char *) al;
	b = (char *) bl;

	max_ndx = ((a_len < b_len) ? a_len : b_len);

	for (; ndx < max_ndx; ndx++) {
		char a1 = *a++, b1 = *b++;

		if (a1 != b1) {
			if ((a1 >= 'A' && a1 <= 'Z') && (b1 >= 'a' && b1 <= 'z'))
				a1 |= 32;
			else if ((a1 >= 'a' && a1 <= 'z') && (b1 >= 'A' && b1 <= 'Z'))
				b1 |= 32;
			if ((a1 - b1) != 0)
				return (a1 - b1);

		}
	}

	/* all chars are the same, and the length match too
	 *
	 * they are the same */
	if (a_len == b_len)
		return 0;

	/* if a is shorter then b, then b is larger */
	return (a_len - b_len);
}

/**
 * check if the rightmost bytes of the string are equal.
 *
 *
 */

int buffer_is_equal_right_len(buffer * b1, buffer * b2, size_t len)
{
	/* no, len -> equal */
	if (len == 0)
		return 1;

	/* len > 0, but empty buffers -> not equal */
	if (b1->used == 0 || b2->used == 0)
		return 0;

	/* buffers too small -> not equal */
	if (b1->used - 1 < len || b1->used - 1 < len)
		return 0;

	if (0 == strncmp(b1->ptr + b1->used - 1 - len,
					 b2->ptr + b2->used - 1 - len, len)) {
		return 1;
	}

	return 0;
}

int buffer_copy_string_hex(buffer * b, const char *in, size_t in_len)
{
	size_t i;

	/* BO protection */
	if (in_len * 2 < in_len)
		return -1;

	buffer_prepare_copy(b, in_len * 2 + 1);

	for (i = 0; i < in_len; i++) {
		b->ptr[b->used++] = hex_chars[(in[i] >> 4) & 0x0F];
		b->ptr[b->used++] = hex_chars[in[i] & 0x0F];
	}
	b->ptr[b->used++] = '\0';

	return 0;
}

int buffer_to_lower(buffer * b)
{
	char *c;

	if (b->used == 0)
		return 0;

	for (c = b->ptr; *c; c++) {
		if (*c >= 'A' && *c <= 'Z') {
			*c |= 32;
		}
	}

	return 0;
}

int buffer_to_upper(buffer * b)
{
	char *c;

	if (b->used == 0)
		return 0;

	for (c = b->ptr; *c; c++) {
		if (*c >= 'a' && *c <= 'z') {
			*c &= ~32;
		}
	}

	return 0;
}
