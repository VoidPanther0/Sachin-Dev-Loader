#include "widgets.h"

#include "colorutil.h"
#include "fonts.h"
#include "palette.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

using colorutil::alpha;
using colorutil::clampf;
using colorutil::mix;

namespace ui {

ImU32 level_color(Level level)
{
	switch (level)
	{
	case Level::Info: return P.info;
	case Level::Success: return P.success;
	case Level::Warning: return P.warning;
	case Level::Danger: return P.danger;
	}
	return P.info;
}

Rect rect(float x, float y, float x2, float y2) { return Rect{ ImVec2(x, y), ImVec2(x2, y2) }; }
float width(const Rect& r) { return r.max.x - r.min.x; }
float height(const Rect& r) { return r.max.y - r.min.y; }
ImVec2 center(const Rect& r) { return ImVec2(r.min.x + width(r) * 0.5f, r.min.y + height(r) * 0.5f); }
Rect inset(const Rect& r, float amount) { return rect(r.min.x + amount, r.min.y + amount, r.max.x - amount, r.max.y - amount); }

namespace {
std::unordered_map<ImGuiID, float> g_anim;
}

float animate(const char* id, float target, float speed)
{
	const ImGuiID key = ImGui::GetID(id);
	float& value = g_anim[key];
	const float delta = clampf(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f);
	value += (target - value) * delta;
	return value;
}

void box(const Rect& r, ImU32 fill, float rounding, ImU32 border, float border_size)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(r.min, r.max, fill, rounding);
	if (border != 0)
		draw->AddRect(r.min, r.max, border, rounding, border_size, 0);
}

void panel(const Rect& r, float rounding)
{
	box(r, P.panel, rounding < 0.0f ? P.rounding : rounding, P.border_strong);
}

void gradient_round_rect(const Rect& r, float rounding, ImU32 left, ImU32 right)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(r.min, r.max, mix(left, right, 0.5f), rounding);

	const float h = height(r);
	const int steps = std::max(1, static_cast<int>(h));
	const float step = h / static_cast<float>(steps);

	for (int i = 0; i < steps; ++i)
	{
		const float y = static_cast<float>(i) * step;
		const float mid = y + step * 0.5f;
		float insetAmount = 0.0f;

		if (mid < rounding)
			insetAmount = rounding - std::sqrt(std::max(0.0f, rounding * rounding - (rounding - mid) * (rounding - mid)));
		else if (mid > h - rounding)
		{
			const float d = mid - (h - rounding);
			insetAmount = rounding - std::sqrt(std::max(0.0f, rounding * rounding - d * d));
		}

		if (insetAmount > 0.0f)
			insetAmount = std::floor(insetAmount) + 0.5f;

		draw->AddRectFilledMultiColor(
			ImVec2(r.min.x + insetAmount, r.min.y + y),
			ImVec2(r.max.x - insetAmount, r.min.y + y + step + 0.4f),
			left, right, right, left);
	}
}

void soft_glow(ImVec2 position, float radius, ImU32 color, float strength, int steps)
{
	if (!P.glow_enabled)
		return;
	strength *= P.glow_intensity;
	if (strength <= 0.0005f)
		return;

	ImDrawList* draw = ImGui::GetWindowDrawList();
	for (int i = steps; i > 0; --i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		const float falloff = (1.0f - t) * (1.0f - t);
		draw->AddCircleFilled(position, radius * t, alpha(color, strength * falloff), 32);
	}
}

void text(ImFont* f, float size, ImVec2 position, ImU32 color, const char* value)
{
	ImGui::GetWindowDrawList()->AddText(f, size, position, color, value);
}

float text_width(ImFont* f, float size, const char* value)
{
	return f->CalcTextSizeA(size, FLT_MAX, 0.0f, value).x;
}

void text_center(ImFont* f, float size, const Rect& r, ImU32 color, const char* value, float offset_y)
{
	const float w = text_width(f, size, value);
	const float x = r.min.x + (width(r) - w) * 0.5f;
	const float y = r.min.y + (height(r) - size) * 0.5f + offset_y;
	text(f, size, ImVec2(x, y), color, value);
}

void text_center_x(ImFont* f, float size, float center_x, float y, ImU32 color, const char* value)
{
	text(f, size, ImVec2(center_x - text_width(f, size, value) * 0.5f, y), color, value);
}

std::string elide(ImFont* f, float size, const std::string& value, float max_width)
{
	if (text_width(f, size, value.c_str()) <= max_width)
		return value;

	std::string result = value;
	while (!result.empty())
	{
		result.pop_back();
		const std::string candidate = result + "...";
		if (text_width(f, size, candidate.c_str()) <= max_width)
			return candidate;
	}
	return "...";
}

// ------------------------------------------------------------------ icons ----

void icon_user(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(ImVec2(c.x, c.y - size * 0.20f), size * 0.20f, color, 24, 1.5f);
	draw->PathClear();
	draw->PathArcTo(ImVec2(c.x, c.y + size * 0.36f), size * 0.34f, 3.14159f, 6.28318f, 24);
	draw->PathStroke(color, 1.5f, 0);
}

void icon_lock(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(
		ImVec2(c.x - size * 0.31f, c.y - size * 0.02f),
		ImVec2(c.x + size * 0.31f, c.y + size * 0.38f),
		color, 3.0f, 1.5f, 0);
	draw->PathClear();
	draw->PathArcTo(ImVec2(c.x, c.y - size * 0.02f), size * 0.20f, 3.14159f, 6.28318f, 20);
	draw->PathStroke(color, 1.5f, 0);
}

void icon_eye(ImVec2 c, float size, ImU32 color, bool open)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = size * 0.52f;
	const float h = size * 0.30f;

	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x - w, c.y));
	draw->PathBezierCubicCurveTo(ImVec2(c.x - w * 0.45f, c.y - h), ImVec2(c.x + w * 0.45f, c.y - h), ImVec2(c.x + w, c.y), 18);
	draw->PathBezierCubicCurveTo(ImVec2(c.x + w * 0.45f, c.y + h), ImVec2(c.x - w * 0.45f, c.y + h), ImVec2(c.x - w, c.y), 18);
	draw->PathStroke(color, 1.5f, 0);

	draw->AddCircle(ImVec2(c.x, c.y), size * 0.13f, color, 20, 1.5f);

	if (!open)
		draw->AddLine(ImVec2(c.x - w * 0.9f, c.y + h * 1.15f), ImVec2(c.x + w * 0.9f, c.y - h * 1.15f), color, 1.5f);
}

void icon_arrow(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.5f;
	draw->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), color, 1.7f);
	draw->AddLine(ImVec2(c.x + s * 0.2f, c.y - s * 0.55f), ImVec2(c.x + s, c.y), color, 1.7f);
	draw->AddLine(ImVec2(c.x + s * 0.2f, c.y + s * 0.55f), ImVec2(c.x + s, c.y), color, 1.7f);
}

void icon_close(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.34f;
	draw->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), color, 1.5f);
	draw->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x - s, c.y + s), color, 1.5f);
}

void icon_minimize(ImVec2 c, float size, ImU32 color)
{
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(c.x - size * 0.34f, c.y),
		ImVec2(c.x + size * 0.34f, c.y), color, 1.5f);
}

void icon_maximize(ImVec2 c, float size, ImU32 color, bool is_maximized)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.30f;
	if (!is_maximized)
	{
		draw->AddRect(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), color, 1.5f, 1.3f, 0);
	}
	else
	{
		draw->AddRect(ImVec2(c.x - s + 3.0f, c.y - s), ImVec2(c.x + s, c.y + s - 3.0f), color, 1.5f, 1.3f, 0);
		draw->AddRect(ImVec2(c.x - s, c.y - s + 3.0f), ImVec2(c.x + s - 3.0f, c.y + s), color, 1.5f, 1.3f, 0);
	}
}

void icon_check(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.30f;
	draw->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * 0.15f, c.y + s * 0.8f), color, 1.8f);
	draw->AddLine(ImVec2(c.x - s * 0.15f, c.y + s * 0.8f), ImVec2(c.x + s, c.y - s * 0.6f), color, 1.8f);
}

void icon_plus(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.34f;
	draw->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), color, 1.7f);
	draw->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y + s), color, 1.7f);
}

void icon_trash(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = size * 0.30f;
	const float top = c.y - size * 0.26f;
	const float bottom = c.y + size * 0.32f;
	draw->AddLine(ImVec2(c.x - w * 1.3f, top), ImVec2(c.x + w * 1.3f, top), color, 1.4f);
	draw->AddRect(ImVec2(c.x - w * 0.5f, top - size * 0.14f), ImVec2(c.x + w * 0.5f, top), color, 1.0f, 1.3f, 0);
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x - w, top));
	draw->PathLineTo(ImVec2(c.x - w * 0.85f, bottom));
	draw->PathLineTo(ImVec2(c.x + w * 0.85f, bottom));
	draw->PathLineTo(ImVec2(c.x + w, top));
	draw->PathStroke(color, 1.4f, 0);
	draw->AddLine(ImVec2(c.x, top + 2.0f), ImVec2(c.x, bottom - 2.0f), color, 1.2f);
}

void icon_edit(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.34f;
	draw->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), color, 1.6f);
	draw->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s * 0.55f, c.y - s * 1.35f), color, 1.6f);
	draw->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x - s * 1.3f, c.y + s * 1.3f), color, 1.6f);
}

void icon_folder(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = size * 0.42f;
	const float h = size * 0.30f;
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x - w, c.y - h * 0.4f));
	draw->PathLineTo(ImVec2(c.x - w * 0.3f, c.y - h * 0.4f));
	draw->PathLineTo(ImVec2(c.x - w * 0.1f, c.y - h));
	draw->PathLineTo(ImVec2(c.x + w, c.y - h));
	draw->PathLineTo(ImVec2(c.x + w, c.y + h));
	draw->PathLineTo(ImVec2(c.x - w, c.y + h));
	draw->PathStroke(color, 1.5f, ImDrawFlags_Closed);
}

void icon_search(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec2 lens(c.x - size * 0.08f, c.y - size * 0.08f);
	draw->AddCircle(lens, size * 0.26f, color, 20, 1.5f);
	const ImVec2 dir(0.72f, 0.72f);
	draw->AddLine(
		ImVec2(lens.x + dir.x * size * 0.26f, lens.y + dir.y * size * 0.26f),
		ImVec2(lens.x + dir.x * size * 0.50f, lens.y + dir.y * size * 0.50f),
		color, 1.6f);
}

void icon_gear(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float outer = size * 0.34f;
	for (int i = 0; i < 8; ++i)
	{
		const float a = static_cast<float>(i) * 0.785398f;
		const ImVec2 p0(c.x + std::cos(a) * outer * 0.78f, c.y + std::sin(a) * outer * 0.78f);
		const ImVec2 p1(c.x + std::cos(a) * outer * 1.18f, c.y + std::sin(a) * outer * 1.18f);
		draw->AddLine(p0, p1, color, 1.6f);
	}
	draw->AddCircle(c, outer * 0.78f, color, 24, 1.4f);
	draw->AddCircle(c, outer * 0.30f, color, 16, 1.3f);
}

void icon_info(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, size * 0.34f, color, 24, 1.4f);
	draw->AddCircleFilled(ImVec2(c.x, c.y - size * 0.14f), 1.4f, color, 8);
	draw->AddLine(ImVec2(c.x, c.y - size * 0.02f), ImVec2(c.x, c.y + size * 0.17f), color, 1.6f);
}

void icon_warning_sign(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.36f;
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x, c.y - s));
	draw->PathLineTo(ImVec2(c.x + s, c.y + s * 0.8f));
	draw->PathLineTo(ImVec2(c.x - s, c.y + s * 0.8f));
	draw->PathStroke(color, 1.5f, ImDrawFlags_Closed);
	draw->AddLine(ImVec2(c.x, c.y - s * 0.25f), ImVec2(c.x, c.y + s * 0.2f), color, 1.5f);
	draw->AddCircleFilled(ImVec2(c.x, c.y + s * 0.55f), 1.3f, color, 8);
}

void icon_home(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.36f;
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x - s, c.y + s * 0.1f));
	draw->PathLineTo(ImVec2(c.x, c.y - s));
	draw->PathLineTo(ImVec2(c.x + s, c.y + s * 0.1f));
	draw->PathStroke(color, 1.5f, 0);
	draw->AddRect(ImVec2(c.x - s * 0.55f, c.y + s * 0.05f), ImVec2(c.x + s * 0.55f, c.y + s * 0.95f), color, 1.0f, 1.4f, 0);
}

void icon_grid(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.17f;
	const float gap = size * 0.08f;
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 2; ++x)
			draw->AddRect(
				ImVec2(c.x - s - gap * 0.5f + x * (s * 2.0f + gap), c.y - s - gap * 0.5f + y * (s * 2.0f + gap)),
				ImVec2(c.x + s - gap * 0.5f + x * (s * 2.0f + gap), c.y + s - gap * 0.5f + y * (s * 2.0f + gap)),
				color, 2.0f, 1.4f, 0);
}

void icon_plug(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.32f;
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x - s * 0.1f, c.y - s));
	draw->PathLineTo(ImVec2(c.x - s * 0.9f, c.y + s * 0.15f));
	draw->PathLineTo(ImVec2(c.x - s * 0.1f, c.y + s * 0.15f));
	draw->PathLineTo(ImVec2(c.x - s * 0.5f, c.y + s));
	draw->PathLineTo(ImVec2(c.x + s * 0.9f, c.y - s * 0.25f));
	draw->PathLineTo(ImVec2(c.x + s * 0.1f, c.y - s * 0.25f));
	draw->PathLineTo(ImVec2(c.x + s * 0.9f, c.y - s));
	draw->PathStroke(color, 1.3f, ImDrawFlags_Closed);
}

void icon_list(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = size * 0.34f;
	for (int i = -1; i <= 1; ++i)
	{
		const float y = c.y + static_cast<float>(i) * size * 0.22f;
		draw->AddCircleFilled(ImVec2(c.x - w, y), 1.6f, color, 8);
		draw->AddLine(ImVec2(c.x - w + 5.0f, y), ImVec2(c.x + w, y), color, 1.5f);
	}
}

void icon_palette(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, size * 0.32f, color, 24, 1.4f);
	draw->AddCircleFilled(ImVec2(c.x - size * 0.12f, c.y - size * 0.10f), 2.4f, color, 10);
	draw->AddCircleFilled(ImVec2(c.x + size * 0.12f, c.y - size * 0.10f), 2.4f, color, 10);
	draw->AddCircleFilled(ImVec2(c.x, c.y + size * 0.14f), 2.4f, color, 10);
}

void icon_star(ImVec2 c, float size, ImU32 color, bool filled)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float outer = size * 0.42f;
	const float innerR = outer * 0.46f;
	std::array<ImVec2, 10> points{};
	for (int i = 0; i < 10; ++i)
	{
		const float a = -1.5707963f + static_cast<float>(i) * 0.628318f;
		const float r = (i % 2 == 0) ? outer : innerR;
		points[i] = ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
	}

	if (filled)
	{
		for (int i = 0; i < 10; ++i)
			draw->AddTriangleFilled(c, points[i], points[(i + 1) % 10], color);
	}
	else
	{
		draw->PathClear();
		for (const ImVec2& p : points) draw->PathLineTo(p);
		draw->PathStroke(color, 1.4f, ImDrawFlags_Closed);
	}
}

void icon_chip(ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.26f;
	draw->AddRect(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), color, 2.0f, 1.4f, 0);
	for (int i = -1; i <= 1; ++i)
	{
		const float off = static_cast<float>(i) * s * 0.9f;
		draw->AddLine(ImVec2(c.x - s, c.y + off), ImVec2(c.x - s - 4.0f, c.y + off), color, 1.3f);
		draw->AddLine(ImVec2(c.x + s, c.y + off), ImVec2(c.x + s + 4.0f, c.y + off), color, 1.3f);
	}
}

void spinner(ImVec2 c, float radius, float thickness, ImU32 color, float time)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, radius, alpha(P.text, 0.12f), 48, thickness);
	const float start = time * 5.2f;
	draw->PathClear();
	draw->PathArcTo(c, radius, start, start + 4.35f, 40);
	draw->PathStroke(color, thickness, 0);

	const float head = start + 4.35f;
	const ImVec2 point(c.x + std::cos(head) * radius, c.y + std::sin(head) * radius);
	draw->AddCircleFilled(point, thickness * 0.60f, mix(color, IM_COL32_WHITE, 0.55f));
}

// --------------------------------------------------------------- buttons ----

bool primary_button(const char* id, const Rect& r, const char* label, bool enabled, bool arrow)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = enabled && ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = enabled && ImGui::IsItemHovered();
	const bool active = enabled && ImGui::IsItemActive();

	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 12.0f);

	if (!enabled)
	{
		box(r, alpha(P.text, 0.06f), P.rounding, P.border_soft, 1.0f);
		const ImU32 label_color = P.text_dim;
		const float label_w = text_width(F.button, 17.0f, label);
		const float icon_w = arrow ? 16.0f : 0.0f;
		const float gap = arrow ? 10.0f : 0.0f;
		const float start = r.min.x + (width(r) - label_w - icon_w - gap) * 0.5f;
		const float cy = r.min.y + height(r) * 0.5f;
		text(F.button, 17.0f, ImVec2(start, cy - 9.0f), label_color, label);
		if (arrow)
			icon_arrow(ImVec2(start + label_w + gap + 8.0f, cy), 16.0f, label_color);
		return false;
	}

	if (t > 0.01f)
		soft_glow(center(r), width(r) * 0.58f, P.accent, 0.020f * t, 20);

	ImU32 left = mix(P.accent, P.button_hover, t * 0.55f);
	ImU32 right = mix(P.accent2, P.button_hover, t * 0.35f);

	if (active)
	{
		left = mix(left, IM_COL32_BLACK, 0.10f);
		right = mix(right, IM_COL32_BLACK, 0.10f);
	}

	gradient_round_rect(r, P.rounding, left, right);
	ImGui::GetWindowDrawList()->AddRect(r.min, r.max, alpha(IM_COL32_WHITE, 0.20f), P.rounding, 1.0f, 0);

	const float label_w = text_width(F.button, 17.0f, label);
	const float icon_w = arrow ? 16.0f : 0.0f;
	const float gap = arrow ? 11.0f : 0.0f;
	const float total = label_w + icon_w + gap;
	const float start = r.min.x + (width(r) - total) * 0.5f;
	const float cy = r.min.y + height(r) * 0.5f;

	text(F.button, 17.0f, ImVec2(start, cy - 9.5f), IM_COL32_WHITE, label);
	if (arrow)
		icon_arrow(ImVec2(start + label_w + gap + 8.0f, cy), 16.0f, IM_COL32_WHITE);

	return clicked;
}

bool danger_button(const char* id, const Rect& r, const char* label, bool enabled)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = enabled && ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = enabled && ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 12.0f);
	const ImU32 fill = enabled ? mix(alpha(P.danger, 0.16f), alpha(P.danger, 0.30f), t) : alpha(P.text, 0.05f);
	const ImU32 border = enabled ? mix(alpha(P.danger, 0.5f), P.danger, t) : P.border_soft;
	box(r, fill, P.rounding, border, 1.0f);
	text_center(F.button, 15.0f, r, enabled ? mix(P.danger, IM_COL32(255, 255, 255, 255), t * 0.4f) : P.text_dim, label, -0.5f);
	return clicked;
}

bool ghost_button(const char* id, const Rect& r, const char* label)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 12.0f);
	box(r, alpha(P.text, 0.03f + 0.04f * t), P.rounding * 0.8f,
		mix(P.border_soft, P.border_strong, t), 1.0f);
	text_center(F.small_font, 13.0f, r, mix(P.text_secondary, P.text, t), label, -0.5f);
	return clicked;
}

bool link_button(const char* id, const Rect& r, const char* label, ImU32 base_color)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);
	const ImU32 color = mix(base_color, P.text, t);
	const ImVec2 position(r.min.x, r.min.y + (height(r) - 14.0f) * 0.5f - 1.0f);
	text(F.link, 14.0f, position, color, label);

	if (t > 0.05f)
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(position.x, position.y + 18.0f),
			ImVec2(position.x + text_width(F.link, 14.0f, label) * t, position.y + 18.0f),
			alpha(color, 0.6f), 1.0f);

	return clicked;
}

bool icon_button(const char* id, const Rect& r, const IconDrawer& icon, bool subtle)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);
	if (subtle)
		box(r, alpha(P.text, 0.05f * t), std::min(width(r), height(r)) * 0.3f);
	if (icon)
		icon(center(r), std::min(width(r), height(r)) * 0.9f, mix(P.text_secondary, P.text, t));
	return clicked;
}

// ----------------------------------------------------------------- input ----

void draw_field(const char* id, const char* hint, const Rect& r, char* buffer, size_t size,
	bool password, bool* reveal, const IconDrawer& leading_icon)
{
	const bool hovered = ImGui::IsMouseHoveringRect(r.min, r.max);
	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);

	const ImU32 fill = mix(P.field, alpha(P.text, 0.03f), t * 0.5f);
	box(r, fill, P.rounding * 0.9f);
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(r.min.x + 14.0f, r.min.y + 1.0f),
		ImVec2(r.max.x - 14.0f, r.min.y + 1.0f),
		alpha(IM_COL32_WHITE, 0.035f));

	ImGui::PushFont(F.input);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(50.0f, 17.0f));
	ImGui::SetCursorScreenPos(r.min);
	ImGui::SetNextItemWidth(width(r));

	ImGuiInputTextFlags flags = 0;
	const bool masked = password && !(reveal && *reveal);
	if (masked)
		flags |= ImGuiInputTextFlags_Password;

	ImGui::InputTextWithHint(id, hint, buffer, size, flags);

	const bool active = ImGui::IsItemActive();
	ImGui::PopStyleVar();
	ImGui::PopFont();

	const ImU32 border = active
		? alpha(P.accent, 0.75f)
		: mix(P.border_soft, alpha(P.accent2, 0.55f), t);
	ImGui::GetWindowDrawList()->AddRect(r.min, r.max, border, P.rounding * 0.9f, active ? 1.5f : 1.0f, 0);

	const ImVec2 icon_center(r.min.x + 27.0f, r.min.y + height(r) * 0.5f);
	const ImU32 icon_color = active ? P.accent : mix(P.text_dim, P.text_secondary, t);
	if (leading_icon)
		leading_icon(icon_center, 18.0f, icon_color);
	else if (password)
		icon_lock(icon_center, 18.0f, icon_color);
	else
		icon_user(icon_center, 18.0f, icon_color);

	if (password && reveal)
	{
		ImGui::PushID(id);
		const Rect eye = rect(r.max.x - 48.0f, r.min.y, r.max.x - 8.0f, r.max.y);
		ImGui::SetCursorScreenPos(eye.min);
		const bool eye_clicked = ImGui::InvisibleButton("##reveal", ImVec2(width(eye), height(eye)));
		const bool eye_hovered = ImGui::IsItemHovered();
		if (eye_hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		icon_eye(center(eye), 18.0f, eye_hovered ? P.text : P.text_secondary, *reveal);
		if (eye_clicked)
			*reveal = !*reveal;
		ImGui::PopID();
	}
}

bool checkbox(const char* id, const Rect& r, bool* value, const char* label)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);
	const Rect square = rect(r.min.x, r.min.y + (height(r) - 18.0f) * 0.5f, r.min.x + 18.0f, r.min.y + (height(r) + 18.0f) * 0.5f);

	box(square,
		*value ? alpha(P.accent, 0.85f) : alpha(P.field, 0.9f),
		4.0f,
		*value ? alpha(P.accent, 0.95f) : mix(P.border_strong, alpha(P.accent2, 0.75f), t),
		1.2f);

	if (*value)
	{
		ImDrawList* draw = ImGui::GetWindowDrawList();
		const ImVec2 c = center(square);
		draw->AddLine(ImVec2(c.x - 4.0f, c.y), ImVec2(c.x - 1.0f, c.y + 3.0f), IM_COL32_WHITE, 1.8f);
		draw->AddLine(ImVec2(c.x - 1.0f, c.y + 3.0f), ImVec2(c.x + 5.0f, c.y - 4.0f), IM_COL32_WHITE, 1.8f);
	}

	if (label && label[0] != '\0')
		text(F.link, 14.0f, ImVec2(r.min.x + 28.0f, r.min.y + (height(r) - 14.0f) * 0.5f - 1.0f),
			mix(P.text_secondary, P.text, t * 0.7f), label);

	if (clicked)
		*value = !*value;

	return clicked;
}

bool toggle_switch(const char* id, const Rect& r, bool* value)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float on_t = animate(id, *value ? 1.0f : 0.0f, 16.0f);
	const float h = height(r);
	box(r, mix(alpha(P.text, 0.12f), P.accent, on_t), h * 0.5f);

	const float knob_r = h * 0.5f - 2.5f;
	const float knob_x = r.min.x + h * 0.5f + (width(r) - h) * on_t;
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(knob_x, center(r).y), knob_r, IM_COL32_WHITE, 20);

	if (clicked)
		*value = !*value;
	return clicked;
}

// ----------------------------------------------------------------- misc ----

void draw_banner(const Rect& r, Level level, const char* message)
{
	const ImU32 tone = level_color(level);
	box(r, alpha(tone, 0.14f), P.rounding * 0.7f, alpha(tone, 0.4f), 1.0f);

	const ImVec2 icon_c(r.min.x + 22.0f, r.min.y + height(r) * 0.5f);
	switch (level)
	{
	case Level::Danger:
		ImGui::GetWindowDrawList()->AddCircleFilled(icon_c, 8.5f, alpha(tone, 0.85f), 20);
		icon_close(icon_c, 11.0f, IM_COL32(40, 10, 16, 255));
		break;
	case Level::Warning:
		icon_warning_sign(icon_c, 20.0f, tone);
		break;
	case Level::Success:
		ImGui::GetWindowDrawList()->AddCircleFilled(icon_c, 8.5f, alpha(tone, 0.85f), 20);
		icon_check(icon_c, 13.0f, IM_COL32(8, 30, 22, 255));
		break;
	default:
		icon_info(icon_c, 20.0f, tone);
		break;
	}

	text(F.small_font, 13.0f, ImVec2(r.min.x + 40.0f, r.min.y + (height(r) - 13.0f) * 0.5f - 1.0f),
		mix(tone, P.text, 0.35f), message);
}

void status_row(const Rect& card, float y, const char* label, const char* value, ImU32 dot)
{
	text(F.micro, 10.0f, ImVec2(card.min.x + 24.0f, y), P.text_dim, label);

	const float value_w = text_width(F.small_font, 13.0f, value);
	const float text_x = card.max.x - 24.0f - value_w;

	if (dot != 0)
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(text_x - 10.0f, y + 6.5f), 3.0f, dot);

	text(F.small_font, 13.0f, ImVec2(text_x, y - 1.0f), P.text, value);
}

void badge(const Rect& r, const char* label, ImU32 color)
{
	box(r, alpha(color, 0.14f), height(r) * 0.5f, alpha(color, 0.4f), 1.0f);
	text_center(F.micro, 10.0f, r, mix(color, P.text, 0.2f), label);
}

void progress_bar(const Rect& r, float progress01, ImU32 left, ImU32 right)
{
	progress01 = clampf(progress01, 0.0f, 1.0f);
	box(r, alpha(P.text, 0.08f), height(r) * 0.5f);
	if (progress01 > 0.002f)
	{
		const Rect fill = rect(r.min.x, r.min.y, r.min.x + width(r) * progress01, r.max.y);
		gradient_round_rect(fill, height(r) * 0.5f, left, right);
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(fill.max.x, center(fill).y), height(r) * 0.42f, IM_COL32_WHITE);
	}
}

int tabs(const char* id, const Rect& r, const char* const* labels, int count, int selected)
{
	if (count <= 0)
		return selected;

	const float seg_w = width(r) / static_cast<float>(count);
	int result = selected;

	for (int i = 0; i < count; ++i)
	{
		const Rect seg = rect(r.min.x + seg_w * i, r.min.y, r.min.x + seg_w * (i + 1), r.max.y);
		ImGui::PushID(i);
		ImGui::PushID(id);
		ImGui::SetCursorScreenPos(seg.min);
		const bool clicked = ImGui::InvisibleButton("##tab", ImVec2(width(seg), height(seg)));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		ImGui::PopID();

		if (hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		char key[64];
		std::snprintf(key, sizeof(key), "%s##tabanim%d", id, i);
		const float t = animate(key, (i == selected) ? 1.0f : (hovered ? 0.4f : 0.0f), 14.0f);

		text_center(F.small_font, 13.0f, seg, mix(P.text_secondary, P.text, t), labels[i], -0.5f);
		if (i == selected)
			ImGui::GetWindowDrawList()->AddLine(
				ImVec2(seg.min.x + seg_w * 0.2f, seg.max.y - 1.0f),
				ImVec2(seg.max.x - seg_w * 0.2f, seg.max.y - 1.0f),
				P.accent, 2.0f);

		if (clicked)
			result = i;
	}

	return result;
}

void empty_state(const Rect& r, const IconDrawer& icon, const char* title, const char* subtitle)
{
	const ImVec2 c = center(r);
	if (icon)
	{
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(c.x, c.y - 44.0f), 28.0f, alpha(P.text, 0.05f), 24);
		icon(ImVec2(c.x, c.y - 44.0f), 28.0f, P.text_secondary);
	}
	text_center_x(F.body, 15.0f, c.x, c.y - 2.0f, P.text, title);
	text_center_x(F.link, 13.0f, c.x, c.y + 20.0f, P.text_dim, subtitle);
}

void begin_scroll(const char* id, const Rect& r)
{
	ImGui::SetCursorScreenPos(r.min);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImGui::ColorConvertU32ToFloat4(alpha(P.text, 0.16f)));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGui::ColorConvertU32ToFloat4(alpha(P.text, 0.28f)));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImGui::ColorConvertU32ToFloat4(alpha(P.text, 0.40f)));
	ImGui::BeginChild(id, ImVec2(width(r), height(r)), false, ImGuiWindowFlags_NoBackground);
}

void end_scroll()
{
	ImGui::EndChild();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
}

Modal modal_frame(const char* id, const char* title, float w, float h)
{
	ImGui::PushID(id);

	const ImVec2 vp = ImGui::GetIO().DisplaySize;
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), vp, alpha(IM_COL32_BLACK, 0.55f));

	const Rect bounds = rect((vp.x - w) * 0.5f, (vp.y - h) * 0.5f, (vp.x + w) * 0.5f, (vp.y + h) * 0.5f);
	soft_glow(center(bounds), w * 0.6f, P.accent, 0.012f, 22);
	box(bounds, P.panel, P.rounding, P.border_strong, 1.0f);

	text(F.brand_small, 18.0f, ImVec2(bounds.min.x + 22.0f, bounds.min.y + 18.0f), P.text, title);

	const Rect close_rect = rect(bounds.max.x - 42.0f, bounds.min.y + 12.0f, bounds.max.x - 12.0f, bounds.min.y + 42.0f);
	const bool close_clicked = icon_button("##modal_close", close_rect, [](ImVec2 c, float s, ImU32 color) { icon_close(c, s, color); });

	Modal modal;
	modal.open = true;
	modal.close_requested = close_clicked || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
	modal.content = rect(bounds.min.x + 22.0f, bounds.min.y + 58.0f, bounds.max.x - 22.0f, bounds.max.y - 22.0f);

	ImGui::PopID();
	return modal;
}

// ---------------------------------------------------------------- toasts ----

namespace {
struct Toast
{
	Level level;
	std::string text;
	float age = 0.0f;
};
std::vector<Toast> g_toasts;
}

void push_toast(Level level, const std::string& text_value)
{
	g_toasts.push_back(Toast{ level, text_value, 0.0f });
	if (g_toasts.size() > 5)
		g_toasts.erase(g_toasts.begin());
}

void render_toasts()
{
	constexpr float kLifetime = 3.6f;
	constexpr float kFade = 0.5f;

	const float dt = clampf(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
	const ImVec2 vp = ImGui::GetIO().DisplaySize;

	float y = 20.0f;
	for (size_t i = 0; i < g_toasts.size();)
	{
		Toast& toast = g_toasts[i];
		toast.age += dt;
		if (toast.age >= kLifetime)
		{
			g_toasts.erase(g_toasts.begin() + static_cast<long>(i));
			continue;
		}

		const float fade_in = clampf(toast.age / 0.2f, 0.0f, 1.0f);
		const float fade_out = clampf((kLifetime - toast.age) / kFade, 0.0f, 1.0f);
		const float a = std::min(fade_in, fade_out);

		const float w = 320.0f;
		const float h = 48.0f;
		const Rect r = rect(vp.x - w - 20.0f, y, vp.x - 20.0f, y + h);
		const ImU32 tone = level_color(toast.level);
		box(r, alpha(mix(P.panel, tone, 0.12f), 0.95f * a), P.rounding * 0.8f, alpha(tone, 0.5f * a), 1.0f);
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(r.min.x + 22.0f, center(r).y), 5.0f, alpha(tone, a));
		text(F.small_font, 13.0f, ImVec2(r.min.x + 40.0f, r.min.y + (h - 13.0f) * 0.5f), alpha(P.text, a), toast.text.c_str());

		y += h + 10.0f;
		++i;
	}
}

} // namespace ui
