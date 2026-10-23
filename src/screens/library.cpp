#include "app.h"
#include "core/dialogs.h"
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
#include <vector>

namespace ui {

namespace {

char g_search[96] = {};
int g_sort = 0; // 0 name, 1 recently played, 2 favorites first

bool g_modal_open = false;
bool g_modal_is_edit = false;
std::string g_modal_game_id;
char g_name[128] = {};
char g_exe[260] = {};
char g_icon[260] = {};
char g_version[64] = {};
std::string g_modal_error;

std::string g_pending_delete_id;
std::string g_pending_delete_name;

bool matches_search(const games::GameEntry& entry)
{
	if (g_search[0] == '\0')
		return true;
	std::string name = entry.name;
	std::string needle = g_search;
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return name.find(needle) != std::string::npos;
}

std::vector<std::string> visible_ids()
{
	std::vector<games::GameEntry> entries = games::Library::instance().entries();
	std::vector<games::GameEntry*> filtered;
	for (games::GameEntry& e : entries)
		if (matches_search(e))
			filtered.push_back(&e);

	if (g_sort == 0)
		std::sort(filtered.begin(), filtered.end(), [](auto* a, auto* b) { return a->name < b->name; });
	else if (g_sort == 1)
		std::sort(filtered.begin(), filtered.end(), [](auto* a, auto* b) { return a->last_played_ms > b->last_played_ms; });
	else
		std::sort(filtered.begin(), filtered.end(), [](auto* a, auto* b) {
			if (a->favorite != b->favorite) return a->favorite > b->favorite;
			return a->name < b->name;
		});

	std::vector<std::string> ids;
	ids.reserve(filtered.size());
	for (auto* e : filtered) ids.push_back(e->id);
	return ids;
}

void open_add_modal()
{
	g_modal_open = true;
	g_modal_is_edit = false;
	g_modal_game_id.clear();
	g_name[0] = g_exe[0] = g_icon[0] = g_version[0] = '\0';
	g_modal_error.clear();
}

void open_edit_modal(const games::GameEntry& entry)
{
	g_modal_open = true;
	g_modal_is_edit = true;
	g_modal_game_id = entry.id;
	std::snprintf(g_name, sizeof(g_name), "%s", entry.name.c_str());
	std::snprintf(g_exe, sizeof(g_exe), "%s", entry.exe_path.c_str());
	std::snprintf(g_icon, sizeof(g_icon), "%s", entry.icon_path.c_str());
	std::snprintf(g_version, sizeof(g_version), "%s", entry.version.c_str());
	g_modal_error.clear();
}

void save_modal()
{
	if (g_name[0] == '\0' || g_exe[0] == '\0')
	{
		g_modal_error = "Name and executable path are required.";
		return;
	}

	games::GameEntry entry;
	entry.name = g_name;
	entry.exe_path = g_exe;
	entry.icon_path = g_icon;
	entry.version = g_version;

	games::Library& library = games::Library::instance();
	if (g_modal_is_edit)
	{
		if (const games::GameEntry* existing = library.find(g_modal_game_id))
		{
			entry.favorite = existing->favorite;
			entry.last_played_ms = existing->last_played_ms;
			entry.config = existing->config;
		}
		library.update(g_modal_game_id, entry);
		logs::info("Updated game entry: " + entry.name);
	}
	else
	{
		library.add(entry);
		logs::info("Added game entry: " + entry.name);
	}

	library.save();
	push_toast(Level::Success, g_modal_is_edit ? "Game updated" : "Game added");
	g_modal_open = false;
}

void draw_add_edit_modal()
{
	Modal modal = modal_frame("##game_modal", g_modal_is_edit ? "Edit game" : "Add game", 520.0f, 400.0f);
	const Rect c = modal.content;
	float y = c.min.y;

	draw_field("##g_name", "Display name", rect(c.min.x, y, c.max.x, y + 50.0f), g_name, sizeof(g_name), false, nullptr,
		[](ImVec2 p, float s, ImU32 color) { icon_chip(p, s, color); });
	y += 60.0f;

	const Rect exe_field = rect(c.min.x, y, c.max.x - 96.0f, y + 50.0f);
	draw_field("##g_exe", "Executable path (.exe)", exe_field, g_exe, sizeof(g_exe), false, nullptr,
		[](ImVec2 p, float s, ImU32 color) { icon_folder(p, s, color); });
	if (ghost_button("##browse_exe", rect(c.max.x - 84.0f, y + 3.0f, c.max.x, y + 47.0f), "Browse"))
	{
		std::string picked;
		if (dialogs::open_file("Executables (*.exe)\0*.exe\0All files (*.*)\0*.*\0", "Choose the game executable", &picked))
			std::snprintf(g_exe, sizeof(g_exe), "%s", picked.c_str());
	}
	y += 60.0f;

	const Rect icon_field = rect(c.min.x, y, c.max.x - 96.0f, y + 50.0f);
	draw_field("##g_icon", "Icon image path (optional)", icon_field, g_icon, sizeof(g_icon), false, nullptr,
		[](ImVec2 p, float s, ImU32 color) { icon_folder(p, s, color); });
	if (ghost_button("##browse_icon", rect(c.max.x - 84.0f, y + 3.0f, c.max.x, y + 47.0f), "Browse"))
	{
		std::string picked;
		if (dialogs::open_file("Images (*.png;*.ico;*.jpg)\0*.png;*.ico;*.jpg\0All files (*.*)\0*.*\0", "Choose an icon image", &picked))
			std::snprintf(g_icon, sizeof(g_icon), "%s", picked.c_str());
	}
	y += 60.0f;

	draw_field("##g_version", "Version label (optional)", rect(c.min.x, y, c.min.x + 220.0f, y + 50.0f), g_version, sizeof(g_version));
	y += 66.0f;

	if (!g_modal_error.empty())
	{
		draw_banner(rect(c.min.x, y, c.max.x, y + 40.0f), Level::Danger, g_modal_error.c_str());
		y += 50.0f;
	}

	const Rect save_rect = rect(c.max.x - 160.0f, c.max.y - 46.0f, c.max.x, c.max.y);
	if (primary_button("##save_game", save_rect, g_modal_is_edit ? "Save changes" : "Add game", true, false))
		save_modal();

	const Rect cancel_rect = rect(c.max.x - 250.0f, c.max.y - 46.0f, c.max.x - 170.0f, c.max.y);
	if (ghost_button("##cancel_game", cancel_rect, "Cancel"))
		g_modal_open = false;

	if (modal.close_requested)
		g_modal_open = false;
}

void draw_delete_confirm()
{
	Modal modal = modal_frame("##delete_modal", "Remove game", 440.0f, 190.0f);
	const Rect c = modal.content;

	text(F.body, 14.0f, ImVec2(c.min.x, c.min.y), P.text,
		("Remove \"" + g_pending_delete_name + "\" from your library?").c_str());
	text(F.link, 12.0f, ImVec2(c.min.x, c.min.y + 26.0f), P.text_dim, "This only removes the entry here - the game files are untouched.");

	const Rect delete_rect = rect(c.max.x - 150.0f, c.max.y - 46.0f, c.max.x, c.max.y);
	if (danger_button("##confirm_delete", delete_rect, "Remove"))
	{
		games::Library::instance().remove(g_pending_delete_id);
		games::Library::instance().save();
		logs::warning("Removed game entry: " + g_pending_delete_name);
		push_toast(Level::Info, "Removed from library");
		g_pending_delete_id.clear();
	}

	const Rect cancel_rect = rect(c.max.x - 250.0f, c.max.y - 46.0f, c.max.x - 170.0f, c.max.y);
	if (ghost_button("##cancel_delete", cancel_rect, "Cancel") || modal.close_requested)
		g_pending_delete_id.clear();
}

void draw_card(const Rect& r, games::GameEntry& entry)
{
	ImGui::PushID(entry.id.c_str());
	const bool valid = games::path_is_valid(entry);
	panel(r, 12.0f);

	if (entry.config.use_accent_override)
	{
		const ImU32 card_accent = ImGui::ColorConvertFloat4ToU32(
			ImVec4(entry.config.accent_override[0], entry.config.accent_override[1], entry.config.accent_override[2], 1.0f));
		box(rect(r.min.x, r.min.y, r.min.x + 4.0f, r.max.y), card_accent, 0.0f);
	}

	text(F.body, 15.0f, ImVec2(r.min.x + 16.0f, r.min.y + 12.0f), P.text, elide(F.body, 15.0f, entry.name, width(r) - 110.0f).c_str());
	text(F.tiny, 11.0f, ImVec2(r.min.x + 16.0f, r.min.y + 34.0f), P.text_dim, elide(F.tiny, 11.0f, entry.exe_path, width(r) - 32.0f).c_str());

	if (!valid)
		badge(rect(r.min.x + 16.0f, r.min.y + 54.0f, r.min.x + 90.0f, r.min.y + 74.0f), "MISSING", P.danger);
	else if (!entry.version.empty())
		badge(rect(r.min.x + 16.0f, r.min.y + 54.0f, r.min.x + 16.0f + text_width(F.micro, 10.0f, entry.version.c_str()) + 20.0f, r.min.y + 74.0f),
			entry.version.c_str(), P.info);

	const Rect fav_rect = rect(r.max.x - 32.0f, r.min.y + 10.0f, r.max.x - 10.0f, r.min.y + 32.0f);
	if (icon_button("##fav", fav_rect, [&](ImVec2 c, float s, ImU32 color) { icon_star(c, s, entry.favorite ? P.warning : color, entry.favorite); }))
	{
		entry.favorite = !entry.favorite;
		games::Library::instance().save();
	}

	const Rect edit_rect = rect(r.max.x - 60.0f, r.min.y + 10.0f, r.max.x - 38.0f, r.min.y + 32.0f);
	if (icon_button("##edit", edit_rect, [](ImVec2 c, float s, ImU32 color) { icon_edit(c, s, color); }))
		open_edit_modal(entry);

	const Rect config_rect = rect(r.min.x + 16.0f, r.max.y - 40.0f, r.min.x + 108.0f, r.max.y - 12.0f);
	if (ghost_button("##configure", config_rect, "Configure"))
	{
		S.selected_game_id = entry.id;
		Navigate(Screen::GameConfig);
	}

	const Rect delete_rect = rect(r.min.x + 116.0f, r.max.y - 40.0f, r.min.x + 148.0f, r.max.y - 12.0f);
	if (icon_button("##delete", delete_rect, [](ImVec2 c, float s, ImU32 color) { icon_trash(c, s, color); }))
	{
		g_pending_delete_id = entry.id;
		g_pending_delete_name = entry.name;
	}

	const Rect launch_rect = rect(r.max.x - 100.0f, r.max.y - 40.0f, r.max.x - 12.0f, r.max.y - 12.0f);
	if (primary_button("##launch", launch_rect, "Launch", valid, false))
	{
		std::string error;
		if (launcher::launch(entry.exe_path, entry.config.launch_arguments, entry.config.working_directory, &error))
		{
			games::Library::instance().touch_last_played(entry.id);
			games::Library::instance().save();
			logs::success("Launched " + entry.name);
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

void page_library()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = S.sidebar_collapsed ? layout::kSidebarWidthCollapsed : layout::kSidebarWidthExpanded;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "Games Library");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim, "Add, edit, launch and organize the games this loader knows about.");

	const float toolbar_y = area.min.y + 58.0f;
	const Rect search_rect = rect(area.min.x, toolbar_y, area.min.x + 260.0f, toolbar_y + 44.0f);
	draw_field("##lib_search", "Search", search_rect, g_search, sizeof(g_search), false, nullptr,
		[](ImVec2 c, float s, ImU32 color) { icon_search(c, s, color); });

	const char* sort_labels[3] = { "Name", "Recent", "Favorites" };
	const Rect sort_rect = rect(search_rect.max.x + 12.0f, toolbar_y, search_rect.max.x + 312.0f, toolbar_y + 44.0f);
	panel(sort_rect, 10.0f);
	g_sort = tabs("##lib_sort", sort_rect, sort_labels, 3, g_sort);

	const Rect add_rect = rect(area.max.x - 150.0f, toolbar_y, area.max.x, toolbar_y + 44.0f);
	if (primary_button("##add_game", add_rect, "Add game", true, false))
		open_add_modal();

	const Rect grid_rect = rect(area.min.x, toolbar_y + 60.0f, area.max.x, area.max.y);
	const std::vector<std::string> ids = visible_ids();

	if (games::Library::instance().entries().empty())
	{
		panel(grid_rect, 14.0f);
		empty_state(grid_rect, [](ImVec2 c, float s, ImU32 color) { icon_grid(c, s, color); },
			"Your library is empty", "Add a game to launch it from here");
	}
	else if (ids.empty())
	{
		panel(grid_rect, 14.0f);
		empty_state(grid_rect, [](ImVec2 c, float s, ImU32 color) { icon_search(c, s, color); }, "No matches", "Try a different search");
	}
	else
	{
		const float card_w = 280.0f;
		const float card_h = 150.0f;
		const float gap = 18.0f;
		const int columns = std::max(1, static_cast<int>((width(grid_rect) + gap) / (card_w + gap)));
		const int rows = (static_cast<int>(ids.size()) + columns - 1) / columns;
		const float content_h = static_cast<float>(rows) * (card_h + gap);

		begin_scroll("##lib_grid_scroll", grid_rect);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		for (size_t i = 0; i < ids.size(); ++i)
		{
			games::GameEntry* entry = games::Library::instance().find(ids[i]);
			if (!entry)
				continue;
			const int col = static_cast<int>(i) % columns;
			const int row = static_cast<int>(i) / columns;
			const Rect card = rect(
				origin.x + col * (card_w + gap), origin.y + row * (card_h + gap),
				origin.x + col * (card_w + gap) + card_w, origin.y + row * (card_h + gap) + card_h);
			draw_card(card, *entry);
		}
		ImGui::Dummy(ImVec2(1.0f, content_h));
		end_scroll();
	}

	if (g_modal_open)
		draw_add_edit_modal();
	if (!g_pending_delete_id.empty())
		draw_delete_confirm();
}

} // namespace ui
