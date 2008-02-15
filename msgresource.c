/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include <psptypes.h>
#include <stdlib.h>
#include "msgresource.h"

#define NELEMS(a)       (sizeof (a) / sizeof ((a)[0]))

//#ifdef LANG_US

MsgResource msg[] = {
	{"是否退出软件?", EXIT_PROMPT}
	,
	{"是", YES}
	,
	{"否", NO}
	,
	{"请按对应按键", BUTTON_PRESS_PROMPT}
	,
	{"按键设置   △ 删除", BUTTON_SETTING_PROMPT}
	,
	{" | ", BUTTON_SETTING_SEP}
	,
	{"书签菜单", BOOKMARK_MENU_KEY}
	,
	{"  上一页", PREV_PAGE_KEY}
	,
	{"  下一页", NEXT_PAGE_KEY}
	,
	{" 前100行", PREV_100_LINE_KEY}
	,
	{" 后100行", NEXT_100_LINE_KEY}
	,
	{" 前500行", PREV_500_LINE_KEY}
	,
	{" 后500行", NEXT_500_LINE_KEY}
	,
	{"  第一页", TO_START_KEY}
	,
	{"最后一页", TO_END_KEY}
	,
	{"上一文件", PREV_FILE}
	,
	{"下一文件", NEXT_FILE}
	,
	{"退出阅读", EXIT_KEY}
	,
	{"    选定", FL_SELECTED}
	,
	{"  第一项", FL_FIRST}
	,
	{"最后一项", FL_LAST}
	,
	{"上级目录", FL_PARENT}
	,
	{"刷新列表", FL_REFRESH}
	,
	{"文件操作", FL_FILE_OPS}
	,
	{"选择文件", FL_SELECT_FILE}
	,
	{"看图选项", IMG_OPT}
	,
	{"三次立方", IMG_OPT_CUBE}
	,
	{"两次线性", IMG_OPT_LINEAR}
	,
	{"秒", IMG_OPT_SECOND}
	,
	{"    缩放算法", IMG_OPT_ALGO}
	,
	{"幻灯播放间隔", IMG_OPT_DELAY}
	,
	{"图片起始位置", IMG_OPT_START_POS}
	,
	{"    卷动速度", IMG_OPT_FLIP_SPEED}
	,
	{"    翻页模式", IMG_OPT_FLIP_MODE}
	,
	{"翻动保留宽度", IMG_OPT_REVERSE_WIDTH}
	,
	{"  缩略图查看", IMG_OPT_THUMB_VIEW}
	,
	{"    图像亮度", IMG_OPT_BRIGHTNESS}
	,
	{"颜色选项", TEXT_COLOR_OPT}
	,
	{"不支持", NO_SUPPORT}
	,
	{"字体颜色", FONT_COLOR}
	,
	{"红", FONT_COLOR_RED}
	,
	{"绿", FONT_COLOR_GREEN}
	,
	{"蓝", FONT_COLOR_BLUE}
	,
	{"背景颜色", FONT_BACK_COLOR}
	,
	{"灰度色", FONT_COLOR_GRAY}
	,
	{"  背景图灰度", FONT_BACK_GRAY}
	,
	{"阅读选项", READ_OPTION}
	,
	{"左向", LEFT_DIRECT}
	,
	{"右向", RIGHT_DIRECT}
	,
	{"无", NONE_DIRECT}
	,
	{"颠倒", REVERSAL_DIRECT}
	,
	{"下篇文章", NEXT_ARTICLE}
	,
	{"无动作", NO_ACTION}
	,
	{"已关闭", HAVE_SHUTDOWN}
	,
	{"      字间距", WORD_SPACE}
	,
	{"      行间距", LINE_SPACE}
	,
	{"    保留边距", REVERSE_SPAN_SPACE}
	,
	{"      信息栏", INFO_BAR_DISPLAY}
	,
	{"翻页保留一行", REVERSE_ONE_LINE_WHEN_PAGE_DOWN}
	,
	{"    文字竖看", TEXT_DIRECTION}
	,
	{"    文字编码", TEXT_ENCODING}
	,
	{"      滚动条", SCROLLBAR}
	,
	{"自动保存书签", AUTOSAVE_BOOKMARK}
	,
	{"重新编排文本", TEXT_REALIGNMENT}
	,
	{"文章末尾翻页", TEXT_TAIL_PAGE_DOWN}
	,
	{"    翻页时间", AUTOPAGE_DELAY}
	,
	{"    滚屏时间", AUTOLINE_TIME}
	,
	{"    滚屏速度", AUTOLINE_STEP}
	,
	{"    线控模式", LINE_CTRL_MODE}
	,
	{"操作设置", OPERATION_SETTING}
	,
	{"控制翻页", CONTROL_PAGE}
	,
	{"控制音乐", CONTROL_MUSIC}
	,
	{"字体设置", FONT_SETTING}
	,
	{"菜单字体大小", MENU_FONT_SIZE}
	,
	{"阅读字体大小", BOOK_FONT_SIZE}
	,
	{" 使用TTF字体", TTF_FONT_SIZE}
	,
	{"    自动休眠", AUTO_SLEEP}
	,
	{"切换翻页", SWITCH_PAGE}
	,
	{"  启用类比键", ENABLE_ANALOG_KEY}
	,
	{"自动翻页模式", AUTOPAGE_MODE}
	,
	{"音乐设置", MUSIC_SETTING}
	,
	{"自动开始播放", AUTO_START_PLAYBACK}
	,
	{"歌词显示行数", LYRIC_DISPLAY_LINENUM}
	,
	{"歌词显示编码", LYRIC_DISPLAY_ENCODE}
	,
	{"系统选项", SYSTEM_OPTION}
	,
	{"显示", DISPLAY_OPTION}
	,
	{"隐藏", HIDDEN_OPTION}
	,
	{"设置选项", SETTING_OPTION}
	,
	{"查看EXIF信息", VIEW_EXIF}
	,
	{NULL, UNKNOWN}
};

// #endif

static int searchid(const void *key, const void *p)
{
	return (int) key - ((MsgResource *) p)->id;
}

const char *getmsgbyid(int id)
{
	MsgResource *p = (MsgResource *) bsearch((void *) id, msg, NELEMS(msg),
											 sizeof(MsgResource), &searchid);

	return p ? p->msg : "未知信息";
}

void setmsglang(const char *langname)
{
	// dummy
	return;
}
