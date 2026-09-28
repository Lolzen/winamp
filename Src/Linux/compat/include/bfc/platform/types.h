/* Winamp for Linux: minimal stand-in for Src/Wasabi/bfc/platform/types.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../../win32_compat.h"
#ifdef __cplusplus
static inline bool operator==(const GUID &a, const GUID &b) { return !memcmp(&a, &b, sizeof(GUID)); }
static inline bool operator!=(const GUID &a, const GUID &b) { return !!memcmp(&a, &b, sizeof(GUID)); }
#endif
