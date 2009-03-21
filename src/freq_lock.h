#ifndef FREQ_LOCK_H
#define FREQ_LOCK_H

int freq_init();
int freq_free();
int freq_update(void);
int freq_enter(int cpu, int bus);
int freq_leave(int freq_id);
int freq_enter_level(unsigned short level);
int freq_leave_level(void);

#endif
