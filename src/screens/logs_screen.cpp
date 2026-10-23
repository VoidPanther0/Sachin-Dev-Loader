#include "app.h"
#include "core/dialogs.h"
#include "core/logs.h"
#include "core/paths.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ui {

namespace {

char g_search[96] = {};
int g_level_filter = 0; // 0 all, 1 info, 2 success, 3 warning, 4 error
bool g_auto_scroll = true;

bool matches(const logs::Entry& entry)
{
	if (g_level_filter != 0)
	{
		const int wanted = g_level_filter - 1;
		if (static_cast<int>(entry.level) != wanted)
			return false;
	}
	if (g_search[0] == '\0')
		return true;

	std::string text = entry.text;
	std::string needle = g_search;
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text.find(needle) != std::string::npos;
}

ImU32 level_dot(logs::Level level)
{
	switch (level)
	{
	case logs::Level::Success: return P.success;
	case logs::Level::Warning: return P.warning;
	case logs::Level::Error: return P.danger;
	default: return P.info;
	}
}

} // namespace

void page_logs()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = S.sidebar_collapsed ? layout::kSidebarWidthCollapsed : layout::kSidebarWidthExpanded;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "Logs");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim, "Everything the loader has done this session.");

	const float toolbar_y = area.min.y + 58.0f;
	const Rect search_rect = rect(area.min.x, toolbar_y, area.min.x + 220.0f, toolbar_y + 44.0f);
	draw_field("##log_search", "Search", search_rect, g_search, sizeof(g_search), false, nullptr,
		[](ImVec2 c, float s, ImU32 color) { icon_search(c, s, color); });

	const char* level_labels[5] = { "All", "Info", "Success", "Warning", "Error" };
	const Rect level_rect = rect(search_rect.max.x + 12.0f, toolbar_y, search_rect.max.x + 452.0f, toolbar_y + 44.0f);
	panel(level_rect, 10.0f);
	g_level_filter = tabs("##log_level", level_rect, level_labels, 5, g_level_filter);

	const Rect autoscroll_rect = rect(level_rect.max.x + 16.0f, toolbar_y + 10.0f, level_rect.max.x + 56.0f, toolbar_y + 34.0f);
	toggle_switch("##log_autoscroll", autoscroll_rect, &g_auto_scroll);
	text(F.link, 12.0f, ImVec2(autoscroll_rect.max.x + 8.0f, toolbar_y + 14.0f), P.text_secondary, "Auto-scroll");

	const Rect export_rect = rect(area.max.x - 180.0f, toolbar_y, area.max.x - 96.0f, toolbar_y + 44.0f);
	if (ghost_button("##log_export", export_rect, "Export"))
	{
		std::string path;
		if (dialogs::save_file("Text file (*.txt)\0*.txt\0", "Export logs", "txt", &path))
		{
			std::string error;
			if (logs::Logger::instance().export_to_file(path, &error))
				push_toast(Level::Success, "Logs exported");
			else
				push_toast(Level::Danger, "Export failed: " + error);
		}
	}

	const Rect clear_rect = rect(area.max.x - 90.0f, toolbar_y, area.max.x, toolbar_y + 44.0f);
	if (danger_button("##log_clear", clear_rect, "Clear"))
		logs::Logger::instance().clear();

	const Rect list_rect = rect(area.min.x, toolbar_y + 60.0f, area.max.x, area.max.y);
	panel(list_rect, 14.0f);

	const std::vector<logs::Entry> all = logs::Logger::instance().snapshot();
	std::vector<const logs::Entry*> filtered;
	for (const logs::Entry& e : all)
		if (matches(e))
			filtered.push_back(&e);

	if (filtered.empty())
	{
		empty_state(list_rect, [](ImVec2 c, float s, ImU32 color) { icon_list(c, s, color); }, "No log entries", "Activity will appear here as you use the app");
		return;
	}

	const Rect scroll_rect = inset(list_rect, 14.0f);
	begin_scroll("##log_scroll", scroll_rect);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	float y = origin.y;
	for (const logs::Entry* entry : filtered)
	{
		const std::string stamp = logs::format_timestamp(entry->timestamp_ms);
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(origin.x + 6.0f, y + 8.0f), 3.0f, level_dot(entry->level));
		text(F.tiny, 11.0f, ImVec2(origin.x + 18.0f, y), P.text_dim, stamp.c_str());
		text(F.small_font, 12.0f, ImVec2(origin.x + 150.0f, y), P.text_secondary,
			elide(F.small_font, 12.0f, entry->text, width(scroll_rect) - 170.0f).c_str());
		y += 24.0f;
	}
	ImGui::Dummy(ImVec2(1.0f, y - origin.y + 8.0f));
	if (g_auto_scroll)
		ImGui::SetScrollHereY(1.0f);
	end_scroll();
}

} // namespace ui
