/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _VERSION_H_
#define _VERSION_H_

#include "Revision.h"
#include "config.h"

#define XREADER_VERSION_LONG VERSION " Built "  __TIME__ " " __DATE__ " " "(REV" REVISION ")"
#define XREADER_VERSION_STR_LONG PACKAGE_NAME " " XREADER_VERSION_LONG

#endif
