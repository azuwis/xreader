/*
 * $Id: Log.h 74 2007-10-20 10:46:39Z soarchin $

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

#include "datatype.h"

class Log
{
private:
	INT m_handle;
	DWORD m_flag;
public:
	enum LogFlag {
		ERROR = 1,
		DEBUG = 4
	};
	Log(CONST CHAR * filename, DWORD flag);
	~Log();
	void LogMsg(DWORD flag, CHAR * fmt, ...);
};

extern Log * g_Log;

#define LogV(fmt...) g_Log->LogMsg(Log::DEBUG,fmt)
