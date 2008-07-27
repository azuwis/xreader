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

extern void ctrl_init(void);
extern void ctrl_destroy(void);

#ifdef ENABLE_ANALOG
extern void ctrl_analog(int *x, int *y);
#endif
extern dword ctrl_read_cont(void);
extern dword ctrl_read(void);
extern void ctrl_waitreleaseintime(int i);
extern void ctrl_waitrelease(void);
extern dword ctrl_waitany(void);
extern dword ctrl_waitkey(dword keymask);
extern dword ctrl_waitmask(dword keymask);
extern dword ctrl_waitlyric(void);
extern int ctrl_waitreleasekey(dword key);

#ifdef ENABLE_HPRM
extern dword ctrl_hprm(void);
extern dword ctrl_hprm_raw(void);
#endif
extern dword ctrl_waittime(dword t);

#ifdef ENABLE_HPRM
extern void ctrl_enablehprm(bool enable);
#endif

#endif
