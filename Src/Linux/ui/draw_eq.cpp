/*
** Winamp for Linux - equalizer window drawing, ported from
** Src/Winamp/draw_eq.cpp. Like the original, eqMainDC is a working copy of
** eqmain.bmp that is drawn onto itself; the window shows its top 116 rows
** (14 in windowshade mode).
*/
#include "ui.h"

#include <algorithm>

static Bitmap eqMainDC;
static bool eq_init = false;

/* ---- Src/Winamp/plush/SPLINE.C (Plush 1.2, (c) 1996-2000 Justin Frankel) ---- */
struct pl_Spline
{
	float *keys;
	int keyWidth;
	int numKeys;
	float cont, bias, tens;
};

static void plSplineGetPoint(pl_Spline *s, float frame, float *out)
{
	int i, i_1, i0, i1, i2;
	float time1, time2, time3;
	float t1, t2, t3, t4, u1, u2, u3, u4, v1, v2, v3;
	float a, b, c, d;

	float *keys = s->keys;

	a = (1 - s->tens) * (1 + s->cont) * (1 + s->bias);
	b = (1 - s->tens) * (1 - s->cont) * (1 - s->bias);
	c = (1 - s->tens) * (1 - s->cont) * (1 + s->bias);
	d = (1 - s->tens) * (1 + s->cont) * (1 - s->bias);
	v1 = t1 = -a / 2.0f; u1 = a;
	u2 = (-6 - 2 * a + 2 * b + c) / 2.0f; v2 = (a - b) / 2.0f; t2 = (4 + a - b - c) / 2.0f;
	t3 = (-4 + b + c - d) / 2.0f;
	u3 = (6 - 2 * b - c + d) / 2.0f;
	v3 = b / 2.0f;
	t4 = d / 2.0f; u4 = -t4;

	i0 = (int)(unsigned)frame;
	i_1 = i0 - 1;
	while (i_1 < 0) i_1 += s->numKeys;
	i1 = i0 + 1;
	while (i1 >= s->numKeys) i1 -= s->numKeys;
	i2 = i0 + 2;
	while (i2 >= s->numKeys) i2 -= s->numKeys;
	time1 = frame - (float)((unsigned)frame);
	time2 = time1 * time1;
	time3 = time2 * time1;
	i0 *= s->keyWidth;
	i1 *= s->keyWidth;
	i2 *= s->keyWidth;
	i_1 *= s->keyWidth;
	for (i = 0; i < s->keyWidth; i++)
	{
		a = t1 * keys[i + i_1] + t2 * keys[i + i0] + t3 * keys[i + i1] + t4 * keys[i + i2];
		b = u1 * keys[i + i_1] + u2 * keys[i + i0] + u3 * keys[i + i1] + u4 * keys[i + i2];
		c = v1 * keys[i + i_1] + v2 * keys[i + i0] + v3 * keys[i + i1];
		*out++ = a * time3 + b * time2 + c * time1 + keys[i + i0];
	}
}

static void update_area_eq(int x1, int y1, int w, int h)
{
	if (!g_eq_wnd || !eq_init) return;
	BitBlt(g_eq_wnd->Buffer(), x1, y1, w, h, eqMainDC, x1, y1);
	g_eq_wnd->InvalidateRect(x1, y1, w, h);
}

void draw_eq_init()
{
	eqMainDC = g_skin.eqmain;
	if (eqMainDC.Height() < 315 || eqMainDC.Width() < 275)
	{
		Bitmap b(std::max(275, eqMainDC.Width()), std::max(315, eqMainDC.Height()));
		BitBlt(b, 0, 0, eqMainDC.Width(), eqMainDC.Height(), eqMainDC, 0, 0);
		eqMainDC = b;
	}
	eq_init = true;
}

void draw_eq_all()
{
	if (!eq_init) return;
	eqMainDC = g_skin.eqmain;
	draw_eq_slid(0, config_preamp, 0);
	for (int x = 1; x <= 10; x++)
		draw_eq_slid(x, eq_tab[x - 1], 0);
	draw_eq_graphthingy();
	draw_eq_onauto(config_use_eq, config_autoload_eq, 0, 0);
	draw_eq_tbar(g_eq_wnd && g_eq_wnd->Active() ? 1 : (config_hilite ? 0 : 1));
	draw_eq_presets(0);
	update_area_eq(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void draw_eq_presets(int pressed)
{
	int top = 18, left = 217;
	int w = 44;
	int h = 12;
	BitBlt(eqMainDC, left, top, w, h, eqMainDC, 224, pressed ? 176 : 164);
	update_area_eq(left, top, w, h);
}

void draw_eq_tbar(int active)
{
	int l = active ? 134 : 149;
	if (!eq_init) return;
	if (config_eq_ws)
	{
		const Bitmap &bmDC = g_skin.eq_ex;
		int xo = 63;
		int p = 94;
		int r;
		int xx = config_volume * 3 / 256;
		BitBlt(eqMainDC, 0, 0, WINDOW_WIDTH, 14, bmDC, 0, active ? 0 : 15);
		r = xo + (p * config_volume) / 255;
		BitBlt(eqMainDC, r - 2, 4, 3, 7, bmDC, xx * 3 + 1, 30);
		r = 166 + (39 * (config_pan + 128)) / 255;
		xx = (config_pan + 128) * 3 / 256;
		BitBlt(eqMainDC, r - 2, 4, 3, 7, bmDC, xx * 3 + 11, 30);
	}
	else
	{
		BitBlt(eqMainDC, 0, 0, WINDOW_WIDTH, 14, eqMainDC, 0, l);
	}
	update_area_eq(0, 0, WINDOW_WIDTH, 14);
}

void draw_eq_slid(int which, int pos, int pressed) // left to right, 0-64
{
	int top = 38, h = 63;
	int num_pos = 63 - 11;
	int w = 14;
	int xp;
	int n = 0;
	if (!which)
		xp = 21;
	else xp = 78 + (96 - 78) * (which - 1);
	if (!eq_init) return;
	n = 27 - ((pos * 28) / 64);
	if (n < 14)
		BitBlt(eqMainDC, xp, top, w, h, eqMainDC, 13 + n * 15, 164);
	else
		BitBlt(eqMainDC, xp, top, w, h, eqMainDC, 13 + (n - 14) * 15, 229);
	BitBlt(eqMainDC, xp + 1, top + h - 12 - ((63 - pos) * num_pos) / 64, 11, 11, eqMainDC, 0, pressed ? 176 : 164);
	update_area_eq(xp, top, w, h);
}

void draw_eq_onauto(int on, int autoon, int onpressed, int autopressed)
{
	int top = 18, left = 14;
	int w1 = 25, w2 = 33;
	int h = 12;
	BitBlt(eqMainDC, left, top, w1, h, eqMainDC, 10 + (onpressed ? 118 : 0) + (on ? 59 : 0), 119);
	BitBlt(eqMainDC, left + w1, top, w2, h, eqMainDC, 35 + (autopressed ? 118 : 0) + (autoon ? 59 : 0), 119);
	update_area_eq(left, top, w1 + w2, h);
}

void draw_eq_graphthingy()
{
	int top = 17, left = 86;
	int src_top = 294;
	int w = 113, h = 19;
	float keys[12] = {0};
	pl_Spline spline = {keys, 1, 12, 0.0f, 0.0f, 0.1f};
	BitBlt(eqMainDC, left, top, w, h, eqMainDC, 0, src_top);
	BitBlt(eqMainDC, left, top - 1 + h - (int)(config_preamp * 19.0f / 64.0f), w, 1, eqMainDC, 0, 314);
	{
		int x;
		int last_p = -1;
		for (x = 0; x < 10; x++)
			keys[x + 1] = eq_tab[x] * 19.0f / 64.0f;
		keys[0] = keys[1];
		keys[11] = keys[10];

		for (x = 0; x < 109; x++)
		{
			float p;
			int this_p;
			int lin_offs = 115;
			plSplineGetPoint(&spline, 1.0f + x / 12.0f, &p);
			this_p = (int)p;
			if (this_p < 0) this_p = 0;
			if (this_p > 18) this_p = 18;
			if (last_p == -1 || this_p == last_p)
				BitBlt(eqMainDC, left + 2 + x, top + this_p, 1, 1, eqMainDC, lin_offs, src_top + this_p);
			else
			{
				if (this_p < last_p)
					BitBlt(eqMainDC, left + 2 + x, top + this_p, 1, last_p - this_p + 1, eqMainDC, lin_offs, src_top + this_p);
				else if (this_p > last_p)
					BitBlt(eqMainDC, left + 2 + x, top + last_p, 1, this_p - last_p + 1, eqMainDC, lin_offs, src_top + last_p);
			}
			last_p = this_p;
		}
	}
	update_area_eq(left, top, w, h);
}

void draw_eq_tbutton(int b3, int wsb)
{
	const Bitmap &bmDC = g_skin.eq_ex;
	if (config_eq_ws)
	{
		if (wsb)
			BitBlt(eqMainDC, 254, 3, 9, 9, bmDC, 1, 47);
		else
			BitBlt(eqMainDC, 254, 3, 9, 9, bmDC, 254, 3);
		BitBlt(eqMainDC, 264, 3, 9, 9, bmDC, 11, 38 + b3 * 9);
	}
	else
	{
		if (wsb && g_skin.enable_eq_windowshade_button)
			BitBlt(eqMainDC, 254, 3, 9, 9, bmDC, 1, 38);
		else
			BitBlt(eqMainDC, 254, 3, 9, 9, eqMainDC, 254, 137);
		BitBlt(eqMainDC, 264, 3, 9, 9, eqMainDC, 0, 116 + b3 * 9);
	}
	update_area_eq(253, 3, 20, 9);
}
