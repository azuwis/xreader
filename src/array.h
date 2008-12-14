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
	/// �����׵�ַ
	Element *elem;
	/// ������ʹ�ô�С��ÿԪ��
	size_t size;
	/// ����ȫ����С��ÿԪ��
	size_t capable;
} Array;

typedef bool(*ArrayFinder) (Element * elem, void *userData);

/**
 * ��ʼ������
 *
 * @return �����ַ����СΪ0, ��ʼʱ��һ��Ԥ���ռ�
 *
 * - NULL �ڴ治��
 */
Array *array_init(void);

/**
 * �������飬�ͷ�ȫ����ռ���ڴ�
 *
 * @param arr Ҫ�ͷŵ��ڴ�
 */
void array_free(Array * arr);

/**
 * �õ����鵱ǰ��С
 *
 * @param arr ����
 * @return �����С
 * - 0 �޿ռ������Ч����
 */
size_t array_get_size(Array * arr);

/**
 * ���Ԫ�ص�����ĳλ��
 *
 * @param arr ����
 * @param pos λ�ã�Ԫ�ؽ�����pos֮ǰ������
 * @param elem Ҫ��ӵ�Ԫ�ص�ַ
 *
 * @return ���Ԫ�ظ�����ͨ��Ϊ1
 * - !=0 �ɹ�
 * - 0 �޿ռ������Ч����
 *
 * @note �Ḵ��Ԫ�����ݵ�����
 */
size_t array_add_element(Array * arr, size_t pos, Element * elem);

/**
 * ���Ԫ�ص�����β��
 *
 * @param arr ����
 * @param elem Ҫ��ӵ�Ԫ�ص�ַ
 * @return ���Ԫ�ظ�����ͨ��Ϊ1
 * - !=0 �ɹ�
 * - 0 �޿ռ������Ч����
 *
 * @note �Ḵ��Ԫ�����ݵ�����
 */
size_t array_append_element(Array * arr, Element * elem);

/**
 * ������ɾ��ĳԪ��
 *
 * @param arr ����
 * @param pos λ��
 *
 * @return ɾ��Ԫ�ظ�����ͨ��Ϊ1
 * - !=0 �ɹ�
 * - 0 �޷�ɾ��������Ч����
 *
 * @note ������滹��Ԫ�أ�����Ԫ�ؽ��ᱻ��������
 */
size_t array_del_element(Array * arr, size_t pos);

/**
 * ʹ�ûص��������������е�Ԫ��
 *
 * @param arr ����
 * @param finder ArrayFinder���͵Ļص�����
 * @param userData �û�����ָ��
 *
 * @return Ԫ��λ��
 * - <0 û���ҵ�
 *
 * @note ����ڻص���������ӻ�ɾ���������е�Ԫ�أ�����ɲ���Ԥ�ϵĺ��
 */
int array_find_element_by_func(Array * arr, ArrayFinder finder, void *userData);

/**
 * ������Ԫ��λ��
 *
 * @param arr ����
 * @param pos1 λ��1
 * @param pos2 λ��2
 *
 * @return
 * - >0 �ɹ�
 * - 0  ʧ��
 */
size_t array_swap_element(Array * arr, size_t pos1, size_t pos2);
