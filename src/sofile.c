#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sofile.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

PTransItem g_item_head = NULL;
PTransItem g_item_tail = NULL;

static const char *search_digit(const char *sdigit, int maxsize)
{
	int found = 0;
	int n = 0;

	while (n <= maxsize && sdigit[n] != '\0') {
		if (!isdigit(sdigit[n])) {
			found = 1;
			break;
		}
		n++;
	}

	if (found == 0)
		return NULL;

	return sdigit + n;
}

static int convert_c_style_string(char *dest, int destsize, char *src)
{
	int n;
	char *p = dest;

	for (n = 0; n < destsize && src[n] != '\0'; ++n) {
		if (src[n] == '\\') {
			if (n + 1 < destsize) {
				switch (src[n + 1]) {
					case '\"':
						*p++ = '\"';
						n++;
						break;
					case 'b':
						if (p > dest)
							p--;
						n++;
						break;
					case 'r':
						n++;
						break;
					case 'n':
						*p++ = '\n';
						n++;
						break;
					case 't':
						*p++ = '\t';
						n++;
						break;
					case '\\':
						*p++ = '\\';
						n++;
						break;
					default:
						if (src[n + 1] == 'x') {
							const char *endp = search_digit(&src[n + 2],
															100);

							if (endp != NULL) {
								char number[80];

								strncpy(number,
										&src[n + 2], endp - &src[n + 2]);
								number[endp - &src[n + 2]] = '\0';
								int digit = strtol(number, NULL,
												   16);

								*p++ = digit;
								n += endp - &src[n + 2] + 1;
							}
						} else if (isdigit(src[n + 1])) {
							const char *endp = search_digit(&src[n + 1],
															100);

							if (endp != NULL) {
								char number[80];

								strncpy(number,
										&src[n + 1], endp - &src[n + 1]);
								number[endp - &src[n + 1]] = '\0';
								int digit = strtol(number, NULL,
												   8);

								*p++ = digit;
								n += endp - &src[n + 1];
							}
						} else {
//                          printf("Bad input\n");
						}
						break;
				}
			} else {
//              printf("Bad input\n");
			}
		} else {
			*p++ = src[n];
		}
	}

	if (n < destsize) {
		*p++ = '\0';
		n++;
	}

	return n;
}

int read_sofile(const char *path, struct hash_control **hash, int *size)
{
	if (path == NULL || size == NULL || hash == NULL)
		return -1;

	FILE *fp = fopen(path, "r");
	char line[BUFSIZ];
	char *msgid = NULL, *msgstr = NULL;

	*size = 0;

	*hash = hash_new();

	if (fp == NULL)
		return -1;

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (line[0] == '#')
			continue;
		if (strncmp(line, "msgid", 5) == 0) {
			int pos1 = strstr(line, " \"") - line;
			int pos2 = strrchr(line, '\"') - line;

			if (pos1 < pos2 - 2) {
				msgid = strndup(line + pos1 + 2, pos2 - pos1 - 2);
				if (msgid == NULL)
					continue;
				char *t = strdup(msgid);

				if (t == NULL) {
					free(msgid);
					msgid = NULL;
				}

				convert_c_style_string(t, pos2 - pos1 - 2 + 1, msgid);

				free(msgid);
				msgid = t;
			}
		} else if (strncmp(line, "msgstr", 6) == 0) {
			int pos1 = strstr(line, " \"") - line;
			int pos2 = strrchr(line, '\"') - line;

			if (pos1 < pos2 - 2) {
				msgstr = strndup(line + pos1 + 2, pos2 - pos1 - 2);
				if (msgid == NULL)
					continue;
				char *t = strdup(msgstr);

				if (t == NULL) {
					free(msgid);
					msgid = NULL;
				}

				convert_c_style_string(t, pos2 - pos1 - 2 + 1, msgstr);
				free(msgstr);
				msgstr = t;
			}
		}
		if (msgid && msgstr) {
			const char *str = hash_insert(*hash, msgid, msgstr);

			if (str == NULL) {
				(*size)++;
			}
			msgid = NULL;
			msgstr = NULL;
		}
	}

	fclose(fp);

	return 0;
}

char *lookup_transitem(struct hash_control *p, const char *msgid)
{
	if (p == NULL || msgid == NULL)
		return NULL;

	char *t = hash_find(p, msgid);

	return t;
}
