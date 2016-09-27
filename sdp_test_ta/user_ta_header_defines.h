/*
 * The name of this file must not be modified
 */

#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H

#include <user_ta_header.h>

#define TA_UUID			{ 0x12345678, 0x5b69, 0x11e4, \
				{ 0x9d, 0xbb, 0x10, 0x1f, 0x74, 0xf0, 0x00, 0x01 } }

#define TA_FLAGS                (TA_FLAG_USER_MODE | \
				TA_FLAG_EXEC_DDR | \
				TA_FLAG_UNSAFE_NW_PARAMS)

#define TA_STACK_SIZE           (2 * 1024)
#define TA_DATA_SIZE            (2 * 1024)

#define TA_CURRENT_TA_EXT_PROPERTIES \
    { "gp.ta.description", USER_TA_PROP_TYPE_STRING, "devkit tz sdp test TA" }, \
    { "gp.ta.version", USER_TA_PROP_TYPE_U32, &(const uint32_t){ 0x0100 } },

#endif
