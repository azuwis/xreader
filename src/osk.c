#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <psputility.h>
#include <psputility_osk.h>
#include <pspgu.h>
#include "display.h"
#include "pspscreen.h"
#include "strsafe.h"
#include "common/datatype.h"
#include "conf.h"

extern t_conf config;

static void ClearScreen()
{
	extern unsigned int __attribute__ ((aligned(16))) list[262144];

	sceGuStart(GU_DIRECT, list);
	sceGuClearColor(config.bgcolor);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish();
}

int get_osk_input(char *buf, int size)
{
	char tstr[128] = { 0 };
	unsigned short intext[128] = { 0 };	// text already in the edit box on start
	unsigned short outtext[128] = { 0 };	// text after input
	unsigned short desc[128] = { 'E', 'n', 't', 'e', 'r', ' ', 'G', 'I', 0 };	// description

	SceUtilityOskData data;

	memset(&data, 0, sizeof(data));
	data.language = 2;			// key glyphs: 0-1=hiragana, 2+=western
	data.lines = 1;				// just one line
	data.unk_24 = 1;			// set to 1
	data.desc = desc;
	data.intext = intext;
	data.outtextlength = 128;	// sizeof(outtext) / sizeof(unsigned short)
	data.outtextlimit = 50;		// just allow 50 chars
	data.outtext = (unsigned short *) outtext;

	SceUtilityOskParams osk;

	memset(&osk, 0, sizeof(osk));
	osk.base.size = sizeof(osk);
	// dialog language: 0=Japanese, 1=English, 2=French, 3=Spanish, 4=German,
	// 5=Italian, 6=Dutch, 7=Portuguese, 8=Russian, 9=Korean, 10-11=Chinese, 12+=default
	osk.base.language = 12;
	osk.base.buttonSwap = 0;
	osk.base.graphicsThread = 37;	// gfx thread pri
	osk.base.accessThread = 49;	// unknown thread pri (?)
	osk.base.fontThread = 38;
	osk.base.soundThread = 36;
	osk.unk_48 = 1;
	osk.data = &data;

	int rc = sceUtilityOskInitStart(&osk);

	if (rc) {
		return 0;
	}

	bool done = false;

	while (!done) {
		ClearScreen();
		switch (sceUtilityOskGetStatus()) {
			case PSP_UTILITY_DIALOG_INIT:
				break;
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityOskUpdate(2);	// 2 is taken from ps2dev.org recommendation
				break;
			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityOskShutdownStart();
				break;
			case PSP_UTILITY_DIALOG_FINISHED:
				done = true;
				break;
			case PSP_UTILITY_DIALOG_NONE:
			default:
				break;
		}

		int i, j = 0;

		for (i = 0; data.outtext[i]; i++) {

			if (data.outtext[i] != '\0' && data.outtext[i] != '\n'
				&& data.outtext[i] != '\r') {
				tstr[j] = data.outtext[i];
				j++;
			}
		}

		strcpy_s(buf, size, tstr);
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	sceDisplayWaitVblankStart();
	void *buffer = sceGuSwapBuffers();

	disp_fix_osk(buffer);

	return 1;
}
