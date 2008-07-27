#pragma once

#ifdef ENABLE_NLS
typedef struct _TextDomainEntry TextDomainEntry;
typedef struct _TextDomainEntry *PTextDomainEntry;

struct _TextDomainEntry
{
	char *domainname;
	char *dirname;
	PTextDomainEntry next;
};

const char *simple_gettext(const char *msgid);
char *simple_bindtextdomain(const char *domainname, const char *dirname);
char *simple_textdomain(const char *domainname);
void simple_gettext_destroy(void);

#define _(STRING) simple_gettext(STRING)
#else

#define _(STRING) (STRING)
#define simple_gettext(msgid)
#define simple_bindtextdomain(domainname, dirname)
#define simple_textdomain(domainname) (domainname)
#define simple_gettext_destroy()

#endif
