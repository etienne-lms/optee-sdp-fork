/*
 * stub.c
 *
 * Copyright (C) Linaro SA 2015
 * Author: Benjamin Gaignard <benjamin.gaignard@linaro.org> for Linaro.
 * License terms: Lesser GNU General Public License (LGPL), version 2.1
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../sdp_platform_api.h"
#include "string_ext.h"

#define ARRAY_SIZE(x) (int)(sizeof(x) / sizeof(*(x)))

struct secure_device {
	const char name[MAX_NAME_SIZE];
	uint32_t id;
	void (*inc_refcount)(void);
	void (*dec_refcount)(void);
};

static int delta_refcount;
static int bdisp_refcount;
static int sti_refcount;
static int cpu_refcount;
static int tee_cpu_refcount;

static void delta_inc_refcount(void)
{
	delta_refcount++;
}

static void delta_dec_refcount(void)
{
	delta_refcount--;
}

static void bdisp_inc_refcount(void)
{
	bdisp_refcount++;
}

static void bdisp_dec_refcount(void)
{
	bdisp_refcount--;
}

static void sti_inc_refcount(void)
{
	sti_refcount++;
}

static void sti_dec_refcount(void)
{
	sti_refcount--;
}

static void cpu_inc_refcount(void)
{
	cpu_refcount++;
}

static void cpu_dec_refcount(void)
{
	cpu_refcount--;
}

static void tee_cpu_inc_refcount(void)
{
	tee_cpu_refcount++;
}

static void tee_cpu_dec_refcount(void)
{
	tee_cpu_refcount--;
}

struct secure_device stm_devices[] = {
	{ "delta" , DECODER | VIDEO,	 &delta_inc_refcount, &delta_dec_refcount },
	{ "bdisp" , TRANSFORMER | VIDEO, &bdisp_inc_refcount, &bdisp_dec_refcount },
	{ "sti"   , SINK | VIDEO,	 &sti_inc_refcount,   &sti_dec_refcount   },
	{ "cpu"   , CPU,		 &cpu_inc_refcount,   &cpu_dec_refcount   },
	{ "tee0"   , TEE_CPU,		 &tee_cpu_inc_refcount, &tee_cpu_dec_refcount   },
};

struct region {
	uint64_t addr;
	uint32_t size;
	uint32_t writer;
	uint32_t attached[4];
	uint32_t direction[4];
};

#define MAX_REGIONS 20
static struct region regions[MAX_REGIONS];

static int find_free_region(void)
{
	int i;

	for (i = 0; i < MAX_REGIONS; i++)
		if (regions[i].addr == 0)
			return i;

	return -1;
}

int platform_init(void)
{
	delta_refcount = 0;
	bdisp_refcount = 0;
	sti_refcount   = 0;

	memset(&regions, 0, sizeof(regions));

	return 0;
}

int platform_create_region(uint64_t addr, uint32_t size)
{
	int index = find_free_region();

	if (index < 0)
		return index;

	regions[index].addr = addr;
	regions[index].size = size;

	return index;
}

int platform_destroy_region(int index)
{
	if(index > MAX_REGIONS)
		return -1;

	memset(&regions[index], 0, sizeof(regions[index]));

	return 0;
}

struct region* platform_find_region_by_id(int index)
{
	if(index > MAX_REGIONS)
		return NULL;

	return &regions[index];
}

struct secure_device* platform_find_device_by_name(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stm_devices); i++) {
		if (!strcmp(stm_devices[i].name, name))
			return &stm_devices[i];
	}

	return NULL;
}

/* return 0 if we can add the device to the region */
int platform_check_permissions(struct region *region, struct secure_device* device, int dir)
{
	/*  Hack: TEE is allows to access any buffer */
	if (IS_TEE_CPU(device->id))
		return 0;

	if ((region->writer == 0) && (dir == DIR_WRITE))
		return 0;

	if ((region->writer == device->id) && (dir == DIR_WRITE))
		return 0;

	if (IS_CPU(region->writer) && (dir != DIR_WRITE) && !IS_CPU(device->id))
		return 0;

	if (IS_DECODER(region->writer) && (dir != DIR_WRITE) && IS_TRANSFORMER(device->id))
		if (STREAM_TYPE(region->writer) == STREAM_TYPE(device->id))
			return 0;

	if (IS_DECODER(region->writer) && (dir != DIR_WRITE) && IS_SINK(device->id))
		if (STREAM_TYPE(region->writer) == STREAM_TYPE(device->id))
			return 0;

	if (IS_TRANSFORMER(region->writer) && (dir != DIR_WRITE) && IS_SINK(device->id))
		if (STREAM_TYPE(region->writer) == STREAM_TYPE(device->id))
			return 0;

	IMSG("platform_check_permissions failed region->writer 0x%x dir %d device->id 0x%x\n", region->writer, dir, device->id);
	return 1;
}

int platform_add_device_to_region(struct region *region, struct secure_device* device, int dir)
{
	int i;

	if (dir == DIR_WRITE) {
		region->writer = device->id;
	}

	for (i = 0; i < ARRAY_SIZE(stm_devices); i++) {
		if (region->attached[i] == 0 || region->attached[i] == device->id) {
			region->attached[i]  = device->id;
			region->direction[i] = dir;
			goto inc_dev;
		}
	}

	return 1;

inc_dev:
	device->inc_refcount();

	return 0;
}

int platform_remove_device_from_region(struct region *region, struct secure_device* device)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stm_devices); i++) {
		if (region->attached[i] == device->id) {
			region->attached[i]  = 0;
			region->direction[i] = 0;
			goto dec_dev;
		}
	}

	return 1;

dec_dev:
	device->dec_refcount();
	return 0;
}

int platform_dump_status(char *dump, int size)
{
	int i, j, writed;
	char *tmp = dump;

	writed = snprintf(tmp, size, "SDP STUB platform\n");
	tmp += writed;
	size -= writed;

	for (i = 0; i < ARRAY_SIZE(stm_devices); i++) {
		writed = snprintf(tmp, size ,"device name %s id 0x%x\n", stm_devices[i].name, stm_devices[i].id);
		tmp += writed;
		size -= writed;
	}

	writed = snprintf(tmp, size, "delta (decoder) refcount %d\n", delta_refcount);
	tmp += writed;
	size -= writed;

	writed = snprintf(tmp, size, "bdisp (transform) refcount %d\n", bdisp_refcount);
	tmp += writed;
	size -= writed;

	writed = snprintf(tmp, size, "sti (display) refcount %d\n", sti_refcount);
	tmp += writed;
	size -= writed;

	for (i = 0; i < MAX_REGIONS; i++) {
		struct region *region = &regions[i];

		if (!region->addr)
			continue;

		writed = snprintf(tmp, size, "region addr 0x%x size %d writer 0x%x\n", (uint32_t)region->addr, region->size, region->writer);
		tmp += writed;
		size -= writed;

		j = 0;
		do {
			if (!region->attached[j]) {
				writed = snprintf(tmp, size, "attached 0x%x direction %d\n", region->attached[j], region->direction[j]);
				tmp += writed;
				size -= writed;
			}
		} while (++j < ARRAY_SIZE(stm_devices));
	}
	return 0;
}
