#include "mpris.h"
#include "ui.h"
#include "../core/player.h"
#include "../core/playlist.h"
#include "../core/plugins.h"

#include <gio/gio.h>
#include <string.h>

static const char *introspection_xml =
	"<node>"
	"  <interface name='org.mpris.MediaPlayer2'>"
	"    <method name='Raise'/>"
	"    <method name='Quit'/>"
	"    <property name='CanQuit' type='b' access='read'/>"
	"    <property name='CanRaise' type='b' access='read'/>"
	"    <property name='HasTrackList' type='b' access='read'/>"
	"    <property name='Identity' type='s' access='read'/>"
	"    <property name='DesktopEntry' type='s' access='read'/>"
	"    <property name='SupportedUriSchemes' type='as' access='read'/>"
	"    <property name='SupportedMimeTypes' type='as' access='read'/>"
	"  </interface>"
	"  <interface name='org.mpris.MediaPlayer2.Player'>"
	"    <method name='Next'/>"
	"    <method name='Previous'/>"
	"    <method name='Pause'/>"
	"    <method name='PlayPause'/>"
	"    <method name='Stop'/>"
	"    <method name='Play'/>"
	"    <method name='Seek'><arg direction='in' name='Offset' type='x'/></method>"
	"    <method name='SetPosition'><arg direction='in' name='TrackId' type='o'/><arg direction='in' name='Position' type='x'/></method>"
	"    <method name='OpenUri'><arg direction='in' name='Uri' type='s'/></method>"
	"    <signal name='Seeked'><arg name='Position' type='x'/></signal>"
	"    <property name='PlaybackStatus' type='s' access='read'/>"
	"    <property name='LoopStatus' type='s' access='readwrite'/>"
	"    <property name='Rate' type='d' access='readwrite'/>"
	"    <property name='Shuffle' type='b' access='readwrite'/>"
	"    <property name='Metadata' type='a{sv}' access='read'/>"
	"    <property name='Volume' type='d' access='readwrite'/>"
	"    <property name='Position' type='x' access='read'/>"
	"    <property name='MinimumRate' type='d' access='read'/>"
	"    <property name='MaximumRate' type='d' access='read'/>"
	"    <property name='CanGoNext' type='b' access='read'/>"
	"    <property name='CanGoPrevious' type='b' access='read'/>"
	"    <property name='CanPlay' type='b' access='read'/>"
	"    <property name='CanPause' type='b' access='read'/>"
	"    <property name='CanSeek' type='b' access='read'/>"
	"    <property name='CanControl' type='b' access='read'/>"
	"  </interface>"
	"</node>";

static GDBusNodeInfo *node_info;
static GDBusConnection *connection;
static guint owner_id, root_reg, player_reg;

static const char *playback_status()
{
	return !playing ? "Stopped" : paused ? "Paused" : "Playing";
}

static std::string track_path()
{
	return "/com/nullsoft/winamp/track/" + std::to_string(PlayList_getPosition());
}

static GVariant *metadata()
{
	GVariantBuilder b;
	g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
	int pos = PlayList_getPosition();
	if (PlayList_getlength())
	{
		std::string fn = playing ? FileName : PlayList_getfilename(pos);
		g_variant_builder_add(&b, "{sv}", "mpris:trackid", g_variant_new_object_path(track_path().c_str()));
		int len = playing ? in_getlength() : PlayList_getsonglength(pos);
		if (len > 0) g_variant_builder_add(&b, "{sv}", "mpris:length", g_variant_new_int64((gint64)len * 1000000));
		std::string v;
		if (in_get_extended_fileinfo(fn, "title", v) && !v.empty())
			g_variant_builder_add(&b, "{sv}", "xesam:title", g_variant_new_string(v.c_str()));
		else
			g_variant_builder_add(&b, "{sv}", "xesam:title", g_variant_new_string(PlayList_gettitle(pos).c_str()));
		if (in_get_extended_fileinfo(fn, "artist", v) && !v.empty())
		{
			const char *artists[] = {v.c_str(), nullptr};
			g_variant_builder_add(&b, "{sv}", "xesam:artist", g_variant_new_strv(artists, 1));
		}
		if (in_get_extended_fileinfo(fn, "album", v) && !v.empty())
			g_variant_builder_add(&b, "{sv}", "xesam:album", g_variant_new_string(v.c_str()));
		gchar *uri = g_filename_to_uri(fn.c_str(), nullptr, nullptr);
		if (uri)
		{
			g_variant_builder_add(&b, "{sv}", "xesam:url", g_variant_new_string(uri));
			g_free(uri);
		}
	}
	return g_variant_builder_end(&b);
}

static GVariant *get_property(GDBusConnection *, const gchar *, const gchar *, const gchar *iface, const gchar *prop, GError **, gpointer)
{
	if (!strcmp(iface, "org.mpris.MediaPlayer2"))
	{
		if (!strcmp(prop, "CanQuit") || !strcmp(prop, "CanRaise")) return g_variant_new_boolean(TRUE);
		if (!strcmp(prop, "HasTrackList")) return g_variant_new_boolean(FALSE);
		if (!strcmp(prop, "Identity")) return g_variant_new_string("Winamp");
		if (!strcmp(prop, "DesktopEntry")) return g_variant_new_string("winamp");
		if (!strcmp(prop, "SupportedUriSchemes"))
		{
			const char *s[] = {"file", nullptr};
			return g_variant_new_strv(s, 1);
		}
		if (!strcmp(prop, "SupportedMimeTypes"))
		{
			const char *s[] = {"audio/mpeg", "audio/ogg", "audio/flac", "audio/x-wav", "audio/x-mod", nullptr};
			return g_variant_new_strv(s, 5);
		}
		return nullptr;
	}
	if (!strcmp(prop, "PlaybackStatus")) return g_variant_new_string(playback_status());
	if (!strcmp(prop, "LoopStatus")) return g_variant_new_string(config_repeat ? "Playlist" : "None");
	if (!strcmp(prop, "Rate") || !strcmp(prop, "MinimumRate") || !strcmp(prop, "MaximumRate")) return g_variant_new_double(1.0);
	if (!strcmp(prop, "Shuffle")) return g_variant_new_boolean(config_shuffle);
	if (!strcmp(prop, "Metadata")) return metadata();
	if (!strcmp(prop, "Volume")) return g_variant_new_double(config_volume / 255.0);
	if (!strcmp(prop, "Position")) return g_variant_new_int64(playing ? (gint64)in_getouttime() * 1000 : 0);
	if (!strcmp(prop, "CanGoNext") || !strcmp(prop, "CanGoPrevious") || !strcmp(prop, "CanPlay") || !strcmp(prop, "CanPause"))
		return g_variant_new_boolean(PlayList_getlength() > 0);
	if (!strcmp(prop, "CanSeek")) return g_variant_new_boolean(in_seekable());
	if (!strcmp(prop, "CanControl")) return g_variant_new_boolean(TRUE);
	return nullptr;
}

static gboolean set_property(GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *prop, GVariant *value, GError **, gpointer)
{
	if (!strcmp(prop, "Volume"))
	{
		double v = g_variant_get_double(value);
		config_volume = (int)(v < 0 ? 0 : v > 1 ? 255 : v * 255);
		in_setvol(config_volume);
		draw_volumebar(config_volume, 0);
	}
	else if (!strcmp(prop, "Shuffle"))
	{
		if ((bool)g_variant_get_boolean(value) != (bool)config_shuffle) Main_OnCommand(WINAMP_FILE_SHUFFLE);
	}
	else if (!strcmp(prop, "LoopStatus"))
	{
		bool loop = strcmp(g_variant_get_string(value, nullptr), "None") != 0;
		if (loop != (bool)config_repeat) Main_OnCommand(WINAMP_FILE_REPEAT);
	}
	mpris_update();
	return TRUE;
}

static void method_call(GDBusConnection *, const gchar *, const gchar *, const gchar *iface, const gchar *method,
                        GVariant *params, GDBusMethodInvocation *inv, gpointer)
{
	if (!strcmp(iface, "org.mpris.MediaPlayer2"))
	{
		if (!strcmp(method, "Raise")) g_main_wnd->Raise();
		else if (!strcmp(method, "Quit")) app_quit();
	}
	else if (!strcmp(method, "Next")) Main_OnCommand(WINAMP_BUTTON5);
	else if (!strcmp(method, "Previous")) Main_OnCommand(WINAMP_BUTTON1);
	else if (!strcmp(method, "Pause")) { if (playing && !paused) PausePlaying(); }
	else if (!strcmp(method, "PlayPause"))
	{
		if (playing) PausePlaying();
		else Main_OnCommand(WINAMP_BUTTON2);
	}
	else if (!strcmp(method, "Stop")) Main_OnCommand(WINAMP_BUTTON4);
	else if (!strcmp(method, "Play"))
	{
		if (paused) PausePlaying();
		else if (!playing) Main_OnCommand(WINAMP_BUTTON2);
	}
	else if (!strcmp(method, "Seek"))
	{
		gint64 off = 0;
		g_variant_get(params, "(x)", &off);
		player_seek_relative((int)(off / 1000));
	}
	else if (!strcmp(method, "SetPosition"))
	{
		const gchar *id = nullptr;
		gint64 pos = 0;
		g_variant_get(params, "(&ox)", &id, &pos);
		if (id && track_path() == id) in_seek((int)(pos / 1000));
	}
	else if (!strcmp(method, "OpenUri"))
	{
		const gchar *uri = nullptr;
		g_variant_get(params, "(&s)", &uri);
		if (uri)
		{
			StopPlaying(0);
			PlayList_clear();
			PlayList_add(wa::file_uri_to_path(uri));
			PlayList_setposition(0);
			StartPlaying();
			plEditRefresh();
		}
	}
	g_dbus_method_invocation_return_value(inv, nullptr);
}

static const GDBusInterfaceVTable vtable = {method_call, get_property, set_property, {nullptr}};

static void on_bus(GDBusConnection *c, const gchar *, gpointer)
{
	connection = c;
	root_reg = g_dbus_connection_register_object(c, "/org/mpris/MediaPlayer2", node_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);
	player_reg = g_dbus_connection_register_object(c, "/org/mpris/MediaPlayer2", node_info->interfaces[1], &vtable, nullptr, nullptr, nullptr);
}

void mpris_init()
{
	node_info = g_dbus_node_info_new_for_xml(introspection_xml, nullptr);
	if (!node_info) return;
	owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, "org.mpris.MediaPlayer2.winamp", G_BUS_NAME_OWNER_FLAGS_NONE,
	                          on_bus, nullptr, nullptr, nullptr, nullptr);
}

void mpris_shutdown()
{
	if (connection)
	{
		if (root_reg) g_dbus_connection_unregister_object(connection, root_reg);
		if (player_reg) g_dbus_connection_unregister_object(connection, player_reg);
	}
	if (owner_id) g_bus_unown_name(owner_id);
	if (node_info) g_dbus_node_info_unref(node_info);
	connection = nullptr;
	owner_id = root_reg = player_reg = 0;
	node_info = nullptr;
}

void mpris_update()
{
	if (!connection || !player_reg) return;
	GVariantBuilder changed;
	g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
	const char *props[] = {"PlaybackStatus", "LoopStatus", "Shuffle", "Metadata", "Volume", "CanSeek", "CanGoNext", "CanGoPrevious", "CanPlay", "CanPause"};
	for (const char *p : props)
		g_variant_builder_add(&changed, "{sv}", p, get_property(nullptr, nullptr, nullptr, "org.mpris.MediaPlayer2.Player", p, nullptr, nullptr));
	g_dbus_connection_emit_signal(connection, nullptr, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
	                              g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed, nullptr), nullptr);
}
