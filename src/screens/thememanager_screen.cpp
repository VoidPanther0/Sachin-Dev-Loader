#include "app.h"
#include "core/dialogs.h"
#include "core/logs.h"
#include "core/theme.h"
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
theme::Theme g_draft;
char g_name_buf[64] = {};

bool g_duplicate_modal = false;
char g_duplicate_name[64] = {};

bool g_delete_modal = false;

void sync_name_buf() { std::snprintf(g_name_buf, sizeof(g_name_buf), "%s", g_draft.name.c_str()); }

void load_from(const theme::Theme& t)
{
	g_draft = t;
	sync_name_buf();
}

ImU32 c32(const theme::Color& c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a)); }

void color_row(const Rect& r, const char* label, theme::Color& color)
{
	text(F.link, 12.0f, ImVec2(r.min.x, r.min.y + 6.0f), P.text_secondary, label);
	ImGui::SetCursorScreenPos(ImVec2(r.min.x + 150.0f, r.min.y));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.08f, 0.14f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(P.border_strong));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));
	ImGui::PushID(label);
	ImGui::SetNextItemWidth(170.0f);
	float v[4] = { color.r, color.g, color.b, color.a };
	if (ImGui::ColorEdit4("##c", v, ImGuiColorEditFlags_AlphaBar))
	{
		color.r = v[0]; color.g = v[1]; color.b = v[2]; color.a = v[3];
	}
	ImGui::PopID();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
}

void slider_row(const Rect& r, const char* label, float* value, float lo, float hi)
{
	text(F.link, 12.0f, ImVec2(r.min.x, r.min.y + 6.0f), P.text_secondary, label);
	ImGui::SetCursorScreenPos(ImVec2(r.min.x + 150.0f, r.min.y));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.08f, 0.14f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::ColorConvertU32ToFloat4(P.accent));
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGui::ColorConvertU32ToFloat4(P.accent2));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));
	ImGui::PushID(label);
	ImGui::SetNextItemWidth(220.0f);
	ImGui::SliderFloat("##s", value, lo, hi, "%.2f");
	ImGui::PopID();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

void draw_preview(const Rect& r)
{
	const ImU32 bg_top = c32(g_draft.palette.window_bg_top);
	const ImU32 bg_bottom = c32(g_draft.palette.window_bg_bottom);
	const ImU32 panel_color = c32(g_draft.palette.panel_bg);
	const ImU32 accent = c32(g_draft.palette.accent);
	const ImU32 accent2 = c32(g_draft.palette.accent2);
	const ImU32 text_color = c32(g_draft.palette.text);
	const ImU32 text_secondary = c32(g_draft.palette.text_secondary);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilledMultiColor(r.min, r.max, bg_top, bg_top, bg_bottom, bg_bottom);
	draw->AddRect(r.min, r.max, c32(g_draft.palette.border), g_draft.style.rounding, 1.0f, 0);

	const Rect card = rect(r.min.x + 20.0f, r.min.y + 20.0f, r.min.x + 220.0f, r.min.y + 96.0f);
	box(card, panel_color, g_draft.style.rounding);
	text(F.small_font, 13.0f, ImVec2(card.min.x + 14.0f, card.min.y + 12.0f), text_color, "Sample Card");
	text(F.link, 11.0f, ImVec2(card.min.x + 14.0f, card.min.y + 32.0f), text_secondary, "Secondary text");

	const Rect btn = rect(card.min.x + 14.0f, card.max.y - 32.0f, card.min.x + 110.0f, card.max.y - 10.0f);
	gradient_round_rect(btn, g_draft.style.rounding * 0.7f, accent, accent2);
	text_center(F.link, 11.0f, btn, text_color, "Button");

	const Rect badge_rect = rect(card.max.x - 70.0f, card.max.y - 30.0f, card.max.x - 14.0f, card.max.y - 12.0f);
	box(badge_rect, colorutil::alpha(c32(g_draft.palette.success), 0.18f), (badge_rect.max.y - badge_rect.min.y) * 0.5f, colorutil::alpha(c32(g_draft.palette.success), 0.5f));
	text_center(F.micro, 9.0f, badge_rect, c32(g_draft.palette.success), "OK");
}

} // namespace

void page_theme_customizer()
{
	if (!g_initialized)
	{
		load_from(theme::Manager::instance().current());
		g_initialized = true;
	}

	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = layout::kSidebarWidth;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "Theme Customizer");
	text(F.link, 13.0f, ImVec2(area.min.x, area.min.y + 30.0f), P.text_dim, "Pick a theme, tune it, and save your own.");

	const float left_w = 260.0f;
	const Rect left = rect(area.min.x, area.min.y + 58.0f, area.min.x + left_w, area.max.y);
	const Rect right = rect(left.max.x + 20.0f, area.min.y + 58.0f, area.max.x, area.max.y);

	panel(left, 14.0f);
	text(F.micro, 10.0f, ImVec2(left.min.x + 16.0f, left.min.y + 14.0f), P.text_dim, "THEMES");

	const Rect theme_scroll = rect(left.min.x + 10.0f, left.min.y + 38.0f, left.max.x - 10.0f, left.max.y - 150.0f);
	begin_scroll("##theme_list_scroll", theme_scroll);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	float ty = origin.y;
	const std::string current_name = theme::Manager::instance().current().name;
	for (const theme::Theme& t : theme::Manager::instance().themes())
	{
		ImGui::PushID(t.name.c_str());
		const Rect row = rect(origin.x, ty, origin.x + width(theme_scroll) - 10.0f, ty + 46.0f);
		const bool active = t.name == current_name;
		box(row, active ? colorutil::alpha(c32(t.palette.accent), 0.20f) : colorutil::alpha(P.text, 0.03f), 10.0f,
			active ? c32(t.palette.accent) : P.border_soft, active ? 1.4f : 1.0f);
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(row.min.x + 18.0f, center(row).y), 8.0f, c32(t.palette.accent), 16);
		text(F.small_font, 13.0f, ImVec2(row.min.x + 36.0f, row.min.y + 8.0f), P.text, t.name.c_str());
		text(F.tiny, 10.0f, ImVec2(row.min.x + 36.0f, row.min.y + 26.0f), P.text_dim, t.builtin ? "Built-in" : "Custom");

		ImGui::SetCursorScreenPos(row.min);
		if (ImGui::InvisibleButton("##pick", ImVec2(width(row), height(row))))
		{
			theme::Manager::instance().set_current(t.name);
			load_from(t);
			push_toast(Level::Info, std::string("Theme: ") + t.name);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		ty += 54.0f;
		ImGui::PopID();
	}
	ImGui::Dummy(ImVec2(1.0f, ty - origin.y));
	end_scroll();

	const float action_y = left.max.y - 140.0f;
	const float btn_w = (left_w - 32.0f - 10.0f) * 0.5f;
	if (ghost_button("##theme_duplicate", rect(left.min.x + 16.0f, action_y, left.min.x + 16.0f + btn_w, action_y + 36.0f), "Duplicate"))
	{
		std::snprintf(g_duplicate_name, sizeof(g_duplicate_name), "%s copy", g_draft.name.c_str());
		g_duplicate_modal = true;
	}
	if (danger_button("##theme_delete", rect(left.min.x + 16.0f + btn_w + 10.0f, action_y, left.max.x - 16.0f, action_y + 36.0f), "Delete", !g_draft.builtin))
		g_delete_modal = true;

	if (ghost_button("##theme_import", rect(left.min.x + 16.0f, action_y + 44.0f, left.min.x + 16.0f + btn_w, action_y + 80.0f), "Import"))
	{
		std::string path;
		if (dialogs::open_file("Theme files (*.json)\0*.json\0", "Import a theme", &path))
		{
			std::string error;
			if (theme::Manager::instance().import_file(path, &error))
				push_toast(Level::Success, "Theme imported");
			else
				push_toast(Level::Danger, "Import failed: " + error);
		}
	}
	if (ghost_button("##theme_export", rect(left.min.x + 16.0f + btn_w + 10.0f, action_y + 44.0f, left.max.x - 16.0f, action_y + 80.0f), "Export"))
	{
		std::string path;
		if (dialogs::save_file("Theme files (*.json)\0*.json\0", "Export this theme", "json", &path))
		{
			std::string error;
			if (theme::Manager::instance().export_file(g_draft.name, path, &error))
				push_toast(Level::Success, "Theme exported");
			else
				push_toast(Level::Danger, "Export failed: " + error);
		}
	}
	if (ghost_button("##theme_reset", rect(left.min.x + 16.0f, action_y + 88.0f, left.max.x - 16.0f, action_y + 124.0f), "Reset to defaults"))
	{
		theme::Manager::instance().reset_to_default();
		load_from(theme::Manager::instance().current());
	}

	// Right: preview + editor.
	panel(rect(right.min.x, right.min.y, right.max.x, right.min.y + 150.0f), 14.0f);
	draw_preview(inset(rect(right.min.x, right.min.y, right.max.x, right.min.y + 150.0f), 10.0f));

	const Rect editor = rect(right.min.x, right.min.y + 166.0f, right.max.x, right.max.y);
	panel(editor, 14.0f);

	draw_field("##theme_name", "Theme name", rect(editor.min.x + 16.0f, editor.min.y + 14.0f, editor.min.x + 280.0f, editor.min.y + 58.0f), g_name_buf, sizeof(g_name_buf));
	g_draft.name = g_name_buf;

	const Rect save_rect = rect(editor.max.x - 140.0f, editor.min.y + 16.0f, editor.max.x - 16.0f, editor.min.y + 54.0f);
	if (primary_button("##theme_save", save_rect, "Save theme", true, false))
	{
		std::string error;
		if (g_draft.name.empty())
		{
			push_toast(Level::Danger, "Give this theme a name first");
		}
		else if (theme::Manager::instance().save_custom(g_draft, &error))
		{
			g_draft.builtin = false;
			theme::Manager::instance().set_current(g_draft.name);
			logs::info("Saved theme: " + g_draft.name);
			push_toast(Level::Success, "Theme saved and applied");
		}
		else
		{
			push_toast(Level::Danger, error);
		}
	}

	const float col1_x = editor.min.x + 16.0f;
	const float col2_x = editor.min.x + width(editor) * 0.5f + 8.0f;
	float y1 = editor.min.y + 76.0f;
	float y2 = editor.min.y + 76.0f;
	const float row_h = 30.0f;

	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Accent", g_draft.palette.accent); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Accent 2", g_draft.palette.accent2); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Window background", g_draft.palette.window_bg_top); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Panel background", g_draft.palette.panel_bg); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Border", g_draft.palette.border); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Text", g_draft.palette.text); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Secondary text", g_draft.palette.text_secondary); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Success", g_draft.palette.success); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Warning", g_draft.palette.warning); y1 += row_h + 6.0f;
	color_row(rect(col1_x, y1, col1_x + 300.0f, y1 + row_h), "Danger", g_draft.palette.danger); y1 += row_h + 6.0f;

	slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Corner rounding", &g_draft.style.rounding, 0.0f, 20.0f); y2 += row_h + 6.0f;
	slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Widget spacing", &g_draft.style.spacing, 2.0f, 28.0f); y2 += row_h + 6.0f;
	slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Window padding", &g_draft.style.window_padding, 8.0f, 48.0f); y2 += row_h + 6.0f;
	slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Font size / UI scale", &g_draft.style.font_scale, 0.85f, 1.35f); y2 += row_h + 6.0f;
	slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Animation intensity", &g_draft.style.anim_intensity, 0.0f, 2.0f); y2 += row_h + 6.0f;

	text(F.link, 12.0f, ImVec2(col2_x, y2 + 6.0f), P.text_secondary, "Glow effects");
	toggle_switch("##theme_glow", rect(col2_x + 150.0f, y2, col2_x + 190.0f, y2 + 24.0f), &g_draft.style.glow_enabled);
	y2 += row_h + 6.0f;
	if (g_draft.style.glow_enabled)
		slider_row(rect(col2_x, y2, col2_x + 320.0f, y2 + row_h), "Glow intensity", &g_draft.style.glow_intensity, 0.0f, 2.0f);

	if (g_duplicate_modal)
	{
		Modal modal = modal_frame("##dup_theme", "Duplicate theme", 420.0f, 170.0f);
		draw_field("##dup_name", "New theme name", rect(modal.content.min.x, modal.content.min.y, modal.content.max.x, modal.content.min.y + 44.0f), g_duplicate_name, sizeof(g_duplicate_name));
		const Rect confirm_rect = rect(modal.content.max.x - 150.0f, modal.content.max.y - 46.0f, modal.content.max.x, modal.content.max.y);
		if (primary_button("##dup_confirm", confirm_rect, "Duplicate", true, false))
		{
			if (theme::Manager::instance().duplicate(g_draft.name, g_duplicate_name))
			{
				load_from(*theme::Manager::instance().find(g_duplicate_name));
				push_toast(Level::Success, "Theme duplicated");
				g_duplicate_modal = false;
			}
			else
			{
				push_toast(Level::Danger, "Could not duplicate - check the name is unique");
			}
		}
		const Rect cancel_rect = rect(modal.content.max.x - 250.0f, modal.content.max.y - 46.0f, modal.content.max.x - 170.0f, modal.content.max.y);
		if (ghost_button("##dup_cancel", cancel_rect, "Cancel") || modal.close_requested)
			g_duplicate_modal = false;
	}

	if (g_delete_modal)
	{
		Modal modal = modal_frame("##del_theme", "Delete theme", 420.0f, 160.0f);
		text(F.body, 14.0f, ImVec2(modal.content.min.x, modal.content.min.y), P.text, ("Delete \"" + g_draft.name + "\"?").c_str());
		text(F.link, 12.0f, ImVec2(modal.content.min.x, modal.content.min.y + 26.0f), P.text_dim, "This cannot be undone.");
		const Rect confirm_rect = rect(modal.content.max.x - 150.0f, modal.content.max.y - 46.0f, modal.content.max.x, modal.content.max.y);
		if (danger_button("##del_confirm", confirm_rect, "Delete"))
		{
			theme::Manager::instance().delete_custom(g_draft.name);
			load_from(theme::Manager::instance().current());
			g_delete_modal = false;
			push_toast(Level::Info, "Theme deleted");
		}
		const Rect cancel_rect = rect(modal.content.max.x - 250.0f, modal.content.max.y - 46.0f, modal.content.max.x - 170.0f, modal.content.max.y);
		if (ghost_button("##del_cancel", cancel_rect, "Cancel") || modal.close_requested)
			g_delete_modal = false;
	}
}

} // namespace ui
