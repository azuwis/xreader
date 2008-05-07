#ifndef _QSORT_H_
#define _QSORT_H_

#include "datatype.h"

typedef int (*qsort_compare) (void *data1, void *data2);
extern void quicksort(void *data, int left, int right, int datasize,
					  qsort_compare compare);

#endif
