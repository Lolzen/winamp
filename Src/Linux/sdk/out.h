/*
** Winamp for Linux - output plug-in interface
**
** Same structure and semantics as Src/Winamp/OUT.H. Output plug-ins are
** shared objects named out_*.so that export winampGetOutModule().
*/
#ifndef NULLSOFT_OUTH
#define NULLSOFT_OUTH

#include "wa_linux.h"

#define OUT_VER 0x10

typedef struct
{
	int       version;              // module version (OUT_VER)
	char     *description;          // description of module, with version string (UTF-8)
	intptr_t  id;                   // module id. non-nullsoft modules should be >= 65536
	HWND      hMainWindow;          // winamp's main window (filled in by winamp)
	HINSTANCE hDllInstance;         // dlopen() handle (filled in by winamp)

	void (__cdecl *Config)(HWND hwndParent);  // configuration dialog
	void (__cdecl *About)(HWND hwndParent);   // about dialog

	void (__cdecl *Init)();         // called when loaded
	void (__cdecl *Quit)();         // called when unloaded

	// returns >=0 on success, <0 on failure. returns max latency in ms (0 for diskwriters, etc)
	// bufferlenms and prebufferms must be in ms. 0 to use defaults.
	int (__cdecl *Open)(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms);
	void (__cdecl *Close)();        // close the ol' output device.
	// 0 on success. Len == bytes to write (<= 8192 always). buf is straight audio data.
	// 1 returns not able to write (yet). Non-blocking, always.
	int (__cdecl *Write)(char *buf, int len);
	int (__cdecl *CanWrite)();      // returns number of bytes possible to write at a given time.
	int (__cdecl *IsPlaying)();     // non0 if output is still going or if data in buffers waiting to be written
	int (__cdecl *Pause)(int pause);// returns previous pause state
	void (__cdecl *SetVolume)(int volume); // volume is 0-255
	void (__cdecl *SetPan)(int pan);       // pan is -128 to 128
	void (__cdecl *Flush)(int t);   // flushes buffers and restarts output at time t (in ms) (used for seeking)
	int (__cdecl *GetOutputTime)(); // returns played time in MS
	int (__cdecl *GetWrittenTime)();// returns time written in MS (used for synching up vis stuff)
} Out_Module;

typedef Out_Module *(*winampGetOutModuleFunc)(void);

#endif
