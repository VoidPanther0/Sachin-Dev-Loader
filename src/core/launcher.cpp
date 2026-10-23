#include "launcher.h"

#include "paths.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vector>

namespace launcher {

bool launch(const std::string& exe_path, const std::string& arguments,
	const std::string& working_directory, std::string* error)
{
	if (!paths::file_exists(exe_path))
	{
		if (error) *error = "the executable could not be found on disk";
		return false;
	}

	std::string command_line = "\"" + exe_path + "\"";
	if (!arguments.empty())
	{
		command_line += " ";
		command_line += arguments;
	}

	std::string dir = working_directory;
	if (dir.empty())
	{
		const size_t slash = exe_path.find_last_of("\\/");
		dir = slash == std::string::npos ? "" : exe_path.substr(0, slash);
	}

	std::vector<char> buffer(command_line.begin(), command_line.end());
	buffer.push_back('\0');

	STARTUPINFOA startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};

	const BOOL ok = CreateProcessA(
		nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
		dir.empty() ? nullptr : dir.c_str(), &startup, &process);

	if (!ok)
	{
		if (error) *error = "CreateProcess failed (error " + std::to_string(GetLastError()) + ")";
		return false;
	}

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

} // namespace launcher
