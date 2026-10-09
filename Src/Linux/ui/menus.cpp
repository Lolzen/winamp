/*
** Winamp for Linux - the popup menus.
**
** The structure, labels and shortcut hints are the WINAMP_MAIN menu resource
** of Src/Winamp/Winamp.rc; items send the same command ids to
** Main_OnCommand(). GTK menus stand in for the Win32 popup menus.
*/
#include "ui.h"
#include "menu_ids.h"
#include "projectm_window.h"
#include "../core/player.h"
#include "../core/playlist.h"

#include <string.h>
#include <functional>
#include <vector>

namespace
{
	struct MI
	{
		enum Kind { ITEM, SEP, POPUP, DYNAMIC } kind;
		std::string label;
		int id;
		std::vector<MI> sub;
		std::function<void(GtkWidget *menu)> fill; // DYNAMIC
		bool disabled;
	};

	MI item(const char *label, int id, bool disabled = false) { return {MI::ITEM, label, id, {}, nullptr, disabled}; }
	MI sep() { return {MI::SEP, "", 0, {}, nullptr, false}; }
	MI popup(const char *label, std::vector<MI> sub) { return {MI::POPUP, label, 0, sub, nullptr, false}; }
	MI dynamic(const char *label, std::function<void(GtkWidget *)> fill) { return {MI::DYNAMIC, label, 0, {}, fill, false}; }
}

// -1: not a check item, else the check state
static int check_state(int id)
{
	switch (id)
	{
	case WINAMP_OPTIONS_PLEDIT: return config_pe_open;
	case WINAMP_OPTIONS_EQ: return config_eq_open;
	case WINAMP_MAIN_WINDOW: return config_mw_open;
	case WINAMP_OPTIONS_ELAPSED: return !config_timeleftmode;
	case WINAMP_OPTIONS_REMAINING: return config_timeleftmode;
	case WINAMP_OPTIONS_AOT: return config_aot;
	case WINAMP_OPTIONS_DSIZE: return config_dsize;
	case WINAMP_OPTIONS_EASYMOVE: return config_easymove;
	case WINAMP_FILE_REPEAT: return config_repeat;
	case WINAMP_FILE_SHUFFLE: return config_shuffle;
	case WINAMP_BUTTON4_CTRL: return g_stopaftercur;
	case WINAMP_TOGGLE_AUTOSCROLL: return config_autoscrollname & 1;
	case WINAMP_FILE_MANUALPLADVANCE: return !config_pladv;
	case EQ_ENABLE: return config_use_eq;
	case EQ_AUTO: return config_autoload_eq;
	case WAL_VIS_ANALYZER: return config_sa == 1;
	case WAL_VIS_SCOPE: return config_sa == 2;
	case WAL_VIS_OFF: return config_sa == 0 && !projectm_window_is_visible();
	case WAL_VIS_PROJECTM: return projectm_window_is_visible();
	case WAL_VIS_NORMAL: return (config_safire & 3) == 0;
	case WAL_VIS_FIRE: return (config_safire & 3) == 1;
	case WAL_VIS_LINE: return (config_safire & 3) == 2;
	case WAL_VIS_PEAKS: return config_sa_peaks;
	case WAL_VIS_THICK: return !(config_safire & 32);
	case WAL_VIS_THIN: return (config_safire & 32) != 0;
	case WAL_VIS_DOTS: return ((config_safire >> 2) & 3) == 0;
	case WAL_VIS_LINES: return ((config_safire >> 2) & 3) == 1;
	case WAL_VIS_SOLID: return ((config_safire >> 2) & 3) == 2;
	case WAL_VIS_REFRESH1: return config_saref == 1;
	case WAL_VIS_REFRESH2: return config_saref == 2;
	case WAL_VIS_REFRESH4: return config_saref == 4;
	case WAL_VIS_REFRESH8: return config_saref == 8;
	case WAL_SKIN_CLASSIC: return config_skin.empty();
	}
	if (id >= WAL_VIS_FALLOFF0 && id <= WAL_VIS_FALLOFF0 + 4) return config_safalloff == id - WAL_VIS_FALLOFF0;
	if (id >= WAL_VIS_PFALLOFF0 && id <= WAL_VIS_PFALLOFF0 + 4) return config_sa_peak_falloff == id - WAL_VIS_PFALLOFF0;
	return -1;
}

static void on_activate(GtkMenuItem *mi, gpointer data)
{
	if (GTK_IS_CHECK_MENU_ITEM(mi) && g_object_get_data(G_OBJECT(mi), "wa-updating")) return;
	int id = GPOINTER_TO_INT(data);
	// run after the menu has been torn down, like WM_COMMAND after TrackPopupMenu
	g_idle_add([](gpointer d) -> gboolean {
		Main_OnCommand(GPOINTER_TO_INT(d));
		return G_SOURCE_REMOVE;
	}, GINT_TO_POINTER(id));
}

// "&Open file...\tL" -> mnemonic label "_Open file..." and accelerator hint "L"
static GtkWidget *make_item(const std::string &text, int id, bool disabled)
{
	std::string label = text, accel;
	size_t tab = text.find('\t');
	if (tab != std::string::npos)
	{
		label = text.substr(0, tab);
		accel = text.substr(tab + 1);
	}
	while (!label.empty() && label.back() == ' ') label.pop_back();
	std::string mn;
	for (size_t i = 0; i < label.size(); i++)
	{
		if (label[i] == '_') mn += "__";
		else if (label[i] == '&' && i + 1 < label.size() && label[i + 1] == '&') { mn += '&'; i++; }
		else if (label[i] == '&') mn += '_';
		else mn += label[i];
	}

	int checked = id ? check_state(id) : -1;
	GtkWidget *mi = checked >= 0 ? gtk_check_menu_item_new() : gtk_menu_item_new();
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
	GtkWidget *l = gtk_label_new_with_mnemonic(mn.c_str());
	gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
	gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
	if (!accel.empty())
	{
		GtkWidget *a = gtk_label_new(accel.c_str());
		gtk_style_context_add_class(gtk_widget_get_style_context(a), "accelerator");
		gtk_box_pack_end(GTK_BOX(box), a, FALSE, FALSE, 0);
	}
	gtk_container_add(GTK_CONTAINER(mi), box);
	if (checked >= 0)
	{
		g_object_set_data(G_OBJECT(mi), "wa-updating", GINT_TO_POINTER(1));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), checked);
		g_object_set_data(G_OBJECT(mi), "wa-updating", nullptr);
	}
	if (disabled) gtk_widget_set_sensitive(mi, FALSE);
	if (id) g_signal_connect(mi, "activate", G_CALLBACK(on_activate), GINT_TO_POINTER(id));
	return mi;
}

static GtkWidget *build(const std::vector<MI> &items);

static void add_items(GtkWidget *menu, const std::vector<MI> &items)
{
	for (const MI &m : items)
	{
		GtkWidget *w;
		switch (m.kind)
		{
		case MI::SEP:
			w = gtk_separator_menu_item_new();
			break;
		case MI::POPUP:
			w = make_item(m.label, 0, m.disabled);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), build(m.sub));
			break;
		case MI::DYNAMIC:
		{
			w = make_item(m.label, 0, m.disabled);
			GtkWidget *sub = gtk_menu_new();
			m.fill(sub);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), sub);
			break;
		}
		default:
			w = make_item(m.label, m.id, m.disabled);
			break;
		}
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), w);
	}
}

static GtkWidget *build(const std::vector<MI> &items)
{
	GtkWidget *menu = gtk_menu_new();
	add_items(menu, items);
	return menu;
}

/* ---- dynamic parts ---- */

static std::vector<std::string> g_skin_menu_names;

static void fill_skins(GtkWidget *menu)
{
	add_items(menu, {item("S&kin Browser...\tAlt+S", WINAMP_SELSKIN), sep(), item("Winamp Classic", WAL_SKIN_CLASSIC)});
	g_skin_menu_names = skin_list();
	for (size_t i = 0; i < g_skin_menu_names.size() && i < 500; i++)
	{
		std::string label = g_skin_menu_names[i];
		std::string ext = wa::path_extension(label);
		if (ext == "wsz" || ext == "zip") label = label.substr(0, label.size() - ext.size() - 1);
		GtkWidget *ci = gtk_check_menu_item_new_with_label(label.c_str());
		g_object_set_data(G_OBJECT(ci), "wa-updating", GINT_TO_POINTER(1));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ci), g_skin_menu_names[i] == config_skin);
		g_object_set_data(G_OBJECT(ci), "wa-updating", nullptr);
		g_signal_connect(ci, "activate", G_CALLBACK(on_activate), GINT_TO_POINTER(WAL_SKIN_FIRST + (int)i));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), ci);
	}
}

std::string skin_menu_name(int id)
{
	int i = id - WAL_SKIN_FIRST;
	if (i >= 0 && i < (int)g_skin_menu_names.size()) return g_skin_menu_names[i];
	return "";
}

static std::vector<MI> vis_menu()
{
	return {
		item("&Analyzer", WAL_VIS_ANALYZER),
		item("&Oscilloscope", WAL_VIS_SCOPE),
		item("O&ff", WAL_VIS_OFF),
		sep(),
		item("projectM (MilkDrop replacement)", WAL_VIS_PROJECTM),
		item("Next projectM preset", WAL_VIS_PROJECTM_NEXT),
		sep(),
		popup("Analyzer &options", {
			item("&Normal style", WAL_VIS_NORMAL),
			item("&Fire style", WAL_VIS_FIRE),
			item("&Line style", WAL_VIS_LINE),
			sep(),
			item("&Peaks", WAL_VIS_PEAKS),
			sep(),
			item("&Thick bands", WAL_VIS_THICK),
			item("T&hin bands", WAL_VIS_THIN),
		}),
		popup("Oscilloscope o&ptions", {
			item("&Dot scope", WAL_VIS_DOTS),
			item("&Line scope", WAL_VIS_LINES),
			item("&Solid scope", WAL_VIS_SOLID),
		}),
		popup("&Refresh rate", {
			item("&Full (~60fps)", WAL_VIS_REFRESH1),
			item("&Half (~30fps)", WAL_VIS_REFRESH2),
			item("&Quarter (~15fps)", WAL_VIS_REFRESH4),
			item("&Eighth (~8fps)", WAL_VIS_REFRESH8),
		}),
		popup("Analyzer &falloff", {
			item("Slo&west", WAL_VIS_FALLOFF0), item("&Slow", WAL_VIS_FALLOFF0 + 1), item("&Moderate", WAL_VIS_FALLOFF0 + 2),
			item("&Fast", WAL_VIS_FALLOFF0 + 3), item("F&astest", WAL_VIS_FALLOFF0 + 4),
		}),
		popup("P&eaks falloff", {
			item("Slo&west", WAL_VIS_PFALLOFF0), item("&Slow", WAL_VIS_PFALLOFF0 + 1), item("&Moderate", WAL_VIS_PFALLOFF0 + 2),
			item("&Fast", WAL_VIS_PFALLOFF0 + 3), item("F&astest", WAL_VIS_PFALLOFF0 + 4),
		}),
	};
}

static std::vector<MI> play_menu()
{
	return {
		item("&File...\tL", WINAMP_FILE_PLAY),
		item("&URL...\tCtrl+L", WINAMP_FILE_LOC),
		item("&Folder...\tShift+L", WINAMP_FILE_DIR),
	};
}

static std::vector<MI> options_menu()
{
	return {
		item("&Preferences...\tCtrl+P", WINAMP_OPTIONS_PREFS),
		dynamic("Skins", fill_skins),
		sep(),
		item("Time &elapsed\tCtrl+T toggles", WINAMP_OPTIONS_ELAPSED),
		item("Time re&maining\tCtrl+T toggles", WINAMP_OPTIONS_REMAINING),
		sep(),
		item("&Always On Top\tCtrl+A", WINAMP_OPTIONS_AOT),
		item("&Double Size\tCtrl+D", WINAMP_OPTIONS_DSIZE),
		item("&EasyMove\tCtrl+E", WINAMP_OPTIONS_EASYMOVE),
		sep(),
		item("&Repeat\tR", WINAMP_FILE_REPEAT),
		item("&Shuffle\tS", WINAMP_FILE_SHUFFLE),
	};
}

static std::vector<MI> playback_menu()
{
	return {
		item("P&revious\tZ", WINAMP_BUTTON1),
		item("&Play\tX", WINAMP_BUTTON2),
		item("P&ause\tC", WINAMP_BUTTON3),
		item("&Stop\tV", WINAMP_BUTTON4),
		item("&Next\tB", WINAMP_BUTTON5),
		sep(),
		item("Stop w/ fa&deout\tShift+V", WINAMP_BUTTON4_SHIFT),
		item("Stop after &current\tCtrl+V", WINAMP_BUTTON4_CTRL),
		item("&Back 5 seconds\tLeft", WINAMP_BUTTON1_SHIFT),
		item("&Fwd 5 seconds\tRight", WINAMP_BUTTON5_SHIFT),
		item("&Start of list\tCtrl+Z", WINAMP_BUTTON1_CTRL),
		item("&End of list\tCtrl+B", WINAMP_BUTTON5_CTRL),
		item("10 t&racks back\tNum. 1", WINAMP_JUMP10BACK),
		item("10 &tracks fwd\tNum. 3", WINAMP_JUMP10FWD),
		sep(),
		item("&Jump to time\tCtrl+J", WINAMP_JUMP),
		item("Ju&mp to file\tJ", WINAMP_JUMPFILE),
	};
}

static std::vector<MI> main_menu()
{
	return {
		item("About &Winamp...\tCtrl+F1", WINAMP_HELP_ABOUT),
		sep(),
		popup("&Play", play_menu()),
		item("View &file info...\tAlt+3", WINAMP_EDIT_ID3),
		sep(),
		item("&Main Window\tAlt+W", WINAMP_MAIN_WINDOW),
		item("Playlist &Editor\tAlt+E", WINAMP_OPTIONS_PLEDIT),
		item("E&qualizer\tAlt+G", WINAMP_OPTIONS_EQ),
		sep(),
		popup("&Options", options_menu()),
		popup("Play&back", playback_menu()),
		popup("&Visualization", vis_menu()),
		dynamic("&Skins", fill_skins),
		sep(),
		item("E&xit\tAlt+F4", WINAMP_FILE_QUIT),
	};
}

static std::vector<MI> menu_for(MenuId which)
{
	switch (which)
	{
	case MENU_MAIN: return main_menu();
	case MENU_PLAY: return play_menu();
	case MENU_OPTIONS: return options_menu();
	case MENU_PLAYBACK: return playback_menu();
	case MENU_VIS: return vis_menu();
	case MENU_SKINS: return {dynamic("&Skins", fill_skins)};
	case MENU_EQ_PRESETS:
		return {
			popup("Load", {item("&Preset...", IDM_EQ_LOADPRE), item("&Auto-load preset...", IDM_EQ_LOADMP3), item("&Default", IDM_EQ_LOADDEFAULT), sep(), item("From &EQF...", ID_LOAD_EQF)}),
			popup("Save", {item("&Preset...", IDM_EQ_SAVEPRE), item("&Auto-load preset...", IDM_EQ_SAVEMP3), item("&Default", IDM_EQ_SAVEDEFAULT), sep(), item("To &EQF...", ID_SAVE_EQF)}),
			popup("Delete", {item("&Preset...", IDM_EQ_DELPRE), item("&Auto-load preset...", IDM_EQ_DELMP3)}),
		};
	case MENU_PL_FILEINFO: return {item("F&ile info...\tAlt+3", ID_PE_ID3), item("Playlist &entry...\tCtrl+E", ID_PE_ENTRY)};
	case MENU_PL_SORT:
		return {item("Sort list by &title\tCtrl+Shift+1", ID_PE_S_TITLE), item("Sort list by &filename\tCtrl+Shift+2", ID_PE_S_FILENAME),
		        item("Sort list by &path and filename\tCtrl+Shift+3", ID_PE_S_PATH), sep(), item("R&everse list\tCtrl+R", ID_PE_S_REV),
		        item("&Randomize list\tCtrl+Shift+R", ID_PE_S_RANDOM)};
	case MENU_PL_MISC:
		return {item("&Generate HTML playlist\tCtrl+Alt+G", ID_PE_PRINT), sep(), item("&Rebuild titles on selection\tCtrl+Alt+E", ID_PE_EXTINFO)};
	case MENU_PL_ADD:
		return {item("Add &file(s)\tL", IDC_PLAYLIST_ADDMP3), item("Add f&older\tShift+L", IDC_PLAYLIST_ADDDIR), item("Add &URL\tCtrl+L", IDC_PLAYLIST_ADDLOC)};
	case MENU_PL_REMOVE:
		return {item("&Remove selected\tDelete", IDC_PLAYLIST_REMOVEMP3), item("Crop &selected\tCtrl+Delete", IDC_PLAYLIST_CROP),
		        item("&Clear playlist\tCtrl+Shift+Delete", ID_PE_CLEAR),
		        popup("Remove...", {item("Remove &missing files from playlist\tAlt+Delete", ID_PE_NONEXIST), item("&Physically remove selected file(s)", ID_PE_DELETEFROMDISK)})};
	case MENU_PL_REMOVE_MISC:
		return {item("Remove &missing files from playlist\tAlt+Delete", ID_PE_NONEXIST), item("&Physically remove selected file(s)", ID_PE_DELETEFROMDISK)};
	case MENU_PL_SELECT:
		return {item("Select &all\tCtrl+A", ID_PE_SELECTALL), item("Select &none", ID_PE_NONE), item("&Invert selection\tCtrl+I", IDC_SELECTINV)};
	case MENU_PL_LIST:
		return {item("&Open playlist...\tCtrl+O", ID_PE_OPEN), item("&Save playlist...\tCtrl+S", ID_PE_SAVEAS), item("&New playlist (clear)\tCtrl+N", ID_PE_CLEAR)};
	case MENU_PL_CONTEXT:
		return {item("&Play item(s)\tEnter", IDC_PLAYLIST_PLAY), sep(), item("&Remove item(s)\tDelete", IDC_PLAYLIST_REMOVEMP3),
		        item("&Crop files\tCtrl+Delete", IDC_PLAYLIST_CROP), sep(), item("&View file info...\tAlt+3", ID_PE_ID3),
		        item("Playlist &entry\tCtrl+E", ID_PE_ENTRY), sep(), item("Explore item(s) &folder\tCtrl+F", ID_PE_FFOD)};
	case MENU_CTX_SONGTITLE:
		return {item("View &file info...\tAlt+3 or Dblclick", WINAMP_EDIT_ID3), item("&Jump to file...\tJ", WINAMP_JUMPFILE),
		        item("Jump to &time...\tCtrl+J", WINAMP_JUMP), item("&Autoscroll songname", WINAMP_TOGGLE_AUTOSCROLL)};
	case MENU_CTX_TIME:
		return {item("Time &elapsed\tCtrl+T toggles", WINAMP_OPTIONS_ELAPSED), item("Time re&maining\tCtrl+T toggles", WINAMP_OPTIONS_REMAINING)};
	case MENU_CTX_PREV:
		return {item("&Previous\tClick", WINAMP_BUTTON1), item("&Start of list\tCtrl+Click", WINAMP_BUTTON1_CTRL), item("&Rewind 5 seconds\tShift+Click", WINAMP_BUTTON1_SHIFT)};
	case MENU_CTX_PLAY:
		return {item("&Play/restart\tClick", WINAMP_BUTTON2), item("Open &URL...\tCtrl+Click", WINAMP_BUTTON2_CTRL), item("Open &file...\tShift+Click", WINAMP_BUTTON2_SHIFT)};
	case MENU_CTX_PAUSE: return {item("&Pause/Unpause\tClick", WINAMP_BUTTON3)};
	case MENU_CTX_STOP:
		return {item("&Stop\tClick", WINAMP_BUTTON4), item("Stop w/&fadeout\tShift+Click", WINAMP_BUTTON4_SHIFT), item("Stop after &current\tCtrl+Click", WINAMP_BUTTON4_CTRL)};
	case MENU_CTX_NEXT:
		return {item("&Next\tClick", WINAMP_BUTTON5), item("&Fastforward 5 seconds\tShift+Click", WINAMP_BUTTON5_SHIFT), item("&End of list\tCtrl+Click", WINAMP_BUTTON5_CTRL)};
	case MENU_CTX_EJECT:
		return {item("Open &file...\tClick", WINAMP_FILE_PLAY), item("Open f&older...\tShift+Click", WINAMP_FILE_DIR), item("Open &URL...\tCtrl+Click", WINAMP_FILE_LOC)};
	case MENU_CTX_SEEK:
		return {item("&Jump to time\tCtrl+J", WINAMP_JUMP), item("&Rewind 5 seconds\tLeft Arrow", WINAMP_REW5S), item("&Forward 5 seconds\tRight Arrow", WINAMP_FFWD5S)};
	case MENU_CTX_SHUFFLE: return {item("&Shuffle\tS", WINAMP_FILE_SHUFFLE)};
	case MENU_CTX_REPEAT: return {item("&Repeat\tR", WINAMP_FILE_REPEAT), item("&Manual Playlist Advance\tShift+R", WINAMP_FILE_MANUALPLADVANCE)};
	case MENU_CTX_EQ: return {item("Graphical E&qualizer\tAlt+G", WINAMP_OPTIONS_EQ)};
	case MENU_CTX_PE: return {item("Playlist &Editor\tAlt+E", WINAMP_OPTIONS_PLEDIT)};
	case MENU_EQ_ENABLE: return {item("&EQ enabled\tN", EQ_ENABLE)};
	case MENU_EQ_AUTO: return {item("&EQ autoloading enabled\tA", EQ_AUTO)};
	}
	return {};
}

void popup_menu(MenuId which, SkinWindow *parent, int at_x, int at_y)
{
	GtkWidget *menu = build(menu_for(which));
	gtk_widget_show_all(menu);
	g_signal_connect(menu, "deactivate", G_CALLBACK(+[](GtkMenuShell *m, gpointer) {
		g_idle_add([](gpointer w) -> gboolean { gtk_widget_destroy(GTK_WIDGET(w)); return G_SOURCE_REMOVE; }, m);
	}), nullptr);
	GtkWidget *anchor = parent ? parent->Widget() : (g_main_wnd ? g_main_wnd->Widget() : nullptr);
	if (anchor) gtk_menu_attach_to_widget(GTK_MENU(menu), anchor, nullptr);
	if (at_x >= 0 && anchor && gtk_widget_get_window(anchor))
	{
		int wx = 0, wy = 0;
		gdk_window_get_origin(gtk_widget_get_window(anchor), &wx, &wy);
		GdkRectangle r = {at_x - wx, at_y - wy, 1, 1};
		gtk_menu_popup_at_rect(GTK_MENU(menu), gtk_widget_get_window(anchor), &r, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, nullptr);
	}
	else
		gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
}
