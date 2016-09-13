/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "ta_sdp.h"

#define PAGE_SHIFT			12

#define ROUNDUP(v, size) (((v) + ((size) - 1)) & ~((size) - 1))
#define ROUNDDOWN(v, size) ((v) & ~((size) - 1))

/* Define to indicate default pool initiation */
#define TEE_MM_POOL_NO_FLAGS            0
/* Flag to indicate that memory is allocated from hi address to low address */
#define TEE_MM_POOL_HI_ALLOC            (1u << 0)

struct _tee_mm_entry_t {
	struct _tee_mm_pool_t *pool;
	struct _tee_mm_entry_t *next;
	uint32_t offset;	/* offset in pages/sections */
	uint32_t size;		/* size in pages/sections */
};
typedef struct _tee_mm_entry_t tee_mm_entry_t;

struct _tee_mm_pool_t {
	tee_mm_entry_t *entry;
	uint32_t lo;		/* low boundery pf the pool */
	uint32_t hi;		/* high boundery pf the pool */
	uint32_t flags;		/* Config flags for the pool */
	uint8_t shift;		/* size shift */
	size_t max_allocated;
};
typedef struct _tee_mm_pool_t tee_mm_pool_t;

static void tee_mm_init(tee_mm_pool_t **pool_ref, uint32_t lo, uint32_t hi, uint8_t shift,
		 uint32_t flags)
{
	tee_mm_pool_t *pool;

	pool = (tee_mm_pool_t *)TEE_Malloc(sizeof(tee_mm_pool_t), 0);
	if (!pool)
		TEE_Panic(0);

	lo = ROUNDUP(lo, 1 << shift);
	hi = ROUNDDOWN(hi, 1 << shift);
	pool->lo = lo;
	pool->hi = hi;
	pool->shift = shift;
	pool->flags = flags;
	pool->entry = TEE_Malloc(sizeof(tee_mm_entry_t), 0);

	if (!pool->entry)
		TEE_Panic(0);

	pool->entry->pool = pool;
	*pool_ref = pool;
}

static tee_mm_entry_t *tee_mm_add(tee_mm_entry_t *p)
{
	/* add to list */
	if (p->next == NULL) {
		p->next = TEE_Malloc(sizeof(tee_mm_entry_t), 0);
		if (p->next == NULL)
			return NULL;
		p->next->next = NULL;
	} else {
		tee_mm_entry_t *nn = TEE_Malloc(sizeof(tee_mm_entry_t), 0);

		if (nn == NULL)
			return NULL;
		nn->next = p->next;
		p->next = nn;
	}
	return p->next;
}

static size_t tee_mm_stats_allocated(tee_mm_pool_t *pool)
{
	tee_mm_entry_t *entry;
	uint32_t sz = 0;

	if (!pool)
		return 0;

	entry = pool->entry;
	while (entry) {
		sz += entry->size;
		entry = entry->next;
	}

	return sz << pool->shift;
}

static __unused void tee_mm_get_pool_stats(tee_mm_pool_t *pool)
{
	IMSG("SMAF secure memory allocation: %dkB / %dkB (%dkB)",
			tee_mm_stats_allocated(pool) / 1024,
			(pool->hi - pool->lo) / 1024,
			pool->max_allocated / 1024);
}

static void update_max_allocated(tee_mm_pool_t *pool)
{
	size_t sz = tee_mm_stats_allocated(pool);

	if (sz > pool->max_allocated)
		pool->max_allocated = sz;
}

static tee_mm_entry_t *tee_mm_alloc(tee_mm_pool_t *pool, uint32_t size)
{
	uint32_t psize;
	tee_mm_entry_t *entry;
	tee_mm_entry_t *nn;
	uint32_t remaining;

	/* Check that pool is initialized */
	if (!pool || !pool->entry)
		return NULL;

	entry = pool->entry;
	if (size == 0)
		psize = 0;
	else
		psize = ((size - 1) >> pool->shift) + 1;
	/* Protect with mutex (multi thread) */

	/* find free slot */
	if (pool->flags & TEE_MM_POOL_HI_ALLOC) {
		while (entry->next != NULL && psize >
		       (entry->offset - entry->next->offset -
			entry->next->size))
			entry = entry->next;
	} else {
		while (entry->next != NULL && psize >
		       (entry->next->offset - entry->size - entry->offset))
			entry = entry->next;
	}

	/* check if we have enough memory */
	if (entry->next == NULL) {
		if (pool->flags & TEE_MM_POOL_HI_ALLOC) {
			/*
			 * entry->offset is a "block count" offset from
			 * pool->lo. The byte offset is
			 * (entry->offset << pool->shift).
			 * In the HI_ALLOC allocation scheme the memory is
			 * allocated from the end of the segment, thus to
			 * validate there is sufficient memory validate that
			 * (entry->offset << pool->shift) > size.
			 */
			if ((entry->offset << pool->shift) < size)
				/* out of memory */
				return NULL;
		} else {
			if (pool->hi <= pool->lo)
				TEE_Panic(0);

			remaining = (pool->hi - pool->lo);
			remaining -= ((entry->offset + entry->size) <<
				      pool->shift);

			if (remaining < size)
				/* out of memory */
				return NULL;
		}
	}

	nn = tee_mm_add(entry);
	if (nn == NULL)
		return NULL;

	if (pool->flags & TEE_MM_POOL_HI_ALLOC)
		nn->offset = entry->offset - psize;
	else
		nn->offset = entry->offset + entry->size;
	nn->size = psize;
	nn->pool = pool;

	update_max_allocated(pool);

	/* Protect with mutex end (multi thread) */

	return nn;
}

static void tee_mm_free(tee_mm_entry_t *p)
{
	tee_mm_entry_t *entry;

	if (!p || !p->pool)
		return;

	entry = p->pool->entry;

	/* Protect with mutex (multi thread) */

	/* remove entry from list */
	while (entry->next != NULL && entry->next != p)
		entry = entry->next;

	if (!entry->next)
		TEE_Panic(0);

	entry->next = entry->next->next;

	TEE_Free(p);

	/* Protect with mutex end (multi thread) */
}

static size_t tee_mm_get_bytes(const tee_mm_entry_t *mm)
{
	if (!mm || !mm->pool)
		return 0;
	else
		return mm->size << mm->pool->shift;
}

static __unused bool tee_mm_addr_is_within_range(tee_mm_pool_t *pool, uint32_t addr)
{
	return (pool && ((addr >= pool->lo) && (addr <= pool->hi)));
}

static __unused bool tee_mm_is_empty(tee_mm_pool_t *pool)
{
	return pool == NULL || pool->entry == NULL || pool->entry->next == NULL;
}

static tee_mm_entry_t *tee_mm_find(const tee_mm_pool_t *pool, uint32_t addr)
{
	tee_mm_entry_t *entry = pool->entry;
	uint16_t offset = (addr - pool->lo) >> pool->shift;

	if (addr > pool->hi || addr < pool->lo)
		return NULL;

	while (entry->next != NULL) {
		entry = entry->next;

		if ((offset >= entry->offset) &&
		    (offset < (entry->offset + entry->size))) {
			return entry;
		}
	}

	return NULL;
}

static uintptr_t tee_mm_get_smem(const tee_mm_entry_t *mm)
{
	return (mm->offset << mm->pool->shift) + mm->pool->lo;
}

static tee_mm_pool_t *pool;

TEE_Result allocate_buffer(uint32_t types, TEE_Param *params);
TEE_Result free_buffer(uint32_t param_types, TEE_Param *params);

TEE_Result free_buffer(uint32_t types, TEE_Param params[4])
{
	if (TEE_PARAM_TYPE_GET(types, 0) != TEE_PARAM_TYPE_VALUE_INPUT)
		return TEE_ERROR_BAD_PARAMETERS;

	if (pool)
		tee_mm_free(tee_mm_find(pool, params[0].value.a));

	IMSG("freed 0x%x", params[0].value.b);
	return TEE_SUCCESS;
}

TEE_Result allocate_buffer(uint32_t types, TEE_Param params[4])
{
	tee_mm_entry_t *mm;

	if (TEE_PARAM_TYPE_GET(types, 0) != TEE_PARAM_TYPE_VALUE_INOUT)
		return TEE_ERROR_BAD_PARAMETERS;

	if (!pool)
		tee_mm_init(&pool,
			    CFG_SMAF_MEMORY_POOL_BASE,
			    CFG_SMAF_MEMORY_POOL_END,
			    PAGE_SHIFT, 0);

	mm = tee_mm_alloc(pool, params[0].value.b);
	if (!mm)
		return TEE_ERROR_OUT_OF_MEMORY;

	params[0].value.a = tee_mm_get_smem(mm);
	params[0].value.b = tee_mm_get_bytes(mm);

	IMSG("allocated 0x%x 0x%x", params[0].value.a, params[0].value.b);
	return TEE_SUCCESS;
}
