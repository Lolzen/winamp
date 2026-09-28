/*
** Winamp for Linux - the playlist (port of the PlayList_* API from
** Src/Winamp/PlayList.cpp plus the M3U/PLS readers and writers).
**
** All functions are meant to be called from the UI thread. Titles and
** lengths are looked up on a background thread; when an entry changes the
** registered change callback runs on the UI thread.
*/
#pragma once

#include <string>
#include <vector>
#include <functional>

int PlayList_getlength();
int PlayList_getPosition();
void PlayList_setposition(int pos);
int PlayList_advance(int n);         // moves position by n, returns new position (clamped)

std::string PlayList_getfilename(int idx);
std::string PlayList_gettitle(int idx);        // formatted title (never empty)
std::string PlayList_getitem_pl(int idx);      // title as shown in the playlist editor ("3. Artist - Title")
int PlayList_getsonglength(int idx);           // seconds, -1 if unknown
int PlayList_getcurrentlength();
bool PlayList_getselect(int idx);
void PlayList_setselect(int idx, bool sel);
int PlayList_getselectcount();
void PlayList_swap(int a, int b);

void PlayList_append(const std::string &filename, const std::string &title = std::string(), int length_sec = -1);
void PlayList_insert(int pos, const std::string &filename, const std::string &title = std::string(), int length_sec = -1);
void PlayList_delete(int idx);
void PlayList_deleteselected();
void PlayList_cropselected();
void PlayList_clear();
void PlayList_refreshtitle(int idx);  // re-read metadata
void PlayList_refreshselected();
void PlayList_removemissing();

enum { PL_SORT_FILENAME = 0, PL_SORT_TITLE = 1, PL_SORT_PATH = 2, PL_SORT_RANDOM = 3, PL_SORT_REVERSE = 4 };
void PlayList_sort(int mode);

// shuffle aware "next"/"previous" (returns -1 when the end is reached and repeat is off)
int PlayList_getnext(bool manual);
int PlayList_getprev();
void PlayList_reshuffle();

// adds a file, a directory (recursive) or a playlist file; returns number of entries added
int PlayList_add(const std::string &path_or_url, int insert_pos = -1);
bool PlayList_isplaylist(const std::string &filename);
int PlayList_load(const std::string &playlist_file, int insert_pos = -1);
bool PlayList_save(const std::string &playlist_file, bool selected_only = false);

void PlayList_setchangecallback(std::function<void()> cb);
void PlayList_shutdown();
