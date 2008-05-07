#pragma once

#include "hash.h"

typedef struct _TransItem TransItem;
typedef struct _TransItem *PTransItem;

struct _TransItem
{
	char *msgid;
	char *msgstr;
	PTransItem next;
};

int read_sofile(const char *path, struct hash_control **hash, int *size);
char *lookup_transitem(struct hash_control *p, const char *msgid);
