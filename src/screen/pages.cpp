#include "common/dashboard_common.h"
#include "common/login_common.h"

#include <cstdio>
#include <string>

// Three content pages shown by the dashboard shell: Product, Game Updates and
// Socials. They share the layout and helpers used by home.cpp so the pages
// read as one system.

namespace dash {

using namespace ui;

namespace {

// =============================================================== Product ====

void draw_product_hero()
{
	const Rect hero = rect(kContentLeft, kContentTop, kContentRight, 216.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	gradient_box(hero, ImVec4(0.055f, 0.085f, 0.205f, 1.0f), ImVec4(0.150f, 0.075f, 0.285f, 1.0f), 16.0f);

	draw->PushClipRect(hero.min, hero.max, true);
	if (!shared.reduced_effects)
	{
		draw->AddCircleFilled(ImVec2(hero.max.x - 60.0f, hero.min.y + 60.0f), 140.0f,
			color_u32(fade(kPurple, 0.14f)), 48);
	}
	login_ui::DrawLogo(ImVec2(hero.max.x - 90.0f, hero.min.y + 72.0f), 82.0f);

	const ImVec2 title_pos(hero.min.x + 32.0f, hero.min.y + 26.0f);
	text(font.title, 28.0f, title_pos, kText, "Sachin Loader");

	char version[32];
	std::snprintf(version, sizeof(version), "v%s", app::kVersion);
	const float pw = text_width(font.semibold, 12.0f, version) + 32.0f;
	pill(rect(title_pos.x, title_pos.y + 42.0f, title_pos.x + pw, title_pos.y + 66.0f), version, accent_color());

	text_wrapped(font.body, 14.0f, ImVec2(title_pos.x, title_pos.y + 86.0f), kMuted,
		"One place to launch every tool released for your account. "
		"Signed, checksummed and started directly from the loader.", 360.0f);

	draw->PopClipRect();
	draw->AddRect(hero.min, hero.max, color_u32(kBorder), 16.0f, 0, 1.0f);
}

void draw_product_stats()
{
	constexpr float gap = 12.0f;
	const float w = (kContentRight - kContentLeft - gap * 2.0f) / 3.0f;
	const float y1 = 232.0f;
	const float y2 = 304.0f;
	float x = kContentLeft;

	char build[32];
	std::snprintf(build, sizeof(build), "v%s", app::kVersion);

	stat_card(rect(x, y1, x + w, y2), "BUILD", build, "Installed", accent_color(), icon_cube);
	x += w + gap;
	stat_card(rect(x, y1, x + w, y2), "RENDERER", "DirectX 11", "Hardware accelerated", kGreen, icon_bolt);
	x += w + gap;

	const int components = static_cast<int>(sdauth::Components().size());
	char ccount[16];
	std::snprintf(ccount, sizeof(ccount), "%d", components);
	stat_card(rect(x, y1, x + w, y2), "COMPONENTS", ccount, "Released to you", kPurple, icon_components);
}

void draw_product_features()
{
	const Rect card = rect(kContentLeft, 320.0f, kContentRight, kContentBottom);
	box(card, kCard, 14.0f, card_border());
	section_header(card, "What's included", icon_product);
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 64.0f);

	struct Feature { const char* name; const char* hint; };
	const Feature features[] = {
		{ "Secure sign-in",  "Server-backed session; passwords never touch disk." },
		{ "Signed launches", "Every component is checked against its SHA-256 before start." },
		{ "Instant updates", "A new build swaps itself in without a reinstall." },
	};

	constexpr float gap = 10.0f;
	const int count = static_cast<int>(sizeof(features) / sizeof(features[0]));
	const float area_top = card.min.y + 80.0f;
	const float area_bottom = card.max.y - 16.0f;
	const float row_h = (area_bottom - area_top - gap * (count - 1)) / count;

	float y = area_top;
	for (int i = 0; i < count; ++i)
	{
		const Rect r = rect(card.min.x + 20.0f, y, card.max.x - 20.0f, y + row_h);
		box(r, kCardSoft, 10.0f, &kLine);
		text(font.semibold, 14.0f, ImVec2(r.min.x + 18.0f, r.min.y + 10.0f), kText, features[i].name);
		text(font.caption, 11.0f, ImVec2(r.min.x + 18.0f, r.min.y + 30.0f), kMuted, features[i].hint);
		icon_arrow(ImVec2(r.max.x - 22.0f, r.min.y + height(r) * 0.5f), color_u32(fade(accent_color(), 0.6f)));
		y += row_h + gap;
	}
}

// ========================================================== Game Updates ====

struct UpdateEntry { const char* version; const char* date; const char* summary; };

const UpdateEntry kUpdates[] = {
	{ "1.0", "2026-11-02", "Reshaped the window shell: new header, sidebar and profile strip." },
	{ "0.9", "2026-10-18", "Signed launches: every component checked against its SHA-256." },
	{ "0.8", "2026-10-04", "Supabase sign-in and remembered username." },
	{ "0.7", "2026-09-22", "Dark and light theme with a saved accent." },
	{ "0.6", "2026-09-10", "First public preview." },
};

void draw_game_updates()
{
	const Rect card = rect(kContentLeft, kContentTop, kContentRight, kContentBottom);
	box(card, kCard, 14.0f, card_border());
	section_header(card, "Recent updates", icon_updates);
	divider(card.min.x + 24.0f, card.max.x - 24.0f, card.min.y + 64.0f);

	const int count = static_cast<int>(sizeof(kUpdates) / sizeof(kUpdates[0]));
	constexpr float gap = 10.0f;
	const float row_h = 60.0f;
	const Rect area = rect(card.min.x + 20.0f, card.min.y + 80.0f, card.max.x - 20.0f, card.max.y - 16.0f);

	ImGui::SetCursorScreenPos(area.min);
	ImGui::BeginChild("##updates_rows", ImVec2(width(area), height(area)), 0, ImGuiWindowFlags_NoBackground);
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	for (int i = 0; i < count; ++i)
	{
		const float y = origin.y + static_cast<float>(i) * (row_h + gap);
		const Rect row = rect(origin.x, y, origin.x + width(area) - 4.0f, y + row_h);
		box(row, kCardSoft, 10.0f, &kLine);

		letter_badge(rect(row.min.x + 12.0f, row.min.y + 10.0f, row.min.x + 54.0f, row.max.y - 10.0f),
			kUpdates[i].version, 14.0f);

		char version_label[32];
		std::snprintf(version_label, sizeof(version_label), "v%s", kUpdates[i].version);
		text(font.semibold, 14.0f, ImVec2(row.min.x + 66.0f, row.min.y + 8.0f), kText, version_label);
		text(font.caption, 11.0f, ImVec2(row.min.x + 66.0f, row.min.y + 28.0f), kMuted, kUpdates[i].date);
		text(font.body, 12.0f, ImVec2(row.min.x + 66.0f, row.min.y + 42.0f), kNavText, kUpdates[i].summary);

		if (i == 0)
		{
			const char* label = "CURRENT";
			const float lw = text_width(font.caption, 10.0f, label) + 32.0f;
			const float cy = row.min.y + height(row) * 0.5f;
			pill(rect(row.max.x - lw - 12.0f, cy - 11.0f, row.max.x - 12.0f, cy + 11.0f), label, accent_color());
		}
	}

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(1.0f, static_cast<float>(count) * (row_h + gap) - gap));
	ImGui::EndChild();
}

// ================================================================ Socials ====

struct SocialEntry { const char* name; const char* handle; const char* url; ImVec4 tint; };

const SocialEntry kSocials[] = {
	{ "Discord", "discord.gg/sachinloader",   "https://discord.gg/sachinloader",   ImVec4(0.44f, 0.49f, 0.95f, 1.0f) },
	{ "YouTube", "@sachinloader",             "https://youtube.com/@sachinloader", ImVec4(0.95f, 0.22f, 0.22f, 1.0f) },
	{ "Twitter", "@sachinloader",             "https://twitter.com/sachinloader",  ImVec4(0.11f, 0.63f, 0.95f, 1.0f) },
	{ "GitHub",  "github.com/sachinloader",   "https://github.com/sachinloader",   ImVec4(0.42f, 0.56f, 1.00f, 1.0f) },
};

void draw_socials_grid()
{
	const int count = static_cast<int>(sizeof(kSocials) / sizeof(kSocials[0]));
	constexpr float gap = 14.0f;
	const float grid_x0 = kContentLeft;
	const float grid_x1 = kContentRight;
	const float grid_y0 = kContentTop;
	const float grid_y1 = kContentBottom;
	const float card_w = (grid_x1 - grid_x0 - gap) * 0.5f;
	const float card_h = (grid_y1 - grid_y0 - gap) * 0.5f;

	ImDrawList* draw = ImGui::GetWindowDrawList();

	for (int i = 0; i < count; ++i)
	{
		const int col = i & 1;
		const int row = i >> 1;
		const float x = grid_x0 + static_cast<float>(col) * (card_w + gap);
		const float y = grid_y0 + static_cast<float>(row) * (card_h + gap);
		const Rect r = rect(x, y, x + card_w, y + card_h);

		const SocialEntry& e = kSocials[i];
		box(r, kCard, 14.0f, card_border());

		const float cx = r.min.x + 48.0f;
		const float cy = r.min.y + 56.0f;
		draw->AddCircleFilled(ImVec2(cx, cy), 26.0f, color_u32(fade(e.tint, 0.20f)), 32);
		draw->AddCircle(ImVec2(cx, cy), 26.0f, color_u32(fade(e.tint, 0.55f)), 32, 1.0f);
		char letter[2] = { static_cast<char>(e.name[0]), 0 };
		const float lw = text_width(font.semibold, 22.0f, letter);
		text(font.semibold, 22.0f, ImVec2(cx - lw * 0.5f, cy - 12.0f), lift(e.tint, 0.14f), letter);

		text(font.semibold, 17.0f, ImVec2(r.min.x + 86.0f, r.min.y + 36.0f), kText, e.name);
		text(font.caption, 11.0f, ImVec2(r.min.x + 86.0f, r.min.y + 58.0f), kMuted, e.handle);

		const Rect btn = rect(r.min.x + 20.0f, r.max.y - 52.0f, r.max.x - 20.0f, r.max.y - 18.0f);
		if (primary_button(e.name, btn, "Open", nullptr))
			open_url(e.url);
	}
}

} // namespace

void DrawProduct()
{
	draw_product_hero();
	draw_product_stats();
	draw_product_features();
}

void DrawGameUpdates()
{
	draw_game_updates();
}

void DrawSocials()
{
	draw_socials_grid();
}

} // namespace dash
