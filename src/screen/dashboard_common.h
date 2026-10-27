#pragma once

#include "../ui.h"
#include "../auth/supabase_auth.h"

#include <string>

// Shared pieces of the dashboard: theme colors, layout, icons, small widgets
// and the per-session state. dashboard.cpp draws the window shell; every page
// has its own file (home.cpp, applications.cpp, downloads.cpp, dll_tools.cpp,
// theme.cpp, account.cpp, settings.cpp, about.cpp).

namespace dash {

// ------------------------------------------------------------------ theme ----

inline const ImVec4 kHomeBg(0.020f, 0.024f, 0.043f, 1.0f);
inline const ImVec4 kSidebar(0.031f, 0.039f, 0.063f, 1.0f);
inline const ImVec4 kCard(0.043f, 0.055f, 0.090f, 1.0f);
inline const ImVec4 kCardSoft(0.055f, 0.071f, 0.115f, 1.0f);
inline const ImVec4 kBorder(0.30f, 0.38f, 0.62f, 0.22f);
inline const ImVec4 kNavText(0.55f, 0.61f, 0.76f, 1.0f);
inline const ImVec4 kBlue(0.17f, 0.33f, 0.86f, 1.0f);
inline const ImVec4 kPurple(0.52f, 0.29f, 0.93f, 1.0f);
inline const ImVec4 kOrange(1.00f, 0.62f, 0.30f, 1.0f);

constexpr float kTitleHeight = 64.0f;
constexpr float kSidebarWidth = 232.0f;
constexpr float kContentLeft = 264.0f;
constexpr float kContentRight = 1132.0f;
constexpr float kContentTop = 152.0f;
constexpr float kContentBottom = 652.0f;

// Sidebar order. ui::state.page holds one of these.
enum Page
{
	PageHome = 0,
	PageApplications,
	PageDownloads,
	PageDllTools,
	PageTheme,
	PageAccount,
	PageSettings,
	PageAbout,
	PageCount,
};

struct AccentPreset { const char* name; float r, g, b; };

extern const AccentPreset kAccentPresets[];
extern const int kAccentPresetCount;

// ------------------------------------------------------------------ state ----

// Per-session dashboard state. Reset when a new session opens the dashboard.
struct Shared
{
	bool show_fps = true;
	bool background_glow = true;
	ULONGLONG session_start = 0;
	char search[64] = "";
	int selected = 0;
	double copied_time = -10.0;
	double diagnostics_time = -10.0;
	double restart_ready_time = -1.0;
};

extern Shared shared;

// settings.ini next to the remembered username.
std::wstring settings_path(bool create_directory);
void save_preferences();
void load_preferences();
void reset_preferences();

// ------------------------------------------------------------------ icons ----

void icon_home(ImVec2 c, ImU32 color);
void icon_apps(ImVec2 c, ImU32 color);
void icon_download(ImVec2 c, ImU32 color);
void icon_dll(ImVec2 c, ImU32 color);
void icon_theme(ImVec2 c, ImU32 color);
void icon_account(ImVec2 c, ImU32 color);
void icon_settings(ImVec2 c, ImU32 color);
void icon_about(ImVec2 c, ImU32 color);
void icon_logout(ImVec2 c, ImU32 color);
void icon_search(ImVec2 c, ImU32 color);

// ---------------------------------------------------------------- helpers ----

// Draws text wrapped at wrap_width and returns its height.
float text_wrapped(ImFont* f, float size, ImVec2 position, const ImVec4& color, const char* value, float wrap_width);

// Small square avatar with the first letter of a name.
void letter_badge(const ui::Rect& r, const std::string& name, float size);

bool contains_ci(const std::string& haystack, const char* needle);

// Component state: online, updating, offline or beta.
ImVec4 status_color(const std::string& status);
std::string status_label(const std::string& status);
bool component_launchable(const sdauth::Component& component);

// Update state of the loader itself. latest_version comes from loader_config.
const char* update_text();
ImVec4 update_color();
bool update_available();

// Days left until the account expires, or -1 for no expiry.
int access_days_left();
std::string access_date();

void open_url(const std::string& url);
void open_data_folder();
std::string short_hwid();
std::string session_time();

// Plain text a user can paste to support. No password, token or full device ID.
std::string diagnostics_text();

// Opens the Applications page on one component.
void open_component(int index);

// ---------------------------------------------------------------- widgets ----

void draw_page_header(const char* title, const char* subtitle);
void draw_empty_state(const ui::Rect& area, const char* title, const char* subtitle);
bool text_link(const char* id, const char* value, ImVec2 position);
void stat_card(const ui::Rect& r, const char* label, const char* value, const char* hint, const ImVec4& color);
void draw_search_box(const ui::Rect& r, const char* hint);

// Card with a title and an optional one-line description.
void card_frame(const ui::Rect& r, const char* title, const char* description);

// Label on the left, value on the right of a card row; draws a divider below.
void info_row(const ui::Rect& card, float y, const char* label, const char* value, const ImVec4& color);

// Switch with a label and a hint. Returns true when toggled.
bool toggle_row(const char* id, const ui::Rect& r, const char* label, const char* hint, bool* value);

// Secondary button that fills the given rect.
bool card_button(const char* label, const ui::Rect& r);

// Thin progress bar in dashboard colors.
void progress_bar(const ui::Rect& r, float fraction, const ImVec4& color);

// ------------------------------------------------------------------ pages ----

void DrawHome();
void DrawApplications();
void DrawDownloads();
void DrawDllTools();
void DrawTheme();
void DrawAccount();
void DrawSettings();
void DrawAbout();

// Called once per frame by the shell; ends a finished launch message.
void tick_launch();

} // namespace dash
