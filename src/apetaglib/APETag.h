#pragma once

#include <stdint.h>
#include "TagHeader.h"
#include "TagItem.h"

enum
{
	APE_HEADER,
	APE_FOOTER
};

struct _APETag
{
	int is_header_or_footer;
	APETagHeader footer;
	int item_count;
	//// TODO 设计不足，items是大小可变的，不是数组
	APETagItems *items;
};

typedef struct _APETag APETag;

enum
{
	APETAG_OK = 0,
	APETAG_ERROR_OPEN,
	APETAG_ERROR_FILEFORMAT,
	APETAG_UNSUPPORT_TAGVERSION,
	APETAG_BAD_ARGUMENT,
	APETAG_NOT_IMPLEMENTED,
	APETAG_MEMORY_NOT_ENOUGH,
};

extern int apetag_errno;

APETag *loadAPETag(const char *filename);
int freeAPETag(APETag * tag);
const char *APETag_strerror(int errno);
char *APETag_SimpleGet(APETag * tag, const char *key);
