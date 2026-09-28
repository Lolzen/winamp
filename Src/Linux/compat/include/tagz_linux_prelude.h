/*
** Winamp for Linux: force-included when compiling Src/tagz.
**
** Pre-defines the include guard of Src/tagz/api__tagz.h so the Wasabi
** service/language headers are not pulled in, and supplies the localised
** strings tagz uses (from Src/tagz/tagz.rc).
*/
#pragma once
#define NULLSOFT_TAGZ_API_H
#include "win32_compat.h"
#ifdef __cplusplus
extern "C"
#endif
const wchar_t *tagz_linux_string(int id);
#define WASABI_API_LNGSTRINGW(id) ((wchar_t *)tagz_linux_string(id))
