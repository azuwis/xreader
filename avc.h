/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _AVC_H
#define _AVC_H

#include "common/datatype.h"

extern bool avc_init();
extern void avc_free();
extern void avc_start();
extern void avc_end();
extern char *pmp_play(char *s, int usePos);

#endif
