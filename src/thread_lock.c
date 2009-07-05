#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include "xrhal.h"
#include "thread_lock.h"

#define CURRENT_THREAD xrKernelGetThreadId()

typedef unsigned long dword;

static dword my_InterlockedExchange(dword volatile *dst, dword exchange)
{
	unsigned int flags = pspSdkDisableInterrupts();
	dword origvalue = *dst;

	*dst = exchange;

	pspSdkEnableInterrupts(flags);
	return origvalue;
}

int xr_lock(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return -1;
	}

	for (;;) {
		if (s->l != 0) {
			if (s->thread_id == CURRENT_THREAD) {
				++s->c;
				return 0;
			}
		} else {
			if (!my_InterlockedExchange(&s->l, 1)) {
				s->thread_id = CURRENT_THREAD;
				s->c = 1;
				return 0;
			}
		}

		xrKernelDelayThread(1000);
	}

	return -1;
}

int xr_unlock(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return -1;
	}

	if (--s->c == 0) {
		s->thread_id = 0;
		my_InterlockedExchange(&s->l, 0);
	}

	return 0;
}

struct psp_mutex_t* xr_lock_init(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return s;
	}
	
	s->l = 0;
	s->c = 0;
	s->thread_id = CURRENT_THREAD;

	return s;
}

void xr_lock_destroy(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return;
	}
	
	s->l = 0;
	s->c = 0;
	s->thread_id = 0;
}
