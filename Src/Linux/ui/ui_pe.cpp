/*
** Winamp for Linux - playlist editor behaviour.
**
** peui_handlemouseevent() and the do_* functions are Src/Winamp/Peui.cpp;
** the window procedure parts (double click, right click, wheel, drag & drop,
** WM_COMMAND) come from Src/Winamp/Pledit.cpp.
*/
#include "ui.h"
#include "../core/player.h"
#include "../core/playlist.h"
#include "../core/plugins.h"
#include "../core/vis.h"

#include <gio/gio.h>
#include <stdio.h>
#include <algorithm>

#define inreg(x, y, x2, y2) \
	((mouse_x <= (x2) && mouse_x >= (x) && \
	  mouse_y <= (y2) && mouse_y >= (y)))

static int mouse_x, mouse_y, mouse_type, mouse_stats, mouse_root_x, mouse_root_y;
static bool pe_dirty = true;

static int which_cap = 0;
enum { NO_CAP, TITLE_CAP, TB_CAP, SZ_CAP, VS_CAP, LB_CAP, VSB_CAP, ADD_CAP, REM_CAP, SEL_CAP, MISC_CAP, FILE_CAP, TD_CAP, MB_CAP };

static void do_titlebar();
static void do_titlebuttons();
static void do_size();
static void do_vscroll();
static void do_lb();
static void do_vsb();
static void do_addbut();
static void do_rembut();
static void do_selbut();
static void do_miscbut();
static void do_filebut();
static void do_timedisplay();
static void do_mb();

void plEditRefresh()
{
	pe_dirty = true;
	if (g_pl_wnd) g_pl_wnd->Invalidate();
}

static int peui_isrbuttoncaptured()
{
	return which_cap >= ADD_CAP && which_cap <= FILE_CAP;
}

static void peui_reset()
{
	if (which_cap >= ADD_CAP && which_cap <= FILE_CAP) plEditRefresh();
	which_cap = 0;
}

static void peui_handlemouseevent(int x, int y, int type, int stats)
{
	mouse_x = x;
	mouse_y = y;
	mouse_type = type;
	mouse_stats = stats;
	if (!which_cap)
	{
		if (playing && mouse_type == 1 && config_pe_height != 14 && config_pe_width >= 350 &&
			inreg(config_pe_width - 150 - 75, config_pe_height - 26, config_pe_width - 150, config_pe_height - 8))
		{
			config_sa++;
			if (config_sa > 2) config_sa = 0;
			sa_setmode(config_sa);
			return;
		}
	}
	switch (which_cap)
	{
	case MB_CAP: do_mb(); return;
	case TD_CAP: do_timedisplay(); return;
	case MISC_CAP: do_miscbut(); return;
	case FILE_CAP: do_filebut(); return;
	case SEL_CAP: do_selbut(); return;
	case ADD_CAP: do_addbut(); return;
	case REM_CAP: do_rembut(); return;
	case VSB_CAP: do_vsb(); return;
	case LB_CAP: do_lb(); return;
	case TITLE_CAP: do_titlebar(); return;
	case TB_CAP: do_titlebuttons(); return;
	case SZ_CAP: do_size(); return;
	case VS_CAP: do_vscroll(); return;
	default: break;
	}
	if (config_pe_height != 14)
	{
		do_mb();
		do_timedisplay();
		do_filebut();
		do_miscbut();
		do_selbut();
		do_addbut();
		do_rembut();
		do_vsb();
		do_lb();
		do_vscroll();
	}
	do_titlebuttons();
	do_size();
	do_titlebar();
}

static void __do_buttons(int which)
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
}

static void do_mb()
{
	if (!which_cap && mouse_type == 1 && inreg(config_pe_width - 144, config_pe_height - 15, config_pe_width - 91, config_pe_height - 8))
	{
		int t = config_pe_width - 144;
		int which = 5 - (mouse_x < 45 + t) - (mouse_x < 36 + t) - (mouse_x < 27 + t) - (mouse_x < 17 + t) - (mouse_x < 7 + t);
		which_cap = MB_CAP;
		__do_buttons(which);
	}
	if (which_cap == MB_CAP && mouse_type == -1)
		which_cap = 0;
}

static void do_timedisplay()
{
	if (!which_cap && mouse_type == 1 && inreg(config_pe_width - 87, config_pe_height - 18, config_pe_width - 53, config_pe_height - 9))
	{
		which_cap = TD_CAP;
		config_timeleftmode = !config_timeleftmode;
		display_timer_tick();
	}
	if (which_cap == TD_CAP && mouse_type == -1)
		which_cap = 0;
}

// the little button "menus" at the bottom of the playlist editor
static void popup_pl(MenuId m, int x, int y)
{
	popup_menu(m, g_pl_wnd, g_pl_wnd->X() + x, g_pl_wnd->Y() + y);
}

static void do_filebut()
{
	int lwc = which_cap;
	if (!which_cap && mouse_type == 1 && inreg(config_pe_width - 44, config_pe_height - 30, config_pe_width - 44 + 22, config_pe_height - 12))
		which_cap = FILE_CAP;

	if (which_cap == FILE_CAP)
	{
		int doit = -1;
		if (inreg(config_pe_width - 44, config_pe_height - 30, config_pe_width - 44 + 22, config_pe_height - 12)) doit = 0;
		else if (inreg(config_pe_width - 44, config_pe_height - 30 - 18, config_pe_width - 44 + 22, config_pe_height - 12 - 18)) doit = 1;
		else if (inreg(config_pe_width - 44, config_pe_height - 30 - 18 * 2, config_pe_width - 44 + 22, config_pe_height - 12 - 18 * 2)) doit = 2;
		draw_pe_iobut(doit);

		if ((config_ospb && mouse_type == -1) || (mouse_type == 1 && lwc == FILE_CAP))
		{
			which_cap = 0;
			plEditRefresh();
			switch (doit)
			{
			case 0: pe_command(ID_PE_OPEN); break;
			case 1: pe_command(ID_PE_SAVEAS); break;
			case 2: pe_command(ID_PE_CLEAR); break;
			}
		}
	}
}

static void do_miscbut()
{
	int lwc = which_cap;
	if (!which_cap && mouse_type == 1 && inreg(101, config_pe_height - 30, 122, config_pe_height - 12))
		which_cap = MISC_CAP;

	if (which_cap == MISC_CAP)
	{
		int doit = -1;
		if (inreg(101, config_pe_height - 30, 122, config_pe_height - 12)) doit = 0;
		else if (inreg(101, config_pe_height - 30 - 18, 122, config_pe_height - 12 - 18)) doit = 1;
		else if (inreg(101, config_pe_height - 30 - 18 * 2, 122, config_pe_height - 12 - 18 * 2)) doit = 2;
		draw_pe_miscbut(doit);
		if ((config_ospb && mouse_type == -1) || (mouse_type == 1 && lwc == MISC_CAP))
		{
			which_cap = 0;
			plEditRefresh();
			switch (doit)
			{
			case 1: popup_pl(MENU_PL_SORT, 122, config_pe_height - 30 - 18); break;
			case 2: popup_pl(MENU_PL_MISC, 122, config_pe_height - 30 - 18 * 2); break;
			case 0: popup_pl(MENU_PL_FILEINFO, 122, config_pe_height - 30); break;
			}
		}
	}
}

static void do_selbut()
{
	int lwc = which_cap;
	if (!which_cap && mouse_type == 1 && inreg(72, config_pe_height - 30, 93, config_pe_height - 12))
		which_cap = SEL_CAP;

	if (which_cap == SEL_CAP)
	{
		int doit = -1;
		if (inreg(72, config_pe_height - 30, 93, config_pe_height - 12)) doit = 0;
		else if (inreg(72, config_pe_height - 30 - 18, 93, config_pe_height - 12 - 18)) doit = 1;
		else if (inreg(72, config_pe_height - 30 - 18 * 2, 93, config_pe_height - 12 - 18 * 2)) doit = 2;
		draw_pe_selbut(doit);
		if ((config_ospb && mouse_type == -1) || (mouse_type == 1 && lwc == SEL_CAP))
		{
			which_cap = 0;
			plEditRefresh();
			switch (doit)
			{
			case 0: pe_command(ID_PE_SELECTALL); break;
			case 1: pe_command(ID_PE_NONE); break;
			case 2: pe_command(IDC_SELECTINV); break;
			}
		}
	}
}

static void do_rembut()
{
	int lwc = which_cap;
	if (!which_cap && mouse_type == 1 && inreg(43, config_pe_height - 30, 64, config_pe_height - 12))
		which_cap = REM_CAP;

	if (which_cap == REM_CAP)
	{
		int doit = -1;
		if (inreg(43, config_pe_height - 30, 64, config_pe_height - 12)) doit = 0;
		else if (inreg(43, config_pe_height - 30 - 18, 64, config_pe_height - 12 - 18)) doit = 1;
		else if (inreg(43, config_pe_height - 30 - 18 * 2, 64, config_pe_height - 12 - 18 * 2)) doit = 2;
		else if (inreg(43, config_pe_height - 30 - 18 * 3, 64, config_pe_height - 12 - 18 * 3)) doit = 3;
		draw_pe_rembut(doit);
		if ((config_ospb && mouse_type == -1) || (mouse_type == 1 && lwc == REM_CAP))
		{
			which_cap = 0;
			plEditRefresh();
			switch (doit)
			{
			case 0: pe_command(IDC_PLAYLIST_REMOVEMP3); break;
			case 1: pe_command(IDC_PLAYLIST_CROP); break;
			case 2: pe_command(ID_PE_CLEAR); break;
			case 3: popup_pl(MENU_PL_REMOVE_MISC, 64, config_pe_height - 30 - 18 * 3); break;
			}
		}
	}
}

static void do_addbut()
{
	int lwc = which_cap;
	if (!which_cap && mouse_type == 1 && inreg(14, config_pe_height - 30, 35, config_pe_height - 12))
		which_cap = ADD_CAP;

	if (which_cap == ADD_CAP)
	{
		int doit = -1;
		if (inreg(14, config_pe_height - 30, 35, config_pe_height - 12)) doit = 0;
		else if (inreg(14, config_pe_height - 30 - 18, 35, config_pe_height - 12 - 18)) doit = 1;
		else if (inreg(14, config_pe_height - 30 - 18 * 2, 35, config_pe_height - 12 - 18 * 2)) doit = 2;
		draw_pe_addbut(doit);
		if ((config_ospb && mouse_type == -1) || (mouse_type == 1 && lwc == ADD_CAP))
		{
			which_cap = 0;
			plEditRefresh();
			switch (doit)
			{
			case 0: pe_command(IDC_PLAYLIST_ADDMP3); break;
			case 1: pe_command(IDC_PLAYLIST_ADDDIR); break;
			case 2: pe_command(IDC_PLAYLIST_ADDLOC); break;
			}
		}
	}
}

static void do_vsb()
{
	if (!which_cap && mouse_type == 1 && inreg(config_pe_width - 15, config_pe_height - 36, config_pe_width - 7, config_pe_height - 32))
	{
		pe_command(ID_PE_SCROLLUP);
		which_cap = VSB_CAP;
	}
	if (!which_cap && mouse_type == 1 && inreg(config_pe_width - 15, config_pe_height - 31, config_pe_width - 7, config_pe_height - 27))
	{
		pe_command(ID_PE_SCROLLDOWN);
		which_cap = VSB_CAP;
	}
	if (which_cap == VSB_CAP && mouse_type == -1) which_cap = 0;
}

static int shiftsel_1 = -1;

static void do_lb()
{
	static int move_mpos, stt;

	if (!which_cap && mouse_type == 1 && inreg(12, 20, config_pe_width - 20, config_pe_height - 38))
	{
		int wh = (mouse_y - 22) / pe_fontheight + pledit_disp_offs;
		if (!(mouse_stats & MK_CONTROL) && !PlayList_getselect(wh))
		{
			int x, t = PlayList_getlength();
			for (x = 0; x < t; x++)
				PlayList_setselect(x, 0);
		}
		if (mouse_stats & MK_SHIFT)
		{
			if (shiftsel_1 != -1)
			{
				int x, t = std::max(std::min(PlayList_getlength(), shiftsel_1), std::min(PlayList_getlength(), wh));
				if (!(mouse_stats & MK_CONTROL) && PlayList_getselect(wh))
				{
					int x, t = PlayList_getlength();
					for (x = 0; x < t; x++)
						PlayList_setselect(x, 0);
				}
				for (x = std::min(shiftsel_1, wh); x <= t; x++)
					PlayList_setselect(x, 1);
				stt = 1;
			}
		}
		else
		{
			if (PlayList_getselect(wh) && mouse_stats & MK_CONTROL) PlayList_setselect(wh, 0);
			else
			{
				int y = wh - pledit_disp_offs;
				if (y < PlayList_getlength() - pledit_disp_offs && y < pe_num_songs())
					PlayList_setselect(wh, 1);
			}
			shiftsel_1 = wh;
		}
		plEditRefresh();
		move_mpos = (mouse_y - 22) / pe_fontheight + pledit_disp_offs;
		which_cap = LB_CAP;
	}
	if (which_cap == LB_CAP)
	{
		int m = (mouse_y - 22) / pe_fontheight + pledit_disp_offs;
		if (mouse_y < 22) m = (mouse_y - 22 - pe_fontheight + 1) / pe_fontheight + pledit_disp_offs;
		if (m != move_mpos)
		{
			int v, x;
			v = PlayList_getlength();
			while (m > move_mpos)
			{
				if (!PlayList_getselect(v - 1))
					for (x = v - 2; x >= 0; x--)
					{
						if (!PlayList_getselect(x)) continue;
						if (shiftsel_1 == x) shiftsel_1 = x + 1;
						PlayList_swap(x, x + 1);
						if (x == PlayList_getPosition())
							PlayList_advance(1);
						else if (x + 1 == PlayList_getPosition())
							PlayList_advance(-1);
					}
				move_mpos++;
			}
			while (m < move_mpos)
			{
				if (!PlayList_getselect(0))
					for (x = 1; x < v; x++)
					{
						if (!PlayList_getselect(x)) continue;
						if (shiftsel_1 == x) shiftsel_1 = x - 1;
						PlayList_swap(x, x - 1);
						if (x == PlayList_getPosition())
							PlayList_advance(-1);
						else if (x - 1 == PlayList_getPosition())
							PlayList_advance(1);
					}
				move_mpos--;
			}

			if (mouse_y < 10)
			{
				pledit_disp_offs--;
				if (pledit_disp_offs < 0)
				{
					pledit_disp_offs = 0;
					move_mpos = 0;
				}
			}
			else if (mouse_y > config_pe_height - 28)
			{
				int num_songs = pe_num_songs();
				int t = PlayList_getlength() - num_songs;
				pledit_disp_offs++;
				if (pledit_disp_offs > t)
				{
					pledit_disp_offs = std::max(t, 0);
					move_mpos = PlayList_getlength();
				}
			}
			plEditRefresh();
			draw_songname_title();
		}

		if (mouse_type == -1)
		{
			if (!stt)
			{
				int wh = (mouse_y - 22) / pe_fontheight + pledit_disp_offs;
				if (!(mouse_stats & MK_CONTROL) && PlayList_getselect(wh))
				{
					int x, t = PlayList_getlength();
					for (x = 0; x < t; x++)
						PlayList_setselect(x, 0);
					PlayList_setselect(wh, 1);
					plEditRefresh();
				}
			}
			stt = 0;
			which_cap = 0;
		}
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
				g_pl_wnd->BeginWMMove(mouse_root_x, mouse_root_y);
				return;
			}
			which_cap = TITLE_CAP;
			drag_dx = mouse_root_x - config_pe_wx;
			drag_dy = mouse_root_y - config_pe_wy;
			break;
		case -1:
			which_cap = 0;
			break;
		case 0:
			if (which_cap == TITLE_CAP && mouse_stats & MK_LBUTTON)
			{
				config_pe_wx = mouse_root_x - drag_dx;
				config_pe_wy = mouse_root_y - drag_dy;
				if ((!!config_snap) ^ (!!(mouse_stats & MK_SHIFT)))
				{
					RECT outrc;
					EstPLWindowRect(&outrc);
					SnapWindowToAllWindows(&outrc, g_pl_wnd);
					SetPLWindowRect(&outrc);
				}
				g_pl_wnd->Move(config_pe_wx, config_pe_wy);
			}
			break;
		}
	}
}

static void do_titlebuttons()
{
	int w = 0;
	w = inreg(config_pe_width - 10, 3, config_pe_width - 1, 3 + 9) ? 1 : w;
	w = inreg(config_pe_width - 20, 3, config_pe_width - 11, 3 + 9) ? 2 : w;

	if (w) // kill button
	{
		if (mouse_type == -1 && which_cap == TB_CAP)
		{
			which_cap = 0;
			draw_pe_tbutton(0, 0, 0);
			Main_OnCommand(w == 1 ? WINAMP_OPTIONS_PLEDIT : WINAMP_OPTIONS_WINDOWSHADE_PL);
		}
		else if (mouse_stats & MK_LBUTTON)
		{
			which_cap = TB_CAP;
			draw_pe_tbutton(w == 2 ? 1 : 0, w == 1 ? 1 : 0, config_pe_height == 14 ? 1 : 0);
		}
	}
	else if (which_cap == TB_CAP)
	{
		which_cap = 0;
		draw_pe_tbutton(0, 0, config_pe_height == 14 ? 1 : 0);
	}
}

static void do_size()
{
	if (which_cap == SZ_CAP || (config_pe_height != 14 && !which_cap &&
		mouse_x > config_pe_width - 20 && mouse_y > config_pe_height - 20 &&
		((config_pe_width - mouse_x + config_pe_height - mouse_y) <= 30))
		||
		(config_pe_height == 14 && !which_cap && mouse_x > config_pe_width - 29 && mouse_x < config_pe_width - 20))
	{
		static int dx, dy;
		if (!which_cap && mouse_type == 1)
		{
			dx = config_pe_width - mouse_x;
			dy = config_pe_height - mouse_y;
			which_cap = SZ_CAP;
		}
		if (which_cap == SZ_CAP)
		{
			int x, y;
			if (mouse_type == -1)
				which_cap = 0;
			x = mouse_x + dx;
			y = mouse_y + dy;
			if (!config_embedwnd_freesize)
			{
				x += 24;
				x -= x % 25;
				y += 28;
				y -= y % 29;
			}
			if (x < 275) x = 275;
			if (y < 20 + 38 + 29 + 29) y = 20 + 38 + 29 + 29;
			if (x != config_pe_width || (config_pe_height != 14 && y != config_pe_height))
			{
				config_pe_width = x;
				if (config_pe_height != 14) config_pe_height = y;
				g_pl_wnd->SetSize(config_pe_width, config_pe_height);
				plEditRefresh();
			}
		}
	}
}

static void do_vscroll()
{
	int top = 20, bottom = config_pe_height - 38;
	int num_songs = pe_num_songs();
	int xoffs = config_pe_width - 15, w = 8;

	if (inreg(xoffs, top, xoffs + w, bottom) || which_cap == VS_CAP)
	{
		if ((!which_cap && mouse_type == 1) || which_cap == VS_CAP)
		{
			int d;
			int p;
			static int click_yoffs = 9;
			int a = PlayList_getlength() - num_songs;
			if (a < 1) p = top;
			else p = top + ((config_pe_height - 38 - 20 - 18) * pledit_disp_offs) / a;
			if (mouse_type == 1 && mouse_y >= p && mouse_y < p + 20)
				click_yoffs = mouse_y - p - 1;
			else if (mouse_type == 1)
				click_yoffs = 9;

			d = ((mouse_y - click_yoffs - top) * a) / (config_pe_height - 38 - 20 - 18);

			pledit_disp_offs = d;
			if (pledit_disp_offs > a) pledit_disp_offs = a;
			if (pledit_disp_offs < 0) pledit_disp_offs = 0;

			plEditRefresh();
			if (mouse_type == -1)
				which_cap = 0;
			else
				which_cap = VS_CAP;
		}
	}
}

void plEditSelect(int song)
{
	int num = pe_num_songs();
	if (song < pledit_disp_offs || song >= pledit_disp_offs + num)
	{
		pledit_disp_offs = song - num / 2;
		int max = PlayList_getlength() - num;
		if (pledit_disp_offs > max) pledit_disp_offs = max;
		if (pledit_disp_offs < 0) pledit_disp_offs = 0;
	}
	plEditRefresh();
}

void pleditDlg(int show)
{
	if (show < 0) show = !config_pe_open;
	config_pe_open = show;
	if (show)
	{
		g_pl_wnd->Move(config_pe_wx, config_pe_wy);
		g_pl_wnd->Show();
		plEditRefresh();
	}
	else
		g_pl_wnd->Hide();
	draw_eqplbut(config_eq_open, 0, config_pe_open, 0);
}

/* ---------------- WM_COMMAND (Pledit.cpp) ---------------- */

static void open_folder_of(const std::string &file)
{
	std::string dir = wa::path_dirname(file);
	gchar *uri = g_filename_to_uri(dir.c_str(), nullptr, nullptr);
	if (uri)
	{
		g_app_info_launch_default_for_uri(uri, nullptr, nullptr);
		g_free(uri);
	}
}

static void generate_html_playlist()
{
	std::string path = wa::path_join(g_get_tmp_dir(), "winamp-playlist.html");
	FILE *f = fopen(path.c_str(), "w");
	if (!f) return;
	int total = 0;
	for (int i = 0; i < PlayList_getlength(); i++)
		if (PlayList_getsonglength(i) > 0) total += PlayList_getsonglength(i);
	fprintf(f, "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>Winamp Generated PlayList</title>\n"
	           "<style>body{background:#000040;color:#fff;font-family:Arial,sans-serif}"
	           "h1{color:#fff;font-size:18px}.t{color:#4040ff}li{color:#fff}.len{color:#40a0ff}</style></head><body>\n"
	           "<h1>WINAMP <span class=\"t\">playlist</span></h1>\n<p><b>%d</b> tracks in playlist, average track length: <b>%d:%02d</b><br>"
	           "Playlist length: <b>%d minutes %d seconds</b></p>\n<ol>\n",
	        PlayList_getlength(), PlayList_getlength() ? total / PlayList_getlength() / 60 : 0,
	        PlayList_getlength() ? (total / PlayList_getlength()) % 60 : 0, total / 60, total % 60);
	for (int i = 0; i < PlayList_getlength(); i++)
	{
		gchar *t = g_markup_escape_text(PlayList_gettitle(i).c_str(), -1);
		int l = PlayList_getsonglength(i);
		if (l >= 0) fprintf(f, "<li>%s <span class=\"len\">(%d:%02d)</span></li>\n", t, l / 60, l % 60);
		else fprintf(f, "<li>%s</li>\n", t);
		g_free(t);
	}
	fprintf(f, "</ol></body></html>\n");
	fclose(f);
	gchar *uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
	if (uri)
	{
		g_app_info_launch_default_for_uri(uri, nullptr, nullptr);
		g_free(uri);
	}
}

static int first_selected()
{
	for (int i = 0; i < PlayList_getlength(); i++)
		if (PlayList_getselect(i)) return i;
	return -1;
}

void pe_command(int id)
{
	switch (id)
	{
	case ID_PE_SHOWPLAYING:
		plEditSelect(PlayList_getPosition());
		return;
	case WINAMP_OPTIONS_WINDOWSHADE:
		Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE_PL);
		return;
	case ID_PE_FFOD:
	{
		int s = first_selected();
		if (s >= 0) open_folder_of(PlayList_getfilename(s));
		return;
	}
	case ID_PE_EXTINFO:
		PlayList_refreshselected();
		return;
	case ID_PE_PRINT:
		generate_html_playlist();
		return;
	case ID_PE_OPEN:
	{
		auto files = dlg_open_files(dlg_parent(), "Load Playlist", false, true);
		if (!files.empty())
		{
			StopPlaying(0);
			PlayList_clear();
			pledit_disp_offs = 0;
			PlayList_load(files[0]);
			plEditRefresh();
			player_update_title();
			draw_songname_title();
		}
		return;
	}
	case ID_PE_SAVEAS:
	{
		std::string fn = dlg_save_file(dlg_parent(), "Save Playlist", "playlist.m3u8", "Playlist files", "*.m3u8;*.m3u;*.pls");
		if (!fn.empty())
		{
			std::string ext = wa::path_extension(fn);
			if (ext != "m3u" && ext != "m3u8" && ext != "pls") fn += ".m3u8";
			PlayList_save(fn);
		}
		return;
	}
	case ID_PE_ID3:
	case ID_PE_EDIT_SEL:
	{
		int s = first_selected();
		if (s < 0) s = PlayList_getPosition();
		if (s < PlayList_getlength()) dlg_file_info(PlayList_getfilename(s));
		return;
	}
	case ID_PE_ENTRY:
	{
		int s = first_selected();
		if (s < 0) return;
		std::string fn = dlg_input(dlg_parent(), "Edit Playlist Entry", "Filename or URL:", PlayList_getfilename(s));
		if (!fn.empty() && fn != PlayList_getfilename(s))
		{
			bool sel = PlayList_getselect(s);
			PlayList_delete(s);
			PlayList_insert(s, fn);
			PlayList_setselect(s, sel);
			plEditRefresh();
		}
		return;
	}
	case ID_PE_CLEAR:
		PlayList_clear();
		pledit_disp_offs = 0;
		plEditRefresh();
		if (!playing)
		{
			FileTitle.clear();
			draw_songname_title();
		}
		return;
	case IDC_SELECTINV:
		for (int x = 0; x < PlayList_getlength(); x++)
			PlayList_setselect(x, !PlayList_getselect(x));
		plEditRefresh();
		return;
	case ID_PE_SELECTALL:
		for (int x = 0; x < PlayList_getlength(); x++)
			PlayList_setselect(x, 1);
		plEditRefresh();
		return;
	case ID_PE_NONE:
		for (int x = 0; x < PlayList_getlength(); x++)
			PlayList_setselect(x, 0);
		plEditRefresh();
		return;
	case ID_PE_SCUP:
	case ID_PE_SCDOWN:
		if (config_pe_height != 14)
		{
			int which, x;
			int num_songs = pe_num_songs();
			if (id == ID_PE_SCUP)
			{
				for (x = pledit_disp_offs; x < pledit_disp_offs + num_songs; x++)
					if (PlayList_getselect(x)) break;
				if (x != pledit_disp_offs + num_songs) which = x - 1;
				else which = pledit_disp_offs;
			}
			else
			{
				for (x = pledit_disp_offs + num_songs - 1; x >= pledit_disp_offs; x--)
					if (PlayList_getselect(x)) break;
				if (x >= pledit_disp_offs) which = x + 1;
				else which = pledit_disp_offs + num_songs - 1;
			}
			if (which < 0) which = 0;
			if (which >= PlayList_getlength()) which = PlayList_getlength() - 1;
			if (!(mouse_stats & MK_SHIFT))
				for (x = PlayList_getlength() - 1; x >= 0; x--) PlayList_setselect(x, 0);
			PlayList_setselect(which, 1);
			if (which < pledit_disp_offs) pledit_disp_offs = which;
			else if (which >= pledit_disp_offs + num_songs)
				pledit_disp_offs = which - num_songs + 1;
			plEditRefresh();
		}
		else
			Main_OnCommand(id == ID_PE_SCUP ? WINAMP_BUTTON1 : WINAMP_BUTTON5);
		return;
	case ID_PE_SCROLLDOWN:
	{
		int num_songs = pe_num_songs();
		int t = PlayList_getlength();
		pledit_disp_offs += 1;
		if (pledit_disp_offs >= t - num_songs) pledit_disp_offs = t - num_songs;
		if (pledit_disp_offs < 0) pledit_disp_offs = 0;
		plEditRefresh();
		return;
	}
	case ID_PE_SCROLLUP:
		if (pledit_disp_offs > 0)
		{
			pledit_disp_offs -= 1;
			if (pledit_disp_offs < 0) pledit_disp_offs = 0;
			plEditRefresh();
		}
		return;
	case ID_PE_MOVEDOWN:
	{
		int v = PlayList_getlength();
		if (!PlayList_getselect(v - 1))
			for (int x = v - 2; x >= 0; x--)
				if (PlayList_getselect(x))
				{
					if (shiftsel_1 == x) shiftsel_1 = x + 1;
					PlayList_swap(x, x + 1);
					if (x == PlayList_getPosition()) PlayList_advance(1);
					else if (x + 1 == PlayList_getPosition()) PlayList_advance(-1);
				}
		plEditRefresh();
		draw_songname_title();
		return;
	}
	case ID_PE_MOVEUP:
	{
		int v = PlayList_getlength();
		if (!PlayList_getselect(0))
			for (int x = 1; x < v; x++)
				if (PlayList_getselect(x))
				{
					if (shiftsel_1 == x) shiftsel_1 = x + 1;
					PlayList_swap(x, x - 1);
					if (x == PlayList_getPosition()) PlayList_advance(-1);
					else if (x - 1 == PlayList_getPosition()) PlayList_advance(1);
				}
		plEditRefresh();
		draw_songname_title();
		return;
	}
	case ID_PE_TOP:
		pledit_disp_offs = 0;
		plEditRefresh();
		return;
	case ID_PE_BOTTOM:
		pledit_disp_offs = std::max(0, PlayList_getlength() - pe_num_songs());
		plEditRefresh();
		return;
	case ID_PE_CLOSE:
		Main_OnCommand(WINAMP_OPTIONS_PLEDIT);
		return;
	case IDC_PLAYLIST_ADDMP3:
	{
		auto files = dlg_open_files(dlg_parent(), "Add file(s) to playlist", true);
		for (auto &f : files) PlayList_add(f);
		plEditRefresh();
		return;
	}
	case IDC_PLAYLIST_ADDDIR:
	{
		std::string dir = dlg_open_folder(dlg_parent(), "Add folder to playlist");
		if (!dir.empty()) PlayList_add(dir);
		plEditRefresh();
		return;
	}
	case IDC_PLAYLIST_ADDLOC:
	{
		std::string url = dlg_input(dlg_parent(), "Add URL", "Enter URL to add:", "http://");
		if (!url.empty() && url != "http://") PlayList_add(url);
		plEditRefresh();
		return;
	}
	case IDC_PLAYLIST_REMOVEMP3:
		PlayList_deleteselected();
		plEditRefresh();
		return;
	case IDC_PLAYLIST_CROP:
		PlayList_cropselected();
		plEditRefresh();
		return;
	case IDC_PLAYLIST_PLAY:
	{
		int s = first_selected();
		if (s >= 0) PlayIndex(s);
		return;
	}
	case ID_PE_NONEXIST:
		PlayList_removemissing();
		plEditRefresh();
		return;
	case ID_PE_DELETEFROMDISK:
	{
		int n = PlayList_getselectcount();
		if (!n) return;
		GtkWidget *d = gtk_message_dialog_new(dlg_parent(), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
		                                      "Are you sure you want to move the %d selected file(s) to the trash?", n);
		gtk_window_set_title(GTK_WINDOW(d), "Winamp");
		int r = gtk_dialog_run(GTK_DIALOG(d));
		gtk_widget_destroy(d);
		if (r != GTK_RESPONSE_YES) return;
		for (int i = PlayList_getlength() - 1; i >= 0; i--)
		{
			if (!PlayList_getselect(i)) continue;
			std::string fn = PlayList_getfilename(i);
			if (playing && i == PlayList_getPosition()) StopPlaying(0);
			GFile *f = g_file_new_for_path(fn.c_str());
			if (g_file_trash(f, nullptr, nullptr)) PlayList_delete(i);
			g_object_unref(f);
		}
		plEditRefresh();
		return;
	}
	case ID_PE_S_TITLE:
	case ID_PE_S_FILENAME:
	case ID_PE_S_PATH:
	case ID_PE_S_RANDOM:
	case ID_PE_S_REV:
	{
		int w = 0;
		if (id == ID_PE_S_TITLE) w = PL_SORT_TITLE;
		else if (id == ID_PE_S_PATH) w = PL_SORT_PATH;
		else if (id == ID_PE_S_FILENAME) w = PL_SORT_FILENAME;
		else if (id == ID_PE_S_RANDOM) w = PL_SORT_RANDOM;
		else if (id == ID_PE_S_REV) w = PL_SORT_REVERSE;
		PlayList_sort(w);
		plEditRefresh();
		draw_songname_title();
		return;
	}
	case ID_PE_FONTBIGGER:
	case ID_PE_FONTSMALLER:
	case ID_PE_FONTRESET:
		if (id == ID_PE_FONTBIGGER) config_pe_fontsize++;
		else if (id == ID_PE_FONTSMALLER)
		{
			if (config_pe_fontsize > 5) config_pe_fontsize--;
		}
		else config_pe_fontsize = 12;
		draw_pe_init();
		plEditRefresh();
		return;
	default:
		Main_OnCommand(id);
	}
}

void pe_command_with_shift(int id, bool shift)
{
	int old = mouse_stats;
	mouse_stats = shift ? MK_SHIFT : 0;
	pe_command(id);
	mouse_stats = old;
}

/* ---------------- the window ---------------- */

class PeHandler : public SkinWindow::Handler
{
public:
	void OnPaint(SkinWindow *) override
	{
		if (pe_dirty)
		{
			pe_dirty = false;
			draw_pe_paint();
		}
	}

	void OnMouse(SkinWindow *, int x, int y, int type, int stats, int rx, int ry) override
	{
		mouse_root_x = rx;
		mouse_root_y = ry;
		if (type == -2)
		{
			peui_reset();
			return;
		}
		peui_handlemouseevent(x, y, type, stats);
	}

	void OnDoubleClick(SkinWindow *, int x, int y, int stats, int rx, int ry) override
	{
		mouse_stats = stats;
		if (y <= 14 && x < config_pe_width - 20)
		{
			which_cap = 0;
			Main_OnCommand(WINAMP_OPTIONS_WINDOWSHADE_PL);
		}
		else if (config_pe_height != 14)
		{
			if (x >= 12 && y >= 20 && x <= config_pe_width - 20 && y <= config_pe_height - 38)
			{
				int t = (y - 22) / pe_fontheight;
				if (t < PlayList_getlength() - pledit_disp_offs && t < pe_num_songs())
				{
					t += pledit_disp_offs;
					which_cap = 0;
					PlayIndex(t);
					plEditRefresh();
				}
			}
			else if (x >= config_pe_width - 44 && y >= config_pe_height - 30 && x < config_pe_width - 44 + 22 && y < config_pe_height - 12)
			{
				peui_reset();
				pe_command(ID_PE_OPEN);
			}
			else if (x >= 101 && y >= config_pe_height - 30 && x < 122 && y < config_pe_height - 12)
			{
				peui_reset();
				popup_pl(MENU_PL_FILEINFO, 122, config_pe_height - 30);
			}
			else if (x >= 72 && y >= config_pe_height - 30 && x < 93 && y < config_pe_height - 12)
			{
				peui_reset();
				pe_command(ID_PE_SELECTALL);
			}
			else if (x >= 43 && y >= config_pe_height - 30 && x < 64 && y < config_pe_height - 12)
			{
				peui_reset();
				pe_command(IDC_PLAYLIST_REMOVEMP3);
			}
			else if (x >= 14 && y >= config_pe_height - 30 && x < 35 && y < config_pe_height - 12)
			{
				peui_reset();
				pe_command(IDC_PLAYLIST_ADDMP3);
			}
			else
			{
				mouse_root_x = rx;
				mouse_root_y = ry;
				peui_handlemouseevent(x, y, 1, stats);
			}
		}
	}

	void OnRightClick(SkinWindow *, int x, int y, int flags) override
	{
		if (peui_isrbuttoncaptured()) return;
		if (config_pe_height != 14 && config_pe_width >= 350 &&
			(x >= config_pe_width - 150 - 75 && y >= config_pe_height - 26 && x <= config_pe_width - 150 && y <= config_pe_height - 8))
			popup_menu(MENU_VIS, g_pl_wnd);
		else if (config_pe_height != 14 && (x >= 14 && y >= config_pe_height - 30 && x <= 32 && y <= config_pe_height - 12))
			popup_menu(MENU_PL_ADD, g_pl_wnd);
		else if (config_pe_height != 14 && (x >= 43 && y >= config_pe_height - 30 && x <= 64 && y <= config_pe_height - 12))
			popup_menu(MENU_PL_REMOVE, g_pl_wnd);
		else if (config_pe_height != 14 && (x >= 72 && y >= config_pe_height - 30 && x <= 93 && y <= config_pe_height - 12))
			popup_menu(MENU_PL_SELECT, g_pl_wnd);
		else if (config_pe_height != 14 && (x >= 101 && y >= config_pe_height - 30 && x <= 122 && y <= config_pe_height - 12))
			popup_menu(MENU_PL_MISC, g_pl_wnd);
		else if (config_pe_height != 14 && (x >= config_pe_width - 44 && y >= config_pe_height - 30 && x <= config_pe_width - 44 + 22 && y <= config_pe_height - 12))
			popup_menu(MENU_PL_LIST, g_pl_wnd);
		else if (config_pe_height != 14 && y >= 20 && y <= config_pe_height - 38 && x >= 12 && x <= config_pe_width - 20)
		{
			// right click in list box
			int wh = (y - 22) / pe_fontheight + pledit_disp_offs;
			int s = (flags & MK_CONTROL);
			if (!s && !PlayList_getselect(wh))
				for (int i = 0, t = PlayList_getlength(); i < t; i++)
					PlayList_setselect(i, 0);
			if (PlayList_getselect(wh) && s)
				PlayList_setselect(wh, 0);
			else
			{
				int yy = wh - pledit_disp_offs;
				if (yy < PlayList_getlength() - pledit_disp_offs && yy < pe_num_songs())
					PlayList_setselect(wh, 1);
			}
			plEditRefresh();
			popup_menu(MENU_PL_CONTEXT, g_pl_wnd);
		}
		else
			popup_menu(MENU_MAIN, g_pl_wnd);
	}

	void OnScroll(SkinWindow *, int, int, int delta, int stats) override
	{
		int lines = 2; // config_plmw2xscroll
		int cmd;
		if (config_pe_height == 14)
			cmd = delta > 0 ? WINAMP_BUTTON1 : WINAMP_BUTTON5;
		else if (stats & MK_MBUTTON)
			cmd = delta > 0 ? ID_PE_MOVEUP : ID_PE_MOVEDOWN;
		else
			cmd = delta > 0 ? ID_PE_SCROLLUP : ID_PE_SCROLLDOWN;
		if (config_pe_height == 14) lines = 1;
		while (lines--) pe_command(cmd);
	}

	void OnActivate(SkinWindow *, bool) override
	{
		plEditRefresh();
	}

	// WM_DROPFILES: insert at the drop position
	void OnDrop(SkinWindow *, int, int y, const std::vector<std::string> &files) override
	{
		int t;
		if (config_pe_height == 14) t = PlayList_getlength();
		else
		{
			t = (y - 20) / pe_fontheight;
			if (t < 0) t = 0;
			else if (t > pe_num_songs()) t = PlayList_getlength();
			else t += pledit_disp_offs;
			if (t > PlayList_getlength()) t = PlayList_getlength();
		}
		for (auto &f : files) t += PlayList_add(f, t);
		plEditRefresh();
	}

	void OnClose(SkinWindow *) override
	{
		app_quit();
	}
};

static PeHandler pe_handler_impl;
SkinWindow::Handler *pe_handler = &pe_handler_impl;
