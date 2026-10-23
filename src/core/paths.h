#pragma once

#include <string>

namespace paths {

// All persisted under %LOCALAPPDATA%\SachinLoader\. Creates the tree on first call.
const std::string& root();
const std::string& settings_file();
const std::string& games_file();
const std::string& accounts_file();
const std::string& authorized_targets_file();
const std::string& logs_dir();
const std::string& themes_dir();
const std::string& log_file();

bool ensure_directory(const std::string& directory);
bool file_exists(const std::string& path);
std::string join(const std::string& a, const std::string& b);

} // namespace paths
