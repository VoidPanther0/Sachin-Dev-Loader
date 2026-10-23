#include "app.h"
#include "core/dialogs.h"
#include "core/dllmgr.h"
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

char g_dll_path[260] = {};
dllmgr::DllInfo g_last_info;
bool g_has_result = false;

char g_target_name[96] = {};
char g_target_exe[96] = {};
char g_target_notes[160] = {};

std::string g_pending_remove_target;

} // namespace

void page_dll_management()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = S.sidebar_collapsed ? layout::kSidebarWidthCollapsed : layout::kSidebarWidthExpanded;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "DLL Management");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim,
		"For your own applications and authorized test environments only.");

	const float col_gap = 20.0f;
	const float left_w = width(area) * 0.56f;
	const Rect left = rect(area.min.x, area.min.y + 58.0f, area.min.x + left_w, area.max.y);
	const Rect right = rect(left.max.x + col_gap, area.min.y + 58.0f, area.max.x, area.max.y);

	// ---- Left: file validation ----
	panel(rect(left.min.x, left.min.y, left.max.x, left.min.y + 190.0f), 14.0f);
	text(F.body, 14.0f, ImVec2(left.min.x + 18.0f, left.min.y + 16.0f), P.text, "Validate a DLL file");

	const Rect path_field = rect(left.min.x + 18.0f, left.min.y + 48.0f, left.max.x - 100.0f, left.min.y + 98.0f);
	draw_field("##dll_path", "Path to a .dll file", path_field, g_dll_path, sizeof(g_dll_path), false, nullptr,
		[](ImVec2 c, float s, ImU32 color) { icon_chip(c, s, color); });

	if (ghost_button("##dll_browse", rect(left.max.x - 86.0f, left.min.y + 51.0f, left.max.x - 18.0f, left.min.y + 95.0f), "Browse"))
	{
		std::string picked;
		if (dialogs::open_file("DLL files (*.dll)\0*.dll\0All files (*.*)\0*.*\0", "Choose a DLL file", &picked))
			std::snprintf(g_dll_path, sizeof(g_dll_path), "%s", picked.c_str());
	}

	if (primary_button("##dll_validate", rect(left.min.x + 18.0f, left.min.y + 112.0f, left.min.x + 178.0f, left.min.y + 156.0f), "Validate", true, false))
	{
		g_last_info = dllmgr::inspect(g_dll_path);
		g_has_result = true;
		logs::info(std::string("Inspected DLL: ") + g_dll_path);
	}

	if (g_has_result)
	{
		const char* status = !g_last_info.exists ? "File not found"
			: !g_last_info.is_valid_pe ? "Not a valid PE file"
			: !g_last_info.is_dll ? "Valid PE, but not a DLL"
			: "Looks like a valid DLL";
		const Level level = (!g_last_info.exists || !g_last_info.is_valid_pe) ? Level::Danger
			: !g_last_info.is_dll ? Level::Warning : Level::Success;
		draw_banner(rect(left.min.x + 190.0f, left.min.y + 112.0f, left.max.x - 18.0f, left.min.y + 156.0f), level, status);
	}

	float detail_y = left.min.y + 210.0f;
	const Rect detail_card = rect(left.min.x, detail_y, left.max.x, detail_y + 150.0f);
	panel(detail_card, 14.0f);
	text(F.micro, 10.0f, ImVec2(left.min.x + 18.0f, detail_y + 14.0f), P.text_dim, "FILE DETAILS");
	if (g_has_result && g_last_info.exists)
	{
		char size_buf[48];
		std::snprintf(size_buf, sizeof(size_buf), "%.1f KB", g_last_info.size_bytes / 1024.0);
		status_row(detail_card, detail_y + 44.0f, "ARCHITECTURE", g_last_info.architecture.empty() ? "unknown" : g_last_info.architecture.c_str());
		status_row(detail_card, detail_y + 76.0f, "SIZE", size_buf);
		status_row(detail_card, detail_y + 108.0f, "VALID PE", g_last_info.is_valid_pe ? "yes" : "no", g_last_info.is_valid_pe ? P.success : P.danger);
	}
	else
	{
		text_center(F.link, 12.0f, rect(left.min.x, detail_y + 30.0f, left.max.x, detail_y + 150.0f), P.text_dim, "Validate a file to see details");
	}

	// ---- Live injection notice ----
	const Rect notice = rect(left.min.x, detail_y + 170.0f, left.max.x, left.max.y);
	panel(notice, 14.0f);
	icon_warning_sign(ImVec2(notice.min.x + 30.0f, notice.min.y + 28.0f), 22.0f, P.warning);
	text(F.small_font, 13.0f, ImVec2(notice.min.x + 56.0f, notice.min.y + 14.0f), P.text, "Live injection is not enabled in this build");
	text(F.link, 12.0f, ImVec2(notice.min.x + 56.0f, notice.min.y + 34.0f), P.text_secondary,
		"This screen validates files and keeps a reference list only.");
	text(F.link, 12.0f, ImVec2(notice.min.x + 56.0f, notice.min.y + 52.0f), P.text_secondary,
		"Safe alternative: call LoadLibrary for this DLL from inside your own app's startup code.");

	// ---- Right: authorized targets ----
	panel(rect(right.min.x, right.min.y, right.max.x, right.min.y + 150.0f), 14.0f);
	text(F.body, 14.0f, ImVec2(right.min.x + 18.0f, right.min.y + 14.0f), P.text, "Add a target application");
	draw_field("##tgt_name", "Display name", rect(right.min.x + 18.0f, right.min.y + 46.0f, right.max.x - 18.0f, right.min.y + 86.0f),
		g_target_name, sizeof(g_target_name));
	draw_field("##tgt_exe", "Process name, e.g. MyApp.exe", rect(right.min.x + 18.0f, right.min.y + 92.0f, right.max.x - 110.0f, right.min.y + 132.0f),
		g_target_exe, sizeof(g_target_exe));

	if (primary_button("##tgt_add", rect(right.max.x - 94.0f, right.min.y + 92.0f, right.max.x - 18.0f, right.min.y + 132.0f), "Add", true, false))
	{
		if (g_target_name[0] != '\0' && g_target_exe[0] != '\0')
		{
			dllmgr::TargetRegistry::instance().add(g_target_name, g_target_exe, g_target_notes);
			dllmgr::TargetRegistry::instance().save();
			logs::info(std::string("Added authorized target: ") + g_target_name);
			g_target_name[0] = g_target_exe[0] = g_target_notes[0] = '\0';
			push_toast(Level::Success, "Target added");
		}
	}

	const Rect list_rect = rect(right.min.x, right.min.y + 166.0f, right.max.x, right.max.y);
	panel(list_rect, 14.0f);
	text(F.micro, 10.0f, ImVec2(list_rect.min.x + 18.0f, list_rect.min.y + 14.0f), P.text_dim, "YOUR AUTHORIZED TARGETS");

	const std::vector<dllmgr::Target>& targets = dllmgr::TargetRegistry::instance().targets();
	if (targets.empty())
	{
		empty_state(list_rect, [](ImVec2 c, float s, ImU32 color) { icon_chip(c, s, color); }, "No targets yet", "Add the apps you plan to work with");
	}
	else
	{
		const Rect scroll_rect = rect(list_rect.min.x, list_rect.min.y + 40.0f, list_rect.max.x, list_rect.max.y - 10.0f);
		begin_scroll("##tgt_scroll", scroll_rect);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		float ty = origin.y;
		for (const dllmgr::Target& target : targets)
		{
			ImGui::PushID(target.id.c_str());
			const Rect row = rect(origin.x, ty, origin.x + width(scroll_rect) - 14.0f, ty + 56.0f);
			box(row, colorutil::alpha(P.text, 0.03f), 10.0f, P.border_soft, 1.0f);
			text(F.small_font, 13.0f, ImVec2(row.min.x + 14.0f, row.min.y + 8.0f), P.text, target.display_name.c_str());
			text(F.tiny, 11.0f, ImVec2(row.min.x + 14.0f, row.min.y + 30.0f), P.text_dim, target.exe_name.c_str());

			if (icon_button("##remove_target", rect(row.max.x - 36.0f, row.min.y + 16.0f, row.max.x - 10.0f, row.min.y + 40.0f),
				[](ImVec2 c, float s, ImU32 color) { icon_trash(c, s, color); }))
			{
				g_pending_remove_target = target.id;
			}

			ty += 64.0f;
			ImGui::PopID();
		}
		ImGui::Dummy(ImVec2(1.0f, ty - origin.y));
		end_scroll();
	}

	if (!g_pending_remove_target.empty())
	{
		for (const dllmgr::Target& t : dllmgr::TargetRegistry::instance().targets())
		{
			if (t.id == g_pending_remove_target)
			{
				logs::warning("Removed authorized target: " + t.display_name);
				break;
			}
		}
		dllmgr::TargetRegistry::instance().remove(g_pending_remove_target);
		dllmgr::TargetRegistry::instance().save();
		g_pending_remove_target.clear();
	}
}

} // namespace ui
