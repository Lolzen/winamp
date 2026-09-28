/*
** Winamp for Linux - Windows BMP decoder.
**
** Classic skins are full of odd BMP variants (RLE8 compressed, 4 bit, OS/2
** headers, 32 bit with garbage in the alpha byte...). Windows' LoadImage()
** accepts them all and ignores alpha, so we do the same instead of relying on
** gdk-pixbuf.
*/
#include "gfx.h"

#include <string.h>

static uint32_t rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static int mask_shift(uint32_t m)
{
	int s = 0;
	if (!m) return 0;
	while (!(m & 1)) { m >>= 1; s++; }
	return s;
}

static int mask_bits(uint32_t m)
{
	int b = 0;
	m >>= mask_shift(m);
	while (m & 1) { m >>= 1; b++; }
	return b;
}

static uint32_t expand(uint32_t v, uint32_t mask)
{
	if (!mask) return 0;
	int bits = mask_bits(mask);
	v = (v & mask) >> mask_shift(mask);
	if (bits >= 8) return v >> (bits - 8);
	uint32_t r = v << (8 - bits);
	// replicate the top bits so full scale maps to 255
	for (int b = bits; b < 8; b += bits) r |= r >> bits;
	return r & 0xff;
}

bool LoadBMP(const std::vector<unsigned char> &data, Bitmap &out)
{
	const size_t n = data.size();
	const unsigned char *d = data.data();
	if (n < 26 || d[0] != 'B' || d[1] != 'M') return false;
	uint32_t off_bits = rd32(d + 10);
	uint32_t hsize = rd32(d + 14);
	if (14 + (size_t)hsize > n) return false;

	int32_t width, height;
	int bpp, compression = 0;
	uint32_t colors_used = 0;
	int pal_entry = 4;
	if (hsize == 12) // BITMAPCOREHEADER
	{
		width = (int16_t)rd16(d + 18);
		height = (int16_t)rd16(d + 20);
		bpp = rd16(d + 24);
		pal_entry = 3;
	}
	else if (hsize >= 40)
	{
		width = (int32_t)rd32(d + 18);
		height = (int32_t)rd32(d + 22);
		bpp = rd16(d + 28);
		compression = (int)rd32(d + 30);
		colors_used = rd32(d + 46);
	}
	else
		return false;

	bool top_down = height < 0;
	if (top_down) height = -height;
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384) return false;

	uint32_t rmask = 0, gmask = 0, bmask = 0;
	if (compression == 3 || compression == 6) // BI_BITFIELDS / BI_ALPHABITFIELDS
	{
		const unsigned char *m = d + 14 + 40;
		if (hsize >= 52) m = d + 14 + 40; // masks are part of V2+ headers at the same offset
		if (m + 12 > d + n) return false;
		rmask = rd32(m);
		gmask = rd32(m + 4);
		bmask = rd32(m + 8);
	}
	else if (bpp == 16)
	{
		rmask = 0x7C00;
		gmask = 0x03E0;
		bmask = 0x001F;
	}

	// palette
	uint32_t palette[256] = {0};
	if (bpp <= 8)
	{
		uint32_t count = colors_used ? colors_used : (1u << bpp);
		if (count > 256) count = 256;
		size_t pal_off = 14 + hsize + ((compression == 3 && hsize == 40) ? 12 : 0);
		for (uint32_t i = 0; i < count && pal_off + i * pal_entry + 3 <= n; i++)
		{
			const unsigned char *p = d + pal_off + i * pal_entry;
			palette[i] = ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
		}
	}

	out.Create(width, height);
	uint32_t *px = out.Pixels();
	auto put = [&](int x, int row, uint32_t c) {
		if (x < 0 || x >= width || row < 0 || row >= height) return;
		int y = top_down ? row : height - 1 - row;
		px[(size_t)y * width + x] = c;
	};

	if (off_bits >= n) return false;
	const unsigned char *bits = d + off_bits;
	size_t avail = n - off_bits;

	if (compression == 1 || compression == 2) // RLE8 / RLE4
	{
		bool rle4 = compression == 2;
		int x = 0, row = 0;
		size_t i = 0;
		while (i + 1 < avail && row < height)
		{
			unsigned count = bits[i], val = bits[i + 1];
			i += 2;
			if (count)
			{
				for (unsigned k = 0; k < count; k++)
				{
					unsigned idx = rle4 ? ((k & 1) ? (val & 0x0F) : (val >> 4)) : val;
					put(x++, row, palette[idx & 0xff]);
				}
			}
			else if (val == 0) { x = 0; row++; }         // end of line
			else if (val == 1) break;                    // end of bitmap
			else if (val == 2)                           // delta
			{
				if (i + 1 >= avail) break;
				x += bits[i];
				row += bits[i + 1];
				i += 2;
			}
			else                                         // absolute run
			{
				unsigned cnt = val;
				for (unsigned k = 0; k < cnt; k++)
				{
					unsigned idx;
					if (rle4)
					{
						if (i + k / 2 >= avail) break;
						unsigned b = bits[i + k / 2];
						idx = (k & 1) ? (b & 0x0F) : (b >> 4);
					}
					else
					{
						if (i + k >= avail) break;
						idx = bits[i + k];
					}
					put(x++, row, palette[idx]);
				}
				size_t used = rle4 ? (cnt + 1) / 2 : cnt;
				i += (used + 1) & ~(size_t)1; // runs are word aligned
			}
		}
		return true;
	}

	if (compression != 0 && compression != 3 && compression != 6) return false;

	size_t stride = (((size_t)width * bpp + 31) / 32) * 4;
	for (int row = 0; row < height; row++)
	{
		if ((size_t)(row + 1) * stride > avail) break;
		const unsigned char *s = bits + (size_t)row * stride;
		for (int x = 0; x < width; x++)
		{
			uint32_t c = 0;
			switch (bpp)
			{
			case 1: c = palette[(s[x >> 3] >> (7 - (x & 7))) & 1]; break;
			case 4: c = palette[(x & 1) ? (s[x >> 1] & 0x0F) : (s[x >> 1] >> 4)]; break;
			case 8: c = palette[s[x]]; break;
			case 16:
			{
				uint32_t v = rd16(s + x * 2);
				c = (expand(v, rmask) << 16) | (expand(v, gmask) << 8) | expand(v, bmask);
				break;
			}
			case 24: c = ((uint32_t)s[x * 3 + 2] << 16) | ((uint32_t)s[x * 3 + 1] << 8) | s[x * 3]; break;
			case 32:
				if (rmask)
				{
					uint32_t v = rd32(s + x * 4);
					c = (expand(v, rmask) << 16) | (expand(v, gmask) << 8) | expand(v, bmask);
				}
				else
					c = ((uint32_t)s[x * 4 + 2] << 16) | ((uint32_t)s[x * 4 + 1] << 8) | s[x * 4];
				break;
			default: return false;
			}
			put(x, row, c);
		}
	}
	return true;
}
