#ifndef _AVC_H
#define _AVC_H

#include "common/datatype.h"

extern bool avc_init(void);
extern void avc_free(void);
extern void avc_start(void);
extern void avc_end(void);
extern char *pmp_play(char *s, int usePos);

#endif
