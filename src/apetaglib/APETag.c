#ifdef PSP
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pspsdk.h>
#include "common/qsort.h"
#include "common/utils.h"
#endif
#include <stdio.h>
#include <string.h>
#include "APETag.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

int apetag_errno = 0;

static long has_id3(FILE * fp)
{
	long p;
	char bytes[3];

	fseek(fp, -128, SEEK_END);
	p = ftell(fp);

	if (fread(bytes, 3, 1, fp) == 1) {
		if (memcmp(bytes, "TAG", 3) == 0) {
			dbg_printf(d, "发现ID3v1 标记，位置 %ld", p);
			return p;
		}
	}

	return -1;
}

static int ReadAPETag(FILE * fp, APETag * tag)
{
	if (fread(&tag->footer, sizeof(APETagHeader), 1, fp) == 1) {
		if (memcmp(&tag->footer.preamble, "APETAGEX", 8) == 0) {
			/*
			   if (tag->footer.version != 2000) {
			   apetag_errno = APETAG_UNSUPPORT_TAGVERSION;
			   return -1;
			   }
			 */

			tag->item_count = tag->footer.item_count;
			tag->items = new_APETagItems();

			if (tag->items == NULL) {
				apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
				return -1;
			}

			fseek(fp, -tag->footer.tag_size, SEEK_CUR);
			dbg_printf(d, "转到MP3位置 %ld", ftell(fp));

			char *raw_tag = (char *) malloc(tag->footer.tag_size);

			if (raw_tag == NULL) {
				apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
				return -1;
			}

			if (fread(raw_tag, tag->footer.tag_size, 1, fp) != 1) {
				apetag_errno = APETAG_ERROR_FILEFORMAT;
				return -1;
			}

			int i;
			const char *p = raw_tag;

			for (i = 0; i < tag->footer.item_count; ++i) {
				int size = APE_ITEM_GET_VALUE_LEN((APETagItem *) p);
				int m = APE_ITEM_GET_KEY_LEN((APETagItem *) p);
				char str[80];

				sprintf(str, "项目 %%d 大小 %%d %%.%ds: %%.%ds", m, size);
//              dbg_printf(d, str, i+1, size, p+8, p + 8 + 1 + m);
				append_APETAGItems(tag->items, (void *) p, size + m + 1 + 8);
				p += size + m + 1 + 8;
			}

			free(raw_tag);

			dbg_printf(d,
					   "发现APE标记: 版本(%d) 项目个数: %d 项目+尾字节大小: %u",
					   (int) tag->footer.version, tag->item_count,
					   (size_t) tag->footer.tag_size);
			for (i = 0; i < tag->items->item_count; ++i) {
				print_item_type(tag->items->items[i]);
			}
			apetag_errno = APETAG_OK;
			tag->is_header_or_footer = APE_FOOTER;

			return 0;
		}
	}

	return -2;
}

static int searchForAPETag(FILE * fp, APETag * tag)
{
	apetag_errno = APETAG_NOT_IMPLEMENTED;

	// 首先搜索文件尾处，ID3V1标签之前，是否有APE标签
	if (has_id3(fp) != -1) {
		fseek(fp, -160, SEEK_END);
		dbg_printf(d, "转到MP3位置 %ld", ftell(fp));
	} else {
		fseek(fp, -32, SEEK_END);
		dbg_printf(d, "转到MP3位置 %ld", ftell(fp));
	}

	int ret;

	if ((ret = ReadAPETag(fp, tag)) == 0)
		return 0;
	// 如果没有找到，搜索文件头是否有APE标签
	if (ret == -2) {
		fseek(fp, 0, SEEK_SET);
		if (ReadAPETag(fp, tag) == 0) {
			tag->is_header_or_footer = APE_HEADER;
			return 0;
		}
	}

	return -1;
}

APETag *loadAPETag(const char *filename)
{
	if (filename == NULL)
		return 0;

	FILE *fp = fopen(filename, "rb");

	if (fp == NULL) {
		apetag_errno = APETAG_ERROR_OPEN;
		return 0;
	}

	APETag *p = (APETag *) malloc(sizeof(APETag));

	if (p == NULL) {
		apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
		fclose(fp);
		return 0;
	}

	if (searchForAPETag(fp, p) == -1) {
		apetag_errno = APETAG_ERROR_FILEFORMAT;
		fclose(fp);
		return 0;
	}

	apetag_errno = APETAG_OK;
	fclose(fp);

	return p;
}

int freeAPETag(APETag * tag)
{
	if (tag == NULL) {
		apetag_errno = APETAG_BAD_ARGUMENT;
		return 0;
	}

	free_APETagItems(tag->items);
	free(tag);

	return 0;
}

char *APETag_SimpleGet(APETag * tag, const char *key)
{
	if (tag == NULL || key == NULL) {
		return NULL;
	}

	APETagItem *pItem = find_APETagItems(tag->items, key);

	if (pItem == NULL || !APE_ITEM_IS_UTF8(pItem)) {
		return NULL;
	}

	char *t = strndup(APE_ITEM_GET_VALUE(pItem), APE_ITEM_GET_VALUE_LEN(pItem));

	return t;
}
