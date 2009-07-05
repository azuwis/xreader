/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "conf.h"
#include "common/utils.h"
#include "array.h"
#include "config.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define NDEBUG

Array *array_init(void)
{
	Array *pArr = calloc(1, sizeof(*pArr));

	assert(pArr);
	if (pArr == NULL)
		return NULL;

	pArr->elem = calloc(INITIAL_CAPABLE, sizeof(*pArr->elem));
	assert(pArr->elem);
	pArr->size = 0;

	if (pArr->elem == NULL) {
		free(pArr);
		return NULL;
	}

	pArr->capable = INITIAL_CAPABLE;

	return pArr;
}

void array_free(Array * arr)
{
	assert(arr != NULL);
	if (arr == NULL)
		return;

	free(arr->elem);
	free(arr);
}

size_t array_get_size(Array * arr)
{
	assert(arr != NULL);
	assert(arr->size <= arr->capable);
	if (arr == NULL)
		return 0;
	return arr->size;
}

size_t array_add_element(Array * arr, size_t pos, Element * elem)
{
	assert(arr != NULL);
	assert(elem);
	assert(pos <= arr->size);

	if (arr == NULL || elem == NULL || pos > arr->size)
		return 0;

	if (arr->size + 1 > arr->capable) {
		while (arr->capable <= arr->size)
			arr->capable += INCREMENT_CAPABLE;
		arr->elem = safe_realloc(arr->elem, arr->capable * sizeof(*arr->elem));
		if (arr->elem == NULL)
			return 0;
	}

	if (pos == arr->size) {
		memcpy(&arr->elem[pos], elem, sizeof(Element));
	} else {
		memmove(&arr->elem[pos + 1], &arr->elem[pos],
				sizeof(Element) * (arr->size - pos));
		memcpy(&arr->elem[pos], elem, sizeof(Element));
	}
	arr->size++;

	return 1;
}

size_t array_append_element(Array * arr, Element * elem)
{
	assert(arr != NULL);
	assert(elem);
	if (arr == NULL || elem == NULL)
		return 0;
	return array_add_element(arr, array_get_size(arr), elem);
}

size_t array_del_element(Array * arr, size_t pos)
{
	assert(arr != NULL);
	assert(pos < arr->size);
	assert(arr->size);

	if (arr == NULL || pos >= arr->size || arr->size == 0)
		return 0;

	if (pos == arr->size - 1) {
		;
	} else if (pos != 0) {
		memmove(&arr->elem[pos - 1], &arr->elem[pos],
				sizeof(Element) * (arr->size - pos - 1));
	} else {
		memmove(&arr->elem[pos], &arr->elem[pos + 1],
				sizeof(Element) * (arr->size - pos - 1));
	}
	arr->size--;

	// ÊÕËõÊý×é
	if (arr->size < arr->capable / 2 && arr->size >= INITIAL_CAPABLE) {
		arr->capable = arr->size + INITIAL_CAPABLE;
		arr->elem = safe_realloc(arr->elem, arr->capable * sizeof(*arr->elem));
		if (arr->elem == NULL)
			return 0;
	}
	return 1;
}

int array_find_element_by_func(Array * arr, ArrayFinder finder, void *userData)
{
	size_t i;
	Element *p;

	assert(arr != NULL);
	assert(arr->elem != NULL);
	assert(finder != NULL);

	if (arr == NULL || arr->elem == NULL || finder == NULL)
		return -1;

	p = arr->elem;

	for (i = 0; i < arr->size; ++i) {
		if ((*finder) (p++, userData))
			return i;
	}
	return -1;
}

size_t array_swap_element(Array * arr, size_t pos1, size_t pos2)
{
	Element elem;

	assert(arr != NULL);
	assert(arr->elem != NULL);

	if (arr == NULL || arr->elem == NULL || arr->size < 2 || pos1 == pos2
		|| pos1 >= arr->size || pos2 >= arr->size)
		return 0;

	memcpy(&elem, &arr->elem[pos1], sizeof(Element));
	memcpy(&arr->elem[pos1], &arr->elem[pos2], sizeof(Element));
	memcpy(&arr->elem[pos2], &elem, sizeof(Element));

	return 1;
}

#ifdef TEST_ARRAY

bool find_path_name(Element * elem, void *userData)
{
	if (strcmp(elem->test, (const char *) userData) == 0)
		return true;
	return false;
}

Element *new_element(int i)
{
	Element *p;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		return p;

	sprintf(p->test, "ms0:/music/test%d.mp3", i);

	return p;
}

int main(int argc, char *argv[])
{
	Array *myArray = array_init();
	Element *p;

	assert(myArray);
	size_t size = array_get_size(myArray);

	assert(size == 0);
	int i;

	for (i = 0; i < 1000; ++i) {
		Element *p = new_element(i);

		array_append_element(myArray, p);
		free(p);
	}
	p = new_element(10);
	array_add_element(myArray, 0, p);
	assert(myArray->size == 1001);
	free(p);
	assert(strcmp(myArray->elem[0].test, "ms0:/music/test10.mp3") == 0);
	assert(strcmp(myArray->elem[1].test, "ms0:/music/test0.mp3") == 0);
	assert(strcmp(myArray->elem[10].test, "ms0:/music/test9.mp3") == 0);
	assert(strcmp(myArray->elem[1000].test, "ms0:/music/test999.mp3") == 0);
	assert(myArray->size <= myArray->capable);
	array_del_element(myArray, 0);
	assert(strcmp(myArray->elem[0].test, "ms0:/music/test0.mp3") == 0);
	array_del_element(myArray, 1);
	assert(strcmp(myArray->elem[1].test, "ms0:/music/test2.mp3") == 0);
	array_del_element(myArray, 998);
	assert(strcmp
		   (myArray->elem[array_get_size(myArray) - 1].test,
			"ms0:/music/test998.mp3") == 0);

	while (array_get_size(myArray)) {
		array_del_element(myArray, 0);
	}
	assert(myArray->size == 0);

	int findpos = array_find_element_by_func(myArray, find_path_name,
											 "ms0:/music/test445.mp3");

	assert(findpos < 0);

	for (i = 0; i < 1000; ++i) {
		Element *p = new_element(i);

		array_append_element(myArray, p);
		free(p);
	}

	findpos =
		array_find_element_by_func(myArray, find_path_name,
								   "ms0:/music/test445.mp3");
	assert(findpos == 445);

	array_swap_element(myArray, 0, 1);
	assert(strcmp(myArray->elem[0].test, "ms0:/music/test1.mp3") == 0);
	assert(strcmp(myArray->elem[1].test, "ms0:/music/test0.mp3") == 0);

	assert(array_swap_element(myArray, 0, 1000) == 0);

	array_free(myArray);
	return 0;
}
#endif
