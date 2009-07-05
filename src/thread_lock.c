#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include "xrhal.h"
#include "thread_lock.h"

int xr_lock(struct lock_entry *e)
{
	if (e == NULL || e->semaid < 0) {
		return -1;
	}

	for(;;) {
		SceKernelSemaInfo info;

		memset(&info, 0, sizeof(info));
		if (sceKernelReferSemaStatus(e->semaid, &info) < 0) {
			return -1;
		}

		if (info.currentCount != 0) {
			if (e->thread_id == xrKernelGetThreadId()) {
				++e->cnt;
				return 0;
			}
		} else {
			if (xrKernelWaitSema(e->semaid, 1, NULL) == 0) {
				e->thread_id = xrKernelGetThreadId();
				e->cnt = 1;
				return 0;
			}
		}
	}

	return -1;
}

int xr_unlock(struct lock_entry *e)
{
	if (e == NULL || e->semaid < 0) {
		return -1;
	}

	if (--e->cnt == 0) {
		e->thread_id = 0;
		xrKernelWaitSema(e->semaid, 1, NULL);
	}

	return -1;
}

struct lock_entry* xr_lock_init(struct lock_entry *e, const char* lockname)
{
	if (e == NULL) {
		return e;
	}
	
	e->cnt = 0;
	e->thread_id = xrKernelGetThreadId();
	e->semaid = xrKernelCreateSema(lockname, 0, 1, 1, NULL);

	return e->semaid < 0 ? NULL : e;
}

void xr_lock_destroy(struct lock_entry *e)
{
	if (e == NULL) {
		return;
	}
	
	if (e->semaid >= 0) {
		xrKernelDeleteSema(e->semaid);
		e->semaid = -1;
	}
}
