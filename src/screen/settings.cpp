#include "dashboard_common.h"

// Settings: interface options, help tools and what the loader stores.

namespace dash {

using namespace ui;

namespace {

void draw_interface_card(const Rect& card)
{
	card_frame(card, "Interface", "Changes save automatically.");

	bool changed = false;
	if (toggle_row("##set_fps", rect(card.min.x + 14.0f, card.min.y + 84.0f, card.max.x - 14.0f, card.min.y + 134.0f),
		"Show FPS counter", "Frame rate in the footer.", &shared.show_fps))
		changed = true;
	if (toggle_row("##set_glow", rect(card.min.x + 14.0f, card.min.y + 142.0f, card.max.x - 14.0f, card.min.y + 192.0f),
		"Background glow", "Soft color glow behind the pages.", &shared.background_glow))
		changed = true;

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 212.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 228.0f), kDim, "ACCENT COLOR");
	text(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 248.0f), kMuted, "Change it on the Theme page.");
	if (text_link("##set_theme", "Open Theme", ImVec2(card.min.x + 28.0f, card.min.y + 276.0f)))
		state.page = PageTheme;

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.max.y - 58.0f));
	if (secondary_button("Reset interface options", ImVec2(200.0f, 34.0f)))
		reset_preferences();
	else if (changed)
		save_preferences();
}

void draw_help_card(const Rect& card)
{
	card_frame(card, "Help", "Copy a report for support or open the loader files.");

	const float button_w = width(card) - 56.0f;

	const bool copied = ImGui::GetTime() - shared.diagnostics_time < 2.0;
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 84.0f));
	if (secondary_button(copied ? "Copied to clipboard" : "Copy diagnostics", ImVec2(button_w, 36.0f)))
	{
		ImGui::SetClipboardText(diagnostics_text().c_str());
		shared.diagnostics_time = ImGui::GetTime();
	}

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 128.0f));
	if (secondary_button("Open data folder", ImVec2(button_w, 36.0f)))
		open_data_folder();

	const std::string release = sdauth::StartupReleaseUrl();
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 172.0f));
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

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 228.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 244.0f), kDim, "PRIVACY");
	text_wrapped(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 264.0f), kMuted,
		"Passwords and tokens never touch disk. Only your accent color, interface options and the remembered "
		"username are saved.", width(card) - 56.0f);
}

} // namespace

void DrawSettings()
{
	draw_page_header("Settings", "Interface options and help.");

	draw_interface_card(rect(kContentLeft, kContentTop, 690.0f, kContentBottom));
	draw_help_card(rect(706.0f, kContentTop, kContentRight, kContentBottom));
}

} // namespace dash
