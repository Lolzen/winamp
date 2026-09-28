/*
** Winamp for Linux - plug-in SDK platform layer
**
** The Windows plug-in API (in2.h / out.h / dsp.h) passes a HWND for the main
** window and uses PostMessage()/SendMessage() to talk to the player. On Linux
** the player hands plug-ins a pointer to a small host object instead, which
** exposes the same two calls. The inline helpers below let plug-in code keep
** calling PostMessage()/SendMessage() exactly like it does on Windows.
*/
#ifndef NULLSOFT_WINAMP_LINUX_PLATFORM_H
#define NULLSOFT_WINAMP_LINUX_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cdecl
#define __cdecl
#endif

#define WA_EXPORT __attribute__((visibility("default")))

typedef struct winamp_host_window *HWND;
typedef void *HINSTANCE;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;

struct winamp_host_window
{
	/* queue a message for the player's UI thread; safe from any thread */
	int (*post_message)(HWND hwnd, unsigned int msg, WPARAM wParam, LPARAM lParam);
	/* synchronous call into the player (runs on the calling thread) */
	LRESULT (*send_message)(HWND hwnd, unsigned int msg, WPARAM wParam, LPARAM lParam);
};

static inline int PostMessage(HWND hwnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	return (hwnd && hwnd->post_message) ? hwnd->post_message(hwnd, msg, wParam, lParam) : 0;
}

static inline LRESULT SendMessage(HWND hwnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	return (hwnd && hwnd->send_message) ? hwnd->send_message(hwnd, msg, wParam, lParam) : 0;
}

#define WM_USER 0x0400

#ifdef __cplusplus
}
#endif

#endif
