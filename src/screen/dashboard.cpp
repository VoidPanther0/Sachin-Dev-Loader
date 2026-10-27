#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cctype>
#include <cstdio>
#include <string>

// Window shell shown after sign-in: title bar, sidebar, footer and the current
// page. Every page lives in its own file; see dashboard_common.h.

namespace screen_dashboard {

namespace {

using namespace ui;
using namespace dash;

struct NavEntry
{
	const char* label;
	Page page;
	void (*icon)(ImVec2, ImU32);
};

const NavEntry kNav[] = {
	{ "Home",         PageHome,         icon_home },
	{ "Applications", PageApplications, icon_apps },
	{ "Downloads",    PageDownloads,    icon_download },
	{ "DLL Tools",    PageDllTools,     icon_dll },
	{ "Theme",        PageTheme,        icon_theme },
	{ "Account",      PageAccount,      icon_account },
	{ "Settings",     PageSettings,     icon_settings },
	{ "About",        PageAbout,        icon_about },
};

constexpr float kNavTop = 108.0f;
constexpr float kNavStep = 48.0f;

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, app::kHeight), color_u32(kHomeBg));
	if (!shared.background_glow)
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

	login_ui::DrawLogo(ImVec2(34.0f, 32.0f), 34.0f);

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

bool nav_item(const NavEntry& entry, float y)
{
	const Rect r = rect(16.0f, y, kSidebarWidth - 16.0f, y + 44.0f);
	ImGui::PushID(entry.label);
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton("##nav", ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();
	const bool active = state.page == entry.page;
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

	entry.icon(ImVec2(r.min.x + 28.0f, r.min.y + 22.0f),
		color_u32(active ? lift(accent_color(), 0.15f) : kNavText));

	text(font.semibold, 14.0f, ImVec2(r.min.x + 48.0f, r.min.y + 13.0f),
		active ? kText : kNavText, entry.label);
	return clicked;
}

// Small badge at the right edge of a nav item: a count or an accent dot.
void nav_badge(float y, const char* value)
{
	const float w = text_width(font.caption, 10.0f, value) + 14.0f;
	const Rect b = rect(kSidebarWidth - 30.0f - w, y + 12.0f, kSidebarWidth - 30.0f, y + 32.0f);
	box(b, fade(accent_color(), 0.18f), 10.0f);
	text_center(font.caption, 10.0f, b, lift(accent_color(), 0.2f), value);
}

void draw_sidebar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, kTitleHeight), ImVec2(kSidebarWidth, app::kHeight), color_u32(kSidebar));
	draw->AddLine(ImVec2(kSidebarWidth - 1.0f, kTitleHeight), ImVec2(kSidebarWidth - 1.0f, app::kHeight),
		color_u32(kLine), 1.0f);

	text(font.caption, 10.0f, ImVec2(28.0f, 86.0f), kDim, "MENU");

	float y = kNavTop;
	for (const NavEntry& entry : kNav)
	{
		if (nav_item(entry, y))
			state.page = entry.page;

		if (entry.page == PageApplications)
		{
			const int count = static_cast<int>(sdauth::Components().size());
			if (count > 0)
			{
				char badge[16];
				std::snprintf(badge, sizeof(badge), "%d", count);
				nav_badge(y, badge);
			}
		}
		else if (entry.page == PageDownloads && update_available())
		{
			nav_badge(y, "NEW");
		}
		y += kNavStep;
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
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		box(rect(16.0f, 632.0f, kSidebarWidth - 16.0f, 668.0f), kCardSoft, 9.0f);
	}

	icon_logout(ImVec2(36.0f, 650.0f), color_u32(sign_out_hovered ? kText : kNavText));
	text(font.body, 13.0f, ImVec2(54.0f, 641.0f), sign_out_hovered ? kText : kNavText, "Sign out");
}

void draw_footer()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(kContentLeft, 672.0f), ImVec2(kContentRight, 672.0f), color_u32(kLine), 1.0f);
	draw->AddCircleFilled(ImVec2(kContentLeft + 4.0f, 690.0f), 3.0f, color_u32(kGreen));

	char left[128];
	std::snprintf(left, sizeof(left), "Sachin Loader v%s  |  Secure  -  Private  -  Fast", app::kVersion);
	text(font.caption, 10.0f, ImVec2(kContentLeft + 14.0f, 684.0f), kDim, left);

	if (!shared.show_fps)
		return;

	char right[64];
	std::snprintf(right, sizeof(right), "FPS %d", static_cast<int>(state.fps + 0.5f));
	const float right_width = text_width(font.caption, 10.0f, right);
	text(font.caption, 10.0f, ImVec2(kContentRight - right_width, 684.0f), kDim, right);
}

} // namespace

void LoadPreferences()
{
	load_preferences();
}

void Render()
{
	// The dashboard is drawn only while signed in, so a break in the frame
	// sequence means a new session: reset the timer, search and page.
	static int last_frame = -2;
	const int frame = ImGui::GetFrameCount();
	if (frame != last_frame + 1)
	{
		shared.session_start = GetTickCount64();
		shared.search[0] = '\0';
		shared.selected = 0;
		shared.restart_ready_time = -1.0;
		state.page = PageHome;
	}
	last_frame = frame;

	if (state.page < 0 || state.page >= PageCount)
		state.page = PageHome;

	tick_launch();

	draw_background();
	draw_title_bar();
	draw_sidebar();

	switch (state.page)
	{
	case PageApplications: DrawApplications(); break;
	case PageDownloads: DrawDownloads(); break;
	case PageDllTools: DrawDllTools(); break;
	case PageTheme: DrawTheme(); break;
	case PageAccount: DrawAccount(); break;
	case PageSettings: DrawSettings(); break;
	case PageAbout: DrawAbout(); break;
	default: DrawHome(); break;
	}

	draw_footer();
}

} // namespace screen_dashboard
