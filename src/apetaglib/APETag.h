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
