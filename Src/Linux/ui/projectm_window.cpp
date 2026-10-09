/*
** Winamp for Linux - native projectM visualization.
**
** projectM is deliberately hosted in a GtkGLArea rather than in the Cairo
** skin bitmap. It needs a real OpenGL context and all projectM calls stay on
** the GTK/render thread. PCM arrives through core/projectm_audio.cpp.
*/
#include "projectm_window.h"

#ifdef WINAMP_HAVE_PROJECTM

#include "../core/projectm_audio.h"
#include "ui.h"

#include <projectM-4/projectM.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;

GtkWidget *window = nullptr;
GtkWidget *area = nullptr;
projectm_handle instance = nullptr;
guint render_source = 0;
std::vector<std::string> presets;
std::vector<std::string> texture_paths;
size_t preset_index = 0;
std::string pending_preset;

bool has_preset_extension(const fs::path &path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == ".milk" || extension == ".milk2" || extension == ".prjm";
}

void add_unique(std::vector<std::string> &paths, const fs::path &path)
{
	std::error_code ec;
	if (!fs::is_directory(path, ec)) return;
	const std::string value = path.lexically_normal().string();
	if (std::find(paths.begin(), paths.end(), value) == paths.end())
		paths.push_back(value);
}

void add_preset_directory(const fs::path &directory)
{
	std::error_code ec;
	if (!fs::is_directory(directory, ec)) return;
	add_unique(texture_paths, directory);
	add_unique(texture_paths, directory / "textures");

	fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec);
	fs::recursive_directory_iterator end;
	for (; it != end && !ec; it.increment(ec))
	{
		if (!it->is_regular_file(ec) || !has_preset_extension(it->path())) continue;
		presets.push_back(it->path().lexically_normal().string());
		add_unique(texture_paths, it->path().parent_path());
	}
}

void add_env_directories()
{
	const char *env = g_getenv("WINAMP_PROJECTM_PRESETS");
	if (!env || !*env) return;
	std::string value(env);
	size_t start = 0;
	while (start <= value.size())
	{
		size_t end = value.find(':', start);
		add_preset_directory(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if (end == std::string::npos) break;
		start = end + 1;
	}
}

void discover_presets()
{
	presets.clear();
	texture_paths.clear();
	add_env_directories();
	add_preset_directory("/usr/share/projectM/presets");
	add_preset_directory("/usr/local/share/projectM/presets");
	add_preset_directory("/usr/share/projectm/presets");
	add_preset_directory(fs::path(g_get_user_data_dir()) / "projectM" / "presets");
	add_preset_directory(fs::path(g_get_user_data_dir()) / "projectm" / "presets");
	std::sort(presets.begin(), presets.end());
	presets.erase(std::unique(presets.begin(), presets.end()), presets.end());
}

void set_texture_paths()
{
	if (!instance || texture_paths.empty()) return;
	std::vector<const char *> paths;
	paths.reserve(texture_paths.size());
	for (const std::string &path : texture_paths)
		paths.push_back(path.c_str());
	projectm_set_texture_search_paths(instance, paths.data(), paths.size());
}

void request_preset(size_t index)
{
	if (presets.empty()) return;
	preset_index = index % presets.size();
	pending_preset = presets[preset_index];
	if (area) gtk_gl_area_queue_render(GTK_GL_AREA(area));
}

void request_relative_preset(int delta)
{
	if (presets.empty()) return;
	int64_t next = static_cast<int64_t>(preset_index) + delta;
	const int64_t count = static_cast<int64_t>(presets.size());
	next %= count;
	if (next < 0) next += count;
	request_preset(static_cast<size_t>(next));
}

void on_realize(GtkGLArea *gl_area, gpointer)
{
	gtk_gl_area_make_current(gl_area);
	if (gtk_gl_area_get_error(gl_area)) return;

	instance = projectm_create();
	if (!instance)
	{
		g_warning("projectM could not create an OpenGL instance");
		return;
	}
	projectm_set_fps(instance, 60);
	projectm_set_aspect_correction(instance, true);
	set_texture_paths();

	int width = gtk_widget_get_allocated_width(GTK_WIDGET(gl_area));
	int height = gtk_widget_get_allocated_height(GTK_WIDGET(gl_area));
	if (width > 0 && height > 0)
		projectm_set_window_size(instance, static_cast<size_t>(width), static_cast<size_t>(height));
	if (!presets.empty()) request_preset(preset_index);
	else projectm_load_preset_file(instance, "idle://", false);
}

void on_unrealize(GtkGLArea *gl_area, gpointer)
{
	gtk_gl_area_make_current(gl_area);
	if (instance)
	{
		projectm_destroy(instance);
		instance = nullptr;
	}
}

void on_resize(GtkGLArea *gl_area, int width, int height, gpointer)
{
	if (instance && width > 0 && height > 0)
	{
		gtk_gl_area_make_current(gl_area);
		if (!gtk_gl_area_get_error(gl_area))
			projectm_set_window_size(instance, static_cast<size_t>(width), static_cast<size_t>(height));
	}
}

gboolean on_render(GtkGLArea *gl_area, GdkGLContext *, gpointer)
{
	gtk_gl_area_make_current(gl_area);
	if (gtk_gl_area_get_error(gl_area)) return FALSE;
	if (!instance) return TRUE;

	if (!pending_preset.empty())
	{
		const std::string path = std::move(pending_preset);
		projectm_load_preset_file(instance, path.c_str(), true);
	}

	projectm_audio::Block block;
	while (projectm_audio::pop(block))
	{
		if (block.frames == 0 || block.samples.empty()) continue;
		projectm_pcm_add_float(instance, block.samples.data(), block.frames,
		                       block.channels == 1 ? PROJECTM_MONO : PROJECTM_STEREO);
	}

	projectm_opengl_render_frame(instance);
	return TRUE;
}

gboolean render_tick(gpointer)
{
	if (!window || !area)
	{
		render_source = 0;
		return G_SOURCE_REMOVE;
	}
	if (gtk_widget_get_visible(window))
		gtk_gl_area_queue_render(GTK_GL_AREA(area));
	return G_SOURCE_CONTINUE;
}

gboolean on_key(GtkWidget *, GdkEventKey *event, gpointer)
{
	switch (event->keyval)
	{
	case GDK_KEY_Escape:
		if (window) gtk_widget_hide(window);
		return TRUE;
	case GDK_KEY_Right:
		request_relative_preset(1);
		return TRUE;
	case GDK_KEY_Left:
		request_relative_preset(-1);
		return TRUE;
	default:
		return FALSE;
	}
}

gboolean on_delete(GtkWidget *, GdkEvent *, gpointer)
{
	if (window) gtk_widget_hide(window);
	return TRUE;
}

void on_destroy(GtkWidget *, gpointer)
{
	if (render_source)
	{
		g_source_remove(render_source);
		render_source = 0;
	}
	window = nullptr;
	area = nullptr;
}

void ensure_window()
{
	if (window)
	{
		gtk_window_present(GTK_WINDOW(window));
		return;
	}

	discover_presets();
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "projectM Visualizer");
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
	if (g_main_wnd)
		gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(g_main_wnd->Widget()));

	area = gtk_gl_area_new();
	gtk_gl_area_set_required_version(GTK_GL_AREA(area), 3, 3);
	gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(area), TRUE);
	gtk_gl_area_set_auto_render(GTK_GL_AREA(area), FALSE);
	gtk_widget_set_can_focus(area, TRUE);
	gtk_container_add(GTK_CONTAINER(window), area);

	g_signal_connect(area, "realize", G_CALLBACK(on_realize), nullptr);
	g_signal_connect(area, "unrealize", G_CALLBACK(on_unrealize), nullptr);
	g_signal_connect(area, "resize", G_CALLBACK(on_resize), nullptr);
	g_signal_connect(area, "render", G_CALLBACK(on_render), nullptr);
	g_signal_connect(area, "key-press-event", G_CALLBACK(on_key), nullptr);
	g_signal_connect(window, "delete-event", G_CALLBACK(on_delete), nullptr);
	g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), nullptr);
	gtk_widget_add_events(area, GDK_KEY_PRESS_MASK);
	gtk_widget_show_all(window);
	gtk_widget_grab_focus(area);
	render_source = g_timeout_add(16, render_tick, nullptr);
}
}

bool projectm_window_is_visible()
{
	return window && gtk_widget_get_visible(window);
}

void projectm_window_toggle()
{
	if (!window) ensure_window();
	if (!window) return;
	if (projectm_window_is_visible())
		gtk_widget_hide(window);
	else
	{
		gtk_widget_show_all(window);
		gtk_window_present(GTK_WINDOW(window));
		if (area) gtk_widget_grab_focus(area);
	}
}

void projectm_window_next_preset()
{
	if (!window) ensure_window();
	request_relative_preset(1);
	if (window) gtk_window_present(GTK_WINDOW(window));
}

void projectm_window_close()
{
	if (window) gtk_widget_destroy(window);
	window = nullptr;
	area = nullptr;
	instance = nullptr;
	projectm_audio::clear();
}

#else

bool projectm_window_is_visible() { return false; }
void projectm_window_toggle() {}
void projectm_window_next_preset() {}
void projectm_window_close() {}

#endif
