#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cstdio>
#include <string>

// About: version, update state and credits.

namespace dash {

using namespace ui;

namespace {

void draw_identity_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);

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

	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.max.y - 56.0f), kDim, "MADE BY");
	text(font.semibold, 15.0f, ImVec2(card.min.x + 28.0f, card.max.y - 38.0f), kText, "Sachin Dev");
}

void draw_build_card(const Rect& card)
{
	card_frame(card, "Build", "");

	const std::string latest = sdauth::StartupLatestVersion();
	const std::string components = std::to_string(sdauth::Components().size());

	float y = card.min.y + 74.0f;
	info_row(card, y, "VERSION", app::kVersion, kText);
	y += 38.0f;
	info_row(card, y, "LATEST", latest.empty() ? app::kVersion : latest.c_str(), kText);
	y += 38.0f;
	info_row(card, y, "UPDATE", update_text(), update_color());
	y += 38.0f;
	info_row(card, y, "COMPONENTS", components.c_str(), kText);
	y += 38.0f;
	info_row(card, y, "BUILT", __DATE__, kText);

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.max.y - 58.0f));
	if (update_available())
	{
		if (ImGui::Button("Go to Downloads", ImVec2(180.0f, 36.0f)))
			state.page = PageDownloads;
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else
	{
		ImGui::BeginDisabled();
		secondary_button("Up to date", ImVec2(180.0f, 36.0f));
		ImGui::EndDisabled();
	}
}

void draw_links_card(const Rect& card)
{
	card_frame(card, "Links", "");

	const std::string release = sdauth::StartupReleaseUrl();
	const float button_w = width(card) - 56.0f;

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 70.0f));
	if (release.empty())
	{
		ImGui::BeginDisabled();
		secondary_button("Release page", ImVec2(button_w, 36.0f));
		ImGui::EndDisabled();
	}
	else if (secondary_button("Release page", ImVec2(button_w, 36.0f)))
	{
		open_url(release);
	}

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 114.0f));
	if (secondary_button("Copy diagnostics", ImVec2(button_w, 36.0f)))
	{
		ImGui::SetClipboardText(diagnostics_text().c_str());
		shared.diagnostics_time = ImGui::GetTime();
	}
	if (ImGui::GetTime() - shared.diagnostics_time < 2.0)
		text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 158.0f), kGreen, "Copied to clipboard.");
}

} // namespace

void DrawAbout()
{
	draw_page_header("About", "Version and credits.");

	draw_identity_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_build_card(rect(706.0f, kContentTop, kContentRight, 452.0f));
	draw_links_card(rect(706.0f, 468.0f, kContentRight, kContentBottom));
}

} // namespace dash
