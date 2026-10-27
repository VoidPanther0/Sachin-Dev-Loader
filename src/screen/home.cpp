#include "common/dashboard_common.h"

#include <cstdio>
#include <string>
#include <vector>

// Home: greeting, key numbers, quick access to released components and the
// loader status.

namespace dash {

using namespace ui;

namespace {

void draw_hero()
{
	const Rect hero = rect(kContentLeft, kContentTop, kContentRight, 292.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	gradient_box(hero, ImVec4(0.055f, 0.085f, 0.205f, 1.0f), ImVec4(0.135f, 0.070f, 0.260f, 1.0f), 16.0f);
	draw->PushClipRect(hero.min, hero.max, true);
	draw->AddCircleFilled(ImVec2(hero.max.x - 70.0f, hero.min.y + 20.0f), 170.0f,
		color_u32(fade(kPurple, 0.10f)), 64);
	text(font.title, 110.0f, ImVec2(hero.max.x - 168.0f, hero.min.y + 14.0f), fade(kPurple, 0.10f), "S");
	draw->PopClipRect();
	draw->AddRect(hero.min, hero.max, color_u32(kBorder), 16.0f, 0, 1.0f);

	text(font.caption, 10.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 22.0f), accent_color(), "WELCOME BACK");

	const sdauth::Session session = sdauth::GetSession();
	char greeting[96];
	std::snprintf(greeting, sizeof(greeting), "Hi, %s",
		session.username.empty() ? "friend" : session.username.c_str());
	text(font.title, 30.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 38.0f), kText, greeting);

	text(font.body, 14.0f, ImVec2(hero.min.x + 32.0f, hero.min.y + 78.0f), kMuted,
		"Your loader is ready. Launch your apps and games from one place.");

	char build[32];
	std::snprintf(build, sizeof(build), "BUILD %s", app::kVersion);
	pill(rect(hero.min.x + 32.0f, hero.min.y + 104.0f, hero.min.x + 112.0f, hero.min.y + 126.0f), build, kGreen);

	const bool available = update_available();
	pill(rect(hero.min.x + 124.0f, hero.min.y + 104.0f, hero.min.x + 124.0f + (available ? 118.0f : 126.0f),
		hero.min.y + 126.0f), available ? "UPDATE READY" : "UP TO DATE", available ? kBlue : accent_color());

	// The primary action follows the situation: update first, apps otherwise.
	ImGui::SetCursorScreenPos(ImVec2(hero.max.x - 164.0f, hero.min.y + 96.0f));
	if (available)
	{
		if (ImGui::Button("Update now", ImVec2(132.0f, 32.0f)))
			state.page = PageDownloads;
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else if (secondary_button("Open applications", ImVec2(132.0f, 32.0f)))
	{
		state.page = PageApplications;
	}
}

void draw_stats()
{
	const std::vector<sdauth::Component> components = sdauth::Components();

	char count[16];
	std::snprintf(count, sizeof(count), "%d", static_cast<int>(components.size()));

	char build[32];
	std::snprintf(build, sizeof(build), "v%s", app::kVersion);

	const std::string uptime = session_time();

	// Access: days left on the account, or "Lifetime" when the server sets none.
	const int days_left = access_days_left();
	const std::string date = access_date();
	char access_value[32];
	char access_hint[48];
	ImVec4 access_color = kGreen;
	if (days_left < 0)
	{
		std::snprintf(access_value, sizeof(access_value), "Lifetime");
		std::snprintf(access_hint, sizeof(access_hint), "No expiry");
	}
	else
	{
		std::snprintf(access_value, sizeof(access_value), "%d day%s", days_left, days_left == 1 ? "" : "s");
		std::snprintf(access_hint, sizeof(access_hint), "Until %s", date.c_str());
		if (days_left <= 7)
			access_color = kOrange;
	}

	constexpr float gap = 16.0f;
	const float w = (kContentRight - kContentLeft - gap * 3.0f) / 4.0f;
	const float y1 = 308.0f;
	const float y2 = 396.0f;
	float x = kContentLeft;

	stat_card(rect(x, y1, x + w, y2), "COMPONENTS", count, "Released for you", accent_color());
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "BUILD", build, update_text(), update_color());
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "ACCESS", access_value, access_hint, access_color);
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "SESSION", uptime.c_str(), "Since sign in", kPurple);
}

void draw_quick_access()
{
	const Rect card = rect(kContentLeft, 412.0f, 800.0f, kContentBottom);
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 24.0f, card.min.y + 20.0f), kText, "Quick access");
	text(font.caption, 11.0f, ImVec2(card.min.x + 24.0f, card.min.y + 46.0f), kMuted,
		"Released apps, games and tools for this account.");

	const std::vector<sdauth::Component> components = sdauth::Components();
	if (!components.empty())
	{
		if (text_link("##quick_all", "View all", ImVec2(card.max.x - 24.0f - text_width(font.semibold, 12.0f, "View all"),
			card.min.y + 24.0f)))
			state.page = PageApplications;
	}
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 72.0f);

	const Rect area = rect(card.min.x + 24.0f, card.min.y + 88.0f, card.max.x - 24.0f, card.max.y - 20.0f);
	if (components.empty())
	{
		draw_empty_state(area, "Nothing here yet", "Content appears when the server releases it.");
		return;
	}

	constexpr float gap = 16.0f;
	const float tile_w = (width(area) - gap) * 0.5f;
	const float tile_h = (height(area) - gap) * 0.5f;
	const int shown = components.size() < 4 ? static_cast<int>(components.size()) : 4;

	for (int i = 0; i < shown; ++i)
	{
		const sdauth::Component& component = components[static_cast<std::size_t>(i)];
		const float x = area.min.x + static_cast<float>(i % 2) * (tile_w + gap);
		const float y = area.min.y + static_cast<float>(i / 2) * (tile_h + gap);
		const Rect tile = rect(x, y, x + tile_w, y + tile_h);

		ImGui::PushID(i);
		ImGui::SetCursorScreenPos(tile.min);
		const bool clicked = ImGui::InvisibleButton("##quick", ImVec2(width(tile), height(tile)));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		box(tile, hovered ? lift(kCardSoft, 0.02f) : kCardSoft, 10.0f, hovered ? &kLineBright : &kLine);
		const float badge = height(tile) - 20.0f;
		letter_badge(rect(tile.min.x + 10.0f, tile.min.y + 10.0f, tile.min.x + 10.0f + badge, tile.max.y - 10.0f),
			component.name, 16.0f);

		ImDrawList* draw = ImGui::GetWindowDrawList();
		draw->PushClipRect(tile.min, ImVec2(tile.max.x - 12.0f, tile.max.y), true);
		text(font.semibold, 14.0f, ImVec2(tile.min.x + badge + 22.0f, tile.min.y + 11.0f), kText,
			component.name.c_str());
		char version[48];
		std::snprintf(version, sizeof(version), "v%s", component.version.c_str());
		text(font.caption, 11.0f, ImVec2(tile.min.x + badge + 22.0f, tile.min.y + 31.0f), kMuted, version);
		draw->PopClipRect();
		draw->AddCircleFilled(ImVec2(tile.max.x - 16.0f, tile.min.y + 19.0f), 3.5f,
			color_u32(status_color(component.status)), 12);

		if (clicked)
			open_component(i);
	}
}

void draw_loader_status()
{
	const Rect card = rect(816.0f, 412.0f, kContentRight, kContentBottom);
	box(card, kCard, 14.0f, &kBorder);
	text(font.semibold, 17.0f, ImVec2(card.min.x + 24.0f, card.min.y + 20.0f), kText, "Loader status");
	if (text_link("##status_about", "About", ImVec2(card.max.x - 24.0f - text_width(font.semibold, 12.0f, "About"),
		card.min.y + 24.0f)))
		state.page = PageAbout;
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 56.0f);

	const std::string latest = sdauth::StartupLatestVersion();
	const std::string components = std::to_string(sdauth::Components().size());

	const Rect inner = rect(card.min.x - 4.0f, card.min.y, card.max.x, card.max.y);
	float y = card.min.y + 74.0f;
	info_row(inner, y, "SERVER", "Connected", kGreen);
	y += 34.0f;
	info_row(inner, y, "VERSION", app::kVersion, kText);
	y += 34.0f;
	info_row(inner, y, "LATEST", latest.empty() ? app::kVersion : latest.c_str(), kText);
	y += 34.0f;
	info_row(inner, y, "UPDATE", update_text(), update_color());
	y += 34.0f;
	info_row(inner, y, "COMPONENTS", components.c_str(), kText);
}

} // namespace

void DrawHome()
{
	draw_page_header("Home", "Your loader at a glance.");
	draw_hero();
	draw_stats();
	draw_quick_access();
	draw_loader_status();
}

} // namespace dash
