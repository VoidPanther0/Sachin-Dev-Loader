#include "ui.h"

#include "auth/supabase_auth.h"

#include <cfloat>
#include <cstdio>
#include <string>

// Shared UI helpers and app lifecycle.
//
// Screens:
//   src/screen/login.cpp   startup check, login form, signing phases
//   src/screen/home.cpp    main dashboard

namespace ui {

float clampf(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

ImVec4 lift(const ImVec4& color, float amount)
{
	return ImVec4(
		clampf(color.x + amount, 0.0f, 1.0f),
		clampf(color.y + amount, 0.0f, 1.0f),
		clampf(color.z + amount, 0.0f, 1.0f),
		color.w);
}

ImVec4 dim(const ImVec4& color, float amount)
{
	return lift(color, -amount);
}

ImVec4 fade(const ImVec4& color, float alpha)
{
	return ImVec4(color.x, color.y, color.z, color.w * alpha);
}

ImU32 color_u32(const ImVec4& color)
{
	return ImGui::ColorConvertFloat4ToU32(color);
}

State state;

ImVec4 accent_color()
{
	return ImVec4(state.accent[0], state.accent[1], state.accent[2], 1.0f);
}

void reset_state()
{
	std::snprintf(state.display_name, sizeof(state.display_name), "%s", "Dear ImGui Template");
	std::snprintf(state.search, sizeof(state.search), "%s", "");
	std::snprintf(state.notes, sizeof(state.notes), "%s",
		"One screen, common widgets, no backend. Use this file as your starting point.");

	state.retries = 3;
	state.timeout = 15.0f;
	state.opacity = 0.85f;
	state.environment = 0;
	state.quality = 2;
	state.density = 0;
	state.notifications = true;
	state.restore_session = false;

	state.accent[0] = 0.42f;
	state.accent[1] = 0.56f;
	state.accent[2] = 1.00f;

	state.progress = 0.35f;
	state.running = false;
	state.status = "Defaults restored.";
}

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
	font.caption = load_font("segoeui.ttf", 12.0f);
	font.body = load_font("segoeui.ttf", 15.0f);
	font.semibold = load_font("segoeuib.ttf", 15.0f);
	font.title = load_font("segoeuib.ttf", 28.0f);

	ImGui::GetIO().FontDefault = font.body;
}

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

void box(const Rect& r, const ImVec4& fill, float rounding, const ImVec4* border)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(r.min, r.max, color_u32(fill), rounding);
	if (border != nullptr)
		draw->AddRect(r.min, r.max, color_u32(*border), rounding, 0, 1.0f);
}

void text(ImFont* f, float size, ImVec2 position, const ImVec4& color, const char* value)
{
	ImGui::GetWindowDrawList()->AddText(f, size, position, color_u32(color), value);
}

float text_width(ImFont* f, float size, const char* value)
{
	return f->CalcTextSizeA(size, FLT_MAX, 0.0f, value).x;
}

void text_center(ImFont* f, float size, const Rect& r, const ImVec4& color, const char* value)
{
	const float w = text_width(f, size, value);
	const float y = r.min.y + (height(r) - size) * 0.5f - 1.0f;
	text(f, size, ImVec2(r.min.x + (width(r) - w) * 0.5f, y), color, value);
}

void label(const char* value, float x, float y)
{
	text(font.caption, 12.0f, ImVec2(x, y), kMuted, value);
}

void divider(float x1, float x2, float y)
{
	ImGui::GetWindowDrawList()->AddLine(ImVec2(x1, y), ImVec2(x2, y), color_u32(kLine), 1.0f);
}

void pill(const Rect& r, const char* value, const ImVec4& color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(r.min, r.max, color_u32(fade(color, 0.10f)), height(r) * 0.5f);
	draw->AddRect(r.min, r.max, color_u32(fade(color, 0.22f)), height(r) * 0.5f, 0, 1.0f);
	draw->AddCircleFilled(ImVec2(r.min.x + 12.0f, r.min.y + height(r) * 0.5f), 2.5f, color_u32(color));
	text(font.caption, 10.0f, ImVec2(r.min.x + 20.0f, r.min.y + (height(r) - 10.0f) * 0.5f - 1.0f),
		lift(color, 0.18f), value);
}

bool secondary_button(const char* value, const ImVec2& size)
{
	ImGui::PushStyleColor(ImGuiCol_Button, kPanel2);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kPanel3);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, dim(kPanel3, 0.03f));
	const bool clicked = ImGui::Button(value, size);
	ImGui::PopStyleColor(3);
	return clicked;
}

bool window_button(const char* id, const Rect& r, bool close)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();

	ImDrawList* draw = ImGui::GetWindowDrawList();
	if (hovered)
	{
		const ImVec4 hover = close ? fade(kRed, 0.85f) : kPanel3;
		box(r, hover, 7.0f);
	}

	const ImVec2 c = center(r);
	const ImU32 glyph = color_u32(hovered ? kText : kMuted);

	if (close)
	{
		draw->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), glyph, 1.5f);
		draw->AddLine(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x - 4.0f, c.y + 4.0f), glyph, 1.5f);
	}
	else
	{
		draw->AddLine(ImVec2(c.x - 5.0f, c.y), ImVec2(c.x + 5.0f, c.y), glyph, 1.5f);
	}

	return clicked;
}

void draw_card(const Rect& r, const char* title, const char* description)
{
	box(r, kPanel, 14.0f, &kLine);
	text(font.semibold, 17.0f, ImVec2(r.min.x + 22.0f, r.min.y + 20.0f), kText, title);
	text(font.caption, 12.0f, ImVec2(r.min.x + 22.0f, r.min.y + 46.0f), kMuted, description);
}

void apply_style()
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowPadding = ImVec2(0.0f, 0.0f);
	style.FramePadding = ImVec2(12.0f, 8.0f);
	style.ItemSpacing = ImVec2(10.0f, 8.0f);
	style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
	style.ScrollbarSize = 10.0f;
	style.GrabMinSize = 10.0f;

	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;

	style.WindowRounding = 0.0f;
	style.ChildRounding = 10.0f;
	style.FrameRounding = 8.0f;
	style.PopupRounding = 10.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 8.0f;
	style.TabRounding = 8.0f;

	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;

	style.Colors[ImGuiCol_Text] = kText;
	style.Colors[ImGuiCol_TextDisabled] = kMuted;
	style.Colors[ImGuiCol_WindowBg] = kBg;
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = kPanel;
	style.Colors[ImGuiCol_Border] = kLine;
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	style.Colors[ImGuiCol_FrameBg] = kPanel2;
	style.Colors[ImGuiCol_FrameBgHovered] = kPanel3;
	style.Colors[ImGuiCol_FrameBgActive] = lift(kPanel3, 0.02f);
	style.Colors[ImGuiCol_CheckMark] = accent_color();
	style.Colors[ImGuiCol_SliderGrab] = accent_color();
	style.Colors[ImGuiCol_SliderGrabActive] = lift(accent_color(), 0.10f);

	style.Colors[ImGuiCol_Button] = accent_color();
	style.Colors[ImGuiCol_ButtonHovered] = lift(accent_color(), 0.08f);
	style.Colors[ImGuiCol_ButtonActive] = dim(accent_color(), 0.06f);

	style.Colors[ImGuiCol_Header] = fade(accent_color(), 0.16f);
	style.Colors[ImGuiCol_HeaderHovered] = fade(accent_color(), 0.24f);
	style.Colors[ImGuiCol_HeaderActive] = fade(accent_color(), 0.32f);

	style.Colors[ImGuiCol_Separator] = kLine;
	style.Colors[ImGuiCol_SeparatorHovered] = kLineBright;
	style.Colors[ImGuiCol_SeparatorActive] = accent_color();
	style.Colors[ImGuiCol_TextSelectedBg] = fade(accent_color(), 0.35f);
	style.Colors[ImGuiCol_PlotHistogram] = accent_color();

	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = kPanel3;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = lift(kPanel3, 0.05f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = lift(kPanel3, 0.09f);
}

void apply_accent()
{
	ImGuiStyle& style = ImGui::GetStyle();
	const ImVec4 accent = accent_color();

	style.Colors[ImGuiCol_CheckMark] = accent;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = lift(accent, 0.10f);
	style.Colors[ImGuiCol_Button] = accent;
	style.Colors[ImGuiCol_ButtonHovered] = lift(accent, 0.08f);
	style.Colors[ImGuiCol_ButtonActive] = dim(accent, 0.06f);
	style.Colors[ImGuiCol_Header] = fade(accent, 0.16f);
	style.Colors[ImGuiCol_HeaderHovered] = fade(accent, 0.24f);
	style.Colors[ImGuiCol_HeaderActive] = fade(accent, 0.32f);
	style.Colors[ImGuiCol_SeparatorActive] = accent;
	style.Colors[ImGuiCol_TextSelectedBg] = fade(accent, 0.35f);
	style.Colors[ImGuiCol_PlotHistogram] = accent;
}

} // namespace ui

void app::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ui::load_fonts();
	ui::apply_style();

	sdauth::Init();
	screen_login::Init();
}

void app::Shutdown()
{
	screen_login::Shutdown();
}

void app::Render()
{
	sdauth::Tick();

	if (screen_login::IsLoggedIn() && !sdauth::HasSession())
		screen_login::SessionLost();

	ImGuiIO& io = ImGui::GetIO();
	ui::state.fps += (io.Framerate - ui::state.fps) * ui::clampf(io.DeltaTime * 4.0f, 0.0f, 1.0f);

	ui::apply_accent();

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

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##root", nullptr, flags);

	if (!screen_login::IsLoggedIn())
	{
		screen_login::Render();
		ImGui::End();
		ImGui::PopStyleVar(2);
		return;
	}

	screen_home::Render();

	ImGui::End();
	ImGui::PopStyleVar(2);
}
