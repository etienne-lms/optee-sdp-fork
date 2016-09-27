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

TEE_Result TA_InvokeCommandEntryPoint(void *sess, uint32_t cmd, uint32_t types,
						TEE_Param params[4])
{
	(void)sess;
	(void)cmd;

	switch(types) {
	/* injecting data to secure buffer */
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_MEMREF_SECURE,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
		if (params[1].memref.size < params[0].memref.size)
			return TEE_ERROR_SHORT_BUFFER;

		TEE_MemMove(params[1].memref.buffer,
			    params[0].memref.buffer,
			    params[0].memref.size);
		// TODO: flush cache for param[1] vmem range
		break;
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_SECURE,
			     TEE_PARAM_TYPE_MEMREF_INPUT,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
		if (params[0].memref.size < params[1].memref.size)
			return TEE_ERROR_SHORT_BUFFER;

		TEE_MemMove(params[0].memref.buffer,
			    params[1].memref.buffer,
			    params[1].memref.size);
		// TODO: flush cache for param[0] vmem range
		break;

	/* dumping data from secure buffer */
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_MEMREF_SECURE,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
		if (params[0].memref.size < params[1].memref.size)
			return TEE_ERROR_SHORT_BUFFER;

		TEE_MemMove(params[0].memref.buffer,
			    params[1].memref.buffer,
			    params[1].memref.size);
		break;
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_SECURE,
			     TEE_PARAM_TYPE_MEMREF_OUTPUT,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
		if (params[1].memref.size < params[0].memref.size)
			return TEE_ERROR_SHORT_BUFFER;

		TEE_MemMove(params[1].memref.buffer,
			    params[0].memref.buffer,
			    params[0].memref.size);
		break;

	/* modify secure buffer content */
	case TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_SECURE,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE,
			     TEE_PARAM_TYPE_NONE):
	{
		unsigned char *p = (unsigned char *)params[0].memref.buffer;
		size_t sz = params[0].memref.size;

		// TODO: flush cache in param[0]

		for (; sz; sz--, p++)
			*p = ~(*p) + 1;

		// TODO: flush cache in param[0]
		break;
	}

	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}
