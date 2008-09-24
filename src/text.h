#ifndef _TEXT_H_
#define _TEXT_H_

#include "common/datatype.h"
#include "fs.h"

typedef struct
{
	char *start;
	dword count;
	dword GI;
} t_textrow, *p_textrow;

typedef struct
{
	/** �ļ�·���� */
	char filename[PATH_MAX];

	dword crow;

	dword size;

	char *buf;

	/** �ı���Unicode����ģʽ
	 *
	 * @note 0 - ��Unicode����
	 * @note 1 - UCS
	 * @note 2 - UTF-8
	 */
	int ucs;

	/** ���� */
	dword row_count;

	/** �нṹָ������ */
	p_textrow rows[1024];
} t_text, *p_text;

/**
 * ��ʽ���ı�
 *
 * @note ����ʾģʽ���ı����Ϊ��
 *
 * @param txt ������ṹָ��
 * @param max_pixels �����ʾ��ȣ������ؼ�
 * @param wordspace �ּ��
 * @param ttf_mode �Ƿ�ΪTTF��ʾģʽ
 *
 * @return �Ƿ�ɹ�
 */
extern bool text_format(p_text txt, dword max_pixels, dword wordspace,
						bool ttf_mode);

/**
 * ���ڴ���һ��������Ϊ�ı�
 *
 * @note �ڴ����ݽ�������
 *
 * @param filename ����;
 * @param data
 * @param size
 * @param ft �ļ�����
 * @param max_pixels �����ʾ��ȣ������ؼ�
 * @param wordspace �ּ��
 * @param encode �ı���������
 * @param reorder �ı��Ƿ����±���
 *
 * @return �µĵ�����ṹָ��
 */
extern p_text text_open_in_raw(const char *filename, const unsigned char *data,
							   size_t size, t_fs_filetype ft, dword max_pixels,
							   dword wordspace, t_conf_encode encode,
							   bool reorder);

/**
 * ���ı��ļ��������ڵ����ļ�
 * 
 * @note ֧��GZ,CHM,RAR,ZIP�Լ�TXT
 *
 * @param filename �ļ�·��
 * @param archname ����·��
 * @param filetype �ı��ļ�����
 * @param max_pixels �����ʾ��ȣ������ؼ� 
 * @param wordspace �־�
 * @param encode �ı��ļ�����
 * @param reorder �Ƿ����±���
 * @param where ��������
 * @param vertread ��ʾ��ʽ
 *
 * @return �µĵ�����ṹָ��
 * - NULL ʧ��
 */
extern p_text text_open_archive(const char *filename,
								const char *archname,
								t_fs_filetype filetype,
								dword max_pixels,
								dword wordspace,
								t_conf_encode encode,
								bool reorder, int where, int vertread);

extern p_text chapter_open_in_umd(const char *umdfile, const char *chaptername,
								  u_int index, dword rowpixels, dword wordspace,
								  t_conf_encode encode, bool reorder);

/**
 * �ر��ı�
 *
 * @param fstext
 */
extern void text_close(p_text fstext);

/*
 * �õ��ı���ǰ�е���ʾ���ȣ������ؼƣ�ϵͳ�˵�ʹ��
 *
 * @param pos �ı���ʼλ��
 * @param size �ļ���С�����ֽڼ�
 * @param wordspace �ּ��
 *
 * @return ��ʾ���ȣ������ؼ�
 */
int text_get_string_width_sys(const byte * pos, size_t size, dword wordspace);

/**
 * �Ƿ�Ϊ���ܽض��ַ�
 *
 * @note ����ʾӢ���жϵ����Ƿ�Ӧ�����õ�
 *
 * @param ch �ַ�
 *
 * @return �Ƿ�Ϊ���ܽض��ַ�
 */
bool is_untruncateable_chars(char ch);

/**
 * �ֽڱ����ݱ��ֵ�õ�״̬
 *
 * @note 0 - �ַ����ػ���
 * @note 1 - �ַ�ʹ���н���
 * @note 2 - �ַ�Ϊ�ո�TAB
 */
extern bool bytetable[256];

#endif
