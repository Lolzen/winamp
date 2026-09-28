#include "playlist.h"
#include "plugins.h"
#include "titleformat.h"
#include "config.h"
#include "common.h"

#include <glib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

namespace
{
	struct Entry
	{
		std::string filename;
		std::string title;     // "" until resolved (or given by the playlist file)
		int length = -1;       // seconds
		bool selected = false;
		bool resolved = false; // metadata lookup done
	};
	typedef std::shared_ptr<Entry> EntryPtr;

	std::mutex lock;                 // protects the Entry fields written by the worker
	std::vector<EntryPtr> list;
	int position = 0;
	std::vector<int> shuffle_order;
	size_t shuffle_list_size = (size_t)-1;
	std::function<void()> change_cb;

	// background metadata lookups
	std::thread worker;
	std::mutex queue_lock;
	std::condition_variable queue_cv;
	std::deque<std::weak_ptr<Entry>> queue;
	bool worker_quit = false;
	std::atomic<bool> notify_pending(false);
}

static gboolean notify_ui(gpointer)
{
	notify_pending = false;
	if (change_cb) change_cb();
	return G_SOURCE_REMOVE;
}

static void schedule_notify()
{
	bool expected = false;
	if (notify_pending.compare_exchange_strong(expected, true))
		g_timeout_add(100, notify_ui, nullptr);
}

static std::string fallback_title(const std::string &fn)
{
	std::string f = wa::path_filename(fn);
	if (wa::path_is_url(fn)) return fn;
	size_t d = f.find_last_of('.');
	if (d != std::string::npos && d > 0) f = f.substr(0, d);
	return f;
}

// returns true if the title came from the file's metadata
static bool resolve_entry(const std::string &filename, std::string &title, int &length)
{
	if (format_title(filename, title, length)) return true;
	std::string t;
	int l = -1;
	in_get_fileinfo(filename, t, l);
	if (length < 0) length = l;
	title = !t.empty() ? t : fallback_title(filename);
	return false;
}

static void worker_main()
{
	for (;;)
	{
		EntryPtr e;
		{
			std::unique_lock<std::mutex> ql(queue_lock);
			queue_cv.wait(ql, [] { return worker_quit || !queue.empty(); });
			if (worker_quit) return;
			e = queue.front().lock();
			queue.pop_front();
		}
		if (!e) continue;
		std::string fn;
		{
			std::lock_guard<std::mutex> l(lock);
			if (e->resolved) continue;
			fn = e->filename;
		}
		std::string title;
		int length = -1;
		bool from_tags = false;
		if (wa::path_is_url(fn))
			title = fn;
		else
			from_tags = resolve_entry(fn, title, length);
		{
			std::lock_guard<std::mutex> l(lock);
			// keep #EXTINF values unless the file itself told us better
			if (from_tags || e->title.empty()) e->title = title;
			if (length >= 0) e->length = length;
			e->resolved = true;
		}
		schedule_notify();
	}
}

static void enqueue(const EntryPtr &e)
{
	{
		std::lock_guard<std::mutex> ql(queue_lock);
		if (!worker.joinable())
		{
			worker_quit = false;
			worker = std::thread(worker_main);
		}
		queue.push_back(e);
	}
	queue_cv.notify_one();
}

void PlayList_setchangecallback(std::function<void()> cb)
{
	change_cb = cb;
}

void PlayList_shutdown()
{
	{
		std::lock_guard<std::mutex> ql(queue_lock);
		worker_quit = true;
		queue.clear();
	}
	queue_cv.notify_all();
	if (worker.joinable()) worker.join();
}

static bool valid(int idx)
{
	return idx >= 0 && idx < (int)list.size();
}

int PlayList_getlength()
{
	return (int)list.size();
}

int PlayList_getPosition()
{
	if (list.empty()) return 0;
	if (position >= (int)list.size()) position = (int)list.size() - 1;
	return position;
}

void PlayList_setposition(int pos)
{
	if (list.empty())
	{
		position = 0;
		return;
	}
	position = std::max(0, std::min(pos, (int)list.size() - 1));
}

int PlayList_advance(int n)
{
	PlayList_setposition(position + n);
	return position;
}

std::string PlayList_getfilename(int idx)
{
	if (!valid(idx)) return "";
	std::lock_guard<std::mutex> l(lock);
	return list[idx]->filename;
}

std::string PlayList_gettitle(int idx)
{
	if (!valid(idx)) return "";
	std::lock_guard<std::mutex> l(lock);
	const Entry &e = *list[idx];
	return e.title.empty() ? fallback_title(e.filename) : e.title;
}

std::string PlayList_getitem_pl(int idx)
{
	if (!valid(idx)) return "";
	std::string t = PlayList_gettitle(idx);
	if (config_shownumsinpl)
		return std::to_string(idx + 1) + ". " + t;
	return t;
}

int PlayList_getsonglength(int idx)
{
	if (!valid(idx)) return -1;
	std::lock_guard<std::mutex> l(lock);
	return list[idx]->length;
}

int PlayList_getcurrentlength()
{
	return PlayList_getsonglength(PlayList_getPosition());
}

bool PlayList_getselect(int idx)
{
	return valid(idx) && list[idx]->selected;
}

void PlayList_setselect(int idx, bool sel)
{
	if (valid(idx)) list[idx]->selected = sel;
}

int PlayList_getselectcount()
{
	int n = 0;
	for (auto &e : list)
		if (e->selected) n++;
	return n;
}

void PlayList_swap(int a, int b)
{
	if (valid(a) && valid(b)) std::swap(list[a], list[b]);
}

void PlayList_insert(int pos, const std::string &filename, const std::string &title, int length_sec)
{
	EntryPtr e = std::make_shared<Entry>();
	e->filename = filename;
	e->title = title;
	e->length = length_sec;
	if (pos < 0 || pos > (int)list.size()) pos = (int)list.size();
	if (!list.empty() && pos <= position && pos < (int)list.size()) position++;
	list.insert(list.begin() + pos, e);
	enqueue(e);
}

void PlayList_append(const std::string &filename, const std::string &title, int length_sec)
{
	PlayList_insert(-1, filename, title, length_sec);
}

void PlayList_delete(int idx)
{
	if (!valid(idx)) return;
	list.erase(list.begin() + idx);
	if (idx < position) position--;
	PlayList_setposition(position);
}

void PlayList_deleteselected()
{
	for (int i = (int)list.size() - 1; i >= 0; i--)
		if (list[i]->selected) PlayList_delete(i);
}

void PlayList_cropselected()
{
	for (int i = (int)list.size() - 1; i >= 0; i--)
		if (!list[i]->selected) PlayList_delete(i);
}

void PlayList_clear()
{
	list.clear();
	position = 0;
}

void PlayList_refreshtitle(int idx)
{
	if (!valid(idx)) return;
	{
		std::lock_guard<std::mutex> l(lock);
		list[idx]->resolved = false;
		list[idx]->title.clear();
	}
	enqueue(list[idx]);
}

void PlayList_refreshselected()
{
	for (int i = 0; i < (int)list.size(); i++)
		if (list[i]->selected) PlayList_refreshtitle(i);
}

void PlayList_removemissing()
{
	for (int i = (int)list.size() - 1; i >= 0; i--)
	{
		const std::string &fn = list[i]->filename;
		if (!wa::path_is_url(fn) && !wa::file_exists(fn)) PlayList_delete(i);
	}
}

void PlayList_sort(int mode)
{
	if (list.empty()) return;
	EntryPtr cur = valid(position) ? list[position] : nullptr;
	std::lock_guard<std::mutex> l(lock);
	auto title_of = [](const EntryPtr &e) { return e->title.empty() ? fallback_title(e->filename) : e->title; };
	switch (mode)
	{
	case PL_SORT_TITLE:
		std::stable_sort(list.begin(), list.end(), [&](const EntryPtr &a, const EntryPtr &b) {
			return g_utf8_collate(title_of(a).c_str(), title_of(b).c_str()) < 0;
		});
		break;
	case PL_SORT_FILENAME:
		std::stable_sort(list.begin(), list.end(), [](const EntryPtr &a, const EntryPtr &b) {
			return g_ascii_strcasecmp(wa::path_filename(a->filename).c_str(), wa::path_filename(b->filename).c_str()) < 0;
		});
		break;
	case PL_SORT_PATH:
		std::stable_sort(list.begin(), list.end(), [](const EntryPtr &a, const EntryPtr &b) {
			return g_ascii_strcasecmp(a->filename.c_str(), b->filename.c_str()) < 0;
		});
		break;
	case PL_SORT_RANDOM:
		for (int i = (int)list.size() - 1; i > 0; i--)
			std::swap(list[i], list[wa::rand_int(i + 1)]);
		break;
	case PL_SORT_REVERSE:
		std::reverse(list.begin(), list.end());
		break;
	}
	if (cur)
		for (int i = 0; i < (int)list.size(); i++)
			if (list[i] == cur) position = i;
}

/* ---------------- shuffle ---------------- */

void PlayList_reshuffle()
{
	int n = (int)list.size();
	shuffle_order.resize(n);
	for (int i = 0; i < n; i++) shuffle_order[i] = i;
	for (int i = n - 1; i > 0; i--)
		std::swap(shuffle_order[i], shuffle_order[wa::rand_int(i + 1)]);
	// keep the current song first so we don't hear it again soon
	for (int i = 0; i < n; i++)
		if (shuffle_order[i] == position)
		{
			std::swap(shuffle_order[0], shuffle_order[i]);
			break;
		}
	shuffle_list_size = list.size();
}

static int shuffle_index_of(int pos)
{
	if (shuffle_list_size != list.size()) PlayList_reshuffle();
	for (int i = 0; i < (int)shuffle_order.size(); i++)
		if (shuffle_order[i] == pos) return i;
	return 0;
}

int PlayList_getnext(bool manual)
{
	int n = (int)list.size();
	if (!n) return -1;
	if (config_shuffle && n > 1)
	{
		int i = shuffle_index_of(position) + 1;
		if (i >= n)
		{
			if (!config_repeat && !manual) return -1; // manual "next" keeps shuffling
			PlayList_reshuffle();
			if (shuffle_order[0] == position) std::swap(shuffle_order[0], shuffle_order[n - 1]);
			return shuffle_order[0];
		}
		return shuffle_order[i];
	}
	(void)manual;
	if (position + 1 < n) return position + 1;
	if (config_repeat) return 0;
	return -1;
}

int PlayList_getprev()
{
	int n = (int)list.size();
	if (!n) return -1;
	if (config_shuffle && n > 1)
	{
		int i = shuffle_index_of(position) - 1;
		if (i < 0) i = n - 1;
		return shuffle_order[i];
	}
	if (position > 0) return position - 1;
	return config_repeat ? n - 1 : -1;
}

/* ---------------- loading ---------------- */

bool PlayList_isplaylist(const std::string &filename)
{
	std::string ext = wa::path_extension(filename);
	return ext == "m3u" || ext == "m3u8" || ext == "pls";
}

static std::string resolve_relative(const std::string &base_dir, std::string p)
{
	if (p.empty()) return p;
	if (p.compare(0, 7, "file://") == 0) return wa::file_uri_to_path(p);
	if (wa::path_is_url(p)) return p;
	std::string posix = p;
	std::replace(posix.begin(), posix.end(), '\\', '/');
	if (posix[0] == '/') return posix;
	if (posix.size() > 1 && posix[1] == ':') return posix; // Windows drive path, can't be resolved
	return wa::path_join(base_dir, posix);
}

static int add_directory(const std::string &dir, int &insert_pos)
{
	std::vector<std::string> files, dirs;
	DIR *d = opendir(dir.c_str());
	if (!d) return 0;
	while (struct dirent *e = readdir(d))
	{
		if (e->d_name[0] == '.') continue;
		std::string full = wa::path_join(dir, e->d_name);
		if (wa::dir_exists(full)) dirs.push_back(full);
		else if (!PlayList_isplaylist(full) && in_is_supported(full)) files.push_back(full);
	}
	closedir(d);
	auto cmp = [](const std::string &a, const std::string &b) { return g_ascii_strcasecmp(a.c_str(), b.c_str()) < 0; };
	std::sort(files.begin(), files.end(), cmp);
	std::sort(dirs.begin(), dirs.end(), cmp);
	int added = 0;
	for (auto &f : files)
	{
		PlayList_insert(insert_pos, f);
		if (insert_pos >= 0) insert_pos++;
		added++;
	}
	for (auto &sub : dirs)
		added += add_directory(sub, insert_pos);
	return added;
}

int PlayList_add(const std::string &path_or_url, int insert_pos)
{
	std::string p = wa::file_uri_to_path(path_or_url);
	if (wa::path_is_url(p))
	{
		PlayList_insert(insert_pos, p);
		return 1;
	}
	if (wa::dir_exists(p))
		return add_directory(p, insert_pos);
	if (PlayList_isplaylist(p))
		return PlayList_load(p, insert_pos);
	PlayList_insert(insert_pos, p);
	return 1;
}

static void strip_cr(std::string &s)
{
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
}

int PlayList_load(const std::string &file, int insert_pos)
{
	std::ifstream f(file, std::ios::binary);
	if (!f) return 0;
	std::string dir = wa::path_dirname(file);
	int added = 0;
	auto add = [&](const std::string &fn, const std::string &title, int len) {
		PlayList_insert(insert_pos, fn, title, len);
		if (insert_pos >= 0) insert_pos++;
		added++;
	};

	if (wa::path_extension(file) == "pls")
	{
		struct Item { std::string file, title; int len = -1; };
		std::vector<Item> items;
		std::string line;
		while (std::getline(f, line))
		{
			strip_cr(line);
			size_t eq = line.find('=');
			if (eq == std::string::npos) continue;
			std::string key = wa::lower(line.substr(0, eq)), val = line.substr(eq + 1);
			int n = 0;
			std::string what;
			if (key.compare(0, 4, "file") == 0) { what = "file"; n = atoi(key.c_str() + 4); }
			else if (key.compare(0, 5, "title") == 0) { what = "title"; n = atoi(key.c_str() + 5); }
			else if (key.compare(0, 6, "length") == 0) { what = "length"; n = atoi(key.c_str() + 6); }
			if (n < 1 || n > 1000000) continue;
			if ((int)items.size() < n) items.resize(n);
			Item &it = items[n - 1];
			if (what == "file") it.file = resolve_relative(dir, val);
			else if (what == "title") it.title = val;
			else if (what == "length") it.len = atoi(val.c_str());
		}
		for (auto &it : items)
			if (!it.file.empty()) add(it.file, it.title, it.len);
		return added;
	}

	std::string line, ext_title;
	int ext_len = -1;
	bool first = true;
	while (std::getline(f, line))
	{
		if (first && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
			line = line.substr(3);
		first = false;
		strip_cr(line);
		if (line.empty()) continue;
		if (line[0] == '#')
		{
			if (line.compare(0, 8, "#EXTINF:") == 0)
			{
				size_t c = line.find(',');
				ext_len = atoi(line.c_str() + 8);
				ext_title = c != std::string::npos ? line.substr(c + 1) : "";
			}
			continue;
		}
		add(resolve_relative(dir, line), ext_title, ext_len);
		ext_title.clear();
		ext_len = -1;
	}
	return added;
}

bool PlayList_save(const std::string &file, bool selected_only)
{
	std::ofstream f(file, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	std::lock_guard<std::mutex> l(lock);
	if (wa::path_extension(file) == "pls")
	{
		f << "[playlist]\n";
		int n = 0;
		for (auto &e : list)
		{
			if (selected_only && !e->selected) continue;
			n++;
			f << "File" << n << "=" << e->filename << "\n";
			f << "Title" << n << "=" << (e->title.empty() ? fallback_title(e->filename) : e->title) << "\n";
			f << "Length" << n << "=" << e->length << "\n";
		}
		f << "NumberOfEntries=" << n << "\nVersion=2\n";
		return (bool)f;
	}
	f << "#EXTM3U\n";
	for (auto &e : list)
	{
		if (selected_only && !e->selected) continue;
		f << "#EXTINF:" << e->length << "," << (e->title.empty() ? fallback_title(e->filename) : e->title) << "\n";
		f << e->filename << "\n";
	}
	return (bool)f;
}
