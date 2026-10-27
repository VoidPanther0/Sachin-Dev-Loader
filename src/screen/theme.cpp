#include "common/dashboard_common.h"

#include <cmath>

// Theme: accent color presets, a custom picker and a live preview. Changes
// apply at once and are saved to settings.ini.

namespace dash {

using namespace ui;

namespace {

bool is_preset(const AccentPreset& preset)
{
	return std::fabs(state.accent[0] - preset.r) < 0.01f
		&& std::fabs(state.accent[1] - preset.g) < 0.01f
		&& std::fabs(state.accent[2] - preset.b) < 0.01f;
}

void draw_accent_card(const Rect& card)
{
	card_frame(card, "Accent color", "Pick a preset or mix your own. Saved automatically.");
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 84.0f), kDim, "PRESETS");

	ImDrawList* draw = ImGui::GetWindowDrawList();
	bool changed = false;
	float x = card.min.x + 44.0f;
	const float y = card.min.y + 126.0f;

	for (int i = 0; i < kAccentPresetCount; ++i)
	{
		const AccentPreset& preset = kAccentPresets[i];
		const ImVec4 color(preset.r, preset.g, preset.b, 1.0f);
		const bool active = is_preset(preset);

		ImGui::SetCursorScreenPos(ImVec2(x - 18.0f, y - 18.0f));
		ImGui::PushID(preset.name);
		const bool clicked = ImGui::InvisibleButton("##swatch", ImVec2(36.0f, 36.0f));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::SetTooltip("%s", preset.name);
		}

		if (active || hovered)
			draw->AddCircle(ImVec2(x, y), 18.0f, color_u32(active ? color : kLineBright), 32, 2.0f);
		draw->AddCircleFilled(ImVec2(x, y), 13.0f, color_u32(color), 32);

		if (clicked)
		{
			state.accent[0] = preset.r;
			state.accent[1] = preset.g;
			state.accent[2] = preset.b;
			changed = true;
		}
		x += 52.0f;
	}

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 172.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 190.0f), kDim, "CUSTOM COLOR");

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 214.0f));
	ImGui::SetNextItemWidth(width(card) - 56.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
	if (ImGui::ColorEdit3("##accent_custom", state.accent, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel))
		changed = true;
	ImGui::PopStyleVar();

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.max.y - 58.0f));
	if (secondary_button("Reset to default", ImVec2(160.0f, 34.0f)))
	{
		reset_preferences();
		return;
	}

	if (changed)
		save_preferences();
}

void draw_preview_card(const Rect& card)
{
	card_frame(card, "Preview", "How the accent looks on common elements.");

	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec4 accent = accent_color();

	// Sample navigation item.
	const Rect nav = rect(card.min.x + 28.0f, card.min.y + 84.0f, card.max.x - 28.0f, card.min.y + 128.0f);
	box(nav, fade(accent, 0.14f), 10.0f);
	draw->AddRect(nav.min, nav.max, color_u32(fade(accent, 0.35f)), 10.0f, 0, 1.0f);
	draw->AddRectFilled(ImVec2(nav.min.x + 1.0f, nav.min.y + 11.0f), ImVec2(nav.min.x + 3.0f, nav.max.y - 11.0f),
		color_u32(accent), 2.0f);
	icon_home(ImVec2(nav.min.x + 28.0f, nav.min.y + 22.0f), color_u32(lift(accent, 0.15f)));
	text(font.semibold, 14.0f, ImVec2(nav.min.x + 48.0f, nav.min.y + 13.0f), kText, "Active page");

	// Pills.
	pill(rect(card.min.x + 28.0f, card.min.y + 146.0f, card.min.x + 124.0f, card.min.y + 168.0f), "ACCENT", accent);
	pill(rect(card.min.x + 134.0f, card.min.y + 146.0f, card.min.x + 224.0f, card.min.y + 168.0f), "ONLINE", kGreen);

	// Progress.
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 190.0f), kDim, "PROGRESS");
	progress_bar(rect(card.min.x + 28.0f, card.min.y + 212.0f, card.max.x - 28.0f, card.min.y + 220.0f), 0.62f, accent);

	// Button.
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 244.0f));
	ImGui::Button("Primary button", ImVec2(160.0f, 36.0f));
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 200.0f, card.min.y + 244.0f));
	secondary_button("Secondary", ImVec2(130.0f, 36.0f));
}

} // namespace

void DrawTheme()
{
	draw_page_header("Theme", "Make the loader yours. Changes apply live.");

	draw_accent_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_preview_card(rect(706.0f, kContentTop, kContentRight, kContentBottom));
}

} // namespace dash
