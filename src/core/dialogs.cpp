#include "dialogs.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h>
#include <shlobj.h>

namespace dialogs {

bool open_file(const char* filter, const char* title, std::string* out_path)
{
	char buffer[MAX_PATH] = {};

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetForegroundWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameA(&ofn))
		return false;

	*out_path = buffer;
	return true;
}

bool save_file(const char* filter, const char* title, const char* default_ext, std::string* out_path)
{
	char buffer[MAX_PATH] = {};

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetForegroundWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title;
	ofn.lpstrDefExt = default_ext;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameA(&ofn))
		return false;

	*out_path = buffer;
	return true;
}

bool pick_folder(const char* title, std::string* out_path)
{
	char display_name[MAX_PATH] = {};

	BROWSEINFOA info{};
	info.hwndOwner = GetForegroundWindow();
	info.pszDisplayName = display_name;
	info.lpszTitle = title;
	info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

	LPITEMIDLIST pidl = SHBrowseForFolderA(&info);
	if (!pidl)
		return false;

	char path[MAX_PATH] = {};
	const bool ok = SHGetPathFromIDListA(pidl, path) != 0;
	CoTaskMemFree(pidl);

	if (!ok)
		return false;

	*out_path = path;
	return true;
}

} // namespace dialogs
