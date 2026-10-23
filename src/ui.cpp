#include "app.h"

#include "core/auth.h"
#include "core/dllmgr.h"
#include "core/games.h"
#include "core/logs.h"
#include "core/settings.h"
#include "core/theme.h"
#include "ui/branding.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace app {

float ViewportWidth()
{
	const float w = ImGui::GetIO().DisplaySize.x;
	return w > 1.0f ? w : kDefaultWidth;
}

float ViewportHeight()
{
	const float h = ImGui::GetIO().DisplaySize.y;
	return h > 1.0f ? h : kDefaultHeight;
}

} // namespace app

namespace ui {

struct NavItem
{
	Screen screen;
	const char* label;
	IconDrawer icon;
};

static Screen nav_highlight()
{
	return S.screen == Screen::GameConfig ? Screen::Library : S.screen;
}

static const std::vector<NavItem>& nav_items()
{
	static const std::vector<NavItem> items = {
		{ Screen::Dashboard, "Dashboard", [](ImVec2 c, float s, ImU32 color) { icon_home(c, s, color); } },
		{ Screen::Library, "Library", [](ImVec2 c, float s, ImU32 color) { icon_grid(c, s, color); } },
		{ Screen::DllManagement, "DLL Tools", [](ImVec2 c, float s, ImU32 color) { icon_plug(c, s, color); } },
		{ Screen::Logs, "Logs", [](ImVec2 c, float s, ImU32 color) { icon_list(c, s, color); } },
		{ Screen::ThemeCustomizer, "Theme", [](ImVec2 c, float s, ImU32 color) { icon_palette(c, s, color); } },
		{ Screen::Settings, "Settings", [](ImVec2 c, float s, ImU32 color) { icon_gear(c, s, color); } },
		{ Screen::About, "About", [](ImVec2 c, float s, ImU32 color) { icon_info(c, s, color); } },
	};
	return items;
}

void draw_sidebar(float h)
{
	const float sidebar_w = layout::kSidebarWidth;
	const Rect bar = rect(0.0f, layout::kTopbarHeight, sidebar_w, h);
	box(bar, P.panel, 0.0f);
	ImGui::GetWindowDrawList()->AddLine(ImVec2(bar.max.x, bar.min.y), ImVec2(bar.max.x, bar.max.y), P.border_soft);

	float y = bar.min.y + 22.0f;
	for (const NavItem& item : nav_items())
	{
		const bool active = nav_highlight() == item.screen;
		const Rect row = rect(bar.min.x + 13.0f, y, bar.max.x - 13.0f, y + 50.0f);

		ImGui::PushID(item.label);
		ImGui::SetCursorScreenPos(row.min);
		const bool clicked = ImGui::InvisibleButton("##navitem", ImVec2(width(row), height(row)));
		const bool hovered = ImGui::IsItemHovered();
		if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		const float t = animate("##navanim", hovered ? 1.0f : 0.0f, 14.0f);

		if (active)
			box(row, colorutil::alpha(P.accent, 0.20f), 12.0f, colorutil::alpha(P.accent, 0.5f), 1.0f);
		else if (t > 0.01f)
			box(row, colorutil::alpha(P.text, 0.05f * t), 12.0f);

		item.icon(center(row), 19.0f, active ? P.accent : colorutil::mix(P.text_secondary, P.text, t));

		if (hovered)
		{
			ImGui::SetNextWindowPos(ImVec2(row.max.x + 10.0f, row.min.y - 2.0f));
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(P.panel));
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(P.border_strong));
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.text));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
			ImGui::PushFont(F.link);
			if (ImGui::BeginTooltip())
			{
				ImGui::TextUnformatted(item.label);
				ImGui::EndTooltip();
			}
			ImGui::PopFont();
			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(3);
		}

		if (clicked)
			Navigate(item.screen);

		ImGui::PopID();
		y += 58.0f;
	}

	const float status_cx = bar.min.x + sidebar_w * 0.5f;
	const float dot_y = bar.max.y - 46.0f;
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(status_cx, dot_y), 3.5f, P.success);
	char version_label[16];
	std::snprintf(version_label, sizeof(version_label), "v%s", app::kVersion);
	text_center_x(F.micro, 9.0f, status_cx, dot_y + 12.0f, P.text_dim, version_label);
}

void draw_topbar(float w)
{
	const Rect bar = rect(0.0f, 0.0f, w, layout::kTopbarHeight);
	box(bar, P.bg_top, 0.0f);
	ImGui::GetWindowDrawList()->AddLine(ImVec2(0, bar.max.y), ImVec2(w, bar.max.y), P.border_soft);

	const float controls_w = 104.0f;
	ImGui::SetCursorScreenPos(ImVec2(0, 0));
	const bool drag_clicked = ImGui::InvisibleButton("##window_drag", ImVec2(w - controls_w, layout::kTopbarHeight));
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
	else if (drag_clicked)
		StartWindowDrag();

	draw_logo(ImVec2(30.0f, bar.max.y * 0.5f), 24.0f);
	draw_wordmark_left(F.brand_small, 54.0f, bar.max.y * 0.5f - 11.0f, 18.0f, 1.0f);

	const Rect minimize = rect(w - 84.0f, 18.0f, w - 54.0f, 46.0f);
	ImGui::SetCursorScreenPos(minimize.min);
	const bool minimize_clicked = ImGui::InvisibleButton("##window_minimize", ImVec2(width(minimize), height(minimize)));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		box(minimize, colorutil::alpha(P.text, 0.06f), 8.0f);
	}
	icon_minimize(center(minimize), 14.0f, P.text_secondary);

	const Rect close = rect(w - 50.0f, 18.0f, w - 20.0f, 46.0f);
	ImGui::SetCursorScreenPos(close.min);
	const bool close_clicked = ImGui::InvisibleButton("##window_close", ImVec2(width(close), height(close)));
	const bool close_hovered = ImGui::IsItemHovered();
	if (close_hovered)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		box(close, colorutil::alpha(P.danger, 0.16f), 8.0f);
	}
	icon_close(center(close), 14.0f, close_hovered ? P.danger : P.text_secondary);

	if (minimize_clicked)
		ShowWindow(g_hwnd, SW_MINIMIZE);
	if (close_clicked)
		PostQuitMessage(0);
}

void dispatch_main_screen()
{
	switch (S.screen)
	{
	case Screen::Dashboard: page_dashboard(); break;
	case Screen::Library: page_library(); break;
	case Screen::DllManagement: page_dll_management(); break;
	case Screen::GameConfig: page_game_config(); break;
	case Screen::Logs: page_logs(); break;
	case Screen::ThemeCustomizer: page_theme_customizer(); break;
	case Screen::Settings: page_settings(); break;
	case Screen::About: page_about(); break;
	default: break;
	}
}

void seed_from_settings()
{
	const settings::AppSettings& s = settings::current();
	if (s.remember_me && !s.remembered_username.empty())
	{
		std::snprintf(S.username, sizeof(S.username), "%s", s.remembered_username.c_str());
		S.remember = true;
	}
	S.screen = s.skip_splash ? Screen::Login : Screen::Splash;
}

} // namespace ui

void app::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	std::srand(static_cast<unsigned int>(std::time(nullptr)));

	settings::load();
	theme::Manager::instance().load_all();
	games::Library::instance().load();
	dllmgr::TargetRegistry::instance().load();
	auth::load();

	ui::LoadFonts();
	ui::seed_from_settings();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(0, 0);
	style.WindowBorderSize = 0.0f;
	style.WindowRounding = 0.0f;
	style.FramePadding = ImVec2(0, 0);
	style.FrameBorderSize = 0.0f;
	style.FrameRounding = 0.0f;
	style.ItemSpacing = ImVec2(0, 0);
	style.ScrollbarSize = 0.0f;
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.49f, 0.30f, 1.0f, 0.45f);

	logs::info("Sachin Game Loader started");
}

void app::Shutdown()
{
	settings::save();
	games::Library::instance().save();
	dllmgr::TargetRegistry::instance().save();
}

void app::Render()
{
	ui::RefreshPalette();

	ImGuiIO& io = ImGui::GetIO();
	const float dt = colorutil::clampf(io.DeltaTime, 0.0f, 0.05f);

	ui::S.time += dt;
	ui::S.screen_time += dt;
	ui::S.banner_age += dt;
	ui::S.fps += (io.Framerate - ui::S.fps) * colorutil::clampf(dt * 5.0f, 0.0f, 1.0f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(ui::P.text);
	style.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(ui::P.text_dim);
	style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(ui::P.bg_top);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);

	if (ui::S.screen == ui::Screen::Splash && ui::S.screen_time >= 2.2f)
		ui::Navigate(ui::Screen::Login);

	if (ui::S.screen == ui::Screen::Login && ui::S.authenticating)
	{
		ui::S.auth_time += dt;
		if (ui::S.auth_time >= 0.7f)
		{
			ui::S.authenticating = false;
			const auth::Result result = auth::login(ui::S.username, ui::S.password);
			if (result.ok)
			{
				ui::S.session_user = ui::S.username;
				ui::S.password[0] = '\0';
				ui::S.reveal_password = false;

				settings::AppSettings& s = settings::current();
				s.remember_me = ui::S.remember;
				s.remembered_username = ui::S.remember ? ui::S.session_user : std::string();
				settings::save();

				logs::success("Signed in as " + ui::S.session_user);
				ui::Navigate(ui::Screen::Dashboard);
			}
			else
			{
				ui::S.login_error = true;
				ui::S.login_info = false;
				ui::S.login_message = result.message;
				ui::S.banner_age = 0.0f;
				logs::warning("Failed sign-in attempt for " + std::string(ui::S.username));
			}
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

	const float w = ViewportWidth();
	const float h = ViewportHeight();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(w, h));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##root", nullptr, flags);

	if (ui::IsAuthScreen(ui::S.screen))
	{
		if (ui::S.screen == ui::Screen::Splash)
			ui::page_splash();
		else
			ui::page_login();
	}
	else
	{
		ui::draw_background(w, h, true);
		ui::draw_sidebar(h);
		ui::dispatch_main_screen();
	}

	// Window drag/minimize/close must always be reachable, even on the
	// splash/login screens where there is no sidebar yet.
	ui::draw_topbar(w);

	ui::render_toasts();

	if (ui::S.screen_time < 0.32f)
	{
		const float fade = 1.0f - ui::S.screen_time / 0.32f;
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(0, 0), ImVec2(w, h),
			colorutil::alpha(ui::P.bg_bottom, fade * 0.75f));
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
}
