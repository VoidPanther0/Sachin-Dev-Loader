#include "common/dashboard_common.h"

#include <cstdio>
#include <string>
#include <vector>

// Downloads: the component files cached on this PC plus the transfer that is
// running right now. Cancel really stops the download, retry starts it again
// and Remove deletes the cached file. Loader updates happen on the startup
// loading screen only, never here.

namespace dash {

using namespace ui;

namespace {

// ------------------------------------------------------------------ active ---

void draw_active_card(const Rect& card)
{
	box(card, kCard, 14.0f, card_border());
	section_header(card, "Active transfer", icon_download);
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 64.0f);

	const Rect body = rect(card.min.x + 24.0f, card.min.y + 80.0f, card.max.x - 24.0f, card.max.y - 20.0f);

	const int status = sdauth::LaunchStatus();
	const std::string name = sdauth::LaunchName();
	const bool busy = status == sdauth::StatusWorking;

	if (name.empty() && status == sdauth::StatusIdle)
	{
		draw_empty_state(body, "Nothing downloading",
			"Files are fetched the first time you run a component, with their checksum verified.");
		return;
	}

	ImDrawList* draw = ImGui::GetWindowDrawList();

	// Name and state line.
	text(font.semibold, 16.0f, ImVec2(body.min.x, body.min.y), kText, name.empty() ? "Component" : name.c_str());

	if (busy)
	{
		const std::string message = sdauth::LaunchMessage();
		draw->PushClipRect(ImVec2(body.min.x, body.min.y + 28.0f), ImVec2(body.max.x, body.min.y + 48.0f), true);
		text(font.body, 12.0f, ImVec2(body.min.x, body.min.y + 30.0f), kMuted, message.c_str());
		draw->PopClipRect();

		const float fraction = static_cast<float>(sdauth::LaunchProgress()) / 100.0f;
		progress_bar(rect(body.min.x, body.min.y + 58.0f, body.max.x, body.min.y + 66.0f), fraction, accent_color());

		char percent[16];
		std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(fraction * 100.0f));
		text(font.caption, 11.0f, ImVec2(body.min.x, body.min.y + 76.0f), kMuted, percent);

		// Cancel stops the transfer; nothing partial is ever started.
		if (card_button("Cancel", rect(body.min.x, body.min.y + 104.0f, body.min.x + 140.0f, body.min.y + 140.0f)))
		{
			sdauth::CancelLaunch();
			toast("Cancelling the download...");
		}
		return;
	}

	if (status == sdauth::StatusOk)
	{
		draw->AddCircleFilled(ImVec2(body.min.x + 5.0f, body.min.y + 34.0f), 4.0f, color_u32(kGreen), 12);
		text(font.body, 13.0f, ImVec2(body.min.x + 18.0f, body.min.y + 26.0f), kGreen, "Finished and started.");
		text_wrapped(font.caption, 11.0f, ImVec2(body.min.x, body.min.y + 56.0f), kDim,
			"The file stays cached, so the next launch starts instantly.", width(body) - 8.0f);
		return;
	}

	if (status == sdauth::StatusError)
	{
		const std::string message = sdauth::LaunchMessage();
		draw->AddCircleFilled(ImVec2(body.min.x + 5.0f, body.min.y + 34.0f), 4.0f, color_u32(kRed), 12);
		draw->PushClipRect(ImVec2(body.min.x + 18.0f, body.min.y + 20.0f), ImVec2(body.max.x, body.min.y + 60.0f), true);
		text(font.body, 13.0f, ImVec2(body.min.x + 18.0f, body.min.y + 26.0f), kRed,
			message.empty() ? "The transfer failed." : message.c_str());
		draw->PopClipRect();

		if (!name.empty() && card_button("Try again",
			rect(body.min.x, body.min.y + 72.0f, body.min.x + 140.0f, body.min.y + 108.0f)))
		{
			if (!sdauth::LaunchComponent(name))
				toast_error("Cannot start that component right now.");
		}
	}
}

// ------------------------------------------------------------ cached files ---

void draw_files_card(const Rect& card)
{
	box(card, kCard, 14.0f, card_border());
	section_header(card, "Component files", icon_folder);

	if (text_link("##files_open", "Open folder",
		ImVec2(card.max.x - 28.0f - text_width(font.semibold, 12.0f, "Open folder"), card.min.y + 27.0f)))
		open_data_folder();

	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 64.0f);

	const std::vector<sdauth::Component> components = sdauth::Components();

	// Only components with a file on disk are listed as downloads.
	std::vector<int> cached;
	for (int i = 0; i < static_cast<int>(components.size()); ++i)
	{
		if (component_cache_size(components[static_cast<std::size_t>(i)].name) >= 0)
			cached.push_back(i);
	}

	const Rect area = rect(card.min.x + 24.0f, card.min.y + 80.0f, card.max.x - 20.0f, card.max.y - 16.0f);
	if (cached.empty())
	{
		draw_empty_state(rect(card.min.x + 24.0f, card.min.y + 80.0f, card.max.x - 24.0f, card.max.y - 16.0f),
			"No files cached", "A component appears here after its first download and check.");
		return;
	}

	// Column headers.
	text(font.caption, 10.0f, ImVec2(area.min.x + 8.0f, area.min.y), kDim, "CACHED FILE");
	text(font.caption, 10.0f, ImVec2(area.max.x - 178.0f, area.min.y), kDim, "ACTIONS");

	constexpr float row_h = 48.0f;
	const float rows_top = area.min.y + 22.0f;

	ImGui::SetCursorScreenPos(ImVec2(area.min.x, rows_top));
	ImGui::BeginChild("##file_rows", ImVec2(width(area), area.max.y - rows_top), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	const int launch_status = sdauth::LaunchStatus();
	const bool launch_busy = launch_status == sdauth::StatusWorking;

	for (int n = 0; n < static_cast<int>(cached.size()); ++n)
	{
		const int i = cached[static_cast<std::size_t>(n)];
		const sdauth::Component& component = components[static_cast<std::size_t>(i)];
		const long long size = component_cache_size(component.name);

		const float y = origin.y + static_cast<float>(n) * row_h;
		const Rect row = rect(origin.x, y, origin.x + width(area) - 8.0f, y + row_h - 8.0f);
		box(row, kCardSoft, 9.0f, &kLine);

		// Name over a version and size line; both clipped before the buttons.
		draw->PushClipRect(row.min, ImVec2(row.max.x - 178.0f, row.max.y), true);
		text(font.semibold, 13.0f, ImVec2(row.min.x + 10.0f, row.min.y + 7.0f), kText, component.name.c_str());
		char details[80];
		std::snprintf(details, sizeof(details), "v%s   %s", component.version.c_str(), format_size(size).c_str());
		text(font.caption, 10.0f, ImVec2(row.min.x + 10.0f, row.min.y + 25.0f), kMuted, details);
		draw->PopClipRect();

		// Run: the normal launch path (cache is reused, checksum re-checked).
		const bool this_busy = launch_busy && sdauth::LaunchName() == component.name;
		const Rect run = rect(row.max.x - 170.0f, row.min.y + 7.0f, row.max.x - 94.0f, row.max.y - 7.0f);
		ImGui::PushID(i);
		ImGui::SetCursorScreenPos(run.min);
		if (component_launchable(component) && !launch_busy)
		{
			if (ImGui::Button("Run", ImVec2(width(run), height(run))))
				sdauth::LaunchComponent(component.name);
			if (ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}
		else
		{
			ImGui::BeginDisabled();
			secondary_button(this_busy ? "Working..." : "Unavailable", ImVec2(width(run), height(run)));
			ImGui::EndDisabled();
		}

		// Remove: deletes the cached file after confirmation.
		const Rect remove = rect(row.max.x - 86.0f, row.min.y + 7.0f, row.max.x - 8.0f, row.max.y - 7.0f);
		if (card_button("Remove", remove))
			ImGui::OpenPopup("##confirm_clear_file");

		const std::string title = "Delete " + component.name + "?";
		const std::string body_text =
			"The cached file is deleted. It downloads again, with a checksum check, on the next launch.";
		if (confirm_modal("##confirm_clear_file", title.c_str(), body_text.c_str(), "Delete file"))
		{
			if (clear_component_cache(component.name))
				toast("Cached file deleted");
			else
				toast_error("Could not delete the file. Close the program if it is running.");
		}
		ImGui::PopID();
	}

	const int rows = static_cast<int>(cached.size());
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, static_cast<float>(rows) * row_h - 8.0f));
	ImGui::EndChild();
}

} // namespace

void DrawDownloads()
{
	draw_active_card(rect(kContentLeft, kContentTop, 700.0f, kContentBottom));
	draw_files_card(rect(716.0f, kContentTop, kContentRight, kContentBottom));
}

} // namespace dash
