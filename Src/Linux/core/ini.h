/*
** Winamp for Linux - winamp.ini style settings file
** (the same [Section] key=value layout the Windows player uses).
*/
#pragma once

#include <string>
#include <vector>
#include <utility>

class IniFile
{
public:
	bool Load(const std::string &path);
	bool Save(const std::string &path) const;

	std::string GetString(const std::string &section, const std::string &key, const std::string &def = "") const;
	int GetInt(const std::string &section, const std::string &key, int def) const;
	bool Has(const std::string &section, const std::string &key) const;

	void SetString(const std::string &section, const std::string &key, const std::string &value);
	void SetInt(const std::string &section, const std::string &key, int value);
	void Remove(const std::string &section, const std::string &key);

	std::vector<std::string> Keys(const std::string &section) const;

private:
	struct Section
	{
		std::string name;
		std::vector<std::pair<std::string, std::string>> values;
	};
	const Section *Find(const std::string &section) const;
	Section &FindOrAdd(const std::string &section);
	std::vector<Section> sections;
};
