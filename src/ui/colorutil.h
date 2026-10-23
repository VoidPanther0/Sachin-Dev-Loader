#pragma once

#include "imgui.h"

namespace colorutil {

inline float clampf(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

inline ImU32 alpha(ImU32 color, float value)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
	c.w *= value;
	c.w = clampf(c.w, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

inline ImU32 mix(ImU32 from, ImU32 to, float t)
{
	t = clampf(t, 0.0f, 1.0f);
	ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
	ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t));
}

} // namespace colorutil
