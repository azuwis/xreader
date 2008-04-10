#ifdef PSP
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pspsdk.h>
#include "config.h"
#include "charsets.h"
#include "common/qsort.h"
#include "common/utils.h"
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "APETag.h"
#include "dbg.h"
#include "strsafe.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

APETagItems *new_APETagItems(void)
{
	APETagItems *p = (APETagItems *) malloc(sizeof(APETagItems));

	p->item_count = 0;
	p->items = NULL;

	return p;
}

int free_APETagItems(APETagItems * items)
{
	if (items == NULL) {
		return -1;
	}

	int i;

	for (i = 0; i < items->item_count; ++i) {
		free(items->items[i]);
		items->items[i] = NULL;
	}
	items->item_count = 0;
	free(items->items);
	items->items = NULL;
	free(items);

	return 0;
}

int append_APETAGItems(APETagItems * items, void *ptr, int size)
{
	if (items == NULL || ptr == NULL || size == 0) {
		apetag_errno = APETAG_BAD_ARGUMENT;
		return -1;
	}

	if (items->item_count == 0) {
		items->item_count++;
		items->items = (APETagItem **) malloc(sizeof(APETagItem *) * 1);
		if (items->items == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		items->items[0] = (APETagItem *) malloc(size);
		if (items->items[0] == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		memcpy(items->items[0], ptr, size);
	} else {
		items->item_count++;
		APETagItem **t = (APETagItem **) realloc(items->items,
												 sizeof(APETagItem *) *
												 items->item_count);
		if (t != NULL)
			items->items = t;
		else {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		items->items[items->item_count - 1] = (APETagItem *) malloc(size);
		if (items->items[items->item_count - 1] == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		memcpy(items->items[items->item_count - 1], ptr, size);
	}

	apetag_errno = APETAG_OK;

	return 0;
}

APETagItem *find_APETagItems(const APETagItems * items, const char *searchstr)
{
	if (items == NULL)
		return NULL;

	int i;

	for (i = 0; i < items->item_count; ++i) {
		if (strcmp(APE_ITEM_GET_KEY(items->items[i]), searchstr) == 0) {
			return items->items[i];
		}
	}

	return NULL;
}

/*
 * Item## : <UTF8/Binary Type> Key: xxx Value: yyy, flags: [hHfF]
 */
void print_item_type(const APETagItem * item)
{
	if (item == NULL)
		return;

	char flagstr[20];

	SPRINTF_S(flagstr, "%s%s%s%s%s%s", APE_ITEM_HAS_HEADER(item) ? "h" : "",
			  APE_ITEM_IS_HEADER(item) ? "H" : "",
			  APE_ITEM_HAS_FOOTER(item) ? "f" : "",
			  APE_ITEM_IS_FOOTER(item) ? "F" : "",
			  APE_ITEM_IS_EXT(item) ? "X" : "",
			  APE_ITEM_IS_READONLY(item) ? "R" : "");

	if (APE_ITEM_IS_UTF8(item)) {
		char *t =
			strndup(APE_ITEM_GET_VALUE(item), APE_ITEM_GET_VALUE_LEN(item));
		if (t) {
			dbg_printf(d,
					   "%s, Key: \"%s\", Value: \"%s\", Flags: %s, Size: %u bytes",
					   "UTF-8 String", APE_ITEM_GET_KEY(item), t, flagstr,
					   (size_t) APE_ITEM_GET_VALUE_LEN(item));
			free(t);
		}
	} else if (APE_ITEM_IS_BINARY(item)) {
		dbg_printf(d, "%s, Key: \"%s\", Flags: %s, Size: %u bytes",
				   "Binary Type", APE_ITEM_GET_KEY(item), flagstr,
				   (size_t) APE_ITEM_GET_VALUE_LEN(item));
		dbg_hexdump_ascii(d, (const byte *) APE_ITEM_GET_KEY(item),
						  (size_t) APE_ITEM_GET_KEY_LEN(item));
	}
}
