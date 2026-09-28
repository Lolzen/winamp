#include "titleformat.h"
#include "plugins.h"
#include "config.h"
#include "common.h"

// the unmodified title formatting engine from Src/tagz
#include "../../tagz/tagz.h"

#include <stdlib.h>
#include <wchar.h>

extern "C" const wchar_t *tagz_linux_string(int id)
{
	switch (id)
	{
	case 1: return L"internal error";
	case 2: return L"[UNKNOWN FUNCTION]";
	case 3: return L"[SYNTAX ERROR IN FORMATTING STRING]";
	case 4: return L"[INVALID $IF SYNTAX]";
	case 5: return L"[INVALID $IFLONGER SYNTAX]";
	case 6: return L"[INVALID $IFGREATER SYNTAX]";
	}
	return L"";
}

namespace
{
	class TagParameters : public ifc_tagparams
	{
	public:
		explicit TagParameters(const wchar_t *fn) : filename(fn) {}
		void *GetParameter(const GUID *parameterID)
		{
			if (*parameterID == filenameParameterID) return (void *)filename;
			return 0;
		}
		int _dispatch(int msg, void *retval, void **params, int nparam) override
		{
			(void)nparam;
			if (msg == IFC_TAGPARAMS_GETPARAMETER)
			{
				*(void **)retval = GetParameter(*(const GUID **)params[0]);
				return 1;
			}
			return 0;
		}
		const wchar_t *filename;
	};

	class TagProvider : public ifc_tagprovider
	{
	public:
		explicit TagProvider(const std::string &fn) : filename(fn), found_any(false) {}

		wchar_t *GetTag(const wchar_t *name, ifc_tagparams *)
		{
			std::string tag = wa::lower(wa::narrow(name));
			if (tag == "filename")
				return wcsdup(wa::widen(filename).c_str());
			if (tag == "folder")
				return wcsdup(wa::widen(wa::path_filename(wa::path_dirname(filename))).c_str());
			if (tag == "tracknumber") tag = "track";
			std::string v;
			if (in_get_extended_fileinfo(filename, tag.c_str(), v) && !v.empty())
			{
				found_any = true;
				return wcsdup(wa::widen(v).c_str());
			}
			return 0;
		}

		int _dispatch(int msg, void *retval, void **params, int nparam) override
		{
			(void)nparam;
			switch (msg)
			{
			case IFC_TAGPROVIDER_GET_TAG:
				*(wchar_t **)retval = GetTag(*(const wchar_t **)params[0], *(ifc_tagparams **)params[1]);
				return 1;
			case IFC_TAGPROVIDER_FREE_TAG:
				free(*(wchar_t **)params[0]);
				return 1;
			}
			return 0;
		}

		std::string filename;
		bool found_any;
	};
}

static std::string run_tagz(const std::wstring &spec, const std::string &filename, bool *found)
{
	TagProvider provider(filename);
	std::wstring wfn = wa::widen(filename);
	TagParameters params(wfn.c_str());
	VarList vars;
	FMT formatter;
	formatter.Open(spec.c_str(), &provider, &params, &vars);
	wchar_t *out = formatter;
	std::wstring result;
	if (out)
	{
		result = out;
		free(out);
	}
	for (auto &c : result)
		if (c == L'\n' || c == L'\r') c = L' ';
	if (found) *found = provider.found_any;
	return wa::narrow(result);
}

std::string format_title_spec(const std::wstring &spec, const std::string &filename)
{
	return run_tagz(spec, filename, nullptr);
}

bool format_title(const std::string &filename, std::string &title, int &length_sec)
{
	length_sec = -1;
	title.clear();
	std::string len;
	bool has_len = in_get_extended_fileinfo(filename, "length", len);
	if (has_len)
	{
		int ms = atoi(len.c_str());
		length_sec = ms > 0 ? ms / 1000 : -1;
	}
	if (!config_useexttitles) return false;
	bool found = false;
	title = run_tagz(wa::widen(config_titlefmt), filename, &found);
	if (!found || title.empty())
	{
		title.clear();
		return false;
	}
	return true;
}
