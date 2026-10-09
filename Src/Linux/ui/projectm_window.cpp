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

#include <GL/gl.h>

#if defined(WINAMP_PROJECTM_API4)
#include <projectM-4/projectM.h>
#elif defined(WINAMP_PROJECTM_API3)
#include <libprojectM/projectM.hpp>
#else
#error "WINAMP_HAVE_PROJECTM requires a projectM API selection"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;

#if defined(WINAMP_PROJECTM_API4)
using projectm_instance_t = projectm_handle;
#else
using projectm_instance_t = projectM *;
#endif

GtkWidget *window = nullptr;
GtkWidget *area = nullptr;
projectm_instance_t instance = nullptr;
guint render_source = 0;
std::vector<std::string> presets;
std::vector<std::string> texture_paths;
size_t preset_index = 0;
std::string pending_preset;
bool preset_load_warning_logged = false;

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
	add_unique(texture_paths, directory.parent_path() / "textures");

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
	if (presets.empty())
		g_warning("projectM: no preset files found; the built-in idle preset will be used");
	else
		g_message("projectM: discovered %zu preset files and %zu texture search paths",
		          presets.size(), texture_paths.size());
}

std::string find_data_directory()
{
	const fs::path candidates[] = {
		"/usr/share/projectM",
		"/usr/local/share/projectM",
		"/usr/share/projectm",
		"/usr/local/share/projectm",
		fs::path(g_get_user_data_dir()) / "projectM",
		fs::path(g_get_user_data_dir()) / "projectm"};

	std::error_code ec;
	for (const fs::path &candidate : candidates)
		if (fs::is_directory(candidate / "presets", ec) ||
		    fs::is_directory(candidate / "textures", ec))
			return candidate.lexically_normal().string();

	return {};
}

void log_opengl_context()
{
	const GLubyte *version = glGetString(GL_VERSION);
	const GLubyte *renderer = glGetString(GL_RENDERER);
	const GLubyte *shading_language = glGetString(GL_SHADING_LANGUAGE_VERSION);
	g_message("projectM: OpenGL version=%s renderer=%s GLSL=%s",
	          version ? reinterpret_cast<const char *>(version) : "unknown",
	          renderer ? reinterpret_cast<const char *>(renderer) : "unknown",
	          shading_language ? reinterpret_cast<const char *>(shading_language) : "unknown");
}

#if defined(WINAMP_PROJECTM_API3)
std::string first_existing_file(std::initializer_list<const char *> candidates)
{
	std::error_code ec;
	for (const char *candidate : candidates)
		if (fs::is_regular_file(candidate, ec)) return candidate;
	return {};
}
#endif

#if defined(WINAMP_PROJECTM_API4)
void set_texture_paths()
{
	if (!instance || texture_paths.empty()) return;
	std::vector<const char *> paths;
	paths.reserve(texture_paths.size());
	for (const std::string &path : texture_paths)
		paths.push_back(path.c_str());
	projectm_set_texture_search_paths(instance, paths.data(), paths.size());
}
#endif

#if defined(WINAMP_PROJECTM_API3)
void add_projectm3_presets()
{
	if (!instance) return;
	const RatingList ratings(TOTAL_RATING_TYPES, 3);
	for (const std::string &path : presets)
		instance->addPresetURL(path, fs::path(path).filename().string(), ratings);
}
#endif

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
	if (GError *error = gtk_gl_area_get_error(gl_area))
	{
		g_warning("projectM: GtkGLArea could not create an OpenGL context: %s", error->message);
		return;
	}
	log_opengl_context();

	int width = gtk_widget_get_allocated_width(GTK_WIDGET(gl_area));
	int height = gtk_widget_get_allocated_height(GTK_WIDGET(gl_area));
	if (width <= 0) width = 800;
	if (height <= 0) height = 600;

#if defined(WINAMP_PROJECTM_API4)
	instance = projectm_create();
	if (!instance)
	{
		g_warning("projectM could not create an OpenGL instance");
		return;
	}
	projectm_set_fps(instance, 60);
	projectm_set_aspect_correction(instance, true);
	set_texture_paths();
	projectm_set_window_size(instance, static_cast<size_t>(width), static_cast<size_t>(height));
	if (!presets.empty()) request_preset(preset_index);
	else projectm_load_preset_file(instance, "idle://", false);
#else
	projectM::Settings settings;
	settings.fps = 60;
	settings.windowWidth = width;
	settings.windowHeight = height;
	settings.aspectCorrection = true;
	settings.presetDuration = 30;
	settings.smoothPresetDuration = 3;
	settings.presetURL = texture_paths.empty() ? std::string() : texture_paths.front();
	settings.datadir = find_data_directory();
	settings.titleFontURL = first_existing_file({
		"/usr/share/projectM/fonts/Vera.ttf",
		"/usr/share/projectm/fonts/Vera.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"});
	settings.menuFontURL = first_existing_file({
		"/usr/share/projectM/fonts/VeraMono.ttf",
		"/usr/share/projectm/fonts/VeraMono.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"});
	try
	{
		instance = new projectM(settings);
	}
	catch (const std::exception &error)
	{
		g_warning("projectM 3 could not create an OpenGL instance: %s", error.what());
		return;
	}
	catch (...)
	{
		g_warning("projectM 3 could not create an OpenGL instance");
		return;
	}
	if (!instance)
	{
		g_warning("projectM 3 could not create an OpenGL instance");
		return;
	}
	instance->clearPlaylist();
	add_projectm3_presets();
	g_message("projectM 3: playlist contains %u preset files", instance->getPlaylistSize());
	preset_load_warning_logged = false;
	if (!presets.empty()) request_preset(preset_index);
#endif
}

void on_unrealize(GtkGLArea *gl_area, gpointer)
{
	gtk_gl_area_make_current(gl_area);
	if (instance)
	{
#if defined(WINAMP_PROJECTM_API4)
		projectm_destroy(instance);
#else
		delete instance;
#endif
		instance = nullptr;
	}
	preset_load_warning_logged = false;
}

void on_resize(GtkGLArea *gl_area, int width, int height, gpointer)
{
	if (instance && width > 0 && height > 0)
	{
		gtk_gl_area_make_current(gl_area);
		if (!gtk_gl_area_get_error(gl_area))
		{
#if defined(WINAMP_PROJECTM_API4)
			projectm_set_window_size(instance, static_cast<size_t>(width), static_cast<size_t>(height));
#else
			instance->projectM_resetGL(width, height);
#endif
		}
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
#if defined(WINAMP_PROJECTM_API4)
		projectm_load_preset_file(instance, path.c_str(), true);
#else
		(void)path;
		instance->selectPreset(static_cast<unsigned int>(preset_index), true);
		if (instance->getErrorLoadingCurrentPreset() && !preset_load_warning_logged)
		{
			g_warning("projectM 3 could not load preset %zu: %s", preset_index,
			          presets[preset_index].c_str());
			preset_load_warning_logged = true;
		}
#endif
	}

	projectm_audio::Block block;
	while (projectm_audio::pop(block))
	{
		if (block.frames == 0 || block.samples.empty()) continue;
#if defined(WINAMP_PROJECTM_API4)
		projectm_pcm_add_float(instance, block.samples.data(), block.frames,
		                       block.channels == 1 ? PROJECTM_MONO : PROJECTM_STEREO);
#else
		if (block.channels == 1)
			instance->pcm()->addPCMfloat(block.samples.data(), static_cast<int>(block.frames));
		else
			instance->pcm()->addPCMfloat_2ch(block.samples.data(), static_cast<int>(block.frames));
#endif
	}

#if defined(WINAMP_PROJECTM_API4)
	projectm_opengl_render_frame(instance);
#else
	// projectM 3 keeps its built-in idle preset active even when the playlist
	// is empty. Always render it so a missing preset directory cannot produce
	// a permanently black GtkGLArea.
	instance->renderFrame();
#endif
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
#if defined(WINAMP_PROJECTM_API4)
	gtk_gl_area_set_required_version(GTK_GL_AREA(area), 3, 3);
#else
	// projectM 3.1.12 uses VAOs and shader programs. Request the same
	// desktop context level used by current projectM frontends instead of
	// allowing a legacy 2.1 context with incomplete extension support.
	gtk_gl_area_set_required_version(GTK_GL_AREA(area), 3, 3);
#endif
	gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(area), TRUE);
	gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(area), TRUE);
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
