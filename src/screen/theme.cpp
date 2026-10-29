#include "common/dashboard_common.h"

#include <cmath>

// Theme: dark or light palette, accent color presets, a custom picker and the
// effects that low-end machines can switch off. Changes apply live and are
// saved to settings.ini. Reset asks for confirmation first.

namespace dash {

using namespace ui;

namespace {

bool is_preset(const AccentPreset& preset)
{
	return std::fabs(state.accent[0] - preset.r) < 0.01f
		&& std::fabs(state.accent[1] - preset.g) < 0.01f
		&& std::fabs(state.accent[2] - preset.b) < 0.01f;
}

// One selectable theme mode tile: Dark or Light.
bool mode_tile(const Rect& r, const char* label, const char* hint, bool active, bool light_preview)
{
	ImGui::SetCursorScreenPos(r.min);
	ImGui::PushID(label);
	const bool clicked = ImGui::InvisibleButton("##mode", ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	// Miniature of the palette this tile selects.
	const ImVec4 face = light_preview ? ImVec4(0.929f, 0.941f, 0.961f, 1.0f) : ImVec4(0.043f, 0.047f, 0.059f, 1.0f);
	const ImVec4 soft = light_preview ? ImVec4(0.847f, 0.863f, 0.902f, 1.0f) : ImVec4(0.129f, 0.141f, 0.165f, 1.0f);
	const ImVec4 accent = accent_color();

	const Rect preview = rect(r.min.x + 14.0f, r.min.y + 14.0f, r.max.x - 14.0f, r.min.y + 64.0f);
	box(preview, face, 8.0f, active ? &accent : &kLine);
	box(rect(preview.min.x + 10.0f, preview.min.y + 10.0f, preview.min.x + 44.0f, preview.max.y - 10.0f), soft, 5.0f);
	box(rect(preview.min.x + 54.0f, preview.min.y + 10.0f, preview.max.x - 10.0f, preview.min.y + 22.0f), soft, 4.0f);
	box(rect(preview.min.x + 54.0f, preview.min.y + 28.0f, preview.max.x - 10.0f, preview.max.y - 10.0f),
		fade(accent, 0.55f), 4.0f);

	text(font.semibold, 14.0f, ImVec2(r.min.x + 14.0f, r.min.y + 76.0f), active ? kText : kMuted, label);
	text(font.caption, 10.0f, ImVec2(r.min.x + 14.0f, r.min.y + 96.0f), kDim, hint);
	return clicked;
}

void draw_mode_card(const Rect& card)
{
	card_frame(card, "Appearance", "Dark is easy on the eyes; light is brighter for daylight.");

	const float tile_w = (width(card) - 28.0f * 2.0f - 16.0f) * 0.5f;
	const Rect dark = rect(card.min.x + 28.0f, card.min.y + 76.0f, card.min.x + 28.0f + tile_w, card.min.y + 196.0f);
	const Rect light = rect(dark.max.x + 16.0f, dark.min.y, dark.max.x + 16.0f + tile_w, dark.max.y);

	if (mode_tile(dark, "Dark", "Default theme", !shared.light, false))
	{
		apply_theme(false);
		save_preferences();
	}
	if (mode_tile(light, "Light", "High contrast", shared.light, true))
	{
		apply_theme(true);
		save_preferences();
	}

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 216.0f);

	// Card look: borders and the soft background glow can be switched off on
	// weak machines; the options say what they do.
	toggle_row("##card_borders", rect(card.min.x + 20.0f, card.min.y + 228.0f, card.max.x - 20.0f, card.min.y + 284.0f),
		"Card borders", "Thin outlines around panels", &shared.card_borders);
	toggle_row("##background_glow", rect(card.min.x + 20.0f, card.min.y + 286.0f, card.max.x - 20.0f, card.min.y + 342.0f),
		"Background glow", "Two soft color shapes behind the pages", &shared.background_glow);
	toggle_row("##animations", rect(card.min.x + 20.0f, card.min.y + 344.0f, card.max.x - 20.0f, card.min.y + 400.0f),
		"Animations", "Hover easing and small motion", &shared.animations);
	toggle_row("##reduced_effects", rect(card.min.x + 20.0f, card.min.y + 402.0f, card.max.x - 20.0f, card.min.y + 458.0f),
		"Reduced effects", "Lowest drawing cost for slow PCs", &shared.reduced_effects);
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

	// Reset asks for confirmation so a mis-click cannot throw the look away.
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.max.y - 58.0f));
	if (secondary_button("Reset to default", ImVec2(170.0f, 36.0f)))
		ImGui::OpenPopup("##reset_theme");

	if (changed)
		save_preferences();

	if (confirm_modal("##reset_theme", "Reset appearance?",
		"Theme, accent color and effects return to the defaults. Your apps and account stay untouched.",
		"Reset appearance"))
	{
		reset_preferences();
		toast("Appearance reset to defaults");
	}
}

void draw_preview_card(const Rect& card)
{
	card_frame(card, "Preview", "How the accent looks on common elements.");

	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec4 accent = accent_color();

	// Sample navigation item.
	const Rect nav = rect(card.min.x + 28.0f, card.min.y + 72.0f, card.max.x - 28.0f, card.min.y + 112.0f);
	box(nav, fade(accent, 0.14f), 10.0f);
	draw->AddRect(nav.min, nav.max, color_u32(fade(accent, 0.35f)), 10.0f, 0, 1.0f);
	draw->AddRectFilled(ImVec2(nav.min.x + 1.0f, nav.min.y + 10.0f), ImVec2(nav.min.x + 3.0f, nav.max.y - 10.0f),
		color_u32(accent), 2.0f);
	icon_home(ImVec2(nav.min.x + 28.0f, nav.min.y + 20.0f), color_u32(lift(accent, 0.15f)));
	text(font.semibold, 14.0f, ImVec2(nav.min.x + 48.0f, nav.min.y + 11.0f), kText, "Active page");

	// Pills.
	pill(rect(card.min.x + 28.0f, card.min.y + 122.0f, card.min.x + 124.0f, card.min.y + 144.0f), "ACCENT", accent);
	pill(rect(card.min.x + 134.0f, card.min.y + 122.0f, card.min.x + 224.0f, card.min.y + 144.0f), "ONLINE", kGreen);

	// Progress.
	text(font.caption, 10.0f, ImVec2(card.min.x + 180.0f, card.min.y + 154.0f), kDim, "PROGRESS");
	progress_bar(rect(card.min.x + 28.0f, card.min.y + 172.0f, card.max.x - 28.0f, card.min.y + 180.0f), 0.62f, accent);

	// Buttons: one gradient primary and one secondary.
	primary_button("##preview_primary", rect(card.min.x + 28.0f, card.min.y + 192.0f,
		card.min.x + 198.0f, card.min.y + 226.0f), "Primary", icon_play);
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 214.0f, card.min.y + 194.0f));
	secondary_button("Secondary", ImVec2(130.0f, 30.0f));
}

} // namespace

void DrawTheme()
{
	// Left: appearance (mode tiles + effects). Right: accent on top with the
	// reset button at its bottom, preview below with room for its buttons.
	draw_mode_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_accent_card(rect(706.0f, kContentTop, kContentRight, 400.0f));
	draw_preview_card(rect(706.0f, 416.0f, kContentRight, kContentBottom));
}

} // namespace dash
