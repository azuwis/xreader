#ifndef __SCELIBASFPARSER_H__
#define __SCELIBASFPARSER_H__

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _SceAsfFrame {
	void *data;		// 帧数据指针
	u32 ms; 		// 当前帧时间毫秒数
	u32 unk1;		// 0~7的整数
	u32 unk2;		// 很可能是帧大小
	u32 unk3;		// 解帧开始第一帧为1,　其余为0
	u32 unk4;		// 128 256 512 1024 2048等等，可能是帧类型标志，有的WMA文件一直稳定在一个值，有的WMA文件经常变化，可能同VBR相关
	u32 unk5;		// 128 256 512 1024 2048等等，可能是帧类型标志，有的WMA文件一直稳定在一个值，有的WMA文件经常变化，可能同VBR相关
	u32 unk6;		// 不同WMA文件不相等的整数值，同一个WMA文件中一般是稳定的
	u8 unk_bytes[32];
} SceAsfFrame_500;

#define SceAsfFrame SceAsfFrame_500

/// Asf处理器上下文类型，必须64位对齐
typedef struct _SceAsfParser {
	SceUInt32 iUnk0;
	SceUInt32 iUnk1;
	SceUInt32 iUnk2;
	SceUInt32 iUnk3;
	SceUInt32 iUnk4;
	SceUInt32 iUnk5;
	SceUInt32 iUnk6;
	SceUInt32 iUnk7;
	SceUInt32 iNeedMem; //8
	ScePVoid  pNeedMemBuffer; //9
//	SceUInt32 iUnk10_3626[3616]; // 10 - 3626
	SceUInt32 iUnk10_20[10]; // 10 - 20 
	SceUInt64 iDuration;	// 20 - 22
	SceUInt32 iUnk22_3626[3604]; // 22 - 3626
	SceAsfFrame sFrame; //3626 - 3345
	SceUInt32 iUnk3345_3643[298]; //3345 - 3643
	SceUInt32 iUnk3644;
	SceUInt32 iUnk3644_4095[451];
} SceAsfParser_500;

#define SceAsfParser SceAsfParser_500

/**
 * 行为同sceIoRead
 */
typedef int64_t (*SceAsfParserReadCB)(void *userdata, void *buf, SceSize size);

/**
 * 当flag = 1时为相对当前位置移动offset字节
 * 当flag = 0时为绝对地址移动offset字节
 * 返回当前文件指针位置
 */
typedef int64_t (*SceAsfParserSeekCB)(void *userdata, void *buf, int offset, int unk, int flag);

/**
 * 检查内存，在初始化前调用
 *
 * @param asf asf处理器上下文
 * @return < 0 为错误, 0 为成功
 */
int sceAsfCheckNeedMem(SceAsfParser *asf);

/**
 * 初始化ASF Parser
 *
 * @param asf asf处理器上下文
 * @param userdata为read_cb, seek_cb的第一个参数
 * @param read_cb为读WMA文件回调函数
 * @param seek_cb为移动WMA文件指针回调函数
 *
 * @return < 0 为错误, 0 为成功
 * @return 0x86400171	非WMA文件
 */
int sceAsfInitParser(SceAsfParser *asf, void* userdata, SceAsfParserReadCB read_cb, SceAsfParserSeekCB seek_cb);

/**
 * 在Asf文件中跳转
 *
 * @param asf asf处理器上下文
 * @param unk 总是为1
 * @param ms 为指向跳转至播放长度毫秒数的指针
 *
 * @return < 0 为错误, 0 为成功
 */
int sceAsfSeekTime(SceAsfParser *asf, int unk, u32 *ms);

/**
 * 得到WMA Tag帧的开始位置与大小
 * 
 * @param asf asf处理器上下文
 * @param ptr 不明，可为NULL
 * @param stara 帧开始地址指针
 * @param size 帧大小指针
 * @param flag 标志
 * 
 * @note 当flags为
 * 0x00000001时: 不明
 * 0x00004000时: 得到标准tag信息
 * 0x00008000时: 得到扩展tag信息
 * 0x00040000时: 不明
 * 0x00080000时: 不明
 * @note 初始化asf_parser后调用
 */
int sceAsfParser_685E0DA7(SceAsfParser *asf, void *ptr, int flag, void *arg4, u64 *start, u64 *size);

/**
 * 读取帧数据
 *
 * @param unk 总是为1
 * @param frame 输出帧结构指针
 */
int sceAsfGetFrameData(SceAsfParser *asf, int unk, SceAsfFrame *frame);

/**
 * 得到WMA 文件信息帧的开始位置与大小
 * 
 * @param asf asf处理器上下文
 * @param unk 总是NULL
 * @param stara 帧开始地址指针
 * @param size 帧大小指针
 */
int sceAsfParser_C6D98C54(SceAsfParser *asf, void *unk, u64 *start, u64 *size);

#ifdef __cplusplus
}
#endif

#endif
