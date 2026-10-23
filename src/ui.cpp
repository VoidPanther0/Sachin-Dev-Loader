#include "app.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// ------------------------------------------------------------------ color ----

constexpr ImU32 rgba(int r, int g, int b, int a = 255)
{
	return IM_COL32(r, g, b, a);
}

float clampf(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

ImU32 alpha(ImU32 color, float value)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
	c.w *= value;
	c.w = clampf(c.w, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 mix(ImU32 from, ImU32 to, float t)
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

const ImU32 kBgTop = rgba(6, 11, 26);
const ImU32 kBgBottom = rgba(4, 7, 18);
const ImU32 kPanel_soft = rgba(11, 19, 39, 190);
const ImU32 kLine = rgba(255, 255, 255, 18);
const ImU32 kLine_strong = rgba(120, 148, 220, 46);
const ImU32 kText = rgba(238, 242, 255);
const ImU32 kMuted = rgba(138, 150, 180);
const ImU32 kDim = rgba(88, 101, 133);
const ImU32 kViolet = rgba(124, 77, 255);
const ImU32 kBlue = rgba(59, 130, 246);
const ImU32 kGreen = rgba(66, 230, 164);
const ImU32 kRed = rgba(255, 90, 118);
const ImU32 kField = rgba(8, 14, 30, 220);

// ------------------------------------------------------------------ state ----

enum class Screen
{
	Splash,
	Login,
	Loader
};

struct UiState
{
	Screen screen = Screen::Splash;

	float time = 0.0f;
	float screen_time = 0.0f;
	float fps = 0.0f;

	char username[128] = {};
	char password[128] = {};
	bool remember = false;
	bool reveal_password = false;

	bool authenticating = false;
	float auth_time = 0.0f;

	bool error = false;
	bool info = false;
	float banner_age = 10.0f;

	bool launching = false;
	bool ready = false;
	float launch = 0.0f;

	std::string session;
};

UiState state;

std::unordered_map<ImGuiID, float> g_anim;

void go(Screen screen)
{
	state.screen = screen;
	state.screen_time = 0.0f;
}

float animate(const char* id, float target, float speed = 14.0f)
{
	const ImGuiID key = ImGui::GetID(id);
	float& value = g_anim[key];
	const float delta = clampf(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f);
	value += (target - value) * delta;
	return value;
}

// ------------------------------------------------------------------ fonts ----

struct Fonts
{
	ImFont* micro = nullptr;
	ImFont* tiny = nullptr;
	ImFont* small_font = nullptr;
	ImFont* body = nullptr;
	ImFont* hero = nullptr;
	ImFont* brand = nullptr;
	ImFont* brand_small = nullptr;
	ImFont* subtitle = nullptr;
	ImFont* input = nullptr;
	ImFont* button = nullptr;
	ImFont* link = nullptr;
};

Fonts font;

std::string font_path(const char* file)
{
	char directory[MAX_PATH] = {};
	const DWORD length = GetWindowsDirectoryA(directory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return std::string();

	std::string path = directory;
	path += "\\Fonts\\";
	path += file;

	if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return std::string();

	return path;
}

ImFont* load_font(const char* file, float size)
{
	const std::string path = font_path(file);
	if (!path.empty())
	{
		ImFontConfig config;
		config.OversampleH = 2;
		config.OversampleV = 2;
		config.PixelSnapH = true;

		if (ImFont* loaded = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config))
			return loaded;
	}

	return ImGui::GetIO().Fonts->AddFontDefault();
}

void load_fonts()
{
	font.micro = load_font("segoeui.ttf", 10.0f);
	font.tiny = load_font("segoeui.ttf", 11.0f);
	font.small_font = load_font("segoeui.ttf", 13.0f);
	font.body = load_font("segoeui.ttf", 15.0f);
	font.hero = load_font("segoeuib.ttf", 44.0f);
	font.brand = load_font("segoeuib.ttf", 42.0f);
	font.brand_small = load_font("segoeuib.ttf", 22.0f);
	font.subtitle = load_font("segoeui.ttf", 19.0f);
	font.input = load_font("segoeui.ttf", 17.0f);
	font.button = load_font("segoeuib.ttf", 17.0f);
	font.link = load_font("segoeui.ttf", 14.0f);
}

// --------------------------------------------------------------- primitives ----

struct Rect
{
	ImVec2 min;
	ImVec2 max;
};

Rect rect(float x, float y, float x2, float y2)
{
	return Rect{ ImVec2(x, y), ImVec2(x2, y2) };
}

float width(const Rect& r)
{
	return r.max.x - r.min.x;
}

float height(const Rect& r)
{
	return r.max.y - r.min.y;
}

ImVec2 center(const Rect& r)
{
	return ImVec2(r.min.x + width(r) * 0.5f, r.min.y + height(r) * 0.5f);
}

void box(const Rect& r, ImU32 fill, float rounding = 14.0f, ImU32 border = 0, float border_size = 1.0f)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(r.min, r.max, fill, rounding);
	if (border != 0)
		draw->AddRect(r.min, r.max, border, rounding, border_size, 0);
}

void text(ImFont* f, float size, ImVec2 position, ImU32 color, const char* value)
{
	ImGui::GetWindowDrawList()->AddText(f, size, position, color, value);
}

float text_width(ImFont* f, float size, const char* value)
{
	return f->CalcTextSizeA(size, FLT_MAX, 0.0f, value).x;
}

void text_center(ImFont* f, float size, const Rect& r, ImU32 color, const char* value, float offset_y = 0.0f)
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

void soft_glow(ImVec2 position, float radius, ImU32 color, float strength, int steps = 22)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	for (int i = steps; i > 0; --i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		const float falloff = (1.0f - t) * (1.0f - t);
		draw->AddCircleFilled(position, radius * t, alpha(color, strength * falloff), 32);
	}
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
		float inset = 0.0f;

		if (mid < rounding)
			inset = rounding - std::sqrt(std::max(0.0f, rounding * rounding - (rounding - mid) * (rounding - mid)));
		else if (mid > h - rounding)
		{
			const float d = mid - (h - rounding);
			inset = rounding - std::sqrt(std::max(0.0f, rounding * rounding - d * d));
		}

		if (inset > 0.0f)
			inset = std::floor(inset) + 0.5f;

		draw->AddRectFilledMultiColor(
			ImVec2(r.min.x + inset, r.min.y + y),
			ImVec2(r.max.x - inset, r.min.y + y + step + 0.4f),
			left, right, right, left);
	}
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
		color, 3.0f, 1.5f);
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

void spinner(ImVec2 c, float radius, float thickness, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, radius, rgba(44, 55, 96, 180), 48, thickness);
	const float start = state.time * 5.2f;
	draw->PathClear();
	draw->PathArcTo(c, radius, start, start + 4.35f, 40);
	draw->PathStroke(color, thickness, 0);

	const float head = start + 4.35f;
	const ImVec2 point(c.x + std::cos(head) * radius, c.y + std::sin(head) * radius);
	draw->AddCircleFilled(point, thickness * 0.60f, mix(color, rgba(255, 255, 255), 0.55f));
}

// ---------------------------------------------------------------- branding ----

void draw_logo(ImVec2 c, float size, float opacity = 1.0f)
{
	soft_glow(c, size * 1.05f, rgba(111, 76, 255), 0.024f * opacity, 24);

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
				color = rgba(16, 8, 42, static_cast<int>(120.0f * opacity));
			}
			else
			{
				const float t = static_cast<float>(i) / static_cast<float>(count - 1);
				color = t < 0.5f
					? mix(rgba(63, 132, 255), rgba(124, 92, 255), t * 2.0f)
					: mix(rgba(124, 92, 255), rgba(176, 85, 255), (t - 0.5f) * 2.0f);
				color = alpha(color, opacity);
			}

			draw->AddCircleFilled(point, radius, color, 20);
		}
	}

	for (int i = 0; i < count; i += 3)
	{
		const ImVec2 point(points[i].x - size * 0.008f, points[i].y - size * 0.008f);
		draw->AddCircleFilled(point, radius * 0.26f, rgba(255, 255, 255, static_cast<int>(26.0f * opacity)), 12);
	}
}

float draw_wordmark_left(ImFont* wordmark_font, float x, float y, float size, float opacity)
{
	const char* first = "Sachin ";
	const char* second = "Loader";

	const ImU32 white = alpha(rgba(244, 247, 255), opacity);
	text(wordmark_font, size, ImVec2(x, y), white, first);

	float cursor = x + text_width(wordmark_font, size, first);
	const int letters = static_cast<int>(std::strlen(second));

	for (int i = 0; i < letters; ++i)
	{
		const float t = letters <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(letters - 1);
		const ImU32 color = alpha(mix(rgba(167, 139, 250), rgba(96, 165, 250), t), opacity);
		char glyph[2] = { second[i], '\0' };
		text(wordmark_font, size, ImVec2(cursor, y), color, glyph);
		cursor += text_width(wordmark_font, size, glyph);
	}

	return cursor - x;
}

void draw_wordmark_center(float cx, float y, float size, float opacity)
{
	const float total =
		text_width(font.brand, size, "Sachin ") +
		text_width(font.brand, size, "Loader");
	draw_wordmark_left(font.brand, cx - total * 0.5f, y, size, opacity);
}

// -------------------------------------------------------------- background ----

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

void draw_stars()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = app::kWidth;
	const float h = app::kHeight;
	const std::array<Star, 84>& points = stars();

	for (const auto& link : star_links())
	{
		const Star& a = points[link.first];
		const Star& b = points[link.second];
		const float twinkle = 0.5f + 0.5f * std::sin(state.time * 0.5f + a.phase + b.phase);
		draw->AddLine(
			ImVec2(a.x * w, a.y * h),
			ImVec2(b.x * w, b.y * h),
			rgba(120, 150, 255, static_cast<int>(5.0f + 7.0f * twinkle)));
	}

	for (const Star& star : points)
	{
		const float twinkle = 0.55f + 0.45f * std::sin(state.time * star.speed + star.phase);
		draw->AddCircleFilled(
			ImVec2(star.x * w, star.y * h),
			star.radius,
			rgba(168, 188, 255, static_cast<int>(18.0f + 30.0f * twinkle)),
			8);
	}
}

void draw_waves(float strength = 1.0f)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = app::kWidth;
	const float h = app::kHeight;
	const float t = state.time;

	soft_glow(ImVec2(20.0f, h + 20.0f), 330.0f, rgba(37, 99, 235), 0.012f * strength, 24);
	soft_glow(ImVec2(w - 10.0f, h + 10.0f), 360.0f, rgba(109, 67, 255), 0.012f * strength, 24);

	draw->PushClipRect(ImVec2(0.0f, h - 250.0f), ImVec2(w, h), true);

	for (int layer = 0; layer < 5; ++layer)
	{
		const float base = h - 34.0f + layer * 17.0f;
		const float amplitude = 15.0f + layer * 5.0f;
		const float phase = layer * 1.37f + t * (0.22f + layer * 0.015f);
		const ImU32 color = layer % 2 == 0
			? rgba(59, 130, 246, static_cast<int>((10.0f + layer * 1.5f) * strength))
			: rgba(139, 92, 246, static_cast<int>((9.0f + layer * 1.5f) * strength));

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

void draw_background(bool grid)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = app::kWidth;
	const float h = app::kHeight;

	draw->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(w, h),
		kBgTop, kBgTop, kBgBottom, kBgBottom);

	soft_glow(ImVec2(w * 0.50f, -30.0f), 470.0f, rgba(88, 62, 210), 0.020f, 28);
	soft_glow(ImVec2(w * 0.14f, h * 0.34f), 320.0f, rgba(30, 78, 180), 0.010f, 22);
	soft_glow(ImVec2(w * 0.90f, h * 0.76f), 400.0f, rgba(104, 53, 220), 0.010f, 22);

	if (grid)
	{
		for (float x = 72.0f; x < w; x += 72.0f)
			draw->AddLine(ImVec2(x, 0), ImVec2(x, h), rgba(255, 255, 255, 4));
		for (float y = 72.0f; y < h; y += 72.0f)
			draw->AddLine(ImVec2(0, y), ImVec2(w, y), rgba(255, 255, 255, 4));
	}

	draw_stars();
	draw_waves();

	draw->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(w * 0.16f, h),
		rgba(1, 3, 10, 90), rgba(1, 3, 10, 0),
		rgba(1, 3, 10, 0), rgba(1, 3, 10, 90));
	draw->AddRectFilledMultiColor(
		ImVec2(w * 0.84f, 0), ImVec2(w, h),
		rgba(1, 3, 10, 0), rgba(1, 3, 10, 90),
		rgba(1, 3, 10, 90), rgba(1, 3, 10, 0));
	draw->AddRectFilledMultiColor(
		ImVec2(0, h * 0.72f), ImVec2(w, h),
		rgba(2, 4, 12, 0), rgba(2, 4, 12, 0),
		rgba(2, 4, 12, 120), rgba(2, 4, 12, 120));

	draw->AddRect(ImVec2(0.5f, 0.5f), ImVec2(w - 0.5f, h - 0.5f), rgba(94, 116, 178, 34), 0.0f, 1.0f);
}

// -------------------------------------------------------------- components ----

void draw_chrome()
{
	const float w = app::kWidth;

	ImGui::SetCursorScreenPos(ImVec2(0, 0));
	ImGui::InvisibleButton("##window_drag", ImVec2(w - 104.0f, 64.0f));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	const Rect minimize = rect(w - 84.0f, 18.0f, w - 54.0f, 46.0f);
	ImGui::SetCursorScreenPos(minimize.min);
	const bool minimize_clicked = ImGui::InvisibleButton("##window_minimize", ImVec2(width(minimize), height(minimize)));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		box(minimize, alpha(rgba(255, 255, 255), 0.06f), 8.0f);
	}
	icon_minimize(center(minimize), 14.0f, rgba(150, 162, 192));

	const Rect close = rect(w - 50.0f, 18.0f, w - 20.0f, 46.0f);
	ImGui::SetCursorScreenPos(close.min);
	const bool close_clicked = ImGui::InvisibleButton("##window_close", ImVec2(width(close), height(close)));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		box(close, alpha(kRed, 0.16f), 8.0f);
	}
	icon_close(center(close), 14.0f, ImGui::IsItemHovered() ? kRed : rgba(150, 162, 192));

	if (minimize_clicked)
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (close_clicked)
		PostQuitMessage(0);
}

bool primary_button(const char* id, const Rect& r, const char* label, bool enabled = true, bool arrow = true)
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
		box(r, rgba(20, 28, 54, 190), 11.0f, rgba(255, 255, 255, 16), 1.0f);
		const ImU32 label_color = rgba(126, 138, 168);
		const float label_w = text_width(font.button, 17.0f, label);
		const float icon_w = arrow ? 16.0f : 0.0f;
		const float gap = arrow ? 10.0f : 0.0f;
		const float start = r.min.x + (width(r) - label_w - icon_w - gap) * 0.5f;
		const float cy = r.min.y + height(r) * 0.5f;
		text(font.button, 17.0f, ImVec2(start, cy - 9.0f), label_color, label);
		if (arrow)
			icon_arrow(ImVec2(start + label_w + gap + 8.0f, cy), 16.0f, label_color);
		return false;
	}

	if (t > 0.01f)
		soft_glow(center(r), width(r) * 0.58f, rgba(90, 90, 255), 0.020f * t, 20);

	ImU32 left = mix(rgba(124, 58, 237), rgba(153, 105, 255), t * 0.55f);
	ImU32 right = mix(rgba(37, 99, 235), rgba(80, 148, 255), t * 0.55f);

	if (active)
	{
		left = mix(left, rgba(0, 0, 0), 0.10f);
		right = mix(right, rgba(0, 0, 0), 0.10f);
	}

	gradient_round_rect(r, 11.0f, left, right);
	ImGui::GetWindowDrawList()->AddRect(r.min, r.max, alpha(rgba(255, 255, 255), 0.20f), 11.0f, 1.0f);

	const float label_w = text_width(font.button, 17.0f, label);
	const float icon_w = arrow ? 16.0f : 0.0f;
	const float gap = arrow ? 11.0f : 0.0f;
	const float total = label_w + icon_w + gap;
	const float start = r.min.x + (width(r) - total) * 0.5f;
	const float cy = r.min.y + height(r) * 0.5f;

	text(font.button, 17.0f, ImVec2(start, cy - 9.5f), rgba(255, 255, 255), label);
	if (arrow)
		icon_arrow(ImVec2(start + label_w + gap + 8.0f, cy), 16.0f, rgba(255, 255, 255));

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
	box(r, alpha(rgba(255, 255, 255), 0.03f + 0.04f * t), 9.0f,
		mix(kLine, kLine_strong, t), 1.0f);
	text_center(font.small_font, 13.0f, r, mix(kMuted, kText, t), label, -0.5f);
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
	const ImU32 color = mix(base_color, rgba(210, 219, 255), t);
	const ImVec2 position(r.min.x, r.min.y + (height(r) - 14.0f) * 0.5f - 1.0f);
	text(font.link, 14.0f, position, color, label);

	if (t > 0.05f)
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(position.x, position.y + 18.0f),
			ImVec2(position.x + text_width(font.link, 14.0f, label) * t, position.y + 18.0f),
			alpha(color, 0.6f), 1.0f);

	return clicked;
}

void draw_field(const char* id, const char* hint, const Rect& r, char* buffer, size_t size, bool password)
{
	const bool hovered = ImGui::IsMouseHoveringRect(r.min, r.max);
	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);

	const ImU32 fill = mix(kField, rgba(15, 25, 49, 235), t * 0.8f);
	box(r, fill, 10.0f);
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(r.min.x + 14.0f, r.min.y + 1.0f),
		ImVec2(r.max.x - 14.0f, r.min.y + 1.0f),
		alpha(rgba(255, 255, 255), 0.035f));

	ImGui::PushFont(font.input);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(50.0f, 17.0f));
	ImGui::SetCursorScreenPos(r.min);
	ImGui::SetNextItemWidth(width(r));

	ImGuiInputTextFlags flags = 0;
	if (password && !state.reveal_password)
		flags |= ImGuiInputTextFlags_Password;

	ImGui::InputTextWithHint(id, hint, buffer, size, flags);

	const bool active = ImGui::IsItemActive();
	ImGui::PopStyleVar();
	ImGui::PopFont();

	const ImU32 border = active
		? alpha(kViolet, 0.75f)
		: mix(kLine, alpha(kBlue, 0.55f), t);
	ImGui::GetWindowDrawList()->AddRect(r.min, r.max, border, 10.0f, active ? 1.5f : 1.0f);

	const ImVec2 icon_center(r.min.x + 27.0f, r.min.y + height(r) * 0.5f);
	const ImU32 icon_color = active ? rgba(179, 160, 255) : mix(kDim, kMuted, t);
	if (password)
		icon_lock(icon_center, 18.0f, icon_color);
	else
		icon_user(icon_center, 18.0f, icon_color);

	if (password)
	{
		ImGui::PushID(id);
		const Rect eye = rect(r.max.x - 48.0f, r.min.y, r.max.x - 8.0f, r.max.y);
		ImGui::SetCursorScreenPos(eye.min);
		const bool eye_clicked = ImGui::InvisibleButton("##reveal", ImVec2(width(eye), height(eye)));
		const bool eye_hovered = ImGui::IsItemHovered();
		if (eye_hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		icon_eye(center(eye), 18.0f, eye_hovered ? rgba(200, 210, 240) : rgba(120, 133, 166), state.reveal_password);
		if (eye_clicked)
			state.reveal_password = !state.reveal_password;
		ImGui::PopID();
	}
}

bool checkbox(const char* id, const Rect& r, bool* value)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);
	const Rect square = rect(r.min.x, r.min.y + (height(r) - 18.0f) * 0.5f, r.min.x + 18.0f, r.min.y + (height(r) + 18.0f) * 0.5f);

	box(square,
		*value ? alpha(kViolet, 0.85f) : alpha(rgba(8, 14, 30), 0.9f),
		4.0f,
		*value ? alpha(kViolet, 0.95f) : mix(rgba(70, 89, 132), alpha(kBlue, 0.75f), t),
		1.2f);

	if (*value)
	{
		ImDrawList* draw = ImGui::GetWindowDrawList();
		const ImVec2 c = center(square);
		draw->AddLine(ImVec2(c.x - 4.0f, c.y), ImVec2(c.x - 1.0f, c.y + 3.0f), rgba(255, 255, 255), 1.8f);
		draw->AddLine(ImVec2(c.x - 1.0f, c.y + 3.0f), ImVec2(c.x + 5.0f, c.y - 4.0f), rgba(255, 255, 255), 1.8f);
	}

	text(font.link, 14.0f, ImVec2(r.min.x + 28.0f, r.min.y + (height(r) - 14.0f) * 0.5f - 1.0f),
		mix(kMuted, kText, t * 0.7f), "Remember me");

	if (clicked)
		*value = !*value;

	return clicked;
}

void draw_banner(const Rect& r, bool error, const char* message)
{
	const float reveal = clampf(state.banner_age * 8.0f, 0.0f, 1.0f);
	const ImU32 tone = error ? kRed : rgba(96, 165, 250);

	box(r, alpha(error ? rgba(104, 20, 39) : rgba(16, 38, 74), 0.72f * reveal), 8.0f, alpha(tone, 0.42f * reveal), 1.0f);

	const ImVec2 icon_c(r.min.x + 22.0f, r.min.y + height(r) * 0.5f);
	ImGui::GetWindowDrawList()->AddCircleFilled(icon_c, 8.5f, alpha(tone, 0.85f * reveal), 20);

	if (error)
	{
		ImGui::GetWindowDrawList()->AddLine(ImVec2(icon_c.x - 3.0f, icon_c.y - 3.0f), ImVec2(icon_c.x + 3.0f, icon_c.y + 3.0f), rgba(60, 12, 24), 1.5f);
		ImGui::GetWindowDrawList()->AddLine(ImVec2(icon_c.x + 3.0f, icon_c.y - 3.0f), ImVec2(icon_c.x - 3.0f, icon_c.y + 3.0f), rgba(60, 12, 24), 1.5f);
	}
	else
	{
		text_center(font.tiny, 11.0f, rect(icon_c.x - 8.0f, icon_c.y - 8.0f, icon_c.x + 8.0f, icon_c.y + 8.0f), rgba(12, 24, 48), "i");
	}

	text(font.small_font, 13.0f, ImVec2(r.min.x + 40.0f, r.min.y + (height(r) - 13.0f) * 0.5f - 1.0f),
		alpha(error ? rgba(255, 165, 178) : rgba(170, 204, 255), reveal), message);
}

void draw_footer(float opacity = 1.0f)
{
	const float w = app::kWidth;
	const float h = app::kHeight;
	text_center_x(font.tiny, 11.0f, w * 0.5f, h - 44.0f, alpha(kDim, opacity), "Secure - Private - Fast");
}

void status_row(const Rect& card, float y, const char* label, const char* value, ImU32 dot = 0)
{
	text(font.micro, 10.0f, ImVec2(card.min.x + 24.0f, y), kDim, label);

	const float value_w = text_width(font.small_font, 13.0f, value);
	const float text_x = card.max.x - 24.0f - value_w;

	if (dot != 0)
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(text_x - 10.0f, y + 6.5f), 3.0f, dot);

	text(font.small_font, 13.0f, ImVec2(text_x, y - 1.0f), kText, value);
}

// ----------------------------------------------------------------- screens ----

void page_splash()
{
	draw_background(false);

	const float w = app::kWidth;
	const float h = app::kHeight;
	const float reveal = clampf(state.screen_time * 2.2f, 0.0f, 1.0f);
	const float progress = clampf(state.screen_time / 2.2f, 0.0f, 1.0f);
	const float pulse = 1.0f + 0.012f * std::sin(state.time * 1.7f);

	soft_glow(ImVec2(w * 0.5f, h * 0.40f), 300.0f, rgba(92, 70, 230), 0.014f * reveal, 28);
	draw_logo(ImVec2(w * 0.5f, h * 0.5f - 92.0f), 126.0f * pulse, reveal);
	draw_wordmark_center(w * 0.5f, h * 0.5f - 8.0f, 42.0f, reveal);
	text_center_x(font.subtitle, 19.0f, w * 0.5f, h * 0.5f + 48.0f, alpha(kMuted, reveal), "Secure -> Private -> Fast");

	const Rect track = rect(w * 0.5f - 160.0f, h * 0.5f + 118.0f, w * 0.5f + 160.0f, h * 0.5f + 123.0f);
	box(track, rgba(255, 255, 255, 14), 2.5f);

	if (progress > 0.002f)
	{
		const Rect fill = rect(track.min.x, track.min.y, track.min.x + width(track) * progress, track.max.y);
		gradient_round_rect(fill, 2.5f, rgba(124, 77, 255), rgba(59, 130, 246));

		const ImVec2 head(fill.max.x, center(fill).y);
		soft_glow(head, 22.0f, rgba(140, 120, 255), 0.05f, 12);
		ImGui::GetWindowDrawList()->AddCircleFilled(head, 2.6f, rgba(220, 226, 255));
	}

	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(progress * 100.0f + 0.5f));
	text(font.micro, 10.0f, ImVec2(track.max.x - text_width(font.micro, 10.0f, percent), track.min.y + 12.0f), alpha(kDim, reveal), percent);

	text_center_x(font.tiny, 11.0f, w * 0.5f, track.min.y + 12.0f, alpha(kMuted, reveal), progress < 0.98f ? "Starting secure session..." : "Ready");

	char version[32];
	std::snprintf(version, sizeof(version), "VERSION %s", app::kVersion);
	text_center_x(font.micro, 10.0f, w * 0.5f, h - 44.0f, alpha(kDim, reveal), version);
}

void page_login_form()
{
	const float w = app::kWidth;
	const float h = app::kHeight;
	const float cx = w * 0.5f;
	const float form_w = 560.0f;
	const float form_x = cx - form_w * 0.5f;

	draw_logo(ImVec2(cx, 116.0f), 92.0f);
	draw_wordmark_center(cx, 166.0f, 42.0f, 1.0f);
	text_center_x(font.subtitle, 19.0f, cx, 218.0f, kMuted, "Secure -> Private -> Fast");

	bool has_banner = state.error || state.info;
	float y = 300.0f;

	if (has_banner)
	{
		const char* message = state.error
			? "Error: Invalid username or password."
			: "Password recovery is not available in this preview.";
		draw_banner(rect(form_x, 260.0f, form_x + form_w, 304.0f), state.error, message);
		y = 322.0f;
	}

	const Rect user_field = rect(form_x, y, form_x + form_w, y + 56.0f);
	draw_field("##username", "Username or IP", user_field, state.username, sizeof(state.username), false);

	const Rect pass_field = rect(form_x, y + 70.0f, form_x + form_w, y + 126.0f);
	draw_field("##password", "Password", pass_field, state.password, sizeof(state.password), true);

	const Rect remember = rect(form_x + 2.0f, y + 134.0f, form_x + 138.0f, y + 158.0f);
	checkbox("##remember", remember, &state.remember);

	const char* forgot_label = "Forgot password?";
	const float forgot_w = text_width(font.link, 14.0f, forgot_label);
	const Rect forgot = rect(form_x + form_w - forgot_w - 4.0f, y + 134.0f, form_x + form_w, y + 158.0f);
	if (link_button("##forgot", forgot, forgot_label, rgba(147, 130, 255)))
	{
		state.info = true;
		state.error = false;
		state.banner_age = 0.0f;
	}

	const Rect login = rect(form_x, y + 176.0f, form_x + form_w, y + 230.0f);
	const bool login_clicked = primary_button("##login", login, "Login", true, true);
	const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);

	if (login_clicked || enter)
	{
		state.error = false;
		state.info = false;

		if (state.username[0] == '\0' || state.password[0] == '\0')
		{
			state.error = true;
			state.banner_age = 0.0f;
		}
		else
		{
			state.authenticating = true;
			state.auth_time = 0.0f;
		}
	}

	draw_footer();
}

void page_login_loading()
{
	const float w = app::kWidth;
	const float h = app::kHeight;
	const float cx = w * 0.5f;

	soft_glow(ImVec2(cx, 240.0f), 320.0f, rgba(92, 70, 230), 0.012f, 26);
	draw_logo(ImVec2(cx, 150.0f), 100.0f);
	draw_wordmark_center(cx, 204.0f, 42.0f, 1.0f);
	text_center_x(font.subtitle, 19.0f, cx, 256.0f, kMuted, "One Loader, All Apps!");

	const char* auth_label = "Authenticating...";
	const float group_w = 24.0f + 10.0f + text_width(font.body, 15.0f, auth_label);
	const float group_x = cx - group_w * 0.5f;
	spinner(ImVec2(group_x + 11.0f, 320.0f), 10.0f, 2.6f, rgba(176, 124, 255));
	text(font.body, 15.0f, ImVec2(group_x + 34.0f, 311.0f), rgba(190, 198, 225), auth_label);

	text_center_x(font.link, 14.0f, cx, 344.0f, kMuted, "This might take a moment");

	const Rect login = rect(cx - 190.0f, 410.0f, cx + 190.0f, 464.0f);
	primary_button("##login_busy", login, "Login", false, true);

	draw_footer();
}

void page_loader()
{
	const float w = app::kWidth;
	const float h = app::kHeight;

	// Header
	draw_logo(ImVec2(50.0f, 44.0f), 30.0f);
	draw_wordmark_left(font.brand_small, 76.0f, 30.0f, 22.0f, 1.0f);
	text(font.micro, 9.0f, ImVec2(77.0f, 56.0f), kDim, "SECURE CLIENT");

	ImGui::GetWindowDrawList()->AddLine(ImVec2(0, 86.0f), ImVec2(w, 86.0f), kLine);

	// Left column
	const float left_x = 72.0f;
	const float right_x = w - 392.0f;

	text(font.micro, 10.0f, ImVec2(left_x, 124.0f), kDim, "LOADER / READY");

	const char* title_a = "Ready when ";
	const char* title_b = "you are.";
	text(font.hero, 44.0f, ImVec2(left_x, 142.0f), kText, title_a);
	text(font.hero, 44.0f, ImVec2(left_x + text_width(font.hero, 44.0f, title_a), 142.0f), rgba(143, 128, 255), title_b);

	text(font.body, 15.0f, ImVec2(left_x + 1.0f, 202.0f), kMuted, "One Loader, All Apps! Start a new session in one click.");

	// Session card
	const Rect session = rect(left_x, 248.0f, left_x + 390.0f, 312.0f);
	box(session, kPanel_soft, 12.0f, kLine, 1.0f);
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(session.min.x + 30.0f, 280.0f), 15.0f, alpha(kViolet, 0.16f), 24);
	icon_user(ImVec2(session.min.x + 30.0f, 280.0f), 17.0f, rgba(176, 154, 255));
	text(font.micro, 9.0f, ImVec2(session.min.x + 56.0f, 262.0f), kDim, "SIGNED IN AS");
	const std::string user = state.session.empty() ? "local" : state.session;
	text(font.small_font, 14.0f, ImVec2(session.min.x + 56.0f, 279.0f), kText,
		elide(font.small_font, 14.0f, user, 250.0f).c_str());
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(session.max.x - 68.0f, 280.0f), 3.0f, kGreen);
	text(font.micro, 9.0f, ImVec2(session.max.x - 58.0f, 275.0f), kGreen, "SECURE");

	// Launch button
	const char* label = state.launching ? "Starting Loader" : (state.ready ? "Launch Again" : "Launch Loader");
	const Rect launch_btn = rect(left_x, 348.0f, left_x + 300.0f, 406.0f);
	const bool launch_clicked = primary_button("##launch_loader", launch_btn, label, !state.launching, !state.launching);

	if (launch_clicked)
	{
		state.launching = true;
		state.ready = false;
		state.launch = 0.0f;
	}

	const float progress = state.launching ? state.launch : (state.ready ? 1.0f : 0.0f);
	const Rect track = rect(left_x, 424.0f, left_x + 300.0f, 432.0f);
	box(track, rgba(255, 255, 255, 11), 4.0f);

	if (progress > 0.002f)
	{
		const Rect fill = rect(track.min.x, track.min.y, track.min.x + width(track) * progress, track.max.y);
		gradient_round_rect(fill, 4.0f, rgba(124, 58, 237), rgba(37, 99, 235));
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(fill.max.x, center(fill).y), 2.8f, rgba(215, 222, 255));
	}

	char launch_status[128];
	if (state.launching)
		std::snprintf(launch_status, sizeof(launch_status), "Initializing modules... %d%%", static_cast<int>(progress * 100.0f + 0.5f));
	else if (state.ready)
		std::snprintf(launch_status, sizeof(launch_status), "Loader is ready. All systems operational.");
	else
		std::snprintf(launch_status, sizeof(launch_status), "Awaiting launch. Your session is secure.");
	text(font.link, 14.0f, ImVec2(left_x + 1.0f, 444.0f), state.ready ? kGreen : kMuted, launch_status);

	if (ghost_button("##logout", rect(left_x, 484.0f, left_x + 110.0f, 518.0f), "Log out"))
	{
		state.authenticating = false;
		state.error = false;
		state.info = false;
		state.launching = false;
		state.ready = false;
		state.launch = 0.0f;
		state.reveal_password = false;
		state.password[0] = '\0';
		if (!state.remember)
			state.username[0] = '\0';
		go(Screen::Login);
	}

	// Status card
	const Rect card = rect(right_x, 124.0f, w - 64.0f, 584.0f);
	soft_glow(ImVec2(card.min.x + width(card) * 0.5f, 220.0f), 260.0f, rgba(86, 64, 210), 0.010f, 22);
	box(card, rgba(9, 15, 32, 225), 18.0f, kLine, 1.0f);

	text(font.micro, 10.0f, ImVec2(card.min.x + 24.0f, 148.0f), kDim, "LOADER STATUS");
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(card.max.x - 78.0f, 153.0f), 3.0f, kGreen);
	text(font.micro, 9.0f, ImVec2(card.max.x - 68.0f, 148.0f), kGreen, "ONLINE");

	const ImVec2 card_center(card.min.x + width(card) * 0.5f, 234.0f);
	draw_logo(card_center, 64.0f);
	text_center_x(font.brand_small, 22.0f, card_center.x, 278.0f, kText, "Sachin Loader");
	char card_version[32];
	std::snprintf(card_version, sizeof(card_version), "VERSION %s", app::kVersion);
	text_center_x(font.micro, 10.0f, card_center.x, 306.0f, kDim, card_version);

	ImGui::GetWindowDrawList()->AddLine(ImVec2(card.min.x + 24.0f, 334.0f), ImVec2(card.max.x - 24.0f, 334.0f), kLine);

	status_row(card, 358.0f, "SESSION", elide(font.small_font, 13.0f, user, 180.0f).c_str());
	status_row(card, 396.0f, "VERSION", app::kVersion);
	status_row(card, 434.0f, "CONNECTION", "SECURE", kGreen);
	status_row(card, 472.0f, "STATE", state.launching ? "STARTING" : (state.ready ? "READY" : "IDLE"),
		state.ready ? kGreen : (state.launching ? rgba(176, 124, 255) : kMuted));

	const Rect online = rect(card.min.x + 24.0f, 512.0f, card.max.x - 24.0f, 548.0f);
	box(online, alpha(kGreen, 0.07f), 18.0f, alpha(kGreen, 0.18f), 1.0f);
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(online.min.x + 22.0f, center(online).y), 3.0f, kGreen);
	text_center(font.micro, 9.0f, online, rgba(158, 226, 198), "ALL SYSTEMS OPERATIONAL");

	// Footer
	ImGui::GetWindowDrawList()->AddLine(ImVec2(0, h - 72.0f), ImVec2(w, h - 72.0f), kLine);
	char version[64];
	std::snprintf(version, sizeof(version), "v%s  /  %d FPS", app::kVersion, static_cast<int>(state.fps + 0.5f));
	text(font.micro, 10.0f, ImVec2(72.0f, h - 44.0f), kDim, version);
	text_center_x(font.tiny, 11.0f, w * 0.5f, h - 45.0f, kDim, "Secure - Private - Fast");
	text(font.micro, 10.0f, ImVec2(w - 72.0f - text_width(font.micro, 10.0f, "2026 SACHIN DEV"), h - 44.0f), kDim, "2026 SACHIN DEV");
}

} // namespace

void app::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	load_fonts();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(0, 0);
	style.WindowBorderSize = 0.0f;
	style.WindowRounding = 0.0f;
	style.FramePadding = ImVec2(0, 0);
	style.FrameBorderSize = 0.0f;
	style.FrameRounding = 0.0f;
	style.ItemSpacing = ImVec2(0, 0);
	style.ScrollbarSize = 0.0f;
	style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(kText);
	style.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(rgba(107, 120, 151));
	style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(kBgTop);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4(alpha(kViolet, 0.45f));
}

void app::Shutdown()
{
	g_anim.clear();
}

void app::Render()
{
	ImGuiIO& io = ImGui::GetIO();
	const float dt = clampf(io.DeltaTime, 0.0f, 0.05f);

	state.time += dt;
	state.screen_time += dt;
	state.banner_age += dt;
	state.fps += (io.Framerate - state.fps) * clampf(dt * 5.0f, 0.0f, 1.0f);

	if (state.screen == Screen::Splash && state.screen_time >= 2.2f)
	{
		go(Screen::Login);
	}

	if (state.screen == Screen::Login && state.authenticating)
	{
		state.auth_time += dt;
		if (state.auth_time >= 1.9f)
		{
			state.authenticating = false;
			state.session = state.username;
			state.password[0] = '\0';
			state.reveal_password = false;
			go(Screen::Loader);
		}
	}

	if (state.screen == Screen::Loader && state.launching)
	{
		state.launch += dt / 2.2f;
		if (state.launch >= 1.0f)
		{
			state.launch = 1.0f;
			state.launching = false;
			state.ready = true;
		}
	}

	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(app::kWidth, app::kHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##root", nullptr, flags);

	switch (state.screen)
	{
	case Screen::Splash:
		page_splash();
		break;
	case Screen::Login:
		draw_background(false);
		if (state.authenticating)
			page_login_loading();
		else
			page_login_form();
		break;
	default:
		draw_background(true);
		page_loader();
		break;
	}

	draw_chrome();

	if (state.screen_time < 0.32f)
	{
		const float fade = 1.0f - state.screen_time / 0.32f;
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(0, 0), ImVec2(app::kWidth, app::kHeight),
			rgba(2, 4, 12, static_cast<int>(fade * 190.0f)));
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
}
