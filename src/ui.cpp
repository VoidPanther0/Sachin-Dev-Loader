#include "app.h"

#include <cfloat>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Single-screen Dear ImGui starter.
//
// The whole interface lives in app::Render(). Edit the cards below to build
// your own screen. Window creation, Direct3D setup and the frame loop stay in
// main.cpp.
// ---------------------------------------------------------------------------

namespace {

// ------------------------------------------------------------------ theme ----

const ImVec4 kBg(0.043f, 0.047f, 0.059f, 1.0f);
const ImVec4 kTitleBg(0.055f, 0.059f, 0.071f, 1.0f);
const ImVec4 kPanel(0.075f, 0.082f, 0.098f, 1.0f);
const ImVec4 kPanel2(0.098f, 0.106f, 0.125f, 1.0f);
const ImVec4 kPanel3(0.129f, 0.141f, 0.165f, 1.0f);
const ImVec4 kLine(1.0f, 1.0f, 1.0f, 0.07f);
const ImVec4 kLineBright(1.0f, 1.0f, 1.0f, 0.13f);
const ImVec4 kText(0.93f, 0.94f, 0.96f, 1.0f);
const ImVec4 kMuted(0.55f, 0.58f, 0.65f, 1.0f);
const ImVec4 kDim(0.38f, 0.40f, 0.46f, 1.0f);
const ImVec4 kGreen(0.35f, 0.82f, 0.55f, 1.0f);
const ImVec4 kRed(0.86f, 0.32f, 0.38f, 1.0f);

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

// ------------------------------------------------------------------ state ----

struct AppState
{
	char display_name[64] = "Dear ImGui Template";
	char search[64] = "";
	char notes[256] = "One screen, common widgets, no backend. Use this file as your starting point.";

	int retries = 3;
	float timeout = 15.0f;
	float opacity = 0.85f;

	int environment = 0;
	int quality = 2;
	int density = 0;
	bool notifications = true;
	bool restore_session = false;

	float accent[3] = { 0.42f, 0.56f, 1.00f };
	float progress = 0.35f;
	bool running = false;

	float fps = 0.0f;
	std::string status = "Ready. Edit src/ui.cpp to start.";
};

AppState state;

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

// ------------------------------------------------------------------ fonts ----

struct Fonts
{
	ImFont* caption = nullptr;
	ImFont* body = nullptr;
	ImFont* semibold = nullptr;
	ImFont* title = nullptr;
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
	font.caption = load_font("segoeui.ttf", 12.0f);
	font.body = load_font("segoeui.ttf", 15.0f);
	font.semibold = load_font("segoeuib.ttf", 15.0f);
	font.title = load_font("segoeuib.ttf", 28.0f);

	ImGui::GetIO().FontDefault = font.body;
}

// ------------------------------------------------------------- primitives ----

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

void box(const Rect& r, const ImVec4& fill, float rounding = 10.0f, const ImVec4* border = nullptr)
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

// ---------------------------------------------------------------- widgets ----

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

// ------------------------------------------------------------------ chrome ----

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, app::kHeight), color_u32(kBg));
	draw->AddCircleFilled(ImVec2(app::kWidth - 60.0f, 0.0f), 280.0f,
		color_u32(fade(accent_color(), 0.05f)), 64);
}

void draw_title_bar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, 60.0f), color_u32(kTitleBg));
	draw->AddLine(ImVec2(0.0f, 59.0f), ImVec2(app::kWidth, 59.0f), color_u32(kLine), 1.0f);

	const Rect logo = rect(20.0f, 14.0f, 50.0f, 44.0f);
	box(logo, accent_color(), 8.0f);
	text_center(font.semibold, 13.0f, logo, ImVec4(0.04f, 0.05f, 0.08f, 1.0f), "IM");

	text(font.semibold, 14.0f, ImVec2(62.0f, 15.0f), kText, "Dear ImGui Template");
	text(font.caption, 11.0f, ImVec2(62.0f, 33.0f), kMuted, "Win32  |  DirectX 11  |  Single screen");

	ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
	ImGui::InvisibleButton("##title_drag", ImVec2(app::kWidth - 100.0f, 60.0f));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	if (window_button("##minimize", rect(app::kWidth - 84.0f, 14.0f, app::kWidth - 52.0f, 46.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (window_button("##close", rect(app::kWidth - 44.0f, 14.0f, app::kWidth - 12.0f, 46.0f), true))
		PostQuitMessage(0);
}

void draw_header()
{
	text(font.caption, 10.0f, ImVec2(36.0f, 86.0f), accent_color(), "DEAR IMGUI / STARTER");
	text(font.title, 28.0f, ImVec2(36.0f, 102.0f), kText, "Single-screen template");
	text(font.body, 13.0f, ImVec2(36.0f, 142.0f), kMuted,
		"All common ImGui widgets in one place. Keep what you need, delete the rest.");

	ImGui::SetCursorScreenPos(ImVec2(976.0f, 104.0f));
	if (secondary_button("Reset all", ImVec2(148.0f, 34.0f)))
		reset_state();

	pill(rect(1006.0f, 146.0f, 1124.0f, 168.0f), "TEMPLATE", accent_color());

	divider(36.0f, 1124.0f, 176.0f);
}

void draw_footer()
{
	divider(36.0f, 1124.0f, 676.0f);

	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(40.0f, 689.0f), 3.0f, color_u32(kGreen));

	char left[96];
	std::snprintf(left, sizeof(left), "Dear ImGui %s  |  Win32  |  DirectX 11", IMGUI_VERSION);
	text(font.caption, 10.0f, ImVec2(50.0f, 682.0f), kDim, left);

	char right[64];
	std::snprintf(right, sizeof(right), "FPS %d  |  v%s",
		static_cast<int>(state.fps + 0.5f), app::kVersion);
	const float right_width = text_width(font.caption, 10.0f, right);
	text(font.caption, 10.0f, ImVec2(1124.0f - right_width, 682.0f), kDim, right);
}

// ------------------------------------------------------------------ cards ----

void draw_inputs_card()
{
	const Rect card = rect(36.0f, 190.0f, 386.0f, 656.0f);
	draw_card(card, "Input fields", "Native text and numeric widgets.");

	const float x = 58.0f;
	const float w = 306.0f;

	label("Display name", x, 258.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 276.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::InputText("##display_name", state.display_name, sizeof(state.display_name));

	label("Search", x, 318.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 336.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::InputTextWithHint("##search", "Search the template...", state.search, sizeof(state.search));

	label("Notes", x, 378.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 396.0f));
	ImGui::InputTextMultiline("##notes", state.notes, sizeof(state.notes), ImVec2(w, 74.0f));

	label("Retries", x, 482.0f);
	label("Timeout (s)", 220.0f, 482.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 500.0f));
	ImGui::SetNextItemWidth(144.0f);
	ImGui::InputInt("##retries", &state.retries, 1, 10);

	ImGui::SetCursorScreenPos(ImVec2(220.0f, 500.0f));
	ImGui::SetNextItemWidth(144.0f);
	ImGui::InputFloat("##timeout", &state.timeout, 1.0f, 10.0f, "%.1f s");

	label("Opacity", x, 544.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 562.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::SliderFloat("##opacity", &state.opacity, 0.0f, 1.0f, "%.2f");

	text(font.caption, 11.0f, ImVec2(x, 612.0f), kDim, "Values stay local to this demo.");
}

void draw_selection_card()
{
	const Rect card = rect(404.0f, 190.0f, 754.0f, 656.0f);
	draw_card(card, "Selection", "Options, colors and state.");

	const float x = 426.0f;
	const float w = 306.0f;

	static const char* environments[] = { "Development", "Staging", "Production" };
	static const char* qualities[] = { "Low", "Medium", "High", "Ultra" };

	label("Environment", x, 258.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 276.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::Combo("##environment", &state.environment, environments, IM_ARRAYSIZE(environments));

	label("Quality", x, 318.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 336.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::Combo("##quality", &state.quality, qualities, IM_ARRAYSIZE(qualities));

	label("Accent color", x, 378.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 396.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::ColorEdit3("##accent", state.accent,
		ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB);

	divider(x, x + w, 446.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 458.0f));
	ImGui::Checkbox("Enable notifications", &state.notifications);

	ImGui::SetCursorScreenPos(ImVec2(x, 486.0f));
	ImGui::Checkbox("Restore last session", &state.restore_session);

	label("Layout density", x, 522.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 540.0f));
	ImGui::RadioButton("Comfortable", &state.density, 0);

	ImGui::SetCursorScreenPos(ImVec2(x, 568.0f));
	ImGui::RadioButton("Compact", &state.density, 1);

	ImGui::SetCursorScreenPos(ImVec2(x, 596.0f));
	ImGui::RadioButton("Dense", &state.density, 2);
}

void draw_actions_card()
{
	const Rect card = rect(772.0f, 190.0f, 1122.0f, 656.0f);
	draw_card(card, "Actions", "Buttons, progress and feedback.");

	const float x = 794.0f;
	const float w = 306.0f;

	ImGui::SetCursorScreenPos(ImVec2(x, 258.0f));
	if (ImGui::Button("Primary action", ImVec2(w, 34.0f)))
		state.status = "Primary action clicked.";

	ImGui::SetCursorScreenPos(ImVec2(x, 300.0f));
	if (secondary_button("Secondary", ImVec2(146.0f, 32.0f)))
		state.status = "Secondary action clicked.";

	ImGui::SetCursorScreenPos(ImVec2(x + 160.0f, 300.0f));
	ImGui::BeginDisabled();
	ImGui::Button("Disabled", ImVec2(146.0f, 32.0f));
	ImGui::EndDisabled();

	ImGui::SetCursorScreenPos(ImVec2(x, 342.0f));
	if (ImGui::SmallButton("Small button"))
		state.status = "Small button clicked.";
	text(font.caption, 11.0f, ImVec2(x + 94.0f, 346.0f), kDim, "Compact variant");

	divider(x, x + w, 374.0f);

	label("Demo progress", x, 386.0f);
	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(state.progress * 100.0f + 0.5f));

	ImGui::SetCursorScreenPos(ImVec2(x, 404.0f));
	ImGui::ProgressBar(state.progress, ImVec2(w, 20.0f), percent);

	label("Status", x, 434.0f);
	ImGui::PushFont(font.caption, 12.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, state.running ? accent_color() : kMuted);
	ImGui::SetCursorScreenPos(ImVec2(x, 452.0f));
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
	ImGui::TextUnformatted(state.status.c_str());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
	ImGui::PopFont();

	ImGui::SetCursorScreenPos(ImVec2(x, 492.0f));
	if (secondary_button("Run demo", ImVec2(146.0f, 32.0f)))
	{
		state.progress = 0.0f;
		state.running = true;
		state.status = "Running demo...";
	}

	ImGui::SetCursorScreenPos(ImVec2(x + 160.0f, 492.0f));
	if (secondary_button("Reset demo", ImVec2(146.0f, 32.0f)))
	{
		state.progress = 0.0f;
		state.running = false;
		state.status = "Demo reset.";
	}

	ImGui::SetCursorScreenPos(ImVec2(x, 540.0f));
	if (ImGui::TreeNodeEx("Template notes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Edit this screen in src/ui.cpp.");
		ImGui::TextDisabled("Change app::kTitle in src/app.h.");
		ImGui::TextDisabled("Dear ImGui is vendored in src/gui.");
		ImGui::TreePop();
	}
}

// ---------------------------------------------------------------- lifecycle ----

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

} // namespace

void app::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	load_fonts();
	apply_style();
}

void app::Shutdown()
{
}

void app::Render()
{
	ImGuiIO& io = ImGui::GetIO();
	state.fps += (io.Framerate - state.fps) * clampf(io.DeltaTime * 4.0f, 0.0f, 1.0f);

	if (state.running)
	{
		state.progress += io.DeltaTime * 0.45f;
		if (state.progress >= 1.0f)
		{
			state.progress = 1.0f;
			state.running = false;
			state.status = "Demo complete.";
		}
	}

	apply_accent();

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
	ImGui::SetNextWindowSize(ImVec2(app::kWidth, app::kHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##root", nullptr, flags);

	draw_background();
	draw_title_bar();
	draw_header();

	draw_inputs_card();
	draw_selection_card();
	draw_actions_card();

	draw_footer();

	ImGui::End();
	ImGui::PopStyleVar(2);
}
