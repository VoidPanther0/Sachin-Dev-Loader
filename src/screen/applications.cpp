#include "dashboard_common.h"

#include <cstdio>
#include <string>
#include <vector>

// Applications: components released by the server for this account, with
// search, details and a launch button (download, SHA-256 check, start).

namespace dash {

using namespace ui;

namespace {

void draw_component_details(const Rect& panel, const sdauth::Component& component)
{
	box(panel, kCard, 14.0f, &kBorder);

	const Rect banner = rect(panel.min.x + 1.0f, panel.min.y + 1.0f, panel.max.x - 1.0f, panel.min.y + 96.0f);
	gradient_box(banner, fade(accent_color(), 0.30f), fade(kPurple, 0.30f), 13.0f);

	letter_badge(rect(panel.min.x + 24.0f, panel.min.y + 64.0f, panel.min.x + 88.0f, panel.min.y + 128.0f),
		component.name, 28.0f);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.min.y), ImVec2(panel.max.x - 24.0f, panel.max.y), true);
	text(font.semibold, 20.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 142.0f), kText, component.name.c_str());
	draw->PopClipRect();

	char version[48];
	std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
	const float pill_w = text_width(font.caption, 10.0f, version) + 32.0f;
	pill(rect(panel.min.x + 24.0f, panel.min.y + 174.0f, panel.min.x + 24.0f + pill_w, panel.min.y + 196.0f),
		version, kGreen);
	const std::string status = status_label(component.status);
	const float status_w = text_width(font.caption, 10.0f, status.c_str()) + 32.0f;
	pill(rect(panel.min.x + 32.0f + pill_w, panel.min.y + 174.0f, panel.min.x + 32.0f + pill_w + status_w,
		panel.min.y + 196.0f), status.c_str(), status_color(component.status));

	divider(panel.min.x + 24.0f, panel.max.x - 24.0f, panel.min.y + 214.0f);

	text(font.caption, 10.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 230.0f), kDim, "ABOUT");
	text_wrapped(font.body, 13.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 248.0f), kMuted,
		component.description.empty() ? "No description provided by the server." : component.description.c_str(),
		width(panel) - 48.0f);

	// Launch area: result line, download bar and the button.
	const int launch_status = sdauth::LaunchStatus();
	const bool launch_here = launch_status != sdauth::StatusIdle && sdauth::LaunchName() == component.name;
	const bool launch_busy = launch_status == sdauth::StatusWorking;

	if (launch_here)
	{
		const ImVec4 color = launch_status == sdauth::StatusError ? kRed
			: (launch_status == sdauth::StatusOk ? kGreen : kMuted);
		const std::string message = sdauth::LaunchMessage();
		draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.max.y - 104.0f),
			ImVec2(panel.max.x - 24.0f, panel.max.y - 84.0f), true);
		text(font.caption, 11.0f, ImVec2(panel.min.x + 24.0f, panel.max.y - 102.0f), color, message.c_str());
		draw->PopClipRect();

		if (launch_busy)
			progress_bar(rect(panel.min.x + 24.0f, panel.max.y - 80.0f, panel.max.x - 24.0f, panel.max.y - 74.0f),
				static_cast<float>(sdauth::LaunchProgress()) / 100.0f, accent_color());
	}

	const char* button_label = "Launch";
	if (component.download_url.empty())
		button_label = "Not launchable";
	else if (component.status == "updating")
		button_label = "Updating";
	else if (component.status == "offline")
		button_label = "Offline";
	else if (launch_here && launch_busy)
		button_label = "Working...";

	const bool can_launch = component_launchable(component) && !launch_busy;
	ImGui::SetCursorScreenPos(ImVec2(panel.min.x + 24.0f, panel.max.y - 62.0f));
	const ImVec2 button_size(width(panel) - 48.0f, 40.0f);
	if (can_launch)
	{
		if (ImGui::Button("Launch##component", button_size))
			sdauth::LaunchComponent(component.name);
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else
	{
		ImGui::BeginDisabled();
		secondary_button(button_label, button_size);
		ImGui::EndDisabled();
	}
}

void draw_component_grid(const Rect& list, const std::vector<sdauth::Component>& components,
	const std::vector<int>& visible)
{
	const Rect area = rect(list.min.x + 24.0f, list.min.y + 88.0f, list.max.x - 16.0f, list.max.y - 16.0f);
	if (visible.empty())
	{
		draw_empty_state(rect(area.min.x, area.min.y, area.max.x - 8.0f, area.max.y - 8.0f),
			"No matches", "Try a different search.");
		return;
	}

	constexpr float gap = 12.0f;
	constexpr float tile_h = 84.0f;
	const float tile_w = (width(area) - 8.0f - gap) * 0.5f;

	ImGui::SetCursorScreenPos(area.min);
	ImGui::BeginChild("##apps_grid", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	for (int n = 0; n < static_cast<int>(visible.size()); ++n)
	{
		const int index = visible[static_cast<std::size_t>(n)];
		const sdauth::Component& component = components[static_cast<std::size_t>(index)];
		const float x = origin.x + static_cast<float>(n % 2) * (tile_w + gap);
		const float y = origin.y + static_cast<float>(n / 2) * (tile_h + gap);
		const Rect tile = rect(x, y, x + tile_w, y + tile_h);

		ImGui::PushID(index);
		ImGui::SetCursorScreenPos(tile.min);
		if (ImGui::InvisibleButton("##tile", ImVec2(width(tile), height(tile))))
			shared.selected = index;
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		const bool active = index == shared.selected;
		const ImVec4 border = active ? fade(accent_color(), 0.55f) : (hovered ? kLineBright : kLine);
		box(tile, active ? fade(accent_color(), 0.10f) : (hovered ? lift(kCardSoft, 0.02f) : kCardSoft),
			10.0f, &border);

		letter_badge(rect(tile.min.x + 14.0f, tile.min.y + 14.0f, tile.min.x + 56.0f, tile.min.y + 56.0f),
			component.name, 18.0f);

		draw->PushClipRect(tile.min, ImVec2(tile.max.x - 14.0f, tile.max.y), true);
		text(font.semibold, 14.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 14.0f), kText, component.name.c_str());
		char version[48];
		std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
		text(font.caption, 10.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 35.0f), accent_color(), version);
		text(font.caption, 11.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 54.0f), kMuted,
			component.description.empty() ? "No description" : component.description.c_str());
		draw->PopClipRect();

		draw->AddCircleFilled(ImVec2(tile.max.x - 18.0f, tile.min.y + 21.0f), 3.5f,
			color_u32(status_color(component.status)), 12);
		if (hovered && ImGui::IsMouseHoveringRect(ImVec2(tile.max.x - 30.0f, tile.min.y + 10.0f),
			ImVec2(tile.max.x - 6.0f, tile.min.y + 32.0f)))
			ImGui::SetTooltip("%s", status_label(component.status).c_str());
	}

	const int rows = (static_cast<int>(visible.size()) + 1) / 2;
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, static_cast<float>(rows) * (tile_h + gap) - gap));
	ImGui::EndChild();
}

} // namespace

void DrawApplications()
{
	draw_page_header("Applications", "Apps, games and tools released for this account.");

	const std::vector<sdauth::Component> components = sdauth::Components();
	if (components.empty())
	{
		const Rect card = rect(kContentLeft, kContentTop, kContentRight, kContentBottom);
		box(card, kCard, 14.0f, &kBorder);
		draw_empty_state(rect(card.min.x + 32.0f, card.min.y + 32.0f, card.max.x - 32.0f, card.max.y - 32.0f),
			"No components yet", "Content released by the server will appear here.");
		return;
	}

	if (shared.selected < 0 || shared.selected >= static_cast<int>(components.size()))
		shared.selected = 0;

	const Rect list = rect(kContentLeft, kContentTop, 812.0f, kContentBottom);
	box(list, kCard, 14.0f, &kBorder);

	std::vector<int> visible;
	for (int i = 0; i < static_cast<int>(components.size()); ++i)
	{
		const sdauth::Component& c = components[static_cast<std::size_t>(i)];
		if (contains_ci(c.name, shared.search) || contains_ci(c.description, shared.search))
			visible.push_back(i);
	}

	text(font.semibold, 17.0f, ImVec2(list.min.x + 24.0f, list.min.y + 20.0f), kText, "Components");
	char count[64];
	if (shared.search[0] != '\0')
		std::snprintf(count, sizeof(count), "%d of %d shown", static_cast<int>(visible.size()),
			static_cast<int>(components.size()));
	else
		std::snprintf(count, sizeof(count), "%d component%s", static_cast<int>(components.size()),
			components.size() == 1 ? "" : "s");
	text(font.caption, 11.0f, ImVec2(list.min.x + 24.0f, list.min.y + 46.0f), kMuted, count);

	draw_search_box(rect(list.max.x - 264.0f, list.min.y + 20.0f, list.max.x - 24.0f, list.min.y + 56.0f),
		"Search components");
	divider(list.min.x + 24.0f, list.max.x - 24.0f, list.min.y + 72.0f);

	draw_component_grid(list, components, visible);

	draw_component_details(rect(828.0f, kContentTop, kContentRight, kContentBottom),
		components[static_cast<std::size_t>(shared.selected)]);
}

} // namespace dash
