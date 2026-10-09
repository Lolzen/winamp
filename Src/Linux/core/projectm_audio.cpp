/*
** Winamp for Linux - bounded PCM queue used by projectM.
*/
#include "projectm_audio.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

namespace projectm_audio
{
namespace
{
constexpr size_t MAX_QUEUED_BLOCKS = 96;
std::deque<Block> queue;
std::mutex queue_mutex;

float decode_sample(const unsigned char *p, int bits)
{
	switch (bits)
	{
	case 8:
		return (static_cast<float>(p[0]) - 128.0f) / 128.0f;
	case 16:
	{
		int16_t value = 0;
		std::memcpy(&value, p, sizeof(value));
		return static_cast<float>(value) / 32768.0f;
	}
	case 24:
	{
		int32_t value = static_cast<int32_t>(p[0]) |
		                (static_cast<int32_t>(p[1]) << 8) |
		                (static_cast<int32_t>(p[2]) << 16);
		if (value & 0x00800000) value |= ~0x00ffffff;
		return static_cast<float>(value) / 8388608.0f;
	}
	case 32:
	{
		int32_t value = 0;
		std::memcpy(&value, p, sizeof(value));
		return static_cast<float>(static_cast<double>(value) / 2147483648.0);
	}
	default:
		return 0.0f;
	}
}

float clamp_sample(float value)
{
	return std::max(-1.0f, std::min(1.0f, value));
}
}

void push_pcm(const void *data, int channels, int bits, unsigned int frames)
{
	if (!data || channels < 1 || frames == 0 ||
	    (bits != 8 && bits != 16 && bits != 24 && bits != 32))
		return;

	const int bytes = bits / 8;
	const unsigned int output_channels = channels > 1 ? 2u : 1u;
	const unsigned char *source = static_cast<const unsigned char *>(data);
	Block block;
	block.channels = output_channels;
	block.frames = frames;
	block.samples.resize(static_cast<size_t>(frames) * output_channels);

	for (unsigned int frame = 0; frame < frames; frame++)
	{
		const unsigned char *left = source + (static_cast<size_t>(frame) * channels) * bytes;
		const unsigned char *right = channels > 1 ? left + bytes : left;
		float l = clamp_sample(decode_sample(left, bits));
		float r = clamp_sample(decode_sample(right, bits));
		if (output_channels == 1)
			block.samples[frame] = l;
		else
		{
			block.samples[frame * 2] = l;
			block.samples[frame * 2 + 1] = r;
		}
	}

	std::lock_guard<std::mutex> lock(queue_mutex);
	if (queue.size() >= MAX_QUEUED_BLOCKS)
		queue.pop_front();
	queue.emplace_back(std::move(block));
}

bool pop(Block &block)
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	if (queue.empty()) return false;
	block = std::move(queue.front());
	queue.pop_front();
	return true;
}

void clear()
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	queue.clear();
}
}
