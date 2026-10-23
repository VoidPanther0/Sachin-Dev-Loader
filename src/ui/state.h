#pragma once

#include <string>

namespace ui {

enum class Screen
{
	Splash,
	Login,
	Dashboard,
	Library,
	DllManagement,
	GameConfig,
	Logs,
	ThemeCustomizer,
	Settings,
	About
};

struct AppState
{
	Screen screen = Screen::Splash;
	Screen last_main_screen = Screen::Dashboard;

	float time = 0.0f;
	float screen_time = 0.0f;
	float fps = 0.0f;

	char username[128] = {};
	char password[128] = {};
	bool remember = false;
	bool reveal_password = false;
	bool register_mode = false;

	bool authenticating = false;
	float auth_time = 0.0f;

	bool login_error = false;
	bool login_info = false;
	std::string login_message;
	float banner_age = 10.0f;

	std::string session_user;

	bool sidebar_collapsed = false;
	std::string selected_game_id;
};

extern AppState S;

void Navigate(Screen screen);
bool IsAuthScreen(Screen screen);

} // namespace ui
