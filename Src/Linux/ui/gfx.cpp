#include "gfx.h"

#include <cairo.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <mutex>

Bitmap &Bitmap::operator=(const Bitmap &o)
{
	if (this != &o)
	{
		DropSurface();
		w = o.w;
		h = o.h;
		px = o.px;
	}
	return *this;
}

Bitmap::~Bitmap()
{
	DropSurface();
}

void Bitmap::DropSurface()
{
	if (surface)
	{
		cairo_surface_destroy(surface);
		surface = nullptr;
	}
}

void Bitmap::Create(int nw, int nh, uint32_t fill)
{
	DropSurface();
	w = nw > 0 ? nw : 0;
	h = nh > 0 ? nh : 0;
	px.assign((size_t)w * h, fill & 0xFFFFFF);
}

uint32_t Bitmap::GetPixel(int x, int y) const
{
	if (x < 0 || y < 0 || x >= w || y >= h) return CLR_INVALID;
	return px[(size_t)y * w + x] & 0xFFFFFF;
}

COLORREF Bitmap::GetPixelRef(int x, int y) const
{
	uint32_t p = GetPixel(x, y);
	if (p == CLR_INVALID) return CLR_INVALID;
	return RGB((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff);
}

cairo_surface_t *Bitmap::Surface()
{
	if (!surface && Valid())
		surface = cairo_image_surface_create_for_data((unsigned char *)px.data(), CAIRO_FORMAT_RGB24, w, h, w * 4);
	if (surface) cairo_surface_mark_dirty(surface);
	return surface;
}

/* ---------------- blits ---------------- */

void BitBlt(Bitmap &dst, int x, int y, int w, int h, const Bitmap &src, int sx, int sy)
{
	if (w <= 0 || h <= 0 || !dst.Valid() || !src.Valid()) return;
	// clip to the source
	if (sx < 0) { x -= sx; w += sx; sx = 0; }
	if (sy < 0) { y -= sy; h += sy; sy = 0; }
	if (sx + w > src.Width()) w = src.Width() - sx;
	if (sy + h > src.Height()) h = src.Height() - sy;
	// clip to the destination
	if (x < 0) { sx -= x; w += x; x = 0; }
	if (y < 0) { sy -= y; h += y; y = 0; }
	if (x + w > dst.Width()) w = dst.Width() - x;
	if (y + h > dst.Height()) h = dst.Height() - y;
	if (w <= 0 || h <= 0) return;
	if (&dst == &src)
	{
		// overlapping copies within one bitmap (the EQ skin blits from itself)
		std::vector<uint32_t> tmp((size_t)w * h);
		for (int r = 0; r < h; r++)
			memcpy(&tmp[(size_t)r * w], src.Pixels() + (size_t)(sy + r) * src.Width() + sx, (size_t)w * 4);
		for (int r = 0; r < h; r++)
			memcpy(dst.Pixels() + (size_t)(y + r) * dst.Width() + x, &tmp[(size_t)r * w], (size_t)w * 4);
		return;
	}
	for (int r = 0; r < h; r++)
		memcpy(dst.Pixels() + (size_t)(y + r) * dst.Width() + x, src.Pixels() + (size_t)(sy + r) * src.Width() + sx, (size_t)w * 4);
}

void StretchBlt(Bitmap &dst, int x, int y, int w, int h, const Bitmap &src, int sx, int sy, int sw, int sh)
{
	if (w <= 0 || h <= 0 || sw <= 0 || sh <= 0 || !dst.Valid() || !src.Valid()) return;
	if (w == sw && h == sh)
	{
		BitBlt(dst, x, y, w, h, src, sx, sy);
		return;
	}
	std::vector<uint32_t> out((size_t)w * h);
	for (int r = 0; r < h; r++)
	{
		int yy = sy + r * sh / h;
		for (int c = 0; c < w; c++)
		{
			int xx = sx + c * sw / w;
			uint32_t p = src.GetPixel(xx, yy);
			out[(size_t)r * w + c] = p == CLR_INVALID ? 0 : p;
		}
	}
	for (int r = 0; r < h; r++)
		for (int c = 0; c < w; c++)
		{
			int dx = x + c, dy = y + r;
			if (dx >= 0 && dy >= 0 && dx < dst.Width() && dy < dst.Height())
				dst.Pixels()[(size_t)dy * dst.Width() + dx] = out[(size_t)r * w + c];
		}
}

void FillRect(Bitmap &dst, int x, int y, int w, int h, uint32_t rgb)
{
	int x2 = std::min(x + w, dst.Width()), y2 = std::min(y + h, dst.Height());
	x = std::max(x, 0);
	y = std::max(y, 0);
	for (int r = y; r < y2; r++)
		for (int c = x; c < x2; c++)
			dst.Pixels()[(size_t)r * dst.Width() + c] = rgb & 0xFFFFFF;
}

/* ---------------- images ---------------- */

bool LoadImageData(const std::vector<unsigned char> &data, Bitmap &out)
{
	if (data.size() >= 2 && data[0] == 'B' && data[1] == 'M')
		return LoadBMP(data, out);
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
	bool ok = gdk_pixbuf_loader_write(loader, data.data(), data.size(), nullptr) && gdk_pixbuf_loader_close(loader, nullptr);
	GdkPixbuf *pb = ok ? gdk_pixbuf_loader_get_pixbuf(loader) : nullptr;
	if (pb)
	{
		int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
		int n = gdk_pixbuf_get_n_channels(pb), stride = gdk_pixbuf_get_rowstride(pb);
		const guchar *p = gdk_pixbuf_get_pixels(pb);
		out.Create(w, h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
			{
				const guchar *s = p + y * stride + x * n;
				out.Pixels()[(size_t)y * w + x] = ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2];
			}
	}
	else if (!ok)
		gdk_pixbuf_loader_close(loader, nullptr);
	g_object_unref(loader);
	return pb != nullptr;
}

/* ---------------- text ---------------- */

static PangoFontDescription *font_desc(const TextFont &f)
{
	PangoFontDescription *d = pango_font_description_new();
	// Windows skins name Windows fonts; fontconfig maps them to metric compatible ones
	std::string fam = f.family.empty() ? "Arial" : f.family;
	fam += ",Liberation Sans,Arimo,DejaVu Sans,Sans";
	pango_font_description_set_family(d, fam.c_str());
	pango_font_description_set_absolute_size(d, f.pixel_size * PANGO_SCALE);
	return d;
}

static PangoLayout *make_layout(cairo_t *cr, const TextFont &f)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	cairo_font_options_t *opt = cairo_font_options_create();
	cairo_font_options_set_antialias(opt, f.antialias ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE);
	cairo_font_options_set_hint_style(opt, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_hint_metrics(opt, CAIRO_HINT_METRICS_ON);
	pango_cairo_context_set_font_options(pango_layout_get_context(layout), opt);
	cairo_font_options_destroy(opt);
	PangoFontDescription *d = font_desc(f);
	pango_layout_set_font_description(layout, d);
	pango_font_description_free(d);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	return layout;
}

// a scratch context for measuring
static cairo_t *measure_cr()
{
	static cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
	static cairo_t *cr = cairo_create(s);
	return cr;
}

int TextFont::Height() const
{
	static std::mutex m;
	static std::map<std::string, int> cache;
	std::lock_guard<std::mutex> l(m);
	std::string key = family + "/" + std::to_string(pixel_size) + (antialias ? "a" : "");
	auto it = cache.find(key);
	if (it != cache.end()) return it->second;
	PangoLayout *layout = make_layout(measure_cr(), *this);
	pango_layout_set_text(layout, "Ag", -1);
	PangoFontMetrics *m2 = pango_context_get_metrics(pango_layout_get_context(layout), pango_layout_get_font_description(layout), nullptr);
	int h = (pango_font_metrics_get_ascent(m2) + pango_font_metrics_get_descent(m2) + PANGO_SCALE / 2) / PANGO_SCALE;
	pango_font_metrics_unref(m2);
	g_object_unref(layout);
	if (h < 1) h = 1;
	cache[key] = h;
	return h;
}

int TextFont::TextWidth(const std::string &utf8) const
{
	PangoLayout *layout = make_layout(measure_cr(), *this);
	pango_layout_set_text(layout, utf8.c_str(), -1);
	int w = 0, h = 0;
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	return w;
}

void DrawText(Bitmap &dst, const RECT &r, const std::string &utf8, const TextFont &font, uint32_t fg, uint32_t bg, int flags)
{
	if (r.right <= r.left || r.bottom <= r.top) return;
	if (bg != CLR_INVALID) FillRect(dst, r.left, r.top, r.right - r.left, r.bottom - r.top, bg);
	if (utf8.empty()) return;
	cairo_surface_t *s = dst.Surface();
	if (!s) return;
	cairo_t *cr = cairo_create(s);
	cairo_rectangle(cr, r.left, r.top, r.right - r.left, r.bottom - r.top);
	cairo_clip(cr);
	PangoLayout *layout = make_layout(cr, font);
	pango_layout_set_text(layout, utf8.c_str(), -1);
	int width = r.right - r.left;
	if (flags & DT_END_ELLIPSIS)
	{
		pango_layout_set_width(layout, width * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	}
	int tw = 0, th = 0;
	pango_layout_get_pixel_size(layout, &tw, &th);
	int x = r.left, y = r.top;
	if (flags & DT_RIGHT) x = r.right - tw;
	else if (flags & DT_CENTER) x = r.left + (width - tw) / 2;
	if (flags & DT_VCENTER) y = r.top + ((r.bottom - r.top) - font.Height()) / 2;
	cairo_set_source_rgb(cr, ((fg >> 16) & 0xff) / 255.0, ((fg >> 8) & 0xff) / 255.0, (fg & 0xff) / 255.0);
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_flush(s);
}

void DrawTextAt(Bitmap &dst, const RECT &clip, int x, int y, const std::string &utf8, const TextFont &font, uint32_t fg)
{
	if (clip.right <= clip.left || clip.bottom <= clip.top || utf8.empty()) return;
	cairo_surface_t *s = dst.Surface();
	if (!s) return;
	cairo_t *cr = cairo_create(s);
	cairo_rectangle(cr, clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top);
	cairo_clip(cr);
	PangoLayout *layout = make_layout(cr, font);
	pango_layout_set_text(layout, utf8.c_str(), -1);
	cairo_set_source_rgb(cr, ((fg >> 16) & 0xff) / 255.0, ((fg >> 8) & 0xff) / 255.0, (fg & 0xff) / 255.0);
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_flush(s);
}
