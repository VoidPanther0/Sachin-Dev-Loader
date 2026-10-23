#include "app.h"
#include "core/games.h"
#include "core/logs.h"
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

std::string g_loaded_for_id;
games::GameConfig g_draft;
char g_launch_args[256] = {};
char g_working_dir[260] = {};
char g_notes[1024] = {};
int g_tab = 0;

void load_draft(const games::GameEntry& entry)
{
	g_draft = entry.config;
	std::snprintf(g_launch_args, sizeof(g_launch_args), "%s", g_draft.launch_arguments.c_str());
	std::snprintf(g_working_dir, sizeof(g_working_dir), "%s", g_draft.working_directory.c_str());
	std::snprintf(g_notes, sizeof(g_notes), "%s", g_draft.notes.c_str());
	g_loaded_for_id = entry.id;
}

} // namespace

void page_game_config()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = S.sidebar_collapsed ? layout::kSidebarWidthCollapsed : layout::kSidebarWidthExpanded;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	games::GameEntry* entry = S.selected_game_id.empty() ? nullptr : games::Library::instance().find(S.selected_game_id);

	if (!entry)
	{
		panel(area, 14.0f);
		empty_state(area, [](ImVec2 c, float s, ImU32 color) { icon_chip(c, s, color); }, "No game selected", "Open the Library and choose Configure on a game");
		const Rect go_rect = rect(center(area).x - 80.0f, center(area).y + 50.0f, center(area).x + 80.0f, center(area).y + 92.0f);
		if (primary_button("##cfg_go_library", go_rect, "Open Library", true, true))
			Navigate(Screen::Library);
		return;
	}

	if (g_loaded_for_id != entry->id)
		load_draft(*entry);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "Game Configuration");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim, entry->name.c_str());

	const float toolbar_y = area.min.y + 58.0f;
	const char* tab_labels[3] = { "General", "Appearance", "Advanced" };
	const Rect tab_rect = rect(area.min.x, toolbar_y, area.min.x + 360.0f, toolbar_y + 44.0f);
	panel(tab_rect, 10.0f);
	g_tab = tabs("##cfg_tabs", tab_rect, tab_labels, 3, g_tab);

	const Rect body = rect(area.min.x, toolbar_y + 60.0f, area.max.x, area.max.y - 60.0f);
	panel(body, 14.0f);

	if (g_tab == 0)
	{
		text(F.micro, 10.0f, ImVec2(body.min.x + 18.0f, body.min.y + 14.0f), P.text_dim, "LAUNCH ARGUMENTS");
		draw_field("##cfg_args", "e.g. -windowed -skipintro", rect(body.min.x + 18.0f, body.min.y + 36.0f, body.max.x - 18.0f, body.min.y + 80.0f),
			g_launch_args, sizeof(g_launch_args));

		text(F.micro, 10.0f, ImVec2(body.min.x + 18.0f, body.min.y + 96.0f), P.text_dim, "WORKING DIRECTORY (optional)");
		draw_field("##cfg_dir", "Defaults to the executable's folder", rect(body.min.x + 18.0f, body.min.y + 118.0f, body.max.x - 18.0f, body.min.y + 162.0f),
			g_working_dir, sizeof(g_working_dir), false, nullptr, [](ImVec2 c, float s, ImU32 color) { icon_folder(c, s, color); });
	}
	else if (g_tab == 1)
	{
		text(F.micro, 10.0f, ImVec2(body.min.x + 18.0f, body.min.y + 14.0f), P.text_dim, "ACCENT COLOR OVERRIDE");
		const Rect toggle_rect = rect(body.min.x + 18.0f, body.min.y + 40.0f, body.min.x + 58.0f, body.min.y + 64.0f);
		toggle_switch("##cfg_override", toggle_rect, &g_draft.use_accent_override);
		text(F.link, 13.0f, ImVec2(toggle_rect.max.x + 12.0f, body.min.y + 44.0f), P.text_secondary,
			g_draft.use_accent_override ? "Using a custom accent for this game's cards" : "Using the app theme's accent color");

		if (g_draft.use_accent_override)
		{
			ImGui::SetCursorScreenPos(ImVec2(body.min.x + 18.0f, body.min.y + 80.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.08f, 0.14f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(P.border_strong));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
			ImGui::SetNextItemWidth(260.0f);
			ImGui::ColorEdit3("##cfg_color", g_draft.accent_override);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);

			const Rect preview = rect(body.min.x + 300.0f, body.min.y + 80.0f, body.min.x + 300.0f + 80.0f, body.min.y + 112.0f);
			box(preview, ImGui::ColorConvertFloat4ToU32(ImVec4(g_draft.accent_override[0], g_draft.accent_override[1], g_draft.accent_override[2], 1.0f)), 8.0f);
		}
	}
	else
	{
		text(F.micro, 10.0f, ImVec2(body.min.x + 18.0f, body.min.y + 14.0f), P.text_dim, "NOTES");
		ImGui::SetCursorScreenPos(ImVec2(body.min.x + 18.0f, body.min.y + 36.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.03f, 0.05f, 0.10f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(P.border_soft));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 12.0f));
		ImGui::PushFont(F.link);
		ImGui::InputTextMultiline("##cfg_notes", g_notes, sizeof(g_notes), ImVec2(width(body) - 36.0f, height(body) - 60.0f));
		ImGui::PopFont();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(2);
	}

	const Rect reset_rect = rect(area.max.x - 330.0f, area.max.y - 46.0f, area.max.x - 220.0f, area.max.y);
	if (ghost_button("##cfg_reset", reset_rect, "Reset"))
	{
		g_draft = games::GameConfig{};
		g_launch_args[0] = '\0';
		g_working_dir[0] = '\0';
		g_notes[0] = '\0';
	}

	const Rect reload_rect = rect(area.max.x - 210.0f, area.max.y - 46.0f, area.max.x - 100.0f, area.max.y);
	if (ghost_button("##cfg_reload", reload_rect, "Reload saved"))
		load_draft(*entry);

	const Rect save_rect = rect(area.max.x - 90.0f, area.max.y - 46.0f, area.max.x, area.max.y);
	if (primary_button("##cfg_save", save_rect, "Save", true, false))
	{
		g_draft.launch_arguments = g_launch_args;
		g_draft.working_directory = g_working_dir;
		g_draft.notes = g_notes;
		entry->config = g_draft;
		games::Library::instance().save();
		logs::info("Saved configuration for " + entry->name);
		push_toast(Level::Success, "Configuration saved");
	}
}

} // namespace ui
