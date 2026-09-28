/*
** Winamp for Linux - playback control (ports of Src/Winamp/In.cpp,
** Play.cpp and the transport parts of main_command.cpp).
*/
#pragma once

#include "../sdk/in2.h"
#include <string>
#include <functional>

extern int playing, paused;
extern std::string FileName;     // currently playing file
extern std::string FileTitle;    // formatted title of the current file
extern int g_brate, g_srate, g_nch; // kbps, kHz, channels of the current stream
extern int g_stopaftercur;

void player_setup_input_module(In_Module *mod); // fills in the callbacks Winamp provides

int in_getouttime();   // ms
int in_getlength();    // seconds, -1 if unknown
void in_setvol(int v); // 0..255
void in_setpan(int p); // -127..127
int in_seek(int time_in_ms);
bool in_seekable();

void StartPlaying();            // play the current playlist entry from the start
void StopPlaying(int fade);
void PausePlaying();            // toggle pause
void PlayNext(bool manual);     // "next" button / end of file
void PlayPrevious();
void PlayIndex(int idx);
void player_seek_relative(int ms);
void player_on_eof();           // WM_WA_MPEG_EOF
void player_update_title();     // re-read FileTitle from the playlist

// UI notifications (run on the UI thread)
struct PlayerCallbacks
{
	std::function<void()> state_changed; // play/stop/pause/new song
	std::function<void()> info_changed;  // bitrate/samplerate/channels
};
void player_set_callbacks(const PlayerCallbacks &cb);
void player_shutdown();
