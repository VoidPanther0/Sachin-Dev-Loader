#include "dashboard_common.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

// Downloads: the loader update (installed version, latest version from
// loader_config, download and restart) and the component files cached on this
// PC.

namespace dash {

using namespace ui;

namespace {

constexpr double kRestartCountdownSeconds = 3.0;

bool restart_failed = false;

// Same naming rule as the launcher: letters, digits, dash and underscore.
std::wstring component_file(const std::string& name)
{
	wchar_t appdata[MAX_PATH] = {};
	const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return std::wstring();

	std::wstring file;
	for (const char c : name)
	{
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')
			file += static_cast<wchar_t>(c);
		else if (c == ' ')
			file += L'_';
	}
	if (file.empty())
		file = L"component";

	return std::wstring(appdata) + L"\\SachinDevLoader\\components\\" + file + L".exe";
}

// Size in bytes of the cached file, or -1 when it is not downloaded.
long long cached_size(const std::string& name)
{
	const std::wstring path = component_file(name);
	if (path.empty())
		return -1;

	WIN32_FILE_ATTRIBUTE_DATA info = {};
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info))
		return -1;

	return (static_cast<long long>(info.nFileSizeHigh) << 32) | static_cast<long long>(info.nFileSizeLow);
}

std::string format_size(long long bytes)
{
	char value[32];
	if (bytes >= 1024LL * 1024LL)
		std::snprintf(value, sizeof(value), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	else
		std::snprintf(value, sizeof(value), "%lld KB", (bytes + 1023LL) / 1024LL);
	return value;
}

// ----------------------------------------------------------- update card ----

void draw_update_card(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 22.0f), kText, "Loader update");

	const int status = sdauth::UpdateStatus();
	const bool available = update_available();
	const bool downloading = status == sdauth::UpdateDownloading;
	const bool ready = status == sdauth::UpdateReady;
	const bool failed = status == sdauth::UpdateFailed;
	const double now = ImGui::GetTime();

	const std::string latest_text = sdauth::StartupLatestVersion();
	const std::string latest = latest_text.empty() ? std::string(app::kVersion) : latest_text;

	// State pill next to the title.
	const char* pill_text = "UP TO DATE";
	ImVec4 pill_color = kGreen;
	if (ready) { pill_text = "READY TO RESTART"; pill_color = kGreen; }
	else if (downloading) { pill_text = "DOWNLOADING"; pill_color = accent_color(); }
	else if (failed) { pill_text = "FAILED"; pill_color = kRed; }
	else if (available) { pill_text = "UPDATE AVAILABLE"; pill_color = accent_color(); }
	const float pill_w = text_width(font.caption, 10.0f, pill_text) + 36.0f;
	pill(rect(card.min.x + 150.0f, card.min.y + 22.0f, card.min.x + 150.0f + pill_w, card.min.y + 44.0f),
		pill_text, pill_color);

	char installed_line[64];
	std::snprintf(installed_line, sizeof(installed_line), "Installed   v%s", app::kVersion);
	text(font.body, 14.0f, ImVec2(card.min.x + 28.0f, card.min.y + 62.0f), kText, installed_line);

	char latest_line[64];
	std::snprintf(latest_line, sizeof(latest_line), "Latest         v%s", latest.c_str());
	text(font.body, 14.0f, ImVec2(card.min.x + 28.0f, card.min.y + 86.0f), available ? accent_color() : kText,
		latest_line);

	// Message line.
	std::string message;
	ImVec4 message_color = kMuted;
	if (restart_failed)
	{
		message = "The restart did not start. Use the button.";
		message_color = kRed;
	}
	else if (downloading || ready || failed)
	{
		message = sdauth::UpdateMessage();
		if (failed)
			message_color = kRed;
	}
	else if (available)
	{
		message = "A new build is ready. It is verified with SHA-256 before it is installed.";
	}
	else if (latest != app::kVersion)
	{
		message = "A newer version is set, but its release is not published yet.";
	}
	else
	{
		message = "You are on the latest version.";
	}
	text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 118.0f), message_color, message.c_str());

	const Rect bar = rect(card.min.x + 28.0f, card.max.y - 28.0f, card.max.x - 240.0f, card.max.y - 22.0f);
	if (downloading || ready)
		progress_bar(bar, static_cast<float>(sdauth::UpdateProgress()) / 100.0f, accent_color());

	// Action button.
	const Rect button = rect(card.max.x - 200.0f, card.min.y + 62.0f, card.max.x - 28.0f, card.min.y + 104.0f);
	ImGui::SetCursorScreenPos(button.min);
	const ImVec2 size(width(button), height(button));

	if (ready)
	{
		if (shared.restart_ready_time < 0.0)
		{
			shared.restart_ready_time = now;
			restart_failed = false;
		}

		const double elapsed = now - shared.restart_ready_time;
		bool restart = ImGui::Button("Restart now", size);
		if (!restart && !restart_failed && elapsed >= kRestartCountdownSeconds)
			restart = true;

		if (restart)
		{
			restart_failed = !sdauth::RestartLoader();
			if (!restart_failed)
				PostQuitMessage(0);
			else
				shared.restart_ready_time = now;
		}
		else if (!restart_failed)
		{
			int remaining = static_cast<int>(kRestartCountdownSeconds - elapsed);
			if (remaining < 1)
				remaining = 1;
			char countdown[48];
			std::snprintf(countdown, sizeof(countdown), "Restarting in %d...", remaining);
			text(font.caption, 11.0f, ImVec2(button.min.x, button.max.y + 10.0f), kMuted, countdown);
		}
		return;
	}

	shared.restart_ready_time = -1.0;
	restart_failed = false;

	if (downloading)
	{
		ImGui::BeginDisabled();
		secondary_button("Downloading...", size);
		ImGui::EndDisabled();
	}
	else if (failed)
	{
		if (ImGui::Button("Retry update", size))
			sdauth::StartUpdate();
	}
	else if (available)
	{
		if (ImGui::Button("Update now", size))
			sdauth::StartUpdate();
	}
	else
	{
		ImGui::BeginDisabled();
		secondary_button("Up to date", size);
		ImGui::EndDisabled();
	}

	if (ImGui::IsItemHovered() && !downloading && (available || failed))
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// ------------------------------------------------------------ file list ----

void draw_component_files(const Rect& card)
{
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 28.0f, card.min.y + 20.0f), kText, "Component files");
	text(font.caption, 11.0f, ImVec2(card.min.x + 28.0f, card.min.y + 46.0f), kMuted,
		"Files are downloaded on first launch and checked with SHA-256.");

	if (text_link("##open_folder", "Open folder",
		ImVec2(card.max.x - 28.0f - text_width(font.semibold, 12.0f, "Open folder"), card.min.y + 24.0f)))
		open_data_folder();

	divider(card.min.x + 28.0f, card.max.x - 28.0f, card.min.y + 72.0f);

	const std::vector<sdauth::Component> components = sdauth::Components();
	const Rect area = rect(card.min.x + 28.0f, card.min.y + 108.0f, card.max.x - 20.0f, card.max.y - 14.0f);

	if (components.empty())
	{
		draw_empty_state(rect(card.min.x + 28.0f, card.min.y + 84.0f, card.max.x - 28.0f, card.max.y - 16.0f),
			"No files yet", "Components released by the server will be listed here.");
		return;
	}

	// Column headers.
	const float col_version = area.min.x + 250.0f;
	const float col_status = area.min.x + 350.0f;
	const float col_cache = area.min.x + 450.0f;
	text(font.caption, 10.0f, ImVec2(area.min.x + 8.0f, card.min.y + 86.0f), kDim, "NAME");
	text(font.caption, 10.0f, ImVec2(col_version, card.min.y + 86.0f), kDim, "VERSION");
	text(font.caption, 10.0f, ImVec2(col_status, card.min.y + 86.0f), kDim, "STATUS");
	text(font.caption, 10.0f, ImVec2(col_cache, card.min.y + 86.0f), kDim, "ON THIS PC");

	const int launch_status = sdauth::LaunchStatus();
	const bool launch_busy = launch_status == sdauth::StatusWorking;

	constexpr float row_h = 46.0f;
	ImGui::SetCursorScreenPos(area.min);
	ImGui::BeginChild("##download_rows", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	for (int i = 0; i < static_cast<int>(components.size()); ++i)
	{
		const sdauth::Component& component = components[static_cast<std::size_t>(i)];
		const float y = origin.y + static_cast<float>(i) * row_h;
		const Rect row = rect(origin.x, y, origin.x + width(area) - 8.0f, y + row_h - 6.0f);
		box(row, kCardSoft, 9.0f, &kLine);

		const float mid = y + (row_h - 6.0f) * 0.5f;
		draw->PushClipRect(row.min, ImVec2(row.min.x + 236.0f, row.max.y), true);
		text(font.semibold, 13.0f, ImVec2(row.min.x + 8.0f, mid - 8.0f), kText, component.name.c_str());
		draw->PopClipRect();

		char version[48];
		std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
		text(font.body, 13.0f, ImVec2(col_version - area.min.x + origin.x, mid - 8.0f), kMuted, version);

		const float status_x = col_status - area.min.x + origin.x;
		draw->AddCircleFilled(ImVec2(status_x + 4.0f, mid), 3.5f, color_u32(status_color(component.status)), 12);
		text(font.body, 13.0f, ImVec2(status_x + 14.0f, mid - 8.0f), kMuted, status_label(component.status).c_str());

		const long long size = cached_size(component.name);
		const float cache_x = col_cache - area.min.x + origin.x;
		if (size >= 0)
			text(font.body, 13.0f, ImVec2(cache_x, mid - 8.0f), kGreen, format_size(size).c_str());
		else
			text(font.body, 13.0f, ImVec2(cache_x, mid - 8.0f), kDim, "Not downloaded");

		const bool this_busy = launch_busy && sdauth::LaunchName() == component.name;
		const Rect button = rect(row.max.x - 124.0f, row.min.y + 6.0f, row.max.x - 8.0f, row.max.y - 6.0f);
		const char* label = this_busy ? "Working..." : (size >= 0 ? "Run" : "Download & run");
		ImGui::PushID(i);
		if (component_launchable(component) && !launch_busy)
		{
			ImGui::SetCursorScreenPos(button.min);
			if (ImGui::Button(label, ImVec2(width(button), height(button))))
				sdauth::LaunchComponent(component.name);
			if (ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}
		else
		{
			ImGui::SetCursorScreenPos(button.min);
			ImGui::BeginDisabled();
			secondary_button(this_busy ? label : (component.download_url.empty() ? "No file" : "Unavailable"),
				ImVec2(width(button), height(button)));
			ImGui::EndDisabled();
		}
		ImGui::PopID();
	}

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, static_cast<float>(components.size()) * row_h));
	ImGui::EndChild();
}

} // namespace

void DrawDownloads()
{
	draw_page_header("Downloads", "Loader updates and component files.");

	draw_update_card(rect(kContentLeft, kContentTop, kContentRight, 332.0f));
	draw_component_files(rect(kContentLeft, 348.0f, kContentRight, kContentBottom));
}

} // namespace dash
