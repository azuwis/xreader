#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <psppower.h>
#include "scene.h"
#include "freq_lock.h"
#include "common/utils.h"
#include "power.h"
#include "conf.h"
#include "musicmgr.h"
#include "dbg.h"

struct _freq_lock_entry {
	unsigned short cpu, bus;
	int id;
};

typedef struct _freq_lock_entry freq_lock_entry;

static freq_lock_entry *freqs = NULL;
static int freqs_cnt = 0;
static int g_freq_sema;

int freq_init()
{
	g_freq_sema = sceKernelCreateSema("Freq Sema", 0, 1, 1, NULL);

	if (freqs_cnt != 0 || freqs != NULL) {
		free(freqs);
		freqs = NULL;
		freqs_cnt = 0;
	}

	return 0;
}

int freq_free()
{
	if (g_freq_sema >= 0) {
		sceKernelDeleteSema(g_freq_sema);
		g_freq_sema = -1;
	}

	return 0;
}

int freq_lock()
{
	return sceKernelWaitSemaCB(g_freq_sema, 1, NULL);
}

int freq_unlock()
{
	return sceKernelSignalSema(g_freq_sema, 1);
}

static int generate_id()
{
	int id;
	bool found;

	id = -1;

	do {
		int i;

		id++;
		found = false;

		for(i=0; i<freqs_cnt; ++i) {
			if (id == freqs[i].id) {
				found = true;
				break;
			}
		}
	} while (id < 0 || found == true);

	return id;
}

int freq_add(int cpu, int bus)
{
	int id;

	id = generate_id();

	if (freqs_cnt == 0) {
		freqs_cnt++;
		freqs = malloc(sizeof(freqs[0]));
		freqs[0].cpu = cpu;
		freqs[0].bus = bus;
		freqs[0].id = id;
	} else {
		freqs = safe_realloc(freqs, sizeof(freqs[0]) * (freqs_cnt + 1));

		if (freqs == NULL)
			return -1;

		freqs[freqs_cnt].cpu = cpu;
		freqs[freqs_cnt].bus = bus;
		freqs[freqs_cnt].id = id;
		freqs_cnt++;
	}

	return id;
}

static int dump_freq(void)
{
	char t[128] = { 0 };
	int i;

	sprintf(t, "[ ");

	for(i = 0; i < freqs_cnt; ++i) {
		sprintf(t + strlen(t), "%d/%d ", freqs[i].cpu, freqs[i].bus);
	}

	STRCAT_S(t, "]");

	dbg_printf(d, "%s: Dump freqs table: %s", __func__, t);

	return 0;
}

int dbg_freq()
{
	freq_lock();
	dump_freq();
	freq_unlock();

	return 0;
}

static int update_freq(void)
{
	int i, max, maxsum;
	int cpu = 0, bus = 0;

	if (freqs == NULL)
		return -1;

	for(i=0, max = 0, maxsum = -1; i<freqs_cnt; ++i) {
		if (maxsum < freqs[i].cpu + freqs[i].bus) {
			maxsum = freqs[i].cpu + freqs[i].bus;
			max = i;
		}
	}

	if (maxsum > 0) {
		cpu = freqs[max].cpu;
		bus = freqs[max].bus;
	}

	cpu = max(cpu, freq_list[config.freqs[0]][0]);
	bus = max(bus, freq_list[config.freqs[0]][1]);

//	dbg_printf(d, "%s: should set cpu/bus to %d/%d", __func__, cpu, bus);

	if (scePowerGetCpuClockFrequency() != cpu || scePowerGetBusClockFrequency() != bus) {
		power_set_clock(cpu, bus);
		{
//			dbg_printf(d, "%s: cpu: %d, bus: %d", __func__, scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency());
		}
	}

	return 0;
}

int freq_update(void)
{
	freq_lock();
	update_freq();
	freq_unlock();

	return 0;
}

int freq_enter(int cpu, int bus)
{
	int idx;

//	dbg_printf(d, "%s: enter %d/%d", __func__, cpu, bus);

	freq_lock();

	idx = freq_add(cpu, bus);

	if (idx < 0) {
		freq_unlock();
		return -1;
	}

	update_freq();

	freq_unlock();

	return idx;
}

int freq_leave(int freq_id)
{
//	dbg_printf(d, "%s: leave %d", __func__, freq_id);
	
	int idx;

	freq_lock();

	for(idx=0; idx<freqs_cnt; ++idx) {
		if (freqs[idx].id == freq_id) {
//			dbg_printf(d, "%s: removing freqs: %d/%d", __func__, freqs[idx].cpu, freqs[idx].bus);
			memmove(&freqs[idx], &freqs[idx+1], sizeof(freqs[0]) * (freqs_cnt - idx - 1));
			freqs = safe_realloc(freqs, sizeof(freqs[0]) * (freqs_cnt - 1));
			freqs_cnt--;
			break;
		}
	}

	update_freq();

	freq_unlock();

	return 0;
}

int freq_enter_hotzone(void)
{
	int cpu, bus;

	if (music_curr_playing()) {
		cpu = freq_list[config.freqs[2]][0];
		bus = freq_list[config.freqs[2]][1];
	} else {
		cpu = freq_list[config.freqs[1]][0];
		bus = freq_list[config.freqs[1]][1];
	}

	return freq_enter(cpu, bus);
}

