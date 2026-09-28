/*
** Winamp for Linux - classic visualization data (spectrum analyzer /
** oscilloscope), ported from Src/Winamp/SA.cpp, classic_vis.cpp and
** SABuffer.cpp.
*/
#pragma once

void sa_init(int numframes);
void sa_deinit();
int sa_add(char *values, int timestamp, int csa);
char *sa_get(int timestamp, int csa, char data[75 * 2 + 8]);
void sa_addpcmdata(void *data_buf, int numChannels, int numBits, int ts);
void sa_setmode(int mode);   // 0 = off, 1 = spectrum analyzer, 2 = oscilloscope
int sa_getmode();

extern volatile int sa_curmode;
extern int g_srate_exact;
