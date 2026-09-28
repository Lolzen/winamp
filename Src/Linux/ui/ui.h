/*
** Winamp for Linux - shared declarations of the classic UI
** (the Linux counterpart of the UI parts of Src/Winamp/Main.h).
*/
#pragma once

#include "gfx.h"
#include "skin.h"
#include "skinwnd.h"
#include "../core/common.h"
#include "../core/config.h"

// command ids are shared with the Windows build
#include "../../Winamp/resource.h"

#include <string>

#define WINDOW_WIDTH 275
#define WINDOW_HEIGHT 116

extern SkinWindow *g_main_wnd, *g_eq_wnd, *g_pl_wnd;

/* ---- draw_main.cpp (Src/Winamp/draw_main.cpp + draw.cpp) ---- */
extern int eggstat;
extern TextFont mfont;               // main window title font (when not using the skin's bitmap font)
extern uint32_t mfont_fgcolor, mfont_bgcolor;
void draw_init();                // after a skin change
void draw_clear();
void draw_setnoupdate(int v);
void update_area(int x, int y, int w, int h);
void getXYfromChar(wchar_t ic, int *x, int *y);
void draw_tbar(int active, int windowshade, int egg);
void draw_tbuttons(int b1, int b2, int b3, int b4);
void draw_eject(int pressed);
void draw_time(int minutes, int seconds, int clear);
void draw_playicon(int whichicon); // 0 none, 1 play, 2 stop, 4 pause
void draw_buttonbar(int buttonpressed);
void draw_bitmixrate(int bitrate, int mixrate);
void draw_positionbar(int position, int pressed);
void draw_monostereo(int value);
void draw_songname(const std::wstring &name, int *position, int songlen);
void draw_songname_title(); // FileTitle with the playing length
void draw_panbar(int volume, int pressed);
void draw_volumebar(int volume, int pressed);
void draw_shuffle(int on, int pressed);
void draw_repeat(int on, int pressed);
void draw_eqplbut(int eqon, int eqpressed, int plon, int plpressed);
void draw_clutterbar(int enable);
void update_panning_text(int songlen);
void update_volume_text(int songlen);
void draw_main_all();            // everything, from the current state

/* ---- draw_sa.cpp ---- */
void draw_sa(unsigned char *values, int draw);
void vis_timer_tick();           // ~60 fps

/* ---- draw_eq.cpp ---- */
void draw_eq_init();
void draw_eq_presets(int pressed);
void draw_eq_tbar(int active);
void draw_eq_slid(int which, int pos, int pressed);
void draw_eq_onauto(int on, int autoon, int onpressed, int autopressed);
void draw_eq_graphthingy();
void draw_eq_tbutton(int b3, int wsb);
void draw_eq_all();

/* ---- draw_pe.cpp ---- */
extern int pe_fontheight;
extern int pledit_disp_offs;
extern TextFont plfont;
void draw_pe_init();             // after a skin/font change
void draw_pe_paint();            // redraw everything into the playlist window buffer
void draw_pe_iobut(int which);
void draw_pe_miscbut(int which);
void draw_pe_selbut(int which);
void draw_pe_rembut(int which);
void draw_pe_addbut(int which);
void draw_pe_vslide(int pushed, int pos);
void draw_pe_tbutton(int b2, int b3, int b2_ws);
void draw_pe_timedisp(int minutes, int seconds, int tlm, int clear);
int pe_num_songs();              // visible rows

/* ---- ui_main.cpp (Src/Winamp/Ui.cpp + main_mouse.cpp) ---- */
extern int ui_songposition, ui_songposition_tts;
extern int do_posbar_active, do_volbar_active, do_panbar_active;
void ui_handlemouseevent(int x, int y, int type, int stats, int root_x, int root_y);
void ui_drawtime(int time_elapsed, int mode);
void ui_doscrolling();
void ui_reset();
extern SkinWindow::Handler *main_handler;

/* ---- ui_eq.cpp (Src/Winamp/Equi.cpp, Eq.cpp) ---- */
extern SkinWindow::Handler *eq_handler;
void eq_dialog(int show);        // toggle equalizer window
void eq_command(int id);         // WM_COMMAND in the EQ window

/* ---- ui_pe.cpp (Src/Winamp/Peui.cpp, Pledit.cpp) ---- */
extern SkinWindow::Handler *pe_handler;
void pleditDlg(int show);        // toggle playlist window
void pe_command(int id);         // WM_COMMAND in the playlist window
void plEditRefresh();
void plEditSelect(int song);     // make song visible

/* ---- docking.cpp (Src/Winamp/DOCK.cpp) ---- */
void EstMainWindowRect(RECT *r);
void EstEQWindowRect(RECT *r);
void EstPLWindowRect(RECT *r);
void SetMainWindowRect(const RECT *r);
void SetEQWindowRect(const RECT *r);
void SetPLWindowRect(const RECT *r);
void MoveRect(RECT *r, int x, int y);
int IsWindowAttached(RECT rc, RECT rc2);
void SnapWindowToWindow(RECT *rcSrc, RECT rcDest);
bool SnapToScreen(RECT *outrc);
void SnapWindowToAllWindows(RECT *outrc, SkinWindow *no_snap);
void main_window_drag(int type, int root_x, int root_y, int stats); // main window titlebar drag with docked windows
void set_window_positions();      // apply config_*wx/wy to the windows
void window_rects(RECT out[3]);   // main, eq, playlist
void resize_with_docking(const RECT before[3]); // after a size change: move docked windows along

/* ---- commands.cpp (Src/Winamp/main_command.cpp) ---- */
void Main_OnCommand(int id);
void set_aot(int apply);
void main_update_sizes();         // after windowshade/double size changes
void main_toggle_windowshade();
void app_quit();
void refresh_skin();
void set_caption();
void on_player_state();
void on_player_info();
void display_timer_tick();        // UPDATE_DISPLAY_TIMER (100 ms)

/* ---- menus.cpp ---- */
enum MenuId
{
	MENU_MAIN,          // "Main" popup (right click / Winamp logo)
	MENU_PLAY,          // Main > Play
	MENU_OPTIONS,       // Main > Options
	MENU_PLAYBACK,      // Main > Playback
	MENU_SKINS,
	MENU_VIS,
	MENU_EQ_PRESETS,
	MENU_PL_SORT, MENU_PL_FILEINFO, MENU_PL_MISC, MENU_PL_ADD, MENU_PL_REMOVE, MENU_PL_REMOVE_MISC,
	MENU_PL_SELECT, MENU_PL_LIST, MENU_PL_CONTEXT,
	MENU_CTX_SONGTITLE, MENU_CTX_TIME, MENU_CTX_PREV, MENU_CTX_PLAY, MENU_CTX_PAUSE, MENU_CTX_STOP,
	MENU_CTX_NEXT, MENU_CTX_EJECT, MENU_CTX_SEEK, MENU_CTX_SHUFFLE, MENU_CTX_REPEAT, MENU_CTX_EQ, MENU_CTX_PE,
	MENU_EQ_ENABLE, MENU_EQ_AUTO,
};
// pops up a menu; at_x/at_y are screen coordinates (<0 = at the mouse pointer)
void popup_menu(MenuId which, SkinWindow *parent, int at_x = -1, int at_y = -1);

/* ---- dialogs.cpp ---- */
std::vector<std::string> dlg_open_files(GtkWindow *parent, const char *title, bool multiple, bool playlists_only = false);
std::string dlg_save_file(GtkWindow *parent, const char *title, const char *default_name, const char *filter_name, const char *pattern);
std::string dlg_open_folder(GtkWindow *parent, const char *title);
std::string dlg_input(GtkWindow *parent, const char *title, const char *label, const std::string &def);
void dlg_jump_to_file();
void dlg_jump_to_time();
void dlg_file_info(const std::string &filename);
void dlg_about();
void dlg_preferences(int page = 0);
void dlg_skin_browser();
void dlg_eq_presets(int mode);  // IDM_EQ_LOADPRE, IDM_EQ_SAVEPRE, ...
void dlg_message(const char *title, const char *text);
GtkWindow *dlg_parent();

/* ---- keyboard.cpp (the accelerator tables of Winamp.rc) ---- */
bool handle_key(SkinWindow *w, GdkEventKey *e);
