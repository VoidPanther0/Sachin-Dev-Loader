#include "app.h"
#include "core/games.h"
#include "core/launcher.h"
#include "core/logs.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace ui {

namespace {

char g_search[96] = {};
int g_filter = 0; // 0 = all, 1 = favorites

bool matches(const games::GameEntry& entry)
{
	if (g_filter == 1 && !entry.favorite)
		return false;
	if (g_search[0] == '\0')
		return true;

	std::string name = entry.name;
	std::string needle = g_search;
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return name.find(needle) != std::string::npos;
}

void draw_game_card(const Rect& r, games::GameEntry& entry)
{
	ImGui::PushID(entry.id.c_str());

	const bool valid = games::path_is_valid(entry);
	panel(r, 12.0f);

	const ImU32 card_accent = entry.config.use_accent_override
		? ImGui::ColorConvertFloat4ToU32(ImVec4(entry.config.accent_override[0], entry.config.accent_override[1], entry.config.accent_override[2], 1.0f))
		: accent_for(entry.id);

	icon_tile(rect(r.min.x + 14.0f, r.min.y + 14.0f, r.min.x + 50.0f, r.min.y + 50.0f),
		[](ImVec2 c, float s, ImU32 color) { icon_chip(c, s, color); }, card_accent, 10.0f);

	text(F.body, 15.0f, ImVec2(r.min.x + 60.0f, r.min.y + 14.0f), P.text, elide(F.body, 15.0f, entry.name, width(r) - 110.0f).c_str());
	text(F.tiny, 11.0f, ImVec2(r.min.x + 60.0f, r.min.y + 36.0f), P.text_dim,
		elide(F.tiny, 11.0f, entry.version.empty() ? "no version set" : entry.version, width(r) - 110.0f).c_str());

	const Rect fav_rect = rect(r.max.x - 32.0f, r.min.y + 10.0f, r.max.x - 10.0f, r.min.y + 32.0f);
	if (icon_button("##fav", fav_rect, [&](ImVec2 c, float s, ImU32 color) { icon_star(c, s, entry.favorite ? P.warning : color, entry.favorite); }))
	{
		entry.favorite = !entry.favorite;
		games::Library::instance().save();
	}

	text(F.link, 12.0f, ImVec2(r.min.x + 16.0f, r.min.y + 58.0f), P.text_dim,
		elide(F.link, 12.0f, entry.exe_path.empty() ? "no path set" : entry.exe_path, width(r) - 32.0f).c_str());

	if (!valid)
		badge(rect(r.min.x + 16.0f, r.min.y + 78.0f, r.min.x + 90.0f, r.min.y + 98.0f), "MISSING", P.danger);

	const Rect launch_rect = rect(r.max.x - 110.0f, r.max.y - 42.0f, r.max.x - 16.0f, r.max.y - 12.0f);
	if (primary_button("##launch", launch_rect, "Launch", valid, false))
	{
		std::string error;
		if (launcher::launch(entry.exe_path, entry.config.launch_arguments, entry.config.working_directory, &error))
		{
			games::Library::instance().touch_last_played(entry.id);
			games::Library::instance().save();
			logs::success("Launched " + entry.name);
			push_toast(Level::Success, "Launching " + entry.name);
		}
		else
		{
			logs::error("Failed to launch " + entry.name + ": " + error);
			push_toast(Level::Danger, "Could not launch " + entry.name);
		}
	}

	ImGui::PopID();
}

} // namespace

void page_dashboard()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = layout::kSidebarWidth;
	const float x0 = sidebar_w + layout::kPad;
	const float y0 = layout::kTopbarHeight + layout::kPad;
	const float right_col_w = 300.0f;
	const float gap = 20.0f;

	const Rect left_col = rect(x0, y0, w - layout::kPad - right_col_w - gap, h - layout::kPad);
	const Rect right_col = rect(w - layout::kPad - right_col_w, y0, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(left_col.min.x, left_col.min.y), P.text,
		(std::string("Welcome back, ") + (S.session_user.empty() ? "friend" : S.session_user)).c_str());
	text(F.link, 13.0f, ImVec2(left_col.min.x, left_col.min.y + 30.0f), P.text_dim, "Pick a game and launch - everything else stays out of your way.");

	const games::Library& library = games::Library::instance();
	const size_t total = library.entries().size();
	size_t favorites = 0;
	for (const auto& e : library.entries()) if (e.favorite) ++favorites;

	float stat_y = left_col.min.y + 58.0f;
	const float stat_w = (width(left_col) - 24.0f) / 3.0f;
	const float stat_h = 96.0f;
	char total_buf[16]; std::snprintf(total_buf, sizeof(total_buf), "%zu", total);
	char fav_buf[16]; std::snprintf(fav_buf, sizeof(fav_buf), "%zu", favorites);
	char version_buf[16]; std::snprintf(version_buf, sizeof(version_buf), "v%s", app::kVersion);

	struct Stat { IconDrawer icon; ImU32 color; const char* value; const char* label; };
	const Stat stats[3] = {
		{ [](ImVec2 c, float s, ImU32 color) { icon_grid(c, s, color); }, P.accent, total_buf, "GAMES" },
		{ [](ImVec2 c, float s, ImU32 color) { icon_star(c, s, color, true); }, P.warning, fav_buf, "FAVORITES" },
		{ [](ImVec2 c, float s, ImU32 color) { icon_info(c, s, color); }, P.info, version_buf, "VERSION" },
	};
	for (int i = 0; i < 3; ++i)
	{
		const Rect card = rect(left_col.min.x + (stat_w + 12.0f) * i, stat_y, left_col.min.x + (stat_w + 12.0f) * i + stat_w, stat_y + stat_h);
		stat_tile(card, stats[i].icon, stats[i].color, stats[i].value, stats[i].label);
	}

	float toolbar_y = stat_y + stat_h + 20.0f;
	const Rect search_rect = rect(left_col.min.x, toolbar_y, left_col.min.x + 240.0f, toolbar_y + 44.0f);
	draw_field("##search", "Search your games", search_rect, g_search, sizeof(g_search), false, nullptr,
		[](ImVec2 c, float s, ImU32 color) { icon_search(c, s, color); });

	const char* filter_labels[2] = { "All", "Favorites" };
	const Rect filter_rect = rect(search_rect.max.x + 12.0f, toolbar_y, search_rect.max.x + 212.0f, toolbar_y + 44.0f);
	panel(filter_rect, 10.0f);
	g_filter = tabs("##dash_filter", filter_rect, filter_labels, 2, g_filter);

	const Rect add_rect = rect(left_col.max.x - 130.0f, toolbar_y, left_col.max.x, toolbar_y + 44.0f);
	if (primary_button("##go_library", add_rect, "Library", true, true))
		Navigate(Screen::Library);

	const Rect grid_rect = rect(left_col.min.x, toolbar_y + 60.0f, left_col.max.x, left_col.max.y);

	if (total == 0)
	{
		panel(grid_rect, 14.0f);
		empty_state(grid_rect, [](ImVec2 c, float s, ImU32 color) { icon_grid(c, s, color); },
			"No games yet", "Add your first game from the Library screen");
	}
	else
	{
		const float card_w = 268.0f;
		const float card_h = 112.0f;
		const int columns = std::max(1, static_cast<int>((width(grid_rect) + gap) / (card_w + gap)));
		int visible = 0;
		for (const auto& entry : library.entries()) if (matches(entry)) ++visible;
		const int rows = (visible + columns - 1) / std::max(1, columns);
		const float content_h = static_cast<float>(rows) * (card_h + gap);

		begin_scroll("##dash_grid_scroll", grid_rect);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		int index = 0;
		for (const games::GameEntry& const_entry : library.entries())
		{
			if (!matches(const_entry))
				continue;

			games::GameEntry& entry = *games::Library::instance().find(const_entry.id);
			const int col = index % columns;
			const int row = index / columns;
			const Rect card = rect(
				origin.x + static_cast<float>(col) * (card_w + gap), origin.y + static_cast<float>(row) * (card_h + gap),
				origin.x + static_cast<float>(col) * (card_w + gap) + card_w, origin.y + static_cast<float>(row) * (card_h + gap) + card_h);
			draw_game_card(card, entry);
			++index;
		}
		ImGui::Dummy(ImVec2(1.0f, content_h));
		end_scroll();

		if (visible == 0)
			empty_state(grid_rect, [](ImVec2 c, float s, ImU32 color) { icon_search(c, s, color); }, "No matches", "Try a different search or filter");
	}

	// Right column: session + quick actions + recent activity.
	const Rect session_card = rect(right_col.min.x, right_col.min.y, right_col.max.x, right_col.min.y + 96.0f);
	panel(session_card, 12.0f);
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(session_card.min.x + 30.0f, session_card.min.y + 30.0f), 15.0f, colorutil::alpha(P.accent, 0.18f), 24);
	icon_user(ImVec2(session_card.min.x + 30.0f, session_card.min.y + 30.0f), 17.0f, P.accent);
	text(F.micro, 9.0f, ImVec2(session_card.min.x + 56.0f, session_card.min.y + 16.0f), P.text_dim, "SIGNED IN AS");
	text(F.small_font, 14.0f, ImVec2(session_card.min.x + 56.0f, session_card.min.y + 33.0f), P.text,
		elide(F.small_font, 14.0f, S.session_user.empty() ? "local" : S.session_user, width(session_card) - 110.0f).c_str());
	badge(rect(session_card.min.x + 56.0f, session_card.min.y + 56.0f, session_card.min.x + 126.0f, session_card.min.y + 78.0f), "SECURE", P.success);

	const Rect logout_rect = rect(session_card.max.x - 76.0f, session_card.min.y + 56.0f, session_card.max.x - 16.0f, session_card.min.y + 80.0f);
	if (ghost_button("##dash_logout", logout_rect, "Log out"))
	{
		S.session_user.clear();
		S.password[0] = '\0';
		S.reveal_password = false;
		Navigate(Screen::Login);
	}

	float qy = session_card.max.y + 20.0f;
	text(F.micro, 10.0f, ImVec2(right_col.min.x, qy), P.text_dim, "QUICK ACTIONS");
	qy += 22.0f;

	struct QuickAction { const char* label; Screen target; };
	const QuickAction actions[] = {
		{ "DLL Management", Screen::DllManagement },
		{ "Theme Customizer", Screen::ThemeCustomizer },
		{ "Settings", Screen::Settings },
	};
	for (const QuickAction& action : actions)
	{
		const Rect r = rect(right_col.min.x, qy, right_col.max.x, qy + 42.0f);
		if (ghost_button(action.label, r, action.label))
			Navigate(action.target);
		qy += 50.0f;
	}

	qy += 10.0f;
	text(F.micro, 10.0f, ImVec2(right_col.min.x, qy), P.text_dim, "RECENT ACTIVITY");
	qy += 22.0f;

	const Rect activity_rect = rect(right_col.min.x, qy, right_col.max.x, right_col.max.y);
	panel(activity_rect, 12.0f);

	const std::vector<logs::Entry> recent = logs::Logger::instance().snapshot();
	if (recent.empty())
	{
		text_center(F.link, 12.0f, activity_rect, P.text_dim, "Nothing yet");
	}
	else
	{
		float ay = activity_rect.min.y + 14.0f;
		const size_t start = recent.size() > 6 ? recent.size() - 6 : 0;
		for (size_t i = recent.size(); i-- > start;)
		{
			if (ay > activity_rect.max.y - 20.0f)
				break;
			const logs::Entry& entry = recent[i];
			ImU32 dot = P.text_dim;
			if (entry.level == logs::Level::Success) dot = P.success;
			else if (entry.level == logs::Level::Warning) dot = P.warning;
			else if (entry.level == logs::Level::Error) dot = P.danger;
			ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(activity_rect.min.x + 18.0f, ay + 6.0f), 3.0f, dot);
			text(F.link, 12.0f, ImVec2(activity_rect.min.x + 30.0f, ay),
				P.text_secondary, elide(F.link, 12.0f, entry.text, width(activity_rect) - 46.0f).c_str());
			ay += 22.0f;
		}
	}
}

} // namespace ui
