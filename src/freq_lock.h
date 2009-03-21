#ifndef FREQ_LOCK_H
#define FREQ_LOCK_H

int freq_init();
int freq_free();
int freq_update(void);
int freq_enter(int cpu, int bus);

/**
 * 离开频率热区
 *
 * @param freq区ID
 *
 * @return 0
 */
int freq_leave(int freq_id);

/**
 * 进入高频率热区
 *
 * @note 如果正在播放音乐,将进入最高频率,否则将进入普通工作频率
 *
 * @return freq区ID
 */
int freq_enter_hotzone(void);

int dbg_freq();

#endif
