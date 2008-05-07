/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _CTRL_H_
#define _CTRL_H_

#include <pspctrl.h>
#ifdef ENABLE_HPRM
#include <psphprm.h>
#endif
#include "common/datatype.h"

#define CTRL_FORWARD 0x10000000
#define CTRL_BACK 0x20000000
#define CTRL_PLAYPAUSE 0x40000000
#ifdef ENABLE_ANALOG
#define CTRL_ANALOG 0x80000000
#endif

extern void ctrl_init();
extern void ctrl_destroy();

#ifdef ENABLE_ANALOG
extern void ctrl_analog(int *x, int *y);
#endif
extern dword ctrl_read_cont();
extern dword ctrl_read();
extern void ctrl_waitreleaseintime(int i);
extern void ctrl_waitrelease();
extern dword ctrl_waitany();
extern dword ctrl_waitkey(dword keymask);
extern dword ctrl_waitmask(dword keymask);
extern dword ctrl_waitlyric();
extern int ctrl_waitreleasekey(dword key);

#ifdef ENABLE_HPRM
extern dword ctrl_hprm();
extern dword ctrl_hprm_raw();
#endif
extern dword ctrl_waittime(dword t);

#ifdef ENABLE_HPRM
extern void ctrl_enablehprm(bool enable);
#endif

#endif
