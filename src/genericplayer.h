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
 * 休眠前播放状态
 */
	extern int g_suspend_status;

/**
 * 当前播放时间，以秒数计
 */
	extern double g_play_time;

/**
 * Wave音乐休眠时播放时间
 */
	extern double g_suspend_playing_time;

/**
 * 音乐快进、退秒数
 */
	extern int g_seek_seconds;

/**
 * 当前驱动播放状态
 */
	extern int g_status;

/**
 * 上次按快进退键类型
 */
	extern bool g_last_seek_is_forward;

/**
 * 上次按快进退键时间
 */
	extern volatile u64 g_last_seek_tick;

/**
 *  按快进退键计数
 */
	extern volatile dword g_seek_count;

/**
 * 当前播放音乐文件信息
 */
	extern MusicInfo g_info;

/**
 * 使用缓冲IO
 */
	extern bool g_use_buffer;

/**
 * 默认缓冲IO缓冲字节大小，最低不小于8192
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
