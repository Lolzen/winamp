/*
** Winamp for Linux - equalizer processing and preset files
** (ports of the EQ parts of Src/Winamp/In.cpp and Src/Winamp/Eq.cpp).
*/
#pragma once

#include <string>
#include <vector>

void eq_set(int on, char data[10], int preamp);
void eq_apply_config();        // eq_set(config_use_eq, eq_tab, config_preamp)
int eq_isactive();
int eq_dosamples(short *samples, int numsamples, int bps, int nch, int srate);
void eq_reset_channels(int nch);

// preset files: Windows compatible "Winamp EQ library file v1.1" format
void eq_create_default_presets();
bool eq_read_preset(const std::string &file, const std::string &name); // loads into eq_tab / config_preamp
bool eq_write_preset(const std::string &file, const std::string &name);
void eq_delete_preset(const std::string &file, const std::string &name);
std::vector<std::string> eq_list_presets(const std::string &file);
void eq_autoload(const std::string &filename);
