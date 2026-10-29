#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cstdio>
#include <string>

// About: branding, real backend state and a manual update check. Nothing here
// downloads or installs anything unless the user presses the button, and the
// check never touches the signed-in component list.

namespace dash {

using namespace ui;

namespace {

void draw_identity_card(const Rect& card)
{
	box(card, kCard, 14.0f, card_border());

	login_ui::DrawLogo(ImVec2(card.min.x + 84.0f, card.min.y + 92.0f), 96.0f);

	text(font.title, 28.0f, ImVec2(card.min.x + 156.0f, card.min.y + 58.0f), kText, "Sachin Loader");
	text(font.body, 14.0f, ImVec2(card.min.x + 156.0f, card.min.y + 98.0f), kMuted, "Software apps and game loader.");

	char build[32];
	std::snprintf(build, sizeof(build), "VERSION %s", app::kVersion);
	pill(rect(card.min.x + 156.0f, card.min.y + 130.0f, card.min.x + 156.0f + 124.0f, card.min.y + 152.0f),
		build, kGreen);

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 196.0f);

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 214.0f), kDim, "WHAT IT DOES");
	text_wrapped(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 236.0f), kMuted,
		"Sign in once, then launch the apps, games and tools released for your account from one place. "
		"Every download is checked with SHA-256 and your account is bound to your PC.",
		width(card) - 56.0f);

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.max.y - 66.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.max.y - 52.0f), kDim, "MADE BY");
	text(font.semibold, 15.0f, ImVec2(card.min.x + 28.0f, card.max.y - 34.0f), kText, "Sachin Dev");
}

// Backend state straight from the session. No invented uptime or status.
void draw_status_card(const Rect& card)
{
	card_frame(card, "Backend", "Taken from the session this loader really holds.");

	const ImVec4 state_color = server_state_color();
	float y = card.min.y + 74.0f;
	info_row(card, y, "CONNECTION", server_state_text(), state_color);
	y += 38.0f;

	const std::string server_version = sdauth::StartupVersion();
	info_row(card, y, "SERVER VERSION", server_version.empty() ? "Unknown" : server_version.c_str(), kText);
	y += 38.0f;

	const std::string components = std::to_string(sdauth::Components().size());
	info_row(card, y, "COMPONENTS", components.c_str(), kText);
	y += 38.0f;

	const std::string check = sdauth::StartupMessage();
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, y + 6.0f), kDim, "LAST CHECK");
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(ImVec2(card.min.x + 24.0f, y + 20.0f), ImVec2(card.max.x - 20.0f, card.max.y - 8.0f), true);
	text_wrapped(font.body, 12.0f, ImVec2(card.min.x + 28.0f, y + 24.0f), kMuted,
		check.empty() ? "No check has run yet." : check.c_str(), width(card) - 56.0f);
	draw->PopClipRect();
}

// Manual update check and install. Every step waits for a click.
void draw_update_card(const Rect& card)
{
	card_frame(card, "Updates", "The loader only updates when you ask it to.");

	const float button_w = width(card) - 56.0f;
	const bool checking = sdauth::StartupStatus() == sdauth::StatusWorking;
	const int update_state = sdauth::UpdateStatus();
	const bool installing = update_state == sdauth::UpdateDownloading;
	const bool ready = update_state == sdauth::UpdateReady;

	// ---------------------------------------------------------- check ----
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 72.0f));
	if (checking)
	{
		ImGui::BeginDisabled();
		secondary_button("Checking...", ImVec2(button_w, 36.0f));
		ImGui::EndDisabled();
		spinner(ImVec2(card.max.x - 46.0f, card.min.y + 90.0f), accent_color());
	}
	else if (secondary_button("Check for updates", ImVec2(button_w, 36.0f)))
	{
		sdauth::StartUpdateCheck();
	}

	// -------------------------------------------------------- result ----
	if (ready)
	{
		text(font.semibold, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 124.0f), kGreen,
			"Update installed. Restart to use it.");
		ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 148.0f));
		if (secondary_button("Restart loader", ImVec2(button_w, 36.0f)))
			sdauth::RestartLoader();
	}
	else if (installing)
	{
		const std::string message = sdauth::UpdateMessage();
		text(font.body, 12.0f, ImVec2(card.min.x + 28.0f, card.min.y + 124.0f), kMuted, message.c_str());
		progress_bar(rect(card.min.x + 28.0f, card.min.y + 146.0f, card.max.x - 28.0f, card.min.y + 154.0f),
			static_cast<float>(sdauth::UpdateProgress()) / 100.0f, accent_color());
	}
	else if (sdauth::StartupUpdateAvailable())
	{
		const std::string version = sdauth::StartupVersion();
		text(font.semibold, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 124.0f), accent_color(),
			version.empty() ? "A newer version is available." : ("Update available: " + version).c_str());
		ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 148.0f));
		if (secondary_button("Install update", ImVec2(button_w, 36.0f)))
			sdauth::StartUpdate();
	}
	else if (update_state == sdauth::UpdateFailed)
	{
		const std::string message = sdauth::UpdateMessage();
		text_wrapped(font.body, 12.0f, ImVec2(card.min.x + 28.0f, card.min.y + 124.0f), kRed,
			message.empty() ? "The update failed. Try again." : message.c_str(), width(card) - 56.0f);
	}
	else if (sdauth::StartupStatus() == sdauth::StatusOk)
	{
		text(font.body, 12.0f, ImVec2(card.min.x + 28.0f, card.min.y + 124.0f), kGreen,
			"This build is up to date.");
	}

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 198.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 212.0f), kDim, "RELEASE PAGE");
	const std::string release = sdauth::StartupReleaseUrl();
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 230.0f));
	if (release.empty())
	{
		ImGui::BeginDisabled();
		secondary_button("Open release page", ImVec2(button_w, 36.0f));
		ImGui::EndDisabled();
	}
	else if (secondary_button("Open release page", ImVec2(button_w, 36.0f)))
	{
		open_url(release);
	}
}

} // namespace

void DrawAbout()
{
	draw_identity_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_status_card(rect(706.0f, kContentTop, kContentRight, 340.0f));
	draw_update_card(rect(706.0f, 356.0f, kContentRight, kContentBottom));
}

} // namespace dash
