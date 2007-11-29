/*
 * $Id: App.h 78 2007-11-06 16:39:35Z soarchin $

 * Copyright 2007 aeolusc

 * This file is part of eReader2.

 * eReader2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * eReader2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Datatype.h"

class App
{
private:
	CHAR m_appDir[256];
	STATIC App * m_mainApp;
	STATIC INT nMainThread(INT args, VOID *argp);
	STATIC INT CallbackThread(INT args, VOID *argp);
	STATIC INT nExitCallback(INT arg1, INT arg2, VOID *arg);
	STATIC INT nPowerCallback(INT arg1, INT arg2, VOID *arg);
protected:
	virtual BOOL Init(INT argc, CHAR * argv[]) {return TRUE;}
	virtual INT MainThread(INT args, VOID *argp) = 0;
	virtual INT ExitCallback(INT arg1, INT arg2) {Exit(); return 0;}
	virtual INT PowerCallback(INT arg1, INT arg2) {return 0;}
public:
	App();
	virtual ~App();
	STATIC App * GetMainApp() {return m_mainApp;}
	VOID Start(INT argc, CHAR * argv[]);
	CONST CHAR * GetPath();
	CHAR * GetRelativePath(CONST CHAR * path, CHAR * destPath);
	VOID Exit();
};

#define APP_GLOBAL(TYPE,NAME) STATIC TYPE NAME;

#if _PSP_FW_VERSION >= 200

#define APP_START(AppName,AppVer,AppBuild) PSP_MODULE_INFO(AppName, 0, AppVer, AppBuild);\
PSP_MAIN_THREAD_ATTR(0);\
PSP_HEAP_SIZE_KB(-1);\
class AppName: public App\
{\
protected:

#define APP_END(AppName) };\
AppName mainApp;\
INT main(INT argc, CHAR *argv[])\
{\
	mainApp.Start(argc, argv);\
	return 0;\
}

#else

#define APP_START(AppName,AppVer,AppBuild) PSP_MODULE_INFO(AppName, 0x1000, AppVer, AppBuild);\
PSP_MAIN_THREAD_ATTR(0);\
class AppName: public App\
{\
protected:

#define APP_END(AppName) };\
AppName mainApp;\
INT main(INT argc, CHAR *argv[])\
{\
	mainApp.Start(argc, argv);\
	return 0;\
}

#endif
