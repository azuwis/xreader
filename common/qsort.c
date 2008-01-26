#include <string.h>
#include "qsort.h"
#include "scene.h"

__inline void swap_data(void * data1, void * data2, int datasize)
{
	byte temp[datasize];
	memcpy(temp, data1, datasize);
	memcpy(data1, data2, datasize);
	memcpy(data2, temp, datasize);
}

void _quicksort(void * data, int left, int right, int datasize, qsort_compare compare)
{
	int i, last;
	if (left >= right)
		return;
	byte (* qdata)[datasize];
	qdata = (byte (*)[datasize])data;
	if(right - left > 1)
		swap_data(&qdata[left], &qdata[(left + right)/2], datasize);
	last = left;
	for (i = left + 1; i <= right; i++)
		if (compare(&qdata[i], &qdata[left]) < 0)
			swap_data(&qdata[++last], &qdata[i], datasize);
	swap_data(&qdata[left], &qdata[last], datasize);
	_quicksort(data, left, last-1, datasize, compare);
	_quicksort(data, last+1, right, datasize, compare);
}

extern void quicksort(void * data, int left, int right, int datasize, qsort_compare compare)
{
	scene_power_save(false);
	_quicksort(data, left, right, datasize, compare);
	scene_power_save(true);
}
