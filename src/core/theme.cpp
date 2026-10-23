#include "theme.h"

#include "json.h"
#include "paths.h"
#include "settings.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace theme {

namespace {

Color rgb(int r, int g, int b, int a = 255)
{
	return Color{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
}

Color lighten(const Color& c, float amount)
{
	return Color{
		std::min(1.0f, c.r + amount), std::min(1.0f, c.g + amount),
		std::min(1.0f, c.b + amount), c.a };
}

Color darken(const Color& c, float amount)
{
	return Color{
		std::max(0.0f, c.r - amount), std::max(0.0f, c.g - amount),
		std::max(0.0f, c.b - amount), c.a };
}

Theme make_dark_theme(const std::string& name, Color accent, Color accent2)
{
	Theme t;
	t.name = name;
	t.builtin = true;
	t.palette.accent = accent;
	t.palette.accent2 = accent2;
	t.palette.window_bg_top = rgb(6, 11, 26);
	t.palette.window_bg_bottom = rgb(4, 7, 18);
	t.palette.panel_bg = rgb(11, 19, 39, 190);
	t.palette.field_bg = rgb(8, 14, 30, 220);
	t.palette.border = rgb(120, 148, 220, 46);
	t.palette.text = rgb(238, 242, 255);
	t.palette.text_secondary = rgb(138, 150, 180);
	t.palette.button = accent;
	t.palette.button_hover = lighten(accent, 0.12f);
	t.palette.button_active = darken(accent, 0.10f);
	t.palette.success = rgb(66, 230, 164);
	t.palette.warning = rgb(250, 204, 21);
	t.palette.danger = rgb(255, 90, 118);
	t.palette.info = accent2;
	return t;
}

Theme make_light_theme()
{
	Theme t;
	t.name = "Light";
	t.builtin = true;
	t.palette.accent = rgb(124, 77, 255);
	t.palette.accent2 = rgb(59, 130, 246);
	t.palette.window_bg_top = rgb(246, 248, 252);
	t.palette.window_bg_bottom = rgb(233, 237, 245);
	t.palette.panel_bg = rgb(255, 255, 255, 235);
	t.palette.field_bg = rgb(255, 255, 255, 245);
	t.palette.border = rgb(30, 41, 59, 36);
	t.palette.text = rgb(15, 23, 42);
	t.palette.text_secondary = rgb(100, 116, 139);
	t.palette.button = t.palette.accent;
	t.palette.button_hover = lighten(t.palette.accent, 0.10f);
	t.palette.button_active = darken(t.palette.accent, 0.10f);
	t.palette.success = rgb(22, 163, 74);
	t.palette.warning = rgb(217, 119, 6);
	t.palette.danger = rgb(220, 38, 38);
	t.palette.info = rgb(37, 99, 235);
	return t;
}

std::vector<Theme> make_builtins()
{
	std::vector<Theme> list;
	list.push_back(make_dark_theme("Purple", rgb(124, 77, 255), rgb(59, 130, 246)));
	list.push_back(make_dark_theme("Blue", rgb(59, 130, 246), rgb(56, 189, 248)));
	list.push_back(make_dark_theme("Green", rgb(34, 197, 94), rgb(16, 185, 129)));
	list.push_back(make_dark_theme("Red", rgb(239, 68, 68), rgb(249, 115, 22)));
	list.push_back(make_light_theme());
	return list;
}

json::Value color_to_json(const Color& c)
{
	json::Value v = json::Value::MakeArray();
	v.push_back(static_cast<double>(c.r));
	v.push_back(static_cast<double>(c.g));
	v.push_back(static_cast<double>(c.b));
	v.push_back(static_cast<double>(c.a));
	return v;
}

Color color_from_json(const json::Value& v, const Color& fallback)
{
	const json::Array& a = v.as_array();
	if (a.size() < 3)
		return fallback;

	Color c = fallback;
	c.r = std::clamp(a[0].as_float(fallback.r), 0.0f, 1.0f);
	c.g = std::clamp(a[1].as_float(fallback.g), 0.0f, 1.0f);
	c.b = std::clamp(a[2].as_float(fallback.b), 0.0f, 1.0f);
	c.a = a.size() > 3 ? std::clamp(a[3].as_float(fallback.a), 0.0f, 1.0f) : 1.0f;
	return c;
}

json::Value theme_to_json(const Theme& t)
{
	json::Value root = json::Value::MakeObject();
	root.set("name", t.name);

	json::Value palette = json::Value::MakeObject();
	palette.set("accent", color_to_json(t.palette.accent));
	palette.set("accent2", color_to_json(t.palette.accent2));
	palette.set("window_bg_top", color_to_json(t.palette.window_bg_top));
	palette.set("window_bg_bottom", color_to_json(t.palette.window_bg_bottom));
	palette.set("panel_bg", color_to_json(t.palette.panel_bg));
	palette.set("field_bg", color_to_json(t.palette.field_bg));
	palette.set("border", color_to_json(t.palette.border));
	palette.set("text", color_to_json(t.palette.text));
	palette.set("text_secondary", color_to_json(t.palette.text_secondary));
	palette.set("button", color_to_json(t.palette.button));
	palette.set("button_hover", color_to_json(t.palette.button_hover));
	palette.set("button_active", color_to_json(t.palette.button_active));
	palette.set("success", color_to_json(t.palette.success));
	palette.set("warning", color_to_json(t.palette.warning));
	palette.set("danger", color_to_json(t.palette.danger));
	palette.set("info", color_to_json(t.palette.info));
	root.set("palette", palette);

	json::Value style = json::Value::MakeObject();
	style.set("rounding", static_cast<double>(t.style.rounding));
	style.set("spacing", static_cast<double>(t.style.spacing));
	style.set("window_padding", static_cast<double>(t.style.window_padding));
	style.set("font_scale", static_cast<double>(t.style.font_scale));
	style.set("anim_intensity", static_cast<double>(t.style.anim_intensity));
	style.set("glow_enabled", t.style.glow_enabled);
	style.set("glow_intensity", static_cast<double>(t.style.glow_intensity));
	root.set("style", style);

	return root;
}

bool theme_from_json(const json::Value& root, Theme& out, std::string* error = nullptr)
{
	const std::string name = root.get("name").as_string();
	if (name.empty())
	{
		if (error) *error = "theme file is missing a \"name\" field";
		return false;
	}

	const Theme fallback = make_dark_theme("__fallback", rgb(124, 77, 255), rgb(59, 130, 246));
	const json::Value& palette = root.get("palette");
	const json::Value& style = root.get("style");

	out = Theme{};
	out.name = name;
	out.builtin = false;

	out.palette.accent = color_from_json(palette.get("accent"), fallback.palette.accent);
	out.palette.accent2 = color_from_json(palette.get("accent2"), fallback.palette.accent2);
	out.palette.window_bg_top = color_from_json(palette.get("window_bg_top"), fallback.palette.window_bg_top);
	out.palette.window_bg_bottom = color_from_json(palette.get("window_bg_bottom"), fallback.palette.window_bg_bottom);
	out.palette.panel_bg = color_from_json(palette.get("panel_bg"), fallback.palette.panel_bg);
	out.palette.field_bg = color_from_json(palette.get("field_bg"), fallback.palette.field_bg);
	out.palette.border = color_from_json(palette.get("border"), fallback.palette.border);
	out.palette.text = color_from_json(palette.get("text"), fallback.palette.text);
	out.palette.text_secondary = color_from_json(palette.get("text_secondary"), fallback.palette.text_secondary);
	out.palette.button = color_from_json(palette.get("button"), fallback.palette.button);
	out.palette.button_hover = color_from_json(palette.get("button_hover"), fallback.palette.button_hover);
	out.palette.button_active = color_from_json(palette.get("button_active"), fallback.palette.button_active);
	out.palette.success = color_from_json(palette.get("success"), fallback.palette.success);
	out.palette.warning = color_from_json(palette.get("warning"), fallback.palette.warning);
	out.palette.danger = color_from_json(palette.get("danger"), fallback.palette.danger);
	out.palette.info = color_from_json(palette.get("info"), fallback.palette.info);

	out.style.rounding = std::clamp(style.get("rounding").as_float(11.0f), 0.0f, 24.0f);
	out.style.spacing = std::clamp(style.get("spacing").as_float(12.0f), 0.0f, 32.0f);
	out.style.window_padding = std::clamp(style.get("window_padding").as_float(24.0f), 0.0f, 64.0f);
	out.style.font_scale = std::clamp(style.get("font_scale").as_float(1.0f), 0.75f, 1.5f);
	out.style.anim_intensity = std::clamp(style.get("anim_intensity").as_float(1.0f), 0.0f, 2.0f);
	out.style.glow_enabled = style.get("glow_enabled").as_bool(true);
	out.style.glow_intensity = std::clamp(style.get("glow_intensity").as_float(1.0f), 0.0f, 2.0f);

	return true;
}

std::string slug_of(const std::string& name)
{
	std::string result;
	for (char c : name)
		result.push_back(std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '_');
	if (result.empty())
		result = "theme";
	return result;
}

bool ieq(const std::string& a, const std::string& b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	return true;
}

} // namespace

Manager& Manager::instance()
{
	static Manager manager;
	return manager;
}

Theme* Manager::find(const std::string& name)
{
	for (Theme& t : themes_)
		if (ieq(t.name, name))
			return &t;
	return nullptr;
}

void Manager::load_custom_from_disk()
{
	WIN32_FIND_DATAA data{};
	const std::string pattern = paths::join(paths::themes_dir(), "*.json");
	HANDLE handle = FindFirstFileA(pattern.c_str(), &data);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		json::Value root;
		if (!json::load_file(paths::join(paths::themes_dir(), data.cFileName), root))
			continue;

		Theme t;
		if (theme_from_json(root, t) && !find(t.name))
			themes_.push_back(t);
	} while (FindNextFileA(handle, &data));

	FindClose(handle);
}

void Manager::apply_persisted_selection()
{
	const std::string wanted = settings::current().theme_name;
	if (!wanted.empty() && find(wanted))
		current_name_ = wanted;
	else
		current_name_ = "Purple";
}

void Manager::load_all()
{
	themes_ = make_builtins();
	load_custom_from_disk();
	apply_persisted_selection();
}

const Theme& Manager::current() const
{
	for (const Theme& t : themes_)
		if (ieq(t.name, current_name_))
			return t;
	return themes_.front();
}

bool Manager::set_current(const std::string& name)
{
	if (!find(name))
		return false;
	current_name_ = name;
	settings::current().theme_name = name;
	settings::save();
	return true;
}

bool Manager::save_custom(Theme t, std::string* error)
{
	for (const Theme& existing : themes_)
	{
		if (existing.builtin && ieq(existing.name, t.name))
		{
			if (error) *error = "A built-in theme already uses that name.";
			return false;
		}
	}

	t.builtin = false;
	const std::string file = paths::join(paths::themes_dir(), slug_of(t.name) + ".json");
	if (!json::save_file(file, theme_to_json(t), error))
		return false;

	if (Theme* existing = find(t.name))
		*existing = t;
	else
		themes_.push_back(t);

	return true;
}

bool Manager::delete_custom(const std::string& name)
{
	for (size_t i = 0; i < themes_.size(); ++i)
	{
		if (themes_[i].builtin || !ieq(themes_[i].name, name))
			continue;

		DeleteFileA(paths::join(paths::themes_dir(), slug_of(themes_[i].name) + ".json").c_str());
		themes_.erase(themes_.begin() + static_cast<long>(i));
		if (ieq(current_name_, name))
			reset_to_default();
		return true;
	}
	return false;
}

bool Manager::duplicate(const std::string& source_name, const std::string& new_name)
{
	const Theme* source = find(source_name);
	if (!source || find(new_name))
		return false;

	Theme copy = *source;
	copy.name = new_name;
	copy.builtin = false;
	return save_custom(copy);
}

bool Manager::import_file(const std::string& path, std::string* error)
{
	json::Value root;
	if (!json::load_file(path, root, error))
		return false;

	Theme t;
	if (!theme_from_json(root, t, error))
		return false;

	std::string unique_name = t.name;
	int suffix = 2;
	while (true)
	{
		const Theme* existing = find(unique_name);
		if (!existing) break;
		if (existing->builtin)
		{
			unique_name = t.name + " (imported)";
			if (!find(unique_name)) break;
		}
		unique_name = t.name + " (" + std::to_string(suffix++) + ")";
	}
	t.name = unique_name;

	return save_custom(t, error);
}

bool Manager::export_file(const std::string& theme_name, const std::string& path, std::string* error)
{
	const Theme* t = find(theme_name);
	if (!t)
	{
		if (error) *error = "theme not found";
		return false;
	}
	return json::save_file(path, theme_to_json(*t), error);
}

void Manager::reset_to_default()
{
	set_current("Purple");
}

} // namespace theme
