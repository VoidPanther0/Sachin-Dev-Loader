#include "common/dashboard_common.h"

#include <commdlg.h>

#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "comdlg32.lib")

// Applications: the components released by the server for this account plus
// the .exe files the user added himself. Search, details, refresh, launch
// with an optional confirmation, and local-app management with persistence.

namespace dash {

using namespace ui;

namespace {

// One tile in the grid: a server component or a local program.
struct Entry
{
	bool local = false;
	int index = 0; // component index, or local app index when local
};

// Refresh result is toasted once when it changes.
int g_refresh_seen = sdauth::StatusIdle;

void tick_refresh_toast()
{
	const int status = sdauth::RefreshStatus();
	if (status == g_refresh_seen)
		return;
	g_refresh_seen = status;
	if (status == sdauth::StatusOk)
		toast(sdauth::RefreshMessage().c_str());
	else if (status == sdauth::StatusError)
		toast_error(sdauth::RefreshMessage().c_str());
}

// Native file picker, .exe only (the validation still re-checks the choice).
std::wstring pick_exe()
{
	wchar_t buffer[MAX_PATH] = {};
	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = g_hwnd;
	dialog.lpstrFilter = L"Programs (*.exe)\0*.exe\0All files\0*.*\0\0";
	dialog.lpstrFile = buffer;
	dialog.nMaxFile = MAX_PATH;
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetOpenFileNameW(&dialog) ? std::wstring(buffer) : std::wstring();
}

// Starts a local app, honoring the confirmation option. Toasts the real result.
void launch_local(int index)
{
	std::string error;
	if (launch_local_app(index, error))
		toast(("Started " + local_apps()[static_cast<std::size_t>(index)].name).c_str());
	else
		toast_error(error.c_str());
}

// Starts a server component, honoring the confirmation option.
void launch_component_now(const std::string& name)
{
	if (!sdauth::LaunchComponent(name))
		toast_error("The component cannot start right now. Try Refresh.");
}

// ---------------------------------------------------------- details: server ---

void draw_component_details(const Rect& panel, const sdauth::Component& component)
{
	box(panel, kCard, 14.0f, card_border());

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
	// Clipped so a very long server description can never reach the FILE block.
	draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.min.y + 246.0f),
		ImVec2(panel.max.x - 24.0f, panel.max.y - 148.0f), true);
	text_wrapped(font.body, 13.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 248.0f), kMuted,
		component.description.empty() ? "No description provided by the server." : component.description.c_str(),
		width(panel) - 48.0f);
	draw->PopClipRect();

	// Download state of the cached file.
	const long long cached = component_cache_size(component.name);
	const std::string cache_text = cached >= 0 ? ("On this PC: " + format_size(cached)) : "Not downloaded yet";
	text(font.caption, 10.0f, ImVec2(panel.min.x + 24.0f, panel.max.y - 142.0f), kDim, "FILE");
	text(font.body, 12.0f, ImVec2(panel.min.x + 24.0f, panel.max.y - 124.0f),
		cached >= 0 ? kGreen : kMuted, cache_text.c_str());

	// Launch area: result line, progress bar and the button.
	const int launch_status = sdauth::LaunchStatus();
	const bool launch_here = launch_status != sdauth::StatusIdle && sdauth::LaunchName() == component.name;
	const bool launch_busy = launch_status == sdauth::StatusWorking;

	if (launch_here && launch_busy)
	{
		const std::string message = sdauth::LaunchMessage();
		draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.max.y - 104.0f),
			ImVec2(panel.max.x - 24.0f, panel.max.y - 84.0f), true);
		text(font.caption, 11.0f, ImVec2(panel.min.x + 24.0f, panel.max.y - 102.0f), kMuted, message.c_str());
		draw->PopClipRect();

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
	const ImVec2 button_size(width(panel) - 48.0f, 40.0f);
	ImGui::SetCursorScreenPos(ImVec2(panel.min.x + 24.0f, panel.max.y - 62.0f));
	if (can_launch)
	{
		if (ImGui::Button("Launch##component", button_size))
		{
			if (shared.confirm_launch)
				ImGui::OpenPopup("##confirm_component");
			else
				launch_component_now(component.name);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else
	{
		ImGui::BeginDisabled();
		secondary_button(button_label, button_size);
		ImGui::EndDisabled();
	}

	const std::string confirm_title = "Launch " + component.name + "?";
	const std::string confirm_body =
		"The file is downloaded once, checked against its SHA-256 checksum and then started.";
	if (confirm_modal("##confirm_component", confirm_title.c_str(), confirm_body.c_str(), "Launch"))
		launch_component_now(component.name);
}

// ----------------------------------------------------------- details: local ---

void draw_local_details(const Rect& panel, int index)
{
	const std::vector<LocalApp>& apps = local_apps();
	if (index < 0 || index >= static_cast<int>(apps.size()))
		return;
	const LocalApp& app = apps[static_cast<std::size_t>(index)];

	box(panel, kCard, 14.0f, card_border());

	letter_badge(rect(panel.min.x + 24.0f, panel.min.y + 24.0f, panel.min.x + 88.0f, panel.min.y + 88.0f),
		app.name, 28.0f);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(ImVec2(panel.min.x + 104.0f, panel.min.y), ImVec2(panel.max.x - 24.0f, panel.min.y + 90.0f), true);
	text(font.semibold, 19.0f, ImVec2(panel.min.x + 104.0f, panel.min.y + 28.0f), kText, app.name.c_str());
	text(font.caption, 11.0f, ImVec2(panel.min.x + 104.0f, panel.min.y + 56.0f), kMuted, "Local program");
	draw->PopClipRect();

	const bool running = local_app_running(index);
	status_badge(rect(panel.min.x + 24.0f, panel.min.y + 104.0f, panel.min.x + 24.0f + 118.0f, panel.min.y + 128.0f),
		running ? "RUNNING" : "NOT RUNNING", running ? kGreen : kMuted);

	divider(panel.min.x + 24.0f, panel.max.x - 24.0f, panel.min.y + 148.0f);

	text(font.caption, 10.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 164.0f), kDim, "FILE");
	draw->PushClipRect(ImVec2(panel.min.x + 24.0f, panel.min.y + 180.0f),
		ImVec2(panel.max.x - 24.0f, panel.min.y + 240.0f), true);
	text_wrapped(font.body, 12.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 182.0f), kMuted,
		app.path.c_str(), width(panel) - 48.0f);
	draw->PopClipRect();

	text_wrapped(font.caption, 11.0f, ImVec2(panel.min.x + 24.0f, panel.min.y + 246.0f), kDim,
		"Started from its own folder. The loader never edits or replaces this file.",
		width(panel) - 48.0f);

	// Launch and remove, both behind confirmation when the option is on.
	const ImVec2 button_size(width(panel) - 48.0f, 40.0f);
	ImGui::SetCursorScreenPos(ImVec2(panel.min.x + 24.0f, panel.max.y - 110.0f));
	if (ImGui::Button("Launch##local", button_size))
	{
		if (shared.confirm_launch)
			ImGui::OpenPopup("##confirm_local");
		else
			launch_local(index);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	ImGui::SetCursorScreenPos(ImVec2(panel.min.x + 24.0f, panel.max.y - 62.0f));
	if (secondary_button("Remove from list", button_size))
		ImGui::OpenPopup("##confirm_remove");

	const std::string launch_title = "Launch " + app.name + "?";
	const std::string launch_body = "This starts the configured program from its own folder.";
	if (confirm_modal("##confirm_local", launch_title.c_str(), launch_body.c_str(), "Launch"))
		launch_local(index);

	const std::string remove_title = "Remove " + app.name + "?";
	const std::string remove_body =
		"Only the entry in the loader is removed. The program itself stays on your PC.";
	if (confirm_modal("##confirm_remove", remove_title.c_str(), remove_body.c_str(), "Remove"))
	{
		remove_local_app(index);
		shared.selected_local = -1;
		toast("Removed from the list");
	}
}

// ------------------------------------------------------------------- grid ----

void draw_tile(const Rect& tile, const char* name, const char* subtitle, const char* tag,
	const ImVec4& dot, bool active, bool hovered)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec4 border = active ? fade(accent_color(), 0.55f) : (hovered ? kLineBright : kLine);
	box(tile, active ? fade(accent_color(), 0.10f) : (hovered ? lift(kCardSoft, 0.02f) : kCardSoft),
		10.0f, &border);

	letter_badge(rect(tile.min.x + 14.0f, tile.min.y + 14.0f, tile.min.x + 56.0f, tile.min.y + 56.0f), name, 18.0f);

	draw->PushClipRect(tile.min, ImVec2(tile.max.x - 14.0f, tile.max.y), true);
	text(font.semibold, 14.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 14.0f), kText, name);
	text(font.caption, 10.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 35.0f), accent_color(), subtitle);
	text(font.caption, 11.0f, ImVec2(tile.min.x + 70.0f, tile.min.y + 54.0f), kMuted, tag);
	draw->PopClipRect();

	draw->AddCircleFilled(ImVec2(tile.max.x - 18.0f, tile.min.y + 21.0f), 3.5f, color_u32(dot), 12);
}

void draw_grid(const Rect& list, const std::vector<Entry>& entries)
{
	const Rect area = rect(list.min.x + 24.0f, list.min.y + 124.0f, list.max.x - 16.0f, list.max.y - 16.0f);

	if (entries.empty())
	{
		const std::vector<sdauth::Component> components = sdauth::Components();
		const bool anything = !components.empty() || !local_apps().empty();
		draw_empty_state(rect(area.min.x, area.min.y, area.max.x - 8.0f, area.max.y - 8.0f),
			anything ? "No matches" : "Nothing here yet",
			anything ? "Try a different search."
				: "Add a program below, or press Refresh once the server released apps for you.");
		return;
	}

	constexpr float gap = 12.0f;
	constexpr float tile_h = 84.0f;
	const float tile_w = (width(area) - 8.0f - gap) * 0.5f;

	ImGui::SetCursorScreenPos(area.min);
	ImGui::BeginChild("##apps_grid", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	const std::vector<sdauth::Component> components = sdauth::Components();
	const std::vector<LocalApp>& apps = local_apps();

	for (int n = 0; n < static_cast<int>(entries.size()); ++n)
	{
		const Entry& entry = entries[static_cast<std::size_t>(n)];
		const float x = origin.x + static_cast<float>(n % 2) * (tile_w + gap);
		const float y = origin.y + static_cast<float>(n / 2) * (tile_h + gap);
		const Rect tile = rect(x, y, x + tile_w, y + tile_h);

		ImGui::PushID(n);
		ImGui::SetCursorScreenPos(tile.min);
		const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(width(tile), height(tile)));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		bool active = false;
		if (entry.local)
		{
			const LocalApp& app = apps[static_cast<std::size_t>(entry.index)];
			active = shared.selected_local == entry.index;
			const bool running = local_app_running(entry.index);
			draw_tile(tile, app.name.c_str(), "LOCAL PROGRAM",
				running ? "Running now" : "Ready to start",
				running ? kGreen : kNavText, active, hovered);
			if (clicked)
			{
				shared.selected_local = entry.index;
				shared.selected = -1;
			}
		}
		else
		{
			const sdauth::Component& component = components[static_cast<std::size_t>(entry.index)];
			active = shared.selected == entry.index && shared.selected_local < 0;
			char version[48];
			std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
			draw_tile(tile, component.name.c_str(), version,
				component.description.empty() ? "From the server" : component.description.c_str(),
				status_color(component.status), active, hovered);
			if (clicked)
			{
				shared.selected = entry.index;
				shared.selected_local = -1;
			}
		}
	}

	const int rows = (static_cast<int>(entries.size()) + 1) / 2;
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, static_cast<float>(rows) * (tile_h + gap) - gap));
	ImGui::EndChild();
}

} // namespace

void DrawApplications()
{
	tick_refresh_toast();

	const std::vector<sdauth::Component> components = sdauth::Components();

	// ------------------------------------------------------------ list card --
	const Rect list = rect(kContentLeft, kContentTop, 812.0f, kContentBottom);
	box(list, kCard, 14.0f, card_border());

	text(font.semibold, 17.0f, ImVec2(list.min.x + 24.0f, list.min.y + 20.0f), kText, "Applications");

	char count[80];
	if (shared.search[0] != '\0')
		std::snprintf(count, sizeof(count), "Showing matches of %d server + %d local",
			static_cast<int>(components.size()), static_cast<int>(local_apps().size()));
	else
		std::snprintf(count, sizeof(count), "%d server component%s, %d local program%s",
			static_cast<int>(components.size()), components.size() == 1 ? "" : "s",
			static_cast<int>(local_apps().size()), local_apps().size() == 1 ? "" : "s");
	text(font.caption, 11.0f, ImVec2(list.min.x + 24.0f, list.min.y + 46.0f), kMuted, count);

	// Refresh: re-runs the signed-in bootstrap on a worker thread.
	const bool refreshing = sdauth::RefreshStatus() == sdauth::StatusWorking;
	ImGui::SetCursorScreenPos(ImVec2(list.max.x - 306.0f, list.min.y + 16.0f));
	if (refreshing)
	{
		ImGui::BeginDisabled();
		secondary_button("Refreshing...", ImVec2(110.0f, 36.0f));
		ImGui::EndDisabled();
	}
	else if (secondary_button("Refresh", ImVec2(110.0f, 36.0f)))
	{
		g_refresh_seen = sdauth::StatusWorking;
		sdauth::StartComponentRefresh();
	}

	// Add application: native picker, then validation and persistence.
	ImGui::SetCursorScreenPos(ImVec2(list.max.x - 184.0f, list.min.y + 16.0f));
	if (secondary_button("Add application", ImVec2(160.0f, 36.0f)))
	{
		const std::wstring path = pick_exe();
		if (!path.empty())
		{
			std::string error;
			if (add_local_app(path, error))
			{
				shared.selected_local = static_cast<int>(local_apps().size()) - 1;
				shared.selected = -1;
				toast("Program added");
			}
			else
			{
				toast_error(error.c_str());
			}
		}
	}

	draw_search_box(rect(list.min.x + 24.0f, list.min.y + 64.0f, list.max.x - 24.0f, list.min.y + 96.0f),
		"Search applications");
	divider(list.min.x + 24.0f, list.max.x - 24.0f, list.min.y + 110.0f);

	// ---------------------------------------------------------------- grid ----
	std::vector<Entry> entries;
	for (int i = 0; i < static_cast<int>(components.size()); ++i)
	{
		const sdauth::Component& c = components[static_cast<std::size_t>(i)];
		if (contains_ci(c.name, shared.search) || contains_ci(c.description, shared.search))
		{
			Entry entry;
			entry.local = false;
			entry.index = i;
			entries.push_back(entry);
		}
	}
	for (int i = 0; i < static_cast<int>(local_apps().size()); ++i)
	{
		const LocalApp& app = local_apps()[static_cast<std::size_t>(i)];
		if (contains_ci(app.name, shared.search) || contains_ci(app.path, shared.search))
		{
			Entry entry;
			entry.local = true;
			entry.index = i;
			entries.push_back(entry);
		}
	}

	draw_grid(list, entries);

	// ------------------------------------------------------------- details ----
	const Rect panel = rect(828.0f, kContentTop, kContentRight, kContentBottom);
	if (shared.selected_local >= 0)
	{
		draw_local_details(panel, shared.selected_local);
	}
	else if (!components.empty())
	{
		if (shared.selected < 0 || shared.selected >= static_cast<int>(components.size()))
			shared.selected = 0;
		draw_component_details(panel, components[static_cast<std::size_t>(shared.selected)]);
	}
	else
	{
		box(panel, kCard, 14.0f, card_border());
		draw_empty_state(rect(panel.min.x + 20.0f, panel.min.y + 20.0f, panel.max.x - 20.0f, panel.max.y - 20.0f),
			"Nothing selected",
			local_apps().empty()
				? "Add a program or press Refresh to load your apps."
				: "Pick a program from the grid to see its details.");
	}
}

} // namespace dash
