/*
** Winamp projectM visualization plug-in.
**
** This is a native Winamp VIS plug-in. It owns a Win32 OpenGL window and
** feeds projectM the waveform data supplied by Winamp on each render call.
** No projectM object is touched from the window procedure; resize and preset
** requests are applied by the render thread while its WGL context is current.
*/
#define WIN32_LEAN_AND_MEAN
#define USE_VIS_HDR_HWND

#include <windows.h>
#include <gl/GL.h>

#include "../../../Winamp/VIS.H"
#include <projectM-4/projectM.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#pragma comment(lib, "opengl32.lib")

namespace
{
namespace fs = std::filesystem;

constexpr char WINDOW_CLASS[] = "WinampProjectMVisualizer";
constexpr char WINDOW_TITLE[] = "projectM Visualizer";
constexpr int PCM_FRAMES = 576;

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;

using WglCreateContextAttribsARB = HGLRC(WINAPI *)(HDC, HGLRC, const int *);

struct ProjectMState
{
	HWND parent = nullptr;
	HINSTANCE module = nullptr;
	HWND window = nullptr;
	HDC dc = nullptr;
	HGLRC gl_context = nullptr;
	projectm_handle instance = nullptr;
	std::vector<std::string> presets;
	std::vector<std::string> texture_paths;
	size_t preset_index = 0;
	std::atomic<int> preset_delta{0};
	std::atomic<int> pending_width{0};
	std::atomic<int> pending_height{0};
	std::atomic<bool> closed{false};
};

ProjectMState state;

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
	add_unique(state.texture_paths, directory);
	add_unique(state.texture_paths, directory / "textures");

	fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec);
	fs::recursive_directory_iterator end;
	for (; it != end && !ec; it.increment(ec))
	{
		if (!it->is_regular_file(ec) || !has_preset_extension(it->path())) continue;
		state.presets.push_back(it->path().lexically_normal().string());
		add_unique(state.texture_paths, it->path().parent_path());
	}
}

void add_env_directories()
{
	char buffer[32768] = {0};
	DWORD length = GetEnvironmentVariableA("WINAMP_PROJECTM_PRESETS", buffer, sizeof(buffer));
	if (!length || length >= sizeof(buffer)) return;
	std::string value(buffer, length);
	size_t start = 0;
	while (start <= value.size())
	{
		size_t end = value.find(';', start);
		add_preset_directory(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if (end == std::string::npos) break;
		start = end + 1;
	}
}

fs::path module_directory()
{
	char path[MAX_PATH] = {0};
	DWORD length = GetModuleFileNameA(state.module, path, sizeof(path));
	if (!length || length >= sizeof(path)) return {};
	return fs::path(path).parent_path();
}

void discover_presets()
{
	state.presets.clear();
	state.texture_paths.clear();
	const fs::path module = module_directory();
	add_env_directories();
	add_preset_directory(module / "projectM" / "presets");
	add_preset_directory(module / "presets");
	char appdata[MAX_PATH] = {0};
	if (GetEnvironmentVariableA("APPDATA", appdata, sizeof(appdata)))
		add_preset_directory(fs::path(appdata) / "projectM" / "presets");
	char programdata[MAX_PATH] = {0};
	if (GetEnvironmentVariableA("PROGRAMDATA", programdata, sizeof(programdata)))
		add_preset_directory(fs::path(programdata) / "projectM" / "presets");
	std::sort(state.presets.begin(), state.presets.end());
	state.presets.erase(std::unique(state.presets.begin(), state.presets.end()), state.presets.end());
}

void set_texture_paths()
{
	if (!state.instance || state.texture_paths.empty()) return;
	std::vector<const char *> paths;
	paths.reserve(state.texture_paths.size());
	for (const std::string &path : state.texture_paths)
		paths.push_back(path.c_str());
	projectm_set_texture_search_paths(state.instance, paths.data(), paths.size());
}

void load_current_preset()
{
	if (!state.instance) return;
	if (state.presets.empty())
	{
		projectm_load_preset_file(state.instance, "idle://", false);
		return;
	}
	const std::string &path = state.presets[state.preset_index % state.presets.size()];
	projectm_load_preset_file(state.instance, path.c_str(), false);
}

void apply_pending_preset()
{
	if (!state.instance || state.presets.empty()) return;
	int delta = state.preset_delta.exchange(0);
	if (!delta) return;
	const int count = static_cast<int>(state.presets.size());
	int next = static_cast<int>(state.preset_index) + delta;
	next %= count;
	if (next < 0) next += count;
	state.preset_index = static_cast<size_t>(next);
	const std::string &path = state.presets[state.preset_index];
	projectm_load_preset_file(state.instance, path.c_str(), true);
}

bool create_gl_context()
{
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int format = ChoosePixelFormat(state.dc, &pfd);
	if (!format || !SetPixelFormat(state.dc, format, &pfd)) return false;

	HGLRC legacy = wglCreateContext(state.dc);
	if (!legacy || !wglMakeCurrent(state.dc, legacy))
	{
		if (legacy) wglDeleteContext(legacy);
		return false;
	}

	WglCreateContextAttribsARB create_attribs = reinterpret_cast<WglCreateContextAttribsARB>(
		wglGetProcAddress("wglCreateContextAttribsARB"));
	if (create_attribs)
	{
		const int attributes[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};
		HGLRC modern = create_attribs(state.dc, nullptr, attributes);
		if (modern)
		{
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(legacy);
			state.gl_context = modern;
			return wglMakeCurrent(state.dc, state.gl_context) == TRUE;
		}
	}

	state.gl_context = legacy;
	return true;
}

void destroy_gl_context()
{
	if (state.gl_context)
	{
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(state.gl_context);
		state.gl_context = nullptr;
	}
	if (state.dc && state.window)
	{
		ReleaseDC(state.window, state.dc);
		state.dc = nullptr;
	}
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_CLOSE:
		state.closed = true;
		ShowWindow(window, SW_HIDE);
		return 0;
	case WM_DESTROY:
		state.closed = true;
		return 0;
	case WM_SIZE:
		state.pending_width = LOWORD(lparam);
		state.pending_height = HIWORD(lparam);
		return 0;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE)
		{
			state.closed = true;
			ShowWindow(window, SW_HIDE);
			return 0;
		}
		if (wparam == VK_RIGHT)
		{
			state.preset_delta.fetch_add(1);
			return 0;
		}
		if (wparam == VK_LEFT)
		{
			state.preset_delta.fetch_sub(1);
			return 0;
		}
		break;
	case WM_ERASEBKGND:
		return 1;
	default:
		break;
	}
	return DefWindowProcA(window, message, wparam, lparam);
}

bool create_window()
{
	WNDCLASSEXA wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = window_proc;
	wc.hInstance = state.module;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WINDOW_CLASS;
	RegisterClassExA(&wc);

	state.window = CreateWindowExA(
		WS_EX_APPWINDOW,
		WINDOW_CLASS,
		WINDOW_TITLE,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		nullptr, nullptr, state.module, nullptr);
	if (!state.window) return false;
	state.dc = GetDC(state.window);
	if (!state.dc || !create_gl_context()) return false;
	return true;
}

void cleanup()
{
	if (state.instance)
	{
		if (state.dc && state.gl_context) wglMakeCurrent(state.dc, state.gl_context);
		projectm_destroy(state.instance);
		state.instance = nullptr;
	}
	destroy_gl_context();
	if (state.window)
	{
		DestroyWindow(state.window);
		state.window = nullptr;
	}
	state.presets.clear();
	state.texture_paths.clear();
	state.closed = false;
}
}

static void __cdecl config(struct winampVisModule *)
{
	MessageBoxA(state.parent,
		"Set WINAMP_PROJECTM_PRESETS to a semicolon-separated list of projectM preset directories.\n"
		"Use the left/right arrow keys in the visualizer to change presets.",
		"projectM Visualizer", MB_OK | MB_ICONINFORMATION);
}

static int __cdecl init(struct winampVisModule *module)
{
	cleanup();
	state.parent = module->hwndParent;
	state.module = module->hDllInstance;
	state.closed = false;
	state.pending_width = 800;
	state.pending_height = 600;
	discover_presets();
	if (!create_window())
	{
		cleanup();
		return 1;
	}

	state.instance = projectm_create();
	if (!state.instance)
	{
		cleanup();
		return 1;
	}
	projectm_set_fps(state.instance, 60);
	projectm_set_aspect_correction(state.instance, true);
	set_texture_paths();
	projectm_set_window_size(state.instance, 800, 600);
	load_current_preset();

	ShowWindow(state.window, SW_SHOW);
	UpdateWindow(state.window);
	return 0;
}

static int __cdecl render(struct winampVisModule *module)
{
	if (!state.instance || !state.window || state.closed) return 1;
	if (!wglMakeCurrent(state.dc, state.gl_context)) return 1;

	int width = state.pending_width.exchange(0);
	int height = state.pending_height.exchange(0);
	if (width > 0 && height > 0)
		projectm_set_window_size(state.instance, static_cast<size_t>(width), static_cast<size_t>(height));
	apply_pending_preset();

	float pcm[PCM_FRAMES * 2];
	for (int i = 0; i < PCM_FRAMES; i++)
	{
		pcm[i * 2] = (static_cast<float>(module->waveformData[0][i]) - 128.0f) / 128.0f;
		pcm[i * 2 + 1] = (static_cast<float>(module->waveformData[1][i]) - 128.0f) / 128.0f;
	}
	projectm_pcm_add_float(state.instance, pcm, PCM_FRAMES, PROJECTM_STEREO);
	projectm_opengl_render_frame(state.instance);
	SwapBuffers(state.dc);
	return 0;
}

static void __cdecl quit(struct winampVisModule *)
{
	cleanup();
}

static char description[] = "projectM Visualizer";
static winampVisModule module = {
	description,
	nullptr,
	nullptr,
	0,
	0,
	0,
	16,
	0,
	2,
	{{0}},
	{{0}},
	config,
	init,
	render,
	quit,
	nullptr
};

static winampVisModule *get_module(int which)
{
	return which == 0 ? &module : nullptr;
}

static winampVisHeader header = {VIS_HDRVER, description, get_module};

extern "C" __declspec(dllexport) winampVisHeader *__cdecl winampVisGetHeader(HWND hwndParent)
{
	state.parent = hwndParent;
	return &header;
}
