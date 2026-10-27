#include "../ui.h"
#include "../auth/supabase_auth.h"

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <shellapi.h>
#include <string>
#include <vector>

// Main window shell: title bar, sidebar, one page of content and a footer.
//
// Pages: Home (dashboard), Library (server components with search and launch),
// News (release notes) and Settings (appearance, account, loader info, help).
// The sidebar, title bar and window buttons are shared by every page.
//
// News, the announcement banner, the support link, component status and
// account expiry all come from the server; see supabase/migrations.

namespace screen_home {

namespace {

using namespace ui;

// ------------------------------------------------------------------ theme ----

const ImVec4 kHomeBg(0.020f, 0.024f, 0.043f, 1.0f);
const ImVec4 kSidebar(0.031f, 0.039f, 0.063f, 1.0f);
const ImVec4 kCard(0.043f, 0.055f, 0.090f, 1.0f);
const ImVec4 kCardSoft(0.055f, 0.071f, 0.115f, 1.0f);
const ImVec4 kBorder(0.30f, 0.38f, 0.62f, 0.22f);
const ImVec4 kNavText(0.55f, 0.61f, 0.76f, 1.0f);
const ImVec4 kBlue(0.17f, 0.33f, 0.86f, 1.0f);
const ImVec4 kPurple(0.52f, 0.29f, 0.93f, 1.0f);
const ImVec4 kOrange(1.00f, 0.62f, 0.30f, 1.0f);

constexpr float kTitleHeight = 64.0f;
constexpr float kSidebarWidth = 232.0f;
constexpr float kContentLeft = 264.0f;
constexpr float kContentRight = 1132.0f;

enum Page { PageHome = 0, PageLibrary = 1, PageNews = 2, PageSettings = 3 };

// Sidebar entries that open a link instead of a page.
constexpr int kIconSupport = 4;

struct AccentPreset { const char* name; float r, g, b; };

const AccentPreset kAccentPresets[] = {
	{ "Blue",   0.42f, 0.56f, 1.00f },
	{ "Violet", 0.62f, 0.42f, 1.00f },
	{ "Cyan",   0.25f, 0.78f, 0.95f },
	{ "Green",  0.35f, 0.82f, 0.55f },
	{ "Pink",   0.95f, 0.42f, 0.68f },
	{ "Orange", 1.00f, 0.62f, 0.30f },
};

// Release notes used until the server sends its own list (offline, or no rows
// in public.loader_news). Newest first.
struct BuiltinNews { const char* version; const char* date; const char* tag; const char* title; const char* body; };

const BuiltinNews kNews[] = {
	{ "0.3.2", "2026-10-27", "UPDATE", "Cleaner releases",
		"Release builds no longer publish to Supabase from CI. The database schema was rebuilt into one migration." },
	{ "0.3.1", "2026-10-26", "FIX", "Legacy update helper",
		"Clients still on 0.1 keep a legacy helper, so they can update to the new in-place swap." },
	{ "0.3.0", "2026-10-26", "FEATURE", "Fully automatic updates",
		"The startup check downloads the new build, verifies its SHA-256, replaces the exe and restarts. No clicks needed." },
	{ "0.2.2", "2026-10-26", "FIX", "In-place exe swap",
		"The separate update helper is gone. The running exe becomes <exe>.old and the new build takes its place." },
	{ "0.2.1", "2026-10-26", "FIX", "Update button after startup",
		"The update button stays visible after the startup screen and the restart is confirmed." },
	{ "0.2", "2026-10-26", "FEATURE", "Update before login",
		"Updates run on the startup screen before the login form opens." },
	{ "0.1", "2026-10-26", "RELEASE", "First release",
		"Supabase login with hardware binding, server build checks and a component list." },
};

// Per-session dashboard state. Reset when a new session opens the home screen.
struct HomeState
{
	bool show_fps = true;
	bool background_glow = true;
	int last_frame = -2;
	ULONGLONG session_start = 0;
	char search[64] = "";
	int selected = 0;
	double copied_time = -10.0;
	double diagnostics_time = -10.0;
	double launch_done_time = -10.0;
	int launch_seen = 0;
};

HomeState home;

// ------------------------------------------------------------ preferences ----

// Appearance settings live next to the remembered username in settings.ini.
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
	WritePrivateProfileStringW(L"ui", L"show_fps", home.show_fps ? L"1" : L"0", path.c_str());
	WritePrivateProfileStringW(L"ui", L"background_glow", home.background_glow ? L"1" : L"0", path.c_str());
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

void icon_library(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(ImVec2(c.x - 8.0f, c.y - 8.0f), ImVec2(c.x + 2.0f, c.y + 2.0f), color, 2.5f, 0, 1.5f);
	draw->AddRect(ImVec2(c.x - 2.0f, c.y - 2.0f), ImVec2(c.x + 8.0f, c.y + 8.0f), color, 2.5f, 0, 1.5f);
	draw->AddLine(ImVec2(c.x - 8.0f, c.y + 5.0f), ImVec2(c.x - 4.0f, c.y + 8.0f), color, 1.5f);
}

void icon_news(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(ImVec2(c.x - 8.0f, c.y - 8.0f), ImVec2(c.x + 8.0f, c.y + 8.0f), color, 2.5f, 0, 1.5f);
	draw->AddLine(ImVec2(c.x - 4.5f, c.y - 3.5f), ImVec2(c.x + 4.5f, c.y - 3.5f), color, 1.5f);
	draw->AddLine(ImVec2(c.x - 4.5f, c.y + 0.5f), ImVec2(c.x + 4.5f, c.y + 0.5f), color, 1.5f);
	draw->AddLine(ImVec2(c.x - 4.5f, c.y + 4.5f), ImVec2(c.x + 1.0f, c.y + 4.5f), color, 1.5f);
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

void icon_support(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, 8.5f, color, 24, 1.6f);
	draw->PathClear();
	draw->PathArcTo(ImVec2(c.x, c.y - 2.2f), 2.8f, 3.14159265f, 6.28318530f + 1.1f, 10);
	draw->PathLineTo(ImVec2(c.x, c.y + 2.0f));
	draw->PathStroke(color, 0, 1.5f);
	draw->AddCircleFilled(ImVec2(c.x, c.y + 5.0f), 1.0f, color, 8);
}

void icon_search(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(ImVec2(c.x - 1.5f, c.y - 1.5f), 5.5f, color, 20, 1.6f);
	draw->AddLine(ImVec2(c.x + 2.5f, c.y + 2.5f), ImVec2(c.x + 7.0f, c.y + 7.0f), color, 1.6f);
}

// ---------------------------------------------------------------- helpers ----

// Draws text wrapped at wrap_width and returns its height.
float text_wrapped(ImFont* f, float size, ImVec2 position, const ImVec4& color, const char* value, float wrap_width)
{
	ImGui::GetWindowDrawList()->AddText(f, size, position, color_u32(color), value, nullptr, wrap_width);
	return f->CalcTextSizeA(size, FLT_MAX, wrap_width, value).y;
}

// Small square avatar with the first letter of a name.
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

const char* update_text()
{
	if (sdauth::StartupUpdateAvailable())
		return "Update available";
	if (sdauth::UpdateStatus() == sdauth::UpdateFailed)
		return "Retrying update";
	return "Up to date";
}

ImVec4 update_color()
{
	if (sdauth::StartupUpdateAvailable())
		return accent_color();
	if (sdauth::UpdateStatus() == sdauth::UpdateFailed)
		return kBlue;
	return kGreen;
}

ImVec4 tag_color(const std::string& tag)
{
	if (tag == "FEATURE")
		return accent_color();
	if (tag == "FIX")
		return kOrange;
	if (tag == "UPDATE")
		return kBlue;
	if (tag == "NOTICE")
		return kOrange;
	return kGreen; // RELEASE and anything unknown
}

// Server news, or the built-in list while the server has sent none.
std::vector<sdauth::NewsItem> news_items()
{
	std::vector<sdauth::NewsItem> items = sdauth::News();
	if (!items.empty())
		return items;

	for (const BuiltinNews& entry : kNews)
		items.push_back({ entry.version, entry.date, entry.tag, entry.title, entry.body });
	return items;
}

// Component state label: online, updating, offline or beta.
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

// Days left until the account expires, or -1 for no expiry.
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

// Plain text a user can paste to support. No password, token or full device ID.
std::string diagnostics_text()
{
	const sdauth::Session session = sdauth::GetSession();
	const std::string latest = sdauth::StartupLatestVersion();
	const std::string date = access_date();

	char text[768];
	std::snprintf(text, sizeof(text),
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
	return text;
}

void open_component(int index)
{
	home.selected = index;
	home.search[0] = '\0';
	state.page = PageLibrary;
}

// ----------------------------------------------------------------- chrome ----

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, app::kHeight), color_u32(kHomeBg));
	if (!home.background_glow)
		return;

	draw->AddCircleFilled(ImVec2(app::kWidth - 40.0f, 20.0f), 320.0f,
		color_u32(fade(kPurple, 0.05f)), 64);
	draw->AddCircleFilled(ImVec2(180.0f, app::kHeight + 60.0f), 260.0f,
		color_u32(fade(kBlue, 0.04f)), 64);
}

void draw_title_bar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, kTitleHeight), color_u32(kSidebar));
	draw->AddLine(ImVec2(0.0f, kTitleHeight - 1.0f), ImVec2(app::kWidth, kTitleHeight - 1.0f), color_u32(kLine), 1.0f);

	const Rect logo = rect(20.0f, 16.0f, 48.0f, 44.0f);
	gradient_box(logo, kBlue, kPurple, 8.0f);
	text_center(font.semibold, 17.0f, logo, kText, "S");

	text(font.semibold, 15.0f, ImVec2(60.0f, 14.0f), kText, "Sachin Loader");
	text(font.caption, 10.0f, ImVec2(60.0f, 34.0f), kMuted, "Apps and games, one place");

	ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
	ImGui::InvisibleButton("##title_drag", ImVec2(app::kWidth - 96.0f, kTitleHeight));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	if (window_button("##minimize", rect(app::kWidth - 84.0f, 14.0f, app::kWidth - 52.0f, 46.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (window_button("##close", rect(app::kWidth - 44.0f, 14.0f, app::kWidth - 12.0f, 46.0f), true))
		PostQuitMessage(0);
}

bool nav_item(const char* id, int page, const char* label, int icon, float y)
{
	const Rect r = rect(16.0f, y, kSidebarWidth - 16.0f, y + 44.0f);
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	const bool active = state.page == page;
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	if (active)
	{
		box(r, fade(accent_color(), 0.14f), 10.0f);
		draw->AddRect(r.min, r.max, color_u32(fade(accent_color(), 0.35f)), 10.0f, 0, 1.0f);
		draw->AddRectFilled(ImVec2(r.min.x + 1.0f, r.min.y + 11.0f), ImVec2(r.min.x + 3.0f, r.max.y - 11.0f),
			color_u32(accent_color()), 2.0f);
	}
	else if (hovered)
	{
		box(r, kCardSoft, 10.0f);
	}

	const ImVec2 icon_center(r.min.x + 28.0f, r.min.y + 22.0f);
	const ImU32 icon_color = color_u32(active ? lift(accent_color(), 0.15f) : kNavText);
	switch (icon)
	{
	case PageHome: icon_home(icon_center, icon_color); break;
	case PageLibrary: icon_library(icon_center, icon_color); break;
	case PageNews: icon_news(icon_center, icon_color); break;
	case kIconSupport: icon_support(icon_center, icon_color); break;
	default: icon_settings(icon_center, icon_color); break;
	}

	text(font.semibold, 14.0f, ImVec2(r.min.x + 48.0f, r.min.y + 13.0f),
		active ? kText : kNavText, label);
	return clicked;
}

void draw_sidebar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, kTitleHeight), ImVec2(kSidebarWidth, app::kHeight), color_u32(kSidebar));
	draw->AddLine(ImVec2(kSidebarWidth - 1.0f, kTitleHeight), ImVec2(kSidebarWidth - 1.0f, app::kHeight),
		color_u32(kLine), 1.0f);

	text(font.caption, 10.0f, ImVec2(28.0f, 88.0f), kDim, "WORKSPACE");
	if (nav_item("##nav_home", PageHome, "Home", PageHome, 108.0f))
		state.page = PageHome;
	if (nav_item("##nav_library", PageLibrary, "Library", PageLibrary, 160.0f))
		state.page = PageLibrary;
	if (nav_item("##nav_news", PageNews, "News", PageNews, 212.0f))
		state.page = PageNews;

	text(font.caption, 10.0f, ImVec2(28.0f, 280.0f), kDim, "PREFERENCES");
	if (nav_item("##nav_settings", PageSettings, "Settings", PageSettings, 300.0f))
		state.page = PageSettings;

	const std::string support_url = sdauth::SupportUrl();
	if (!support_url.empty() && nav_item("##nav_support", -1, "Support", kIconSupport, 352.0f))
		open_url(support_url);

	// Component count badge next to Library.
	const int count = static_cast<int>(sdauth::Components().size());
	if (count > 0)
	{
		char badge[16];
		std::snprintf(badge, sizeof(badge), "%d", count);
		const float w = text_width(font.caption, 10.0f, badge) + 14.0f;
		const Rect b = rect(kSidebarWidth - 30.0f - w, 172.0f, kSidebarWidth - 30.0f, 192.0f);
		box(b, fade(accent_color(), 0.18f), 10.0f);
		text_center(font.caption, 10.0f, b, lift(accent_color(), 0.2f), badge);
	}

	// Account block at the bottom of the sidebar.
	const Rect account = rect(16.0f, 556.0f, kSidebarWidth - 16.0f, 620.0f);
	box(account, kCard, 12.0f, &kBorder);

	const sdauth::Session session = sdauth::GetSession();
	const char initial = session.username.empty() ? '?' : static_cast<char>(
		std::toupper(static_cast<unsigned char>(session.username[0])));

	const ImVec2 avatar_center(42.0f, 588.0f);
	draw->AddCircleFilled(avatar_center, 17.0f, color_u32(fade(accent_color(), 0.18f)), 32);
	draw->AddCircle(avatar_center, 17.0f, color_u32(fade(accent_color(), 0.45f)), 32, 1.2f);
	char letter[2] = { initial, '\0' };
	const float letter_w = text_width(font.semibold, 15.0f, letter);
	text(font.semibold, 15.0f, ImVec2(avatar_center.x - letter_w * 0.5f, avatar_center.y - 8.0f),
		lift(accent_color(), 0.2f), letter);

	text(font.semibold, 13.0f, ImVec2(68.0f, 570.0f), kText,
		session.username.empty() ? "Guest" : session.username.c_str());
	text(font.caption, 10.0f, ImVec2(68.0f, 591.0f), kGreen, "Signed in");

	ImGui::SetCursorScreenPos(ImVec2(24.0f, 632.0f));
	if (ImGui::InvisibleButton("##sign_out_sidebar", ImVec2(kSidebarWidth - 48.0f, 36.0f)))
		screen_login::SignOut();
	const bool sign_out_hovered = ImGui::IsItemHovered();
	if (sign_out_hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	if (sign_out_hovered)
		box(rect(16.0f, 632.0f, kSidebarWidth - 16.0f, 668.0f), kCardSoft, 9.0f);

	icon_logout(ImVec2(36.0f, 650.0f), color_u32(sign_out_hovered ? kText : kNavText));
	text(font.body, 13.0f, ImVec2(54.0f, 641.0f), sign_out_hovered ? kText : kNavText, "Sign out");
}

void draw_page_header(const char* title, const char* subtitle)
{
	text(font.title, 25.0f, ImVec2(kContentLeft, 90.0f), kText, title);
	text(font.caption, 12.0f, ImVec2(kContentLeft, 124.0f), kMuted, subtitle);

	pill(rect(kContentRight - 136.0f, 94.0f, kContentRight, 118.0f), "SESSION ACTIVE", kGreen);

	// Server announcement between the title and the session pill.
	const sdauth::Announcement banner = sdauth::GetAnnouncement();
	if (banner.message.empty())
		return;

	const float start = kContentLeft + text_width(font.title, 25.0f, title) + 28.0f;
	const Rect strip = rect(start, 92.0f, kContentRight - 152.0f, 120.0f);
	if (width(strip) < 140.0f)
		return;

	const ImVec4 color = banner.level == "warning" ? kOrange : accent_color();
	box(strip, fade(color, 0.10f), 9.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(strip.min, strip.max, color_u32(fade(color, 0.30f)), 9.0f, 0, 1.0f);
	draw->AddCircleFilled(ImVec2(strip.min.x + 14.0f, strip.min.y + height(strip) * 0.5f), 3.0f, color_u32(color), 12);

	draw->PushClipRect(ImVec2(strip.min.x + 24.0f, strip.min.y), ImVec2(strip.max.x - 10.0f, strip.max.y), true);
	text(font.body, 12.0f, ImVec2(strip.min.x + 26.0f, strip.min.y + 6.0f), lift(color, 0.25f), banner.message.c_str());
	draw->PopClipRect();

	if (ImGui::IsMouseHoveringRect(strip.min, strip.max))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(380.0f);
		ImGui::TextUnformatted(banner.message.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void draw_footer()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(kContentLeft, 672.0f), ImVec2(kContentRight, 672.0f), color_u32(kLine), 1.0f);
	draw->AddCircleFilled(ImVec2(kContentLeft + 4.0f, 690.0f), 3.0f, color_u32(kGreen));

	char left[128];
	std::snprintf(left, sizeof(left), "Sachin Loader v%s  |  Secure  -  Private  -  Fast", app::kVersion);
	text(font.caption, 10.0f, ImVec2(kContentLeft + 14.0f, 684.0f), kDim, left);

	if (!home.show_fps)
		return;

	char right[64];
	std::snprintf(right, sizeof(right), "FPS %d", static_cast<int>(state.fps + 0.5f));
	const float right_width = text_width(font.caption, 10.0f, right);
	text(font.caption, 10.0f, ImVec2(kContentRight - right_width, 684.0f), kDim, right);
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

// Clickable text link drawn at a position. Returns true on click.
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

// ------------------------------------------------------------------- home ----

void draw_hero()
{
	const Rect hero = rect(kContentLeft, 152.0f, kContentRight, 292.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	gradient_box(hero, ImVec4(0.055f, 0.085f, 0.205f, 1.0f), ImVec4(0.135f, 0.070f, 0.260f, 1.0f), 16.0f);
	draw->PushClipRect(hero.min, hero.max, true);
	draw->AddCircleFilled(ImVec2(hero.max.x - 70.0f, hero.min.y + 20.0f), 170.0f,
		color_u32(fade(kPurple, 0.10f)), 64);
	text(font.title, 110.0f, ImVec2(hero.max.x - 168.0f, hero.min.y + 14.0f), fade(kPurple, 0.10f), "S");
	draw->PopClipRect();
	draw->AddRect(hero.min, hero.max, color_u32(kBorder), 16.0f, 0, 1.0f);

	text(font.caption, 10.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 22.0f), accent_color(), "WELCOME BACK");

	const sdauth::Session session = sdauth::GetSession();
	char greeting[96];
	std::snprintf(greeting, sizeof(greeting), "Hi, %s",
		session.username.empty() ? "friend" : session.username.c_str());
	text(font.title, 30.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 38.0f), kText, greeting);

	text(font.body, 14.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 78.0f), kMuted,
		"Your loader is ready. Launch your apps and games from one place.");

	char build[32];
	std::snprintf(build, sizeof(build), "BUILD %s", app::kVersion);
	pill(rect(hero.min.x + 32.0f, hero.min.y + 104.0f, hero.min.x + 112.0f, hero.min.y + 126.0f), build, kGreen);

	const bool update_available = sdauth::StartupUpdateAvailable();
	const bool update_retrying = sdauth::UpdateStatus() == sdauth::UpdateFailed;
	const char* update_label = update_available ? "UPDATE READY"
		: (update_retrying ? "UPDATE RETRY" : "AUTO UPDATE ON");
	pill(rect(hero.min.x + 124.0f, hero.min.y + 104.0f,
		hero.min.x + 124.0f + (update_available || update_retrying ? 118.0f : 140.0f),
		hero.min.y + 126.0f), update_label, update_available ? kBlue : accent_color());

	ImGui::SetCursorScreenPos(ImVec2(hero.max.x - 164.0f, hero.min.y + 96.0f));
	if (secondary_button("Open library", ImVec2(132.0f, 32.0f)))
		state.page = PageLibrary;
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

void draw_stats()
{
	const std::vector<sdauth::Component> components = sdauth::Components();

	char count[16];
	std::snprintf(count, sizeof(count), "%d", static_cast<int>(components.size()));

	char build[32];
	std::snprintf(build, sizeof(build), "v%s", app::kVersion);

	const ULONGLONG seconds = (GetTickCount64() - home.session_start) / 1000ULL;
	char uptime[32];
	if (seconds >= 3600ULL)
		std::snprintf(uptime, sizeof(uptime), "%lluh %02llum", seconds / 3600ULL, (seconds / 60ULL) % 60ULL);
	else
		std::snprintf(uptime, sizeof(uptime), "%llum %02llus", seconds / 60ULL, seconds % 60ULL);

	// Access: days left on the account, or "Lifetime" when the server sets none.
	const int days_left = access_days_left();
	const std::string date = access_date();
	char access_value[32];
	char access_hint[48];
	ImVec4 access_color = kGreen;
	if (days_left < 0)
	{
		std::snprintf(access_value, sizeof(access_value), "Lifetime");
		std::snprintf(access_hint, sizeof(access_hint), "No expiry");
	}
	else
	{
		std::snprintf(access_value, sizeof(access_value), "%d day%s", days_left, days_left == 1 ? "" : "s");
		std::snprintf(access_hint, sizeof(access_hint), "Until %s", date.c_str());
		if (days_left <= 7)
			access_color = kOrange;
	}

	constexpr float gap = 16.0f;
	const float w = (kContentRight - kContentLeft - gap * 3.0f) / 4.0f;
	const float y1 = 308.0f;
	const float y2 = 396.0f;
	float x = kContentLeft;

	stat_card(rect(x, y1, x + w, y2), "COMPONENTS", count, "Released for you", accent_color());
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "BUILD", build, update_text(), update_color());
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "ACCESS", access_value, access_hint, access_color);
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "SESSION", uptime, "Since sign in", kPurple);
}

void draw_quick_launch()
{
	const Rect card = rect(kContentLeft, 412.0f, 800.0f, 652.0f);
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 24.0f, card.min.y + 20.0f), kText, "Quick access");
	text(font.caption, 11.0f, ImVec2(card.min.x + 24.0f, card.min.y + 46.0f), kMuted,
		"Released apps, games and tools for this account.");

	const std::vector<sdauth::Component> components = sdauth::Components();
	if (!components.empty())
	{
		if (text_link("##quick_all", "View all", ImVec2(card.max.x - 24.0f - text_width(font.semibold, 12.0f, "View all"),
			card.min.y + 24.0f)))
			state.page = PageLibrary;
	}
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 72.0f);

	const Rect area = rect(card.min.x + 24.0f, card.min.y + 88.0f, card.max.x - 24.0f, card.max.y - 20.0f);
	if (components.empty())
	{
		draw_empty_state(area, "Nothing here yet", "Content appears when the server releases it.");
		return;
	}

	constexpr float gap = 16.0f;
	const float tile_w = (width(area) - gap) * 0.5f;
	const float tile_h = (height(area) - gap) * 0.5f;
	const int shown = components.size() < 4 ? static_cast<int>(components.size()) : 4;

	for (int i = 0; i < shown; ++i)
	{
		const sdauth::Component& component = components[static_cast<std::size_t>(i)];
		const float x = area.min.x + static_cast<float>(i % 2) * (tile_w + gap);
		const float y = area.min.y + static_cast<float>(i / 2) * (tile_h + gap);
		const Rect tile = rect(x, y, x + tile_w, y + tile_h);

		ImGui::PushID(i);
		ImGui::SetCursorScreenPos(tile.min);
		const bool clicked = ImGui::InvisibleButton("##quick", ImVec2(width(tile), height(tile)));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		box(tile, hovered ? lift(kCardSoft, 0.02f) : kCardSoft, 10.0f, hovered ? &kLineBright : &kLine);
		const float badge = height(tile) - 20.0f;
		letter_badge(rect(tile.min.x + 10.0f, tile.min.y + 10.0f, tile.min.x + 10.0f + badge, tile.max.y - 10.0f),
			component.name, 16.0f);

		ImDrawList* draw = ImGui::GetWindowDrawList();
		draw->PushClipRect(tile.min, ImVec2(tile.max.x - 12.0f, tile.max.y), true);
		text(font.semibold, 14.0f, ImVec2(tile.min.x + badge + 22.0f, tile.min.y + 11.0f), kText,
			component.name.c_str());
		char version[48];
		std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
		text(font.caption, 11.0f, ImVec2(tile.min.x + badge + 22.0f, tile.min.y + 31.0f), kMuted, version);
		draw->PopClipRect();
		draw->AddCircleFilled(ImVec2(tile.max.x - 16.0f, tile.min.y + 19.0f), 3.5f,
			color_u32(status_color(component.status)), 12);

		if (clicked)
			open_component(i);
	}
}

void draw_whats_new()
{
	const Rect card = rect(816.0f, 412.0f, kContentRight, 652.0f);
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 24.0f, card.min.y + 20.0f), kText, "What's new");
	if (text_link("##news_all", "All news", ImVec2(card.max.x - 24.0f - text_width(font.semibold, 12.0f, "All news"),
		card.min.y + 24.0f)))
		state.page = PageNews;
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 56.0f);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	float y = card.min.y + 70.0f;
	const std::vector<sdauth::NewsItem> news = news_items();
	const int count = static_cast<int>(news.size());
	const int shown = count < 3 ? count : 3;
	for (int i = 0; i < shown; ++i)
	{
		const sdauth::NewsItem& item = news[static_cast<std::size_t>(i)];
		const ImVec4 color = tag_color(item.tag);
		draw->AddCircleFilled(ImVec2(card.min.x + 28.0f, y + 8.0f), 3.5f, color_u32(color), 12);
		if (i + 1 < shown)
			draw->AddLine(ImVec2(card.min.x + 28.0f, y + 16.0f), ImVec2(card.min.x + 28.0f, y + 50.0f),
				color_u32(kLine), 1.0f);

		draw->PushClipRect(ImVec2(card.min.x + 42.0f, y - 2.0f), ImVec2(card.max.x - 20.0f, y + 40.0f), true);
		text(font.semibold, 13.0f, ImVec2(card.min.x + 42.0f, y), kText, item.title.c_str());
		char meta[96];
		if (item.version.empty())
			std::snprintf(meta, sizeof(meta), "%s", item.date.c_str());
		else
			std::snprintf(meta, sizeof(meta), "v%s  -  %s", item.version.c_str(), item.date.c_str());
		text(font.caption, 10.0f, ImVec2(card.min.x + 42.0f, y + 20.0f), kMuted, meta);
		draw->PopClipRect();
		y += 54.0f;
	}
}

void draw_home_page()
{
	draw_page_header("Home", "Your loader at a glance.");
	draw_hero();
	draw_stats();
	draw_quick_launch();
	draw_whats_new();
}

// ---------------------------------------------------------------- library ----

void draw_search_box(const Rect& r)
{
	const bool has_text = home.search[0] != '\0';
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
	ImGui::InputTextWithHint("##library_search", "Search components", home.search, sizeof(home.search));

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
	ImGui::PopFont();

	if (!has_text)
		return;

	const Rect clear = rect(r.max.x - 28.0f, r.min.y + 6.0f, r.max.x - 6.0f, r.max.y - 6.0f);
	ImGui::SetCursorScreenPos(clear.min);
	if (ImGui::InvisibleButton("##search_clear", ImVec2(width(clear), height(clear))))
		home.search[0] = '\0';
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	const ImVec2 c = center(clear);
	const ImU32 glyph = color_u32(hovered ? kText : kMuted);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), glyph, 1.5f);
	draw->AddLine(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x - 4.0f, c.y + 4.0f), glyph, 1.5f);
}

void draw_component_details(const Rect& panel, const sdauth::Component& component)
{
	box(panel, kCard, 14.0f, &kBorder);

	const Rect banner = rect(panel.min.x + 1.0f, panel.min.y + 1.0f, panel.max.x - 1.0f, panel.min.y + 96.0f);
	gradient_box(banner, fade(accent_color(), 0.30f), fade(kPurple, 0.30f), 13.0f);

	letter_badge(rect(panel.min.x + 24.0f, panel.min.y + 64.0f, panel.min.x + 88.0f, panel.min.y + 128.0f),
		component.name, 28.0f);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.min.y), ImVec2(panel.max.x - 24.0f, panel.max.y), true);
	text(font.semibold, 20.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 142.0f), kText, component.name.c_str());
	draw->PopClipRect();

	char version[48];
	std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
	const float pill_w = text_width(font.caption, 10.0f, version) + 32.0f;
	pill(rect(panel.min.x + 24.0f, panel.min.y + 174.0f, panel.min.x + 24.0f + pill_w, panel.min.y + 196.0f),
		version, kGreen);
	const std::string status = status_label(component.status);
	const float status_w = text_width(font.caption, 10.0f, status.c_str()) + 32.0f;
	pill(rect(panel.min.x + 32.0f + pill_w, panel.min.y + 174.0f, panel.min.x + 32.0f + pill_w + status_w,
		panel.min.y + 196.0f), status.c_str(), status_color(component.status));

	divider(panel.min.x + 24.0f, panel.max.x - 24.0f, panel.min.y + 214.0f);

	text(font.caption, 10.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 230.0f), kDim, "ABOUT");
	text_wrapped(font.body, 13.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 248.0f), kMuted,
		component.description.empty() ? "No description provided by the server." : component.description.c_str(),
		width(panel) - 48.0f);

	// Launch area: result line, download bar and the button.
	const int launch_status = sdauth::LaunchStatus();
	const bool launch_here = launch_status != sdauth::StatusIdle && sdauth::LaunchName() == component.name;
	const bool launch_busy = launch_status == sdauth::StatusWorking;

	if (launch_here)
	{
		const ImVec4 color = launch_status == sdauth::StatusError ? kRed
			: (launch_status == sdauth::StatusOk ? kGreen : kMuted);
		const std::string message = sdauth::LaunchMessage();
		draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.max.y - 104.0f),
			ImVec2(panel.max.x - 24.0f, panel.max.y - 84.0f), true);
		text(font.caption, 11.0f, ImVec2(panel.min.x + 24.0f, panel.max.y - 102.0f), color, message.c_str());
		draw->PopClipRect();

		if (launch_busy)
		{
			const Rect bar = rect(panel.min.x + 24.0f, panel.max.y - 80.0f, panel.max.x - 24.0f, panel.max.y - 74.0f);
			box(bar, kCardSoft, 3.0f);
			const float done = clampf(static_cast<float>(sdauth::LaunchProgress()) / 100.0f, 0.02f, 1.0f);
			box(rect(bar.min.x, bar.min.y, bar.min.x + width(bar) * done, bar.max.y), accent_color(), 3.0f);
		}
	}

	const char* button_label = "Launch";
	if (component.download_url.empty())
		button_label = "Not launchable";
	else if (component.status == "updating")
		button_label = "Updating";
	else if (component.status == "offline")
		button_label = "Offline";
	else if (launch_here && launch_busy)
		button_label = "Working...";

	const bool can_launch = component_launchable(component) && !launch_busy;
	ImGui::SetCursorScreenPos(ImVec2(panel.min.x + 24.0f, panel.max.y - 62.0f));
	const ImVec2 button_size(width(panel) - 48.0f, 40.0f);
	if (can_launch)
	{
		if (ImGui::Button("Launch##component", button_size))
			sdauth::LaunchComponent(component.name);
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else
	{
		ImGui::BeginDisabled();
		secondary_button(button_label, button_size);
		ImGui::EndDisabled();
	}
}

void draw_library_page()
{
	draw_page_header("Library", "Content released by the server for this account.");

	const std::vector<sdauth::Component> components = sdauth::Components();
	if (components.empty())
	{
		const Rect card = rect(kContentLeft, 152.0f, kContentRight, 652.0f);
		box(card, kCard, 14.0f, &kBorder);
		draw_empty_state(rect(card.min.x + 32.0f, card.min.y + 32.0f, card.max.x - 32.0f, card.max.y - 32.0f),
			"No components yet", "Content released by the server will appear here.");
		return;
	}

	if (home.selected < 0 || home.selected >= static_cast<int>(components.size()))
		home.selected = 0;

	const Rect list = rect(kContentLeft, 152.0f, 812.0f, 652.0f);
	box(list, kCard, 14.0f, &kBorder);

	std::vector<int> visible;
	for (int i = 0; i < static_cast<int>(components.size()); ++i)
	{
		const sdauth::Component& c = components[static_cast<std::size_t>(i)];
		if (contains_ci(c.name, home.search) || contains_ci(c.description, home.search))
			visible.push_back(i);
	}

	text(font.semibold, 17.0f, ImVec2(list.min.x + 24.0f, list.min.y + 20.0f), kText, "Components");
	char count[64];
	if (home.search[0] != '\0')
		std::snprintf(count, sizeof(count), "%d of %d shown", static_cast<int>(visible.size()),
			static_cast<int>(components.size()));
	else
		std::snprintf(count, sizeof(count), "%d component%s", static_cast<int>(components.size()),
			components.size() == 1 ? "" : "s");
	text(font.caption, 11.0f, ImVec2(list.min.x + 24.0f, list.min.y + 46.0f), kMuted, count);

	draw_search_box(rect(list.max.x - 264.0f, list.min.y + 20.0f, list.max.x - 24.0f, list.min.y + 56.0f));
	divider(list.min.x + 24.0f, list.max.x - 24.0f, list.min.y + 72.0f);

	const Rect area = rect(list.min.x + 24.0f, list.min.y + 88.0f, list.max.x - 16.0f, list.max.y - 16.0f);
	if (visible.empty())
	{
		draw_empty_state(rect(area.min.x, area.min.y, area.max.x - 8.0f, area.max.y - 8.0f),
			"No matches", "Try a different search.");
	}
	else
	{
		constexpr float gap = 12.0f;
		constexpr float tile_h = 84.0f;
		const float tile_w = (width(area) - 8.0f - gap) * 0.5f;

		ImGui::SetCursorScreenPos(area.min);
		ImGui::BeginChild("##library_grid", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImDrawList* draw = ImGui::GetWindowDrawList();

		for (int n = 0; n < static_cast<int>(visible.size()); ++n)
		{
			const int index = visible[static_cast<std::size_t>(n)];
			const sdauth::Component& component = components[static_cast<std::size_t>(index)];
			const float x = origin.x + static_cast<float>(n % 2) * (tile_w + gap);
			const float y = origin.y + static_cast<float>(n / 2) * (tile_h + gap);
			const Rect tile = rect(x, y, x + tile_w, y + tile_h);

			ImGui::PushID(index);
			ImGui::SetCursorScreenPos(tile.min);
			if (ImGui::InvisibleButton("##tile", ImVec2(width(tile), height(tile))))
				home.selected = index;
			const bool hovered = ImGui::IsItemHovered();
			ImGui::PopID();
			if (hovered)
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			const bool active = index == home.selected;
			const ImVec4 border = active ? fade(accent_color(), 0.55f) : (hovered ? kLineBright : kLine);
			box(tile, active ? fade(accent_color(), 0.10f) : (hovered ? lift(kCardSoft, 0.02f) : kCardSoft),
				10.0f, &border);

			letter_badge(rect(tile.min.x + 14.0f, tile.min.y + 14.0f, tile.min.x + 56.0f, tile.min.y + 56.0f),
				component.name, 18.0f);

			draw->PushClipRect(tile.min, ImVec2(tile.max.x - 14.0f, tile.max.y), true);
			text(font.semibold, 14.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 14.0f), kText, component.name.c_str());
			char version[48];
			std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
			text(font.caption, 10.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 35.0f), accent_color(), version);
			text(font.caption, 11.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 54.0f), kMuted,
				component.description.empty() ? "No description" : component.description.c_str());
			draw->PopClipRect();

			const ImVec4 dot = status_color(component.status);
			draw->AddCircleFilled(ImVec2(tile.max.x - 18.0f, tile.min.y + 21.0f), 3.5f, color_u32(dot), 12);
			if (hovered && ImGui::IsMouseHoveringRect(ImVec2(tile.max.x - 30.0f, tile.min.y + 10.0f),
				ImVec2(tile.max.x - 6.0f, tile.min.y + 32.0f)))
				ImGui::SetTooltip("%s", status_label(component.status).c_str());
		}

		const int rows = (static_cast<int>(visible.size()) + 1) / 2;
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy(ImVec2(1.0f, static_cast<float>(rows) * (tile_h + gap) - gap));
		ImGui::EndChild();
	}

	draw_component_details(rect(828.0f, 152.0f, kContentRight, 652.0f),
		components[static_cast<std::size_t>(home.selected)]);
}

// ------------------------------------------------------------------- news ----

void draw_news_page()
{
	draw_page_header("News", "Release notes and announcements.");

	const Rect card = rect(kContentLeft, 152.0f, kContentRight, 652.0f);
	box(card, kCard, 14.0f, &kBorder);

	const Rect area = rect(card.min.x + 24.0f, card.min.y + 20.0f, card.max.x - 12.0f, card.max.y - 16.0f);
	ImGui::SetCursorScreenPos(area.min);
	ImGui::BeginChild("##news_list", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	const float body_w = width(area) - 72.0f - 24.0f;
	const std::vector<sdauth::NewsItem> news = news_items();
	const int news_count = static_cast<int>(news.size());
	float y = origin.y + 4.0f;
	for (int i = 0; i < news_count; ++i)
	{
		const sdauth::NewsItem& item = news[static_cast<std::size_t>(i)];
		const ImVec4 color = tag_color(item.tag);
		const float x = origin.x;

		const float body_h = font.body->CalcTextSizeA(13.0f, FLT_MAX, body_w, item.body.c_str()).y;
		const float entry_h = 70.0f + body_h;

		// Timeline rail.
		draw->AddCircleFilled(ImVec2(x + 10.0f, y + 12.0f), 6.0f, color_u32(fade(color, 0.20f)), 16);
		draw->AddCircleFilled(ImVec2(x + 10.0f, y + 12.0f), 3.5f, color_u32(color), 12);
		if (i + 1 < news_count)
			draw->AddLine(ImVec2(x + 10.0f, y + 22.0f), ImVec2(x + 10.0f, y + entry_h + 2.0f), color_u32(kLine), 1.0f);

		const float tag_w = text_width(font.caption, 10.0f, item.tag.c_str()) + 32.0f;
		pill(rect(x + 36.0f, y, x + 36.0f + tag_w, y + 22.0f), item.tag.c_str(), color);

		char meta[96];
		if (item.version.empty())
			std::snprintf(meta, sizeof(meta), "%s", item.date.c_str());
		else
			std::snprintf(meta, sizeof(meta), "v%s  -  %s", item.version.c_str(), item.date.c_str());
		text(font.caption, 11.0f, ImVec2(x + 48.0f + tag_w, y + 4.0f), kMuted, meta);

		if (i == 0)
			pill(rect(x + width(area) - 120.0f, y, x + width(area) - 24.0f, y + 22.0f), "LATEST", kGreen);

		text(font.semibold, 16.0f, ImVec2(x + 36.0f, y + 32.0f), kText, item.title.c_str());
		text_wrapped(font.body, 13.0f, ImVec2(x + 36.0f, y + 56.0f), kMuted, item.body.c_str(), body_w);

		y += entry_h + 18.0f;
	}

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, y - origin.y));
	ImGui::EndChild();
}

// --------------------------------------------------------------- settings ----

void draw_appearance_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Appearance");
	text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 48.0f), kMuted,
		"Pick an accent color. Changes save automatically.");

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 80.0f), kDim, "ACCENT");

	ImDrawList* draw = ImGui::GetWindowDrawList();
	bool changed = false;
	float x = card.min.x + 44.0f;
	const float y = card.min.y + 116.0f;
	for (const AccentPreset& preset : kAccentPresets)
	{
		const ImVec4 color(preset.r, preset.g, preset.b, 1.0f);
		const bool active = std::fabs(state.accent[0] - preset.r) < 0.01f
			&& std::fabs(state.accent[1] - preset.g) < 0.01f
			&& std::fabs(state.accent[2] - preset.b) < 0.01f;

		ImGui::SetCursorScreenPos(ImVec2(x - 16.0f, y - 16.0f));
		ImGui::PushID(preset.name);
		const bool clicked = ImGui::InvisibleButton("##swatch", ImVec2(32.0f, 32.0f));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::SetTooltip("%s", preset.name);
		}

		if (active || hovered)
			draw->AddCircle(ImVec2(x, y), 16.0f, color_u32(active ? color : kLineBright), 32, 2.0f);
		draw->AddCircleFilled(ImVec2(x, y), 11.5f, color_u32(color), 32);

		if (clicked)
		{
			state.accent[0] = preset.r;
			state.accent[1] = preset.g;
			state.accent[2] = preset.b;
			changed = true;
		}
		x += 44.0f;
	}

	// Custom color picker after the presets.
	ImGui::SetCursorScreenPos(ImVec2(x - 14.0f, y - 14.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 7.0f));
	if (ImGui::ColorEdit3("##accent_custom", state.accent,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel))
		changed = true;
	ImGui::PopStyleVar(2);
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	text(font.caption, 10.0f, ImVec2(x + 22.0f, y - 6.0f), kDim, "CUSTOM");

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 152.0f);

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 168.0f), kDim, "INTERFACE");
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 186.0f));
	if (ImGui::Checkbox("Show FPS counter in footer", &home.show_fps))
		changed = true;
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 220.0f));
	if (ImGui::Checkbox("Background glow", &home.background_glow))
		changed = true;

	if (changed)
		save_preferences();
}

void draw_account_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Account");

	const sdauth::Session session = sdauth::GetSession();
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 60.0f), kDim, "SIGNED IN AS");
	text(font.body, 15.0f, ImVec2(card.min.x + 28.0f, card.min.y + 78.0f), kText,
		session.username.empty() ? "Guest" : session.username.c_str());

	// Access: the server sets an expiry per account, none means lifetime.
	const int days_left = access_days_left();
	const std::string date = access_date();
	char access[64];
	if (days_left < 0)
		std::snprintf(access, sizeof(access), "Lifetime");
	else
		std::snprintf(access, sizeof(access), "%d day%s  -  %s", days_left, days_left == 1 ? "" : "s", date.c_str());
	text(font.caption, 10.0f, ImVec2(card.min.x + 210.0f, card.min.y + 60.0f), kDim, "ACCESS");
	text(font.body, 15.0f, ImVec2(card.min.x + 210.0f, card.min.y + 78.0f),
		days_left >= 0 && days_left <= 7 ? kOrange : kGreen, access);

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 112.0f), kDim, "DEVICE ID");
	const std::string& hwid = sdauth::MachineHwid();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(card.min, ImVec2(card.max.x - 120.0f, card.max.y), true);
	text(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 130.0f), kText,
		hwid.empty() ? "Unavailable" : hwid.c_str());
	draw->PopClipRect();

	if (!hwid.empty())
	{
		const bool copied = ImGui::GetTime() - home.copied_time < 2.0;
		ImGui::SetCursorScreenPos(ImVec2(card.max.x - 108.0f, card.min.y + 116.0f));
		if (secondary_button(copied ? "Copied" : "Copy", ImVec2(80.0f, 30.0f)))
		{
			ImGui::SetClipboardText(hwid.c_str());
			home.copied_time = ImGui::GetTime();
		}
	}

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.max.y - 32.0f), kDim,
		"Ask an admin to reset the device binding if you change PC.");

	ImGui::SetCursorScreenPos(ImVec2(card.max.x - 188.0f, card.min.y + 20.0f));
	if (secondary_button("Sign out", ImVec2(160.0f, 32.0f)))
		screen_login::SignOut();
}

void draw_loader_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Loader");

	struct Row { const char* label; const char* value; ImVec4 color; };
	const std::string latest = sdauth::StartupLatestVersion();
	const std::string components = std::to_string(sdauth::Components().size());
	const Row rows[] = {
		{ "VERSION", app::kVersion, kText },
		{ "LATEST", latest.empty() ? app::kVersion : latest.c_str(), kText },
		{ "UPDATE CHANNEL", "Automatic", kGreen },
		{ "UPDATE STATE", update_text(), update_color() },
		{ "COMPONENTS", components.c_str(), kText },
	};

	float y = card.min.y + 62.0f;
	for (const Row& row : rows)
	{
		text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, y), kDim, row.label);
		text(font.body, 14.0f, ImVec2(card.min.x + 180.0f, y - 2.0f), row.color, row.value);
		y += 32.0f;
		divider(card.min.x + 28.0f, card.max.x - 28.0f, y - 10.0f);
	}

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.max.y - 30.0f), kDim,
		"Updates download, verify and restart by themselves.");
}

void draw_help_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Help");
	text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 48.0f), kMuted,
		"Contact support, copy a report or open the loader data folder.");

	const float button_w = width(card) - 56.0f;
	const std::string support_url = sdauth::SupportUrl();

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 78.0f));
	if (support_url.empty())
	{
		ImGui::BeginDisabled();
		secondary_button("Support link not set", ImVec2(button_w, 34.0f));
		ImGui::EndDisabled();
	}
	else if (secondary_button("Open support link", ImVec2(button_w, 34.0f)))
	{
		open_url(support_url);
	}

	const bool copied = ImGui::GetTime() - home.diagnostics_time < 2.0;
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 120.0f));
	if (secondary_button(copied ? "Copied to clipboard" : "Copy diagnostics", ImVec2(button_w, 34.0f)))
	{
		ImGui::SetClipboardText(diagnostics_text().c_str());
		home.diagnostics_time = ImGui::GetTime();
	}

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 162.0f));
	if (secondary_button("Open data folder", ImVec2(button_w, 34.0f)))
		open_data_folder();

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.max.y - 22.0f), kDim,
		"Passwords and tokens never touch disk.");
}

void draw_settings_page()
{
	draw_page_header("Settings", "Appearance, account and loader information.");

	draw_appearance_card(rect(kContentLeft, 152.0f, 690.0f, 412.0f));
	draw_account_card(rect(kContentLeft, 428.0f, 690.0f, 652.0f));
	draw_loader_card(rect(706.0f, 152.0f, kContentRight, 412.0f));
	draw_help_card(rect(706.0f, 428.0f, kContentRight, 652.0f));
}

} // namespace

void LoadPreferences()
{
	const std::wstring path = settings_path(false);
	if (path.empty())
		return;

	wchar_t accent[48] = {};
	GetPrivateProfileStringW(L"ui", L"accent", L"", accent, 48, path.c_str());
	int r = 0, g = 0, b = 0;
	if (std::swscanf(accent, L"%d,%d,%d", &r, &g, &b) == 3)
	{
		state.accent[0] = clampf(static_cast<float>(r) / 255.0f, 0.0f, 1.0f);
		state.accent[1] = clampf(static_cast<float>(g) / 255.0f, 0.0f, 1.0f);
		state.accent[2] = clampf(static_cast<float>(b) / 255.0f, 0.0f, 1.0f);
	}

	home.show_fps = GetPrivateProfileIntW(L"ui", L"show_fps", 1, path.c_str()) != 0;
	home.background_glow = GetPrivateProfileIntW(L"ui", L"background_glow", 1, path.c_str()) != 0;
}

void Render()
{
	// The home screen is drawn only while signed in, so a break in the frame
	// sequence means a new session: reset the timer, search and page.
	const int frame = ImGui::GetFrameCount();
	if (frame != home.last_frame + 1)
	{
		home.session_start = GetTickCount64();
		home.search[0] = '\0';
		home.selected = 0;
		state.page = PageHome;
	}
	home.last_frame = frame;

	// Clear a finished launch message a few seconds after it appears.
	const int launch_status = sdauth::LaunchStatus();
	if (launch_status != home.launch_seen)
	{
		home.launch_seen = launch_status;
		home.launch_done_time = ImGui::GetTime();
	}
	if ((launch_status == sdauth::StatusOk || launch_status == sdauth::StatusError)
		&& ImGui::GetTime() - home.launch_done_time > 8.0)
		sdauth::ResetLaunch();

	draw_background();
	draw_title_bar();
	draw_sidebar();

	switch (state.page)
	{
	case PageLibrary: draw_library_page(); break;
	case PageNews: draw_news_page(); break;
	case PageSettings: draw_settings_page(); break;
	default: draw_home_page(); break;
	}

	draw_footer();
}

} // namespace screen_home
