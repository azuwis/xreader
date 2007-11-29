/*
 * $Id: Thread.h 74 2007-10-20 10:46:39Z soarchin $

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

class Thread
{
private:
	INT m_ID;
	BOOL m_succ;
	INT m_sema;
	STATIC INT nThreadEntry(INT arglen, VOID * argp);
	typedef struct {
		Thread * entry;
		INT param1;
		VOID * param2;
		BOOL fot;
	} ThreadParam;
	ThreadParam nThreadParam;
protected:
	virtual INT ThreadEntry(INT, VOID *) = 0;
public:
	Thread(BOOL freeOnTerm, CONST CHAR * name, INT priority = 0x10, INT stackSize = 0x10000, UINT attr = 0x80000000);
	virtual ~Thread();
	BOOL CreateSucc();
	VOID Start(INT arglen, VOID * argp);
	VOID Terminate();
	VOID Suspend();
	VOID Resume();
	VOID Wakeup();
	INT GetID() CONST;
	VOID WaitEnd(DWORD timeout);
	VOID WaitEndAsCB(DWORD timeout);
	VOID ChangePriority(INT priority);
	VOID WaitSignal(INT signal = 1);
	VOID SetSignal(INT signal = 1);

	STATIC VOID Exit();
	STATIC VOID Sleep();
	STATIC VOID SleepAsCB();
	STATIC VOID Delay(DWORD microSec);
	STATIC INT GetCurrentID();
	STATIC INT GetCurrentPriority();
};
