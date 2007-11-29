/*
 * $Id: Thread.cpp 74 2007-10-20 10:46:39Z soarchin $

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

#include "Thread.h"
#include <pspkernel.h>

Thread::Thread(BOOL freeOnTerm, CONST CHAR * name, INT priority, INT stackSize, UINT attr)
{
	m_sema = -1;
	nThreadParam.entry = this;
	nThreadParam.fot = freeOnTerm;
	m_ID = sceKernelCreateThread(name, (SceKernelThreadEntry)nThreadEntry, priority, stackSize, attr, NULL);
	m_succ = (m_ID >= 0);
	if(m_succ)
		m_sema = sceKernelCreateSema(name, 0, 1, 1, NULL);
}

Thread::~Thread()
{
	if(m_sema >= 0)
	{
		sceKernelDeleteSema(m_sema);
		m_sema = -1;
	}
	Wakeup();
	Resume();
	Terminate();
}

BOOL Thread::CreateSucc()
{
	return m_succ;
}

VOID Thread::Start(INT arglen, VOID * argp)
{
	nThreadParam.param1 = arglen;
	nThreadParam.param2 = argp;
	sceKernelStartThread(m_ID, sizeof(ThreadParam), &nThreadParam);
}

VOID Thread::Exit()
{
	sceKernelExitDeleteThread(0);
}

VOID Thread::Sleep()
{
    sceKernelSleepThread();
}

VOID Thread::SleepAsCB()
{
    sceKernelSleepThreadCB();
}

VOID Thread::Terminate()
{
	sceKernelTerminateThread(m_ID);
}

VOID Thread::Suspend()
{
	sceKernelSuspendThread(m_ID);
}

VOID Thread::Resume()
{
	sceKernelResumeThread(m_ID);
}

VOID Thread::Delay(DWORD microSec)
{
	sceKernelDelayThread(microSec);
}

INT Thread::GetID() CONST
{
	return m_ID;
}

INT Thread::GetCurrentID()
{
	return sceKernelGetThreadId();
}

VOID Thread::Wakeup()
{
	sceKernelWakeupThread(m_ID);
}

VOID Thread::WaitEnd(DWORD timeout)
{
	SceUInt sto = timeout;
	if(timeout != INVALID)
		sceKernelWaitThreadEnd(m_ID, &sto);
	else
		sceKernelWaitThreadEnd(m_ID, NULL);
}

VOID Thread::WaitEndAsCB(DWORD timeout)
{
	SceUInt sto = timeout;
	if(timeout != INVALID)
		sceKernelWaitThreadEndCB(m_ID, &sto);
	else
		sceKernelWaitThreadEndCB(m_ID, NULL);
}

VOID Thread::ChangePriority(INT priority)
{
	sceKernelChangeThreadPriority(m_ID, priority);
}

INT Thread::GetCurrentPriority()
{
	return sceKernelGetThreadCurrentPriority();
}

VOID Thread::WaitSignal(INT signal)
{
	sceKernelWaitSema(m_sema, signal, NULL);
}

VOID Thread::SetSignal(INT signal)
{
	sceKernelSignalSema(m_sema, signal);
}

INT Thread::nThreadEntry(INT arglen, VOID * argp)
{
	ThreadParam * params = (ThreadParam *)argp;
	INT result = params->entry->ThreadEntry(params->param1, params->param2);
	if(params->fot)
		delete params->entry;
	return result;
}
