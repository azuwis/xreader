#ifndef __SCELIBASFPARSER_H__
#define __SCELIBASFPARSER_H__

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _SceAsfFrame {
	void *data;		// ֡����ָ��
	u32 ms; 		// ��ǰ֡ʱ�������
	u32 unk1;		// 0~7������
	u32 unk2;		// �ܿ�����֡��С
	u32 unk3;		// ��֡��ʼ��һ֡Ϊ1,������Ϊ0
	u32 unk4;		// 128 256 512 1024 2048�ȵȣ�������֡���ͱ�־���е�WMA�ļ�һֱ�ȶ���һ��ֵ���е�WMA�ļ������仯������ͬVBR���
	u32 unk5;		// 128 256 512 1024 2048�ȵȣ�������֡���ͱ�־���е�WMA�ļ�һֱ�ȶ���һ��ֵ���е�WMA�ļ������仯������ͬVBR���
	u32 unk6;		// ��ͬWMA�ļ�����ȵ�����ֵ��ͬһ��WMA�ļ���һ�����ȶ���
	u8 unk_bytes[32];
} SceAsfFrame_500;

#define SceAsfFrame SceAsfFrame_500

/// Asf���������������ͣ�����64λ����
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
 * ��ΪͬsceIoRead
 */
typedef int64_t (*SceAsfParserReadCB)(void *userdata, void *buf, SceSize size);

/**
 * ��flag = 1ʱΪ��Ե�ǰλ���ƶ�offset�ֽ�
 * ��flag = 0ʱΪ���Ե�ַ�ƶ�offset�ֽ�
 * ���ص�ǰ�ļ�ָ��λ��
 */
typedef int64_t (*SceAsfParserSeekCB)(void *userdata, void *buf, int offset, int unk, int flag);

/**
 * ����ڴ棬�ڳ�ʼ��ǰ����
 *
 * @param asf asf������������
 * @return < 0 Ϊ����, 0 Ϊ�ɹ�
 */
int sceAsfCheckNeedMem(SceAsfParser *asf);

/**
 * ��ʼ��ASF Parser
 *
 * @param asf asf������������
 * @param userdataΪread_cb, seek_cb�ĵ�һ������
 * @param read_cbΪ��WMA�ļ��ص�����
 * @param seek_cbΪ�ƶ�WMA�ļ�ָ��ص�����
 *
 * @return < 0 Ϊ����, 0 Ϊ�ɹ�
 * @return 0x86400171	��WMA�ļ�
 */
int sceAsfInitParser(SceAsfParser *asf, void* userdata, SceAsfParserReadCB read_cb, SceAsfParserSeekCB seek_cb);

/**
 * ��Asf�ļ�����ת
 *
 * @param asf asf������������
 * @param unk ����Ϊ1
 * @param ms Ϊָ����ת�����ų��Ⱥ�������ָ��
 *
 * @return < 0 Ϊ����, 0 Ϊ�ɹ�
 */
int sceAsfSeekTime(SceAsfParser *asf, int unk, u32 *ms);

/**
 * �õ�WMA Tag֡�Ŀ�ʼλ�����С
 * 
 * @param asf asf������������
 * @param ptr ��������ΪNULL
 * @param stara ֡��ʼ��ַָ��
 * @param size ֡��Сָ��
 * @param flag ��־
 * 
 * @note ��flagsΪ
 * 0x00000001ʱ: ����
 * 0x00004000ʱ: �õ���׼tag��Ϣ
 * 0x00008000ʱ: �õ���չtag��Ϣ
 * 0x00040000ʱ: ����
 * 0x00080000ʱ: ����
 * @note ��ʼ��asf_parser�����
 */
int sceAsfParser_685E0DA7(SceAsfParser *asf, void *ptr, int flag, void *arg4, u64 *start, u64 *size);

/**
 * ��ȡ֡����
 *
 * @param unk ����Ϊ1
 * @param frame ���֡�ṹָ��
 */
int sceAsfGetFrameData(SceAsfParser *asf, int unk, SceAsfFrame *frame);

/**
 * �õ�WMA �ļ���Ϣ֡�Ŀ�ʼλ�����С
 * 
 * @param asf asf������������
 * @param unk ����NULL
 * @param stara ֡��ʼ��ַָ��
 * @param size ֡��Сָ��
 */
int sceAsfParser_C6D98C54(SceAsfParser *asf, void *unk, u64 *start, u64 *size);

#ifdef __cplusplus
}
#endif

#endif
