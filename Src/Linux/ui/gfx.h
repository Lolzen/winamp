/*
** Winamp for Linux - the tiny bitmap/blit layer the classic skin drawing code
** (ported from Src/Winamp/draw*.cpp) is written against. It mirrors the few
** GDI calls that code uses: BitBlt, StretchBlt, filled rectangles and text.
*/
#pragma once

#include "../core/common.h"

#include <stdint.h>
#include <string>
#include <vector>

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

class Bitmap
{
public:
	Bitmap() {}
	Bitmap(int w, int h, uint32_t fill = 0) { Create(w, h, fill); }
	Bitmap(const Bitmap &o) : w(o.w), h(o.h), px(o.px) {}
	Bitmap &operator=(const Bitmap &o);
	~Bitmap();

	void Create(int w, int h, uint32_t fill = 0);
	bool Valid() const { return w > 0 && h > 0; }
	int Width() const { return w; }
	int Height() const { return h; }
	uint32_t *Pixels() { return px.data(); }
	const uint32_t *Pixels() const { return px.data(); }

	// pixel as 0x00RRGGBB, or 0xFFFFFFFF (CLR_INVALID) outside the bitmap
	uint32_t GetPixel(int x, int y) const;
	COLORREF GetPixelRef(int x, int y) const; // as a Windows COLORREF (0x00BBGGRR)

	// cairo surface over our pixels (for text drawing / painting to the screen)
	cairo_surface_t *Surface();

private:
	void DropSurface();
	int w = 0, h = 0;
	std::vector<uint32_t> px; // 0x00RRGGBB (cairo RGB24)
	cairo_surface_t *surface = nullptr;
};

#define CLR_INVALID 0xFFFFFFFF

static inline uint32_t colorref_to_rgb(COLORREF c)
{
	return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c);
}

// all functions clip to both bitmaps like GDI does
void BitBlt(Bitmap &dst, int x, int y, int w, int h, const Bitmap &src, int sx, int sy);
void StretchBlt(Bitmap &dst, int x, int y, int w, int h, const Bitmap &src, int sx, int sy, int sw, int sh);
void FillRect(Bitmap &dst, int x, int y, int w, int h, uint32_t rgb);

// BMP files as written by Windows paint programs (1/4/8/16/24/32 bit, RLE4/RLE8)
bool LoadBMP(const std::vector<unsigned char> &data, Bitmap &out);
// any image format (BMP via LoadBMP, PNG/others via gdk-pixbuf)
bool LoadImageData(const std::vector<unsigned char> &data, Bitmap &out);

/* ---- text ---- */
struct TextFont
{
	std::string family = "Arial";
	int pixel_size = 12;    // em height in pixels, like CreateFont(-size)
	bool antialias = false; // DRAFT_QUALITY in the Windows build
	int Height() const;     // tmHeight
	int TextWidth(const std::string &utf8) const;
};

enum
{
	DT_LEFT = 0,
	DT_RIGHT = 2,
	DT_CENTER = 1,
	DT_VCENTER = 4,
	DT_END_ELLIPSIS = 0x8000,
};

// draws text with its top left corner at x,y, clipped to clip
void DrawTextAt(Bitmap &dst, const RECT &clip, int x, int y, const std::string &utf8, const TextFont &font, uint32_t fg_rgb);
// draws text clipped to r, filling the background of r with bg if bg != CLR_INVALID
void DrawText(Bitmap &dst, const RECT &r, const std::string &utf8, const TextFont &font, uint32_t fg_rgb, uint32_t bg_rgb, int flags);
