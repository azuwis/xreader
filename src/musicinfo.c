#include <stdio.h>
#include <pspkernel.h>
#include "strsafe.h"
#include "musicinfo.h"

/**
 * ͨ�������ļ���ǩ��ȡ
 *
 * @param pInfo ������Ϣ�ṹ��ָ��
 * @param spath �ļ���·��, 8.3�ļ�����ʽ
 *
 * @return �ɹ�����0, ���򷵻�-1
 */
int generic_readtag(MusicInfo *pInfo, const char* spath)
{
	if (pInfo == NULL || spath == NULL) {
		return -1;
	}

	return 0;
}
