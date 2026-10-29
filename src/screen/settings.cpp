#include "common/dashboard_common.h"

// Settings, grouped in sections. Everything in the "Working options" sections
// really changes the loader. Anything not implemented in this build sits in
// its own section with a NOT AVAILABLE badge, so nothing here pretends to work.

namespace dash {

using namespace ui;

namespace {

// ---------------------------------------------------------------- interface ---

void draw_interface_card(const Rect& card)
{
	card_frame(card, "Interface", "Working options. Changes save automatically.");

	bool changed = false;
	if (toggle_row("##set_fps", rect(card.min.x + 14.0f, card.min.y + 76.0f, card.max.x - 14.0f, card.min.y + 130.0f),
		"Show FPS counter", "Frame rate in the footer.", &shared.show_fps))
		changed = true;
	if (toggle_row("##set_toasts", rect(card.min.x + 14.0f, card.min.y + 132.0f, card.max.x - 14.0f, card.min.y + 186.0f),
		"Notifications", "Small pop-ups when something finishes or fails.", &shared.toasts))
		changed = true;
	if (toggle_row("##set_glow", rect(card.min.x + 14.0f, card.min.y + 188.0f, card.max.x - 14.0f, card.min.y + 242.0f),
		"Background glow", "Soft color shapes behind the pages.", &shared.background_glow))
		changed = true;

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 262.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 278.0f), kDim, "THEME AND ACCENT");
	text(font.body, 13.0f, ImVec2(card.min.x + 28.0f, card.min.y + 298.0f), kMuted,
		"Theme, accent color and effects live on the Theme page.");
	if (text_link("##set_theme", "Open Theme", ImVec2(card.min.x + 28.0f, card.min.y + 326.0f)))
		state.page = PageTheme;

	if (changed)
		save_preferences();
}

// -------------------------------------------------------------- launching ----

void draw_launch_card(const Rect& card)
{
	card_frame(card, "Launching", "Working options for starting apps and components.");

	bool changed = false;
	if (toggle_row("##set_confirm", rect(card.min.x + 14.0f, card.min.y + 72.0f, card.max.x - 14.0f, card.min.y + 126.0f),
		"Confirm before launching", "Ask once before a program or component starts.", &shared.confirm_launch))
		changed = true;
	if (toggle_row("##set_dupes", rect(card.min.x + 14.0f, card.min.y + 128.0f, card.max.x - 14.0f, card.min.y + 182.0f),
		"Prevent duplicate launches", "Refuse to start a program that is already running.", &shared.stop_duplicates))
		changed = true;

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 194.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 208.0f), kDim, "HOW FILES ARE STARTED");
	text_wrapped(font.body, 12.0f, ImVec2(card.min.x + 28.0f, card.min.y + 226.0f), kMuted,
		"Components come from the server and are SHA-256 checked. Local apps are only "
		"the .exe files you added yourself.",
		width(card) - 56.0f);

	if (changed)
		save_preferences();
}

// ------------------------------------------------------------ not available ---

void draw_missing_card(const Rect& card)
{
	card_frame(card, "Not available yet", "Reserved settings. Nothing in this section works in this build.");

	placeholder_row(rect(card.min.x + 14.0f, card.min.y + 76.0f, card.max.x - 14.0f, card.min.y + 130.0f),
		"Language", "Interface language picker");
	placeholder_row(rect(card.min.x + 14.0f, card.min.y + 138.0f, card.max.x - 14.0f, card.min.y + 192.0f),
		"Start with Windows", "Open the loader after sign-in");
	placeholder_row(rect(card.min.x + 14.0f, card.min.y + 200.0f, card.max.x - 14.0f, card.min.y + 254.0f),
		"Minimize to tray", "Keep the loader in the background");
}

// -------------------------------------------------------------------- help ---

void draw_help_card(const Rect& card)
{
	card_frame(card, "Help and data", "Copy a report for support or open the loader files.");

	const float button_w = width(card) - 56.0f;

	const bool copied = ImGui::GetTime() - shared.diagnostics_time < 2.0;
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 68.0f));
	if (secondary_button(copied ? "Copied to clipboard" : "Copy diagnostics", ImVec2(button_w, 36.0f)))
	{
		ImGui::SetClipboardText(diagnostics_text().c_str());
		shared.diagnostics_time = ImGui::GetTime();
	}

	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 104.0f));
	if (secondary_button("Open data folder", ImVec2(button_w, 36.0f)))
		open_data_folder();

	const std::string release = sdauth::StartupReleaseUrl();
	ImGui::SetCursorScreenPos(ImVec2(card.min.x + 28.0f, card.min.y + 140.0f));
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

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 192.0f);
	text(font.caption, 10.0f, ImVec2(card.min.x + 28.0f, card.min.y + 206.0f), kDim, "PRIVACY");
	text_wrapped(font.body, 12.0f, ImVec2(card.min.x + 28.0f, card.min.y + 222.0f), kMuted,
		"Passwords and tokens never touch disk. Only theme, options, added apps and "
		"your username are saved locally.",
		width(card) - 56.0f);
}

} // namespace

void DrawSettings()
{
	draw_interface_card(rect(kContentLeft, kContentTop, 690.0f, 372.0f));
	draw_launch_card(rect(kContentLeft, 388.0f, 690.0f, kContentBottom));

	draw_missing_card(rect(706.0f, kContentTop, kContentRight, 372.0f));
	draw_help_card(rect(706.0f, 388.0f, kContentRight, kContentBottom));
}

} // namespace dash
