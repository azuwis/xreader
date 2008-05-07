#include <stdio.h>
#include "APETag.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

char *APETag_errlist[] = {
	"OK",
	"Can't Open File",
	"Error File Format",
	"Unsupport Tag Version",
	"Bad Argument Error",
	"Function Not Implemented",
	"Memory Not Enough",
};

int APETag_nerr = (sizeof(APETag_errlist) / sizeof(APETag_errlist[0]));

const char *APETag_strerror(int errno)
{
	static char emsg[1024];

	if (errno < 0 || errno >= APETag_nerr)
		snprintf(emsg, sizeof(emsg), "Error %d occurred.", errno);
	else
		snprintf(emsg, sizeof(emsg), "%s", APETag_errlist[errno]);

	return emsg;
}
