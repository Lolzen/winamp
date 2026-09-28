#include "ini.h"
#include "common.h"

#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

static std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

bool IniFile::Load(const std::string &path)
{
	sections.clear();
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::string line;
	Section *cur = nullptr;
	bool first = true;
	while (std::getline(f, line))
	{
		if (first && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
			line = line.substr(3);
		first = false;
		std::string t = trim(line);
		if (t.empty() || t[0] == ';' || t[0] == '#') continue;
		if (t[0] == '[')
		{
			size_t e = t.find(']');
			cur = &FindOrAdd(t.substr(1, e == std::string::npos ? std::string::npos : e - 1));
			continue;
		}
		size_t eq = t.find('=');
		if (eq == std::string::npos || !cur) continue;
		cur->values.emplace_back(trim(t.substr(0, eq)), t.substr(eq + 1));
	}
	return true;
}

bool IniFile::Save(const std::string &path) const
{
	std::string tmp = path + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f) return false;
		for (const auto &s : sections)
		{
			f << "[" << s.name << "]\n";
			for (const auto &kv : s.values)
				f << kv.first << "=" << kv.second << "\n";
		}
		if (!f) return false;
	}
	return rename(tmp.c_str(), path.c_str()) == 0;
}

const IniFile::Section *IniFile::Find(const std::string &section) const
{
	for (const auto &s : sections)
		if (wa::iequals(s.name, section)) return &s;
	return nullptr;
}

IniFile::Section &IniFile::FindOrAdd(const std::string &section)
{
	for (auto &s : sections)
		if (wa::iequals(s.name, section)) return s;
	sections.push_back({section, {}});
	return sections.back();
}

std::string IniFile::GetString(const std::string &section, const std::string &key, const std::string &def) const
{
	const Section *s = Find(section);
	if (!s) return def;
	for (const auto &kv : s->values)
		if (wa::iequals(kv.first, key)) return kv.second;
	return def;
}

int IniFile::GetInt(const std::string &section, const std::string &key, int def) const
{
	std::string v = GetString(section, key, "");
	if (v.empty()) return def;
	char *end = nullptr;
	long r = strtol(v.c_str(), &end, 0);
	if (end == v.c_str()) return def;
	return (int)r;
}

bool IniFile::Has(const std::string &section, const std::string &key) const
{
	const Section *s = Find(section);
	if (!s) return false;
	for (const auto &kv : s->values)
		if (wa::iequals(kv.first, key)) return true;
	return false;
}

void IniFile::SetString(const std::string &section, const std::string &key, const std::string &value)
{
	Section &s = FindOrAdd(section);
	for (auto &kv : s.values)
		if (wa::iequals(kv.first, key))
		{
			kv.second = value;
			return;
		}
	s.values.emplace_back(key, value);
}

void IniFile::SetInt(const std::string &section, const std::string &key, int value)
{
	SetString(section, key, std::to_string(value));
}

void IniFile::Remove(const std::string &section, const std::string &key)
{
	for (auto &s : sections)
		if (wa::iequals(s.name, section))
			for (size_t i = 0; i < s.values.size(); i++)
				if (wa::iequals(s.values[i].first, key))
				{
					s.values.erase(s.values.begin() + i);
					return;
				}
}

std::vector<std::string> IniFile::Keys(const std::string &section) const
{
	std::vector<std::string> r;
	const Section *s = Find(section);
	if (s)
		for (const auto &kv : s->values) r.push_back(kv.first);
	return r;
}
