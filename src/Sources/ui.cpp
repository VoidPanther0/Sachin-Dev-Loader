#include "core.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <unordered_map>

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

struct Theme
{
	ImU32 bg = rgba(7, 7, 10);
	ImU32 panel = rgba(17, 17, 22);
	ImU32 panel2 = rgba(23, 23, 30);
	ImU32 panel3 = rgba(32, 32, 42);
	ImU32 line = rgba(255, 255, 255, 16);
	ImU32 line_bright = rgba(255, 255, 255, 30);
	ImU32 text = rgba(244, 244, 247);
	ImU32 muted = rgba(125, 125, 140);
	ImU32 dim = rgba(82, 82, 98);
	ImU32 green = rgba(53, 208, 127);
	ImU32 red = rgba(255, 77, 94);
};

const ImU32 kAccents[4] = {
	rgba(255, 92, 26),
	rgba(52, 214, 255),
	rgba(167, 123, 255),
	rgba(182, 255, 69),
};

Theme theme;

// ------------------------------------------------------------------ state ----

struct UiState
{
	int page = 0;
	int game = 0;
	int profile = 0;
	int accent = 0;

	bool features[5] = { true, true, false, true, true };
	bool animations = true;
	bool glow = true;
	bool show_fps = true;

	bool loading = false;
	float load = 0.0f;

	float time = 0.0f;
	float fps = 0.0f;
};

UiState state;

std::unordered_map<ImGuiID, float> g_anim;

ImU32 accent()
{
	return kAccents[state.accent & 3];
}

float animate(const char* id, float target, float speed = 14.0f)
{
	const ImGuiID key = ImGui::GetID(id);
	float& value = g_anim[key];
	if (!state.animations)
	{
		value = target;
		return value;
	}
	const float delta = ImGui::GetIO().DeltaTime * speed;
	value += (target - value) * clampf(delta, 0.0f, 1.0f);
	return value;
}

// ------------------------------------------------------------------ fonts ----

struct Fonts
{
	ImFont* micro = nullptr;
	ImFont* tiny = nullptr;
	ImFont* small_font = nullptr;
	ImFont* body = nullptr;
	ImFont* bold = nullptr;
	ImFont* medium = nullptr;
	ImFont* medium_bold = nullptr;
	ImFont* title = nullptr;
	ImFont* title_bold = nullptr;
	ImFont* display = nullptr;
	ImFont* hero = nullptr;
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

	const DWORD attributes = GetFileAttributesA(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
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
	font.bold = load_font("segoeuib.ttf", 15.0f);
	font.medium = load_font("segoeui.ttf", 18.0f);
	font.medium_bold = load_font("segoeuib.ttf", 18.0f);
	font.title = load_font("segoeuib.ttf", 26.0f);
	font.title_bold = load_font("segoeuib.ttf", 26.0f);
	font.display = load_font("segoeuib.ttf", 34.0f);
	font.hero = load_font("segoeuib.ttf", 46.0f);
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
		draw->AddRect(r.min, r.max, border, rounding, 0, border_size);
}

void shadow(const Rect& r, float size = 18.0f, ImU32 color = rgba(0, 0, 0, 90))
{
	ImGui::GetWindowDrawList()->AddShadowRect(r.min, r.max, color, size, ImVec2(0, 6), 0, 12.0f);
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

void glow(ImVec2 position, float radius, ImU32 color, int steps = 24)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	for (int i = steps; i > 0; --i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		const float falloff = (1.0f - t) * (1.0f - t);
		draw->AddCircleFilled(position, radius * t, alpha(color, falloff * 0.16f), 64);
	}
}

enum class Icon
{
	None,
	Home,
	Grid,
	Sliders,
	Settings,
	Play,
	Close,
	Minus,
	Check,
	Chevron,
	Bolt,
	User,
	Gamepad,
	Lock,
	Arrow
};

void icon(Icon kind, ImVec2 c, float size, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float s = size * 0.5f;
	const float thickness = 1.6f;

	switch (kind)
	{
	case Icon::None:
		break;
	case Icon::Home:
		draw->AddLine(ImVec2(c.x - s * 0.95f, c.y - s * 0.05f), ImVec2(c.x, c.y - s * 0.85f), color, thickness);
		draw->AddLine(ImVec2(c.x, c.y - s * 0.85f), ImVec2(c.x + s * 0.95f, c.y - s * 0.05f), color, thickness);
		draw->AddRect(ImVec2(c.x - s * 0.62f, c.y - s * 0.05f), ImVec2(c.x + s * 0.62f, c.y + s * 0.82f), color, 2.0f, 0, thickness);
		draw->AddLine(ImVec2(c.x, c.y + s * 0.82f), ImVec2(c.x, c.y + s * 0.22f), color, thickness);
		break;
	case Icon::Grid:
		draw->AddRectFilled(ImVec2(c.x - s * 0.8f, c.y - s * 0.8f), ImVec2(c.x - s * 0.08f, c.y - s * 0.08f), color, 2.0f);
		draw->AddRectFilled(ImVec2(c.x + s * 0.08f, c.y - s * 0.8f), ImVec2(c.x + s * 0.8f, c.y - s * 0.08f), color, 2.0f);
		draw->AddRectFilled(ImVec2(c.x - s * 0.8f, c.y + s * 0.08f), ImVec2(c.x - s * 0.08f, c.y + s * 0.8f), color, 2.0f);
		draw->AddRectFilled(ImVec2(c.x + s * 0.08f, c.y + s * 0.08f), ImVec2(c.x + s * 0.8f, c.y + s * 0.8f), color, 2.0f);
		break;
	case Icon::Sliders:
	case Icon::Settings:
		draw->AddLine(ImVec2(c.x - s * 0.9f, c.y - s * 0.55f), ImVec2(c.x + s * 0.9f, c.y - s * 0.55f), color, thickness);
		draw->AddLine(ImVec2(c.x - s * 0.9f, c.y), ImVec2(c.x + s * 0.9f, c.y), color, thickness);
		draw->AddLine(ImVec2(c.x - s * 0.9f, c.y + s * 0.55f), ImVec2(c.x + s * 0.9f, c.y + s * 0.55f), color, thickness);
		draw->AddCircleFilled(ImVec2(c.x - s * 0.25f, c.y - s * 0.55f), s * 0.22f, color);
		draw->AddCircleFilled(ImVec2(c.x + s * 0.38f, c.y), s * 0.22f, color);
		draw->AddCircleFilled(ImVec2(c.x - s * 0.05f, c.y + s * 0.55f), s * 0.22f, color);
		break;
	case Icon::Play:
		draw->AddTriangleFilled(
			ImVec2(c.x - s * 0.35f, c.y - s * 0.62f),
			ImVec2(c.x - s * 0.35f, c.y + s * 0.62f),
			ImVec2(c.x + s * 0.68f, c.y), color);
		break;
	case Icon::Close:
		draw->AddLine(ImVec2(c.x - s * 0.48f, c.y - s * 0.48f), ImVec2(c.x + s * 0.48f, c.y + s * 0.48f), color, thickness);
		draw->AddLine(ImVec2(c.x + s * 0.48f, c.y - s * 0.48f), ImVec2(c.x - s * 0.48f, c.y + s * 0.48f), color, thickness);
		break;
	case Icon::Minus:
		draw->AddLine(ImVec2(c.x - s * 0.5f, c.y), ImVec2(c.x + s * 0.5f, c.y), color, thickness);
		break;
	case Icon::Check:
		draw->AddLine(ImVec2(c.x - s * 0.55f, c.y), ImVec2(c.x - s * 0.1f, c.y + s * 0.45f), color, thickness);
		draw->AddLine(ImVec2(c.x - s * 0.1f, c.y + s * 0.45f), ImVec2(c.x + s * 0.6f, c.y - s * 0.45f), color, thickness);
		break;
	case Icon::Chevron:
		draw->AddLine(ImVec2(c.x - s * 0.25f, c.y - s * 0.45f), ImVec2(c.x + s * 0.25f, c.y), color, thickness);
		draw->AddLine(ImVec2(c.x + s * 0.25f, c.y), ImVec2(c.x - s * 0.25f, c.y + s * 0.45f), color, thickness);
		break;
	case Icon::Bolt:
	{
		draw->AddTriangleFilled(
			ImVec2(c.x + s * 0.08f, c.y - s * 0.9f),
			ImVec2(c.x - s * 0.55f, c.y + s * 0.08f),
			ImVec2(c.x - s * 0.06f, c.y + s * 0.08f), color);
		draw->AddTriangleFilled(
			ImVec2(c.x - s * 0.18f, c.y + s * 0.9f),
			ImVec2(c.x + s * 0.6f, c.y - s * 0.16f),
			ImVec2(c.x + s * 0.08f, c.y - s * 0.16f), color);
		break;
	}
	case Icon::User:
		draw->AddCircleFilled(ImVec2(c.x, c.y - s * 0.35f), s * 0.38f, color);
		draw->AddCircleFilled(ImVec2(c.x, c.y + s * 0.78f), s * 0.72f, alpha(color, 0.55f), 32);
		break;
	case Icon::Gamepad:
		draw->AddRectFilled(ImVec2(c.x - s * 0.95f, c.y - s * 0.38f), ImVec2(c.x + s * 0.95f, c.y + s * 0.45f), color, s * 0.45f);
		draw->AddCircleFilled(ImVec2(c.x - s * 0.82f, c.y + s * 0.55f), s * 0.38f, color);
		draw->AddCircleFilled(ImVec2(c.x + s * 0.82f, c.y + s * 0.55f), s * 0.38f, color);
		draw->AddCircleFilled(ImVec2(c.x - s * 0.5f, c.y + s * 0.02f), s * 0.12f, rgba(0, 0, 0, 180));
		draw->AddCircleFilled(ImVec2(c.x + s * 0.5f, c.y + s * 0.02f), s * 0.12f, rgba(0, 0, 0, 180));
		break;
	case Icon::Lock:
		draw->AddRectFilled(ImVec2(c.x - s * 0.6f, c.y - s * 0.05f), ImVec2(c.x + s * 0.6f, c.y + s * 0.78f), color, 3.0f);
		draw->PathClear();
		draw->PathArcTo(ImVec2(c.x, c.y - s * 0.08f), s * 0.42f, 3.14159f, 6.28318f, 24);
		draw->PathStroke(color, 0, 1.8f);
		break;
	case Icon::Arrow:
		draw->AddLine(ImVec2(c.x - s * 0.6f, c.y), ImVec2(c.x + s * 0.55f, c.y), color, thickness);
		draw->AddLine(ImVec2(c.x + s * 0.1f, c.y - s * 0.45f), ImVec2(c.x + s * 0.55f, c.y), color, thickness);
		draw->AddLine(ImVec2(c.x + s * 0.1f, c.y + s * 0.45f), ImVec2(c.x + s * 0.55f, c.y), color, thickness);
		break;
	}
}

// --------------------------------------------------------------- components ----

bool button(const char* id, const Rect& r, const char* label, bool primary, Icon glyph = Icon::None)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();

	const float t = animate(id, hovered ? 1.0f : 0.0f, 14.0f);
	const float press = active ? 0.06f : 0.0f;

	ImU32 fill = primary
		? mix(accent(), mix(accent(), rgba(255, 255, 255), 0.18f), t)
		: mix(theme.panel2, theme.panel3, t);
	fill = mix(fill, rgba(0, 0, 0), press);

	const ImU32 border = primary ? alpha(accent(), 0.55f) : (t > 0.01f ? theme.line_bright : theme.line);
	box(r, fill, 10.0f, border, 1.0f);

	const ImU32 label_color = primary ? rgba(255, 255, 255) : mix(theme.muted, theme.text, t);
	const float label_size = 13.0f;
	const float label_width = text_width(font.small_font, label_size, label);
	const float icon_size = glyph == Icon::None ? 0.0f : 14.0f;
	const float gap = glyph == Icon::None ? 0.0f : 7.0f;
	const float total = label_width + icon_size + gap;
	const float start_x = r.min.x + (width(r) - total) * 0.5f;
	const float center_y = r.min.y + height(r) * 0.5f;

	if (glyph != Icon::None)
		icon(glyph, ImVec2(start_x + icon_size * 0.5f, center_y), icon_size, label_color);

	text(font.small_font, label_size, ImVec2(start_x + icon_size + gap, center_y - label_size * 0.5f - 1.0f), label_color, label);
	return clicked;
}

bool icon_button(const char* id, const Rect& r, Icon glyph)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	const float t = animate(id, hovered ? 1.0f : 0.0f, 16.0f);

	if (t > 0.01f)
		box(r, alpha(rgba(255, 255, 255), 0.06f * t), 8.0f, 0, 0.0f);

	icon(glyph, center(r), 14.0f, mix(theme.muted, theme.text, t));
	return clicked;
}

void pill(const Rect& r, const char* label, ImU32 color)
{
	box(r, alpha(color, 0.08f), height(r) * 0.5f, alpha(color, 0.18f), 1.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec2 dot(r.min.x + 14.0f, r.min.y + height(r) * 0.5f);
	draw->AddCircleFilled(dot, 3.0f, color);

	const float size = 10.0f;
	const float y = r.min.y + (height(r) - size) * 0.5f - 1.0f;
	text(font.micro, size, ImVec2(r.min.x + 24.0f, y), mix(color, theme.text, 0.35f), label);
}

void chip(const Rect& r, const char* label)
{
	box(r, theme.panel2, height(r) * 0.5f, theme.line, 1.0f);
	text_center(font.micro, 9.0f, r, theme.muted, label);
}

void switch_visual(const Rect& r, float t)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImU32 track = mix(theme.panel3, accent(), t);
	box(r, track, height(r) * 0.5f, 0, 0.0f);

	const float radius = height(r) * 0.34f;
	const float x = r.min.x + radius + 3.0f + (width(r) - radius * 2.0f - 6.0f) * t;
	draw->AddCircleFilled(ImVec2(x, r.min.y + height(r) * 0.5f), radius, rgba(255, 255, 255));
}

bool nav_item(int index, const char* label, Icon glyph, const Rect& r)
{
	static const char* ids[3] = { "##nav_dashboard", "##nav_loader", "##nav_settings" };

	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(ids[index], ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	const bool selected = state.page == index;
	const float t = animate(ids[index], hovered ? 1.0f : 0.0f, 14.0f);

	if (selected)
	{
		box(r, alpha(accent(), 0.10f), 10.0f, alpha(accent(), 0.18f), 1.0f);
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(r.min.x, r.min.y + 10.0f),
			ImVec2(r.min.x + 3.0f, r.max.y - 10.0f),
			accent(), 2.0f);
	}
	else if (t > 0.01f)
	{
		box(r, alpha(rgba(255, 255, 255), 0.035f * t), 10.0f, 0, 0.0f);
	}

	const ImU32 glyph_color = selected ? accent() : mix(theme.dim, theme.muted, t);
	const ImU32 label_color = selected ? theme.text : mix(theme.dim, theme.text, t);

	icon(glyph, ImVec2(r.min.x + 24.0f, r.min.y + height(r) * 0.5f), 16.0f, glyph_color);
	text(font.small_font, 13.0f, ImVec2(r.min.x + 44.0f, r.min.y + height(r) * 0.5f - 7.0f), label_color, label);

	if (selected)
		ImGui::GetWindowDrawList()->AddCircleFilled(
			ImVec2(r.max.x - 18.0f, r.min.y + height(r) * 0.5f), 2.5f, accent());

	return clicked;
}

// ------------------------------------------------------------------- data ----

struct Game
{
	const char* name;
	const char* mark;
	const char* tag;
	ImU32 color;
};

const Game kGames[6] = {
	{ "VALORANT", "VA", "TACTICAL FPS", rgba(255, 70, 85) },
	{ "RUST", "RU", "SURVIVAL", rgba(217, 130, 43) },
	{ "FORTNITE", "FN", "BATTLE ROYALE", rgba(139, 124, 255) },
	{ "TARKOV", "EFT", "EXTRACTION", rgba(163, 170, 184) },
	{ "WARZONE", "MW", "BATTLE ROYALE", rgba(143, 209, 79) },
	{ "MORE TITLES", "+", "COMING SOON", rgba(70, 70, 84) },
};

const char* kFeatureNames[5] = {
	"Aimbot",
	"ESP / Visuals",
	"Radar",
	"No Recoil",
	"Stream Proof",
};

const char* kFeatureDesc[5] = {
	"Smooth target assistance",
	"Player and item overlay",
	"Extended minimap awareness",
	"Recoil compensation",
	"Hide overlay from capture",
};

// ------------------------------------------------------------------ panels ----

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0, 0), ImVec2(app::kWidth, app::kHeight), theme.bg);

	if (state.glow)
	{
		glow(ImVec2(1030.0f, 10.0f), 520.0f, accent(), 22);
		glow(ImVec2(150.0f, 760.0f), 480.0f, rgba(95, 78, 255), 22);
	}

	for (float x = 232.0f; x < app::kWidth; x += 54.0f)
		draw->AddLine(ImVec2(x, 76.0f), ImVec2(x, app::kHeight), rgba(255, 255, 255, 5));

	for (float y = 76.0f; y < app::kHeight; y += 54.0f)
		draw->AddLine(ImVec2(232.0f, y), ImVec2(app::kWidth, y), rgba(255, 255, 255, 5));
}

void draw_sidebar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(ImVec2(0, 0), ImVec2(232, app::kHeight), rgba(10, 10, 14));
	draw->AddLine(ImVec2(231, 0), ImVec2(231, app::kHeight), theme.line);

	draw->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(232, 130),
		alpha(accent(), 0.10f), alpha(accent(), 0.0f),
		alpha(accent(), 0.0f), alpha(accent(), 0.0f));

	const Rect logo = rect(24, 26, 66, 68);
	box(logo, accent(), 12.0f, alpha(rgba(255, 255, 255), 0.18f), 1.0f);
	draw->AddRectFilled(
		ImVec2(logo.min.x + 2, logo.min.y + 2),
		ImVec2(logo.max.x - 2, logo.min.y + 22),
		alpha(rgba(255, 255, 255), 0.10f), 10.0f);

	text_center(font.medium_bold, 22.0f, logo, rgba(8, 8, 10), "S");

	text(font.medium_bold, 17.0f, ImVec2(78, 26), theme.text, "SACHIN");
	text(font.micro, 9.0f, ImVec2(79, 51), theme.muted, "DEV LOADER");

	text(font.micro, 9.0f, ImVec2(28, 108), theme.dim, "MENU");
	draw->AddLine(ImVec2(68, 113), ImVec2(208, 113), theme.line);

	const char* nav_labels[3] = { "Dashboard", "Loader", "Settings" };
	const Icon nav_icons[3] = { Icon::Home, Icon::Gamepad, Icon::Sliders };

	for (int i = 0; i < 3; ++i)
	{
		const Rect item = rect(20, 126 + i * 52, 212, 168 + i * 52);
		if (nav_item(i, nav_labels[i], nav_icons[i], item))
		{
			state.page = i;
			if (i == 1)
			{
				state.loading = false;
				state.load = 0.0f;
			}
		}
	}

	const Rect status = rect(20, 516, 212, 646);
	box(status, theme.panel, 12.0f, theme.line, 1.0f);

	draw->AddCircleFilled(ImVec2(38, 538), 3.5f, theme.green);
	text(font.small_font, 12.0f, ImVec2(48, 530), theme.text, "UI READY");
	text(font.micro, 9.0f, ImVec2(36, 552), theme.muted, "Local preview build");

	draw->AddLine(ImVec2(36, 576), ImVec2(196, 576), theme.line);

	text(font.micro, 8.0f, ImVec2(36, 590), theme.dim, "VERSION");
	text(font.medium, 16.0f, ImVec2(36, 604), theme.text, app::kVersion);
	text(font.micro, 8.0f, ImVec2(138, 610), theme.dim, "STABLE");

	text(font.micro, 8.0f, ImVec2(24, 670), theme.dim, "2026 SACHIN DEV");
}

void draw_top_bar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	static const char* names[3] = { "DASHBOARD", "LOADER", "SETTINGS" };

	const char* current = names[state.page];
	text(font.micro, 9.0f, ImVec2(260, 27), theme.muted, "SACHIN DEV");
	const float first = text_width(font.micro, 9.0f, "SACHIN DEV");
	text(font.micro, 9.0f, ImVec2(260 + first + 10, 27), theme.dim, "/");
	text(font.micro, 9.0f, ImVec2(260 + first + 22, 27), theme.text, current);

	const Rect online = rect(876, 21, 984, 53);
	pill(online, "SYSTEM READY", theme.green);

	if (icon_button("##minimize", rect(1012, 20, 1048, 56), Icon::Minus))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (icon_button("##close", rect(1052, 20, 1088, 56), Icon::Close))
		PostQuitMessage(0);

	draw->AddLine(ImVec2(232, 76), ImVec2(app::kWidth, 76), theme.line);

	ImGui::SetCursorScreenPos(ImVec2(232, 0));
	ImGui::InvisibleButton("##drag_content", ImVec2(620, 76));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	ImGui::SetCursorScreenPos(ImVec2(0, 0));
	ImGui::InvisibleButton("##drag_sidebar", ImVec2(232, 76));
	if (ImGui::IsItemClicked())
		StartWindowDrag();
}

void draw_status_bar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(232, 660), ImVec2(app::kWidth, 660), theme.line);

	draw->AddCircleFilled(ImVec2(260, 680), 3.0f, theme.green);
	text(font.micro, 9.0f, ImVec2(270, 674), theme.text, "READY");
	text(font.micro, 9.0f, ImVec2(310, 674), theme.dim, "UI PREVIEW");

	char fps[32];
	std::snprintf(fps, sizeof(fps), "FPS %d", static_cast<int>(state.fps + 0.5f));

	if (state.show_fps)
		text(font.micro, 9.0f, ImVec2(986, 674), theme.dim, fps);

	text(font.micro, 9.0f, ImVec2(1042, 674), theme.dim, "v");
	text(font.micro, 9.0f, ImVec2(1049, 674), theme.muted, app::kVersion);
}

void page_dashboard()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	text(font.title_bold, 27.0f, ImVec2(260, 92), theme.text, "Dashboard");
	text(font.small_font, 12.0f, ImVec2(260, 129), theme.muted,
		"Welcome back. Pick a title and build your loadout.");

	pill(rect(900, 100, 1092, 134), "ALL SYSTEMS ONLINE", theme.green);

	const Rect hero = rect(260, 166, 1092, 356);
	shadow(hero, 22.0f, rgba(0, 0, 0, 110));
	box(hero, rgba(18, 15, 21), 16.0f, alpha(accent(), 0.18f), 1.0f);

	draw->PushClipRect(hero.min, hero.max, true);
	if (state.glow)
	{
		glow(ImVec2(1010, 176), 230.0f, accent(), 26);
		draw->AddCircle(ImVec2(1010, 176), 92.0f, alpha(accent(), 0.12f), 64, 1.0f);
		draw->AddCircle(ImVec2(1010, 176), 132.0f, alpha(accent(), 0.07f), 64, 1.0f);
	}
	draw->PopClipRect();

	text(font.display, 32.0f, ImVec2(288, 190), theme.text, "SACHIN DEV");
	const float title_width = text_width(font.display, 32.0f, "SACHIN DEV");
	text(font.display, 32.0f, ImVec2(288 + title_width + 8, 190), accent(), "LOADER");

	text(font.small_font, 12.0f, ImVec2(288, 242), theme.muted,
		"A clean launcher interface built for speed.");

	if (button("##launch_loader", rect(288, 278, 458, 316), "LAUNCH LOADER", true, Icon::Play))
	{
		state.page = 1;
		state.loading = false;
		state.load = 0.0f;
	}

	if (button("##hero_settings", rect(470, 278, 590, 316), "SETTINGS", false, Icon::Sliders))
		state.page = 2;

	draw->AddLine(ImVec2(792, 190), ImVec2(792, 334), theme.line);

	const char* stat_values[3] = { "06", "15", "UI" };
	const char* stat_labels[3] = { "GAMES", "MODULES", "BUILD" };
	const float stat_x[3] = { 824.0f, 922.0f, 1020.0f };

	for (int i = 0; i < 3; ++i)
	{
		text(font.display, 28.0f, ImVec2(stat_x[i], 218), i == 2 ? accent() : theme.text, stat_values[i]);
		text(font.micro, 8.0f, ImVec2(stat_x[i] + 1, 254), theme.dim, stat_labels[i]);
	}

	text(font.micro, 9.0f, ImVec2(260, 380), theme.dim, "SELECT A GAME");
	text(font.micro, 9.0f, ImVec2(1032, 380), theme.dim, "06 AVAILABLE");

	for (int i = 0; i < 6; ++i)
	{
		const int column = i % 3;
		const int row = i / 3;
		const float x = 260.0f + column * 282.0f;
		const float y = 398.0f + row * 130.0f;

		static const char* ids[6] = {
			"##game_0", "##game_1", "##game_2",
			"##game_3", "##game_4", "##game_5"
		};

		Rect card = rect(x, y, x + 268.0f, y + 118.0f);

		ImGui::SetCursorScreenPos(card.min);
		const bool clicked = ImGui::InvisibleButton(ids[i], ImVec2(width(card), height(card)));
		const bool hovered = ImGui::IsItemHovered();
		const bool selected = state.game == i && i < 5;
		const float t = animate(ids[i], hovered ? 1.0f : 0.0f, 13.0f);

		card.min.y -= 2.0f * t;
		card.max.y -= 2.0f * t;

		shadow(card, 14.0f, rgba(0, 0, 0, 70 + static_cast<int>(t * 40.0f)));
		box(card,
			mix(theme.panel, theme.panel2, t * 0.8f),
			13.0f,
			selected ? alpha(accent(), 0.7f) : (t > 0.01f ? theme.line_bright : theme.line),
			selected ? 1.5f : 1.0f);

		const Rect art = rect(card.min.x + 10.0f, card.min.y + 10.0f, card.max.x - 10.0f, card.min.y + 68.0f);
		box(art, mix(rgba(28, 28, 38), alpha(kGames[i].color, 0.30f), 0.55f), 9.0f, 0, 0.0f);

		draw->PushClipRect(art.min, art.max, true);
		for (float line_x = art.min.x - 30.0f; line_x < art.max.x + 40.0f; line_x += 20.0f)
			draw->AddLine(ImVec2(line_x, art.max.y), ImVec2(line_x + 70.0f, art.min.y), rgba(255, 255, 255, 8));
		draw->PopClipRect();

		text(font.medium_bold, 19.0f, ImVec2(art.min.x + 12, art.min.y + 18),
			alpha(rgba(255, 255, 255), 0.8f), kGames[i].mark);

		if (i < 5)
		{
			draw->AddCircleFilled(ImVec2(art.max.x - 45, art.min.y + 20), 2.5f, theme.green);
			text(font.micro, 8.0f, ImVec2(art.max.x - 37, art.min.y + 15), theme.green, "READY");
		}
		else
		{
			text(font.micro, 8.0f, ImVec2(art.max.x - 64, art.min.y + 15), theme.dim, "SOON");
		}

		text(font.bold, 14.0f, ImVec2(card.min.x + 12, card.min.y + 80), theme.text, kGames[i].name);
		text(font.micro, 8.0f, ImVec2(card.min.x + 12, card.min.y + 99), theme.dim, kGames[i].tag);

		icon(Icon::Chevron, ImVec2(card.max.x - 18.0f, card.min.y + 88.0f), 10.0f,
			mix(theme.dim, accent(), t));

		if (clicked && i < 5)
		{
			state.game = i;
			state.page = 1;
			state.loading = false;
			state.load = 0.0f;
		}
	}
}

void feature_row(int index, float y, float x, float w)
{
	static const char* ids[5] = {
		"##feature_aimbot", "##feature_esp", "##feature_radar",
		"##feature_recoil", "##feature_stream"
	};
	static const char* on_ids[5] = {
		"##on_aimbot", "##on_esp", "##on_radar",
		"##on_recoil", "##on_stream"
	};

	const Rect row = rect(x, y, x + w, y + 44.0f);
	ImGui::SetCursorScreenPos(row.min);
	const bool clicked = ImGui::InvisibleButton(ids[index], ImVec2(width(row), height(row)));
	const bool hovered = ImGui::IsItemHovered();
	const bool enabled = state.features[index];
	const float t = animate(ids[index], hovered ? 1.0f : 0.0f, 12.0f);
	const float on = animate(on_ids[index], enabled ? 1.0f : 0.0f, 18.0f);

	if (t > 0.01f)
		box(row, alpha(rgba(255, 255, 255), 0.025f * t), 8.0f, 0, 0.0f);

	if (enabled)
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(row.min.x, row.min.y + 11.0f),
			ImVec2(row.min.x + 2.0f, row.max.y - 11.0f),
			alpha(accent(), 0.8f), 1.0f);

	text(font.body, 13.0f, ImVec2(row.min.x + 10.0f, row.min.y + 7.0f),
		mix(theme.text, theme.muted, 1.0f - t), kFeatureNames[index]);
	text(font.micro, 8.0f, ImVec2(row.min.x + 10.0f, row.min.y + 25.0f), theme.dim, kFeatureDesc[index]);

	const Rect track = rect(row.max.x - 50.0f, row.min.y + 13.0f, row.max.x - 10.0f, row.min.y + 33.0f);
	switch_visual(track, on);

	if (index < 4)
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(row.min.x + 10.0f, row.max.y), ImVec2(row.max.x, row.max.y),
			theme.line, 1.0f);

	if (clicked)
		state.features[index] = !state.features[index];
}

void profile_row(int index, const char* label, float y, float x, float w)
{
	static const char* ids[3] = { "##profile_default", "##profile_performance", "##profile_stream" };
	const Rect row = rect(x, y, x + w, y + 32.0f);
	const bool selected = state.profile == index;

	ImGui::SetCursorScreenPos(row.min);
	const bool clicked = ImGui::InvisibleButton(ids[index], ImVec2(width(row), height(row)));
	const bool hovered = ImGui::IsItemHovered();
	const float t = animate(ids[index], hovered ? 1.0f : 0.0f, 12.0f);

	box(row,
		selected ? alpha(accent(), 0.11f) : alpha(rgba(255, 255, 255), 0.02f * t),
		7.0f,
		selected ? alpha(accent(), 0.28f) : (t > 0.01f ? theme.line : 0),
		1.0f);

	text(font.small_font, 12.0f, ImVec2(row.min.x + 11.0f, row.min.y + 9.0f),
		selected ? theme.text : theme.muted, label);

	if (selected)
		icon(Icon::Check, ImVec2(row.max.x - 17.0f, row.min.y + 16.0f), 10.0f, accent());

	if (clicked)
		state.profile = index;
}

void page_loader()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const int game_index = state.game < 5 ? state.game : 0;
	const Game& game = kGames[game_index];

	text(font.title_bold, 27.0f, ImVec2(260, 92), theme.text, "Loader");
	text(font.small_font, 12.0f, ImVec2(260, 129), theme.muted,
		"Configure modules for your selected title.");

	if (button("##back", rect(1000, 96, 1092, 130), "BACK", false, Icon::Arrow))
		state.page = 0;

	const Rect card = rect(260, 166, 780, 620);
	shadow(card, 20.0f, rgba(0, 0, 0, 100));
	box(card, theme.panel, 16.0f, theme.line, 1.0f);

	const Rect art = rect(272, 178, 768, 316);
	box(art, mix(rgba(27, 27, 36), alpha(game.color, 0.34f), 0.55f), 12.0f, 0, 0.0f);

	draw->PushClipRect(art.min, art.max, true);
	if (state.glow)
		glow(ImVec2(690, 236), 150.0f, game.color, 20);
	for (float line_x = art.min.x - 40.0f; line_x < art.max.x + 50.0f; line_x += 22.0f)
		draw->AddLine(ImVec2(line_x, art.max.y), ImVec2(line_x + 90.0f, art.min.y), rgba(255, 255, 255, 9));
	draw->PopClipRect();

	text(font.hero, 48.0f, ImVec2(292, 196), alpha(rgba(255, 255, 255), 0.9f), game.mark);
	text(font.title_bold, 23.0f, ImVec2(292, 250), theme.text, game.name);
	text(font.small_font, 10.0f, ImVec2(292, 282), theme.muted, game.tag);

	pill(rect(642, 194, 748, 224), "READY", theme.green);

	text(font.micro, 9.0f, ImVec2(292, 342), theme.dim, "MODULES");
	text(font.micro, 9.0f, ImVec2(666, 342), theme.dim, "05 AVAILABLE");

	for (int i = 0; i < 5; ++i)
		feature_row(i, 360.0f + i * 44.0f, 282.0f, 474.0f);

	text(font.micro, 8.0f, ImVec2(292, 594), theme.dim, "Visual configuration only. Nothing is executed.");

	const Rect status = rect(800, 166, 1092, 398);
	shadow(status, 18.0f, rgba(0, 0, 0, 90));
	box(status, theme.panel, 16.0f, theme.line, 1.0f);

	text(font.micro, 9.0f, ImVec2(820, 186), theme.dim, "LOAD STATUS");
	pill(rect(1014, 180, 1070, 206), state.loading ? "BUSY" : (state.load >= 0.999f ? "DONE" : "IDLE"),
		state.loading ? accent() : (state.load >= 0.999f ? theme.green : theme.muted));

	const ImVec2 ring_center(946, 268);
	const float radius = 39.0f;
	const float progress = state.loading ? state.load : (state.load >= 0.999f ? 1.0f : 0.0f);

	draw->AddCircle(ring_center, radius, theme.panel3, 64, 7.0f);
	if (progress > 0.001f)
	{
		draw->PathClear();
		draw->PathArcTo(ring_center, radius, -1.570796f, -1.570796f + 6.283185f * progress, 64);
		draw->PathStroke(accent(), 0, 7.0f);
	}

	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(progress * 100.0f + 0.5f));
	text_center(font.medium_bold, 18.0f, rect(ring_center.x - 40, ring_center.y - 16, ring_center.x + 40, ring_center.y + 16),
		theme.text, percent);

	const char* status_text = state.loading
		? "Preparing module..."
		: (state.load >= 0.999f ? "Module ready" : "Ready to load");
	text_center(font.small_font, 11.0f, rect(810, 316, 1082, 336), theme.muted, status_text);

	const char* button_label = state.loading ? "LOADING" : (state.load >= 0.999f ? "RELOAD" : "LOAD GAME");
	if (button("##load_game", rect(820, 344, 1072, 382), button_label, true, Icon::Bolt))
	{
		if (!state.loading)
		{
			state.loading = true;
			state.load = 0.0f;
		}
	}

	const Rect profile = rect(800, 412, 1092, 620);
	shadow(profile, 18.0f, rgba(0, 0, 0, 90));
	box(profile, theme.panel, 16.0f, theme.line, 1.0f);

	text(font.micro, 9.0f, ImVec2(820, 432), theme.dim, "PROFILE");
	text(font.small_font, 11.0f, ImVec2(820, 450), theme.muted, "Visual configuration");

	profile_row(0, "Default", 472.0f, 820.0f, 252.0f);
	profile_row(1, "Performance", 508.0f, 820.0f, 252.0f);
	profile_row(2, "Stream Safe", 544.0f, 820.0f, 252.0f);

	text(font.micro, 8.0f, ImVec2(820, 590), theme.dim, "UI PREVIEW ONLY");
}

void accent_swatch(int index, float x, float y)
{
	static const char* ids[4] = { "##accent_orange", "##accent_cyan", "##accent_violet", "##accent_lime" };
	const Rect swatch = rect(x, y, x + 48.0f, y + 48.0f);
	const bool selected = state.accent == index;

	ImGui::SetCursorScreenPos(swatch.min);
	const bool clicked = ImGui::InvisibleButton(ids[index], ImVec2(48.0f, 48.0f));
	const bool hovered = ImGui::IsItemHovered();
	const float t = animate(ids[index], hovered ? 1.0f : 0.0f, 14.0f);

	box(swatch, mix(kAccents[index], rgba(255, 255, 255), 0.12f * t), 13.0f,
		selected ? rgba(255, 255, 255) : alpha(rgba(255, 255, 255), 0.08f),
		selected ? 2.0f : 1.0f);

	if (selected)
		icon(Icon::Check, center(swatch), 15.0f, rgba(10, 10, 12));

	if (clicked)
		state.accent = index;
}

void setting_row(int index, const char* label, const char* desc, bool* value, float y, float x, float w)
{
	static const char* ids[3] = { "##setting_anim", "##setting_glow", "##setting_fps" };
	static const char* on_ids[3] = { "##setting_anim_on", "##setting_glow_on", "##setting_fps_on" };
	const Rect row = rect(x, y, x + w, y + 48.0f);

	ImGui::SetCursorScreenPos(row.min);
	const bool clicked = ImGui::InvisibleButton(ids[index], ImVec2(width(row), height(row)));
	const bool hovered = ImGui::IsItemHovered();
	const float t = animate(ids[index], hovered ? 1.0f : 0.0f, 12.0f);
	const float on = animate(on_ids[index], *value ? 1.0f : 0.0f, 18.0f);

	if (t > 0.01f)
		box(row, alpha(rgba(255, 255, 255), 0.025f * t), 8.0f, 0, 0.0f);

	text(font.body, 13.0f, ImVec2(row.min.x + 8.0f, row.min.y + 8.0f), theme.text, label);
	text(font.micro, 8.0f, ImVec2(row.min.x + 8.0f, row.min.y + 26.0f), theme.dim, desc);

	switch_visual(rect(row.max.x - 50.0f, row.min.y + 14.0f, row.max.x - 10.0f, row.min.y + 34.0f), on);

	if (index < 2)
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(row.min.x + 8.0f, row.max.y), ImVec2(row.max.x, row.max.y),
			theme.line, 1.0f);

	if (clicked)
		*value = !*value;
}

void page_settings()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	text(font.title_bold, 27.0f, ImVec2(260, 92), theme.text, "Settings");
	text(font.small_font, 12.0f, ImVec2(260, 129), theme.muted,
		"Tune the interface to match your setup.");

	const Rect accent_card = rect(260, 166, 656, 428);
	shadow(accent_card, 16.0f, rgba(0, 0, 0, 80));
	box(accent_card, theme.panel, 16.0f, theme.line, 1.0f);

	text(font.micro, 9.0f, ImVec2(284, 186), theme.dim, "ACCENT COLOR");
	text(font.medium_bold, 17.0f, ImVec2(284, 204), theme.text, "Interface color");
	text(font.small_font, 11.0f, ImVec2(284, 232), theme.muted,
		"Used for highlights, buttons and active states.");

	for (int i = 0; i < 4; ++i)
		accent_swatch(i, 284.0f + i * 72.0f, 266.0f);

	const char* accent_names[4] = { "ORANGE", "CYAN", "VIOLET", "LIME" };
	for (int i = 0; i < 4; ++i)
		text(font.micro, 8.0f, ImVec2(284.0f + i * 72.0f, 322.0f), theme.dim, accent_names[i]);

	draw->AddLine(ImVec2(284, 352), ImVec2(632, 352), theme.line);

	text(font.micro, 8.0f, ImVec2(284, 368), theme.dim, "PREVIEW");
	box(rect(336, 362, 480, 382), accent(), 10.0f, alpha(rgba(255, 255, 255), 0.18f), 1.0f);
	text_center(font.micro, 9.0f, rect(336, 362, 480, 382), rgba(10, 10, 12), "ACTIVE ELEMENT");

	const Rect interface_card = rect(676, 166, 1092, 428);
	shadow(interface_card, 16.0f, rgba(0, 0, 0, 80));
	box(interface_card, theme.panel, 16.0f, theme.line, 1.0f);

	text(font.micro, 9.0f, ImVec2(700, 186), theme.dim, "INTERFACE");
	text(font.medium_bold, 17.0f, ImVec2(700, 204), theme.text, "Display options");

	setting_row(0, "Animations", "Smooth hover and transition effects", &state.animations, 244.0f, 700.0f, 368.0f);
	setting_row(1, "Glow effects", "Ambient light behind panels", &state.glow, 304.0f, 700.0f, 368.0f);
	setting_row(2, "FPS counter", "Show frame rate in the status bar", &state.show_fps, 364.0f, 700.0f, 368.0f);

	const Rect about = rect(260, 448, 1092, 618);
	shadow(about, 16.0f, rgba(0, 0, 0, 80));
	box(about, theme.panel, 16.0f, theme.line, 1.0f);

	text(font.micro, 9.0f, ImVec2(284, 468), theme.dim, "ABOUT");
	text(font.medium_bold, 18.0f, ImVec2(284, 486), theme.text, "Sachin Dev Loader");
	text(font.micro, 8.0f, ImVec2(284, 514), theme.dim, "VERSION 2.0.0  /  UI BUILD");

	text(font.small_font, 11.0f, ImVec2(284, 536), theme.muted,
		"Pure interface build. No network calls, no process access, no hardware IDs.");

	chip(rect(284, 566, 344, 590), "LOCAL");
	chip(rect(354, 566, 414, 590), "UI ONLY");
	chip(rect(424, 566, 510, 590), "NO BACKEND");

	if (button("##reset_ui", rect(944, 486, 1068, 522), "RESET UI", false, Icon::Sliders))
	{
		state.accent = 0;
		state.profile = 0;
		state.features[0] = true;
		state.features[1] = true;
		state.features[2] = false;
		state.features[3] = true;
		state.features[4] = true;
		state.animations = true;
		state.glow = true;
		state.show_fps = true;
	}
}

} // namespace

void app::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.Fonts->TexDesiredWidth = 2048;

	load_fonts();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(0, 0);
	style.WindowBorderSize = 0.0f;
	style.WindowRounding = 0.0f;
	style.FramePadding = ImVec2(0, 0);
	style.ItemSpacing = ImVec2(0, 0);
	style.ScrollbarSize = 0.0f;
	style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(theme.text);
	style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(theme.bg);
}

void app::Shutdown()
{
	g_anim.clear();
}

void app::Render()
{
	ImGuiIO& io = ImGui::GetIO();
	state.time += io.DeltaTime;
	state.fps += (io.Framerate - state.fps) * clampf(io.DeltaTime * 4.0f, 0.0f, 1.0f);

	if (state.loading)
	{
		state.load += io.DeltaTime * 0.55f;
		if (state.load >= 1.0f)
		{
			state.load = 1.0f;
			state.loading = false;
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

	draw_background();
	draw_sidebar();
	draw_top_bar();

	switch (state.page)
	{
	case 0: page_dashboard(); break;
	case 1: page_loader(); break;
	default: page_settings(); break;
	}

	draw_status_bar();

	ImGui::End();
	ImGui::PopStyleVar(2);
}
