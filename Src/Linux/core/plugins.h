/*
** Winamp for Linux - input/output plug-in host (port of Src/Winamp/In.cpp,
** Out.cpp plug-in loading).
*/
#pragma once

#include "../sdk/in2.h"
#include <string>
#include <vector>

struct InputPlugin
{
	std::string path;
	void *handle;
	In_Module *mod;
	winampGetExtendedFileInfoFunc getExtendedFileInfo;
	std::vector<std::string> extensions; // lower case, no dot
	std::vector<std::pair<std::string, std::string>> filters; // ("*.mp3;*.mp2", "MPEG audio")
};

struct OutputPlugin
{
	std::string path;
	std::string file; // "out_pulse.so"
	void *handle;
	Out_Module *mod;
};

extern HWND hMainWindow;                   // host handle handed to plug-ins
extern std::vector<InputPlugin> g_inputs;
extern std::vector<OutputPlugin> g_outputs;
extern In_Module *in_mod;                  // currently playing input module
extern Out_Module *out_mod;                // selected output module

void plugins_load(HWND host);
void plugins_unload();
std::vector<std::string> plugins_search_dirs();

InputPlugin *in_find(const std::string &filename);
bool in_is_supported(const std::string &filename);
std::vector<std::string> in_extensions();

// metadata through the input plug-ins' winampGetExtendedFileInfo()
bool in_get_extended_fileinfo(const std::string &filename, const char *metadata, std::string &out);
// title + length (seconds, -1 if unknown) using the plug-in's GetFileInfo
void in_get_fileinfo(const std::string &filename, std::string &title, int &length_sec);
bool in_infobox(const std::string &filename);

void out_select(const std::string &file);
OutputPlugin *out_current();
