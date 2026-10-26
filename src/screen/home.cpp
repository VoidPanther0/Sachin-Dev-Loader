#include "../ui.h"

#include <cstdio>

namespace screen_home {

namespace {

using namespace ui;

// ------------------------------------------------------------------ chrome ----

void draw_background()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, app::kHeight), color_u32(kBg));
	draw->AddCircleFilled(ImVec2(app::kWidth - 60.0f, 0.0f), 280.0f,
		color_u32(fade(accent_color(), 0.05f)), 64);
}

void draw_title_bar()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(app::kWidth, 60.0f), color_u32(kTitleBg));
	draw->AddLine(ImVec2(0.0f, 59.0f), ImVec2(app::kWidth, 59.0f), color_u32(kLine), 1.0f);

	const Rect logo = rect(20.0f, 14.0f, 50.0f, 44.0f);
	box(logo, accent_color(), 8.0f);
	text_center(font.semibold, 13.0f, logo, ImVec4(0.04f, 0.05f, 0.08f, 1.0f), "IM");

	text(font.semibold, 14.0f, ImVec2(62.0f, 15.0f), kText, "Sachin Loader");
	text(font.caption, 11.0f, ImVec2(62.0f, 33.0f), kMuted, "Win32  |  DirectX 11  |  Loader");

	ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
	ImGui::InvisibleButton("##title_drag", ImVec2(app::kWidth - 100.0f, 60.0f));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	if (window_button("##minimize", rect(app::kWidth - 84.0f, 14.0f, app::kWidth - 52.0f, 46.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);

	if (window_button("##close", rect(app::kWidth - 44.0f, 14.0f, app::kWidth - 12.0f, 46.0f), true))
		PostQuitMessage(0);
}

void draw_header()
{
	text(font.caption, 10.0f, ImVec2(36.0f, 86.0f), accent_color(), "DEAR IMGUI / STARTER");
	text(font.title, 28.0f, ImVec2(36.0f, 102.0f), kText, "Single-screen template");
	text(font.body, 13.0f, ImVec2(36.0f, 142.0f), kMuted,
		"All common ImGui widgets in one place. Keep what you need, delete the rest.");

	ImGui::SetCursorScreenPos(ImVec2(830.0f, 104.0f));
	if (secondary_button("Sign out", ImVec2(120.0f, 34.0f)))
		screen_login::SignOut();

	ImGui::SetCursorScreenPos(ImVec2(976.0f, 104.0f));
	if (secondary_button("Reset all", ImVec2(148.0f, 34.0f)))
		reset_state();

	pill(rect(1006.0f, 146.0f, 1124.0f, 168.0f), "TEMPLATE", accent_color());

	divider(36.0f, 1124.0f, 176.0f);
}

void draw_footer()
{
	divider(36.0f, 1124.0f, 676.0f);

	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(40.0f, 689.0f), 3.0f, color_u32(kGreen));

	char left[96];
	std::snprintf(left, sizeof(left), "Dear ImGui %s  |  Win32  |  DirectX 11", IMGUI_VERSION);
	text(font.caption, 10.0f, ImVec2(50.0f, 682.0f), kDim, left);

	char right[64];
	std::snprintf(right, sizeof(right), "FPS %d  |  v%s",
		static_cast<int>(state.fps + 0.5f), app::kVersion);
	const float right_width = text_width(font.caption, 10.0f, right);
	text(font.caption, 10.0f, ImVec2(1124.0f - right_width, 682.0f), kDim, right);
}

// ------------------------------------------------------------------ cards ----

void draw_inputs_card()
{
	const Rect card = rect(36.0f, 190.0f, 386.0f, 656.0f);
	draw_card(card, "Input fields", "Native text and numeric widgets.");

	const float x = 58.0f;
	const float w = 306.0f;

	label("Display name", x, 258.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 276.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::InputText("##display_name", state.display_name, sizeof(state.display_name));

	label("Search", x, 318.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 336.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::InputTextWithHint("##search", "Search the template...", state.search, sizeof(state.search));

	label("Notes", x, 378.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 396.0f));
	ImGui::InputTextMultiline("##notes", state.notes, sizeof(state.notes), ImVec2(w, 74.0f));

	label("Retries", x, 482.0f);
	label("Timeout (s)", 220.0f, 482.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 500.0f));
	ImGui::SetNextItemWidth(144.0f);
	ImGui::InputInt("##retries", &state.retries, 1, 10);

	ImGui::SetCursorScreenPos(ImVec2(220.0f, 500.0f));
	ImGui::SetNextItemWidth(144.0f);
	ImGui::InputFloat("##timeout", &state.timeout, 1.0f, 10.0f, "%.1f s");

	label("Opacity", x, 544.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 562.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::SliderFloat("##opacity", &state.opacity, 0.0f, 1.0f, "%.2f");

	text(font.caption, 11.0f, ImVec2(x, 612.0f), kDim, "Values stay local to this demo.");
}

void draw_selection_card()
{
	const Rect card = rect(404.0f, 190.0f, 754.0f, 656.0f);
	draw_card(card, "Selection", "Options, colors and state.");

	const float x = 426.0f;
	const float w = 306.0f;

	static const char* environments[] = { "Development", "Staging", "Production" };
	static const char* qualities[] = { "Low", "Medium", "High", "Ultra" };

	label("Environment", x, 258.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 276.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::Combo("##environment", &state.environment, environments, IM_ARRAYSIZE(environments));

	label("Quality", x, 318.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 336.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::Combo("##quality", &state.quality, qualities, IM_ARRAYSIZE(qualities));

	label("Accent color", x, 378.0f);
	ImGui::SetCursorScreenPos(ImVec2(x, 396.0f));
	ImGui::SetNextItemWidth(w);
	ImGui::ColorEdit3("##accent", state.accent,
		ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB);

	divider(x, x + w, 446.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 458.0f));
	ImGui::Checkbox("Enable notifications", &state.notifications);

	ImGui::SetCursorScreenPos(ImVec2(x, 486.0f));
	ImGui::Checkbox("Restore last session", &state.restore_session);

	label("Layout density", x, 522.0f);

	ImGui::SetCursorScreenPos(ImVec2(x, 540.0f));
	ImGui::RadioButton("Comfortable", &state.density, 0);

	ImGui::SetCursorScreenPos(ImVec2(x, 568.0f));
	ImGui::RadioButton("Compact", &state.density, 1);

	ImGui::SetCursorScreenPos(ImVec2(x, 596.0f));
	ImGui::RadioButton("Dense", &state.density, 2);
}

void draw_actions_card()
{
	const Rect card = rect(772.0f, 190.0f, 1122.0f, 656.0f);
	draw_card(card, "Actions", "Buttons, progress and feedback.");

	const float x = 794.0f;
	const float w = 306.0f;

	ImGui::SetCursorScreenPos(ImVec2(x, 258.0f));
	if (ImGui::Button("Primary action", ImVec2(w, 34.0f)))
		state.status = "Primary action clicked.";

	ImGui::SetCursorScreenPos(ImVec2(x, 300.0f));
	if (secondary_button("Secondary", ImVec2(146.0f, 32.0f)))
		state.status = "Secondary action clicked.";

	ImGui::SetCursorScreenPos(ImVec2(x + 160.0f, 300.0f));
	ImGui::BeginDisabled();
	ImGui::Button("Disabled", ImVec2(146.0f, 32.0f));
	ImGui::EndDisabled();

	ImGui::SetCursorScreenPos(ImVec2(x, 342.0f));
	if (ImGui::SmallButton("Small button"))
		state.status = "Small button clicked.";
	text(font.caption, 11.0f, ImVec2(x + 94.0f, 346.0f), kDim, "Compact variant");

	divider(x, x + w, 374.0f);

	label("Demo progress", x, 386.0f);
	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(state.progress * 100.0f + 0.5f));

	ImGui::SetCursorScreenPos(ImVec2(x, 404.0f));
	ImGui::ProgressBar(state.progress, ImVec2(w, 20.0f), percent);

	label("Status", x, 434.0f);
	ImGui::PushFont(font.caption, 12.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, state.running ? accent_color() : kMuted);
	ImGui::SetCursorScreenPos(ImVec2(x, 452.0f));
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
	ImGui::TextUnformatted(state.status.c_str());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
	ImGui::PopFont();

	ImGui::SetCursorScreenPos(ImVec2(x, 492.0f));
	if (secondary_button("Run demo", ImVec2(146.0f, 32.0f)))
	{
		state.progress = 0.0f;
		state.running = true;
		state.status = "Running demo...";
	}

	ImGui::SetCursorScreenPos(ImVec2(x + 160.0f, 492.0f));
	if (secondary_button("Reset demo", ImVec2(146.0f, 32.0f)))
	{
		state.progress = 0.0f;
		state.running = false;
		state.status = "Demo reset.";
	}

	ImGui::SetCursorScreenPos(ImVec2(x, 540.0f));
	if (ImGui::TreeNodeEx("Template notes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Edit this screen in src/screen/home.cpp.");
		ImGui::TextDisabled("Change app::kTitle in src/app.h.");
		ImGui::TextDisabled("Dear ImGui is vendored in src/gui.");
		ImGui::TreePop();
	}
}

} // namespace

void Render()
{
	const ImGuiIO& io = ImGui::GetIO();

	if (ui::state.running)
	{
		ui::state.progress += io.DeltaTime * 0.45f;
		if (ui::state.progress >= 1.0f)
		{
			ui::state.progress = 1.0f;
			ui::state.running = false;
			ui::state.status = "Demo complete.";
		}
	}

	draw_background();
	draw_title_bar();
	draw_header();

	draw_inputs_card();
	draw_selection_card();
	draw_actions_card();

	draw_footer();
}

} // namespace screen_home
