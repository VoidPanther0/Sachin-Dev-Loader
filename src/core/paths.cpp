#include "paths.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shlobj.h>

namespace paths {

bool file_exists(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES;
}

std::string join(const std::string& a, const std::string& b)
{
	if (!a.empty() && (a.back() == '\\' || a.back() == '/'))
		return a + b;
	return a + "\\" + b;
}

bool ensure_directory(const std::string& directory)
{
	const DWORD attributes = GetFileAttributesA(directory.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	return CreateDirectoryA(directory.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

namespace {

std::string compute_root()
{
	PWSTR wide_path = nullptr;
	std::string result;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wide_path)) && wide_path)
	{
		char buffer[MAX_PATH] = {};
		const int written = WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, buffer, MAX_PATH, nullptr, nullptr);
		if (written > 0)
			result = buffer;
		CoTaskMemFree(wide_path);
	}

	if (result.empty())
	{
		char fallback[MAX_PATH] = {};
		const DWORD length = GetEnvironmentVariableA("TEMP", fallback, MAX_PATH);
		result = (length > 0 && length < MAX_PATH) ? fallback : std::string(".");
	}

	result = join(result, "SachinLoader");
	ensure_directory(result);
	return result;
}

} // namespace

const std::string& root()
{
	static const std::string value = compute_root();
	return value;
}

const std::string& settings_file()
{
	static const std::string value = join(root(), "settings.json");
	return value;
}

const std::string& games_file()
{
	static const std::string value = join(root(), "games.json");
	return value;
}

const std::string& accounts_file()
{
	static const std::string value = join(root(), "accounts.json");
	return value;
}

const std::string& authorized_targets_file()
{
	static const std::string value = join(root(), "authorized_targets.json");
	return value;
}

const std::string& logs_dir()
{
	static const std::string value = [] {
		std::string dir = join(root(), "logs");
		ensure_directory(dir);
		return dir;
	}();
	return value;
}

const std::string& log_file()
{
	static const std::string value = join(logs_dir(), "loader.log");
	return value;
}

const std::string& themes_dir()
{
	static const std::string value = [] {
		std::string dir = join(root(), "themes");
		ensure_directory(dir);
		return dir;
	}();
	return value;
}

} // namespace paths
