/*
 * main.c
 *
 * Copyright (c) outmatch 2007
 *
 * You may distribute and modify this program under the terms of either
 * the GNU General Public License Version 3.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "config.h"
#include "dbg.h"

#if !(defined(MAGICTOKEN) && defined(SPLITTOKEN))
#error û�ж���MAGICTOKEN �� SPLITTOKEN
#endif

DBG *d;

// Ŀ¼��
typedef struct {
	char *name;
	size_t page;
} DirEntry;

typedef struct {
	char* path;
	char* name;
	size_t size;
	size_t dirsize;
	size_t context;
} TxtFile;

TxtFile g_file;
DirEntry *g_dirs = 0;
size_t g_dircnt = 0;
size_t g_dircap = 0;

DirEntry * AddDirEntry(const char* name, int page);
void ParseFile(void);
int PrintDir(FILE *fp);
int VPrintDir(void);
void FreeDirEntry(void);

#if defined(WIN32) || defined(_MSC_VER)
#include <windows.h>
void GetTempFilename(char* str, size_t size)
{
	GetTempPath(size, str);
	GetTempFileName(str, ("GenIndex_"), 0, str);
}
#else
void GetTempFilename(char* str, size_t size)
{
	strcpy(str, "/tmp/");
	mktemp(str);
}
#endif

int main(int argc, char* argv[])
{
	struct stat statbuf;
	d = dbg_init();
	dbg_open_stream(d, stderr);
	dbg_switch(d, 0);
#ifdef WIN32
	if(stricmp(argv[argc-1], "-d") == 0)
		dbg_switch(d, 1);
#else
	if(strcasecmp(argv[argc-1], "-d") == 0)
		dbg_switch(d, 1);
#endif

	if(argc < 2) {
		fprintf(stderr, "xReader Ŀ¼���ɹ��� GenIndex (version 0.1)\n");
		fprintf(stderr, "�÷�: GenIndex.exe �ļ���.txt [-d]\n");
		fprintf(stderr, "                               -d: ����ģʽ\n");
		fprintf(stderr, "ʹ�÷���\n");
		fprintf(stderr, "1. �����TXT�����������<<<<���ű�עÿ�¿�ʼ\n");
		fprintf(stderr, "��\n");
		fprintf(stderr, "<<<<��һ�� XXX\n");
		fprintf(stderr, "....\n");
		fprintf(stderr, "<<<<�ڶ��� YYY\n");
		fprintf(stderr, "2. ����󣬽�TXT�ϵ�GenIndexͼ���У�GenIndex�ͻ�Ϊ��ĵ���������\n");
		fprintf(stderr, "һ������Ŀ¼��\n");
		fprintf(stderr, "===================\n");
		fprintf(stderr, "ҳ�� Ŀ¼\n");
		fprintf(stderr, "1    ��һ�� XXX\n");
		fprintf(stderr, "2    �ڶ��� YYY\n");
		fprintf(stderr, "===================\n");
		fprintf(stderr, "3. ҳ�����־���GIֵ������ʱ����Ϣ��������ʾ�������Ҫ�ƶ���ĳ�£���ҳֱ��GI��ȼ����ҵ����¡�GI�Ǿ���ҳ�룬������Ϊ�����С���ı䡣\n");
		system("pause");
		return -1;
	}
	dbg_printf(d, "�����ļ���: %s", argv[1]);
	if(stat(argv[1], &statbuf) != 0) {
		fprintf(stderr, "�ļ�%sû���ҵ�", argv[1]);
		return -1;
	}
	else  {
		dbg_printf(d, "�ļ�: %s ��С: %ld�ֽ�", argv[1], statbuf.st_size);
		g_file.name = strdup(argv[1]);
		if(strrchr(g_file.name, '\\') != 0) {
			strcpy(g_file.name, strrchr(g_file.name, '\\')+1);
		}
		g_file.path = strdup(argv[1]);
		g_file.size = statbuf.st_size;
	}

	ParseFile();
	FreeDirEntry();

	dbg_close(d);
	return 0;
}

int GetCurPageNum(FILE *fp, int offset) {
	size_t pos;
	pos = ftell(fp) + offset;
	if(pos/1023 != 0)
	{
		return (int)((double)(pos) / 1023.0 + 0.5);
	}
	else
		return 1; /* ���ⷵ��0ҳ */
}

void SearchDir(FILE *fp)
{
	int iSplitCnt = 0;
	char buf[65536];

	// ���Ŀ¼
	g_dircnt = 0;

	fseek(fp, 0, SEEK_SET);
	
	// �ҵ�Ŀ¼���ַ
	while(!feof(fp)) {
		if(fgets(buf, sizeof(buf) / sizeof(buf[0]), fp)) {
			if(strcmp(buf, SPLITTOKEN) == 0)
				iSplitCnt++;
		}
		if(iSplitCnt >= 2)
			break;
	}

	if(iSplitCnt == 0)
		g_file.context = 0;
	if(iSplitCnt >= 2) {
		g_file.context = ftell(fp);
	}

	dbg_printf(d, "���Ŀ�ʼ��%d�ֽ�", g_file.context);

	fseek(fp, g_file.context, SEEK_SET);

	dbg_printf(d, "ɨ���ļ�%s,��%d�ֽ�ƫ��", g_file.name, g_file.dirsize);
	while(!feof(fp)) {
		if(fgets(buf, sizeof(buf) / sizeof(buf[0]), fp)) {
			if(buf[strlen(buf)-1] == '\n')
				buf[strlen(buf)-1] = '\0';
			if(strstr(buf, MAGICTOKEN) != 0) {
				char *p;
				dbg_printf(d, "���ҵ����: %s", buf);

				p = strstr(buf, MAGICTOKEN) + strlen(MAGICTOKEN);
				AddDirEntry(p, GetCurPageNum(fp, g_file.dirsize - g_file.context));
				dbg_printf(d, "��Ӽ�¼�%d��(����: %s ҳ�� %d)", g_dircnt, g_dirs[g_dircnt-1].name, g_dirs[g_dircnt-1].page);
			}
		}
	}
}

void ParseFile(void)
{
	char buf[BUFSIZ];
	FILE *fp, *foutp;
	char *szOutFn = 0;
	dbg_printf(d, "����TXT�ļ�: ·�� %s ���� %s ��С %d", g_file.path, g_file.name, g_file.size);
	fp = fopen(g_file.path, "r");
	char str[4];
	fgets(str, 4, fp);
	if(str[0] == '\xef' && str[1] == '\xbb' && str[2] == '\xbf')
	{
		fprintf(stderr, "GenIndex���ܴ���UTF8 TXT�ļ�������ת��ΪGBK����\n");
		system("pause");
		return;
	}
	
	g_file.dirsize = 0;
	fseek(fp, 0, SEEK_SET);
	SearchDir(fp);

	g_file.dirsize = VPrintDir();
	dbg_printf(d, "VPrintDir ����Ŀ¼��СΪ%d�ֽ�", g_file.dirsize);

	SearchDir(fp);

	szOutFn = (char*)malloc(MAX_PATH+1);
	GetTempFilename(szOutFn, MAX_PATH);
	dbg_printf(d, "�õ���ʱ�ļ���%s", szOutFn);
	foutp = fopen(szOutFn, "w");
	if(!foutp) {
		dbg_printf(d, "�����ļ�%sʧ��", szOutFn);
		return;
	}
	PrintDir(foutp);
	// ��ӡ��������
	fseek(fp, g_file.context, SEEK_SET);
	while(!feof(fp)) {
		if(fgets(buf, sizeof(buf) / sizeof(buf[0]), fp)) {
			fprintf(foutp, "%s", buf);
		}
	}
	fclose(fp);
	fclose(foutp);
#ifdef WIN32
	DeleteFile(g_file.path);
#else
	unlink(g_file.path);
#endif
#ifdef WIN32
	MoveFile(szOutFn, g_file.path);
#else
	rename(szOutFn, g_file.path);
#endif
	dbg_printf(d, "M630Ŀ¼������ GenIndex: %s Ŀ¼��С%d�ֽ� �������\n", g_file.name, g_file.dirsize);
	free(szOutFn);
}

int VPrintDir()
{
	int i, bytes;
	char buf[BUFSIZ];
	if(g_dircnt == 0)
		return 0;
	bytes = sprintf(buf, SPLITTOKEN);
	bytes += sprintf(buf, "ҳ�� Ŀ¼\n");
	for(i=0; i<g_dircnt; ++i) {
		bytes += sprintf(buf, "%-4d %s\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += sprintf(buf, SPLITTOKEN);
	return bytes;
}

int PrintDir(FILE *fp)
{
	int i, bytes;
	dbg_printf(d, "��ʼ��ӡĿ¼:");
	if(g_dircnt == 0)
		return 0;
	bytes = fprintf(fp, SPLITTOKEN);
	bytes += fprintf(fp, "ҳ�� Ŀ¼\n");
	for(i=0; i<g_dircnt; ++i) {
		bytes += fprintf(fp, "%-4d %s\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += fprintf(fp, SPLITTOKEN);
	return bytes;
}

DirEntry * AddDirEntry(const char* name, int page)
{
	dbg_printf(d, "g_dircnt %d g_dircap %d", g_dircnt, g_dircap);
	if(g_dircnt == 0) {
		g_dirs = (DirEntry*) malloc(sizeof(DirEntry));
		g_dirs->name = strdup(name);
		g_dirs->page = page;
		g_dircap = 1;
	}
	else {
		if(g_dircnt >= g_dircap) {
			g_dircap += 16;
			g_dirs = (DirEntry*) realloc(g_dirs, sizeof(DirEntry)*g_dircap);
		}
		g_dirs[g_dircnt].name = strdup(name);
		g_dirs[g_dircnt].page = page;
	}
	g_dircnt++;
	return g_dirs;
}

void FreeDirEntry(void)
{
	int i;

	if (g_dirs == NULL || g_dircnt == 0)
		return;
	for(i=0; i<g_dircnt; ++i) {
		free(g_dirs[i].name);
	}
	free(g_dirs);
	g_dircnt = 0;
	g_dirs = NULL;
}
