#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cctype>
#include <cstdio>
#include <string>

// Window shell shown after sign-in: sidebar with the logo and navigation, the
// title bar with the page title, session pill and window buttons, and the
// current page. Every page lives in its own file; see dashboard_common.h.

namespace screen_dashboard {

namespace {

using namespace ui;
using namespace dash;

struct NavEntry
{
	const char* label;
	const char* subtitle;
	Page page;
	IconFn icon;
};

const NavEntry kNav[] = {
	{ "Home",          "Your loader at a glance.",                          PageHome,         icon_home },
	{ "Product",       "Product details and features.",                     PageProduct,      icon_product },
	{ "Game Updates",  "Game updates and patches.",                         PageGameUpdates,  icon_updates },
	{ "Socials",       "Connect with us on social media.",                  PageSocials,      icon_socials },
	{ "Settings",      "Interface, performance and launch options.",        PageSettings,     icon_settings },
};

constexpr int kNavCount = static_cast<int>(sizeof(kNav) / sizeof(kNav[0]));

constexpr float kNavTop = 126.0f;
constexpr float kNavStep = 46.0f;
constexpr float kNavItemHeight = 40.0f;

// Hover highlight per page; eased so the sidebar never snaps.
float g_nav_hover[kNavCount] = {};

void draw_background()
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), display, color_u32(kHomeBg));

	if (!shared.background_glow || shared.reduced_effects)
		return;

	draw->AddCircleFilled(ImVec2(display.x - 40.0f, 20.0f), 320.0f,
		color_u32(fade(kPurple, state.light ? 0.04f : 0.05f)), 64);
	draw->AddCircleFilled(ImVec2(180.0f, display.y + 60.0f), 260.0f,
		color_u32(fade(kBlue, state.light ? 0.03f : 0.04f)), 64);
}

// The title bar covers the content area only; the sidebar owns the top-left
// corner with the logo.
void draw_title_bar()
{
	const NavEntry& entry = (state.page >= 0 && state.page < kNavCount) ? kNav[state.page] : kNav[0];

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(kSidebarWidth, 0.0f), ImVec2(app::kWidth, kTitleHeight), color_u32(kHomeBg));
	draw->AddLine(ImVec2(kSidebarWidth, kTitleHeight - 1.0f), ImVec2(app::kWidth, kTitleHeight - 1.0f),
		color_u32(kLine), 1.0f);

	// Page title with the accent bar from the reference design.
	const Rect accent_bar = rect(256.0f, 20.0f, 260.0f, 44.0f);
	box(accent_bar, accent_color(), 2.0f);
	text(font.semibold, 19.0f, ImVec2(272.0f, 12.0f), kText, entry.label);

	draw->PushClipRect(ImVec2(272.0f, 36.0f), ImVec2(900.0f, 54.0f), true);
	text(font.caption, 11.0f, ImVec2(272.0f, 38.0f), kMuted, entry.subtitle);
	draw->PopClipRect();

	// Drag strip: everything left of the window buttons.
	ImGui::SetCursorScreenPos(ImVec2(kSidebarWidth, 0.0f));
	ImGui::InvisibleButton("##title_drag", ImVec2(820.0f, kTitleHeight));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	// Session state is reported from the real session, never assumed.
	const bool live = sdauth::HasSession();
	pill(rect(924.0f, 20.0f, 1044.0f, 44.0f), live ? "SESSION ACTIVE" : "SESSION ENDED", live ? kGreen : kRed);

	if (window_button("##minimize", rect(1052.0f, 16.0f, 1084.0f, 48.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (window_button("##close", rect(1116.0f, 16.0f, 1148.0f, 48.0f), true))
		PostQuitMessage(0);
}

bool nav_item(const NavEntry& entry, float y, float hover)
{
	const Rect r = rect(16.0f, y, kSidebarWidth - 16.0f, y + kNavItemHeight);
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
		// Blue to purple gradient, as in the reference design.
		gradient_box(r, accent_color(), kPurple, 10.0f);
		if (!shared.reduced_effects)
			draw->AddRect(r.min, r.max, color_u32(fade(kWhite, 0.18f)), 10.0f, 0, 1.0f);
	}
	else if (hover > 0.01f)
	{
		box(r, fade(kCardSoft, hover), 10.0f);
	}

	const ImU32 icon_color = color_u32(active ? kWhite : fade(kNavText, 0.55f + 0.45f * hover));
	entry.icon(ImVec2(r.min.x + 28.0f, r.min.y + kNavItemHeight * 0.5f), icon_color);

	const ImVec4 label_color = active ? kWhite : fade(kNavText, 0.55f + 0.45f * hover);
	text(font.semibold, 14.0f, ImVec2(r.min.x + 48.0f, r.min.y + 12.0f), label_color, entry.label);
	return clicked;
}

// Small badge at the right edge of a nav item: a count.
void nav_badge(float y, const char* value)
{
	const float w = text_width(font.caption, 10.0f, value) + 14.0f;
	const Rect b = rect(kSidebarWidth - 30.0f - w, y + 10.0f, kSidebarWidth - 30.0f, y + 30.0f);
	box(b, fade(accent_color(), 0.18f), 10.0f);
	const ImVec4 label_color = state.light ? dim(accent_color(), 0.25f) : lift(accent_color(), 0.20f);
	text_center(font.caption, 10.0f, b, label_color, value);
}

void draw_sidebar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(kSidebarWidth, app::kHeight), color_u32(kSidebar));
	draw->AddLine(ImVec2(kSidebarWidth - 1.0f, 0.0f), ImVec2(kSidebarWidth - 1.0f, app::kHeight),
		color_u32(kLine), 1.0f);

	// ---------------------------------------------------------------- logo --
	login_ui::DrawLogo(ImVec2(kSidebarWidth * 0.5f, 46.0f), 44.0f);

	const char* wordmark = "SACHIN LOADER";
	const float wordmark_w = text_width(font.semibold, 15.0f, wordmark);
	gradient_text(font.semibold, 15.0f, ImVec2((kSidebarWidth - wordmark_w) * 0.5f, 78.0f), wordmark,
		lift(kBlue, 0.15f), kPurple);

	const char* dots = "PLAY  \u2022  LAUNCH  \u2022  ENJOY";
	const float tagline_w = text_width(font.caption, 9.0f, dots);
	text(font.caption, 9.0f, ImVec2((kSidebarWidth - tagline_w) * 0.5f, 100.0f), kDim, dots);

	divider(24.0f, kSidebarWidth - 24.0f, 116.0f);

	// ------------------------------------------------------------ nav items --
	float y = kNavTop;
	for (int i = 0; i < kNavCount; ++i)
	{
		const NavEntry& entry = kNav[i];
		const bool active = state.page == entry.page;

		// The highlight eases toward the target; nav_item() draws the value
		// from the previous frame and the new one is computed right after the
		// button was submitted, so hover can be read back safely here.
		float& anim = g_nav_hover[i];
		if (nav_item(entry, y, anim))
			state.page = entry.page;

		const float wanted = active ? 1.0f : (ImGui::IsItemHovered() ? 1.0f : 0.0f);
		if (!shared.animations || shared.reduced_effects)
			anim = wanted;
		else
			anim += (wanted - anim) * clampf(ImGui::GetIO().DeltaTime * 14.0f, 0.0f, 1.0f);

		y += kNavStep;
	}
}

void draw_footer()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(kContentLeft, 672.0f), ImVec2(kContentRight, 672.0f), color_u32(kLine), 1.0f);

	const ImVec4 state_color = server_state_color();
	draw->AddCircleFilled(ImVec2(kContentLeft + 4.0f, 690.0f), 3.0f, color_u32(state_color));

	char left[128];
	std::snprintf(left, sizeof(left), "Sachin Loader v%s  |  %s", app::kVersion, server_state_text());
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
		shared.selected_local = -1;
		state.page = PageHome;
		reload_local_apps();
	}
	last_frame = frame;

	if (state.page < 0 || state.page >= PageCount)
		state.page = PageHome;

	// The saved palette applies to the dashboard; the login screens keep the
	// dark one (see ui.cpp).
	if (shared.light != state.light)
		dash::apply_theme(shared.light);

	tick_launch();

	draw_background();
	draw_sidebar();
	draw_title_bar();

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
	draw_toasts();
}

} // namespace screen_dashboard
