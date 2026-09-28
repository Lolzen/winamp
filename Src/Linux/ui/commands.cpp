/*
** Winamp for Linux - command handling.
**
** Main_OnCommand() is the WM_COMMAND handler of Src/Winamp/main_command.cpp
** and main_buttons.cpp (transport buttons, window toggles, options);
** display_timer_tick() is Main_OnTimer(UPDATE_DISPLAY_TIMER) from
** Src/Winamp/main_timer.cpp.
*/
#include "ui.h"
#include "menu_ids.h"
#include "mpris.h"
#include "../core/eq.h"
#include "../core/player.h"
#include "../core/playlist.h"
#include "../core/plugins.h"
#include "../core/vis.h"

#include <gio/gio.h>

static int last_brate = -1;

void set_caption()
{
	std::string t = FileTitle.empty() ? std::string(APP_NAME) + " " + APP_VERSION : FileTitle + " - " + APP_NAME;
	if (playing && config_dotitlenum && !FileTitle.empty())
		t = std::to_string(PlayList_getPosition() + 1) + ". " + t;
	if (paused) t += " [Paused]";
	else if (!playing && !FileTitle.empty()) t += " [Stopped]";
	if (g_main_wnd) gtk_window_set_title(GTK_WINDOW(g_main_wnd->Widget()), t.c_str());
}

void set_aot(int apply)
{
	(void)apply;
	g_main_wnd->SetKeepAbove(config_aot);
	g_eq_wnd->SetKeepAbove(config_aot);
	g_pl_wnd->SetKeepAbove(config_aot);
	draw_clutterbar(0);
}

// sizes of all windows for the current windowshade/double size settings
void main_update_sizes()
{
	g_main_wnd->HideOverlay(); // the visualizer redraws itself at the new size
	g_main_wnd->SetScale(config_dsize ? 2 : 1);
	g_main_wnd->SetSize(WINDOW_WIDTH, config_windowshade ? 14 : WINDOW_HEIGHT);
	g_main_wnd->SetShape(config_windowshade ? &g_skin.rgn_main_ws : &g_skin.rgn_main);
	g_eq_wnd->SetScale(config_dsize && config_eqdsize ? 2 : 1);
	g_eq_wnd->SetSize(WINDOW_WIDTH, config_eq_ws ? 14 : WINDOW_HEIGHT);
	g_eq_wnd->SetShape(config_eq_ws ? &g_skin.rgn_eq_ws : &g_skin.rgn_eq);
	g_pl_wnd->SetSize(config_pe_width, config_pe_height);
}

void main_toggle_windowshade()
{
	RECT before[3];
	window_rects(before);
	config_windowshade = !config_windowshade;
	main_update_sizes();
	resize_with_docking(before);
	draw_main_all();
}

// the notices ToggleShuffle/Repeat show in the song title display
static void notice(const wchar_t *text)
{
	draw_songname(text, &ui_songposition, -2);
}

void refresh_skin()
{
	skin_load();
	draw_init();
	draw_eq_init();
	draw_pe_init();
	main_update_sizes();
	draw_main_all();
	draw_eq_all();
	plEditRefresh();
}

void app_quit()
{
	g_application_quit(g_application_get_default());
}

void on_player_info()
{
	if (!playing) return;
	last_brate = g_brate;
	draw_bitmixrate(g_brate, g_srate);
	draw_monostereo(g_nch <= 0 ? 0 : g_nch == 1 ? 1 : 2);
}

void on_player_state()
{
	static int was_playing = -1;
	if (playing)
	{
		draw_playicon(paused ? 4 : 1);
		if (was_playing != 1)
		{
			ui_songposition = 0;
			draw_monostereo(g_nch <= 0 ? 0 : g_nch == 1 ? 1 : 2);
			if (in_seekable()) draw_positionbar(0, 0);
		}
		display_timer_tick();
		plEditSelect(PlayList_getPosition());
	}
	else
	{
		// StopPlaying(): back to the stopped look
		draw_clear();
		draw_main_all();
		plEditRefresh();
	}
	was_playing = playing;
	draw_songname_title();
	set_caption();
	mpris_update();
}

void display_timer_tick()
{
	if (playing)
	{
		int a = in_getouttime() / 1000;
		int l = in_getlength();
		if (!config_minimized && (config_mw_open || config_pe_open))
		{
			static int t = -1;
			static int la = -123;
			static int ll = -15055;
			if (paused)
			{
				if (t == -1) t = 10;
				else t--;
			}
			else t = -1;
			if (a != la || l != ll || paused) ui_drawtime(a, 0);
			la = a;
			ll = l;
		}
		if (g_brate != last_brate)
			draw_bitmixrate(last_brate = g_brate, g_srate);
	}
}

static void open_and_play(const std::vector<std::string> &items)
{
	if (items.empty()) return;
	StopPlaying(0);
	PlayList_clear();
	pledit_disp_offs = 0;
	for (auto &f : items) PlayList_add(f);
	PlayList_setposition(0);
	if (config_shuffle) PlayList_reshuffle();
	StartPlaying();
	plEditRefresh();
}

static void cycle_window()
{
	// WINAMP_NEXT_WINDOW: main -> eq -> playlist
	SkinWindow *order[3] = {g_main_wnd, g_eq_wnd, g_pl_wnd};
	int cur = 0;
	for (int i = 0; i < 3; i++)
		if (order[i]->Active()) cur = i;
	for (int i = 1; i <= 3; i++)
	{
		SkinWindow *w = order[(cur + i) % 3];
		if (w->Visible())
		{
			w->Raise();
			return;
		}
	}
}

void Main_OnCommand(int id)
{
	if (id >= WAL_SKIN_FIRST && id <= WAL_SKIN_LAST)
	{
		config_skin = skin_menu_name(id);
		refresh_skin();
		return;
	}
	if (id >= WAL_VIS_FALLOFF0 && id < WAL_VIS_FALLOFF0 + 5)
	{
		config_safalloff = id - WAL_VIS_FALLOFF0;
		return;
	}
	if (id >= WAL_VIS_PFALLOFF0 && id < WAL_VIS_PFALLOFF0 + 5)
	{
		config_sa_peak_falloff = id - WAL_VIS_PFALLOFF0;
		return;
	}

	switch (id)
	{
	/* ---- transport (main_buttons.cpp) ---- */
	case WINAMP_BUTTON1:
		PlayPrevious();
		return;
	case WINAMP_BUTTON1_SHIFT:
	case WINAMP_REW5S:
		player_seek_relative(-5000);
		return;
	case WINAMP_BUTTON1_CTRL:
		PlayList_setposition(0);
		if (playing) StartPlaying();
		else
		{
			player_update_title();
			on_player_state();
		}
		return;
	case WINAMP_BUTTON2:
	case WINAMP_BUTTON2_SHIFT:
	case WINAMP_BUTTON2_CTRL:
		if (id == WINAMP_BUTTON2_CTRL) Main_OnCommand(WINAMP_FILE_LOC);
		else if (id == WINAMP_BUTTON2_SHIFT) Main_OnCommand(WINAMP_FILE_PLAY);
		else if (!playing)
		{
			if (!PlayList_getlength()) Main_OnCommand(WINAMP_FILE_PLAY);
			else StartPlaying();
		}
		else if (paused) PausePlaying();
		else StartPlaying();
		return;
	case WINAMP_BUTTON3:
		if (playing) PausePlaying();
		return;
	case WINAMP_BUTTON4:
		StopPlaying(0);
		return;
	case WINAMP_BUTTON4_SHIFT:
		StopPlaying(1);
		return;
	case WINAMP_BUTTON4_CTRL:
		if (playing) g_stopaftercur = !g_stopaftercur;
		return;
	case WINAMP_BUTTON5:
		PlayNext(true);
		return;
	case WINAMP_BUTTON5_SHIFT:
	case WINAMP_FFWD5S:
		player_seek_relative(5000);
		return;
	case WINAMP_BUTTON5_CTRL:
		PlayList_setposition(PlayList_getlength() - 1);
		if (playing) StartPlaying();
		else
		{
			player_update_title();
			on_player_state();
		}
		return;
	case WINAMP_JUMP10BACK:
	case WINAMP_JUMP10FWD:
		PlayList_advance(id == WINAMP_JUMP10FWD ? 10 : -10);
		if (playing) StartPlaying();
		else
		{
			player_update_title();
			on_player_state();
		}
		return;

	/* ---- files ---- */
	case WINAMP_FILE_PLAY:
		open_and_play(dlg_open_files(dlg_parent(), "Open file(s)", true));
		return;
	case WINAMP_FILE_DIR:
	{
		std::string d = dlg_open_folder(dlg_parent(), "Open folder");
		if (!d.empty()) open_and_play({d});
		return;
	}
	case WINAMP_FILE_LOC:
	{
		std::string u = dlg_input(dlg_parent(), "Open URL", "Enter URL to open:", "http://");
		if (!u.empty() && u != "http://") open_and_play({u});
		return;
	}
	case WINAMP_FILE_QUIT:
		app_quit();
		return;
	case WINAMP_EDIT_ID3:
	{
		std::string fn = playing ? FileName : PlayList_getfilename(PlayList_getPosition());
		if (!fn.empty()) dlg_file_info(fn);
		return;
	}
	case WINAMP_JUMP:
		dlg_jump_to_time();
		return;
	case WINAMP_JUMPFILE:
		dlg_jump_to_file();
		return;

	/* ---- toggles ---- */
	case WINAMP_FILE_SHUFFLE:
		config_shuffle = !config_shuffle;
		if (config_shuffle) PlayList_reshuffle();
		draw_shuffle(config_shuffle, 0);
		notice(config_shuffle ? L"Shuffle: On" : L"Shuffle: Off");
		mpris_update();
		return;
	case WINAMP_FILE_REPEAT:
		config_repeat = !config_repeat;
		draw_repeat(config_repeat, 0);
		notice(!config_repeat ? L"Repeat: Off" : config_pladv ? L"Repeat: Playlist" : L"Repeat: Track");
		mpris_update();
		return;
	case WINAMP_FILE_MANUALPLADVANCE:
		config_pladv = !config_pladv;
		if (!config_repeat) notice(config_pladv ? L"Manual Advance: Off" : L"Manual Advance: On");
		else notice(config_pladv ? L"Repeat: Playlist" : L"Repeat: Track");
		return;
	case WINAMP_TOGGLE_AUTOSCROLL:
		config_autoscrollname ^= 1;
		ui_songposition = 0;
		draw_songname_title();
		return;
	case WINAMP_OPTIONS_ELAPSED:
		config_timeleftmode = 0;
		display_timer_tick();
		return;
	case WINAMP_OPTIONS_REMAINING:
		config_timeleftmode = 1;
		display_timer_tick();
		return;
	case WINAMP_OPTIONS_AOT:
		config_aot = !config_aot;
		set_aot(1);
		return;
	case WINAMP_OPTIONS_DSIZE:
	{
		RECT before[3];
		window_rects(before);
		config_dsize = !config_dsize;
		main_update_sizes();
		resize_with_docking(before);
		draw_main_all();
		draw_eq_all();
		return;
	}
	case WINAMP_OPTIONS_EASYMOVE:
		config_easymove = !config_easymove;
		return;
	case WINAMP_OPTIONS_WINDOWSHADE:
		main_toggle_windowshade();
		return;
	case WINAMP_OPTIONS_WINDOWSHADE_EQ:
	{
		RECT before[3];
		window_rects(before);
		config_eq_ws = !config_eq_ws;
		draw_eq_init();
		main_update_sizes();
		resize_with_docking(before);
		draw_eq_all();
		return;
	}
	case WINAMP_OPTIONS_WINDOWSHADE_PL:
	{
		RECT before[3];
		window_rects(before);
		if (config_pe_height == 14)
		{
			if (config_pe_height_ws < 116) config_pe_height_ws = 116;
			config_pe_height = config_pe_height_ws;
			config_pe_height_ws = 0;
		}
		else
		{
			config_pe_height_ws = config_pe_height;
			config_pe_height = 14;
		}
		g_pl_wnd->SetSize(config_pe_width, config_pe_height);
		resize_with_docking(before);
		plEditRefresh();
		return;
	}
	case WINAMP_OPTIONS_EQ:
		eq_dialog(-1);
		return;
	case WINAMP_OPTIONS_PLEDIT:
		pleditDlg(-1);
		return;
	case WINAMP_MAIN_WINDOW:
		config_mw_open = !config_mw_open;
		if (config_mw_open) g_main_wnd->Show();
		else g_main_wnd->Hide();
		return;
	case WINAMP_NEXT_WINDOW:
		cycle_window();
		return;
	case WINAMP_VOLUMEUP:
	case WINAMP_VOLUMEDOWN:
		if (id == WINAMP_VOLUMEUP)
		{
			if (config_volume <= 251) config_volume += 4;
			else config_volume = 255;
		}
		else
		{
			if (config_volume > 3) config_volume -= 4;
			else config_volume = 0;
		}
		in_setvol(config_volume);
		draw_volumebar(config_volume, 0);
		update_volume_text(-2);
		if (config_eq_ws) draw_eq_tbar(g_eq_wnd->Active() ? 1 : (config_hilite ? 0 : 1));
		return;

	/* ---- dialogs ---- */
	case WINAMP_HELP_ABOUT:
	case WINAMP_LIGHTNING_CLICK:
		dlg_about();
		return;
	case WINAMP_OPTIONS_PREFS:
		dlg_preferences();
		return;
	case WINAMP_SELSKIN:
		dlg_skin_browser();
		return;
	case WAL_SKIN_CLASSIC:
		config_skin.clear();
		refresh_skin();
		return;
	case WINAMP_REFRESHSKIN:
		refresh_skin();
		return;

	/* ---- classic visualizer options ---- */
	case WAL_VIS_ANALYZER: config_sa = 1; sa_setmode(1); return;
	case WAL_VIS_SCOPE: config_sa = 2; sa_setmode(2); return;
	case WAL_VIS_OFF: config_sa = 0; sa_setmode(0); return;
	case WAL_VIS_NORMAL: config_safire = (config_safire & ~3) | 0; return;
	case WAL_VIS_FIRE: config_safire = (config_safire & ~3) | 1; return;
	case WAL_VIS_LINE: config_safire = (config_safire & ~3) | 2; return;
	case WAL_VIS_PEAKS: config_sa_peaks = !config_sa_peaks; return;
	case WAL_VIS_THICK: config_safire &= ~32; return;
	case WAL_VIS_THIN: config_safire |= 32; return;
	case WAL_VIS_DOTS: config_safire = (config_safire & ~12) | (0 << 2); return;
	case WAL_VIS_LINES: config_safire = (config_safire & ~12) | (1 << 2); return;
	case WAL_VIS_SOLID: config_safire = (config_safire & ~12) | (2 << 2); return;
	case WAL_VIS_REFRESH1: config_saref = 1; return;
	case WAL_VIS_REFRESH2: config_saref = 2; return;
	case WAL_VIS_REFRESH4: config_saref = 4; return;
	case WAL_VIS_REFRESH8: config_saref = 8; return;
	}

	// equalizer and playlist editor commands
	if ((id >= EQ_INC1 && id <= EQ_INC10) || (id >= EQ_DEC1 && id <= EQ_DEC10) || id == EQ_INCPRE || id == EQ_DECPRE ||
	    id == EQ_ENABLE || id == EQ_AUTO || id == EQ_PRESETS || id == EQ_PANLEFT || id == EQ_PANRIGHT ||
	    id == IDM_EQ_LOADPRE || id == IDM_EQ_LOADMP3 || id == IDM_EQ_LOADDEFAULT || id == ID_LOAD_EQF ||
	    id == IDM_EQ_SAVEPRE || id == IDM_EQ_SAVEMP3 || id == IDM_EQ_SAVEDEFAULT || id == ID_SAVE_EQF ||
	    id == IDM_EQ_DELPRE || id == IDM_EQ_DELMP3)
	{
		eq_command(id);
		return;
	}
	switch (id)
	{
	case ID_PE_SHOWPLAYING: case ID_PE_FFOD: case ID_PE_EXTINFO: case ID_PE_PRINT: case ID_PE_OPEN: case ID_PE_SAVEAS:
	case ID_PE_ID3: case ID_PE_EDIT_SEL: case ID_PE_ENTRY: case ID_PE_CLEAR: case IDC_SELECTINV: case ID_PE_SELECTALL:
	case ID_PE_NONE: case ID_PE_SCUP: case ID_PE_SCDOWN: case ID_PE_SCROLLDOWN: case ID_PE_SCROLLUP: case ID_PE_MOVEDOWN:
	case ID_PE_MOVEUP: case ID_PE_TOP: case ID_PE_BOTTOM: case IDC_PLAYLIST_ADDMP3: case IDC_PLAYLIST_ADDDIR:
	case IDC_PLAYLIST_ADDLOC: case IDC_PLAYLIST_REMOVEMP3: case IDC_PLAYLIST_CROP: case IDC_PLAYLIST_PLAY: case ID_PE_NONEXIST:
	case ID_PE_DELETEFROMDISK: case ID_PE_S_TITLE: case ID_PE_S_FILENAME: case ID_PE_S_PATH: case ID_PE_S_RANDOM:
	case ID_PE_S_REV: case ID_PE_FONTBIGGER: case ID_PE_FONTSMALLER: case ID_PE_FONTRESET:
		pe_command(id);
		return;
	}
}
