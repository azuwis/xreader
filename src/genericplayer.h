#ifndef GENERICPLAY_H
#define GENERICPLAY_H

#include "musicinfo.h"
#include "buffered_reader.h"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct reader_data_t
	{
		buffered_reader_t *r;
		int fd;
		bool use_buffer;
	} reader_data;
	
/**
 * ����ǰ����״̬
 */
	extern int g_suspend_status;

/**
 * ��ǰ����ʱ�䣬��������
 */
	extern double g_play_time;

/**
 * Wave��������ʱ����ʱ��
 */
	extern double g_suspend_playing_time;

/**
 * ���ֿ����������
 */
	extern int g_seek_seconds;

/**
 * ��ǰ��������״̬
 */
	extern int g_status;

/**
 * �ϴΰ�����˼�����
 */
	extern bool g_last_seek_is_forward;

/**
 * �ϴΰ�����˼�ʱ��
 */
	extern volatile u64 g_last_seek_tick;

/**
 *  ������˼�����
 */
	extern volatile dword g_seek_count;

/**
 * ��ǰ���������ļ���Ϣ
 */
	extern MusicInfo g_info;

/**
 * ʹ�û���IO
 */
	extern bool g_use_buffer;

/**
 * Ĭ�ϻ���IO�����ֽڴ�С����Ͳ�С��8192
 */
	extern int g_io_buffer_size;

	extern reader_data data;

	int generic_lock(void);
	int generic_unlock(void);
	int generic_set_opt(const char *unused, const char *values);
	int generic_play(void);
	int generic_pause(void);
	int generic_get_status(void);
	int generic_fforward(int sec);
	int generic_fbackward(int sec);
	int generic_end(void);
	int generic_init(void);
	int generic_resume(const char *spath, const char *lpath);
	int generic_suspend(void);
	void generic_set_playback(bool playing);
	int generic_get_info(struct music_info *info);

#ifdef __cplusplus
}
#endif

#endif
