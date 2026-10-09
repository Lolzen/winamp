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

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>

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
unsigned int render_count = 0;
bool render_error_logged = false;
bool audio_block_logged = false;

bool has_preset_extension(const fs::path &path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	#if defined(WINAMP_PROJECTM_API3)
	// projectM 3.1.12 handles these through MilkdropPresetFactory. Its
	// separate .so/.dylib factory is intentionally not exposed here: distro
	// native presets are not part of Winamp's portable preset playlist.
	return extension == ".milk" || extension == ".prjm";
	#else
	return extension == ".milk" || extension == ".milk2" || extension == ".prjm";
	#endif
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
	// Void Linux packages projectM 3 presets below libexec rather than the
	// usual share directory (projectM-3.1.12_2). Keep both spellings so the
	// discovery remains robust across package revisions and installations.
	add_preset_directory("/usr/libexec/projectM/presets");
	add_preset_directory("/usr/libexec/projectm/presets");
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
	// Use the directory that actually supplied the discovered presets first.
	// Void Linux installs projectM 3 data below /usr/libexec/projectM, while
	// older frontends still default to /usr/local/share/projectM.
	if (!texture_paths.empty())
	{
		const fs::path first_path(texture_paths.front());
		if (first_path.filename() == "presets" &&
		    first_path.has_parent_path())
			return first_path.parent_path().lexically_normal().string();
	}

	const fs::path candidates[] = {
		"/usr/libexec/projectM",
		"/usr/libexec/projectm",
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

void log_first_render_state()
{
	if (render_count++ != 0) return;
	GLint draw_framebuffer = 0;
	GLint read_framebuffer = 0;
	GLint vertex_array = 0;
	GLint read_buffer = 0;
	GLint viewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glGetIntegerv(GL_VIEWPORT, viewport);
	const GLenum framebuffer_status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	g_message("projectM: first GtkGLArea render callback; draw_fbo=%d read_fbo=%d "
	          "vao=%d read_buffer=0x%04x fbo_status=0x%04x viewport=%d,%d %dx%d",
	          draw_framebuffer, read_framebuffer, vertex_array, read_buffer,
	          framebuffer_status, viewport[0], viewport[1], viewport[2], viewport[3]);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
		g_warning("projectM: GtkGLArea framebuffer is not complete (0x%04x)",
		          framebuffer_status);
}

void log_render_error()
{
	GLenum error = GL_NO_ERROR;
	while ((error = glGetError()) != GL_NO_ERROR)
	{
		if (!render_error_logged)
		{
			g_warning("projectM: OpenGL error after rendering: 0x%04x", error);
			render_error_logged = true;
		}
	}
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
	g_message("projectM 3: preset path=%s data directory=%s",
	          settings.presetURL.empty() ? "(embedded idle only)" : settings.presetURL.c_str(),
	          settings.datadir.empty() ? "(not found)" : settings.datadir.c_str());
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
		// Keep projectM's embedded idle preset as the first frame and build a
		// portable playlist from our own scan. Without this flag the Void
		// package also scans native .so presets, which changes playlist indices
		// and can select a non-portable library during startup.
		instance = new projectM(settings, projectM::FLAG_DISABLE_PLAYLIST_LOAD);
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
	// The playlist is deliberately populated from the same portable list used
	// by the UI, so the UI index and projectM's index always refer to the same
	// file. The embedded idle preset remains active until the user chooses one.
	add_projectm3_presets();
	g_message("projectM 3: playlist contains %u preset files", instance->getPlaylistSize());
	preset_load_warning_logged = false;
	if (!presets.empty())
	{
		// The playlist now contains only portable Milkdrop presets, so the
		// first real preset can be selected after the idle frame is initialized.
		request_preset(preset_index);
		g_message("projectM 3: selecting first portable preset");
	}
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
	render_count = 0;
	render_error_logged = false;
	audio_block_logged = false;
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
	if (gtk_gl_area_get_error(gl_area)) return FALSE;
	if (!instance) return TRUE;
	log_first_render_state();

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
		if (!audio_block_logged)
		{
			g_message("projectM: first PCM block on render thread; channels=%u frames=%u floats=%zu first=%+.5f",
			          block.channels, block.frames, block.samples.size(), block.samples.front());
			audio_block_logged = true;
		}
		if (block.channels == 1)
			instance->pcm()->addPCMfloat(block.samples.data(), static_cast<int>(block.frames));
		else
			// projectM 3 expects the number of interleaved float elements here,
			// not the number of frames.
			instance->pcm()->addPCMfloat_2ch(block.samples.data(),
			                                 static_cast<int>(block.samples.size()));
#endif
	}

// GtkGLArea renders into a private FBO, not framebuffer 0. projectM 3.1.12
	// predates that integration pattern and may change the active FBO/viewport
	// while rendering, so preserve and restore GTK's targets around the call.
	GLint screen_draw_fbo = 0;
	GLint screen_read_fbo = 0;
	GLint screen_vao = 0;
	GLint screen_read_buffer = 0;
	GLint screen_viewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &screen_draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &screen_read_fbo);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &screen_vao);
	glGetIntegerv(GL_READ_BUFFER, &screen_read_buffer);
	glGetIntegerv(GL_VIEWPORT, screen_viewport);

#if defined(WINAMP_PROJECTM_API4)
	projectm_opengl_render_frame(instance);
#else
	// projectM 3 keeps its built-in idle preset active even when the playlist
	// is empty. Always render it so a missing preset directory cannot produce
	// a permanently black GtkGLArea.
	instance->renderFrame();
#endif

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(screen_draw_fbo));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(screen_read_fbo));
	glBindVertexArray(static_cast<GLuint>(screen_vao));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glViewport(screen_viewport[0], screen_viewport[1], screen_viewport[2], screen_viewport[3]);
	glReadBuffer(static_cast<GLenum>(screen_read_buffer));

	if (render_count == 1)
	{
		unsigned char pixel[4] = {0, 0, 0, 0};
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		g_message("projectM: first rendered pixel RGBA=(%u,%u,%u,%u)",
		          pixel[0], pixel[1], pixel[2], pixel[3]);
	}
	log_render_error();
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
	// projectM outputs opaque RGB imagery. Avoid asking GTK to composite an
	// alpha channel whose initial value is transparent black.
	gtk_gl_area_set_has_alpha(GTK_GL_AREA(area), FALSE);
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
	gtk_window_present(GTK_WINDOW(window));
	gtk_widget_grab_focus(area);
	gtk_gl_area_queue_render(GTK_GL_AREA(area));
	render_source = g_timeout_add(16, render_tick, nullptr);
}
}

bool projectm_window_is_visible()
{
	return window && gtk_widget_get_visible(window);
}

void projectm_window_toggle()
{
	if (!window)
	{
		ensure_window();
		return;
	}
	if (!window) return;
	if (projectm_window_is_visible())
		gtk_widget_hide(window);
	else
	{
		gtk_widget_show_all(window);
		gtk_window_present(GTK_WINDOW(window));
		if (area) gtk_widget_grab_focus(area);
		if (area) gtk_gl_area_queue_render(GTK_GL_AREA(area));
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
