#ifndef THREAD_LOCK_H
#define THREAD_LOCK_H

struct psp_mutex_t {
	volatile unsigned long l;
	int c;
	int thread_id;
};

int xr_lock(struct psp_mutex_t *e);
int xr_unlock(struct psp_mutex_t *e);
struct psp_mutex_t* xr_lock_init(struct psp_mutex_t *e);
void xr_lock_destroy(struct psp_mutex_t *e);

#endif
