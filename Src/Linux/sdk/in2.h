/*
** Winamp for Linux - input plug-in interface
**
** Same structure and semantics as Src/Winamp/IN2.H. File names are UTF-8
** (in_char is always char). Input plug-ins are shared objects named in_*.so
** exporting winampGetInModule2(), and optionally:
**
**   int winampGetExtendedFileInfo(const char *filename, const char *metadata,
**                                 char *ret, int retlen);
**
** which is used for playlist titles ("artist", "title", "album", "length",
** ...), exactly like winampGetExtendedFileInfo on Windows.
*/
#ifndef NULLSOFT_WINAMP_IN2H
#define NULLSOFT_WINAMP_IN2H

#include "out.h"

#define in_char char
#define IN_VER 0x101

#define IN_MODULE_FLAG_USES_OUTPUT_PLUGIN 1
#define IN_MODULE_FLAG_EQ 2
#define IN_MODULE_FLAG_REPLAYGAIN 8
#define IN_MODULE_FLAG_REPLAYGAIN_PREAMP 16

#define GETFILEINFO_TITLE_LENGTH 2048

#define INFOBOX_EDITED 0
#define INFOBOX_UNCHANGED 1

typedef struct
{
	int version;              // module type (IN_VER)
	char *description;        // description of module, with version string
	HWND hMainWindow;         // Winamp's main window (filled in by Winamp)
	HINSTANCE hDllInstance;   // dlopen() handle (filled in by Winamp)
	char *FileExtensions;     // "mp3\0Layer 3 MPEG\0mp2\0Layer 2 MPEG\0mpg\0Layer 1 MPEG\0"
	int is_seekable;          // is this stream seekable?
	int UsesOutputPlug;       // flags, see IN_MODULE_FLAG_*

	void (__cdecl *Config)(HWND hwndParent);
	void (__cdecl *About)(HWND hwndParent);

	int (__cdecl *Init)();    // called at program init
	void (__cdecl *Quit)();   // called at program quit

	// If file == NULL then the currently playing file is used
	void (*GetFileInfo)(const in_char *file, in_char *title, int *length_in_ms);

	int (__cdecl *InfoBox)(const in_char *file, HWND hwndParent);
	int (__cdecl *IsOurFile)(const in_char *fn); // called before extension checks, to allow detection of http://, etc

	// playback stuff
	int (__cdecl *Play)(const in_char *fn); // return zero on success, -1 on file-not-found, some other value on other (stopping Winamp) error
	void (__cdecl *Pause)();
	void (__cdecl *UnPause)();
	int (__cdecl *IsPaused)();
	void (__cdecl *Stop)();

	// time stuff
	int (__cdecl *GetLength)();              // get length in ms
	int (__cdecl *GetOutputTime)();          // returns current output time in ms. (usually returns outMod->GetOutputTime()
	void (__cdecl *SetOutputTime)(int time_in_ms); // seeks to point in stream (in ms)

	// volume stuff
	void (__cdecl *SetVolume)(int volume);   // from 0 to 255.. usually just call outMod->SetVolume
	void (__cdecl *SetPan)(int pan);         // from -127 to 127.. usually just call outMod->SetPan

	// in-window builtin vis stuff
	void (__cdecl *SAVSAInit)(int maxlatency_in_ms, int srate);
	void (__cdecl *SAVSADeInit)();
	void (__cdecl *SAAddPCMData)(void *PCMData, int nch, int bps, int timestamp);
	int (__cdecl *SAGetMode)();
	int (__cdecl *SAAdd)(void *data, int timestamp, int csa);

	void (__cdecl *VSAAddPCMData)(void *PCMData, int nch, int bps, int timestamp);
	int (__cdecl *VSAGetMode)(int *specNch, int *waveNch);
	int (__cdecl *VSAAdd)(void *data, int timestamp);
	void (__cdecl *VSASetInfo)(int srate, int nch);

	// dsp plug-in processing (filled in by Winamp, called by input plug)
	int (__cdecl *dsp_isactive)();
	int (__cdecl *dsp_dosamples)(short int *samples, int numsamples, int bps, int nch, int srate);

	// eq stuff
	void (__cdecl *EQSet)(int on, char data[10], int preamp); // 0-64 each, 31 is +0, 0 is +12, 63 is -12. Do nothing to ignore.

	// info setting (filled in by Winamp)
	void (__cdecl *SetInfo)(int bitrate, int srate, int stereo, int synched);

	Out_Module *outMod; // filled in by Winamp, optionally used :)

	void *service;      // unused on Linux
} In_Module;

#define IN_INIT_SUCCESS 0
#define IN_INIT_FAILURE 1

typedef In_Module *(*winampGetInModule2Func)(void);
typedef int (*winampGetExtendedFileInfoFunc)(const char *filename, const char *metadata, char *ret, int retlen);

#endif
