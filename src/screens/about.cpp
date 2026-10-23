#include "app.h"
#include "ui/branding.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <cstdio>

namespace ui {

void page_about()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();
	const float sidebar_w = S.sidebar_collapsed ? layout::kSidebarWidthCollapsed : layout::kSidebarWidthExpanded;
	const Rect area = rect(sidebar_w + layout::kPad, layout::kTopbarHeight + layout::kPad, w - layout::kPad, h - layout::kPad);

	text(F.brand_small, 22.0f, ImVec2(area.min.x, area.min.y), P.text, "About");

	const Rect card = rect(area.min.x, area.min.y + 50.0f, area.min.x + 460.0f, area.min.y + 230.0f);
	panel(card, 14.0f);
	draw_logo(ImVec2(card.min.x + 56.0f, card.min.y + 60.0f), 48.0f);
	draw_wordmark_left(F.brand_small, card.min.x + 100.0f, card.min.y + 44.0f, 20.0f, 1.0f);
	char version_line[64];
	std::snprintf(version_line, sizeof(version_line), "Version %s", app::kVersion);
	text(F.link, 12.0f, ImVec2(card.min.x + 100.0f, card.min.y + 70.0f), P.text_dim, version_line);
	text(F.link, 12.0f, ImVec2(card.min.x + 100.0f, card.min.y + 88.0f), P.text_dim, "Windows 10/11 x64 - Dear ImGui - Direct3D 11");

	text(F.link, 12.0f, ImVec2(card.min.x + 20.0f, card.min.y + 140.0f), P.text_secondary,
		"A lightweight desktop loader for your own games and tools.");
	text(F.link, 12.0f, ImVec2(card.min.x + 20.0f, card.min.y + 160.0f), P.text_secondary,
		"Local-only: no accounts, telemetry, or network calls in this build.");

	const Rect check_rect = rect(card.min.x + 20.0f, card.max.y - 44.0f, card.min.x + 220.0f, card.max.y - 12.0f);
	ghost_button("##check_updates", check_rect, "Check for updates");
	text(F.tiny, 10.0f, ImVec2(check_rect.max.x + 12.0f, card.max.y - 32.0f), P.text_dim, "Needs a trusted update service - not wired up yet");

	const Rect credits = rect(area.min.x, card.max.y + 20.0f, area.min.x + 460.0f, area.max.y);
	panel(credits, 14.0f);
	text(F.micro, 10.0f, ImVec2(credits.min.x + 18.0f, credits.min.y + 14.0f), P.text_dim, "CREDITS & LICENSES");

	struct Credit { const char* name; const char* detail; };
	const Credit items[] = {
		{ "Dear ImGui", "MIT License - vendored in src/gui" },
		{ "Direct3D 11 / Win32", "Windows SDK, Microsoft" },
		{ "Segoe UI", "Loaded at runtime from the system font directory" },
	};

	float cy = credits.min.y + 40.0f;
	for (const Credit& item : items)
	{
		text(F.small_font, 12.0f, ImVec2(credits.min.x + 18.0f, cy), P.text, item.name);
		text(F.tiny, 10.0f, ImVec2(credits.min.x + 18.0f, cy + 18.0f), P.text_dim, item.detail);
		cy += 42.0f;
	}

	const Rect right = rect(card.max.x + 24.0f, area.min.y + 50.0f, area.max.x, area.max.y);
	panel(right, 14.0f);
	text(F.micro, 10.0f, ImVec2(right.min.x + 18.0f, right.min.y + 14.0f), P.text_dim, "BUILD INFORMATION");
	status_row(right, right.min.y + 50.0f, "VERSION", app::kVersion);
	status_row(right, right.min.y + 80.0f, "BUILD", __DATE__);
	status_row(right, right.min.y + 110.0f, "RENDERER", "Direct3D 11");
	status_row(right, right.min.y + 140.0f, "WINDOWING", "Win32");
	status_row(right, right.min.y + 170.0f, "UPDATE SERVICE", "Not configured", P.text_dim);
}

} // namespace ui
