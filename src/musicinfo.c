#include <stdio.h>
#include <pspkernel.h>
#include "strsafe.h"
#include "musicinfo.h"

/**
 * 通用音乐文件标签读取
 *
 * @param pInfo 音乐信息结构体指针
 * @param spath 文件短路径, 8.3文件名形式
 *
 * @return 成功返回0, 否则返回-1
 */
int generic_readtag(MusicInfo *pInfo, const char* spath)
{
	if (pInfo == NULL || spath == NULL) {
		return -1;
	}

	return 0;
}
