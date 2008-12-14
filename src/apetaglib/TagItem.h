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

struct _APETagItem
{
	uint32_t item_value;
	uint32_t item_flags;
} __attribute__ ((packed));

typedef struct _APETagItem APETagItem;

struct _APETagItems
{
	int item_count;
	APETagItem **items;
};

typedef struct _APETagItems APETagItems;

enum
{
	APE_ITEM_UTF8 = 0,
	APE_ITEM_BINARY = 1,
	APE_ITEM_EXT = 2
};

#define APE_ISBITSET(v,n) ((((char*)v)[n/8] &   (1<<(n%8))))
#define APE_ITEM_HAS_HEADER(item) APE_ISBITSET((&(item)->item_flags), (31))
#define APE_ITEM_HAS_FOOTER(item) APE_ISBITSET((&(item)->item_flags), (30))
#define APE_ITEM_IS_HEADER(item) APE_ISBITSET((&(item)->item_flags), (29))
#define APE_ITEM_IS_FOOTER(item) (!APE_ISBITSET((&(item)->item_flags), (29)))
#define APE_ITEM_IS_UTF8(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_UTF8)
#define APE_ITEM_IS_BINARY(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_BINARY)
#define APE_ITEM_IS_EXT(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_EXT)
#define APE_ITEM_IS_READONLY(item) APE_ISBITSET((&(item)->item_flags), (0))
#define APE_ITEM_GET_KEY(item) (((char*)(item))+8)
#define APE_ITEM_GET_KEY_LEN(item) (strlen(((char*)(item))+8))
#define APE_ITEM_GET_VALUE(item) (APE_ITEM_GET_KEY(item) + 1 + strlen(APE_ITEM_GET_KEY(item)))
#define APE_ITEM_GET_VALUE_LEN(item) ((item)->item_value)

APETagItems *new_APETagItems(void);
int free_APETagItems(APETagItems * items);
int append_APETAGItems(APETagItems * items, void *ptr, int size);
APETagItem *find_APETagItems(const APETagItems * items, const char *searchstr);
void print_item_type(const APETagItem * item);
char *ConvertGB2312toUTF8(const char *pData, size_t size);
