#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "log.h"

static FILE * logstrm = NULL;

extern bool log_open(const char * logfile)
{
	if(logstrm != NULL)
		fclose(logstrm);

	logstrm = fopen(logfile, "a");
	if(!logstrm)
		return false;

	return true;
}

extern void log_msg(const char * module, const char * fmt, ...)
{
	if(!logstrm)
		return;

    va_list     args;
    char        time_string[32];
    struct tm * tmnow;
    time_t      now;

	time(&now);
	if (!(tmnow = localtime(&now)))
		strcpy(time_string, "Unknown");
	else
		strftime(time_string, 32, "%m-%d %H:%M:%S", tmnow);
	if(module != NULL)
		fprintf(logstrm, "%s [%s]: ", time_string, module);
	else
		fprintf(logstrm, "%s: ", time_string);

	va_start(args, fmt);
    vfprintf(logstrm, fmt, args);
    va_end(args);
    fprintf(logstrm, "\r\n");
	fflush(logstrm);
}

extern void log_close()
{
	if(logstrm != NULL)
	{
		fclose(logstrm);
		logstrm = NULL;
	}
}
