#pragma once

#include "app.h"

#include <string>

// ---------------------------------------------------------------------------
// Shared UI API.
//
// ui.cpp implements this header and owns the app lifecycle. Each screen keeps
// its drawing code in src/screen and includes only this file.
// ---------------------------------------------------------------------------

namespace ui {

// ------------------------------------------------------------------ theme ----

inline const ImVec4 kBg(0.043f, 0.047f, 0.059f, 1.0f);
inline const ImVec4 kTitleBg(0.055f, 0.059f, 0.071f, 1.0f);
inline const ImVec4 kPanel(0.075f, 0.082f, 0.098f, 1.0f);
inline const ImVec4 kPanel2(0.098f, 0.106f, 0.125f, 1.0f);
inline const ImVec4 kPanel3(0.129f, 0.141f, 0.165f, 1.0f);
inline const ImVec4 kLine(1.0f, 1.0f, 1.0f, 0.07f);
inline const ImVec4 kLineBright(1.0f, 1.0f, 1.0f, 0.13f);
inline const ImVec4 kText(0.93f, 0.94f, 0.96f, 1.0f);
inline const ImVec4 kMuted(0.55f, 0.58f, 0.65f, 1.0f);
inline const ImVec4 kDim(0.38f, 0.40f, 0.46f, 1.0f);
inline const ImVec4 kGreen(0.35f, 0.82f, 0.55f, 1.0f);
inline const ImVec4 kRed(0.86f, 0.32f, 0.38f, 1.0f);

float clampf(float value, float low, float high);
ImVec4 lift(const ImVec4& color, float amount);
ImVec4 dim(const ImVec4& color, float amount);
ImVec4 fade(const ImVec4& color, float alpha);
ImU32 color_u32(const ImVec4& color);

// ------------------------------------------------------------------ state ----

struct State
{
	char display_name[64] = "Dear ImGui Template";
	char search[64] = "";
	char notes[256] = "One screen, common widgets, no backend. Use this file as your starting point.";

	int retries = 3;
	float timeout = 15.0f;
	float opacity = 0.85f;

	int environment = 0;
	int quality = 2;
	int density = 0;
	bool notifications = true;
	bool restore_session = false;

	float accent[3] = { 0.42f, 0.56f, 1.00f };
	float progress = 0.35f;
	bool running = false;

	float fps = 0.0f;
	std::string status = "Ready. Edit src/screen/home.cpp to start.";
};

extern State state;

ImVec4 accent_color();
void reset_state();

// ------------------------------------------------------------------ fonts ----

struct Fonts
{
	ImFont* caption = nullptr;
	ImFont* body = nullptr;
	ImFont* semibold = nullptr;
	ImFont* title = nullptr;
};

extern Fonts font;

void load_fonts();

// ------------------------------------------------------------- primitives ----

struct Rect
{
	ImVec2 min;
	ImVec2 max;
};

Rect rect(float x, float y, float x2, float y2);
float width(const Rect& r);
float height(const Rect& r);
ImVec2 center(const Rect& r);

void box(const Rect& r, const ImVec4& fill, float rounding = 10.0f, const ImVec4* border = nullptr);
void text(ImFont* f, float size, ImVec2 position, const ImVec4& color, const char* value);
float text_width(ImFont* f, float size, const char* value);
void text_center(ImFont* f, float size, const Rect& r, const ImVec4& color, const char* value);
void label(const char* value, float x, float y);
void divider(float x1, float x2, float y);
void pill(const Rect& r, const char* value, const ImVec4& color);

// ---------------------------------------------------------------- widgets ----

bool secondary_button(const char* value, const ImVec2& size);
bool window_button(const char* id, const Rect& r, bool close);
void draw_card(const Rect& r, const char* title, const char* description);

// ------------------------------------------------------------------ style ----

void apply_style();
void apply_accent();

} // namespace ui

// ------------------------------------------------------------------ screens ---

namespace screen_login {

// Loads the embedded logo texture, remembered username and startup check.
void Init();

// Releases the logo texture.
void Shutdown();

// Draws the startup, form or signing phase. Shrinks the window on entry.
void Render();

bool IsLoggedIn();

// Clears the session, the password field and returns to the login size.
void SignOut();

// Called when the background auth check reports that the session ended.
void SessionLost();

} // namespace screen_login

namespace screen_home {

// Draws the main dashboard.
void Render();

} // namespace screen_home
