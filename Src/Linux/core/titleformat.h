/*
** Winamp for Linux - playlist title formatting through the original
** Src/tagz engine (e.g. "[%artist% - ]$if2(%title%,$filepart(%filename%))").
*/
#pragma once

#include <string>

// Returns false if the file has no metadata at all (caller falls back to the file name).
bool format_title(const std::string &filename, std::string &title, int &length_sec);
std::string format_title_spec(const std::wstring &spec, const std::string &filename);
