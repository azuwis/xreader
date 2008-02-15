/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef DBG_H
#define DBG_H

#include <assert.h>
#include "buffer.h"

#define TODO	assert("TODO" && 0)

typedef void (*dbg_func) (void *arg, const char *str);
typedef struct _dbg_handle
{
	void (*init) (void *arg);
	dbg_func write;
	void (*cleanup) (void *arg);
	void *arg;
} dbg_handle;

typedef struct _Dbg
{
	dbg_handle *ot;
	size_t otsize;
	short on;
} DBG, *PDBG;

/** ��ʼ�� */
extern DBG *dbg_init();

/** �ر� */
extern int dbg_close(DBG * d);

/** �ر�ĳһ��� */
int dbg_close_handle(DBG * d, size_t index);

/** ������ļ� */
extern int dbg_open_file(DBG * d, const char *fn);

/** ������ļ��� */
extern int dbg_open_stream(DBG * d, FILE * stream);

#ifdef WIN32
/** ���������̨ */
extern int dbg_open_console(DBG * d, HANDLE console);
#endif
/** ������ڶ� */
extern int dbg_open_dummy(DBG * d);

#ifdef WIN32
/** ��������� */
extern int dbg_open_net(DBG * d, const char *hostname, int port);
#endif
/** �����PSPӢ���ն� */
int dbg_open_psp(DBG * d);

/** �����PSP�����ն� */
int dbg_open_psp_hz(DBG * d);

/** �����PSP��¼�ļ� */
int dbg_open_psp_logfile(DBG * d, const char *logfile);

/** ������Զ��庯�� */
extern int dbg_open_custom(DBG * d, void (*writer) (const char *));

/** ��ʱ���ʽ����� */
extern int dbg_printf(DBG * d, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** ��ʽ����� */
extern int dbg_printf_raw(DBG * d, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** ʮ������ת��,��ASCII�汾 */
extern int dbg_hexdump(DBG * d, const unsigned char *data, size_t len);

/** ʮ������ת��,ASCII�汾 */
extern int dbg_hexdump_ascii(DBG * d, const unsigned char *data, size_t len);

/** ����������� */
extern void dbg_switch(DBG * d, short on);

extern int dbg_open_memorylog(DBG * d);
extern const char *dbg_get_memorylog(void);

void dbg_assert(DBG * d, char *info, int test, const char *func,
				const char *file, int line);
#define DBG_ASSERT(d, info, test) dbg_assert(d, info, test, __FUNCTION__, __FILE__, __LINE__)

enum
{ DBG_BUFSIZE = 800 };

extern DBG *d;
double pspDiffTime(u64 * t1, u64 * t2);

extern buffer *dbg_memory_buffer;

#endif
