#include <psptypes.h>
#include "msgresource.h"

#define NELEMS(a)       (sizeof (a) / sizeof ((a)[0]))

//#ifdef LANG_US

MsgResource msg[] = 
{
	{"�Ƿ��˳����?", EXIT_PROMPT},
	{"��", YES},
	{"��", NO},
	{"�밴��Ӧ����",  BUTTON_PRESS_PROMPT},
	{"��������   �� ɾ��", BUTTON_SETTING_PROMPT},
	{" | ", BUTTON_SETTING_SEP},
	{"��ǩ�˵�", BOOKMARK_MENU_KEY},
	{"  ��һҳ", PREV_PAGE_KEY},
	{"  ��һҳ", NEXT_PAGE_KEY},
	{" ǰ100��", PREV_100_LINE_KEY},
	{" ��100��", NEXT_100_LINE_KEY},
	{" ǰ500��", PREV_500_LINE_KEY},
	{" ��500��", NEXT_500_LINE_KEY},
	{"  ��һҳ", TO_START_KEY},
	{"���һҳ", TO_END_KEY},
	{"��һ�ļ�", PREV_FILE},
	{"��һ�ļ�", NEXT_FILE},
	{"�˳��Ķ�", EXIT_KEY},
	{"    ѡ��", FL_SELECTED},
	{"  ��һ��", FL_FIRST},
	{"���һ��", FL_LAST},
	{"�ϼ�Ŀ¼", FL_PARENT},
	{"ˢ���б�", FL_REFRESH},
	{"�ļ�����", FL_FILE_OPS},
	{"ѡ���ļ�", FL_SELECT_FILE},
	{"��ͼѡ��", IMG_OPT},
	{"��������", IMG_OPT_CUBE},
	{"��������", IMG_OPT_LINEAR},
	{"��", IMG_OPT_SECOND},
	{"    �����㷨", IMG_OPT_ALGO},
	{"�õƲ��ż��", IMG_OPT_DELAY},
	{"ͼƬ��ʼλ��", IMG_OPT_START_POS},
	{"    ���ٶ�", IMG_OPT_FLIP_SPEED},
	{"    ��ҳģʽ", IMG_OPT_FLIP_MODE},
	{"�����������", IMG_OPT_REVERSE_WIDTH},
	{"  ����ͼ�鿴", IMG_OPT_THUMB_VIEW},
	{"    ͼ������", IMG_OPT_BRIGHTNESS},
	{"��ɫѡ��", TEXT_COLOR_OPT},
	{"��֧��", NO_SUPPORT},
	{"������ɫ", FONT_COLOR},
	{"��", FONT_COLOR_RED},
	{"��", FONT_COLOR_GREEN},
	{"��", FONT_COLOR_BLUE},
	{"������ɫ", FONT_BACK_COLOR},
	{"�Ҷ�ɫ", FONT_COLOR_GRAY},
	{"  ����ͼ�Ҷ�", FONT_BACK_GRAY},
	{"�Ķ�ѡ��", READ_OPTION},
	{"����", LEFT_DIRECT},
	{"����", RIGHT_DIRECT},
	{"��", NONE_DIRECT},
	{"�ߵ�", REVERSAL_DIRECT},
	{"��ƪ����", NEXT_ARTICLE},
	{"�޶���", NO_ACTION},
	{"�ѹر�", AUTOPAGE_SHUTDOWN},
	{"      �ּ��", WORD_SPACE},
	{"      �м��", LINE_SPACE},
	
	{NULL,   UNKNOWN}
};

// #endif

const char* getmsgbyid(int id)
{
	MsgResource *head = &msg[0];
	int count = NELEMS(msg);
	while(count -- > 0) {
		if(id == head->id) {
			return head->msg;
		}
		head++;
	}

	return "δ֪��Ϣ";
}

void setmsglang(const char* langname)
{
	// dummy
	return;
}
