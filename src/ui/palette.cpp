#include "palette.h"

#include "colorutil.h"
#include "core/theme.h"

namespace ui {

Palette P;

namespace {

ImU32 to_u32(const theme::Color& c)
{
	return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
}

bool looks_light(const theme::Color& bg)
{
	return (bg.r + bg.g + bg.b) / 3.0f > 0.5f;
}

} // namespace

void RefreshPalette()
{
	const theme::Theme& t = theme::Manager::instance().current();
	const theme::Palette& src = t.palette;

	P.bg_top = to_u32(src.window_bg_top);
	P.bg_bottom = to_u32(src.window_bg_bottom);
	P.panel = to_u32(src.panel_bg);
	P.field = to_u32(src.field_bg);
	P.border_strong = to_u32(src.border);
	P.text = to_u32(src.text);
	P.text_secondary = to_u32(src.text_secondary);
	P.accent = to_u32(src.accent);
	P.accent2 = to_u32(src.accent2);
	P.button = to_u32(src.button);
	P.button_hover = to_u32(src.button_hover);
	P.button_active = to_u32(src.button_active);
	P.success = to_u32(src.success);
	P.warning = to_u32(src.warning);
	P.danger = to_u32(src.danger);
	P.info = to_u32(src.info);

	P.is_light = looks_light(src.window_bg_top);
	P.border_soft = colorutil::alpha(P.text, P.is_light ? 0.10f : 0.07f);
	P.text_dim = colorutil::mix(P.text_secondary, P.bg_bottom, 0.35f);

	P.rounding = t.style.rounding;
	P.spacing = t.style.spacing;
	P.window_padding = t.style.window_padding;
	P.font_scale = t.style.font_scale;
	P.anim_intensity = t.style.anim_intensity;
	P.glow_enabled = t.style.glow_enabled;
	P.glow_intensity = t.style.glow_intensity;
}

} // namespace ui
