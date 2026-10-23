#pragma once

#include <string>
#include <vector>

namespace theme {

struct Color
{
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

struct Palette
{
	Color accent;
	Color accent2;
	Color window_bg_top;
	Color window_bg_bottom;
	Color panel_bg;
	Color field_bg;
	Color border;
	Color text;
	Color text_secondary;
	Color button;
	Color button_hover;
	Color button_active;
	Color success;
	Color warning;
	Color danger;
	Color info;
};

struct Style
{
	float rounding = 11.0f;
	float spacing = 12.0f;
	float window_padding = 24.0f;
	float font_scale = 1.0f;
	float anim_intensity = 1.0f;
	bool glow_enabled = true;
	float glow_intensity = 1.0f;
};

struct Theme
{
	std::string name;
	bool builtin = false;
	Palette palette;
	Style style;
};

class Manager
{
public:
	static Manager& instance();

	void load_all();
	const std::vector<Theme>& themes() const { return themes_; }
	const Theme& current() const;
	bool set_current(const std::string& name);

	bool save_custom(Theme theme, std::string* error = nullptr);
	bool delete_custom(const std::string& name);
	bool duplicate(const std::string& source_name, const std::string& new_name);
	bool import_file(const std::string& path, std::string* error = nullptr);
	bool export_file(const std::string& theme_name, const std::string& path, std::string* error = nullptr);
	void reset_to_default();

	Theme* find(const std::string& name);

private:
	std::vector<Theme> themes_;
	std::string current_name_ = "Purple";

	void load_custom_from_disk();
	void apply_persisted_selection();
};

} // namespace theme
