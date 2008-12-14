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

#pragma once

#define INITIAL_CAPABLE 10
#define INCREMENT_CAPABLE 5

typedef struct _Element
{
	char path[PATH_MAX];
} Element;

typedef struct _Array
{
	/// 数组首地址
	Element *elem;
	/// 数组已使用大小，每元素
	size_t size;
	/// 数组全部大小，每元素
	size_t capable;
} Array;

typedef bool(*ArrayFinder) (Element * elem, void *userData);

/**
 * 初始化数组
 *
 * @return 数组地址，大小为0, 初始时有一定预留空间
 *
 * - NULL 内存不足
 */
Array *array_init(void);

/**
 * 销毁数组，释放全部所占用内存
 *
 * @param arr 要释放的内存
 */
void array_free(Array * arr);

/**
 * 得到数组当前大小
 *
 * @param arr 数组
 * @return 数组大小
 * - 0 无空间或者无效数组
 */
size_t array_get_size(Array * arr);

/**
 * 添加元素到数组某位置
 *
 * @param arr 数组
 * @param pos 位置，元素将会在pos之前被插入
 * @param elem 要添加的元素地址
 *
 * @return 添加元素个数，通常为1
 * - !=0 成功
 * - 0 无空间或者无效数组
 *
 * @note 会复制元素内容到数组
 */
size_t array_add_element(Array * arr, size_t pos, Element * elem);

/**
 * 添加元素到数组尾部
 *
 * @param arr 数组
 * @param elem 要添加的元素地址
 * @return 添加元素个数，通常为1
 * - !=0 成功
 * - 0 无空间或者无效数组
 *
 * @note 会复制元素内容到数组
 */
size_t array_append_element(Array * arr, Element * elem);

/**
 * 从数组删除某元素
 *
 * @param arr 数组
 * @param pos 位置
 *
 * @return 删除元素个数，通常为1
 * - !=0 成功
 * - 0 无法删除或者无效数组
 *
 * @note 如果后面还有元素，后面元素将会被依次上移
 */
size_t array_del_element(Array * arr, size_t pos);

/**
 * 使用回调函数查找数组中的元素
 *
 * @param arr 数组
 * @param finder ArrayFinder类型的回调函数
 * @param userData 用户数据指针
 *
 * @return 元素位置
 * - <0 没有找到
 *
 * @note 如果在回调函数中添加或删除此数组中的元素，将造成不可预料的后果
 */
int array_find_element_by_func(Array * arr, ArrayFinder finder, void *userData);

/**
 * 交换两元素位置
 *
 * @param arr 数组
 * @param pos1 位置1
 * @param pos2 位置2
 *
 * @return
 * - >0 成功
 * - 0  失败
 */
size_t array_swap_element(Array * arr, size_t pos1, size_t pos2);
