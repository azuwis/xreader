#pragma once

#define MD_GET_TITLE       (1 << 0)
#define MD_GET_ARTIST      (1 << 1)
#define MD_GET_COMMENT     (1 << 3)
#define MD_GET_CURTIME     (1 << 4)
#define MD_GET_DURATION    (1 << 5)
#define MD_GET_CPUFREQ     (1 << 6)
#define MD_GET_FREQ        (1 << 7)
#define MD_GET_DECODERNAME (1 << 8)
#define MD_GET_ENCODEMSG   (1 << 9)
#define MD_GET_AVGKBPS     (1 << 10)
#define MD_GET_INSKBPS     (1 << 11)
#define MD_GET_FILEFD      (1 << 12)
#define MD_GET_SNDCHL      (1 << 13)
#define MD_GET_CHANNELS    (1 << 14)

#define INFO_STR_SIZE 80

struct music_info
{
	int type;

	char title[INFO_STR_SIZE];
	char artist[INFO_STR_SIZE];
	char comment[INFO_STR_SIZE];
	char decoder_name[INFO_STR_SIZE];
	char encode_msg[INFO_STR_SIZE];

	float cur_time;
	float duration;
	int freq;
	int channels;
	int psp_freq[2];
	int avg_kbps;
	int ins_kbps;

	int file_handle;
	int channel_handle;
};

/* get_status returns: */
enum
{
	ST_LOADED = 0,
	ST_PAUSED = 1,
	ST_PLAYING = 2,
	ST_STOPPED = 3,				/* In this status the alloctated resources were freed */
	ST_FFOWARD = 4,
	ST_FBACKWARD = 5,
	ST_SUSPENDED = 6,
	ST_UNKNOWN = 7
};

struct music_ops
{
	const char *name;
	int (*set_opt) (const char *key, const char *value);
	int (*load) (const char *spath, const char *lpath);
	int (*play) (void);
	int (*pause) (void);
	int (*fforward) (int);
	int (*fbackward) (int);
	int (*get_status) (void);
	int (*get_info) (struct music_info *);
	int (*suspend) (void);
	int (*resume) (const char *filename);
	int (*end) (void);

	struct music_ops *next;
};

int register_musicdrv(struct music_ops *drv);
int unregister_musicdrv(struct music_ops *drv);
struct music_ops *get_musicdrv(const char *name);
struct music_ops *set_musicdrv(const char *name);
int musicdrv_maxindex(void);
int musicdrv_set_opt(const char *key, const char *value);
int musicdrv_load(const char *spath, const char *lpath);
int musicdrv_play(void);
int musicdrv_pause(void);
int musicdrv_end(void);
int musicdrv_fforward(int);
int musicdrv_fbackward(int);
int musicdrv_get_status(void);
int musicdrv_suspend(void);
int musicdrv_resume(const char *filename);
int musicdrv_get_info(struct music_info *info);
