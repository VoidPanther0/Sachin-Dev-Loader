#pragma once

#include <string>

namespace settings {

struct AppSettings
{
	std::string theme_name = "Purple";
	std::string default_game_directory;
	std::string language = "en";
	bool remember_me = false;
	std::string remembered_username;
	bool skip_splash = false;
	bool notifications_enabled = true;
};

AppSettings& current();
bool load();
bool save(std::string* error = nullptr);

} // namespace settings
