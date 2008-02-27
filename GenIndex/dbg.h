#ifndef DBG_H
#define DBG_H

#include <stdio.h>
#include <assert.h>

#define TODO	assert("TODO" && 0)

typedef void (*dbg_func)(void *arg, const char* str);
typedef struct _dbg_handle {
	void (*init)(void *arg);
	dbg_func write;
	void (*cleanup)(void *arg);
	void *arg;
} dbg_handle;

typedef struct _Dbg {
	dbg_handle* ot;
	size_t      otsize;
	short on;
} DBG, *PDBG;

/** ��ʼ�� */
extern DBG* dbg_init();
/** �ر� */
extern int  dbg_close(DBG* d);
/** ������ļ� */
extern int  dbg_open_file(DBG *d, const char* fn);
/** ������ļ��� */
extern int  dbg_open_stream(DBG *d, FILE *stream);
/** ���������̨ */
// extern int  dbg_open_console(DBG *d, HANDLE console);
/** ������ڶ� */
extern int  dbg_open_dummy(DBG *d);
/** ��������� */
extern int  dbg_open_net(DBG *d, const char* hostname, int port);
/** ������Զ��庯�� */
extern int  dbg_open_custom(DBG *d, void (*writer)(const char*));
/** ��ʽ����� */
extern int  dbg_printf(DBG *d, const char* fmt, ...);
/** ʮ������ת��,��ASCII�汾 */
extern int  dbg_hexdump(DBG *d, const unsigned char* data, size_t len);
/** ʮ������ת��,ASCII�汾 */
extern int  dbg_hexdump_ascii(DBG *d, const unsigned char* data, size_t len);
/** ����������� */
extern void dbg_switch(DBG *d, short on);

enum { DBG_BUFSIZE = 80 };

#endif
