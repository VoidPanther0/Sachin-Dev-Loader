#pragma once

#include "imgui.h"

#include <functional>
#include <string>

namespace ui {

enum class Level
{
	Info,
	Success,
	Warning,
	Danger
};

ImU32 level_color(Level level);

struct Rect
{
	ImVec2 min;
	ImVec2 max;
};

Rect rect(float x, float y, float x2, float y2);
float width(const Rect& r);
float height(const Rect& r);
ImVec2 center(const Rect& r);
Rect inset(const Rect& r, float amount);

// Animated hover/active blend in [0,1] for a widget id; call every frame the
// widget is visible so the spring has a chance to move toward `target`.
float animate(const char* id, float target, float speed = 14.0f);

void box(const Rect& r, ImU32 fill, float rounding, ImU32 border = 0, float border_size = 1.0f);
void panel(const Rect& r, float rounding = -1.0f);
void gradient_round_rect(const Rect& r, float rounding, ImU32 left, ImU32 right);
void soft_glow(ImVec2 position, float radius, ImU32 color, float strength, int steps = 22);

void text(ImFont* f, float size, ImVec2 position, ImU32 color, const char* value);
float text_width(ImFont* f, float size, const char* value);
void text_center(ImFont* f, float size, const Rect& r, ImU32 color, const char* value, float offset_y = 0.0f);
void text_center_x(ImFont* f, float size, float center_x, float y, ImU32 color, const char* value);
std::string elide(ImFont* f, float size, const std::string& value, float max_width);

using IconDrawer = std::function<void(ImVec2 center, float size, ImU32 color)>;

void icon_user(ImVec2 c, float size, ImU32 color);
void icon_lock(ImVec2 c, float size, ImU32 color);
void icon_eye(ImVec2 c, float size, ImU32 color, bool open);
void icon_arrow(ImVec2 c, float size, ImU32 color);
void icon_close(ImVec2 c, float size, ImU32 color);
void icon_minimize(ImVec2 c, float size, ImU32 color);
void icon_maximize(ImVec2 c, float size, ImU32 color, bool is_maximized);
void icon_check(ImVec2 c, float size, ImU32 color);
void icon_plus(ImVec2 c, float size, ImU32 color);
void icon_trash(ImVec2 c, float size, ImU32 color);
void icon_edit(ImVec2 c, float size, ImU32 color);
void icon_folder(ImVec2 c, float size, ImU32 color);
void icon_search(ImVec2 c, float size, ImU32 color);
void icon_gear(ImVec2 c, float size, ImU32 color);
void icon_info(ImVec2 c, float size, ImU32 color);
void icon_warning_sign(ImVec2 c, float size, ImU32 color);
void icon_home(ImVec2 c, float size, ImU32 color);
void icon_grid(ImVec2 c, float size, ImU32 color);
void icon_plug(ImVec2 c, float size, ImU32 color);
void icon_list(ImVec2 c, float size, ImU32 color);
void icon_palette(ImVec2 c, float size, ImU32 color);
void icon_star(ImVec2 c, float size, ImU32 color, bool filled);
void icon_chip(ImVec2 c, float size, ImU32 color);

void spinner(ImVec2 c, float radius, float thickness, ImU32 color, float time);

bool primary_button(const char* id, const Rect& r, const char* label, bool enabled = true, bool arrow = true);
bool ghost_button(const char* id, const Rect& r, const char* label);
bool danger_button(const char* id, const Rect& r, const char* label, bool enabled = true);
bool link_button(const char* id, const Rect& r, const char* label, ImU32 base_color);
bool icon_button(const char* id, const Rect& r, const IconDrawer& icon, bool subtle = true);

void draw_field(const char* id, const char* hint, const Rect& r, char* buffer, size_t size,
	bool password = false, bool* reveal = nullptr, const IconDrawer& leading_icon = {});
bool checkbox(const char* id, const Rect& r, bool* value, const char* label);
bool toggle_switch(const char* id, const Rect& r, bool* value);

void draw_banner(const Rect& r, Level level, const char* message);
void status_row(const Rect& card, float y, const char* label, const char* value, ImU32 dot = 0);
void badge(const Rect& r, const char* label, ImU32 color);
void progress_bar(const Rect& r, float progress01, ImU32 left, ImU32 right);

// A deterministic, pleasant accent picked from a small fixed palette by
// hashing `seed` (e.g. a game id) - gives each card/tile its own identity
// without needing per-entry color configuration.
ImU32 accent_for(const std::string& seed);

// Rounded, filled color chip with an icon centered in it (app/stat icons).
void icon_tile(const Rect& r, const IconDrawer& icon, ImU32 color, float rounding = -1.0f);

// A labeled number with an icon chip, e.g. "GAMES  12".
void stat_tile(const Rect& r, const IconDrawer& icon, ImU32 color, const char* value, const char* label);

// Returns the selected tab index (unchanged if nothing was clicked this frame).
int tabs(const char* id, const Rect& r, const char* const* labels, int count, int selected);

void empty_state(const Rect& r, const IconDrawer& icon, const char* title, const char* subtitle);

// Paired like ImGui::BeginChild/EndChild - always call end_scroll() once for
// every begin_scroll(), even if the region turned out empty.
void begin_scroll(const char* id, const Rect& r);
void end_scroll();

struct Modal
{
	bool open = false;
	bool close_requested = false;
	Rect content;
};
// Call every frame while the modal should be visible. Draws the dim backdrop
// and framed panel, and returns the inner content rect to draw into.
Modal modal_frame(const char* id, const char* title, float w, float h);

void push_toast(Level level, const std::string& text);
void render_toasts();

} // namespace ui
