#include "app.h"
#include "core/dialogs.h"
#include "core/logs.h"
#include "core/settings.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

bool g_initialized = false;
char g_default_dir[260] = {};
bool g_notifications = true;
bool g_skip_splash = false;
int g_tab = 0;

void load_draft()
{
	const settings::AppSettings& s = settings::current();
	std::snprintf(g_default_dir, sizeof(g_default_dir), "%s", s.default_game_directory.c_str());
	g_notifications = s.notifications_enabled;
	g_skip_splash = s.skip_splash;
}

void save_draft()
{
	settings::AppSettings& s = settings::current();
	s.default_game_directory = g_default_dir;
	s.notifications_enabled = g_notifications;
	s.skip_splash = g_skip_splash;
	settings::save();
}

} // namespace

void page_settings()
{
	if (!g_initialized)
	{
		load_draft();
		g_initialized = true;
	}

	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = layout::kSidebarWidth;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "Settings");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim, "Only options that actually do something live here.");

	const char* tab_labels[3] = { "General", "Appearance", "Advanced" };
	const Rect tab_rect = rect(area.min.x, area.min.y + 58.0f, area.min.x + 360.0f, area.min.y + 102.0f);
	panel(tab_rect, 10.0f);
	g_tab = tabs("##settings_tabs", tab_rect, tab_labels, 3, g_tab);

	const Rect body = rect(area.min.x, tab_rect.max.y + 16.0f, area.min.x + 620.0f, area.max.y - 60.0f);
	panel(body, 14.0f);

	float y = body.min.y + 20.0f;

	if (g_tab == 0)
	{
		text(F.link, 12.0f, ImVec2(body.min.x + 18.0f, y + 12.0f), P.text_secondary, "Default game directory");
		draw_field("##set_dir", "Where new games are usually installed", rect(body.min.x + 190.0f, y, body.max.x - 108.0f, y + 44.0f), g_default_dir, sizeof(g_default_dir),
			false, nullptr, [](ImVec2 c, float s, ImU32 color) { icon_folder(c, s, color); });
		if (ghost_button("##set_browse_dir", rect(body.max.x - 94.0f, y + 2.0f, body.max.x - 18.0f, y + 42.0f), "Browse"))
		{
			std::string picked;
			if (dialogs::pick_folder("Choose a default game directory", &picked))
				std::snprintf(g_default_dir, sizeof(g_default_dir), "%s", picked.c_str());
		}
		y += 64.0f;

		text(F.link, 12.0f, ImVec2(body.min.x + 18.0f, y + 6.0f), P.text_secondary, "Language");
		const Rect lang_rect = rect(body.min.x + 190.0f, y, body.min.x + 340.0f, y + 32.0f);
		box(lang_rect, colorutil::alpha(P.text, 0.04f), 8.0f, P.border_soft, 1.0f);
		text(F.link, 12.0f, ImVec2(lang_rect.min.x + 12.0f, lang_rect.min.y + 8.0f), P.text, "English");
		text(F.tiny, 10.0f, ImVec2(lang_rect.max.x + 10.0f, y + 8.0f), P.text_dim, "More languages coming later");
		y += 50.0f;

		text(F.link, 12.0f, ImVec2(body.min.x + 18.0f, y + 6.0f), P.text_secondary, "Skip the splash screen on launch");
		toggle_switch("##set_skip_splash", rect(body.max.x - 58.0f, y, body.max.x - 18.0f, y + 24.0f), &g_skip_splash);
		y += 44.0f;

		text(F.link, 12.0f, ImVec2(body.min.x + 18.0f, y + 6.0f), P.text_secondary, "In-app notifications (toasts)");
		toggle_switch("##set_notifications", rect(body.max.x - 58.0f, y, body.max.x - 18.0f, y + 24.0f), &g_notifications);
	}
	else if (g_tab == 1)
	{
		text(F.body, 14.0f, ImVec2(body.min.x + 18.0f, y), P.text, "Theme, colors, rounding and UI scale live in the Theme Customizer.");
		y += 30.0f;
		text(F.link, 12.0f, ImVec2(body.min.x + 18.0f, y), P.text_dim, "Every screen updates live when you change the active theme there.");
		y += 40.0f;
		if (primary_button("##set_go_theme", rect(body.min.x + 18.0f, y, body.min.x + 198.0f, y + 44.0f), "Open Theme Customizer", true, true))
			Navigate(Screen::ThemeCustomizer);
	}
	else
	{
		draw_banner(rect(body.min.x + 18.0f, y, body.max.x - 18.0f, y + 56.0f), Level::Info,
			"Launch on Windows startup, cloud sync and update checks need a signed installer or a backend service.");
	}

	const Rect reset_rect = rect(area.min.x + 620.0f + 20.0f, area.min.y + 58.0f, area.min.x + 620.0f + 160.0f, area.min.y + 100.0f);
	if (ghost_button("##set_reset", reset_rect, "Reset to defaults"))
	{
		settings::current() = settings::AppSettings{};
		load_draft();
		push_toast(Level::Info, "Settings reset");
	}

	const Rect save_rect = rect(area.min.x, area.max.y - 46.0f, area.min.x + 110.0f, area.max.y);
	if (primary_button("##set_save", save_rect, "Save", true, false))
	{
		save_draft();
		logs::info("Settings saved");
		push_toast(Level::Success, "Settings saved");
	}

	const Rect cancel_rect = rect(area.min.x + 122.0f, area.max.y - 46.0f, area.min.x + 232.0f, area.max.y);
	if (ghost_button("##set_cancel", cancel_rect, "Cancel"))
		load_draft();
}

} // namespace ui
