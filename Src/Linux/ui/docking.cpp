/*
** Winamp for Linux - window docking and snapping.
**
** The rectangle helpers are Src/Winamp/DOCK.cpp; main_window_drag() is the
** "do_titlebar_clicking" part of Src/Winamp/Ui.cpp that moves the main window
** together with the windows docked to it and snaps to screen edges/windows.
** The only change is that the drag works with screen coordinates, because
** on X11 the window position updates asynchronously.
*/
#include "ui.h"

#include <gdk/gdk.h>

#define WA_SNAP_EQ 2
#define WA_SNAP_PL 4

void EstMainWindowRect(RECT *r)
{
	r->left = config_wx;
	r->top = config_wy;
	r->right = config_wx + (WINDOW_WIDTH << (config_dsize ? 1 : 0));
	r->bottom = config_wy + ((config_windowshade ? 14 : WINDOW_HEIGHT) << (config_dsize ? 1 : 0));
}

void EstEQWindowRect(RECT *r)
{
	r->left = config_eq_wx;
	r->top = config_eq_wy;
	r->right = config_eq_wx + (WINDOW_WIDTH << (config_dsize && config_eqdsize ? 1 : 0));
	r->bottom = config_eq_wy + ((config_eq_ws ? 14 : WINDOW_HEIGHT) << (config_dsize && config_eqdsize ? 1 : 0));
}

void EstPLWindowRect(RECT *r)
{
	r->left = config_pe_wx;
	r->top = config_pe_wy;
	r->right = config_pe_wx + config_pe_width;
	r->bottom = config_pe_wy + config_pe_height;
}

void SetMainWindowRect(const RECT *r)
{
	config_wx = r->left;
	config_wy = r->top;
}

void SetEQWindowRect(const RECT *r)
{
	config_eq_wx = r->left;
	config_eq_wy = r->top;
}

void SetPLWindowRect(const RECT *r)
{
	config_pe_wx = r->left;
	config_pe_wy = r->top;
}

void MoveRect(RECT *r, int x, int y)
{
	r->left += x;
	r->right += x;
	r->top += y;
	r->bottom += y;
}

int IsWindowAttached(RECT rc, RECT rc2)
{
#define INREG(x, l, h) ((x) >= (l) && (x) <= (h))
	int r = 0;
	if (rc2.right == rc.left || rc2.left == rc.right)
	{
		if (INREG(rc.top, rc2.top, rc2.bottom) || INREG(rc.bottom, rc2.top, rc2.bottom) ||
			INREG(rc2.top, rc.top, rc.bottom) || INREG(rc2.bottom, rc.top, rc.bottom))
			r |= 1;
	}
	if (rc2.bottom == rc.top || rc2.top == rc.bottom)
	{
		if (INREG(rc2.left, rc.left, rc.right) || INREG(rc2.right, rc.left, rc.right) ||
			INREG(rc.left, rc2.left, rc2.right) || INREG(rc.right, rc2.left, rc2.right))
			r |= 2;
	}
#undef INREG
	return r;
}

void SnapWindowToWindow(RECT *rcSrc, RECT rcDest)
{
#define INREG(x, l, h) ((x) >= (l) && (x) <= (h))
#define IRR(l1, r1, l2, r2) (INREG(l1, l2, r2) || INREG(r1, l2, r2) || INREG(l2, l1, r1) || INREG(r2, l1, r1))
#define CLOSETO(x, t) INREG(x, t - config_snaplen, t + config_snaplen)
	if (IRR(rcDest.left, rcDest.right, rcSrc->left, rcSrc->right))
	{
		if (CLOSETO(rcSrc->top, rcDest.bottom))
		{
			rcSrc->bottom += rcDest.bottom - rcSrc->top;
			rcSrc->top = rcDest.bottom;
		}
		else if (CLOSETO(rcSrc->bottom, rcDest.top))
		{
			rcSrc->top = rcDest.top - (rcSrc->bottom - rcSrc->top);
			rcSrc->bottom = rcDest.top;
		}
	}

	if (IRR(rcDest.top, rcDest.bottom, rcSrc->top, rcSrc->bottom))
	{
		if (CLOSETO(rcSrc->right, rcDest.left))
		{
			rcSrc->left = rcDest.left - (rcSrc->right - rcSrc->left);
			rcSrc->right = rcDest.left;
		}
		else if (CLOSETO(rcSrc->left, rcDest.right))
		{
			rcSrc->right += (rcDest.right - rcSrc->left);
			rcSrc->left = rcDest.right;
		}
	}

	if (rcSrc->right == rcDest.left || rcSrc->left == rcDest.right)
	{
		if (CLOSETO(rcSrc->top, rcDest.top))
		{
			rcSrc->bottom += rcDest.top - rcSrc->top;
			rcSrc->top = rcDest.top;
		}
		else if (CLOSETO(rcSrc->bottom, rcDest.bottom))
		{
			rcSrc->top += rcDest.bottom - rcSrc->bottom;
			rcSrc->bottom = rcDest.bottom;
		}
	}

	if (rcSrc->bottom == rcDest.top || rcSrc->top == rcDest.bottom)
	{
		if (CLOSETO(rcSrc->left, rcDest.left))
		{
			rcSrc->right += rcDest.left - rcSrc->left;
			rcSrc->left = rcDest.left;
		}
		else if (CLOSETO(rcSrc->right, rcDest.right))
		{
			rcSrc->left += rcDest.right - rcSrc->right;
			rcSrc->right = rcDest.right;
		}
	}
#undef INREG
#undef IRR
#undef CLOSETO
}

// the work area of the monitor the rectangle is on (getViewport)
static void get_viewport(const RECT *r, RECT *out)
{
	GdkDisplay *d = gdk_display_get_default();
	GdkMonitor *m = gdk_display_get_monitor_at_point(d, (r->left + r->right) / 2, (r->top + r->bottom) / 2);
	GdkRectangle wa = {0, 0, 1024, 768};
	if (m) gdk_monitor_get_workarea(m, &wa);
	out->left = wa.x;
	out->top = wa.y;
	out->right = wa.x + wa.width;
	out->bottom = wa.y + wa.height;
}

bool SnapToScreen(RECT *outrc)
{
	if (config_keeponscreen & 1)
	{
		RECT rc;
		int w = outrc->right - outrc->left;
		int h = outrc->bottom - outrc->top;

		get_viewport(outrc, &rc);

		if (outrc->left < (rc.left + config_snaplen) && outrc->left > (rc.left - config_snaplen))
		{
			outrc->left = rc.left;
			outrc->right = rc.left + w;
		}
		if (outrc->top < (rc.top + config_snaplen) && outrc->top > (rc.top - config_snaplen))
		{
			outrc->top = rc.top;
			outrc->bottom = rc.top + h;
		}
		if (outrc->right > rc.right - config_snaplen && outrc->right < rc.right + config_snaplen)
		{
			outrc->left = rc.right - w;
			outrc->right = rc.right;
		}
		if (outrc->bottom > rc.bottom - config_snaplen && outrc->bottom < rc.bottom + config_snaplen)
		{
			outrc->top = rc.bottom - h;
			outrc->bottom = rc.bottom;
			return true;
		}
	}
	return false;
}

void SnapWindowToAllWindows(RECT *outrc, SkinWindow *no_snap)
{
	RECT rc;
	SnapToScreen(outrc);
	if (config_pe_open && no_snap != g_pl_wnd)
	{
		EstPLWindowRect(&rc);
		SnapWindowToWindow(outrc, rc);
	}
	if (config_eq_open && no_snap != g_eq_wnd)
	{
		EstEQWindowRect(&rc);
		SnapWindowToWindow(outrc, rc);
	}
	if (config_mw_open && no_snap != g_main_wnd)
	{
		EstMainWindowRect(&rc);
		SnapWindowToWindow(outrc, rc);
	}
}

/* ---- Set.cpp doMyDirtyShitholeDockingShit(): when a window changes size, the
   windows docked to its bottom or right edge follow the edge (recursively) ---- */

static RECT est_rect(int w)
{
	RECT r;
	if (w == 0) EstMainWindowRect(&r);
	else if (w == 1) EstEQWindowRect(&r);
	else EstPLWindowRect(&r);
	return r;
}

static int is_open(int w)
{
	return w == 0 ? config_mw_open : w == 1 ? config_eq_open : config_pe_open;
}

// before[]: the window rectangles before any change (GetWindowRect() in the
// original, where windows only move after all the config positions are set)
static void dock_follow(int me, RECT oldr, RECT newr, const RECT *before, int depth)
{
	if (depth > 3) return;
	for (int whichedge = 0; whichedge < 2; whichedge++)
	{
		int diff = whichedge == 0 ? newr.bottom - oldr.bottom : newr.right - oldr.right;
		if (!diff) continue;
		for (int x = 0; x < 3; x++)
		{
			if (x == me || !is_open(x)) continue;
			RECT oldrect = before[x];
			int isdocked;
			if (!whichedge)
				isdocked = oldrect.top == oldr.bottom && (
					(oldrect.left >= oldr.left && oldrect.left < oldr.right) ||
					(oldrect.right > oldr.left && oldrect.right <= oldr.right) ||
					(oldr.left >= oldrect.left && oldr.left < oldrect.right) ||
					(oldr.right > oldrect.left && oldr.right <= oldrect.right));
			else
				isdocked = oldrect.left == oldr.right && (
					(oldrect.top >= oldr.top && oldrect.top < oldr.bottom) ||
					(oldrect.bottom > oldr.top && oldrect.bottom <= oldr.bottom) ||
					(oldr.top >= oldrect.top && oldr.top < oldrect.bottom) ||
					(oldr.bottom > oldrect.top && oldr.bottom <= oldrect.bottom));
			if (!isdocked) continue;
			int *wx = x == 0 ? &config_wx : x == 1 ? &config_eq_wx : &config_pe_wx;
			int *wy = x == 0 ? &config_wy : x == 1 ? &config_eq_wy : &config_pe_wy;
			if (!whichedge) *wy = newr.bottom;
			else *wx = newr.right;
			dock_follow(x, oldrect, est_rect(x), before, depth + 1);
		}
	}
}

void resize_with_docking(const RECT before[3])
{
	for (int w = 0; w < 3; w++)
	{
		RECT now = est_rect(w);
		if (is_open(w) && (now.right - now.left != before[w].right - before[w].left ||
		                   now.bottom - now.top != before[w].bottom - before[w].top))
		{
			// "now" already includes a move caused by a window it is docked to
			dock_follow(w, before[w], now, before, 0);
		}
	}
	set_window_positions();
}

void window_rects(RECT out[3])
{
	for (int w = 0; w < 3; w++) out[w] = est_rect(w);
}

void set_window_positions()
{
	if (g_main_wnd) g_main_wnd->Move(config_wx, config_wy);
	if (g_eq_wnd) g_eq_wnd->Move(config_eq_wx, config_eq_wy);
	if (g_pl_wnd) g_pl_wnd->Move(config_pe_wx, config_pe_wy);
}

/* ---- Ui.cpp do_titlebar(), with screen coordinates ---- */

static int do_titlebar_clicking;
static int click_root_x, click_root_y;
static RECT start_main;

void main_window_drag(int type, int root_x, int root_y, int stats)
{
	bool snap = (!!config_snap) ^ (!!(stats & MK_SHIFT));
	if (type == 1)
	{
		// find the windows docked to the main window (directly or through each other)
		struct rec
		{
			int open;
			int at;
			RECT r;
		} a[3] = {};
		a[0].at = 1;
		a[0].open = 1;
		EstMainWindowRect(&a[0].r);
		EstEQWindowRect(&a[1].r);
		a[1].open = config_eq_open;
		EstPLWindowRect(&a[2].r);
		a[2].open = config_pe_open;

		do_titlebar_clicking = 1;
		if (snap)
		{
			int x = 0, t = 0;
			const int cnt = 3;
			for (;;)
			{
				if (a[x].open && a[x].at)
					for (int b = 0; b < cnt; b++)
					{
						if (b != x && a[b].open && !a[b].at && IsWindowAttached(a[x].r, a[b].r))
						{
							a[b].at = 1;
							t = 1;
						}
					}
				if (!x)
				{
					if (!t) break;
					t = 0;
				}
				x++;
				x %= cnt;
			}
			if (a[1].at) do_titlebar_clicking |= WA_SNAP_EQ;
			if (a[2].at) do_titlebar_clicking |= WA_SNAP_PL;
		}
		click_root_x = root_x;
		click_root_y = root_y;
		start_main = a[0].r;
		return;
	}
	if (type == -1)
	{
		do_titlebar_clicking = 0;
		return;
	}
	if (!do_titlebar_clicking) return;

	RECT eqr, plr, mwr, omwr;
	EstEQWindowRect(&eqr);
	EstPLWindowRect(&plr);
	EstMainWindowRect(&omwr);
	mwr = start_main;
	MoveRect(&mwr, root_x - click_root_x, root_y - click_root_y);

	// snap to nondocked windows
	if (snap)
	{
		SnapToScreen(&mwr);
		if (config_eq_open && !(do_titlebar_clicking & WA_SNAP_EQ)) SnapWindowToWindow(&mwr, eqr);
		if (config_pe_open && !(do_titlebar_clicking & WA_SNAP_PL)) SnapWindowToWindow(&mwr, plr);
	}

	// move docked windows the same amount the the main window has moved
	int movex = mwr.left - omwr.left;
	int movey = mwr.top - omwr.top;
	if (do_titlebar_clicking & WA_SNAP_EQ) MoveRect(&eqr, movex, movey);
	if (do_titlebar_clicking & WA_SNAP_PL) MoveRect(&plr, movex, movey);

	if (snap && (config_keeponscreen & 1))
	{
		// dock the attached windows to the screen
		RECT *rs[2] = {&eqr, &plr};
		int masks[2] = {WA_SNAP_EQ, WA_SNAP_PL};
		int opens[2] = {config_eq_open, config_pe_open};
		for (int x = 0; x < 2; x++)
		{
			if (opens[x] && (do_titlebar_clicking & masks[x]))
			{
				RECT oldr = *rs[x];
				SnapToScreen(rs[x]);
				int dx = rs[x]->left - oldr.left, dy = rs[x]->top - oldr.top;
				if (dx || dy)
				{
					MoveRect(&mwr, dx, dy);
					for (int y = 0; y < 2; y++)
						if (y != x && (do_titlebar_clicking & masks[y])) MoveRect(rs[y], dx, dy);
				}
			}
		}
		RECT o = mwr;
		SnapToScreen(&mwr);
		if (do_titlebar_clicking & WA_SNAP_EQ) MoveRect(&eqr, mwr.left - o.left, mwr.top - o.top);
		if (do_titlebar_clicking & WA_SNAP_PL) MoveRect(&plr, mwr.left - o.left, mwr.top - o.top);
	}

	if (snap)
	{
		// dock the attached windows to the non attached windows
		RECT *rs[2] = {&eqr, &plr};
		int masks[2] = {WA_SNAP_EQ, WA_SNAP_PL};
		int opens[2] = {config_eq_open, config_pe_open};
		for (int x = 0; x < 2; x++)
		{
			if ((do_titlebar_clicking & masks[x]) || !opens[x]) continue; // only non docked targets
			for (int x2 = 0; x2 < 2; x2++)
			{
				if (x2 == x || !opens[x2] || !(do_titlebar_clicking & masks[x2])) continue;
				RECT r4 = *rs[x2], r3 = *rs[x2];
				SnapWindowToWindow(&r4, *rs[x]);
				int dx = r4.left - r3.left, dy = r4.top - r3.top;
				MoveRect(&mwr, dx, dy);
				for (int y = 0; y < 2; y++)
					if (do_titlebar_clicking & masks[y]) MoveRect(rs[y], dx, dy);
			}
		}
	}

	if (do_titlebar_clicking & WA_SNAP_EQ)
	{
		SetEQWindowRect(&eqr);
		g_eq_wnd->Move(config_eq_wx, config_eq_wy);
	}
	if (do_titlebar_clicking & WA_SNAP_PL)
	{
		SetPLWindowRect(&plr);
		g_pl_wnd->Move(config_pe_wx, config_pe_wy);
	}
	SetMainWindowRect(&mwr);
	g_main_wnd->Move(config_wx, config_wy);
}
