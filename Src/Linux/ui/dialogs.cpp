/*
** Winamp for Linux - dialogs.
**
** On Windows these are native dialog resources (Winamp.rc); here they are
** native GTK dialogs with the same purpose and wording: file/folder/URL
** open, jump to file/time, file info, EQ presets, skin browser, preferences
** and about.
*/
#include "ui.h"
#include "../core/eq.h"
#include "../core/player.h"
#include "../core/playlist.h"
#include "../core/plugins.h"
#include "../core/titleformat.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

GtkWindow *dlg_parent()
{
	return g_main_wnd ? GTK_WINDOW(g_main_wnd->Widget()) : nullptr;
}

void dlg_message(const char *title, const char *text)
{
	GtkWidget *d = gtk_message_dialog_new(dlg_parent(), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", text);
	gtk_window_set_title(GTK_WINDOW(d), title);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

/* ---------------- file choosers ---------------- */

static void add_filter(GtkFileChooser *fc, const std::string &name, const std::vector<std::string> &patterns)
{
	GtkFileFilter *f = gtk_file_filter_new();
	gtk_file_filter_set_name(f, name.c_str());
	for (auto &p : patterns)
	{
		// case-insensitive "*.mp3"
		std::string ci = "*.";
		for (char c : p)
		{
			if (g_ascii_isalpha(c))
			{
				ci += '[';
				ci += (char)g_ascii_tolower(c);
				ci += (char)g_ascii_toupper(c);
				ci += ']';
			}
			else
				ci += c;
		}
		gtk_file_filter_add_pattern(f, ci.c_str());
	}
	gtk_file_chooser_add_filter(fc, f);
}

static void set_cwd(GtkFileChooser *fc)
{
	if (!config_cwd.empty() && wa::dir_exists(config_cwd))
		gtk_file_chooser_set_current_folder(fc, config_cwd.c_str());
}

static void remember_cwd(GtkFileChooser *fc)
{
	gchar *d = gtk_file_chooser_get_current_folder(fc);
	if (d)
	{
		config_cwd = d;
		g_free(d);
	}
}

std::vector<std::string> dlg_open_files(GtkWindow *parent, const char *title, bool multiple, bool playlists_only)
{
	std::vector<std::string> out;
	GtkWidget *d = gtk_file_chooser_dialog_new(title, parent, GTK_FILE_CHOOSER_ACTION_OPEN,
	                                           "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr);
	GtkFileChooser *fc = GTK_FILE_CHOOSER(d);
	gtk_file_chooser_set_select_multiple(fc, multiple);
	gtk_file_chooser_set_local_only(fc, TRUE);
	set_cwd(fc);
	std::vector<std::string> playlists = {"m3u", "m3u8", "pls"};
	if (playlists_only)
		add_filter(fc, "Playlist files (*.m3u;*.m3u8;*.pls)", playlists);
	else
	{
		std::vector<std::string> all = in_extensions();
		all.insert(all.end(), playlists.begin(), playlists.end());
		add_filter(fc, "All supported types", all);
		add_filter(fc, "Playlist files (*.m3u;*.m3u8;*.pls)", playlists);
		for (auto &p : g_inputs)
			for (auto &f : p.filters)
			{
				std::vector<std::string> exts;
				std::string pat = f.first;
				size_t s = 0;
				while (s < pat.size())
				{
					size_t e = pat.find(';', s);
					std::string one = pat.substr(s, e == std::string::npos ? std::string::npos : e - s);
					if (one.compare(0, 2, "*.") == 0) exts.push_back(one.substr(2));
					if (e == std::string::npos) break;
					s = e + 1;
				}
				add_filter(fc, f.second, exts);
			}
	}
	GtkFileFilter *any = gtk_file_filter_new();
	gtk_file_filter_set_name(any, "All files (*.*)");
	gtk_file_filter_add_pattern(any, "*");
	gtk_file_chooser_add_filter(fc, any);

	if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT)
	{
		GSList *files = gtk_file_chooser_get_filenames(fc);
		for (GSList *l = files; l; l = l->next)
		{
			out.push_back((const char *)l->data);
			g_free(l->data);
		}
		g_slist_free(files);
		remember_cwd(fc);
	}
	gtk_widget_destroy(d);
	std::sort(out.begin(), out.end());
	return out;
}

std::string dlg_save_file(GtkWindow *parent, const char *title, const char *default_name, const char *filter_name, const char *pattern)
{
	std::string out;
	GtkWidget *d = gtk_file_chooser_dialog_new(title, parent, GTK_FILE_CHOOSER_ACTION_SAVE,
	                                           "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, nullptr);
	GtkFileChooser *fc = GTK_FILE_CHOOSER(d);
	gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
	set_cwd(fc);
	gtk_file_chooser_set_current_name(fc, default_name);
	if (filter_name && pattern)
	{
		GtkFileFilter *f = gtk_file_filter_new();
		gtk_file_filter_set_name(f, filter_name);
		gchar **pats = g_strsplit(pattern, ";", -1);
		for (gchar **p = pats; *p; p++) gtk_file_filter_add_pattern(f, *p);
		g_strfreev(pats);
		gtk_file_chooser_add_filter(fc, f);
	}
	if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *fn = gtk_file_chooser_get_filename(fc);
		if (fn)
		{
			out = fn;
			g_free(fn);
		}
		remember_cwd(fc);
	}
	gtk_widget_destroy(d);
	return out;
}

std::string dlg_open_folder(GtkWindow *parent, const char *title)
{
	std::string out;
	GtkWidget *d = gtk_file_chooser_dialog_new(title, parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                           "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr);
	set_cwd(GTK_FILE_CHOOSER(d));
	if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		if (fn)
		{
			out = fn;
			config_cwd = fn;
			g_free(fn);
		}
	}
	gtk_widget_destroy(d);
	return out;
}

std::string dlg_input(GtkWindow *parent, const char *title, const char *label, const std::string &def)
{
	GtkWidget *d = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_set_border_width(GTK_CONTAINER(box), 10);
	gtk_box_set_spacing(GTK_BOX(box), 6);
	GtkWidget *l = gtk_label_new(label);
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	GtkWidget *e = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e), def.c_str());
	gtk_entry_set_activates_default(GTK_ENTRY(e), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(e), 50);
	gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), e, FALSE, FALSE, 0);
	gtk_widget_show_all(d);
	std::string out;
	if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) out = gtk_entry_get_text(GTK_ENTRY(e));
	gtk_widget_destroy(d);
	return out;
}

/* ---------------- jump to file / time ---------------- */

namespace
{
	struct JumpDlg
	{
		GtkWidget *dialog, *entry, *view;
		GtkListStore *store;
	};
}

static void jump_refill(JumpDlg *j)
{
	gtk_list_store_clear(j->store);
	std::string q = g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(j->entry)), -1);
	gchar **words = g_strsplit(q.c_str(), " ", -1);
	int first = -1;
	for (int i = 0; i < PlayList_getlength(); i++)
	{
		std::string t = PlayList_getitem_pl(i);
		gchar *hay = g_utf8_casefold((t + " " + PlayList_getfilename(i)).c_str(), -1);
		bool ok = true;
		for (gchar **w = words; *w && ok; w++)
			if (**w && !strstr(hay, *w)) ok = false;
		g_free(hay);
		if (!ok) continue;
		GtkTreeIter it;
		gtk_list_store_append(j->store, &it);
		gtk_list_store_set(j->store, &it, 0, t.c_str(), 1, i, -1);
		if (first < 0) first = i;
	}
	g_strfreev(words);
	GtkTreePath *p = gtk_tree_path_new_first();
	if (first >= 0) gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(j->view)), p);
	gtk_tree_path_free(p);
}

static int jump_selected(JumpDlg *j)
{
	GtkTreeModel *m;
	GtkTreeIter it;
	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(j->view)), &m, &it)) return -1;
	int idx = -1;
	gtk_tree_model_get(m, &it, 1, &idx, -1);
	return idx;
}

void dlg_jump_to_file()
{
	JumpDlg j;
	j.dialog = gtk_dialog_new_with_buttons("Jump to file", dlg_parent(), GTK_DIALOG_MODAL,
	                                       "_Close", GTK_RESPONSE_CLOSE, "_Jump to file", GTK_RESPONSE_OK, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(j.dialog), GTK_RESPONSE_OK);
	gtk_window_set_default_size(GTK_WINDOW(j.dialog), 460, 380);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(j.dialog));
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);
	gtk_box_set_spacing(GTK_BOX(box), 6);
	GtkWidget *l = gtk_label_new("Search for:");
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	j.entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(j.entry), TRUE);
	j.store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	j.view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(j.store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(j.view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(j.view), gtk_tree_view_column_new_with_attributes("", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	GtkWidget *sw = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_container_add(GTK_CONTAINER(sw), j.view);
	gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), j.entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
	g_signal_connect(j.entry, "changed", G_CALLBACK(+[](GtkEditable *, gpointer p) { jump_refill((JumpDlg *)p); }), &j);
	g_signal_connect(j.view, "row-activated", G_CALLBACK(+[](GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, gpointer p) {
		gtk_dialog_response(GTK_DIALOG(((JumpDlg *)p)->dialog), GTK_RESPONSE_OK);
	}), &j);
	jump_refill(&j);
	gtk_widget_show_all(j.dialog);
	if (gtk_dialog_run(GTK_DIALOG(j.dialog)) == GTK_RESPONSE_OK)
	{
		int idx = jump_selected(&j);
		if (idx >= 0)
		{
			PlayIndex(idx);
			plEditSelect(idx);
		}
	}
	gtk_widget_destroy(j.dialog);
}

void dlg_jump_to_time()
{
	if (!playing || !in_seekable()) return;
	int len = in_getlength();
	int now = in_getouttime() / 1000;
	char def[32], label[128];
	snprintf(def, sizeof(def), "%d:%02d", now / 60, now % 60);
	snprintf(label, sizeof(label), "Jump to (mm:ss) - track length: %d:%02d", len / 60, len % 60);
	std::string t = dlg_input(dlg_parent(), "Jump to time", label, def);
	if (t.empty()) return;
	int m = 0, s = 0;
	if (sscanf(t.c_str(), "%d:%d", &m, &s) == 2)
		in_seek((m * 60 + s) * 1000);
	else if (sscanf(t.c_str(), "%d", &s) == 1)
		in_seek(s * 1000);
}

/* ---------------- file info ---------------- */

void dlg_file_info(const std::string &filename)
{
	GtkWidget *d = gtk_dialog_new_with_buttons("File Info", dlg_parent(), GTK_DIALOG_MODAL, "_Close", GTK_RESPONSE_CLOSE, nullptr);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_set_border_width(GTK_CONTAINER(box), 10);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
	gtk_box_pack_start(GTK_BOX(box), grid, TRUE, TRUE, 0);
	int row = 0;
	auto add = [&](const char *label, const std::string &value) {
		GtkWidget *l = gtk_label_new(label);
		gtk_label_set_xalign(GTK_LABEL(l), 1);
		GtkWidget *e = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(e), value.c_str());
		gtk_editable_set_editable(GTK_EDITABLE(e), FALSE);
		gtk_entry_set_width_chars(GTK_ENTRY(e), 48);
		gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), e, 1, row, 1, 1);
		row++;
	};
	add("File:", filename);
	const char *fields[][2] = {{"Title:", "title"}, {"Artist:", "artist"}, {"Album:", "album"}, {"Album Artist:", "albumartist"},
	                           {"Year:", "year"}, {"Track #:", "track"}, {"Genre:", "genre"}, {"Composer:", "composer"}, {"Comment:", "comment"}};
	for (auto &f : fields)
	{
		std::string v;
		in_get_extended_fileinfo(filename, f[1], v);
		add(f[0], v);
	}
	std::string v;
	std::string tech;
	if (in_get_extended_fileinfo(filename, "family", v)) tech += v;
	if (in_get_extended_fileinfo(filename, "length", v))
	{
		int ms = atoi(v.c_str());
		char buf[64];
		snprintf(buf, sizeof(buf), "%s%d:%02d", tech.empty() ? "" : ", ", ms / 60000, (ms / 1000) % 60);
		tech += buf;
	}
	if (in_get_extended_fileinfo(filename, "bitrate", v)) tech += ", " + v + " kbps";
	if (in_get_extended_fileinfo(filename, "samplerate", v)) tech += ", " + v + " Hz";
	add("Format:", tech);
	InputPlugin *p = in_find(filename);
	add("Decoder:", p ? p->mod->description : "(no input plug-in for this file)");
	gtk_widget_show_all(d);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

/* ---------------- about ---------------- */

void dlg_about()
{
	GtkWidget *d = gtk_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(d), dlg_parent());
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(d), "Winamp");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(d), APP_VERSION_STRING);
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(d), "It really whips the llama's ass.\n\nNative Linux port of the classic Winamp player.");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(d), "Copyright © 1997-2024 Nullsoft, Inc. / Winamp SA");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(d), "Winamp Collaborative License (WCL) Version 1.0.1\nSee LICENSE.md in the source tree.");
	GdkPixbuf *icon = gtk_window_get_icon(dlg_parent());
	if (icon) gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(d), icon);
	std::string plugins;
	for (auto &p : g_inputs) plugins += std::string(p.mod->description) + "\n";
	for (auto &p : g_outputs) plugins += std::string(p.mod->description) + "\n";
	const char *credits[] = {plugins.c_str(), nullptr};
	gtk_about_dialog_add_credit_section(GTK_ABOUT_DIALOG(d), "Plug-ins", credits);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

/* ---------------- EQ presets (Eq.cpp dialogs) ---------------- */

// list dialog; returns the chosen/typed name or "" on cancel
static std::string preset_dialog(const char *title, const std::vector<std::string> &names, bool with_entry, const char *ok_label,
                                 const std::string &initial, bool preview)
{
	GtkWidget *d = gtk_dialog_new_with_buttons(title, dlg_parent(), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, ok_label, GTK_RESPONSE_OK, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
	gtk_window_set_default_size(GTK_WINDOW(d), 320, 360);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);
	gtk_box_set_spacing(GTK_BOX(box), 6);
	GtkWidget *entry = nullptr;
	if (with_entry)
	{
		entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(entry), initial.c_str());
		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
		gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
	}
	GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
	for (auto &n : names)
	{
		GtkTreeIter it;
		gtk_list_store_append(store, &it);
		gtk_list_store_set(store, &it, 0, n.c_str(), -1);
	}
	GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), gtk_tree_view_column_new_with_attributes("", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	GtkWidget *sw = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_container_add(GTK_CONTAINER(sw), view);
	gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
	for (size_t i = 0; i < names.size(); i++)
		if (!g_ascii_strcasecmp(names[i].c_str(), initial.c_str()))
		{
			GtkTreePath *p = gtk_tree_path_new_from_indices((int)i, -1);
			gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), p);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(view), p, nullptr, FALSE, 0, 0);
			gtk_tree_path_free(p);
		}
	struct Ctx { GtkWidget *entry; bool preview; GtkWidget *dialog; } ctx = {entry, preview, d};
	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed", G_CALLBACK(+[](GtkTreeSelection *s, gpointer p) {
		Ctx *c = (Ctx *)p;
		GtkTreeModel *m;
		GtkTreeIter it;
		if (!gtk_tree_selection_get_selected(s, &m, &it)) return;
		gchar *n = nullptr;
		gtk_tree_model_get(m, &it, 0, &n, -1);
		if (c->entry) gtk_entry_set_text(GTK_ENTRY(c->entry), n);
		if (c->preview) g_object_set_data_full(G_OBJECT(c->dialog), "preview", g_strdup(n), g_free);
		g_free(n);
	}), &ctx);
	g_signal_connect(view, "row-activated", G_CALLBACK(+[](GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, gpointer p) {
		gtk_dialog_response(GTK_DIALOG(p), GTK_RESPONSE_OK);
	}), d);
	gtk_widget_show_all(d);
	std::string out;
	if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK)
	{
		if (entry) out = gtk_entry_get_text(GTK_ENTRY(entry));
		else
		{
			GtkTreeModel *m;
			GtkTreeIter it;
			if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), &m, &it))
			{
				gchar *n = nullptr;
				gtk_tree_model_get(m, &it, 0, &n, -1);
				out = n ? n : "";
				g_free(n);
			}
		}
	}
	gtk_widget_destroy(d);
	g_object_unref(store);
	return out;
}

static std::string last_preset = "Default";

void dlg_eq_presets(int mode)
{
	std::string cur_file = wa::path_filename(playing ? FileName : PlayList_getfilename(PlayList_getPosition()));
	switch (mode)
	{
	case IDM_EQ_LOADPRE:
	{
		unsigned char backup[10];
		memcpy(backup, eq_tab, 10);
		int pre = config_preamp;
		std::string n = preset_dialog("Load EQ preset", eq_list_presets(config_eq_path()), false, "_Load", last_preset, true);
		if (!n.empty())
		{
			last_preset = n;
			eq_read_preset(config_eq_path(), n);
		}
		else
		{
			memcpy(eq_tab, backup, 10);
			config_preamp = pre;
			eq_apply_config();
		}
		break;
	}
	case IDM_EQ_LOADMP3:
	{
		std::string n = preset_dialog("Load auto-load preset", eq_list_presets(config_eq_auto_path()), false, "_Load", cur_file, true);
		if (!n.empty()) eq_read_preset(config_eq_auto_path(), n);
		break;
	}
	case IDM_EQ_SAVEPRE:
	{
		std::string n = preset_dialog("Save EQ preset", eq_list_presets(config_eq_path()), true, "_Save", last_preset, false);
		if (!n.empty())
		{
			last_preset = n;
			eq_write_preset(config_eq_path(), n);
		}
		break;
	}
	case IDM_EQ_SAVEMP3:
	{
		std::string n = preset_dialog("Save auto-load preset", eq_list_presets(config_eq_auto_path()), true, "_Save", cur_file, false);
		if (!n.empty()) eq_write_preset(config_eq_auto_path(), n);
		break;
	}
	case IDM_EQ_DELPRE:
	{
		std::string n = preset_dialog("Delete EQ preset", eq_list_presets(config_eq_path()), false, "_Delete", last_preset, false);
		if (!n.empty()) eq_delete_preset(config_eq_path(), n);
		break;
	}
	case IDM_EQ_DELMP3:
	{
		std::string n = preset_dialog("Delete auto-load preset", eq_list_presets(config_eq_auto_path()), false, "_Delete", cur_file, false);
		if (!n.empty()) eq_delete_preset(config_eq_auto_path(), n);
		break;
	}
	case ID_LOAD_EQF:
	{
		auto f = dlg_open_files(dlg_parent(), "Load EQ file", false);
		if (!f.empty()) eq_read_preset(f[0], "Entry1");
		break;
	}
	case ID_SAVE_EQF:
	{
		std::string f = dlg_save_file(dlg_parent(), "Save EQ file", "preset.eqf", "EQ files (*.eqf)", "*.eqf");
		if (!f.empty())
		{
			if (wa::path_extension(f) != "eqf") f += ".eqf";
			remove(f.c_str());
			eq_write_preset(f, "Entry1");
		}
		break;
	}
	}
}

/* ---------------- skin browser ---------------- */

static void fill_skin_list(GtkListStore *store)
{
	gtk_list_store_clear(store);
	GtkTreeIter it;
	gtk_list_store_append(store, &it);
	gtk_list_store_set(store, &it, 0, "<Base Skin>  (Winamp Classic)", 1, "", -1);
	for (auto &s : skin_list())
	{
		gtk_list_store_append(store, &it);
		gtk_list_store_set(store, &it, 0, s.c_str(), 1, s.c_str(), -1);
	}
}

void dlg_skin_browser()
{
	GtkWidget *d = gtk_dialog_new_with_buttons("Skin Browser", dlg_parent(), GTK_DIALOG_MODAL, "_Close", GTK_RESPONSE_CLOSE, nullptr);
	gtk_window_set_default_size(GTK_WINDOW(d), 380, 420);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);
	gtk_box_set_spacing(GTK_BOX(box), 6);
	GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	fill_skin_list(store);
	GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), gtk_tree_view_column_new_with_attributes("", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	GtkWidget *sw = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_container_add(GTK_CONTAINER(sw), view);
	gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);

	GtkWidget *hint = gtk_label_new(nullptr);
	std::string text = "Skins folder: " + config_skin_dirs_user() + "\nClassic skins (.wsz / .zip / folders) are supported.";
	gtk_label_set_text(GTK_LABEL(hint), text.c_str());
	gtk_label_set_xalign(GTK_LABEL(hint), 0);
	gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
	gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *install = gtk_button_new_with_mnemonic("_Install skin...");
	GtkWidget *folder = gtk_button_new_with_mnemonic("Open skins _folder");
	GtkWidget *more = gtk_button_new_with_mnemonic("_Get more skins!");
	gtk_box_pack_start(GTK_BOX(buttons), install, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttons), folder, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(buttons), more, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), buttons, FALSE, FALSE, 0);

	// select the current skin
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GtkTreeIter it;
	for (bool ok = gtk_tree_model_get_iter_first(model, &it); ok; ok = gtk_tree_model_iter_next(model, &it))
	{
		gchar *n = nullptr;
		gtk_tree_model_get(model, &it, 1, &n, -1);
		if (n && config_skin == n) gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), &it);
		g_free(n);
	}

	// selecting a skin switches to it right away, like the Windows skin browser
	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed", G_CALLBACK(+[](GtkTreeSelection *s, gpointer) {
		GtkTreeModel *m;
		GtkTreeIter it;
		if (!gtk_tree_selection_get_selected(s, &m, &it)) return;
		gchar *n = nullptr;
		gtk_tree_model_get(m, &it, 1, &n, -1);
		std::string name = n ? n : "";
		g_free(n);
		if (name != config_skin)
		{
			config_skin = name;
			refresh_skin();
		}
	}), nullptr);
	g_signal_connect(install, "clicked", G_CALLBACK(+[](GtkButton *, gpointer st) {
		GtkWidget *fd = gtk_file_chooser_dialog_new("Install skin", dlg_parent(), GTK_FILE_CHOOSER_ACTION_OPEN,
		                                            "_Cancel", GTK_RESPONSE_CANCEL, "_Install", GTK_RESPONSE_ACCEPT, nullptr);
		GtkFileFilter *f = gtk_file_filter_new();
		gtk_file_filter_set_name(f, "Winamp skins (*.wsz;*.zip)");
		gtk_file_filter_add_pattern(f, "*.[wW][sS][zZ]");
		gtk_file_filter_add_pattern(f, "*.[zZ][iI][pP]");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fd), f);
		if (gtk_dialog_run(GTK_DIALOG(fd)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fd));
			if (fn && skin_install(fn))
			{
				config_skin = wa::path_filename(fn);
				refresh_skin();
				fill_skin_list((GtkListStore *)st);
			}
			g_free(fn);
		}
		gtk_widget_destroy(fd);
	}), store);
	g_signal_connect(folder, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
		std::string dir = config_skin_dirs_user();
		wa::make_dirs(dir);
		gchar *uri = g_filename_to_uri(dir.c_str(), nullptr, nullptr);
		if (uri) g_app_info_launch_default_for_uri(uri, nullptr, nullptr);
		g_free(uri);
	}), nullptr);
	g_signal_connect(more, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
		g_app_info_launch_default_for_uri("https://skins.webamp.org/", nullptr, nullptr);
	}), nullptr);

	gtk_widget_show_all(d);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
	g_object_unref(store);
}

/* ---------------- preferences ---------------- */

static GtkWidget *check(const char *label, int *value, void (*on_change)() = nullptr)
{
	GtkWidget *c = gtk_check_button_new_with_mnemonic(label);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c), *value != 0);
	g_object_set_data(G_OBJECT(c), "cb", (gpointer)on_change);
	g_signal_connect(c, "toggled", G_CALLBACK(+[](GtkToggleButton *b, gpointer v) {
		*(int *)v = gtk_toggle_button_get_active(b) ? 1 : 0;
		void (*cb)() = (void (*)())g_object_get_data(G_OBJECT(b), "cb");
		if (cb) cb();
	}), value);
	return c;
}

static GtkWidget *page(GtkWidget *nb, const char *title)
{
	GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(b), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), b, gtk_label_new(title));
	return b;
}

static GtkWidget *frame(GtkWidget *parent, const char *title)
{
	GtkWidget *f = gtk_frame_new(title);
	GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(b), 8);
	gtk_container_add(GTK_CONTAINER(f), b);
	gtk_box_pack_start(GTK_BOX(parent), f, FALSE, FALSE, 0);
	return b;
}

static void redraw_everything()
{
	draw_init();
	draw_pe_init();
	main_update_sizes();
	draw_main_all();
	draw_eq_all();
	plEditRefresh();
}

void dlg_preferences(int start_page)
{
	GtkWidget *d = gtk_dialog_new_with_buttons("Winamp Preferences", dlg_parent(), GTK_DIALOG_MODAL, "_Close", GTK_RESPONSE_CLOSE, nullptr);
	gtk_window_set_default_size(GTK_WINDOW(d), 520, 460);
	GtkWidget *nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), nb, TRUE, TRUE, 0);

	/* General */
	GtkWidget *p = page(nb, "General");
	GtkWidget *f = frame(p, "Playlist");
	gtk_box_pack_start(GTK_BOX(f), check("Show _numbers in playlist", &config_shownumsinpl, plEditRefresh), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("Read _titles from file metadata", &config_useexttitles), FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(row), gtk_label_new("Title formatting:"), FALSE, FALSE, 0);
	GtkWidget *fmt = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(fmt), config_titlefmt.c_str());
	g_signal_connect(fmt, "changed", G_CALLBACK(+[](GtkEditable *e, gpointer) { config_titlefmt = gtk_entry_get_text(GTK_ENTRY(e)); }), nullptr);
	gtk_box_pack_start(GTK_BOX(row), fmt, TRUE, TRUE, 0);
	GtkWidget *apply = gtk_button_new_with_label("Refresh titles");
	g_signal_connect(apply, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) {
		for (int i = 0; i < PlayList_getlength(); i++) PlayList_refreshtitle(i);
		player_update_title();
	}), nullptr);
	gtk_box_pack_start(GTK_BOX(row), apply, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), row, FALSE, FALSE, 0);
	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(row), gtk_label_new("Playlist font size:"), FALSE, FALSE, 0);
	GtkWidget *fs = gtk_spin_button_new_with_range(5, 40, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(fs), config_pe_fontsize);
	g_signal_connect(fs, "value-changed", G_CALLBACK(+[](GtkSpinButton *s, gpointer) {
		config_pe_fontsize = gtk_spin_button_get_value_as_int(s);
		draw_pe_init();
		plEditRefresh();
	}), nullptr);
	gtk_box_pack_start(GTK_BOX(row), fs, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), row, FALSE, FALSE, 0);

	f = frame(p, "Playback");
	gtk_box_pack_start(GTK_BOX(f), check("_Advance playlist automatically", &config_pladv), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("Show track _number in the main window title display", &config_dotitlenum, draw_songname_title), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("_Scroll the song title in the main window", &config_autoscrollname, draw_songname_title), FALSE, FALSE, 0);

	/* Classic skins */
	p = page(nb, "Classic Skins");
	f = frame(p, "Windows");
	gtk_box_pack_start(GTK_BOX(f), check("_Snap windows to each other and screen edges", &config_snap), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("_Keep windows on screen when snapping", &config_keeponscreen), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("Enable _EasyMove (drag windows from anywhere)", &config_easymove), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("_Dim titlebars of inactive windows", &config_hilite, redraw_everything), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("Use doublesize setting on equalizer as well as main window", &config_eqdsize, redraw_everything), FALSE, FALSE, 0);
	f = frame(p, "Display");
	gtk_box_pack_start(GTK_BOX(f), check("Use skinned _font for main window title display\n(no international title support)", &config_bifont, redraw_everything), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(f), check("Always show clutterbar", &config_ascb_new, redraw_everything), FALSE, FALSE, 0);
	GtkWidget *sb = gtk_button_new_with_mnemonic("Skin _Browser...");
	g_signal_connect(sb, "clicked", G_CALLBACK(+[](GtkButton *, gpointer) { dlg_skin_browser(); }), nullptr);
	gtk_box_pack_start(GTK_BOX(p), sb, FALSE, FALSE, 0);

	/* Equalizer */
	p = page(nb, "Equalizer");
	f = frame(p, "Equalizer");
	gtk_box_pack_start(GTK_BOX(f), check("Use _limiter (prevents clipping)", &config_eq_limiter), FALSE, FALSE, 0);
	GtkWidget *iso = gtk_check_button_new_with_mnemonic("Use _ISO frequency bands (31 Hz - 16 kHz) instead of Winamp's (70 Hz - 16 kHz)");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iso), config_eq_frequencies != EQ_FREQUENCIES_WINAMP);
	g_signal_connect(iso, "toggled", G_CALLBACK(+[](GtkToggleButton *b, gpointer) {
		config_eq_frequencies = gtk_toggle_button_get_active(b) ? EQ_FREQUENCIES_ISO : EQ_FREQUENCIES_WINAMP;
		refresh_skin(); // the EQ bitmap has the band labels
		eq_apply_config();
	}), nullptr);
	gtk_box_pack_start(GTK_BOX(f), iso, FALSE, FALSE, 0);

	/* Plug-ins */
	p = page(nb, "Plug-ins");
	f = frame(p, "Input");
	GtkListStore *in_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	for (auto &ip : g_inputs)
	{
		std::string exts;
		for (auto &e : ip.extensions) exts += (exts.empty() ? "" : " ") + e;
		GtkTreeIter it;
		gtk_list_store_append(in_store, &it);
		gtk_list_store_set(in_store, &it, 0, ip.mod->description, 1, wa::path_filename(ip.path).c_str(), -1);
	}
	GtkWidget *in_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(in_store));
	gtk_tree_view_append_column(GTK_TREE_VIEW(in_view), gtk_tree_view_column_new_with_attributes("Plug-in", gtk_cell_renderer_text_new(), "text", 0, nullptr));
	gtk_tree_view_append_column(GTK_TREE_VIEW(in_view), gtk_tree_view_column_new_with_attributes("File", gtk_cell_renderer_text_new(), "text", 1, nullptr));
	gtk_box_pack_start(GTK_BOX(f), in_view, FALSE, FALSE, 0);
	g_object_unref(in_store);

	f = frame(p, "Output");
	GtkWidget *combo = gtk_combo_box_text_new();
	int active = 0;
	for (size_t i = 0; i < g_outputs.size(); i++)
	{
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), g_outputs[i].file.c_str(), g_outputs[i].mod->description);
		if (out_current() && out_current()->file == g_outputs[i].file) active = (int)i;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	g_signal_connect(combo, "changed", G_CALLBACK(+[](GtkComboBox *c, gpointer) {
		const gchar *id = gtk_combo_box_get_active_id(c);
		if (!id) return;
		bool was = playing;
		int pos = was ? in_getouttime() : 0;
		if (was) StopPlaying(0);
		out_select(id);
		if (was)
		{
			StartPlaying();
			in_seek(pos);
		}
	}), nullptr);
	gtk_box_pack_start(GTK_BOX(f), combo, FALSE, FALSE, 0);
	std::string dirs = "Plug-in folders:";
	for (auto &dd : plugins_search_dirs()) dirs += "\n  " + dd;
	GtkWidget *dl = gtk_label_new(dirs.c_str());
	gtk_label_set_xalign(GTK_LABEL(dl), 0);
	gtk_label_set_selectable(GTK_LABEL(dl), TRUE);
	gtk_box_pack_start(GTK_BOX(p), dl, FALSE, FALSE, 0);

	gtk_widget_show_all(d);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), start_page);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
	config_write();
}
