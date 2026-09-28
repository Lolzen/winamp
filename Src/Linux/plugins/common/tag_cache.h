/*
** Winamp for Linux - helper for winampGetExtendedFileInfo(): reads all the
** metadata of a file once and answers the following per-field queries for the
** same file from memory (the title formatter asks for several fields in a row).
*/
#pragma once

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <map>
#include <mutex>
#include <string>

typedef std::map<std::string, std::string> TagMap; // lower case keys: "title", "artist", "length" (ms) ...

class TagCache
{
public:
	template <class Reader>
	int Get(const char *fn, const char *field, char *dest, int destlen, Reader read)
	{
		struct stat st;
		if (!fn || stat(fn, &st) != 0) return 0;
		std::lock_guard<std::mutex> l(lock);
		if (file != fn || mtime != st.st_mtime || size != st.st_size)
		{
			tags.clear();
			read(fn, tags);
			file = fn;
			mtime = st.st_mtime;
			size = st.st_size;
		}
		std::string key = field;
		for (auto &c : key) c = (char)tolower((unsigned char)c);
		auto it = tags.find(key);
		if (it == tags.end() || it->second.empty() || destlen <= 0) return 0;
		size_t n = it->second.size() < (size_t)(destlen - 1) ? it->second.size() : (size_t)(destlen - 1);
		memcpy(dest, it->second.data(), n);
		dest[n] = 0;
		return 1;
	}

private:
	std::mutex lock;
	std::string file;
	time_t mtime = 0;
	off_t size = 0;
	TagMap tags;
};
