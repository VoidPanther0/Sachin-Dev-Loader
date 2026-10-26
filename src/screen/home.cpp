#include "../ui.h"
#include "../auth/supabase_auth.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// Main window shell: title bar, sidebar, one page of content and a footer.
//
// This is the blank template home screen. It shows real session data and an
// empty content area. Replace the page functions at the bottom with your own
// layout; the sidebar, title bar and window buttons can stay as they are.

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

constexpr float kTitleHeight = 64.0f;
constexpr float kSidebarWidth = 232.0f;
constexpr float kContentLeft = 264.0f;
constexpr float kContentRight = 1132.0f;

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

void icon_settings(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(c, 6.5f, color, 24, 1.6f);
	draw->AddCircleFilled(c, 2.2f, color, 12);
	for (int i = 0; i < 8; ++i)
	{
		const float angle = static_cast<float>(i) * (IM_PI * 2.0f / 8.0f);
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

// ----------------------------------------------------------------- chrome ----

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, app::kHeight), color_u32(kHomeBg));
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
	text(font.caption, 10.0f, ImVec2(60.0f, 34.0f), kMuted, "Template home screen");

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
	case 0: icon_home(icon_center, icon_color); break;
	case 1: icon_library(icon_center, icon_color); break;
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
	if (nav_item("##nav_home", 0, "Home", 0, 108.0f))
		state.page = 0;
	if (nav_item("##nav_library", 1, "Library", 1, 160.0f))
		state.page = 1;
	if (nav_item("##nav_settings", 2, "Settings", 2, 212.0f))
		state.page = 2;

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
}

void draw_footer()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(kContentLeft, 672.0f), ImVec2(kContentRight, 672.0f), color_u32(kLine), 1.0f);
	draw->AddCircleFilled(ImVec2(kContentLeft + 4.0f, 690.0f), 3.0f, color_u32(kGreen));

	char left[128];
	std::snprintf(left, sizeof(left), "Sachin Loader v%s  |  Secure  -  Private  -  Fast", app::kVersion);
	text(font.caption, 10.0f, ImVec2(kContentLeft + 14.0f, 684.0f), kDim, left);

	char right[64];
	std::snprintf(right, sizeof(right), "FPS %d", static_cast<int>(state.fps + 0.5f));
	const float right_width = text_width(font.caption, 10.0f, right);
	text(font.caption, 10.0f, ImVec2(kContentRight - right_width, 684.0f), kDim, right);
}

// ------------------------------------------------------------------ pages ----

void draw_hero()
{
	const Rect hero = rect(kContentLeft, 152.0f, kContentRight, 332.0f);
	gradient_box(hero, ImVec4(0.055f, 0.085f, 0.205f, 1.0f), ImVec4(0.135f, 0.070f, 0.260f, 1.0f), 16.0f);
	ImGui::GetWindowDrawList()->AddRect(hero.min, hero.max, color_u32(kBorder), 16.0f, 0, 1.0f);
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(hero.max.x - 70.0f, hero.min.y + 20.0f), 170.0f,
		color_u32(fade(kPurple, 0.10f)), 64);
	text(font.title, 118.0f, ImVec2(hero.max.x - 168.0f, 200.0f), fade(kPurple, 0.10f), "S");

	text(font.caption, 10.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 26.0f), accent_color(), "WELCOME BACK");

	const sdauth::Session session = sdauth::GetSession();
	char greeting[96];
	std::snprintf(greeting, sizeof(greeting), "Hi, %s",
		session.username.empty() ? "friend" : session.username.c_str());
	text(font.title, 30.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 46.0f), kText, greeting);

	text(font.body, 14.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 92.0f), kMuted,
		"Your loader is ready. This is the blank template home screen.");

	char build[32];
	std::snprintf(build, sizeof(build), "BUILD %s", app::kVersion);
	pill(rect(hero.min.x + 32.0f, hero.min.y + 126.0f, hero.min.x + 112.0f, hero.min.y + 148.0f), build, kGreen);

	const bool update_ready = sdauth::StartupUpdateAvailable() || sdauth::UpdateStatus() == sdauth::UpdateFailed;
	pill(rect(hero.min.x + 124.0f, hero.min.y + 126.0f, hero.min.x + 124.0f + (update_ready ? 118.0f : 140.0f),
		hero.min.y + 148.0f), update_ready ? "UPDATE READY" : "AUTO UPDATE ON",
		update_ready ? kBlue : accent_color());

	ImGui::SetCursorScreenPos(ImVec2(hero.max.x - 164.0f, hero.min.y + 126.0f));
	if (secondary_button("Open library", ImVec2(132.0f, 32.0f)))
		state.page = 1;
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
	text(font.semibold, 16.0f, ImVec2(c.x - title_w * 0.5f, c.y + 10.0f), kText, title);

	const float sub_w = text_width(font.caption, 11.0f, subtitle);
	text(font.caption, 11.0f, ImVec2(c.x - sub_w * 0.5f, c.y + 36.0f), kMuted, subtitle);
}

void draw_home_page()
{
	draw_page_header("Home", "Your loader at a glance.");
	draw_hero();

	const Rect content = rect(kContentLeft, 352.0f, 800.0f, 652.0f);
	box(content, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(content.min.x + 24.0f, content.min.y + 20.0f), kText, "Content");
	text(font.caption, 11.0f, ImVec2(content.min.x + 24.0f, content.min.y + 46.0f), kMuted,
		"Your empty workspace. Add apps, games or tools here.");
	divider(content.min.x + 24.0f, content.max.x - 24.0f, content.min.y + 72.0f);

	draw_empty_state(rect(content.min.x + 24.0f, content.min.y + 90.0f, content.max.x - 24.0f, content.max.y - 22.0f),
		"Nothing here yet", "Start in src/screen/home.cpp");

	const Rect side = rect(816.0f, 352.0f, kContentRight, 652.0f);
	box(side, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(side.min.x + 24.0f, side.min.y + 20.0f), kText, "Session");

	const sdauth::Session session = sdauth::GetSession();
	const std::vector<sdauth::Component> components = sdauth::Components();

	char components_text[32];
	std::snprintf(components_text, sizeof(components_text), "%d", static_cast<int>(components.size()));

	const ImVec4 update_color = sdauth::StartupUpdateAvailable() ? accent_color()
		: (sdauth::UpdateStatus() == sdauth::UpdateFailed ? kRed : kGreen);
	const char* update_text = sdauth::StartupUpdateAvailable() ? "Update available"
		: (sdauth::UpdateStatus() == sdauth::UpdateFailed ? "Update failed" : "Up to date");

	struct Row { const char* label; const char* value; ImVec4 color; };
	const Row rows[] = {
		{ "ACCOUNT", session.username.empty() ? "Guest" : session.username.c_str(), kText },
		{ "BUILD", app::kVersion, kText },
		{ "COMPONENTS", components_text, kText },
		{ "UPDATES", update_text, update_color },
	};

	float y = side.min.y + 52.0f;
	for (const Row& row : rows)
	{
		text(font.caption, 10.0f, ImVec2(side.min.x + 24.0f, y), kDim, row.label);
		text(font.body, 14.0f, ImVec2(side.min.x + 24.0f, y + 16.0f), row.color, row.value);
		y += 42.0f;
		divider(side.min.x + 24.0f, side.max.x - 24.0f, y - 6.0f);
	}

	ImGui::SetCursorScreenPos(ImVec2(side.min.x + 24.0f, side.max.y - 54.0f));
	if (secondary_button("Sign out", ImVec2(side.max.x - side.min.x - 48.0f, 34.0f)))
		screen_login::SignOut();
}

void draw_library_page()
{
	draw_page_header("Library", "Content released by the server for this account.");

	const Rect card = rect(kContentLeft, 152.0f, kContentRight, 652.0f);
	box(card, kCard, 14.0f, &kBorder);

	const std::vector<sdauth::Component> components = sdauth::Components();
	if (components.empty())
	{
		draw_empty_state(rect(card.min.x + 32.0f, card.min.y + 32.0f, card.max.x - 32.0f, card.max.y - 32.0f),
			"No components yet", "Add rows to loader_components in Supabase");
		return;
	}

	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Available content");
	char count[64];
	std::snprintf(count, sizeof(count), "%d component%s", static_cast<int>(components.size()),
		components.size() == 1 ? "" : "s");
	text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 50.0f), kMuted, count);
	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 76.0f);

	float y = card.min.y + 96.0f;
	int shown = 0;
	for (const sdauth::Component& component : components)
	{
		if (shown == 6)
			break;

		text(font.semibold, 15.0f, ImVec2(card.min.x + 28.0f, y), kText, component.name.c_str());
		text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, y + 22.0f), kMuted,
			component.description.empty() ? "No description" : component.description.c_str());

		const float pill_w = text_width(font.caption, 10.0f, component.version.c_str()) + 60.0f;
		pill(rect(card.max.x - 28.0f - pill_w, y, card.max.x - 28.0f, y + 22.0f), component.version.c_str(),
			kGreen);

		y += 58.0f;
		++shown;
		divider(card.min.x + 28.0f, card.max.x - 28.0f, y - 12.0f);
	}

	if (static_cast<int>(components.size()) > shown)
	{
		char more[64];
		std::snprintf(more, sizeof(more), "+ %d more components",
			static_cast<int>(components.size()) - shown);
		text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, y), kDim, more);
	}
}

void draw_settings_page()
{
	draw_page_header("Settings", "Build, account and update information.");

	const Rect loader = rect(kContentLeft, 152.0f, 780.0f, 452.0f);
	box(loader, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(loader.min.x + 28.0f, loader.min.y + 22.0f), kText, "Loader");

	struct Row { const char* label; const char* value; ImVec4 color; };
	const char* update_mode = "Automatic";
	const char* update_state = sdauth::StartupUpdateAvailable() ? "Update available"
		: (sdauth::UpdateStatus() == sdauth::UpdateFailed ? "Last update failed" : "Up to date");

	const Row rows[] = {
		{ "VERSION", app::kVersion, kText },
		{ "UPDATE CHANNEL", update_mode, kGreen },
		{ "UPDATE STATE", update_state, sdauth::StartupUpdateAvailable() ? accent_color() : kGreen },
		{ "STARTUP CHECK", "Passed", kGreen },
	};

	float y = loader.min.y + 64.0f;
	for (const Row& row : rows)
	{
		text(font.caption, 10.0f, ImVec2(loader.min.x + 28.0f, y), kDim, row.label);
		text(font.body, 14.0f, ImVec2(loader.min.x + 220.0f, y - 2.0f), row.color, row.value);
		y += 50.0f;
		divider(loader.min.x + 28.0f, loader.max.x - 28.0f, y - 14.0f);
	}

	text(font.caption, 10.0f, ImVec2(loader.min.x + 28.0f, loader.max.y - 32.0f), kDim,
		"Updates download, verify and restart by themselves.");

	const Rect account = rect(kContentLeft, 472.0f, 780.0f, 652.0f);
	box(account, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(account.min.x + 28.0f, account.min.y + 22.0f), kText, "Account");

	const sdauth::Session session = sdauth::GetSession();
	text(font.caption, 10.0f, ImVec2(account.min.x + 28.0f, account.min.y + 64.0f), kDim, "SIGNED IN AS");
	text(font.body, 15.0f, ImVec2(account.min.x + 28.0f, account.min.y + 82.0f), kText,
		session.username.empty() ? "Guest" : session.username.c_str());

	ImGui::SetCursorScreenPos(ImVec2(account.min.x + 28.0f, account.max.y - 58.0f));
	if (secondary_button("Sign out", ImVec2(160.0f, 34.0f)))
		screen_login::SignOut();

	const Rect about = rect(796.0f, 152.0f, kContentRight, 652.0f);
	box(about, kCard, 14.0f, &kBorder);

	const Rect logo = rect(about.min.x + 28.0f, about.min.y + 28.0f, about.min.x + 76.0f, about.min.y + 76.0f);
	gradient_box(logo, kBlue, kPurple, 10.0f);
	text_center(font.semibold, 24.0f, logo, kText, "S");

	text(font.semibold, 20.0f, ImVec2(about.min.x + 28.0f, about.min.y + 96.0f), kText, "Sachin Loader");
	text(font.caption, 11.0f, ImVec2(about.min.x + 28.0f, about.min.y + 124.0f), kMuted,
		"Blank home screen template");

	text(font.body, 13.0f, ImVec2(about.min.x + 28.0f, about.min.y + 168.0f), kMuted,
		"Edit src/screen/home.cpp to add your");
	text(font.body, 13.0f, ImVec2(about.min.x + 28.0f, about.min.y + 192.0f), kMuted,
		"own pages, cards and actions.");

	divider(about.min.x + 28.0f, about.max.x - 28.0f, about.min.y + 236.0f);

	text(font.caption, 10.0f, ImVec2(about.min.x + 28.0f, about.min.y + 260.0f), kDim, "BUILT WITH");
	text(font.body, 13.0f, ImVec2(about.min.x + 28.0f, about.min.y + 278.0f), kText,
		"Dear ImGui, Win32 and DirectX 11");

	text(font.caption, 10.0f, ImVec2(about.min.x + 28.0f, about.min.y + 320.0f), kDim, "AUTO UPDATE");
	text(font.body, 13.0f, ImVec2(about.min.x + 28.0f, about.min.y + 338.0f), kText,
		"Checks the server on every launch");

	text(font.caption, 10.0f, ImVec2(about.min.x + 28.0f, about.min.y + 380.0f), kDim, "DEVICE ID");
	const std::string hwid = sdauth::MachineHwid();
	const std::string short_hwid = hwid.size() > 12 ? hwid.substr(0, 12) + "..." : hwid;
	text(font.body, 13.0f, ImVec2(about.min.x + 28.0f, about.min.y + 398.0f), kText,
		short_hwid.empty() ? "Unavailable" : short_hwid.c_str());
}

} // namespace

void Render()
{
	draw_background();
	draw_title_bar();
	draw_sidebar();

	switch (state.page)
	{
	case 1: draw_library_page(); break;
	case 2: draw_settings_page(); break;
	default: draw_home_page(); break;
	}

	draw_footer();
}

} // namespace screen_home
