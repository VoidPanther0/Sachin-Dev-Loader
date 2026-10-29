#include "common/dashboard_common.h"

#include <cctype>
#include <cstdio>
#include <string>

// Account: profile, access time and the device this account is bound to.

namespace dash {

using namespace ui;

namespace {

void draw_profile_card(const Rect& card)
{
	box(card, kCard, 14.0f, card_border());

	const sdauth::Session session = sdauth::GetSession();
	const std::string name = session.username.empty() ? "Guest" : session.username;

	// Avatar.
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec2 avatar(card.min.x + 64.0f, card.min.y + 64.0f);
	draw->AddCircleFilled(avatar, 36.0f, color_u32(fade(accent_color(), 0.18f)), 48);
	draw->AddCircle(avatar, 36.0f, color_u32(fade(accent_color(), 0.5f)), 48, 1.6f);
	char letter[2] = { static_cast<char>(std::toupper(static_cast<unsigned char>(name[0]))), '\0' };
	const float letter_w = text_width(font.title, 30.0f, letter);
	text(font.title, 30.0f, ImVec2(avatar.x - letter_w * 0.5f, avatar.y - 18.0f), lift(accent_color(), 0.2f), letter);

	text(font.title, 24.0f, ImVec2(card.min.x + 120.0f, card.min.y + 38.0f), kText, name.c_str());
	pill(rect(card.min.x + 120.0f, card.min.y + 76.0f, card.min.x + 232.0f, card.min.y + 98.0f), "SIGNED IN", kGreen);

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 128.0f);

	const int days_left = access_days_left();
	const std::string date = access_date();
	char access[64];
	if (days_left < 0)
		std::snprintf(access, sizeof(access), "Lifetime");
	else
		std::snprintf(access, sizeof(access), "%d day%s  -  %s", days_left, days_left == 1 ? "" : "s", date.c_str());

	const std::string uptime = session_time();
	float y = card.min.y + 152.0f;
	info_row(card, y, "ACCESS", access, days_left >= 0 && days_left <= 7 ? kOrange : kGreen);
	y += 38.0f;
	info_row(card, y, "SESSION", uptime.c_str(), kText);
	y += 38.0f;
	info_row(card, y, "COMPONENTS", std::to_string(sdauth::Components().size()).c_str(), kText);
	y += 38.0f;
	info_row(card, y, "LOADER", app::kVersion, kText);

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.max.y - 64.0f));
	if (secondary_button("Sign out", ImVec2(160.0f, 36.0f)))
		screen_login::SignOut();
}

void draw_device_card(const Rect& card)
{
	card_frame(card, "Device", "This account is bound to one PC.");

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 84.0f), kDim, "DEVICE ID");

	const std::string& hwid = sdauth::MachineHwid();
	const Rect field = rect(card.min.x + 28.0f, card.min.y + 104.0f, card.max.x - 28.0f, card.min.y + 142.0f);
	box(field, kCardSoft, 9.0f, &kLine);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(field.min, ImVec2(field.max.x - 8.0f, field.max.y), true);
	text(font.body, 13.0f, ImVec2(field.min.x + 12.0f, field.min.y + 10.0f), kText,
		hwid.empty() ? "Unavailable" : hwid.c_str());
	draw->PopClipRect();

	if (!hwid.empty())
	{
		const bool copied = ImGui::GetTime() - shared.copied_time < 2.0;
		ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 156.0f));
		if (secondary_button(copied ? "Copied" : "Copy device ID", ImVec2(160.0f, 34.0f)))
		{
			ImGui::SetClipboardText(hwid.c_str());
			shared.copied_time = ImGui::GetTime();
		}
	}

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 214.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 230.0f), kDim, "NEW PC?");
	text_wrapped(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 250.0f), kMuted,
		"If you change your PC, ask an admin to reset the device binding. Then sign in once on the new PC.",
		width(card) - 56.0f);
}

} // namespace

void DrawAccount()
{
	draw_profile_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_device_card(rect(706.0f, kContentTop, kContentRight, 452.0f));
}

} // namespace dash
