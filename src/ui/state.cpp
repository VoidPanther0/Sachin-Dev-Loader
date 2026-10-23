#include "state.h"

namespace ui {

AppState S;

bool IsAuthScreen(Screen screen)
{
	return screen == Screen::Splash || screen == Screen::Login;
}

void Navigate(Screen screen)
{
	S.screen = screen;
	S.screen_time = 0.0f;
	if (!IsAuthScreen(screen))
		S.last_main_screen = screen;
}

} // namespace ui
