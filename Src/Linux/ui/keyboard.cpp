/*
** Winamp for Linux - keyboard shortcuts.
**
** The tables are IDR_ACCELERATOR_MAIN / _PL / _EQ / _GLOBAL from
** Src/Winamp/Winamp.rc, with Windows virtual keys mapped to GDK key values.
*/
#include "ui.h"

#include <gdk/gdkkeysyms.h>

namespace
{
	enum { C = 1, S = 2, A = 4 };
	struct Accel
	{
		guint key;
		int mods;
		int id;
	};

	const Accel accel_main[] = {
		{GDK_KEY_F4, C, ID_PE_CLOSE},
		{GDK_KEY_KP_4, 0, WINAMP_BUTTON1}, {GDK_KEY_KP_Left, 0, WINAMP_BUTTON1},
		{GDK_KEY_z, 0, WINAMP_BUTTON1}, {GDK_KEY_z, C, WINAMP_BUTTON1_CTRL}, {GDK_KEY_z, S, WINAMP_BUTTON1_SHIFT},
		{GDK_KEY_KP_5, 0, WINAMP_BUTTON2}, {GDK_KEY_KP_Begin, 0, WINAMP_BUTTON2},
		{GDK_KEY_x, 0, WINAMP_BUTTON2},
		{GDK_KEY_c, 0, WINAMP_BUTTON3},
		{GDK_KEY_v, 0, WINAMP_BUTTON4}, {GDK_KEY_v, C, WINAMP_BUTTON4_CTRL}, {GDK_KEY_v, S, WINAMP_BUTTON4_SHIFT},
		{GDK_KEY_b, 0, WINAMP_BUTTON5}, {GDK_KEY_KP_6, 0, WINAMP_BUTTON5}, {GDK_KEY_KP_Right, 0, WINAMP_BUTTON5},
		{GDK_KEY_b, C, WINAMP_BUTTON5_CTRL}, {GDK_KEY_b, S, WINAMP_BUTTON5_SHIFT},
		{GDK_KEY_3, A, WINAMP_EDIT_ID3},
		{GDK_KEY_s, A, WINAMP_SELSKIN},
		{GDK_KEY_KP_9, 0, WINAMP_FFWD5S}, {GDK_KEY_KP_Page_Up, 0, WINAMP_FFWD5S}, {GDK_KEY_Right, 0, WINAMP_FFWD5S},
		{GDK_KEY_l, S, WINAMP_FILE_DIR}, {GDK_KEY_Insert, 0, WINAMP_FILE_DIR},
		{GDK_KEY_l, C, WINAMP_FILE_LOC}, {GDK_KEY_KP_0, C, WINAMP_FILE_LOC}, {GDK_KEY_x, C, WINAMP_FILE_LOC},
		{GDK_KEY_r, S, WINAMP_FILE_MANUALPLADVANCE},
		{GDK_KEY_l, 0, WINAMP_FILE_PLAY}, {GDK_KEY_KP_0, 0, WINAMP_FILE_PLAY}, {GDK_KEY_KP_Insert, 0, WINAMP_FILE_PLAY}, {GDK_KEY_x, S, WINAMP_FILE_PLAY},
		{GDK_KEY_r, 0, WINAMP_FILE_REPEAT},
		{GDK_KEY_s, 0, WINAMP_FILE_SHUFFLE},
		{GDK_KEY_KP_1, 0, WINAMP_JUMP10BACK}, {GDK_KEY_KP_End, 0, WINAMP_JUMP10BACK},
		{GDK_KEY_KP_3, 0, WINAMP_JUMP10FWD}, {GDK_KEY_KP_Page_Down, 0, WINAMP_JUMP10FWD},
		{GDK_KEY_j, 0, WINAMP_JUMPFILE}, {GDK_KEY_KP_Decimal, 0, WINAMP_JUMPFILE}, {GDK_KEY_KP_Delete, 0, WINAMP_JUMPFILE},
		{GDK_KEY_a, C, WINAMP_OPTIONS_AOT},
		{GDK_KEY_F5, 0, WINAMP_REFRESHSKIN},
		{GDK_KEY_Left, 0, WINAMP_REW5S}, {GDK_KEY_KP_7, 0, WINAMP_REW5S}, {GDK_KEY_KP_Home, 0, WINAMP_REW5S},
		{GDK_KEY_Down, 0, WINAMP_VOLUMEDOWN}, {GDK_KEY_KP_2, 0, WINAMP_VOLUMEDOWN}, {GDK_KEY_KP_Down, 0, WINAMP_VOLUMEDOWN},
		{GDK_KEY_KP_8, 0, WINAMP_VOLUMEUP}, {GDK_KEY_KP_Up, 0, WINAMP_VOLUMEUP}, {GDK_KEY_Up, 0, WINAMP_VOLUMEUP},
		{GDK_KEY_e, C, WINAMP_OPTIONS_EASYMOVE},
		{0, 0, 0}};

	const Accel accel_pl[] = {
		{GDK_KEY_End, 0, ID_PE_BOTTOM},
		{GDK_KEY_Down, 0, ID_PE_SCDOWN},
		{GDK_KEY_Page_Down, 0, ID_PE_SCROLLDOWN},
		{GDK_KEY_Page_Up, 0, ID_PE_SCROLLUP},
		{GDK_KEY_Up, 0, ID_PE_SCUP},
		{GDK_KEY_space, 0, ID_PE_SHOWPLAYING},
		{GDK_KEY_Home, 0, ID_PE_TOP},
		{GDK_KEY_Insert, 0, IDC_PLAYLIST_ADDDIR},
		{GDK_KEY_l, 0, IDC_PLAYLIST_ADDMP3}, {GDK_KEY_KP_0, 0, IDC_PLAYLIST_ADDMP3}, {GDK_KEY_KP_Insert, 0, IDC_PLAYLIST_ADDMP3},
		{GDK_KEY_Return, 0, IDC_PLAYLIST_PLAY}, {GDK_KEY_KP_Enter, 0, IDC_PLAYLIST_PLAY},
		{GDK_KEY_Delete, 0, IDC_PLAYLIST_REMOVEMP3},
		{GDK_KEY_KP_4, 0, WINAMP_BUTTON1}, {GDK_KEY_KP_Left, 0, WINAMP_BUTTON1}, {GDK_KEY_z, 0, WINAMP_BUTTON1},
		{GDK_KEY_KP_5, 0, WINAMP_BUTTON2}, {GDK_KEY_KP_Begin, 0, WINAMP_BUTTON2}, {GDK_KEY_x, 0, WINAMP_BUTTON2},
		{GDK_KEY_c, 0, WINAMP_BUTTON3},
		{GDK_KEY_v, 0, WINAMP_BUTTON4},
		{GDK_KEY_b, 0, WINAMP_BUTTON5}, {GDK_KEY_KP_6, 0, WINAMP_BUTTON5}, {GDK_KEY_KP_Right, 0, WINAMP_BUTTON5},
		{GDK_KEY_KP_9, 0, WINAMP_FFWD5S}, {GDK_KEY_KP_Page_Up, 0, WINAMP_FFWD5S}, {GDK_KEY_Right, 0, WINAMP_FFWD5S},
		{GDK_KEY_r, 0, WINAMP_FILE_REPEAT},
		{GDK_KEY_s, 0, WINAMP_FILE_SHUFFLE},
		{GDK_KEY_KP_1, 0, WINAMP_JUMP10BACK}, {GDK_KEY_KP_End, 0, WINAMP_JUMP10BACK},
		{GDK_KEY_KP_3, 0, WINAMP_JUMP10FWD}, {GDK_KEY_KP_Page_Down, 0, WINAMP_JUMP10FWD},
		{GDK_KEY_j, 0, WINAMP_JUMPFILE}, {GDK_KEY_KP_Decimal, 0, WINAMP_JUMPFILE}, {GDK_KEY_KP_Delete, 0, WINAMP_JUMPFILE},
		{GDK_KEY_F5, 0, WINAMP_REFRESHSKIN},
		{GDK_KEY_Left, 0, WINAMP_REW5S}, {GDK_KEY_KP_7, 0, WINAMP_REW5S}, {GDK_KEY_KP_Home, 0, WINAMP_REW5S},
		{GDK_KEY_KP_2, 0, WINAMP_VOLUMEDOWN}, {GDK_KEY_KP_Down, 0, WINAMP_VOLUMEDOWN},
		{GDK_KEY_KP_8, 0, WINAMP_VOLUMEUP}, {GDK_KEY_KP_Up, 0, WINAMP_VOLUMEUP},
		{GDK_KEY_n, C, ID_PE_CLEAR},
		{GDK_KEY_F4, C, ID_PE_CLOSE},
		{GDK_KEY_e, C, ID_PE_ENTRY},
		{GDK_KEY_f, C, ID_PE_FFOD},
		{GDK_KEY_KP_Add, C, ID_PE_FONTBIGGER}, {GDK_KEY_plus, C, ID_PE_FONTBIGGER}, {GDK_KEY_equal, C, ID_PE_FONTBIGGER},
		{GDK_KEY_Return, C, ID_PE_FONTRESET},
		{GDK_KEY_KP_Subtract, C, ID_PE_FONTSMALLER}, {GDK_KEY_minus, C, ID_PE_FONTSMALLER},
		{GDK_KEY_r, C, ID_PE_S_REV},
		{GDK_KEY_a, C, ID_PE_SELECTALL},
		{GDK_KEY_l, C, IDC_PLAYLIST_ADDLOC}, {GDK_KEY_KP_0, C, IDC_PLAYLIST_ADDLOC},
		{GDK_KEY_Insert, C, IDC_PLAYLIST_ADDMP3},
		{GDK_KEY_Delete, C, IDC_PLAYLIST_CROP},
		{GDK_KEY_i, C, IDC_SELECTINV},
		{GDK_KEY_z, C, WINAMP_BUTTON1_CTRL},
		{GDK_KEY_v, C, WINAMP_BUTTON4_CTRL},
		{GDK_KEY_b, C, WINAMP_BUTTON5_CTRL},
		{GDK_KEY_x, C, WINAMP_FILE_LOC},
		{GDK_KEY_3, A, ID_PE_ID3},
		{GDK_KEY_Down, A, ID_PE_MOVEDOWN},
		{GDK_KEY_Up, A, ID_PE_MOVEUP},
		{GDK_KEY_Delete, A, ID_PE_NONEXIST},
		{GDK_KEY_s, A, WINAMP_SELSKIN},
		{GDK_KEY_e, C | A, ID_PE_EXTINFO},
		{GDK_KEY_g, C | A, ID_PE_PRINT},
		{GDK_KEY_a, C | A, WINAMP_OPTIONS_AOT},
		{GDK_KEY_Down, S, ID_PE_SCDOWN},
		{GDK_KEY_Up, S, ID_PE_SCUP},
		{GDK_KEY_l, S, IDC_PLAYLIST_ADDDIR},
		{GDK_KEY_Insert, S, IDC_PLAYLIST_ADDLOC},
		{GDK_KEY_z, S, WINAMP_BUTTON1_SHIFT},
		{GDK_KEY_v, S, WINAMP_BUTTON4_SHIFT},
		{GDK_KEY_b, S, WINAMP_BUTTON5_SHIFT},
		{GDK_KEY_x, S, WINAMP_FILE_PLAY},
		{GDK_KEY_Delete, S | C, ID_PE_CLEAR},
		{GDK_KEY_2, S | C, ID_PE_S_FILENAME},
		{GDK_KEY_3, S | C, ID_PE_S_PATH},
		{GDK_KEY_r, S | C, ID_PE_S_RANDOM},
		{GDK_KEY_1, S | C, ID_PE_S_TITLE},
		{GDK_KEY_e, S, ID_PE_EDIT_SEL},
		{0, 0, 0}};

	const Accel accel_eq[] = {
		{GDK_KEY_a, 0, EQ_AUTO},
		{GDK_KEY_q, 0, EQ_DEC1}, {GDK_KEY_w, 0, EQ_DEC2}, {GDK_KEY_e, 0, EQ_DEC3}, {GDK_KEY_r, 0, EQ_DEC4}, {GDK_KEY_t, 0, EQ_DEC5},
		{GDK_KEY_y, 0, EQ_DEC6}, {GDK_KEY_u, 0, EQ_DEC7}, {GDK_KEY_i, 0, EQ_DEC8}, {GDK_KEY_o, 0, EQ_DEC9}, {GDK_KEY_p, 0, EQ_DEC10},
		{GDK_KEY_Tab, 0, EQ_DECPRE},
		{GDK_KEY_n, 0, EQ_ENABLE},
		{GDK_KEY_1, 0, EQ_INC1}, {GDK_KEY_2, 0, EQ_INC2}, {GDK_KEY_3, 0, EQ_INC3}, {GDK_KEY_4, 0, EQ_INC4}, {GDK_KEY_5, 0, EQ_INC5},
		{GDK_KEY_6, 0, EQ_INC6}, {GDK_KEY_7, 0, EQ_INC7}, {GDK_KEY_8, 0, EQ_INC8}, {GDK_KEY_9, 0, EQ_INC9}, {GDK_KEY_0, 0, EQ_INC10},
		{GDK_KEY_grave, 0, EQ_INCPRE},
		{GDK_KEY_Left, 0, EQ_PANLEFT},
		{GDK_KEY_Right, 0, EQ_PANRIGHT},
		{GDK_KEY_s, 0, EQ_PRESETS},
		{GDK_KEY_F4, C, ID_PE_CLOSE},
		{GDK_KEY_s, C | A, IDM_EQ_LOADPRE},
		{GDK_KEY_z, 0, WINAMP_BUTTON1}, {GDK_KEY_z, C, WINAMP_BUTTON1_CTRL},
		{GDK_KEY_x, 0, WINAMP_BUTTON2},
		{GDK_KEY_c, 0, WINAMP_BUTTON3},
		{GDK_KEY_v, 0, WINAMP_BUTTON4}, {GDK_KEY_v, C, WINAMP_BUTTON4_CTRL}, {GDK_KEY_v, S, WINAMP_BUTTON4_SHIFT},
		{GDK_KEY_b, 0, WINAMP_BUTTON5}, {GDK_KEY_b, C, WINAMP_BUTTON5_CTRL},
		{GDK_KEY_x, C, WINAMP_FILE_LOC},
		{GDK_KEY_x, S, WINAMP_FILE_PLAY},
		{GDK_KEY_j, 0, WINAMP_JUMPFILE}, {GDK_KEY_KP_Decimal, 0, WINAMP_JUMPFILE},
		{GDK_KEY_a, C, WINAMP_OPTIONS_AOT},
		{GDK_KEY_Down, 0, WINAMP_VOLUMEDOWN},
		{GDK_KEY_Up, 0, WINAMP_VOLUMEUP},
		{GDK_KEY_e, C, WINAMP_OPTIONS_EASYMOVE},
		{0, 0, 0}};

	const Accel accel_global[] = {
		{GDK_KEY_F1, 0, WINAMP_HELP_ABOUT}, // ID_HELP_HELPTOPICS: there is no help file, show the about box
		{GDK_KEY_F4, A, WINAMP_FILE_QUIT},
		{GDK_KEY_F1, C, WINAMP_HELP_ABOUT},
		{GDK_KEY_j, C, WINAMP_JUMP},
		{GDK_KEY_w, A, WINAMP_MAIN_WINDOW},
		{GDK_KEY_f, A, WINAMP_MAINMENU},
		{GDK_KEY_m, A, WINAMP_MINIMIZE},
		{GDK_KEY_Tab, C, WINAMP_NEXT_WINDOW},
		{GDK_KEY_d, C, WINAMP_OPTIONS_DSIZE},
		{GDK_KEY_g, A, WINAMP_OPTIONS_EQ},
		{GDK_KEY_e, A, WINAMP_OPTIONS_PLEDIT},
		{GDK_KEY_p, C, WINAMP_OPTIONS_PREFS},
		{GDK_KEY_t, C, WINAMP_OPTIONS_TOGTIME},
		{GDK_KEY_w, C, WINAMP_OPTIONS_WINDOWSHADE_GLOBAL},
		{GDK_KEY_o, C, ID_PE_OPEN},
		{GDK_KEY_s, C, ID_PE_SAVEAS},
		{0, 0, 0}};
}

static int find(const Accel *table, guint key, int mods)
{
	for (const Accel *a = table; a->key; a++)
		if (a->key == key && a->mods == mods) return a->id;
	return 0;
}

bool handle_key(SkinWindow *w, GdkEventKey *e)
{
	int mods = 0;
	if (e->state & GDK_CONTROL_MASK) mods |= C;
	if (e->state & GDK_SHIFT_MASK) mods |= S;
	if (e->state & GDK_MOD1_MASK) mods |= A;

	// the key without shift/caps lock, so that Ctrl+Shift+1 is "1" and not "!"
	guint key = e->keyval;
	guint plain = 0;
	if (gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(gdk_display_get_default()), e->hardware_keycode,
	                                        (GdkModifierType)(e->state & GDK_MOD2_MASK), e->group, &plain, nullptr, nullptr, nullptr))
		key = plain;
	key = gdk_keyval_to_lower(key);

	// the "NULLSOFT" easter egg (WM_KEYDOWN in main.cpp) toggles the titlebar
	if (w == g_main_wnd && !mods)
	{
		static const char egg[] = "nullsoft";
		static int pos;
		if (key == (guint)egg[pos])
		{
			if (!egg[++pos])
			{
				pos = 0;
				eggstat = !eggstat;
				draw_tbar(1, config_windowshade, eggstat);
			}
		}
		else
			pos = key == (guint)egg[0] ? 1 : 0;
	}

	const Accel *table = w == g_eq_wnd ? accel_eq : w == g_pl_wnd ? accel_pl : accel_main;
	int id = find(table, key, mods);
	if (!id) id = find(accel_global, key, mods);
	if (!id) return false;

	if (id == ID_PE_CLOSE)
	{
		if (w == g_eq_wnd) Main_OnCommand(WINAMP_OPTIONS_EQ);
		else if (w == g_pl_wnd) Main_OnCommand(WINAMP_OPTIONS_PLEDIT);
		return true;
	}
	if (id == WINAMP_OPTIONS_WINDOWSHADE_GLOBAL)
	{
		Main_OnCommand(w == g_eq_wnd ? WINAMP_OPTIONS_WINDOWSHADE_EQ : w == g_pl_wnd ? WINAMP_OPTIONS_WINDOWSHADE_PL : WINAMP_OPTIONS_WINDOWSHADE);
		return true;
	}
	if (id == WINAMP_MAINMENU)
	{
		popup_menu(MENU_MAIN, w, w->X(), w->Y() + 14 * w->Scale());
		return true;
	}
	if (id == WINAMP_MINIMIZE)
	{
		gtk_window_iconify(GTK_WINDOW(g_main_wnd->Widget()));
		return true;
	}
	if (id == WINAMP_OPTIONS_TOGTIME)
	{
		Main_OnCommand(config_timeleftmode ? WINAMP_OPTIONS_ELAPSED : WINAMP_OPTIONS_REMAINING);
		return true;
	}
	if (w == g_pl_wnd && (id == ID_PE_SCUP || id == ID_PE_SCDOWN))
	{
		// shift extends the selection (uses the modifier state like GetAsyncKeyState)
		extern void pe_command_with_shift(int id, bool shift);
		pe_command_with_shift(id, mods & S);
		return true;
	}
	Main_OnCommand(id);
	return true;
}
