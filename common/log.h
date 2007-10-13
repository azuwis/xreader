#ifndef _LOG_H_
#define _LOG_H_

#include "datatype.h"

extern bool log_open(const char * logfile);
extern void log_msg(const char * module, const char * fmt, ...);
extern void log_close();

#endif
