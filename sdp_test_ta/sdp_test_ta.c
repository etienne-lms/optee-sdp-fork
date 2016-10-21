/*
 * Copyright (c) 2016, Linaro Limited
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

#define CMD_INJECT	1
#define CMD_TRANSFORM	2
#define CMD_DUMP	3

static TEE_Result inject(uint32_t types, TEE_Param params[4])
{
	TEE_Result rc;
	int sec_idx;
	int ns_idx;

	/* strict on parameter types */
	switch (types) {
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE):
		ns_idx = 0;
		sec_idx = 1;
		break;
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE):
		sec_idx = 0;
		ns_idx = 1;
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[sec_idx].memref.size < params[ns_idx].memref.size)
		return TEE_ERROR_SHORT_BUFFER;

	/* strict on memory access attributes */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[ns_idx].memref.buffer,
					 params[ns_idx].memref.size);
	if (rc != TEE_SUCCESS)
		return rc;

	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_WRITE |
					 TEE_MEMORY_ACCESS_SECURE,
					 params[sec_idx].memref.buffer,
					 params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS)
		return rc;

#ifdef CFG_CACHE_API
	/*
	 * cache invalidationis required, but flush is fine and
	 * prevents corrupting cache line for unaligned buffer.
	 */
	rc = TEE_CacheFlush(params[sec_idx].memref.buffer,
			    params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	/* inject data */
	TEE_MemMove(params[sec_idx].memref.buffer,
		    params[ns_idx].memref.buffer,
		    params[sec_idx].memref.size);

#ifdef CFG_CACHE_API
	/* flush data to physical memory */
	rc = TEE_CacheFlush(params[sec_idx].memref.buffer,
			    params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	return rc;
}

static TEE_Result transform(uint32_t types, TEE_Param params[4])
{
	TEE_Result rc;
	unsigned char *p;
	size_t sz;

	/* strict on parameter types */
	switch (types) {
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
			     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* strict on memory access attributes */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_WRITE |
					 TEE_MEMORY_ACCESS_SECURE,
					 params[0].memref.buffer,
					 params[0].memref.size);
	if (rc != TEE_SUCCESS)
		return rc;

#ifdef CFG_CACHE_API
	/*
	 * cache invalidationis required, but flush is fine and
	 * prevents corrupting cache line for unaligned buffer.
	 */
	rc = TEE_CacheFlush(params[0].memref.buffer,
			    params[0].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	/* transform the data */
	p = (unsigned char *)params[0].memref.buffer;
	sz = params[0].memref.size;
	for (; sz; sz--, p++)
			*p = ~(*p) + 1;

#ifdef CFG_CACHE_API
	/* flush data to physical memory */
	rc = TEE_CacheFlush(params[0].memref.buffer,
			    params[0].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	return rc;
}

static TEE_Result dump(uint32_t types, TEE_Param params[4])
{
	TEE_Result rc;
	int sec_idx;
	int ns_idx;

	/* strict on parameter types */
	switch (types) {
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE):
		sec_idx = 0;
		ns_idx = 1;
		break;
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE):
		sec_idx = 1;
		ns_idx = 0;
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[sec_idx].memref.size < params[ns_idx].memref.size)
		return TEE_ERROR_SHORT_BUFFER;

	/* strict on memory access attributes */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_WRITE |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[ns_idx].memref.buffer,
					 params[ns_idx].memref.size);
	if (rc != TEE_SUCCESS)
		return rc;

	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_SECURE,
					 params[sec_idx].memref.buffer,
					 params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS)
		return rc;

#ifdef CFG_CACHE_API
	/*
	 * cache invalidationis required, but flush is fine and
	 * prevents corrupting cache line for unaligned buffer.
	 */
	rc = TEE_CacheFlush(params[sec_idx].memref.buffer,
			    params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	/* dump the data */
	TEE_MemMove(params[ns_idx].memref.buffer,
		    params[sec_idx].memref.buffer,
		    params[sec_idx].memref.size);

#ifdef CFG_CACHE_API
	/* flush data to physical memory */
	rc = TEE_CacheFlush(params[sec_idx].memref.buffer,
			    params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush() failed: 0x%x", rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	return rc;
}

TEE_Result TA_CreateEntryPoint(void)
{
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t types, TEE_Param params[4],
						void **sess)
{
	(void)types;
	(void) params;
	(void)sess;
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *ctx)
{
	(void)ctx;
}

TEE_Result TA_InvokeCommandEntryPoint(void *sess __unused,
		uint32_t cmd, uint32_t types, TEE_Param params[4])
{
	switch (cmd) {
	case CMD_INJECT:
		return inject(types, params);
	case CMD_TRANSFORM:
		return transform(types, params);
	case CMD_DUMP:
		return dump(types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
