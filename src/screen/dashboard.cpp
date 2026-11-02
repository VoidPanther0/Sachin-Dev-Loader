#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cctype>
#include <cstdio>
#include <string>

// Window shell shown after sign-in: sidebar with the logo and navigation, the
// title bar with the page title, session pill and window buttons, and the
// current page. Only Home has its own file (home.cpp); the other navigation
// entries draw a placeholder until they get one.

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

// Nav sits below the full-width header strip (y=0..40). draw_header owns the
// logo and the wordmark now, so the sidebar is just the icon rail.
constexpr float kHeaderHeight = 40.0f;
constexpr float kProfileStripHeight = 64.0f;
constexpr float kNavTop = 60.0f;
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

// Full-width top strip: brand (logo + wordmark), window drag, session pill,
// window buttons. The sidebar and the content area start below it.
void draw_header()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, kHeaderHeight), color_u32(kSidebar));
	draw->AddLine(ImVec2(0.0f, kHeaderHeight - 1.0f), ImVec2(app::kWidth, kHeaderHeight - 1.0f),
		color_u32(kLine), 1.0f);

	// Brand: Slogo + wordmark on the left.
	login_ui::DrawLogo(ImVec2(26.0f, kHeaderHeight * 0.5f), 22.0f);
	const char* wordmark = "SACHIN LOADER";
	gradient_text(font.semibold, 15.0f, ImVec2(50.0f, 12.0f), wordmark, lift(kBlue, 0.15f), kPurple);

	// Drag strip across the header except the controls on the right.
	ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
	ImGui::InvisibleButton("##header_drag", ImVec2(app::kWidth - 240.0f, kHeaderHeight));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	// Session state is reported from the real session, never assumed.
	const bool live = sdauth::HasSession();
	pill(rect(app::kWidth - 232.0f, 10.0f, app::kWidth - 112.0f, 30.0f),
		live ? "SESSION ACTIVE" : "SESSION ENDED", live ? kGreen : kRed);

	if (window_button("##minimize", rect(app::kWidth - 96.0f, 4.0f, app::kWidth - 64.0f, kHeaderHeight - 4.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (window_button("##close", rect(app::kWidth - 48.0f, 4.0f, app::kWidth - 16.0f, kHeaderHeight - 4.0f), true))
		PostQuitMessage(0);
}

// Page title inside the content area (sits above the first page card).
void draw_title_bar()
{
	const NavEntry& entry = (state.page >= 0 && state.page < kNavCount) ? kNav[state.page] : kNav[0];

	const Rect accent_bar = rect(kContentLeft - 10.0f, 54.0f, kContentLeft - 6.0f, 78.0f);
	box(accent_bar, accent_color(), 2.0f);
	text(font.semibold, 19.0f, ImVec2(kContentLeft, 46.0f), kText, entry.label);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(ImVec2(kContentLeft, 70.0f), ImVec2(kContentRight, 86.0f), true);
	text(font.caption, 11.0f, ImVec2(kContentLeft, 72.0f), kMuted, entry.subtitle);
	draw->PopClipRect();
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

void draw_sidebar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float sidebar_bottom = app::kHeight - kProfileStripHeight;
	draw->AddRectFilled(ImVec2(0.0f, kHeaderHeight), ImVec2(kSidebarWidth, sidebar_bottom), color_u32(kSidebar));
	draw->AddLine(ImVec2(kSidebarWidth - 1.0f, kHeaderHeight), ImVec2(kSidebarWidth - 1.0f, sidebar_bottom),
		color_u32(kLine), 1.0f);

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

// Full-width strip at the bottom: avatar + name on the left, STATUS / VERSION
// (and FPS when enabled) on the right.
void draw_profile_strip()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float y0 = app::kHeight - kProfileStripHeight;
	const float y1 = app::kHeight;
	const float cy = (y0 + y1) * 0.5f;

	draw->AddRectFilled(ImVec2(0.0f, y0), ImVec2(app::kWidth, y1), color_u32(kSidebar));
	draw->AddLine(ImVec2(0.0f, y0), ImVec2(app::kWidth, y0), color_u32(kLine), 1.0f);

	// --- left: gradient avatar + name + hint ------------------------------
	const float r = 18.0f;
	const ImVec2 avatar(26.0f + r, cy);
	const Rect ar = rect(avatar.x - r, avatar.y - r, avatar.x + r, avatar.y + r);
	gradient_box(ar, accent_color(), kPurple, r);

	const sdauth::Session session = sdauth::GetSession();
	const char* uname = session.username.empty() ? "friend" : session.username.c_str();
	char letter[2] = { static_cast<char>(std::toupper(static_cast<unsigned char>(uname[0]))), 0 };
	const float letter_w = text_width(font.semibold, 18.0f, letter);
	text(font.semibold, 18.0f, ImVec2(avatar.x - letter_w * 0.5f, avatar.y - 10.0f), kWhite, letter);

	text(font.semibold, 14.0f, ImVec2(avatar.x + r + 14.0f, cy - 13.0f), kText, uname);
	text(font.caption, 11.0f, ImVec2(avatar.x + r + 14.0f, cy + 4.0f), kMuted, "Signed in");

	// --- right: STATUS | VERSION (and FPS) --------------------------------
	char version[32];
	std::snprintf(version, sizeof(version), "v%s", app::kVersion);
	const float v_w = text_width(font.semibold, 13.0f, version);
	const float version_x = app::kWidth - 24.0f - v_w;
	text(font.caption, 10.0f, ImVec2(version_x, cy - 15.0f), kDim, "VERSION");
	text(font.semibold, 13.0f, ImVec2(version_x, cy + 2.0f), accent_color(), version);

	const ImVec4 state_color = server_state_color();
	const char* status_text_v = server_state_text();
	const float s_w = text_width(font.semibold, 13.0f, status_text_v) + 14.0f;
	const float status_x = version_x - 36.0f - s_w;
	text(font.caption, 10.0f, ImVec2(status_x, cy - 15.0f), kDim, "STATUS");
	draw->AddCircleFilled(ImVec2(status_x + 5.0f, cy + 7.0f), 3.0f, color_u32(state_color));
	text(font.semibold, 13.0f, ImVec2(status_x + 14.0f, cy + 2.0f),
		state.light ? dim(state_color, 0.25f) : lift(state_color, 0.10f), status_text_v);

	if (shared.show_fps)
	{
		char fps[32];
		std::snprintf(fps, sizeof(fps), "FPS %d", static_cast<int>(state.fps + 0.5f));
		const float f_w = text_width(font.caption, 11.0f, fps);
		const float fps_x = status_x - 24.0f - f_w;
		text(font.caption, 11.0f, ImVec2(fps_x, cy + 2.0f), kDim, fps);
	}
}

// Pages that do not have their own file yet. They stay in the navigation, so
// they show a clear placeholder instead of pretending to be the Home page.
void draw_placeholder(const char* title, IconFn icon)
{
	const Rect card = rect(kContentLeft, kContentTop, kContentRight, kContentBottom);
	box(card, kCard, 14.0f, card_border());
	section_header(card, title, icon);
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 64.0f);

	const Rect area = rect(card.min.x + 20.0f, card.min.y + 80.0f, card.max.x - 20.0f, card.max.y - 16.0f);
	draw_empty_state(area, "Coming soon", "This page is not part of this build yet.");
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
	draw_header();
	draw_sidebar();
	draw_title_bar();

	// Page dispatch. Home has its own file; the rest of the navigation shows
	// the placeholder above until a page gets its own file and dash::Page value.
	switch (state.page)
	{
	case PageProduct:     DrawProduct(); break;
	case PageGameUpdates: DrawGameUpdates(); break;
	case PageSocials:     DrawSocials(); break;
	case PageSettings:    draw_placeholder("Settings", icon_settings); break;
	default:              DrawHome(); break;
	}

	draw_profile_strip();
	draw_toasts();
}

} // namespace screen_dashboard
