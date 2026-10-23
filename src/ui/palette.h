#pragma once

#include "imgui.h"

namespace ui {

// Runtime, ImU32-flavored mirror of the active theme::Theme. Refreshed once
// per frame; every screen and widget reads from this instead of hardcoded
// colors so a theme change updates the whole app immediately.
struct Palette
{
	ImU32 bg_top = 0;
	ImU32 bg_bottom = 0;
	ImU32 panel = 0;
	ImU32 field = 0;
	ImU32 border_soft = 0;
	ImU32 border_strong = 0;
	ImU32 text = 0;
	ImU32 text_secondary = 0;
	ImU32 text_dim = 0;
	ImU32 accent = 0;
	ImU32 accent2 = 0;
	ImU32 button = 0;
	ImU32 button_hover = 0;
	ImU32 button_active = 0;
	ImU32 success = 0;
	ImU32 warning = 0;
	ImU32 danger = 0;
	ImU32 info = 0;

	float rounding = 11.0f;
	float spacing = 12.0f;
	float window_padding = 24.0f;
	float font_scale = 1.0f;
	float anim_intensity = 1.0f;
	bool glow_enabled = true;
	float glow_intensity = 1.0f;
	bool is_light = false;
};

extern Palette P;

void RefreshPalette();

} // namespace ui
