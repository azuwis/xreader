#ifndef FREQ_LOCK_H
#define FREQ_LOCK_H

int freq_init();
int freq_free();
int freq_update(void);
int freq_enter(int cpu, int bus);

/**
 * �뿪Ƶ������
 *
 * @param freq��ID
 *
 * @return 0
 */
int freq_leave(int freq_id);

/**
 * �����Ƶ������
 *
 * @note ������ڲ�������,���������Ƶ��,���򽫽�����ͨ����Ƶ��
 *
 * @return freq��ID
 */
int freq_enter_hotzone(void);

int dbg_freq();

#endif
