#include "settings.h"

#include "json.h"
#include "paths.h"

namespace settings {

namespace {

AppSettings g_settings;

} // namespace

AppSettings& current()
{
	return g_settings;
}

bool load()
{
	if (!paths::file_exists(paths::settings_file()))
		return false;

	json::Value root;
	if (!json::load_file(paths::settings_file(), root))
		return false;

	if (root.has("theme_name")) g_settings.theme_name = root.get("theme_name").as_string("Purple");
	if (root.has("default_game_directory")) g_settings.default_game_directory = root.get("default_game_directory").as_string();
	if (root.has("language")) g_settings.language = root.get("language").as_string("en");
	if (root.has("remember_me")) g_settings.remember_me = root.get("remember_me").as_bool(false);
	if (root.has("remembered_username")) g_settings.remembered_username = root.get("remembered_username").as_string();
	if (root.has("skip_splash")) g_settings.skip_splash = root.get("skip_splash").as_bool(false);
	if (root.has("notifications_enabled")) g_settings.notifications_enabled = root.get("notifications_enabled").as_bool(true);

	return true;
}

bool save(std::string* error)
{
	json::Value root = json::Value::MakeObject();
	root.set("theme_name", g_settings.theme_name);
	root.set("default_game_directory", g_settings.default_game_directory);
	root.set("language", g_settings.language);
	root.set("remember_me", g_settings.remember_me);
	root.set("remembered_username", g_settings.remember_me ? g_settings.remembered_username : std::string());
	root.set("skip_splash", g_settings.skip_splash);
	root.set("notifications_enabled", g_settings.notifications_enabled);

	return json::save_file(paths::settings_file(), root, error);
}

} // namespace settings
