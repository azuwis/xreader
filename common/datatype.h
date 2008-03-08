#ifndef _DATATYPE_H_
#define _DATATYPE_H_

#ifndef byte
typedef unsigned char byte;
#endif

#ifndef word
typedef unsigned short word;
#endif

#ifndef dword
typedef unsigned long dword;
#endif

#ifndef bool
typedef int bool;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef null
#define null ((void *)0)
#endif

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef max
#define max(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef INVALID
#define INVALID ((dword)-1)
#endif

#define PATH_MAX 512

#endif
