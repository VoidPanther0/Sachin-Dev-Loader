#include "branding.h"

#include "colorutil.h"
#include "fonts.h"
#include "palette.h"
#include "state.h"
#include "widgets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

using colorutil::alpha;
using colorutil::mix;

namespace ui {

void draw_logo(ImVec2 c, float size, float opacity)
{
	soft_glow(c, size * 1.05f, P.accent, 0.024f * opacity, 24);

	const ImVec2 p0(0.34f, -0.40f);
	const ImVec2 p1(-0.32f, -0.04f);
	const ImVec2 p2(0.32f, 0.04f);
	const ImVec2 p3(-0.30f, 0.42f);

	std::array<ImVec2, 96> points{};
	int count = 0;

	auto sample = [&](ImVec2 start, ImVec2 control, ImVec2 end) {
		for (int i = 0; i <= 31 && count < static_cast<int>(points.size()); ++i)
		{
			const float t = static_cast<float>(i) / 31.0f;
			const float u = 1.0f - t;
			const ImVec2 p(
				u * u * start.x + 2.0f * u * t * control.x + t * t * end.x,
				u * u * start.y + 2.0f * u * t * control.y + t * t * end.y);
			points[count++] = ImVec2(c.x + p.x * size, c.y + p.y * size);
		}
	};

	sample(p0, ImVec2(-0.34f, -0.50f), p1);
	sample(p1, ImVec2(0.02f, 0.06f), p2);
	sample(p2, ImVec2(0.38f, 0.50f), p3);

	if (count == 0)
		return;

	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float radius = size * 0.082f;

	for (int pass = 0; pass < 2; ++pass)
	{
		const bool shade = pass == 0;
		for (int i = 0; i < count; ++i)
		{
			const ImVec2 point = shade
				? ImVec2(points[i].x + size * 0.014f, points[i].y + size * 0.035f)
				: points[i];

			ImU32 color = 0;
			if (shade)
			{
				color = IM_COL32(16, 8, 42, static_cast<int>(120.0f * opacity));
			}
			else
			{
				const float t = static_cast<float>(i) / static_cast<float>(count - 1);
				color = alpha(mix(P.accent, P.accent2, t), opacity);
			}

			draw->AddCircleFilled(point, radius, color, 20);
		}
	}

	for (int i = 0; i < count; i += 3)
	{
		const ImVec2 point(points[i].x - size * 0.008f, points[i].y - size * 0.008f);
		draw->AddCircleFilled(point, radius * 0.26f, IM_COL32(255, 255, 255, static_cast<int>(26.0f * opacity)), 12);
	}
}

float draw_wordmark_left(ImFont* wordmark_font, float x, float y, float size, float opacity)
{
	const char* first = "Sachin ";
	const char* second = "Game Loader";

	const ImU32 white = alpha(P.text, opacity);
	text(wordmark_font, size, ImVec2(x, y), white, first);

	float cursor = x + text_width(wordmark_font, size, first);
	const int letters = static_cast<int>(std::strlen(second));

	for (int i = 0; i < letters; ++i)
	{
		const float t = letters <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(letters - 1);
		const ImU32 color = alpha(mix(P.accent, P.accent2, t), opacity);
		char glyph[2] = { second[i], '\0' };
		text(wordmark_font, size, ImVec2(cursor, y), color, glyph);
		cursor += text_width(wordmark_font, size, glyph);
	}

	return cursor - x;
}

void draw_wordmark_center(float cx, float y, float size, float opacity)
{
	const float total =
		text_width(F.brand, size, "Sachin ") +
		text_width(F.brand, size, "Game Loader");
	draw_wordmark_left(F.brand, cx - total * 0.5f, y, size, opacity);
}

namespace {

struct Star
{
	float x;
	float y;
	float radius;
	float phase;
	float speed;
};

const std::array<Star, 84>& stars()
{
	static const std::array<Star, 84> value = [] {
		std::array<Star, 84> result{};
		unsigned int seed = 0x51ED270Bu;

		for (Star& star : result)
		{
			seed = seed * 1664525u + 1013904223u;
			star.x = 0.05f + static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f * 0.90f;
			seed = seed * 1664525u + 1013904223u;
			star.y = 0.05f + static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f * 0.72f;
			seed = seed * 1664525u + 1013904223u;
			star.radius = 0.7f + static_cast<float>((seed >> 8) & 0xFFu) / 255.0f * 1.1f;
			seed = seed * 1664525u + 1013904223u;
			star.phase = static_cast<float>((seed >> 8) & 0xFFu) / 255.0f * 6.28318f;
			seed = seed * 1664525u + 1013904223u;
			star.speed = 0.4f + static_cast<float>((seed >> 8) & 0xFFu) / 255.0f * 0.9f;
		}
		return result;
	}();

	return value;
}

const std::vector<std::pair<int, int>>& star_links()
{
	static const std::vector<std::pair<int, int>> value = [] {
		std::vector<std::pair<int, int>> result;
		const std::array<Star, 84>& points = stars();

		for (int i = 0; i < static_cast<int>(points.size()); ++i)
		{
			for (int j = i + 1; j < static_cast<int>(points.size()); ++j)
			{
				const float dx = points[i].x - points[j].x;
				const float dy = (points[i].y - points[j].y) * 0.85f;
				if (dx * dx + dy * dy < 0.018f && (i + j) % 3 == 0)
					result.emplace_back(i, j);
			}
		}
		return result;
	}();

	return value;
}

void draw_stars(float w, float h)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const std::array<Star, 84>& points = stars();

	for (const auto& link : star_links())
	{
		const Star& a = points[link.first];
		const Star& b = points[link.second];
		const float twinkle = 0.5f + 0.5f * std::sin(S.time * 0.5f + a.phase + b.phase);
		draw->AddLine(
			ImVec2(a.x * w, a.y * h),
			ImVec2(b.x * w, b.y * h),
			IM_COL32(120, 150, 255, static_cast<int>(5.0f + 7.0f * twinkle)));
	}

	for (const Star& star : points)
	{
		const float twinkle = 0.55f + 0.45f * std::sin(S.time * star.speed + star.phase);
		draw->AddCircleFilled(
			ImVec2(star.x * w, star.y * h),
			star.radius,
			IM_COL32(168, 188, 255, static_cast<int>(18.0f + 30.0f * twinkle)),
			8);
	}
}

void draw_waves(float w, float h, float strength = 1.0f)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float t = S.time;

	soft_glow(ImVec2(20.0f, h + 20.0f), 330.0f, P.accent2, 0.012f * strength, 24);
	soft_glow(ImVec2(w - 10.0f, h + 10.0f), 360.0f, P.accent, 0.012f * strength, 24);

	draw->PushClipRect(ImVec2(0.0f, h - 250.0f), ImVec2(w, h), true);

	for (int layer = 0; layer < 5; ++layer)
	{
		const float base = h - 34.0f + layer * 17.0f;
		const float amplitude = 15.0f + layer * 5.0f;
		const float phase = layer * 1.37f + t * (0.22f + layer * 0.015f);
		const ImU32 color = layer % 2 == 0
			? alpha(P.accent2, (10.0f + layer * 1.5f) / 255.0f * strength)
			: alpha(P.accent, (9.0f + layer * 1.5f) / 255.0f * strength);

		draw->PathClear();
		for (float x = -34.0f; x <= w + 34.0f; x += 10.0f)
		{
			const float y = base
				+ std::sin(x * 0.0068f + phase) * amplitude
				+ std::sin(x * 0.017f - phase * 0.62f) * amplitude * 0.36f;
			draw->PathLineTo(ImVec2(x, y));
		}
		draw->PathStroke(alpha(color, 0.22f), 6.0f, 0);
		draw->PathStroke(color, 1.15f, 0);
	}

	draw->PopClipRect();
}

} // namespace

void draw_background(float w, float h, bool grid)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	draw->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(w, h),
		P.bg_top, P.bg_top, P.bg_bottom, P.bg_bottom);

	soft_glow(ImVec2(w * 0.50f, -30.0f), 470.0f, P.accent, 0.020f, 28);
	soft_glow(ImVec2(w * 0.14f, h * 0.34f), 320.0f, P.accent2, 0.010f, 22);
	soft_glow(ImVec2(w * 0.90f, h * 0.76f), 400.0f, P.accent, 0.010f, 22);

	if (grid)
	{
		for (float x = 72.0f; x < w; x += 72.0f)
			draw->AddLine(ImVec2(x, 0), ImVec2(x, h), alpha(P.text, 0.016f));
		for (float y = 72.0f; y < h; y += 72.0f)
			draw->AddLine(ImVec2(0, y), ImVec2(w, y), alpha(P.text, 0.016f));
	}

	draw_stars(w, h);
	draw_waves(w, h);

	draw->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(w * 0.16f, h),
		alpha(P.bg_bottom, 0.35f), alpha(P.bg_bottom, 0.0f),
		alpha(P.bg_bottom, 0.0f), alpha(P.bg_bottom, 0.35f));
	draw->AddRectFilledMultiColor(
		ImVec2(w * 0.84f, 0), ImVec2(w, h),
		alpha(P.bg_bottom, 0.0f), alpha(P.bg_bottom, 0.35f),
		alpha(P.bg_bottom, 0.35f), alpha(P.bg_bottom, 0.0f));
	draw->AddRectFilledMultiColor(
		ImVec2(0, h * 0.72f), ImVec2(w, h),
		alpha(P.bg_bottom, 0.0f), alpha(P.bg_bottom, 0.0f),
		alpha(P.bg_bottom, 0.47f), alpha(P.bg_bottom, 0.47f));

	draw->AddRect(ImVec2(0.5f, 0.5f), ImVec2(w - 0.5f, h - 0.5f), alpha(P.text, 0.07f), 0.0f, 1.0f, 0);
}

} // namespace ui
