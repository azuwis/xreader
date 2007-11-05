#include <psptypes.h>
#include "msgresource.h"

#define NELEMS(a)       (sizeof (a) / sizeof ((a)[0]))

//#ifdef LANG_US

MsgResource msg[] = 
{
	{"是否退出软件?", EXIT_PROMPT},
	{"是", YES},
	{"否", NO},
	{"请按对应按键",  BUTTON_PRESS_PROMPT},
	{"按键设置   △ 删除", BUTTON_SETTING_PROMPT},
	{" | ", BUTTON_SETTING_SEP},
	{"书签菜单", BOOKMARK_MENU_KEY},
	{"  上一页", PREV_PAGE_KEY},
	{"  下一页", NEXT_PAGE_KEY},
	{" 前100行", PREV_100_LINE_KEY},
	{" 后100行", NEXT_100_LINE_KEY},
	{" 前500行", PREV_500_LINE_KEY},
	{" 后500行", NEXT_500_LINE_KEY},
	{"  第一页", TO_START_KEY},
	{"最后一页", TO_END_KEY},
	{"上一文件", PREV_FILE},
	{"下一文件", NEXT_FILE},
	{"退出阅读", EXIT_KEY},
	{"    选定", FL_SELECTED},
	{"  第一项", FL_FIRST},
	{"最后一项", FL_LAST},
	{"上级目录", FL_PARENT},
	{"刷新列表", FL_REFRESH},
	{"文件操作", FL_FILE_OPS},
	{"选择文件", FL_SELECT_FILE},
	{"看图选项", IMG_OPT},
	{"三次立方", IMG_OPT_CUBE},
	{"两次线性", IMG_OPT_LINEAR},
	{"秒", IMG_OPT_SECOND},
	{"    缩放算法", IMG_OPT_ALGO},
	{"幻灯播放间隔", IMG_OPT_DELAY},
	{"图片起始位置", IMG_OPT_START_POS},
	{"    卷动速度", IMG_OPT_FLIP_SPEED},
	{"    翻页模式", IMG_OPT_FLIP_MODE},
	{"翻动保留宽度", IMG_OPT_REVERSE_WIDTH},
	{"  缩略图查看", IMG_OPT_THUMB_VIEW},
	{"    图像亮度", IMG_OPT_BRIGHTNESS},
	{"颜色选项", TEXT_COLOR_OPT},
	{"不支持", NO_SUPPORT},
	{"字体颜色", FONT_COLOR},
	{"红", FONT_COLOR_RED},
	{"绿", FONT_COLOR_GREEN},
	{"蓝", FONT_COLOR_BLUE},
	{"背景颜色", FONT_BACK_COLOR},
	{"灰度色", FONT_COLOR_GRAY},
	{"  背景图灰度", FONT_BACK_GRAY},
	{"阅读选项", READ_OPTION},
	{"左向", LEFT_DIRECT},
	{"右向", RIGHT_DIRECT},
	{"无", NONE_DIRECT},
	{"颠倒", REVERSAL_DIRECT},
	{"下篇文章", NEXT_ARTICLE},
	{"无动作", NO_ACTION},
	{"已关闭", AUTOPAGE_SHUTDOWN},
	{"      字间距", WORD_SPACE},
	{"      行间距", LINE_SPACE},
	
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

	return "未知信息";
}

void setmsglang(const char* langname)
{
	// dummy
	return;
}
