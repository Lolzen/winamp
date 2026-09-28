#include "common.h"

#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <random>
#include <algorithm>

namespace wa
{
	std::wstring widen(const std::string &utf8)
	{
		std::wstring out;
		out.reserve(utf8.size());
		size_t i = 0, n = utf8.size();
		while (i < n)
		{
			unsigned char c = (unsigned char)utf8[i++];
			uint32_t cp;
			int extra;
			if (c < 0x80) { cp = c; extra = 0; }
			else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
			else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
			else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
			else
			{
				// not UTF-8: treat as Latin-1 (old playlists/tags)
				out.push_back((wchar_t)c);
				continue;
			}
			bool ok = i + extra <= n;
			for (int e = 0; ok && e < extra; e++)
				ok = ((unsigned char)utf8[i + e] & 0xC0) == 0x80;
			if (!ok)
			{
				out.push_back((wchar_t)c);
				continue;
			}
			for (int e = 0; e < extra; e++)
				cp = (cp << 6) | ((unsigned char)utf8[i++] & 0x3F);
			out.push_back((wchar_t)cp);
		}
		return out;
	}

	std::string narrow(const std::wstring &wide)
	{
		std::string out;
		out.reserve(wide.size());
		for (wchar_t wc : wide)
		{
			uint32_t c = (uint32_t)wc;
			if (c < 0x80) out.push_back((char)c);
			else if (c < 0x800)
			{
				out.push_back((char)(0xC0 | (c >> 6)));
				out.push_back((char)(0x80 | (c & 0x3F)));
			}
			else if (c < 0x10000)
			{
				out.push_back((char)(0xE0 | (c >> 12)));
				out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
				out.push_back((char)(0x80 | (c & 0x3F)));
			}
			else
			{
				out.push_back((char)(0xF0 | (c >> 18)));
				out.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
				out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
				out.push_back((char)(0x80 | (c & 0x3F)));
			}
		}
		return out;
	}

	std::string lower(const std::string &s)
	{
		std::string r = s;
		for (auto &c : r) c = (char)g_ascii_tolower(c);
		return r;
	}

	bool iequals(const std::string &a, const std::string &b)
	{
		return g_ascii_strcasecmp(a.c_str(), b.c_str()) == 0;
	}

	std::string path_join(const std::string &a, const std::string &b)
	{
		if (a.empty()) return b;
		if (b.empty()) return a;
		if (b[0] == '/') return b;
		if (a.back() == '/') return a + b;
		return a + "/" + b;
	}

	std::string path_filename(const std::string &p)
	{
		size_t s = p.find_last_of('/');
		if (path_is_url(p))
		{
			std::string r = s == std::string::npos ? p : p.substr(s + 1);
			return r.empty() ? p : r;
		}
		return s == std::string::npos ? p : p.substr(s + 1);
	}

	std::string path_dirname(const std::string &p)
	{
		size_t s = p.find_last_of('/');
		if (s == std::string::npos) return ".";
		if (s == 0) return "/";
		return p.substr(0, s);
	}

	std::string path_extension(const std::string &p)
	{
		std::string f = path_filename(p);
		size_t q = f.find_first_of("?#");
		if (path_is_url(p) && q != std::string::npos) f = f.substr(0, q);
		size_t d = f.find_last_of('.');
		if (d == std::string::npos) return "";
		return lower(f.substr(d + 1));
	}

	bool path_is_url(const std::string &p)
	{
		size_t c = p.find("://");
		if (c == std::string::npos || c == 0) return false;
		for (size_t i = 0; i < c; i++)
			if (!g_ascii_isalnum(p[i]) && p[i] != '+' && p[i] != '-' && p[i] != '.') return false;
		return p.compare(0, 7, "file://") != 0;
	}

	bool file_exists(const std::string &p)
	{
		struct stat st;
		return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
	}

	bool dir_exists(const std::string &p)
	{
		struct stat st;
		return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
	}

	bool make_dirs(const std::string &p)
	{
		return g_mkdir_with_parents(p.c_str(), 0755) == 0;
	}

	std::string file_uri_to_path(const std::string &uri)
	{
		if (uri.compare(0, 7, "file://") != 0) return uri;
		gchar *fn = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
		if (!fn) return uri;
		std::string r = fn;
		g_free(fn);
		return r;
	}

	std::string config_dir()
	{
		const char *env = getenv("WINAMP_CONFIG_DIR");
		if (env && *env) return env;
		return path_join(g_get_user_config_dir(), "winamp");
	}

	std::string data_dir()
	{
		return path_join(g_get_user_data_dir(), "winamp");
	}

	std::string exe_dir()
	{
		char buf[PATH_MAX] = {0};
		ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
		if (n <= 0) return ".";
		buf[n] = 0;
		return path_dirname(buf);
	}

	unsigned int tick_count()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (unsigned int)(ts.tv_sec * 1000ull + ts.tv_nsec / 1000000);
	}

	int rand_int(int max)
	{
		static std::mt19937 rng((unsigned)time(nullptr) ^ (unsigned)getpid());
		if (max <= 0) return 0;
		return (int)(rng() % (unsigned)max);
	}
}
