#pragma once

#include <stdint.h>

struct _APETagHeader
{
	uint64_t preamble;
	uint32_t version;
	uint32_t tag_size;
	uint32_t item_count;
	uint32_t flags;
	uint64_t reserved;
} __attribute__ ((packed));

typedef struct _APETagHeader APETagHeader;
