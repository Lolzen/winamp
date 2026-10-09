/*
** Winamp for Linux - audio hand-off for the projectM visualizer.
**
** Decoder threads call push_pcm(). The GTK/OpenGL render thread drains the
** bounded queue with pop(). Keeping the projectM instance out of the decoder
** thread is important: projectM owns OpenGL state and is not a DSP plug-in.
*/
#pragma once

#include <vector>

namespace projectm_audio
{
struct Block
{
	std::vector<float> samples; // interleaved for stereo
	unsigned int frames = 0;   // samples per channel
	unsigned int channels = 0; // 1 or 2
};

void push_pcm(const void *data, int channels, int bits, unsigned int frames);
bool pop(Block &block);
void clear();
}
