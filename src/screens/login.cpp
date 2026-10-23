#include "app.h"
#include "core/auth.h"
#include "ui/branding.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

namespace ui {

namespace {

void reset_banner(bool error, bool info, const std::string& message)
{
	S.login_error = error;
	S.login_info = info;
	S.login_message = message;
	S.banner_age = 0.0f;
}

void submit()
{
	if (S.username[0] == '\0' || S.password[0] == '\0')
	{
		reset_banner(true, false, "Enter a username and password.");
		return;
	}

	if (S.register_mode)
	{
		const auth::Result result = auth::register_account(S.username, S.password);
		if (result.ok)
		{
			S.register_mode = false;
			S.password[0] = '\0';
			reset_banner(false, true, result.message);
		}
		else
		{
			reset_banner(true, false, result.message);
		}
		return;
	}

	S.authenticating = true;
	S.auth_time = 0.0f;
}

} // namespace

void page_login()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float cx = w * 0.5f;

	draw_background(w, h, false);

	if (S.authenticating)
	{
		soft_glow(ImVec2(cx, h * 0.30f), 320.0f, P.accent, 0.012f, 26);
		draw_logo(ImVec2(cx, h * 0.5f - 150.0f), 100.0f);
		draw_wordmark_center(cx, h * 0.5f - 96.0f, 36.0f, 1.0f);
		text_center_x(F.subtitle, 19.0f, cx, h * 0.5f - 44.0f, P.text_secondary, "One Loader. Every Game.");

		const char* auth_label = "Signing in...";
		const float group_w = 24.0f + 10.0f + text_width(F.body, 15.0f, auth_label);
		const float group_x = cx - group_w * 0.5f;
		spinner(ImVec2(group_x + 11.0f, h * 0.5f + 20.0f), 10.0f, 2.6f, P.accent, S.time);
		text(F.body, 15.0f, ImVec2(group_x + 34.0f, h * 0.5f + 11.0f), P.text_secondary, auth_label);

		return;
	}

	const float form_w = 560.0f;
	const float form_x = cx - form_w * 0.5f;

	draw_logo(ImVec2(cx, h * 0.5f - 232.0f), 84.0f);
	draw_wordmark_center(cx, h * 0.5f - 184.0f, 36.0f, 1.0f);
	text_center_x(F.subtitle, 19.0f, cx, h * 0.5f - 138.0f, P.text_secondary, S.register_mode ? "Create a local account" : "One Loader. Every Game.");

	const bool has_banner = S.login_error || S.login_info;
	float y = h * 0.5f - 56.0f;

	if (has_banner)
	{
		draw_banner(rect(form_x, y - 44.0f, form_x + form_w, y), S.login_error ? Level::Danger : Level::Info, S.login_message.c_str());
		y += 22.0f;
	}

	const Rect user_field = rect(form_x, y, form_x + form_w, y + 56.0f);
	draw_field("##username", "Username", user_field, S.username, sizeof(S.username), false);

	const Rect pass_field = rect(form_x, y + 70.0f, form_x + form_w, y + 126.0f);
	draw_field("##password", "Password", pass_field, S.password, sizeof(S.password), true, &S.reveal_password);

	if (!S.register_mode)
	{
		const Rect remember = rect(form_x + 2.0f, y + 134.0f, form_x + 158.0f, y + 158.0f);
		checkbox("##remember", remember, &S.remember, "Remember me");

		const char* switch_label = "Create an account";
		const float switch_w = text_width(F.link, 14.0f, switch_label);
		const Rect switch_rect = rect(form_x + form_w - switch_w - 4.0f, y + 134.0f, form_x + form_w, y + 158.0f);
		if (link_button("##go_register", switch_rect, switch_label, P.accent))
		{
			S.register_mode = true;
			S.password[0] = '\0';
			reset_banner(false, false, "");
		}
	}
	else
	{
		const char* switch_label = "Back to sign in";
		const float switch_w = text_width(F.link, 14.0f, switch_label);
		const Rect switch_rect = rect(form_x + form_w - switch_w - 4.0f, y + 134.0f, form_x + form_w, y + 158.0f);
		if (link_button("##go_login", switch_rect, switch_label, P.accent))
		{
			S.register_mode = false;
			S.password[0] = '\0';
			reset_banner(false, false, "");
		}
	}

	const Rect submit_rect = rect(form_x, y + 176.0f, form_x + form_w, y + 230.0f);
	const bool submit_clicked = primary_button("##login_submit", submit_rect, S.register_mode ? "Create account" : "Sign in", true, true);
	const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);

	if (submit_clicked || enter)
		submit();

	text_center_x(F.tiny, 11.0f, cx, h - 44.0f, P.text_dim, "Local demo sign-in - no server, no network calls");
}

} // namespace ui
