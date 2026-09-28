/*
** Winamp for Linux - tiny Win32 compatibility layer.
**
** This is NOT a general Windows emulation. It only provides the handful of
** types and string/path helpers used by the platform-neutral parts of the
** original code base (currently Src/tagz) so those sources can be compiled
** unmodified on Linux. It is only on the include path of those targets.
**
** TCHAR is wchar_t (the Windows build is UNICODE); on Linux wchar_t is UTF-32,
** so CharNext()/CharPrev() are simple pointer steps.
*/
#ifndef WINAMP_LINUX_WIN32_COMPAT_H
#define WINAMP_LINUX_WIN32_COMPAT_H

#include <wchar.h>
#include <wctype.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifndef WINAPI
#define WINAPI
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef CALLBACK
#define CALLBACK
#endif

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef uint32_t LCID;
typedef wchar_t WCHAR;
typedef wchar_t TCHAR;
typedef wchar_t *LPTSTR, *LPWSTR;
typedef const wchar_t *LPCTSTR, *LPCWSTR;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef void *HANDLE;
typedef void *HINSTANCE;
typedef long HRESULT;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define TEXT(x) L##x
#define _T(x) L##x

#define S_OK ((HRESULT)0)
#define STRSAFE_E_INSUFFICIENT_BUFFER ((HRESULT)0x8007007AL)

typedef struct _GUID
{
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} GUID;

typedef struct _SYSTEMTIME
{
	WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;

static inline void GetLocalTime(SYSTEMTIME *st)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm t;
	localtime_r(&ts.tv_sec, &t);
	st->wYear = (WORD)(t.tm_year + 1900);
	st->wMonth = (WORD)(t.tm_mon + 1);
	st->wDayOfWeek = (WORD)t.tm_wday;
	st->wDay = (WORD)t.tm_mday;
	st->wHour = (WORD)t.tm_hour;
	st->wMinute = (WORD)t.tm_min;
	st->wSecond = (WORD)t.tm_sec;
	st->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

static inline DWORD GetLastError(void) { return (DWORD)errno; }

static inline int MulDiv(int a, int b, int c)
{
	return c ? (int)(((int64_t)a * b) / c) : -1;
}

/* ---- characters ---- */
static inline wchar_t *CharNextW(const wchar_t *p) { return (wchar_t *)(*p ? p + 1 : p); }
static inline wchar_t *CharPrevW(const wchar_t *start, const wchar_t *p) { return (wchar_t *)(p > start ? p - 1 : start); }
static inline wchar_t *CharLowerW(wchar_t *s) { for (wchar_t *p = s; *p; p++) *p = towlower(*p); return s; }
static inline wchar_t *CharUpperW(wchar_t *s) { for (wchar_t *p = s; *p; p++) *p = towupper(*p); return s; }
static inline DWORD CharUpperBuffW(wchar_t *s, DWORD n) { for (DWORD i = 0; i < n; i++) s[i] = towupper(s[i]); return n; }
#define CharNext CharNextW
#define CharPrev CharPrevW
#define CharLower CharLowerW
#define CharUpper CharUpperW
#define CharUpperBuff CharUpperBuffW

#define LOCALE_USER_DEFAULT 0x0400
#define NORM_IGNORECASE 0x00000001
#define NORM_IGNOREWIDTH 0x00020000
#define CSTR_LESS_THAN 1
#define CSTR_EQUAL 2
#define CSTR_GREATER_THAN 3

static inline int CompareStringW(LCID lcid, DWORD flags, const wchar_t *a, int na, const wchar_t *b, int nb)
{
	(void)lcid;
	size_t la = na < 0 ? wcslen(a) : (size_t)na;
	size_t lb = nb < 0 ? wcslen(b) : (size_t)nb;
	size_t n = la < lb ? la : lb;
	for (size_t i = 0; i < n; i++)
	{
		wint_t ca = a[i], cb = b[i];
		if (flags & NORM_IGNORECASE) { ca = towlower(ca); cb = towlower(cb); }
		if (ca != cb) return ca < cb ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
	}
	if (la == lb) return CSTR_EQUAL;
	return la < lb ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
}
#define CompareString CompareStringW

#define CT_CTYPE1 1
#define C1_UPPER 0x0001
#define C1_LOWER 0x0002
#define C1_DIGIT 0x0004
#define C1_SPACE 0x0008
#define C1_PUNCT 0x0010
#define C1_CNTRL 0x0020
#define C1_BLANK 0x0040
#define C1_XDIGIT 0x0080
#define C1_ALPHA 0x0100

static inline BOOL GetStringTypeExW(LCID lcid, DWORD type, const wchar_t *s, int n, WORD *out)
{
	(void)lcid; (void)type;
	if (n < 0) n = (int)wcslen(s) + 1;
	for (int i = 0; i < n; i++)
	{
		wint_t c = s[i];
		WORD t = 0;
		if (iswupper(c)) t |= C1_UPPER;
		if (iswlower(c)) t |= C1_LOWER;
		if (iswdigit(c)) t |= C1_DIGIT;
		if (iswspace(c)) t |= C1_SPACE;
		if (iswpunct(c)) t |= C1_PUNCT;
		if (iswcntrl(c)) t |= C1_CNTRL;
		if (c == L' ' || c == L'\t') t |= C1_BLANK;
		if (iswxdigit(c)) t |= C1_XDIGIT;
		if (iswalpha(c)) t |= C1_ALPHA;
		out[i] = t;
	}
	return TRUE;
}
#define GetStringTypeEx GetStringTypeExW

#define CP_ACP 0
#define CP_UTF8 65001

/* Only UTF-8 and UCS-4 (code page 12000) are needed. Output is wchar_t (UTF-32). */
static inline int MultiByteToWideChar(UINT cp, DWORD flags, const char *src, int srclen, wchar_t *dst, int dstlen)
{
	(void)flags;
	int out = 0;
	if (cp == 12000)
	{
		int count = srclen / 4;
		for (int i = 0; i < count; i++)
		{
			uint32_t c;
			memcpy(&c, src + i * 4, 4);
			if (dst && out < dstlen) dst[out] = (wchar_t)c;
			out++;
		}
		return out;
	}
	mbstate_t st;
	memset(&st, 0, sizeof(st));
	size_t len = srclen < 0 ? strlen(src) + 1 : (size_t)srclen;
	size_t i = 0;
	while (i < len)
	{
		unsigned char c = (unsigned char)src[i];
		uint32_t cp32;
		int extra;
		if (c < 0x80) { cp32 = c; extra = 0; }
		else if ((c & 0xE0) == 0xC0) { cp32 = c & 0x1F; extra = 1; }
		else if ((c & 0xF0) == 0xE0) { cp32 = c & 0x0F; extra = 2; }
		else if ((c & 0xF8) == 0xF0) { cp32 = c & 0x07; extra = 3; }
		else { cp32 = 0xFFFD; extra = 0; }
		i++;
		for (int e = 0; e < extra && i < len; e++, i++)
			cp32 = (cp32 << 6) | ((unsigned char)src[i] & 0x3F);
		if (dst && out < dstlen) dst[out] = (wchar_t)cp32;
		out++;
	}
	return out;
}

/* ---- strings ---- */
static inline int _wtoi(const wchar_t *s) { return (int)wcstol(s, 0, 10); }
static inline int _wcsicmp(const wchar_t *a, const wchar_t *b) { return wcscasecmp(a, b); }
static inline int _wcsnicmp(const wchar_t *a, const wchar_t *b, size_t n) { return wcsncasecmp(a, b, n); }
static inline wchar_t *_wcsdup(const wchar_t *s) { return wcsdup(s); }
static inline wchar_t *lstrcpynW(wchar_t *dst, const wchar_t *src, int n)
{
	if (n <= 0) return dst;
	int i = 0;
	for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
	dst[i] = 0;
	return dst;
}
#define lstrcpyn lstrcpynW
static inline int lstrlenW(const wchar_t *s) { return s ? (int)wcslen(s) : 0; }
#define lstrlen lstrlenW

static inline HRESULT StringCchVPrintfW(wchar_t *dst, size_t cch, const wchar_t *fmt, va_list ap)
{
	if (!cch) return STRSAFE_E_INSUFFICIENT_BUFFER;
	int r = vswprintf(dst, cch, fmt, ap);
	if (r < 0) { dst[cch - 1] = 0; return STRSAFE_E_INSUFFICIENT_BUFFER; }
	return S_OK;
}

static inline HRESULT StringCchPrintfW(wchar_t *dst, size_t cch, const wchar_t *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	HRESULT r = StringCchVPrintfW(dst, cch, fmt, ap);
	va_end(ap);
	return r;
}

static inline HRESULT StringCbPrintfW(wchar_t *dst, size_t cb, const wchar_t *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	HRESULT r = StringCchVPrintfW(dst, cb / sizeof(wchar_t), fmt, ap);
	va_end(ap);
	return r;
}

static inline HRESULT StringCchCopyW(wchar_t *dst, size_t cch, const wchar_t *src)
{
	if (!cch) return STRSAFE_E_INSUFFICIENT_BUFFER;
	size_t i = 0;
	for (; i + 1 < cch && src[i]; i++) dst[i] = src[i];
	dst[i] = 0;
	return src[i] ? STRSAFE_E_INSUFFICIENT_BUFFER : S_OK;
}

static inline HRESULT StringCchCatW(wchar_t *dst, size_t cch, const wchar_t *src)
{
	size_t l = wcslen(dst);
	if (l >= cch) return STRSAFE_E_INSUFFICIENT_BUFFER;
	return StringCchCopyW(dst + l, cch - l, src);
}

#define StringCchPrintf StringCchPrintfW
#define StringCbPrintf StringCbPrintfW
#define StringCchCopy StringCchCopyW
#define StringCchCat StringCchCatW

/* ---- shlwapi ---- */
static inline int StrCmpIW(const wchar_t *a, const wchar_t *b) { return wcscasecmp(a, b); }
static inline int StrCmpNIW(const wchar_t *a, const wchar_t *b, int n) { return wcsncasecmp(a, b, (size_t)n); }
static inline wchar_t *StrStrW(const wchar_t *h, const wchar_t *n) { return (wchar_t *)wcsstr(h, n); }
#define StrCmpI StrCmpIW
#define StrCmpNI StrCmpNIW
#define StrStr StrStrW

/* Paths use '/' on Linux; '\\' is accepted too so that Windows style
   playlists keep working with $filepart() and friends. */
static inline int wa_is_sep(wchar_t c) { return c == L'/' || c == L'\\'; }

static inline wchar_t *PathFindFileNameW(const wchar_t *p)
{
	const wchar_t *r = p;
	for (const wchar_t *s = p; *s; s++)
		if (wa_is_sep(*s) && s[1]) r = s + 1;
	return (wchar_t *)r;
}

static inline wchar_t *PathFindExtensionW(const wchar_t *p)
{
	const wchar_t *dot = 0;
	const wchar_t *s = p;
	for (; *s; s++)
	{
		if (*s == L'.') dot = s;
		else if (wa_is_sep(*s) || *s == L' ') dot = 0;
	}
	return (wchar_t *)(dot ? dot : s);
}

static inline wchar_t *PathRemoveBackslashW(wchar_t *p)
{
	size_t l = wcslen(p);
	if (l > 1 && wa_is_sep(p[l - 1])) { p[l - 1] = 0; return p + l - 1; }
	return p + l;
}

static inline wchar_t *PathAddBackslashW(wchar_t *p)
{
	size_t l = wcslen(p);
	if (!l || !wa_is_sep(p[l - 1])) { p[l] = L'/'; p[l + 1] = 0; l++; }
	return p + l;
}

static inline BOOL PathRemoveFileSpecW(wchar_t *p)
{
	wchar_t *last = 0;
	for (wchar_t *s = p; *s; s++)
		if (wa_is_sep(*s)) last = s;
	if (!last) { if (*p) { *p = 0; return TRUE; } return FALSE; }
	if (last == p) last[1] = 0; else *last = 0;
	return TRUE;
}

static inline void PathStripPathW(wchar_t *p)
{
	wchar_t *f = PathFindFileNameW(p);
	if (f != p) memmove(p, f, (wcslen(f) + 1) * sizeof(wchar_t));
}

static inline wchar_t *PathFindNextComponentW(const wchar_t *p)
{
	if (!p || !*p) return 0;
	while (*p && !wa_is_sep(*p)) p++;
	if (*p) p++;
	return (wchar_t *)p;
}

#define PathFindFileName PathFindFileNameW
#define PathFindExtension PathFindExtensionW
#define PathRemoveBackslash PathRemoveBackslashW
#define PathAddBackslash PathAddBackslashW
#define PathRemoveFileSpec PathRemoveFileSpecW
#define PathStripPath PathStripPathW
#define PathFindNextComponent PathFindNextComponentW

#endif
