#include "dashboard_common.h"
#include "imgui_internal.h"

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <shellapi.h>
#include <vector>

namespace dash {

using namespace ui;

const AccentPreset kAccentPresets[] = {
	{ "Blue",   0.42f, 0.56f, 1.00f },
	{ "Violet", 0.62f, 0.42f, 1.00f },
	{ "Cyan",   0.25f, 0.78f, 0.95f },
	{ "Green",  0.35f, 0.82f, 0.55f },
	{ "Pink",   0.95f, 0.42f, 0.68f },
	{ "Orange", 1.00f, 0.62f, 0.30f },
};

const int kAccentPresetCount = static_cast<int>(sizeof(kAccentPresets) / sizeof(kAccentPresets[0]));

Shared shared;

// ------------------------------------------------------------ preferences ----

std::wstring settings_path(bool create_directory)
{
	wchar_t appdata[MAX_PATH] = {};
	const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return std::wstring();

	std::wstring directory = std::wstring(appdata) + L"\\SachinDevLoader";
	if (create_directory)
		CreateDirectoryW(directory.c_str(), nullptr);

	return directory + L"\\settings.ini";
}

void save_preferences()
{
	const std::wstring path = settings_path(true);
	if (path.empty())
		return;

	wchar_t accent[48] = {};
	std::swprintf(accent, 48, L"%d,%d,%d",
		static_cast<int>(state.accent[0] * 255.0f + 0.5f),
		static_cast<int>(state.accent[1] * 255.0f + 0.5f),
		static_cast<int>(state.accent[2] * 255.0f + 0.5f));
	WritePrivateProfileStringW(L"ui", L"accent", accent, path.c_str());
	WritePrivateProfileStringW(L"ui", L"show_fps", shared.show_fps ? L"1" : L"0", path.c_str());
	WritePrivateProfileStringW(L"ui", L"background_glow", shared.background_glow ? L"1" : L"0", path.c_str());
}

void load_preferences()
{
	const std::wstring path = settings_path(false);
	if (path.empty())
		return;

	wchar_t accent[48] = {};
	GetPrivateProfileStringW(L"ui", L"accent", L"", accent, 48, path.c_str());
	int r = 0, g = 0, b = 0;
	if (swscanf_s(accent, L"%d,%d,%d", &r, &g, &b) == 3)
	{
		state.accent[0] = clampf(static_cast<float>(r) / 255.0f, 0.0f, 1.0f);
		state.accent[1] = clampf(static_cast<float>(g) / 255.0f, 0.0f, 1.0f);
		state.accent[2] = clampf(static_cast<float>(b) / 255.0f, 0.0f, 1.0f);
	}

	shared.show_fps = GetPrivateProfileIntW(L"ui", L"show_fps", 1, path.c_str()) != 0;
	shared.background_glow = GetPrivateProfileIntW(L"ui", L"background_glow", 1, path.c_str()) != 0;
}

void reset_preferences()
{
	state.accent[0] = kAccentPresets[0].r;
	state.accent[1] = kAccentPresets[0].g;
	state.accent[2] = kAccentPresets[0].b;
	shared.show_fps = true;
	shared.background_glow = true;
	save_preferences();
}

// ------------------------------------------------------------------ icons ----

void icon_home(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(c.x - 8.0f, c.y - 1.0f), ImVec2(c.x, c.y - 8.0f), color, 1.6f);
	draw->AddLine(ImVec2(c.x, c.y - 8.0f), ImVec2(c.x + 8.0f, c.y - 1.0f), color, 1.6f);
	draw->AddRect(ImVec2(c.x - 6.0f, c.y - 1.0f), ImVec2(c.x + 6.0f, c.y + 8.0f), color, 2.0f, 0, 1.6f);
	draw->AddLine(ImVec2(c.x, c.y + 3.0f), ImVec2(c.x, c.y + 8.0f), color, 1.6f);
}

void icon_apps(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(ImVec2(c.x - 8.0f, c.y - 8.0f), ImVec2(c.x - 1.5f, c.y - 1.5f), color, 2.0f, 0, 1.5f);
	draw->AddRect(ImVec2(c.x + 1.5f, c.y - 8.0f), ImVec2(c.x + 8.0f, c.y - 1.5f), color, 2.0f, 0, 1.5f);
	draw->AddRect(ImVec2(c.x - 8.0f, c.y + 1.5f), ImVec2(c.x - 1.5f, c.y + 8.0f), color, 2.0f, 0, 1.5f);
	draw->AddRect(ImVec2(c.x + 1.5f, c.y + 1.5f), ImVec2(c.x + 8.0f, c.y + 8.0f), color, 2.0f, 0, 1.5f);
}

void icon_download(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(c.x, c.y - 8.0f), ImVec2(c.x, c.y + 3.0f), color, 1.7f);
	draw->AddLine(ImVec2(c.x - 5.0f, c.y - 2.0f), ImVec2(c.x, c.y + 3.5f), color, 1.7f);
	draw->AddLine(ImVec2(c.x + 5.0f, c.y - 2.0f), ImVec2(c.x, c.y + 3.5f), color, 1.7f);
	draw->AddLine(ImVec2(c.x - 8.0f, c.y + 8.0f), ImVec2(c.x + 8.0f, c.y + 8.0f), color, 1.7f);
}

void icon_dll(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(ImVec2(c.x - 8.0f, c.y - 4.0f), ImVec2(c.x + 5.0f, c.y + 8.0f), color, 2.0f, 0, 1.5f);
	draw->AddLine(ImVec2(c.x - 3.0f, c.y - 8.0f), ImVec2(c.x - 3.0f, c.y - 4.0f), color, 1.5f);
	draw->AddLine(ImVec2(c.x + 2.0f, c.y - 8.0f), ImVec2(c.x + 2.0f, c.y - 4.0f), color, 1.5f);
	draw->AddLine(ImVec2(c.x + 5.0f, c.y + 2.0f), ImVec2(c.x + 9.0f, c.y + 2.0f), color, 1.5f);
}

void icon_theme(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PathClear();
	draw->PathLineTo(ImVec2(c.x, c.y - 9.0f));
	draw->PathBezierCubicCurveTo(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x + 8.0f, c.y), ImVec2(c.x + 7.5f, c.y + 3.5f), 16);
	draw->PathBezierCubicCurveTo(ImVec2(c.x + 7.0f, c.y + 8.0f), ImVec2(c.x - 7.0f, c.y + 8.0f), ImVec2(c.x - 7.5f, c.y + 3.5f), 16);
	draw->PathBezierCubicCurveTo(ImVec2(c.x - 8.0f, c.y), ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x, c.y - 9.0f), 16);
	draw->PathStroke(color, ImDrawFlags_Closed, 1.6f);
}

void icon_account(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(ImVec2(c.x, c.y - 4.0f), 4.0f, color, 20, 1.6f);
	draw->PathArcTo(ImVec2(c.x, c.y + 9.0f), 7.5f, IM_PI, IM_PI * 2.0f, 16);
	draw->PathStroke(color, 0, 1.6f);
}

void icon_settings(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, 6.5f, color, 24, 1.6f);
	draw->AddCircleFilled(c, 2.2f, color, 12);
	for (int i = 0; i < 8; ++i)
	{
		const float angle = static_cast<float>(i) * (6.28318530718f / 8.0f);
		const ImVec2 from(c.x + std::cos(angle) * 8.0f, c.y + std::sin(angle) * 8.0f);
		const ImVec2 to(c.x + std::cos(angle) * 10.5f, c.y + std::sin(angle) * 10.5f);
		draw->AddLine(from, to, color, 1.6f);
	}
}

void icon_about(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, 9.0f, color, 28, 1.6f);
	draw->AddLine(ImVec2(c.x, c.y - 1.0f), ImVec2(c.x, c.y + 5.0f), color, 1.8f);
	draw->AddCircleFilled(ImVec2(c.x, c.y - 4.5f), 1.2f, color, 8);
}

void icon_logout(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(c.x - 7.0f, c.y - 8.0f), ImVec2(c.x - 7.0f, c.y + 8.0f), color, 1.6f);
	draw->AddLine(ImVec2(c.x - 7.0f, c.y - 8.0f), ImVec2(c.x - 2.0f, c.y - 8.0f), color, 1.6f);
	draw->AddLine(ImVec2(c.x - 7.0f, c.y + 8.0f), ImVec2(c.x - 2.0f, c.y + 8.0f), color, 1.6f);
	draw->AddLine(ImVec2(c.x - 3.0f, c.y), ImVec2(c.x + 8.0f, c.y), color, 1.6f);
	draw->AddLine(ImVec2(c.x + 3.0f, c.y - 5.0f), ImVec2(c.x + 8.0f, c.y), color, 1.6f);
	draw->AddLine(ImVec2(c.x + 3.0f, c.y + 5.0f), ImVec2(c.x + 8.0f, c.y), color, 1.6f);
}

void icon_search(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(ImVec2(c.x - 1.5f, c.y - 1.5f), 5.5f, color, 20, 1.6f);
	draw->AddLine(ImVec2(c.x + 2.5f, c.y + 2.5f), ImVec2(c.x + 7.0f, c.y + 7.0f), color, 1.6f);
}

// ---------------------------------------------------------------- helpers ----

float text_wrapped(ImFont* f, float size, ImVec2 position, const ImVec4& color, const char* value, float wrap_width)
{
	ImGui::GetWindowDrawList()->AddText(f, size, position, color_u32(color), value, nullptr, wrap_width);
	return f->CalcTextSizeA(size, FLT_MAX, wrap_width, value).y;
}

void letter_badge(const Rect& r, const std::string& name, float size)
{
	gradient_box(r, fade(accent_color(), 0.85f), kPurple, 8.0f);
	char letter[2] = { name.empty() ? '?' : static_cast<char>(
		std::toupper(static_cast<unsigned char>(name[0]))), '\0' };
	text_center(font.semibold, size, r, kText, letter);
}

bool contains_ci(const std::string& haystack, const char* needle)
{
	if (needle[0] == '\0')
		return true;

	const std::size_t n = std::char_traits<char>::length(needle);
	if (n > haystack.size())
		return false;

	for (std::size_t i = 0; i + n <= haystack.size(); ++i)
	{
		std::size_t j = 0;
		while (j < n && std::tolower(static_cast<unsigned char>(haystack[i + j]))
			== std::tolower(static_cast<unsigned char>(needle[j])))
			++j;
		if (j == n)
			return true;
	}
	return false;
}

ImVec4 status_color(const std::string& status)
{
	if (status == "online")
		return kGreen;
	if (status == "beta")
		return accent_color();
	if (status == "updating")
		return kOrange;
	return kRed;
}

std::string status_label(const std::string& status)
{
	std::string label = status.empty() ? "online" : status;
	for (char& c : label)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return label;
}

bool component_launchable(const sdauth::Component& component)
{
	return !component.download_url.empty() && (component.status == "online" || component.status == "beta");
}

bool update_available()
{
	return sdauth::StartupUpdateAvailable();
}

const char* update_text()
{
	if (update_available())
		return "Update available";
	if (sdauth::UpdateStatus() == sdauth::UpdateFailed)
		return "Retrying update";
	return "Up to date";
}

ImVec4 update_color()
{
	if (update_available())
		return accent_color();
	if (sdauth::UpdateStatus() == sdauth::UpdateFailed)
		return kOrange;
	return kGreen;
}

int access_days_left()
{
	const long long expires = sdauth::AccountExpiresAt();
	if (expires <= 0)
		return -1;

	const long long left = expires - static_cast<long long>(std::time(nullptr));
	return left <= 0 ? 0 : static_cast<int>((left + 86399) / 86400);
}

std::string access_date()
{
	const long long expires = sdauth::AccountExpiresAt();
	if (expires <= 0)
		return std::string();

	const std::time_t stamp = static_cast<std::time_t>(expires);
	std::tm parts = {};
	if (localtime_s(&parts, &stamp) != 0)
		return std::string();

	char text[32];
	std::strftime(text, sizeof(text), "%Y-%m-%d", &parts);
	return text;
}

void open_url(const std::string& url)
{
	if (url.compare(0, 8, "https://") != 0)
		return;
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void open_data_folder()
{
	wchar_t appdata[MAX_PATH] = {};
	const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return;

	const std::wstring directory = std::wstring(appdata) + L"\\SachinDevLoader";
	CreateDirectoryW(directory.c_str(), nullptr);
	ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string short_hwid()
{
	const std::string& hwid = sdauth::MachineHwid();
	return hwid.size() > 12 ? hwid.substr(0, 12) + "..." : hwid;
}

std::string session_time()
{
	const ULONGLONG seconds = (GetTickCount64() - shared.session_start) / 1000ULL;
	char value[32];
	if (seconds >= 3600ULL)
		std::snprintf(value, sizeof(value), "%lluh %02llum", seconds / 3600ULL, (seconds / 60ULL) % 60ULL);
	else
		std::snprintf(value, sizeof(value), "%llum %02llus", seconds / 60ULL, seconds % 60ULL);
	return value;
}

std::string diagnostics_text()
{
	const sdauth::Session session = sdauth::GetSession();
	const std::string latest = sdauth::StartupLatestVersion();
	const std::string date = access_date();

	char value[768];
	std::snprintf(value, sizeof(value),
		"Sachin Loader diagnostics\r\n"
		"Version: %s\r\n"
		"Latest: %s\r\n"
		"Update: %s\r\n"
		"Account: %s\r\n"
		"Access until: %s\r\n"
		"Components: %d\r\n"
		"Device: %s\r\n"
		"FPS: %d\r\n",
		app::kVersion,
		latest.empty() ? app::kVersion : latest.c_str(),
		update_text(),
		session.username.empty() ? "guest" : session.username.c_str(),
		date.empty() ? "no expiry" : date.c_str(),
		static_cast<int>(sdauth::Components().size()),
		short_hwid().c_str(),
		static_cast<int>(state.fps + 0.5f));
	return value;
}

void open_component(int index)
{
	shared.selected = index;
	shared.search[0] = '\0';
	state.page = PageApplications;
}

void tick_launch()
{
	static int seen = 0;
	static double done_time = -10.0;

	// Clear a finished launch message a few seconds after it appears.
	const int status = sdauth::LaunchStatus();
	if (status != seen)
	{
		seen = status;
		done_time = ImGui::GetTime();
	}
	if ((status == sdauth::StatusOk || status == sdauth::StatusError) && ImGui::GetTime() - done_time > 8.0)
		sdauth::ResetLaunch();
}

// ---------------------------------------------------------------- widgets ----

void draw_page_header(const char* title, const char* subtitle)
{
	text(font.title, 25.0f, ImVec2(kContentLeft, 90.0f), kText, title);
	text(font.caption, 12.0f, ImVec2(kContentLeft, 124.0f), kMuted, subtitle);

	pill(rect(kContentRight - 136.0f, 94.0f, kContentRight, 118.0f), "SESSION ACTIVE", kGreen);

	// New build from loader_config.latest_version: chip that opens Downloads.
	if (!update_available())
		return;

	char label[64];
	const std::string latest = sdauth::StartupLatestVersion();
	std::snprintf(label, sizeof(label), "UPDATE %s%s", latest.empty() ? "" : "v", latest.c_str());
	const float chip_w = text_width(font.caption, 10.0f, label) + 36.0f;
	const Rect chip = rect(kContentRight - 152.0f - chip_w, 94.0f, kContentRight - 152.0f, 118.0f);

	ImGui::SetCursorScreenPos(chip.min);
	const bool clicked = ImGui::InvisibleButton("##header_update", ImVec2(width(chip), height(chip)));
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	pill(chip, label, accent_color());
	if (clicked)
		state.page = PageDownloads;
}

void draw_empty_state(const Rect& area, const char* title, const char* subtitle)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(area.min, area.max, color_u32(kBorder), 12.0f, 0, 1.0f);

	const ImVec2 c = center(area);
	draw->AddCircleFilled(ImVec2(c.x, c.y - 34.0f), 26.0f, color_u32(fade(accent_color(), 0.09f)), 40);
	draw->AddCircle(ImVec2(c.x, c.y - 34.0f), 26.0f, color_u32(fade(accent_color(), 0.35f)), 40, 1.2f);
	draw->AddLine(ImVec2(c.x - 8.0f, c.y - 34.0f), ImVec2(c.x + 8.0f, c.y - 34.0f),
		color_u32(accent_color()), 1.8f);
	draw->AddLine(ImVec2(c.x, c.y - 42.0f), ImVec2(c.x, c.y - 26.0f), color_u32(accent_color()), 1.8f);

	const float title_w = text_width(font.semibold, 16.0f, title);
	text(font.semibold, 16.0f, ImVec2(c.x - title_w * 0.5f, c.y + 6.0f), kText, title);

	const float sub_w = text_width(font.caption, 11.0f, subtitle);
	text(font.caption, 11.0f, ImVec2(c.x - sub_w * 0.5f, c.y + 30.0f), kMuted, subtitle);
}

bool text_link(const char* id, const char* value, ImVec2 position)
{
	const float w = text_width(font.semibold, 12.0f, value);
	ImGui::SetCursorScreenPos(position);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(w, 16.0f));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	text(font.semibold, 12.0f, position, hovered ? lift(accent_color(), 0.2f) : accent_color(), value);
	return clicked;
}

void stat_card(const Rect& r, const char* label, const char* value, const char* hint, const ImVec4& color)
{
	box(r, kCard, 14.0f, &kBorder);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(r.min.x + 20.0f, r.min.y + 18.0f), ImVec2(r.min.x + 23.0f, r.min.y + 30.0f),
		color_u32(color), 1.5f);
	text(font.caption, 10.0f, ImVec2(r.min.x + 30.0f, r.min.y + 17.0f), kDim, label);
	text(font.semibold, 20.0f, ImVec2(r.min.x + 20.0f, r.min.y + 36.0f), kText, value);
	text(font.caption, 11.0f, ImVec2(r.min.x + 20.0f, r.min.y + 62.0f), kMuted, hint);
}

void draw_search_box(const Rect& r, const char* hint)
{
	const bool has_text = shared.search[0] != '\0';
	box(r, kCardSoft, 9.0f, &kLine);
	icon_search(ImVec2(r.min.x + 18.0f, r.min.y + height(r) * 0.5f), color_u32(kNavText));

	constexpr float font_size = 13.0f;
	ImGui::PushFont(font.body, font_size);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, (height(r) - font_size) * 0.5f - 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, kDim);

	ImGui::SetCursorScreenPos(ImVec2(r.min.x + 34.0f, r.min.y));
	ImGui::SetNextItemWidth(width(r) - 34.0f - (has_text ? 30.0f : 10.0f));
	ImGui::InputTextWithHint("##dash_search", hint, shared.search, sizeof(shared.search));

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
	ImGui::PopFont();

	if (!has_text)
		return;

	const Rect clear = rect(r.max.x - 28.0f, r.min.y + 6.0f, r.max.x - 6.0f, r.max.y - 6.0f);
	ImGui::SetCursorScreenPos(clear.min);
	if (ImGui::InvisibleButton("##search_clear", ImVec2(width(clear), height(clear))))
		shared.search[0] = '\0';
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	const ImVec2 c = center(clear);
	const ImU32 glyph = color_u32(hovered ? kText : kMuted);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), glyph, 1.5f);
	draw->AddLine(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x - 4.0f, c.y + 4.0f), glyph, 1.5f);
}

void card_frame(const Rect& r, const char* title, const char* description)
{
	box(r, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(r.min.x + 28.0f, r.min.y + 22.0f), kText, title);
	if (description != nullptr && description[0] != '\0')
		text(font.caption, 11.0f, ImVec2(r.min.x + 28.0f, r.min.y + 48.0f), kMuted, description);
}

void info_row(const Rect& card, float y, const char* label, const char* value, const ImVec4& color)
{
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, y + 2.0f), kDim, label);
	text(font.body, 14.0f, ImVec2(card.min.x + 150.0f, y - 2.0f), color, value);
	divider(card.min.x + 28.0f, card.max.x - 28.0f, y + 22.0f);
}

bool toggle_row(const char* id, const Rect& r, const char* label, const char* hint, bool* value)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (clicked)
		*value = !*value;

	if (hovered)
		box(r, kCardSoft, 10.0f);

	text(font.semibold, 14.0f, ImVec2(r.min.x + 14.0f, r.min.y + 9.0f), kText, label);
	text(font.caption, 11.0f, ImVec2(r.min.x + 14.0f, r.min.y + 29.0f), kMuted, hint);

	// Switch.
	const float sw = 42.0f;
	const float sh = 22.0f;
	const Rect track = rect(r.max.x - 14.0f - sw, r.min.y + (height(r) - sh) * 0.5f,
		r.max.x - 14.0f, r.min.y + (height(r) + sh) * 0.5f);
	box(track, *value ? accent_color() : kPanel3, sh * 0.5f);
	const float knob_x = *value ? track.max.x - sh * 0.5f : track.min.x + sh * 0.5f;
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(knob_x, track.min.y + sh * 0.5f), sh * 0.5f - 3.0f,
		color_u32(kText), 16);
	return clicked;
}

bool card_button(const char* label, const Rect& r)
{
	ImGui::SetCursorScreenPos(r.min);
	return secondary_button(label, ImVec2(width(r), height(r)));
}

void progress_bar(const Rect& r, float fraction, const ImVec4& color)
{
	box(r, kCardSoft, height(r) * 0.5f);
	const float fill = clampf(fraction, 0.0f, 1.0f);
	if (fill > 0.01f)
		box(rect(r.min.x, r.min.y, r.min.x + width(r) * fill, r.max.y), color, height(r) * 0.5f);
}

} // namespace dash
