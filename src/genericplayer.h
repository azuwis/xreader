#ifndef GENERICPLAY_H
#define GENERICPLAY_H

#ifdef __cplusplus
extern "C"
{
#endif
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
 * 当前驱动播放状态写锁
 */
	extern SceUID g_status_sema;

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

#ifdef __cplusplus
}
#endif

#endif
