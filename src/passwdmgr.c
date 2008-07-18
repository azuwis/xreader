#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include "common/datatype.h"
#include "common/utils.h"
#include "buffer.h"
#include "passwdmgr.h"
#include "scene.h"
#include "strsafe.h"
#include "dbg.h"

static buffer **list = NULL;
static int list_count = 0;

static char *chopper(const char *str)
{
	char *p = strdup(str);

	if (p[strlen(p) - 1] == '\n' || p[strlen(p) - 1] == '\r')
		p[strlen(p) - 1] = '\0';
	if (p[strlen(p) - 1] == '\n' || p[strlen(p) - 1] == '\r')
		p[strlen(p) - 1] = '\0';

	return p;
}

bool load_passwords()
{
	char path[PATH_MAX];
	char buf[80];
	FILE *fp;
	int t = 0;

	STRCPY_S(path, scene_appdir());
	STRCAT_S(path, "password.lst");
	fp = fopen(path, "rb");
	if (fp == NULL) {
		return false;
	}

	free_passwords();
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		t++;
	}
	fseek(fp, 0, SEEK_SET);

	list = malloc(sizeof(list[0]) * t);
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] == '\0' || buf[0] == '\r' || buf[0] == '\n')
			continue;
		char *t = chopper(buf);

		add_password(t);
		free(t);
	}

	fclose(fp);

	return true;
}

bool save_passwords()
{
	FILE *fp;
	char path[PATH_MAX];
	int i, n;

	STRCPY_S(path, scene_appdir());
	STRCAT_S(path, "password.lst");
	fp = fopen(path, "wb");
	if (fp == NULL) {
		return false;
	}

	for (i = 0, n = get_password_count(); i < n; ++i) {
		buffer *b = get_password(i);

		fprintf(fp, "%s\r\n", b->ptr);
	}
	fclose(fp);
	return true;
}

void free_passwords()
{
	if (list != NULL) {
		int i;

		for (i = 0; i < list_count; ++i)
			buffer_free(list[i]);
		free(list);
		list = NULL;
		list_count = 0;
	}
}

static int find_password(const char *password)
{
	if (list == NULL || password == NULL)
		return -1;

	int i, n;

	for (i = 0, n = get_password_count(); i < n; ++i) {
		if (list[i]->ptr && strcmp(password, list[i]->ptr) == 0)
			return i;
	}

	return -1;
}

void add_password(const char *passwd)
{
	int s;

	if (passwd == NULL || passwd[0] == '\0')
		return;
	if ((s = find_password(passwd)) != -1) {
		if (s != 0) {
			buffer *t;

			memcpy(&t, &list[s], sizeof(list[s]));
			memcpy(&list[s], &list[0], sizeof(list[s]));
			memcpy(&list[0], &t, sizeof(list[s]));
		}
		return;
	}

	if (list == NULL) {
		list = malloc(sizeof(list[0]));
		char *t = chopper(passwd);

		list[0] = buffer_init_string(t);
		free(t);
		list_count = 1;
	} else {
		list = safe_realloc(list, sizeof(list[0]) * (list_count + 1));
		memmove(list + 1, list, sizeof(list[0]) * list_count);
		char *t = chopper(passwd);

		list[0] = buffer_init_string(t);
		free(t);
		list_count++;
	}
}

int get_password_count()
{
	return list_count;
}

buffer *get_password(int num)
{
	if (num < 0 || num >= list_count)
		return NULL;

	return list[num];
}
