/*
** Winamp for Linux - main window mouse handling.
**
** Port of Src/Winamp/Ui.cpp (ui_handlemouseevent and the do_* hit testing,
** unchanged in logic) and Src/Winamp/main_mouse.cpp (double click and the
** right click context menus). SendMessage(WM_COMMAND, x) became
** Main_OnCommand(x).
*/
#include "ui.h"
#include "../core/player.h"
#include "../core/playlist.h"
#include "../core/plugins.h"
#include "../core/vis.h"

#include <stdio.h>
#include <wchar.h>

#define inreg(x, y, x2, y2) \
	((mouse_x <= (x2) && mouse_x >= (x) && \
	  mouse_y <= (y2) && mouse_y >= (y)))

static int mouse_x, mouse_y, mouse_type, mouse_stats, mouse_root_x, mouse_root_y;
static int do_titlebar_clicking;

static int do_buttonbar();
static int do_shuffle();
static int do_peeq();
static int do_repeat();
static int do_eject();
static int do_icon();
static int do_posbar();
static int do_volbar();
static int do_panbar();
static int do_songname();
static int do_titlebar();
static int do_titlebuttons();
static int do_timedisplay();
static int do_clutterbar();

void ui_handlemouseevent(int x, int y, int type, int stats, int root_x, int root_y)
{
	mouse_x = x;
	mouse_y = y;
	mouse_type = type;
	mouse_stats = stats;
	mouse_root_x = root_x;
	mouse_root_y = root_y;
	if (do_titlebar_clicking || (!do_posbar() &&
		!do_volbar() && !do_panbar() && !do_songname()))
	{
		if (do_titlebar_clicking || !(do_buttonbar() +
			do_shuffle() +
			do_repeat() +
			do_eject() +
			do_peeq() +
			do_icon() +
			do_clutterbar() +
			do_timedisplay()))
		{
			if (do_titlebar_clicking || !do_titlebuttons()) do_titlebar();
		}
	}
}

static int __do_buttons(int which)
{
	int m = WINAMP_BUTTON1 + which;
	if (which == 5)
	{
		if (mouse_stats & MK_CONTROL) Main_OnCommand(WINAMP_FILE_LOC);
		else if (mouse_stats & MK_SHIFT) Main_OnCommand(WINAMP_FILE_DIR);
		else Main_OnCommand(WINAMP_FILE_PLAY);
	}
	else
	{
		if (mouse_stats & MK_SHIFT) m += 100;
		else if (mouse_stats & MK_CONTROL) m += 110;
		Main_OnCommand(m);
	}
	return 0;
}

// menus that pop up at a point of the main window (ClientToScreen)
static void popup_at(MenuId menu, int x, int y)
{
	int s = config_dsize ? 2 : 1;
	popup_menu(menu, g_main_wnd, g_main_wnd->X() + x * s, g_main_wnd->Y() + y * s);
}

static int do_clutterbar_active;
static int do_clutterbar()
{
	int en = 0, t = 0;

	if (mouse_type == -2)
		return 0;

	if (inreg(11, 24, 19, 31))
	{
		en = 1;
		if (mouse_type == -1) popup_at(MENU_OPTIONS, 14, 25);
		if (mouse_stats & MK_LBUTTON) draw_songname(L"Options Menu", &t, -1);
	}
	if (inreg(11, 32, 19, 39))
	{
		en = 2;
		if (mouse_type == -1) Main_OnCommand(WINAMP_OPTIONS_AOT);
		if (mouse_stats & MK_LBUTTON)
			draw_songname(config_aot ? L"Disable Always-On-Top" : L"Enable Always-On-Top", &t, -1);
	}
	if (inreg(11, 40, 19, 47))
	{
		en = 3;
		if (mouse_type == -1)
			if (!FileName.empty() || PlayList_getlength()) Main_OnCommand(WINAMP_EDIT_ID3);
		if (mouse_stats & MK_LBUTTON) draw_songname(L"File Info Box", &t, -1);
	}
	if (inreg(11, 48, 19, 55))
	{
		en = 4;
		if (mouse_type == -1) Main_OnCommand(WINAMP_OPTIONS_DSIZE);
		if (mouse_stats & MK_LBUTTON)
			draw_songname(config_dsize ? L"Disable Doublesize Mode" : L"Enable Doublesize Mode", &t, -1);
	}
	if (inreg(11, 56, 19, 62))
	{
		en = 5;
		if (mouse_type == -1) popup_at(MENU_VIS, 14, 56);
		if (mouse_stats & MK_LBUTTON) draw_songname(L"Visualization Menu", &t, -1);
	}
	if (inreg(9, 20, 19, 65) && mouse_stats & MK_LBUTTON)
	{
		draw_clutterbar(1 + en);
		do_clutterbar_active = 1;
		return 1;
	}
	if (do_clutterbar_active || mouse_type == -1)
	{
		draw_songname_title();
		do_clutterbar_active = 0;
		draw_clutterbar(0);
	}
	return 0;
}

static int do_timedisplay()
{
	if (mouse_type == -2)
		return 0;
	if (((!config_windowshade && inreg(36, 26, 96, 39)) ||
		(config_windowshade && inreg(129, 3, 129 + 28, 3 + 6))) && mouse_type == 1)
	{
		config_timeleftmode = !config_timeleftmode;
		display_timer_tick();
		return 1;
	}
	if (playing && inreg(27, 40, 99, 61) && mouse_type == 1)
	{
		config_sa++;
		if (config_sa > 2) config_sa = 0;
		sa_setmode(config_sa);
		return 1;
	}
	if (config_windowshade && inreg(78, 4, 116, 11) && mouse_type == 1)
	{
		config_sa++;
		if (config_sa > 2) config_sa = 0;
		sa_setmode(config_sa);
		return 1;
	}
	if (config_windowshade && inreg(168, 2, 213 + 11, 11) && mouse_type == 1)
	{
		int which = 5 - (mouse_x < 215) - (mouse_x < 204) - (mouse_x < 195) - (mouse_x < 186) - (mouse_x < 176);
		__do_buttons(which);
		return 1;
	}
	return 0;
}

static int do_titlebar()
{
	if (mouse_type == -2)
	{
		do_titlebar_clicking = 0;
		return 0;
	}
	if (do_titlebar_clicking || config_easymove || mouse_y < 14)
	{
		switch (mouse_type)
		{
		case 1:
			if (!do_titlebar_clicking)
			{
				if (!SkinWindow::CanPositionWindows())
				{
					g_main_wnd->BeginWMMove(mouse_root_x, mouse_root_y);
					return 1;
				}
				do_titlebar_clicking = 1;
				main_window_drag(1, mouse_root_x, mouse_root_y, mouse_stats);
			}
			return 1;
		case -1:
			if (do_titlebar_clicking)
			{
				main_window_drag(-1, mouse_root_x, mouse_root_y, mouse_stats);
				do_titlebar_clicking = 0;
			}
			return 1;
		case 0:
			if (do_titlebar_clicking)
				main_window_drag(0, mouse_root_x, mouse_root_y, mouse_stats);
			return 1;
		}
	}
	return 0;
}

static guint menu_timer = 0;
static int title_buttons_active[4];

static gboolean main_menu_timer(gpointer)
{
	// Main_OnTimer(666): the Winamp menu button pops up the main menu while held
	menu_timer = 0;
	title_buttons_active[0] = 0;
	draw_tbuttons(0, -1, -1, -1);
	popup_at(MENU_MAIN, 6, 14);
	return G_SOURCE_REMOVE;
}

static int do_titlebuttons()
{
	if (mouse_type == -2)
		return 0;
	if (inreg(254, 3, 262, 12)) // windowshade button
	{
		if (mouse_type == -1)
		{
			Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE);
			draw_tbuttons(-1, -1, -1, 0);
			title_buttons_active[3] = 0;
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			title_buttons_active[3] = 1;
			draw_tbuttons(-1, -1, -1, 1);
		}
		if (mouse_type) return 1;
	}
	else if (title_buttons_active[3])
	{
		title_buttons_active[3] = 0;
		draw_tbuttons(-1, -1, -1, 0);
	}

	if (inreg(244, 3, 253, 12)) // minimize button
	{
		if (mouse_type == -1)
		{
			gtk_window_iconify(GTK_WINDOW(g_main_wnd->Widget()));
			draw_tbuttons(-1, 0, -1, -1);
			title_buttons_active[1] = 0;
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			title_buttons_active[1] = 1;
			draw_tbuttons(-1, 1, -1, -1);
		}
		if (mouse_type) return 1;
	}
	else if (title_buttons_active[1])
	{
		title_buttons_active[1] = 0;
		draw_tbuttons(-1, 0, -1, -1);
	}

	if (inreg(264, 3, 272, 12)) // kill button
	{
		if (mouse_type == -1)
		{
			title_buttons_active[2] = 0;
			draw_tbuttons(-1, -1, 0, -1);
			if (mouse_stats & MK_SHIFT)
				Main_OnCommand(WINAMP_MAIN_WINDOW);
			else
				app_quit();
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			title_buttons_active[2] = 1;
			draw_tbuttons(-1, -1, 1, -1);
		}
		if (mouse_type) return 1;
	}
	else if (title_buttons_active[2])
	{
		title_buttons_active[2] = 0;
		draw_tbuttons(-1, -1, 0, -1);
	}

	if (inreg(5, 3, 17, 13))
	{
		if (mouse_stats & MK_LBUTTON && !title_buttons_active[0])
		{
			if (menu_timer) g_source_remove(menu_timer);
			menu_timer = g_timeout_add(155, main_menu_timer, nullptr);
			title_buttons_active[0] = 1;
			draw_tbuttons(1, -1, -1, -1);
		}
		if (mouse_type) return 1;
	}
	else if (title_buttons_active[0])
	{
		if (menu_timer)
		{
			g_source_remove(menu_timer);
			menu_timer = 0;
		}
		title_buttons_active[0] = 0;
		draw_tbuttons(0, -1, -1, -1);
	}

	return 0;
}

static int do_buttonbar_needupdate;

static int do_buttonbar()
{
	if (mouse_type == -2)
		return 0;
	if (inreg(7 + 8, 15 + 72, 7 + 123, 15 + 91))
	{
		int which = 4 - (mouse_x < 100 + 7) - (mouse_x < 77 + 7) - (mouse_x < 53 + 7) - (mouse_x < 31 + 7);
		switch (mouse_type)
		{
		case 1: // mousedown
			draw_buttonbar(which);
			do_buttonbar_needupdate = 1;
			break;
		case 0: // mousemove
			if (mouse_stats & MK_LBUTTON)
			{
				draw_buttonbar(which);
				do_buttonbar_needupdate = 1;
			}
			break;
		case -1: // mouseup
			do_buttonbar_needupdate = 0;
			draw_buttonbar(-1);
			__do_buttons(which);
			break;
		}
		return 1;
	}
	if (do_buttonbar_needupdate)
	{
		do_buttonbar_needupdate = 0;
		draw_buttonbar(-1);
	}
	return 0;
}

static int do_eq_needupdate, do_pe_needupdate;

static int do_peeq()
{
	if (mouse_type == -2)
	{
		if (do_eq_needupdate || do_pe_needupdate)
		{
			draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
			do_eq_needupdate = 0;
			do_pe_needupdate = 0;
			return 1;
		}
		return 0;
	}

	if (inreg(219, 58, 219 + 23, 58 + 12)) // eq
	{
		switch (mouse_type)
		{
		case 1:
			draw_eqplbut(config_eq_open, 1, config_pe_open, 0);
			do_eq_needupdate = 1;
			break;
		case 0:
			if (mouse_stats & MK_LBUTTON)
			{
				draw_eqplbut(config_eq_open, 1, config_pe_open, 0);
				do_eq_needupdate = 1;
			}
			break;
		case -1:
			do_eq_needupdate = 0;
			Main_OnCommand(WINAMP_OPTIONS_EQ);
			break;
		}
		return 1;
	}
	if (do_eq_needupdate)
	{
		draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
		do_eq_needupdate = 0;
	}
	if (inreg(219 + 23, 58, 219 + 23 + 23, 58 + 12)) // pl
	{
		switch (mouse_type)
		{
		case 1:
			draw_eqplbut(config_eq_open, 0, config_pe_open, 1);
			do_pe_needupdate = 1;
			break;
		case 0:
			if (mouse_stats & MK_LBUTTON)
			{
				draw_eqplbut(config_eq_open, 0, config_pe_open, 1);
				do_pe_needupdate = 1;
			}
			break;
		case -1:
			do_pe_needupdate = 0;
			Main_OnCommand(WINAMP_OPTIONS_PLEDIT);
			break;
		}
		return 1;
	}
	if (do_pe_needupdate)
	{
		draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
		do_pe_needupdate = 0;
	}

	return 0;
}

static int do_shuffle_needupdate;

static int do_shuffle()
{
	if (mouse_type == -2)
		return 0;
	if (inreg(133 + 7 - 4 + 29, 75 + 15, 181 + 7 - 4 - 4 + 29, 87 + 15))
	{
		switch (mouse_type)
		{
		case 1:
			draw_shuffle(config_shuffle, 1);
			do_shuffle_needupdate = 1;
			break;
		case 0:
			if (mouse_stats & MK_LBUTTON)
			{
				draw_shuffle(config_shuffle, 1);
				do_shuffle_needupdate = 1;
			}
			break;
		case -1:
			do_shuffle_needupdate = 0;
			Main_OnCommand(WINAMP_FILE_SHUFFLE);
			break;
		}
		return 1;
	}
	if (do_shuffle_needupdate)
	{
		draw_shuffle(config_shuffle, 0);
		do_shuffle_needupdate = 0;
	}
	return 0;
}

static int do_repeat_needupdate;

static int do_repeat()
{
	if (mouse_type == -2)
		return 0;
	if (inreg(182 + 7 - 4 - 4 + 29, 75 + 15, 182 + 29 + 7 - 4 - 4 + 29, 87 + 15))
	{
		switch (mouse_type)
		{
		case 1:
			draw_repeat(config_repeat, 1);
			do_repeat_needupdate = 1;
			break;
		case 0:
			if (mouse_stats & MK_LBUTTON)
			{
				draw_repeat(config_repeat, 1);
				do_repeat_needupdate = 1;
			}
			break;
		case -1:
			do_repeat_needupdate = 0;
			Main_OnCommand(WINAMP_FILE_REPEAT);
			break;
		}
		return 1;
	}
	if (do_repeat_needupdate)
	{
		draw_repeat(config_repeat, 0);
		do_repeat_needupdate = 0;
	}
	return 0;
}

static int do_eject_needupdate;

static int do_eject()
{
	if (mouse_type == -2)
	{
		if (do_eject_needupdate)
		{
			draw_eject(0);
			return 1;
		}
		return 0;
	}
	if (inreg(132 + 7 - 4 + 1, 60 + 14 + 15, 132 + 7 - 4 + 22 + 1, 60 + 14 + 15 + 16))
	{
		switch (mouse_type)
		{
		case 1:
			draw_eject(1);
			do_eject_needupdate = 1;
			break;
		case 0:
			if (mouse_stats & MK_LBUTTON)
			{
				draw_eject(1);
				do_eject_needupdate = 1;
			}
			break;
		case -1:
			do_eject_needupdate = 0;
			draw_eject(0);
			if (mouse_stats & MK_CONTROL) Main_OnCommand(WINAMP_FILE_LOC);
			else if (mouse_stats & MK_SHIFT) Main_OnCommand(WINAMP_FILE_DIR);
			else Main_OnCommand(WINAMP_FILE_PLAY);
			break;
		}
		return 1;
	}
	if (do_eject_needupdate)
	{
		draw_eject(0);
		do_eject_needupdate = 0;
	}
	return 0;
}

static int do_icon()
{
	if (mouse_type == -2)
		return 0;
	if (inreg(246 + 7, 76 + 15, 258 + 7, 90 + 15))
	{
		if (mouse_type == -1)
			Main_OnCommand(WINAMP_LIGHTNING_CLICK);
		return 1;
	}
	return 0;
}

int do_posbar_active, do_posbar_clickx = 14;

static int MulDiv(int a, int b, int c)
{
	return c ? (int)(((long long)a * b) / c) : 0;
}

static int do_posbar()
{
	int a, t;

	if (mouse_type == -2)
	{
		if (do_posbar_active)
		{
			do_posbar_active = 0;
			return 1;
		}
		return 0;
	}

	if (!in_seekable())
	{
		if (mouse_type == -1 && do_posbar_active) do_posbar_active = 0;
		return 0;
	}

	if ((mouse_type == -1 || (mouse_type == 0 && (mouse_stats & MK_SHIFT) && (mouse_stats & MK_LBUTTON))) && do_posbar_active)
	{
		if (mouse_type == -1)
			do_posbar_active = 0;
		if (config_windowshade)
		{
			a = mouse_x - 228;
			a *= (257 - 29 - 10);
			a /= 13;
		}
		else
		{
			a = mouse_x - do_posbar_clickx - (10 + 7); // posbar_clickx is offset from start of bar
			do_posbar_clickx = 14;
		}

		if (a < 0) a = 0;
		if (a > 257 - 29 - 10) a = 257 - 29 - 10;
		t = MulDiv(in_getlength(), 1000 * a, (257 - 29 - 10));
		if (in_seek(t) < 0) StopPlaying(0);
		if (mouse_type == -1)
		{
			draw_songname_title();
			return 1;
		}
	}
	if (mouse_type == 1)
		if ((inreg(11 + 7, 58 + 15, 256 + 7, 66 + 15) && !config_windowshade) || (inreg(228, 3, 228 + 14, 12) && config_windowshade))
		{
			do_posbar_active = 1;
			if (!config_windowshade)
			{
				int len = in_getlength();
				int curpos = 11 + 7 + (len > 0 ? ((in_getouttime() * (257 - 10 - 29)) / len) / 1000 : 0);
				if (mouse_x <= curpos + 29 && mouse_x >= curpos - 1) do_posbar_clickx = mouse_x - curpos;
				else do_posbar_clickx = 14;
			}
			else do_posbar_clickx = 0;
		}

	if (do_posbar_active)
	{
		if (config_windowshade)
		{
			a = mouse_x - 228;
			a *= (257 - 29 - 10);
			a /= 13;
		}
		else
		{
			a = mouse_x - do_posbar_clickx - (10 + 7);
		}
		if (a < 0) a = 0;
		if (a > 257 - 29 - 10) a = 257 - 29 - 10;
		t = MulDiv(in_getlength(), 1000 * a, (257 - 29 - 10)) / 1000;
		{
			wchar_t buf[256];
			int a = in_getlength();
			if (a) draw_positionbar((t * 256) / a, 1);
			else draw_positionbar(0, 1);
			swprintf(buf, 256, L"Seek to: %02d:%02d/%02d:%02d (%d%%)", t / 60, t % 60, a / 60, a % 60, (t * 100) / (a ? a : 1));
			if (config_windowshade) draw_time(t / 60, t % 60, 0);
			t = 0;
			draw_songname(buf, &t, -1);
		}
		return 1;
	}
	return 0;
}

void ui_drawtime(int time_elapsed, int mode)
{
	if (time_elapsed < 0) time_elapsed = 0;
	if (!mode && paused)
	{
		static int i;
		draw_time(time_elapsed / 60, time_elapsed % 60, i);
		i ^= 1;
	}
	else
	{
		if (!do_posbar_active || !config_windowshade)
			draw_time(time_elapsed / 60, time_elapsed % 60, 0);
	}
	if (playing)
		if (!do_posbar_active && in_seekable())
		{
			if (in_getlength() > 0) draw_positionbar((time_elapsed * 256) / in_getlength(), mode);
			else draw_positionbar(0, mode);
		}
}

int do_volbar_clickx = 4, do_volbar_active;

static int do_volbar()
{
	if (mouse_type == -2)
	{
		if (do_volbar_active)
		{
			do_volbar_active = 0;
			draw_volumebar(config_volume, 0);
			return 1;
		}
		return 0;
	}

	if (mouse_type == -1 && do_volbar_active)
	{
		do_volbar_active = 0;
		do_volbar_clickx = 7;
		draw_volumebar(config_volume, 0);
		in_setvol(config_volume);
		draw_songname_title();
		if (config_eq_ws) draw_eq_tbar(g_eq_wnd->Active() ? 1 : (config_hilite ? 0 : 1));
		return 1;
	}

	if (inreg(107, 43 + 15, 107 + 68, 51 + 15) && mouse_type == 1)
	{
		int vs = 107 + (config_volume * 51) / 255;
		if (mouse_x < vs + 15 && mouse_x >= vs) do_volbar_clickx = mouse_x - vs;
		else do_volbar_clickx = 7;
		do_volbar_active = 1;
	}

	if (do_volbar_active)
	{
		int v = mouse_x - do_volbar_clickx - (107);
		v = (v * 255) / 51;
		if (v < 0) v = 0;
		if (v > 255) v = 255;
		config_volume = v;
		draw_volumebar(config_volume, 1);
		in_setvol(config_volume);
		update_volume_text(-1);
		return 1;
	}
	return 0;
}

int do_panbar_clickx = 4, do_panbar_active;

static int do_panbar()
{
	if (mouse_type == -2)
	{
		if (do_panbar_active)
		{
			draw_panbar(config_pan, 0);
			do_panbar_active = 0;
			return 1;
		}
		return 0;
	}

	if (mouse_type == -1 && do_panbar_active)
	{
		do_panbar_active = 0;
		do_panbar_clickx = 7;
		if (config_pan < 27 && config_pan > -27) config_pan = 0;
		draw_panbar(config_pan, 0);
		in_setpan(config_pan);
		draw_songname_title();
		if (config_eq_ws) draw_eq_tbar(g_eq_wnd->Active() ? 1 : (config_hilite ? 0 : 1));
		return 1;
	}

	if (inreg(177, 43 + 15, 177 + 38, 51 + 15) && mouse_type == 1)
	{
		int vs = 177 + 12 + (config_pan * 12) / 127;
		if (mouse_x < vs + 15 && mouse_x >= vs) do_panbar_clickx = mouse_x - vs;
		else do_panbar_clickx = 7;
		do_panbar_active = 1;
	}

	if (do_panbar_active)
	{
		int v = mouse_x - do_panbar_clickx - (177);
		if (v < 0) v = 0;
		if (v > 24) v = 24;
		v -= 12;
		// changed in 5.64 to have a lower limit (~16% vs 24%) and for
		// holding shift to drop the central clamp (allows 7% balance)
		if (!(mouse_stats & MK_SHIFT))
			if (v < 2 && v > -2) v = 0;
		v *= 127;
		v /= 12;
		config_pan = v;
		draw_panbar(config_pan, 1);
		in_setpan(config_pan);
		update_panning_text(-1);
		return 1;
	}
	return 0;
}

int ui_songposition = 0;
int ui_songposition_tts = 10;
static int do_songname_clickx = -1;
static int do_songname_active = 0;

void ui_doscrolling()
{
	if (!do_clutterbar_active && !ui_songposition_tts && !do_songname_active && !do_volbar_active && !do_panbar_active && !do_posbar_active)
	{
		ui_songposition += 5;
		draw_songname_title();
	}
	else if (ui_songposition_tts) ui_songposition_tts--;
	else ui_songposition_tts = 10;
}

static int do_songname()
{
	if (mouse_type == -2)
		return 0;
	if (mouse_stats & MK_LBUTTON || mouse_type == -1)
		if (do_songname_active || (inreg(117, 24, 266, 35) && mouse_type == 1))
		{
			if (mouse_type == -1)
			{
				do_songname_clickx = -1;
				do_songname_active = 0;
				ui_songposition_tts = 10;
				return 1;
			}
			if (do_songname_active)
			{
				ui_songposition -= mouse_x - do_songname_clickx;
				draw_songname_title();
			}
			do_songname_clickx = mouse_x;
			do_songname_active = 1;
			return 1;
		}
	return 0;
}

void ui_reset()
{
	title_buttons_active[0] = title_buttons_active[1] = title_buttons_active[2] = 0;
	do_clutterbar_active = 0;
	do_songname_clickx = -1;
	do_songname_active = 0;
	do_buttonbar_needupdate = 0;
	do_shuffle_needupdate = 0;
	do_eq_needupdate = do_pe_needupdate = 0;
	do_repeat_needupdate = 0;
	do_eject_needupdate = 0;
	do_posbar_active = 0;
	do_posbar_clickx = 14;
	do_volbar_active = 0;
	do_volbar_clickx = 7;
	do_panbar_active = 0;
	do_panbar_clickx = 7;
	ui_songposition = 0;
	ui_songposition_tts = 0;
}

/* ---------------- Src/Winamp/main_mouse.cpp ---------------- */

class MainHandler : public SkinWindow::Handler
{
public:
	void OnMouse(SkinWindow *, int x, int y, int type, int stats, int rx, int ry) override
	{
		ui_handlemouseevent(x, y, type, stats, rx, ry);
	}

	// Doubleclick handler. Checks for a few regions, and usually does nothing,
	// except pass on a WM_LBUTTONDOWN.
	void OnDoubleClick(SkinWindow *, int nx, int ny, int stats, int rx, int ry) override
	{
		if (nx <= 16 && nx >= 6 && ny <= 12 && ny >= 4)
		{
			if (menu_timer)
			{
				g_source_remove(menu_timer);
				menu_timer = 0;
			}
			app_quit();
		}
		else if (config_windowshade && nx <= 158 && nx >= 129 && ny <= 10 && ny >= 5)
			ui_handlemouseevent(nx, ny, 1, stats, rx, ry);
		else if (config_windowshade && nx <= 213 && nx >= 168 && ny <= 11 && ny >= 2)
			ui_handlemouseevent(nx, ny, 1, stats, rx, ry);
		else if (nx < 266 && nx > 117 && ny < 35 && ny > 24)
			Main_OnCommand(WINAMP_EDIT_ID3);
		else if ((nx >= 24 && ny >= 43 && nx < 24 + 76 && ny < 43 + 16) ||
		         (config_windowshade && nx >= 79 && ny >= 5 && nx < 79 + 38 && ny < 5 + 5))
		{
			if (config_windowshade) config_sa = !config_sa;
			else
			{
				config_sa += 2;
				config_sa %= 3;
			}
			sa_setmode(config_sa);
		}
		else if (ny < 15 && nx < 244)
		{
			do_titlebar_clicking = 0;
			Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE);
		}
		else
			ui_handlemouseevent(nx, ny, 1, stats, rx, ry);
	}

	void OnRightClick(SkinWindow *, int x, int y, int) override
	{
		MenuId m = MENU_MAIN;
		if (x < 266 && x > 105 && y < 35 && y > 24 && !config_windowshade)
			m = MENU_CTX_SONGTITLE;
		else if ((!config_windowshade && x >= 36 && y >= 26 && x < 96 && y < 39) ||
		         (config_windowshade && x >= 129 && y >= 3 && x < 129 + 28 && y < 3 + 6))
			m = MENU_CTX_TIME;
		else if ((x >= 27 && y >= 40 && x < 99 && y < 61 && !config_windowshade) ||
		         (config_windowshade && x >= 78 && y >= 4 && x < 116 && y < 11))
			m = MENU_VIS;
		else if ((x >= 16 && y >= 88 && x < 37 && y < 106 && !config_windowshade) ||
		         (config_windowshade && x >= 167 && y >= 3 && x < 176 && y < 12))
			m = MENU_CTX_PREV;
		else if ((x >= 37 && y >= 88 && x < 62 && y < 106 && !config_windowshade) ||
		         (config_windowshade && x >= 176 && y >= 3 && x < 186 && y < 12))
			m = MENU_CTX_PLAY;
		else if ((x >= 62 && y >= 88 && x < 89 && y < 106 && !config_windowshade) ||
		         (config_windowshade && x >= 186 && y >= 3 && x < 196 && y < 12))
			m = MENU_CTX_PAUSE;
		else if ((x >= 89 && y >= 88 && x < 107 && y < 106 && !config_windowshade) ||
		         (config_windowshade && x >= 196 && y >= 3 && x < 206 && y < 12))
			m = MENU_CTX_STOP;
		else if ((x >= 107 && y >= 88 && x < 130 && y < 106 && !config_windowshade) ||
		         (config_windowshade && x >= 206 && y >= 3 && x < 216 && y < 12))
			m = MENU_CTX_NEXT;
		else if ((x >= 136 && y >= 89 && x < 158 && y < 105 && !config_windowshade) ||
		         (config_windowshade && x >= 215 && y >= 3 && x < 225 && y < 12))
			m = MENU_CTX_EJECT;
		else if (in_seekable() && x >= 18 && y >= 73 && x < 263 && y < 81 && !config_windowshade)
			m = MENU_CTX_SEEK;
		else if (x >= 164 && y >= 89 && x < 211 && y < 104 && !config_windowshade)
			m = MENU_CTX_SHUFFLE;
		else if (x >= 211 && y >= 89 && x < 211 + 28 && y < 104 && !config_windowshade)
			m = MENU_CTX_REPEAT;
		else if (x >= 219 && y >= 58 && x < 219 + (265 - 219) / 2 && y < 70 && !config_windowshade)
			m = MENU_CTX_EQ;
		else if (x >= 219 + (265 - 219) / 2 && y >= 58 && x < 265 && y < 70 && !config_windowshade)
			m = MENU_CTX_PE;
		popup_menu(m, g_main_wnd);
	}

	void OnScroll(SkinWindow *, int, int, int delta, int) override
	{
		// WM_MOUSEWHEEL on the main window changes the volume
		Main_OnCommand(delta > 0 ? WINAMP_VOLUMEUP : WINAMP_VOLUMEDOWN);
	}

	void OnActivate(SkinWindow *, bool active) override
	{
		draw_tbar(active ? 1 : (config_hilite ? 0 : 1), config_windowshade, eggstat);
		if (active)
		{
			// Winamp raises its other windows along with the main window
			if (g_eq_wnd && g_eq_wnd->Visible()) gdk_window_raise(gtk_widget_get_window(g_eq_wnd->Widget()));
			if (g_pl_wnd && g_pl_wnd->Visible()) gdk_window_raise(gtk_widget_get_window(g_pl_wnd->Widget()));
		}
	}

	// Main_OnDropFiles: replaces the playlist and plays, or enqueues with shift held
	void OnDrop(SkinWindow *, int, int, const std::vector<std::string> &files) override
	{
		GdkModifierType mods = (GdkModifierType)0;
		GdkDisplay *d = gdk_display_get_default();
		GdkSeat *seat = gdk_display_get_default_seat(d);
		gdk_window_get_device_position(gtk_widget_get_window(g_main_wnd->Widget()), gdk_seat_get_pointer(seat), nullptr, nullptr, &mods);
		bool enqueue = mods & GDK_SHIFT_MASK;
		if (!enqueue)
		{
			StopPlaying(0);
			PlayList_clear();
		}
		for (auto &f : files) PlayList_add(f);
		if (!enqueue)
		{
			PlayList_setposition(0);
			StartPlaying();
		}
		plEditRefresh();
	}

	void OnClose(SkinWindow *) override
	{
		app_quit();
	}
};

static MainHandler main_handler_impl;
SkinWindow::Handler *main_handler = &main_handler_impl;
