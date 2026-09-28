/*
** Winamp for Linux - equalizer window behaviour.
**
** equi_handlemouseevent() and the do_* functions are Src/Winamp/Equi.cpp;
** the window procedure parts (commands, right click menus) come from
** Src/Winamp/Eq.cpp.
*/
#include "ui.h"
#include "../core/eq.h"
#include "../core/player.h"

#include <string.h>
#include <wchar.h>

#define inreg(x, y, x2, y2) \
	((mouse_x <= (x2) && mouse_x >= (x) && \
	  mouse_y <= (y2) && mouse_y >= (y)))

static int mouse_x, mouse_y, mouse_type, mouse_stats, mouse_root_x, mouse_root_y;

static int which_cap = 0;
enum { NO_CAP, TITLE_CAP, TB_CAP, QB_CAP, TOGBUTS_CAP, PB_CAP, PAN_CAP, VOL_CAP, SLID_CAP = 100 };

static void do_titlebar();
static void do_titlebuttons();
static void do_quickbuts();
static void do_sliders(int which);
static void do_togbuts();
static void do_volctrl();
static void do_panctrl();
static void do_presetbutton();

static void eq_changed()
{
	eq_apply_config();
}

static void equi_handlemouseevent(int x, int y, int type, int stats)
{
	mouse_x = x;
	mouse_y = y;
	mouse_type = type;
	mouse_stats = stats;
	switch (which_cap)
	{
	case PAN_CAP: do_panctrl(); return;
	case VOL_CAP: do_volctrl(); return;
	case PB_CAP: do_presetbutton(); return;
	case TOGBUTS_CAP: do_togbuts(); return;
	case TITLE_CAP: do_titlebar(); return;
	case TB_CAP: do_titlebuttons(); return;
	case QB_CAP: do_quickbuts(); return;
	default:
		if (which_cap >= SLID_CAP)
		{
			do_sliders(which_cap - SLID_CAP);
			return;
		}
	}
	if (config_eq_ws)
	{
		do_volctrl();
		do_panctrl();
	}
	else
	{
		for (x = 0; x < 11; x++)
			do_sliders(x);
	}
	do_titlebuttons();
	do_quickbuts();
	do_togbuts();
	do_presetbutton();
	do_titlebar();
}

static void do_volctrl()
{
	if (inreg(61, 3, 162, 11) || which_cap == VOL_CAP)
	{
		if (mouse_type == 1 && !which_cap) which_cap = VOL_CAP;
		if (which_cap == VOL_CAP && mouse_stats & MK_LBUTTON)
		{
			int t = mouse_x - 61;
			if (t < 0) t = 0;
			if (t > 157 - 61) t = 157 - 61;
			config_volume = (t * 255) / (157 - 61);
			in_setvol(config_volume);
			draw_volumebar(config_volume, 0);
			update_volume_text(-1);
			draw_eq_tbar(1);
			do_volbar_active = 1;
		}
		if (mouse_type == -1 && which_cap == VOL_CAP)
		{
			which_cap = 0;
			do_volbar_active = 0;
			draw_songname_title();
		}
	}
	else if (which_cap == VOL_CAP)
	{
		which_cap = 0;
		draw_songname_title();
		do_volbar_active = 0;
	}
}

static void do_panctrl()
{
	if (inreg(163, 3, 206, 11) || which_cap == PAN_CAP)
	{
		if (mouse_type == 1 && !which_cap) which_cap = PAN_CAP;
		if (which_cap == PAN_CAP && mouse_stats & MK_LBUTTON)
		{
			int t = mouse_x - 164;
			if (t < 0) t = 0;
			if (t > 206 - 164) t = 206 - 164;
			int p = (t * 255) / (206 - 164) - 127;
			config_pan = (p > 127 ? 127 : p);
			// changed in 5.64 to have a lower limit (~18% vs 9%) and for
			// holding shift to drop the central clamp (allows 4% balance)
			if (!(mouse_stats & MK_SHIFT))
				if (config_pan < 9 && config_pan > -9) config_pan = 0;
			in_setpan(config_pan);
			draw_panbar(config_pan, 0);
			update_panning_text(-1);
			draw_eq_tbar(1);
			do_volbar_active = 1;
		}
		if (mouse_type == -1 && which_cap == PAN_CAP)
		{
			draw_songname_title();
			do_volbar_active = 0;
			which_cap = 0;
		}
	}
	else if (which_cap == PAN_CAP)
	{
		draw_songname_title();
		do_volbar_active = 0;
		which_cap = 0;
	}
}

static void do_presetbutton()
{
	if (inreg(217, 18, 217 + 44, 18 + 12))
	{
		if (!which_cap && mouse_stats & MK_LBUTTON)
		{
			draw_eq_presets(1);
			which_cap = PB_CAP;
		}
		if (mouse_type == -1 && which_cap == PB_CAP)
		{
			draw_eq_presets(0);
			which_cap = 0;
			eq_command(EQ_PRESETS);
		}
	}
	else if (which_cap == PB_CAP)
	{
		which_cap = 0;
		draw_eq_presets(0);
	}
}

static void do_togbuts()
{
	if (inreg(14, 18, 14 + 25 + 33, 18 + 12))
	{
		int w = mouse_x >= 14 + 25 ? 1 : 0;
		if (mouse_type == -1)
		{
			if (w) config_autoload_eq = !config_autoload_eq;
			else config_use_eq = !config_use_eq;
			eq_changed();
			draw_eq_onauto(config_use_eq, config_autoload_eq, 0, 0);
			which_cap = 0;
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			which_cap = TOGBUTS_CAP;
			draw_eq_onauto(config_use_eq, config_autoload_eq, w ? 0 : 1, w ? 1 : 0);
		}
	}
	else if (which_cap == TOGBUTS_CAP)
	{
		which_cap = 0;
		draw_eq_onauto(config_use_eq, config_autoload_eq, 0, 0);
	}
}

static void do_sliders(int which)
{
	int top = 39, bottom = 98;
	int xoffs, w = 33 - 21;

	if (!which) xoffs = 21;
	else xoffs = 78 + (which - 1) * (96 - 78);

	if (which_cap == SLID_CAP + which || inreg(xoffs, top, xoffs + w, bottom))
	{
		int v_int = which ? eq_tab[which - 1] : config_preamp;
		if (mouse_type == 1 || which_cap == SLID_CAP + which || (!which_cap && mouse_stats & MK_LBUTTON))
		{
			static int click_yoffs = 5;
			int num_pos = 63 - 11;
			int d;
			int p;
			int t = (mouse_type == -1 || (!(mouse_x >= xoffs - 3 && mouse_x <= xoffs + w + 3) && (mouse_y >= top && mouse_y <= bottom && mouse_x >= 78 && mouse_x <= 180 + 78) && which));
			p = 63 - 12 - ((63 - v_int) * num_pos) / 64;
			if (mouse_type == 1 && mouse_y - top >= p - 1 && mouse_y - top < p + 11)
				click_yoffs = mouse_y - top - p;
			else if (mouse_type == 1)
				click_yoffs = 5;
			d = ((mouse_y - click_yoffs - top) * 64) / num_pos;
			if (d < 0) d = 0;
			if (d > 63) d = 63;

			// changed in 5.66 for holding shift to drop the central clamp (allows 7% balance)
			if (!(mouse_stats & MK_SHIFT))
				if (d >= 30 && d <= 32) d = 31;

			if (which) eq_tab[which - 1] = (unsigned char)d;
			else config_preamp = d;
			draw_eq_slid(which, d, t ? 0 : 1);
			draw_eq_graphthingy();
			if (t)
			{
				do_posbar_active = 0;
				draw_songname_title();
				which_cap = 0;
			}
			else
			{
				wchar_t buf[128];
				float v = (float)d;
				static const wchar_t *bands[11] = {
					L"Preamp",                            // PREAMP
					L"70", L"180", L"320", L"600",        // Hz
					L"1", L"3", L"6", L"12", L"14", L"16" // KHz
				};
				static const wchar_t *bandsISO[11] = {
					L"Preamp",                            // PREAMP
					L"31.5", L"63", L"125", L"250",       // Hz
					L"500", L"1", L"2", L"4", L"8", L"16" // KHz
				};
				v -= 31.5f;
				v /= 31.5f;
				v *= -12.0f;
				if (v >= -0.32 && v <= 0.32) v = 0.0;

				const wchar_t *hz = (which < 5 + !(config_eq_frequencies == EQ_FREQUENCIES_WINAMP)) ? L"Hz" : L"KHz";
				swprintf(buf, 128, L"EQ: %ls%ls: %ls%0.01f %ls",
				         ((config_eq_frequencies == EQ_FREQUENCIES_WINAMP) ? bands[which] : bandsISO[which]),
				         (!which ? L"" : hz),
				         v >= 0.0 ? L"+" : L"", v, L"DB");
				d = 0;
				do_posbar_active = 1;
				draw_songname(buf, &d, -1);
				which_cap = SLID_CAP + which;
			}
			eq_changed();
		}
	}
}

static void do_quickbuts()
{
	int l = 42, r = 67;
	if (inreg(l, 65, r, 74) || inreg(l, 33, r, 42) || inreg(l, 92, r, 101)) // +0
	{
		if (mouse_type == -1 && which_cap == QB_CAP)
		{
			int v;
			which_cap = 0;
			if (mouse_y <= 42) v = 0;
			else if (mouse_y <= 74) v = 31;
			else v = 63;

			memset(eq_tab, v, 10);
			for (int x = 1; x <= 10; x++)
				draw_eq_slid(x, eq_tab[x - 1], 0);
			eq_changed();
			draw_eq_graphthingy();
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			which_cap = QB_CAP;
		}
	}
	else if (which_cap == QB_CAP)
	{
		which_cap = 0;
	}
}

static int drag_dx, drag_dy;

static void do_titlebar()
{
	if (which_cap == TITLE_CAP || (!which_cap && (config_easymove || mouse_y < 14)))
	{
		switch (mouse_type)
		{
		case 1:
			if (!SkinWindow::CanPositionWindows())
			{
				g_eq_wnd->BeginWMMove(mouse_root_x, mouse_root_y);
				return;
			}
			which_cap = TITLE_CAP;
			drag_dx = mouse_root_x - config_eq_wx;
			drag_dy = mouse_root_y - config_eq_wy;
			break;
		case -1:
			which_cap = 0;
			break;
		case 0:
			if (which_cap == TITLE_CAP && mouse_stats & MK_LBUTTON)
			{
				config_eq_wx = mouse_root_x - drag_dx;
				config_eq_wy = mouse_root_y - drag_dy;
				if ((!!config_snap) ^ (!!(mouse_stats & MK_SHIFT)))
				{
					RECT rr;
					EstEQWindowRect(&rr);
					SnapWindowToAllWindows(&rr, g_eq_wnd);
					SetEQWindowRect(&rr);
				}
				g_eq_wnd->Move(config_eq_wx, config_eq_wy);
			}
			break;
		}
	}
}

static void do_titlebuttons()
{
	if (inreg(253, 3, 264 + 9, 3 + 9)) // kill button
	{
		int ws;
		if (mouse_x < 264) ws = 1;
		else ws = 0;
		if (mouse_type == -1 && which_cap == TB_CAP)
		{
			which_cap = 0;
			draw_eq_tbutton(0, 0);
			if (ws == 0) Main_OnCommand(WINAMP_OPTIONS_EQ);
			else Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE_EQ);
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			which_cap = TB_CAP;
			if (ws) draw_eq_tbutton(0, 1);
			else draw_eq_tbutton(1, 0);
		}
	}
	else if (which_cap == TB_CAP)
	{
		which_cap = 0;
		draw_eq_tbutton(0, 0);
	}
}

/* ---------------- Src/Winamp/Eq.cpp ---------------- */

void eq_dialog(int show)
{
	if (show < 0) show = !config_eq_open;
	config_eq_open = show;
	if (show)
	{
		g_eq_wnd->Move(config_eq_wx, config_eq_wy);
		g_eq_wnd->Show();
		draw_eq_all();
	}
	else
		g_eq_wnd->Hide();
	draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
}

static void redraw_sliders()
{
	draw_eq_slid(0, config_preamp, 0);
	for (int x = 1; x <= 10; x++)
		draw_eq_slid(x, eq_tab[x - 1], 0);
	draw_eq_graphthingy();
}

void eq_command(int id)
{
	switch (id)
	{
	case EQ_PANLEFT:
		if (config_pan - 4 < -127) config_pan = -127;
		else
		{
			if (config_pan - 4 > 0 && config_pan - 4 < 20) config_pan = 0;
			else config_pan -= 4;
		}
		in_setpan(config_pan);
		draw_panbar(config_pan, 0);
		update_panning_text(-2);
		return;
	case EQ_PANRIGHT:
		if (config_pan + 4 > 127) config_pan = 127;
		else
		{
			if (config_pan + 4 < 0 && config_pan + 4 > -20) config_pan = 0;
			else config_pan += 4;
		}
		in_setpan(config_pan);
		draw_panbar(config_pan, 0);
		update_panning_text(-2);
		return;
	case WINAMP_OPTIONS_WINDOWSHADE:
		Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE_EQ);
		return;
	case IDM_EQ_LOADDEFAULT:
		eq_read_preset(config_eq_path(), "Default");
		redraw_sliders();
		return;
	case IDM_EQ_SAVEDEFAULT:
		eq_write_preset(config_eq_path(), "Default");
		return;
	case ID_SAVE_EQF:
	case ID_LOAD_EQF:
	case IDM_EQ_SAVEPRE:
	case IDM_EQ_SAVEMP3:
	case IDM_EQ_LOADPRE:
	case IDM_EQ_DELPRE:
	case IDM_EQ_DELMP3:
	case IDM_EQ_LOADMP3:
		dlg_eq_presets(id);
		redraw_sliders();
		return;
	case EQ_PRESETS:
	{
		int s = (config_dsize && config_eqdsize) ? 2 : 1;
		popup_menu(MENU_EQ_PRESETS, g_eq_wnd, g_eq_wnd->X() + 218 * s, g_eq_wnd->Y() + 19 * s);
		return;
	}
	case EQ_AUTO:
		config_autoload_eq = !config_autoload_eq;
		draw_eq_onauto(config_use_eq, config_autoload_eq, 0, 0);
		return;
	case EQ_ENABLE:
		config_use_eq = !config_use_eq;
		eq_changed();
		draw_eq_onauto(config_use_eq, config_autoload_eq, 0, 0);
		return;
	case ID_PE_CLOSE:
		Main_OnCommand(WINAMP_OPTIONS_EQ);
		return;
	default:
		if (id == EQ_INCPRE || id == EQ_DECPRE || (id >= EQ_DEC1 && id <= EQ_DEC10) || (id >= EQ_INC1 && id <= EQ_INC10))
		{
			int addsub = ((id >= EQ_DEC1 && id <= EQ_DEC10) || id == EQ_DECPRE) ? -2 : 2;
			if (id == EQ_INCPRE || id == EQ_DECPRE)
			{
				config_preamp -= addsub;
				if (config_preamp < 0) config_preamp = 0;
				if (config_preamp > 63) config_preamp = 63;
			}
			else
			{
				int o = (id - (addsub > 0 ? EQ_INC1 : EQ_DEC1));
				int p = eq_tab[o];
				p -= addsub;
				if (p < 0) p = 0;
				if (p > 63) p = 63;
				eq_tab[o] = (unsigned char)p;
			}
			redraw_sliders();
			eq_changed();
		}
		else
			Main_OnCommand(id);
	}
}

class EqHandler : public SkinWindow::Handler
{
public:
	void OnMouse(SkinWindow *, int x, int y, int type, int stats, int rx, int ry) override
	{
		mouse_root_x = rx;
		mouse_root_y = ry;
		if (type == -2)
		{
			which_cap = 0;
			return;
		}
		equi_handlemouseevent(x, y, type, stats);
	}

	void OnDoubleClick(SkinWindow *, int x, int y, int stats, int rx, int ry) override
	{
		if (y <= 14 && x < 252)
		{
			which_cap = 0;
			Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE_EQ);
		}
		else
		{
			mouse_root_x = rx;
			mouse_root_y = ry;
			equi_handlemouseevent(x, y, 1, stats);
		}
	}

	void OnRightClick(SkinWindow *, int x, int y, int) override
	{
		if (x >= 14 && y >= 18 && x <= 14 + 25 && y <= 18 + 12 && !config_eq_ws)
			popup_menu(MENU_EQ_ENABLE, g_eq_wnd);
		else if (x >= 14 + 25 && y >= 18 && x <= 14 + 25 + 33 && y <= 18 + 12 && !config_eq_ws)
			popup_menu(MENU_EQ_AUTO, g_eq_wnd);
		else
			popup_menu(MENU_MAIN, g_eq_wnd);
	}

	void OnScroll(SkinWindow *, int, int, int delta, int) override
	{
		Main_OnCommand(delta > 0 ? WINAMP_VOLUMEUP : WINAMP_VOLUMEDOWN);
	}

	void OnActivate(SkinWindow *, bool active) override
	{
		draw_eq_tbar(active ? 1 : (config_hilite ? 0 : 1));
	}

	void OnDrop(SkinWindow *w, int x, int y, const std::vector<std::string> &files) override
	{
		main_handler->OnDrop(w, x, y, files);
	}

	void OnClose(SkinWindow *) override
	{
		app_quit();
	}
};

static EqHandler eq_handler_impl;
SkinWindow::Handler *eq_handler = &eq_handler_impl;
