/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef XREADER_HAL_H
#define XREADER_HAL_H

#include <pspsdk.h>
#include <pspkernel.h>
#include <psptypes.h>
#include <pspgu.h>
#include <pspaudio.h>
#include <pspaudiocodec.h>
#include <pspdisplay.h>
#include <psphprm.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <psputility.h>
#include <psputility_osk.h>
#include "pspasfparser.h"

static inline SceUID xrIoOpen(const char *file, int flags, SceMode mode)
{
	return sceIoOpen(file, flags, mode);
}

static inline int xrIoClose(SceUID fd)
{
	return sceIoClose(fd);
}

static inline int xrIoRead(SceUID fd, void *data, SceSize size)
{
	return sceIoRead(fd, data, size);
}

static inline int xrIoWrite(SceUID fd, const void *data, SceSize size)
{
	return sceIoWrite(fd, data, size);
}

static inline SceOff xrIoLseek(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek(fd, offset, whence);
}

static inline SceOff xrIoLseek32(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek32(fd, offset, whence);
}

static inline int xrIoGetstat(const char *file, SceIoStat * stat)
{
	return sceIoGetstat(file, stat);
}

static inline int xrIoMkdir(const char *dir, SceMode mode)
{
	return sceIoMkdir(dir, mode);
}

static inline int xrIoRmdir(const char *path)
{
	return sceIoRmdir(path);
}

static inline SceUID xrIoDopen(const char *dirname)
{
	return sceIoDopen(dirname);
}

static inline int xrIoDread(SceUID fd, SceIoDirent * dir)
{
	return sceIoDread(fd, dir);
}

static inline int xrIoDclose(SceUID fd)
{
	return sceIoDclose(fd);
}

static inline int xrIoChangeAsyncPriority(SceUID fd, int pri)
{
	return sceIoChangeAsyncPriority(fd, pri);
}

static inline int xrIoReadAsync(SceUID fd, void *data, SceSize size)
{
	return sceIoReadAsync(fd, data, size);
}

static inline int xrIoWriteAsync(SceUID fd, const void *data, SceSize size)
{
	return sceIoWriteAsync(fd, data, size);
}

static inline int xrIoWaitAsync(SceUID fd, SceInt64 * res)
{
	return sceIoWaitAsync(fd, res);
}

static inline int xrIoChstat(const char *file, SceIoStat * stat, int bits)
{
	return sceIoChstat(file, stat, bits);
}

static inline int xrIoRemove(const char *file)
{
	return sceIoRemove(file);
}

static inline void xrGuAmbientColor(unsigned int color)
{
	sceGuAmbientColor(color);
}

static inline void xrGuClear(int flags)
{
	sceGuClear(flags);
}

static inline void xrGuClearColor(unsigned int color)
{
	sceGuClearColor(color);
}

static inline void xrGuDepthBuffer(void *zbp, int zbw)
{
	sceGuDepthBuffer(zbp, zbw);
}

static inline void xrGuDepthRange(int near, int far)
{
	sceGuDepthRange(near, far);
}

static inline void xrGuDisable(int state)
{
	sceGuDisable(state);
}

static inline
	void xrGuDispBuffer(int width, int height, void *dispbp, int dispbw)
{
	sceGuDispBuffer(width, height, dispbp, dispbw);
}

static inline int xrGuDisplay(int state)
{
	return sceGuDisplay(state);
}

static inline
	void xrGuDrawArray(int prim, int vtype, int count, const void *indices,
					   const void *vertices)
{
	sceGuDrawArray(prim, vtype, count, indices, vertices);
}

static inline void xrGuDrawBuffer(int psm, void *fbp, int fbw)
{
	sceGuDrawBuffer(psm, fbp, fbw);
}

static inline void xrGuEnable(int state)
{
	sceGuEnable(state);
}

static inline int xrGuFinish(void)
{
	return sceGuFinish();
}

static inline void xrGuFrontFace(int order)
{
	sceGuFrontFace(order);
}

static inline void *xrGuGetMemory(int size)
{
	return sceGuGetMemory(size);
}

static inline void xrGuInit(void)
{
	sceGuInit();
}

static inline void xrGuOffset(unsigned int x, unsigned int y)
{
	return sceGuOffset(x, y);
}

static inline void xrGuScissor(int x, int y, int w, int h)
{
	sceGuScissor(x, y, w, h);
}

static inline void xrGuShadeModel(int mode)
{
	sceGuShadeModel(mode);
}

static inline void xrGuStart(int cid, void *list)
{
	sceGuStart(cid, list);
}

static inline void *xrGuSwapBuffers(void)
{
	return sceGuSwapBuffers();
}

static inline int xrGuSync(int mode, int what)
{
	return sceGuSync(mode, what);
}

static inline void xrGuTerm(void)
{
	sceGuTerm();
}

static inline void xrGuTexFilter(int min, int mag)
{
	sceGuTexFilter(min, mag);
}

static inline void xrGuTexFunc(int tfx, int tcc)
{
	sceGuTexFunc(tfx, tcc);
}

static inline
	void xrGuTexImage(int mipmap, int width, int height, int tbw,
					  const void *tbp)
{
	sceGuTexImage(mipmap, width, height, tbw, tbp);
}

static inline void xrGuTexMode(int tpsm, int maxmips, int a2, int swizzle)
{
	sceGuTexMode(tpsm, maxmips, a2, swizzle);
}

static inline void xrGuViewport(int cx, int cy, int width, int height)
{
	sceGuViewport(cx, cy, width, height);
}

static inline int xrKernelDelayThread(SceUInt delay)
{
	return sceKernelDelayThread(delay);
}

static inline int xrKernelDevkitVersion(void)
{
	return sceKernelDevkitVersion();
}

static inline
	SceUID xrKernelCreateSema(const char *name, SceUInt attr, int initVal,
							  int maxVal, SceKernelSemaOptParam * option)
{
	return sceKernelCreateSema(name, attr, initVal, maxVal, option);
}

static inline int xrKernelDeleteSema(SceUID semaid)
{
	return sceKernelDeleteSema(semaid);
}

static inline
	SceUID xrKernelCreateThread(const char *name, SceKernelThreadEntry entry,
								int initPriority, int stackSize, SceUInt attr,
								SceKernelThreadOptParam * option)
{
	return sceKernelCreateThread(name, entry, initPriority, stackSize, attr,
								 option);
}

static inline int xrKernelStartThread(SceUID thid, SceSize arglen, void *argp)
{
	return sceKernelStartThread(thid, arglen, argp);
}

static inline int xrKernelExitDeleteThread(int status)
{
	return sceKernelExitDeleteThread(status);
}

static inline int xrKernelWaitThreadEnd(SceUID thid, SceUInt * timeout)
{
	return sceKernelWaitThreadEnd(thid, timeout);
}

static inline int xrKernelTerminateDeleteThread(SceUID thid)
{
	return sceKernelTerminateDeleteThread(thid);
}

static inline int xrKernelDeleteThread(SceUID thid)
{
	return sceKernelDeleteThread(thid);
}

static inline int xrKernelWaitSema(SceUID semaid, int signal, SceUInt * timeout)
{
	return sceKernelWaitSema(semaid, signal, timeout);
}

static inline int xrKernelSignalSema(SceUID semaid, int signal)
{
	return sceKernelSignalSema(semaid, signal);
}

static inline int xrKernelWaitThreadEndCB(SceUID thid, SceUInt * timeout)
{
	return sceKernelWaitThreadEndCB(thid, timeout);
}

static inline int xrKernelSetEventFlag(SceUID evid, u32 bits)
{
	return sceKernelSetEventFlag(evid, bits);
}

static inline int xrKernelWaitEventFlag(SceUID evid, u32 bits, u32 wait,
										u32 * outBits, SceUInt * timeout)
{
	return sceKernelWaitEventFlag(evid, bits, wait, outBits, timeout);
}

static inline SceUID xrKernelCreateEventFlag(const char *name, int attr,
											 int bits,
											 SceKernelEventFlagOptParam * opt)
{
	return sceKernelCreateEventFlag(name, attr, bits, opt);
}

static inline int xrKernelDeleteEventFlag(int evid)
{
	return sceKernelDeleteEventFlag(evid);
}

static inline
	int xrKernelStartModule(SceUID modid, SceSize argsize, void *argp,
							int *status, SceKernelSMOption * option)
{
	return sceKernelStartModule(modid, argsize, argp, status, option);
}

static inline
	SceUID xrKernelLoadModule(const char *path, int flags,
							  SceKernelLMOption * option)
{
	return sceKernelLoadModule(path, flags, option);
}

static inline
	int xrKernelStopModule(SceUID modid,
						   SceSize argsize,
						   void *argp, int *status, SceKernelSMOption * option)
{
	return sceKernelStopModule(modid, argsize, argp, status, option);
}

static inline int xrKernelUnloadModule(SceUID modid)
{
	return sceKernelUnloadModule(modid);
}

static inline void xrKernelExitGame(void)
{
	sceKernelExitGame();
}

static inline int xrKernelExitThread(int status)
{
	return sceKernelExitThread(status);
}

static inline int xrKernelSleepThread(void)
{
	return sceKernelSleepThread();
}

static inline int xrKernelSleepThreadCB(void)
{
	return sceKernelSleepThreadCB();
}

static inline
	int xrKernelWaitSemaCB(SceUID semaid, int signal, SceUInt * timeout)
{
	return sceKernelWaitSemaCB(semaid, signal, timeout);
}

static inline
	int xrKernelCreateCallback(const char *name, SceKernelCallbackFunction func,
							   void *arg)
{
	return sceKernelCreateCallback(name, func, arg);
}

static inline int xrKernelRegisterExitCallback(int cbid)
{
	return sceKernelRegisterExitCallback(cbid);
}

static inline int xrAudioOutput2GetRestSample(void)
{
	return sceAudioOutput2GetRestSample();
}

static inline int xrAudioSRCChRelease(void)
{
	return sceAudioSRCChRelease();
}

static inline int xrAudioSRCChReserve(int samplecount, int freq, int channels)
{
	return sceAudioSRCChReserve(samplecount, freq, channels);
}

static inline int xrAudioSRCOutputBlocking(int vol, void *buf)
{
	return sceAudioSRCOutputBlocking(vol, buf);
}

static inline int xrAudioSetChannelDataLen(int channel, int samplecount)
{
	return sceAudioSetChannelDataLen(channel, samplecount);
}

static inline int xrAudiocodecCheckNeedMem(unsigned long *Buffer, int Type)
{
	return sceAudiocodecCheckNeedMem(Buffer, Type);
}

static inline int xrAudiocodecDecode(unsigned long *Buffer, int Type)
{
	return sceAudiocodecDecode(Buffer, Type);
}

static inline int xrAudiocodecGetEDRAM(unsigned long *Buffer, int Type)
{
	return sceAudiocodecGetEDRAM(Buffer, Type);
}

static inline int xrAudiocodecInit(unsigned long *Buffer, int Type)
{
	return sceAudiocodecInit(Buffer, Type);
}

static inline int xrAudiocodecReleaseEDRAM(unsigned long *Buffer)
{
	return sceAudiocodecReleaseEDRAM(Buffer);
}

static inline int xrCtrlReadBufferPositive(SceCtrlData * pad_data, int count)
{
	return sceCtrlReadBufferPositive(pad_data, count);
}

static inline int xrCtrlSetSamplingCycle(int cycle)
{
	return sceCtrlSetSamplingCycle(cycle);
}

static inline int xrCtrlSetSamplingMode(int mode)
{
	return sceCtrlSetSamplingMode(mode);
}

static inline
	int xrDisplaySetFrameBuf(void *topaddr, int bufferwidth, int pixelformat,
							 int sync)
{
	return sceDisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync);
}

static inline int xrDisplaySetMode(int mode, int width, int height)
{
	return sceDisplaySetMode(mode, width, height);
}

static inline int xrDisplayWaitVblankStart(void)
{
	return sceDisplayWaitVblankStart();
}

static inline int xrHprmIsRemoteExist(void)
{
	return sceHprmIsRemoteExist();
}

static inline int xrHprmPeekCurrentKey(u32 * key)
{
	return sceHprmPeekCurrentKey(key);
}

static inline int xrPowerGetBatteryChargingStatus(void)
{
	return scePowerGetBatteryChargingStatus();
}

static inline int xrPowerGetBatteryLifePercent(void)
{
	return scePowerGetBatteryLifePercent();
}

static inline int xrPowerGetBatteryLifeTime(void)
{
	return scePowerGetBatteryLifeTime();
}

static inline int xrPowerGetBatteryTemp(void)
{
	return scePowerGetBatteryTemp();
}

static inline int xrPowerGetBatteryVolt(void)
{
	return scePowerGetBatteryVolt();
}

static inline int xrPowerGetBusClockFrequency(void)
{
	return scePowerGetBusClockFrequency();
}

static inline int xrPowerGetCpuClockFrequency(void)
{
	return scePowerGetCpuClockFrequency();
}

static inline int xrPowerRegisterCallback(int slot, SceUID cbid)
{
	return scePowerRegisterCallback(slot, cbid);
}

static inline int xrPowerRequestSuspend(void)
{
	return scePowerRequestSuspend();
}

static inline int xrPowerSetBusClockFrequency(int busfreq)
{
	return scePowerSetBusClockFrequency(busfreq);
}

static inline
	int xrPowerSetClockFrequency(int pllfreq, int cpufreq, int busfreq)
{
	return scePowerSetClockFrequency(pllfreq, cpufreq, busfreq);
}

static inline int xrPowerSetCpuClockFrequency(int cpufreq)
{
	return scePowerSetCpuClockFrequency(cpufreq);
}

static inline int xrPowerTick(int type)
{
	return scePowerTick(type);
}

static inline int xrPowerUnregisterCallback(int slot)
{
	return scePowerUnregisterCallback(slot);
}

static inline int xrRtcGetCurrentClockLocalTime(pspTime * time)
{
	return sceRtcGetCurrentClockLocalTime(time);
}

static inline int xrRtcGetCurrentTick(u64 * tick)
{
	return sceRtcGetCurrentTick(tick);
}

static inline int xrRtcGetDayOfWeek(int year, int month, int day)
{
	return sceRtcGetDayOfWeek(year, month, day);
}

static inline int xrRtcGetDaysInMonth(int year, int month)
{
	return sceRtcGetDaysInMonth(year, month);
}

static inline u32 xrRtcGetTickResolution()
{
	return sceRtcGetTickResolution();
}

static inline int xrUsbActivate(u32 pid)
{
	return sceUsbActivate(pid);
}

static inline int xrUsbDeactivate(u32 pid)
{
	return sceUsbDeactivate(pid);
}

static inline int xrUsbGetState(void)
{
	return sceUsbGetState();
}

static inline int xrUsbStart(const char *driverName, int size, void *args)
{
	return sceUsbStart(driverName, size, args);
}

static inline int xrUsbStop(const char *driverName, int size, void *args)
{
	return sceUsbStop(driverName, size, args);
}

static inline int xrUsbstorBootSetCapacity(u32 size)
{
	return sceUsbstorBootSetCapacity(size);
}

static inline int xrUtilityLoadAvModule(int module)
{
	return sceUtilityLoadAvModule(module);
}

static inline int xrUtilityOskGetStatus(void)
{
	return sceUtilityOskGetStatus();
}

static inline int xrUtilityOskInitStart(SceUtilityOskParams * params)
{
	return sceUtilityOskInitStart(params);
}

static inline int xrUtilityOskShutdownStart(void)
{
	return sceUtilityOskShutdownStart();
}

static inline int xrUtilityOskUpdate(int n)
{
	return sceUtilityOskUpdate(n);
}

static inline int xrAudioChRelease(int channel)
{
	return sceAudioChRelease(channel);
}

static inline int xrAudioChReserve(int channel, int samplecount, int format)
{
	return sceAudioChReserve(channel, samplecount, format);
}

static inline
	int xrAudioOutputPannedBlocking(int channel, int leftvol, int rightvol,
									void *buf)
{
	return sceAudioOutputPannedBlocking(channel, leftvol, rightvol, buf);
}

static inline int xrAsfSeekTime(SceAsfParser * asf, int unk, SceUInt32 * ms)
{
	return sceAsfSeekTime(asf, unk, ms);
}

static inline int xrAsfParser_C6D98C54(SceAsfParser * asf, void *arg2,
									   u64 * start, u64 * offset)
{
	return sceAsfParser_C6D98C54(asf, arg2, start, offset);
}

static inline int xrAsfCheckNeedMem(SceAsfParser * asf)
{
	return sceAsfCheckNeedMem(asf);
}

static inline int xrAsfGetFrameData(SceAsfParser * asf, int unk,
									SceAsfFrame * frame)
{
	return sceAsfGetFrameData(asf, unk, frame);
}

static inline int xrAsfInitParser(SceAsfParser * asf, void *userdata,
								  SceAsfParserReadCB read_cb,
								  SceAsfParserSeekCB seek_cb)
{
	return sceAsfInitParser(asf, userdata, read_cb, seek_cb);
}

static inline int xrAsfParser_685E0DA7(SceAsfParser * asf, void *ptr, int flag,
									   void *arg4, u64 * start, u64 * offset)
{
	return sceAsfParser_685E0DA7(asf, ptr, flag, arg4, start, offset);
}

static inline int xrKernelGetThreadId(void)
{
	return sceKernelGetThreadId();
}

static inline int xrKernelGetThreadCurrentPriority(void)
{
	return sceKernelGetThreadCurrentPriority();
}

static inline int xrKernelChangeThreadPriority(SceUID thid, int priority)
{
	return sceKernelChangeThreadPriority(thid, priority);
}

#endif
