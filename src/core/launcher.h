#pragma once

#include <string>

// Starts an executable the user explicitly configured in their own game
// library or settings. No elevation, no injection, no hidden process - a
// plain CreateProcess, same as double-clicking the file.
namespace launcher {

bool launch(const std::string& exe_path, const std::string& arguments,
	const std::string& working_directory, std::string* error = nullptr);

} // namespace launcher
