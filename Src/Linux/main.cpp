/*
** Winamp for Linux - program entry point.
**
** Startup/shutdown follow Src/Winamp/main.cpp and main_init.cpp: read the
** config, load plug-ins and the skin, create the main, equalizer and playlist
** windows, restore the playlist and run the display timers. A second
** "winamp file..." invocation hands its files to the running player (like
** the Windows player does), through GApplication.
*/
#include "ui/ui.h"
#include "ui/projectm_window.h"
#include "ui/mpris.h"
#include "core/eq.h"
#include "core/player.h"
#include "core/playlist.h"
#include "core/plugins.h"
#include "core/vis.h"
#include "sdk/wa_ipc.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

SkinWindow *g_main_wnd, *g_eq_wnd, *g_pl_wnd;

/* ---- the "main window" handle plug-ins talk to (PostMessage/SendMessage) ---- */

struct PostedMessage
{
	unsigned int msg;
	WPARAM wParam;
	LPARAM lParam;
};

static LRESULT host_send(HWND, unsigned int msg, WPARAM wParam, LPARAM lParam);

static gboolean dispatch_posted(gpointer data)
{
	PostedMessage *m = (PostedMessage *)data;
	if (m->msg == WM_WA_MPEG_EOF)
		player_on_eof();
	else
		host_send(nullptr, m->msg, m->wParam, m->lParam);
	delete m;
	return G_SOURCE_REMOVE;
}

static int host_post(HWND, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	g_idle_add(dispatch_posted, new PostedMessage{msg, wParam, lParam});
	return 1;
}

static LRESULT host_send(HWND, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	static std::string strbuf;
	if (msg != WM_WA_IPC) return 0;
	switch (lParam)
	{
	case IPC_GETVERSION: return 0x5092;
	case IPC_ISPLAYING: return playing ? (paused ? 3 : 1) : 0;
	case IPC_GETOUTPUTTIME:
		if (wParam == 0) return playing ? in_getouttime() : -1;
		if (wParam == 1) return playing ? in_getlength() : -1;
		if (wParam == 2) return playing && in_mod ? in_mod->GetLength() : -1;
		return -1;
	case IPC_GETLISTLENGTH: return PlayList_getlength();
	case IPC_GETLISTPOS: return PlayList_getPosition();
	case IPC_GETPLAYLISTFILE:
		strbuf = PlayList_getfilename((int)wParam);
		return (LRESULT)strbuf.c_str();
	case IPC_GETPLAYLISTTITLE:
		strbuf = PlayList_gettitle((int)wParam);
		return (LRESULT)strbuf.c_str();
	case IPC_GET_SHUFFLE: return config_shuffle;
	case IPC_GET_REPEAT: return config_repeat;
	case IPC_GETINIFILE:
		strbuf = config_ini_path();
		return (LRESULT)strbuf.c_str();
	case IPC_GETINIDIRECTORY:
		strbuf = wa::config_dir();
		return (LRESULT)strbuf.c_str();
	case IPC_GETINFO:
		if (wParam == 0) return g_srate;
		if (wParam == 1) return g_brate;
		if (wParam == 2) return g_nch;
		return 0;
	}
	return 0;
}

static winamp_host_window host_window = {host_post, host_send};

/* ---- timers (Main_OnTimer) ---- */

static gboolean display_timer(gpointer)
{
	display_timer_tick();
	return G_SOURCE_CONTINUE;
}

static gboolean scroll_timer(gpointer)
{
	if (!config_minimized && !config_windowshade && config_mw_open && (config_autoscrollname & 1))
		ui_doscrolling();
	return G_SOURCE_CONTINUE;
}

static gboolean vis_timer(gpointer)
{
	vis_timer_tick();
	return G_SOURCE_CONTINUE;
}

/* ---- startup / shutdown ---- */

static GtkApplication *app;
static bool started = false;

static GdkPixbuf *load_icon()
{
	GBytes *b = g_resources_lookup_data("/winamp/skin/winamp.ico", G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
	if (!b) return nullptr;
	GdkPixbufLoader *l = gdk_pixbuf_loader_new();
	gsize size = 0;
	const guchar *d = (const guchar *)g_bytes_get_data(b, &size);
	GdkPixbuf *pb = nullptr;
	if (gdk_pixbuf_loader_write(l, d, size, nullptr) && gdk_pixbuf_loader_close(l, nullptr))
		pb = gdk_pixbuf_loader_get_pixbuf(l);
	else
		gdk_pixbuf_loader_close(l, nullptr);
	if (pb) g_object_ref(pb);
	g_object_unref(l);
	g_bytes_unref(b);
	return pb;
}

static void on_minimize(GtkWidget *, GdkEventWindowState *e, gpointer)
{
	if (e->changed_mask & GDK_WINDOW_STATE_ICONIFIED)
		config_minimized = (e->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
}

static void startup()
{
	config_read();
	eq_create_default_presets();
	sa_setmode(config_sa);

	plugins_load(&host_window);
	if (g_inputs.empty())
		fprintf(stderr, "winamp: no input plug-ins found (looked in the Plugins folders next to the binary, "
		                "in ~/.local/share/winamp/Plugins and in the install location). Set WINAMP_PLUGIN_DIR to override.\n");

	skin_load();

	g_main_wnd = new SkinWindow("Winamp", "winamp-main", main_handler);
	g_eq_wnd = new SkinWindow("Winamp Equalizer", "winamp-equalizer", eq_handler);
	g_pl_wnd = new SkinWindow("Winamp Playlist Editor", "winamp-playlist", pe_handler);
	for (SkinWindow *w : {g_main_wnd, g_eq_wnd, g_pl_wnd})
		gtk_application_add_window(app, GTK_WINDOW(w->Widget()));
	for (SkinWindow *w : {g_eq_wnd, g_pl_wnd})
	{
		gtk_window_set_transient_for(GTK_WINDOW(w->Widget()), GTK_WINDOW(g_main_wnd->Widget()));
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(w->Widget()), TRUE);
	}
	g_signal_connect(g_main_wnd->Widget(), "window-state-event", G_CALLBACK(on_minimize), nullptr);
	if (GdkPixbuf *icon = load_icon())
	{
		gtk_window_set_default_icon(icon);
		g_object_unref(icon);
	}

	draw_init();
	draw_eq_init();
	draw_pe_init();
	main_update_sizes();
	set_window_positions();

	PlayList_setchangecallback([] {
		plEditRefresh();
		if (FileName.empty() || !playing)
		{
			player_update_title();
			if (!do_volbar_active && !do_panbar_active && !do_posbar_active) draw_songname_title();
		}
		else
		{
			std::string t = PlayList_gettitle(PlayList_getPosition());
			if (t != FileTitle && PlayList_getfilename(PlayList_getPosition()) == FileName)
			{
				FileTitle = t;
				if (!do_volbar_active && !do_panbar_active && !do_posbar_active) draw_songname_title();
				set_caption();
			}
		}
	});
	player_set_callbacks({on_player_state, on_player_info});

	PlayList_load(config_m3u_path());
	PlayList_setposition(config_pilp);
	player_update_title();
	eq_apply_config();

	draw_main_all();
	draw_eq_all();
	set_aot(0);
	set_caption();

	if (config_mw_open) g_main_wnd->Show();
	if (config_eq_open) g_eq_wnd->Show();
	if (config_pe_open) g_pl_wnd->Show();
	if (!config_mw_open && !config_eq_open && !config_pe_open)
	{
		config_mw_open = 1;
		g_main_wnd->Show();
	}
	set_window_positions();

	g_timeout_add(100, display_timer, nullptr);
	g_timeout_add(200, scroll_timer, nullptr);
	g_timeout_add(16, vis_timer, nullptr);
	mpris_init();
	started = true;
}

static void shutdown_app(GApplication *, gpointer)
{
	if (!started) return;
	config_pilp = PlayList_getPosition();
	projectm_window_close();
	player_shutdown();
	PlayList_save(config_m3u_path());
	config_write();
	mpris_shutdown();
	PlayList_shutdown();
	plugins_unload();
}

/* ---- command line (also for files sent to a running instance) ---- */

static int on_command_line(GApplication *, GApplicationCommandLine *cmdline, gpointer)
{
	gchar **argv = nullptr;
	int argc = 0;
	argv = g_application_command_line_get_arguments(cmdline, &argc);
	bool enqueue = false;
	std::vector<std::string> files;
	std::vector<int> commands;
	for (int i = 1; i < argc; i++)
	{
		const char *a = argv[i];
		if (!strcmp(a, "--enqueue") || !strcasecmp(a, "/ADD") || !strcmp(a, "-e")) enqueue = true;
		else if (!strcmp(a, "--play")) commands.push_back(WINAMP_BUTTON2);
		else if (!strcmp(a, "--pause") || !strcmp(a, "--play-pause")) commands.push_back(WINAMP_BUTTON3);
		else if (!strcmp(a, "--stop")) commands.push_back(WINAMP_BUTTON4);
		else if (!strcmp(a, "--next")) commands.push_back(WINAMP_BUTTON5);
		else if (!strcmp(a, "--prev") || !strcmp(a, "--previous")) commands.push_back(WINAMP_BUTTON1);
		else if (!strcmp(a, "--help") || !strcmp(a, "-h"))
		{
			g_application_command_line_print(cmdline,
				"Usage: winamp [options] [files, folders, playlists or URLs]\n\n"
				"  -e, --enqueue   add to the playlist instead of replacing it (/ADD works too)\n"
				"  --play, --pause, --stop, --next, --prev\n"
				"                  control a running Winamp\n\n"
				"Files: ~/.config/winamp (settings, playlist, EQ presets)\n"
				"Skins: %s\n", config_skin_dirs_user().c_str());
			g_strfreev(argv);
			return 0;
		}
		else
		{
			GFile *f = g_application_command_line_create_file_for_arg(cmdline, a);
			gchar *path = g_file_get_path(f);
			files.push_back(path ? path : a);
			g_free(path);
			g_object_unref(f);
		}
	}
	g_strfreev(argv);

	if (!started) startup();
	if (!files.empty())
	{
		if (!enqueue)
		{
			StopPlaying(0);
			PlayList_clear();
			pledit_disp_offs = 0;
		}
		int first = PlayList_getlength();
		for (auto &f : files) PlayList_add(f);
		if (!enqueue)
		{
			PlayList_setposition(first);
			StartPlaying();
		}
		plEditRefresh();
	}
	for (int c : commands) Main_OnCommand(c);
	if (g_application_command_line_get_is_remote(cmdline) && files.empty() && commands.empty() && g_main_wnd)
		g_main_wnd->Raise();
	return 0;
}

static void on_activate(GApplication *, gpointer)
{
	if (!started) startup();
}

int main(int argc, char **argv)
{
	// Winamp positions its windows itself (docking, snapping). Wayland doesn't
	// allow that, so prefer X11 (XWayland on Wayland desktops).
	if (!getenv("WINAMP_ALLOW_WAYLAND")) gdk_set_allowed_backends("x11,*");
	g_set_application_name("Winamp");
	g_set_prgname("winamp");

	app = gtk_application_new("com.nullsoft.winamp", G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect(app, "command-line", G_CALLBACK(on_command_line), nullptr);
	g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
	g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_app), nullptr);
	int r = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return r;
}
