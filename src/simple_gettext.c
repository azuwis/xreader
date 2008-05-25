#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "strsafe.h"
#include "sofile.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif
#include "simple_gettext.h"

static PTextDomainEntry g_domain_head = NULL;
static PTextDomainEntry g_domain_tail = NULL;
static struct hash_control *g_curr_transitem = NULL;
static char g_simple_curr_domainname[80];

static PTextDomainEntry find_domain(const char *domainname)
{
	PTextDomainEntry p = g_domain_head;

	while (p != NULL) {
		if (strcmp(p->domainname, domainname) == 0)
			break;
		p = p->next;
	}

	return p;
}

const char *simple_gettext(const char *msgid)
{
	if (msgid == NULL)
		return NULL;

	char *p;

	if ((p = lookup_transitem(g_curr_transitem, msgid)) != NULL) {
		return p;
	}
#ifdef _DEBUG_
	return "MISSING";
#else
	return msgid;
#endif
}

char *simple_bindtextdomain(const char *domainname, const char *dirname)
{
	if (domainname == NULL)
		return NULL;

	if (dirname == NULL) {
		PTextDomainEntry p = find_domain(domainname);

		if (p == NULL)
			return NULL;
		return p->dirname;
	}

	PTextDomainEntry p = NULL;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;

	p->domainname = strdup(domainname);
	p->dirname = strdup(dirname);
	p->next = NULL;

	if (g_domain_head == NULL) {
		g_domain_head = p;
		g_domain_tail = g_domain_head;
	} else {
		g_domain_tail->next = p;
		g_domain_tail = p;
	}

	STRCPY_S(g_simple_curr_domainname, domainname);

	return g_simple_curr_domainname;
}

char *simple_textdomain(const char *domainname)
{
	if (domainname == NULL) {
		return g_simple_curr_domainname;
	}

	PTextDomainEntry p = find_domain(domainname);

	if (p == NULL)
		return NULL;

	STRCPY_S(g_simple_curr_domainname, domainname);

	char path[BUFSIZ];

	STRCPY_S(path, p->dirname);
	STRCAT_S(path, "/");
	STRCAT_S(path, p->domainname);
	STRCAT_S(path, ".so");

	if (g_curr_transitem) {
		simple_gettext_destroy();
		g_curr_transitem = NULL;
	}

	int size = 0;

	if (read_sofile(path, &g_curr_transitem, &size) != 0) {
		return NULL;
	}

	return g_simple_curr_domainname;
}

static void destroy(string, value)
char *string;
char *value;
{
	free(string);
	free(value);
}

void simple_gettext_destroy(void)
{
	hash_traverse(g_curr_transitem, destroy);
	hash_die(g_curr_transitem);
	g_curr_transitem = NULL;
}
