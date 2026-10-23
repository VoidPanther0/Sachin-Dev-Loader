#include "app.h"
#include "ui/branding.h"
#include "ui/colorutil.h"
#include "ui/fonts.h"
#include "ui/palette.h"
#include "ui/screens.h"
#include "ui/state.h"
#include "ui/widgets.h"

#include <cmath>
#include <cstdio>

namespace ui {

void page_splash()
{
	const float w = app::ViewportWidth();
	const float h = app::ViewportHeight();

	draw_background(w, h, false);

	const float reveal = colorutil::clampf(S.screen_time * 2.2f, 0.0f, 1.0f);
	const float progress = colorutil::clampf(S.screen_time / 2.2f, 0.0f, 1.0f);
	const float pulse = 1.0f + 0.012f * std::sin(S.time * 1.7f);

	soft_glow(ImVec2(w * 0.5f, h * 0.40f), 300.0f, P.accent, 0.014f * reveal, 28);
	draw_logo(ImVec2(w * 0.5f, h * 0.5f - 92.0f), 126.0f * pulse, reveal);
	draw_wordmark_center(w * 0.5f, h * 0.5f - 8.0f, 38.0f, reveal);
	text_center_x(F.subtitle, 19.0f, w * 0.5f, h * 0.5f + 48.0f, colorutil::alpha(P.text_secondary, reveal), "One Loader. Every Game.");

	const Rect track = rect(w * 0.5f - 160.0f, h * 0.5f + 118.0f, w * 0.5f + 160.0f, h * 0.5f + 123.0f);
	box(track, colorutil::alpha(P.text, 0.05f), 2.5f);

	if (progress > 0.002f)
	{
		const Rect fill = rect(track.min.x, track.min.y, track.min.x + width(track) * progress, track.max.y);
		gradient_round_rect(fill, 2.5f, P.accent, P.accent2);

		const ImVec2 head(fill.max.x, center(fill).y);
		soft_glow(head, 22.0f, P.accent, 0.05f, 12);
		ImGui::GetWindowDrawList()->AddCircleFilled(head, 2.6f, colorutil::alpha(P.text, reveal));
	}

	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(progress * 100.0f + 0.5f));
	text(F.micro, 10.0f, ImVec2(track.max.x - text_width(F.micro, 10.0f, percent), track.min.y + 12.0f), colorutil::alpha(P.text_dim, reveal), percent);

	text_center_x(F.tiny, 11.0f, w * 0.5f, track.min.y + 12.0f, colorutil::alpha(P.text_secondary, reveal), progress < 0.98f ? "Starting secure session..." : "Ready");

	char version[32];
	std::snprintf(version, sizeof(version), "VERSION %s", app::kVersion);
	text_center_x(F.micro, 10.0f, w * 0.5f, h - 44.0f, colorutil::alpha(P.text_dim, reveal), version);
}

} // namespace ui
