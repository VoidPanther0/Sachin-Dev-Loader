#include "games.h"

#include "clock.h"
#include "json.h"
#include "paths.h"

#include <cstdio>
#include <cstdlib>

namespace games {

bool path_is_valid(const GameEntry& entry)
{
	return !entry.exe_path.empty() && paths::file_exists(entry.exe_path);
}

namespace {

std::string make_id()
{
	char buffer[48];
	std::snprintf(buffer, sizeof(buffer), "g_%llx_%04x",
		static_cast<unsigned long long>(clock_util::now_ms()), static_cast<unsigned int>(std::rand() & 0xFFFF));
	return buffer;
}

json::Value config_to_json(const GameConfig& config)
{
	json::Value v = json::Value::MakeObject();
	v.set("launch_arguments", config.launch_arguments);
	v.set("working_directory", config.working_directory);
	v.set("notes", config.notes);
	v.set("use_accent_override", config.use_accent_override);
	json::Value color = json::Value::MakeArray();
	color.push_back(config.accent_override[0]);
	color.push_back(config.accent_override[1]);
	color.push_back(config.accent_override[2]);
	v.set("accent_override", color);
	return v;
}

GameConfig config_from_json(const json::Value& v)
{
	GameConfig config;
	config.launch_arguments = v.get("launch_arguments").as_string();
	config.working_directory = v.get("working_directory").as_string();
	config.notes = v.get("notes").as_string();
	config.use_accent_override = v.get("use_accent_override").as_bool(false);
	const json::Array& color = v.get("accent_override").as_array();
	for (size_t i = 0; i < color.size() && i < 3; ++i)
		config.accent_override[i] = color[i].as_float(config.accent_override[i]);
	return config;
}

json::Value entry_to_json(const GameEntry& entry)
{
	json::Value v = json::Value::MakeObject();
	v.set("id", entry.id);
	v.set("name", entry.name);
	v.set("exe_path", entry.exe_path);
	v.set("icon_path", entry.icon_path);
	v.set("version", entry.version);
	v.set("favorite", entry.favorite);
	v.set("last_played_ms", static_cast<double>(entry.last_played_ms));
	v.set("config", config_to_json(entry.config));
	return v;
}

GameEntry entry_from_json(const json::Value& v)
{
	GameEntry entry;
	entry.id = v.get("id").as_string();
	entry.name = v.get("name").as_string();
	entry.exe_path = v.get("exe_path").as_string();
	entry.icon_path = v.get("icon_path").as_string();
	entry.version = v.get("version").as_string();
	entry.favorite = v.get("favorite").as_bool(false);
	entry.last_played_ms = static_cast<long long>(v.get("last_played_ms").as_double(0.0));
	entry.config = config_from_json(v.get("config"));
	return entry;
}

} // namespace

Library& Library::instance()
{
	static Library library;
	return library;
}

bool Library::load()
{
	entries_.clear();
	if (!paths::file_exists(paths::games_file()))
		return false;

	json::Value root;
	if (!json::load_file(paths::games_file(), root))
		return false;

	for (const json::Value& item : root.get("games").as_array())
		entries_.push_back(entry_from_json(item));

	return true;
}

bool Library::save(std::string* error) const
{
	json::Value root = json::Value::MakeObject();
	json::Value list = json::Value::MakeArray();
	for (const GameEntry& entry : entries_)
		list.push_back(entry_to_json(entry));
	root.set("games", list);
	return json::save_file(paths::games_file(), root, error);
}

GameEntry* Library::find(const std::string& id)
{
	for (GameEntry& entry : entries_)
		if (entry.id == id)
			return &entry;
	return nullptr;
}

std::string Library::add(const GameEntry& entry)
{
	GameEntry copy = entry;
	copy.id = make_id();
	entries_.push_back(copy);
	return copy.id;
}

bool Library::update(const std::string& id, const GameEntry& entry)
{
	GameEntry* existing = find(id);
	if (!existing)
		return false;
	GameEntry updated = entry;
	updated.id = id;
	*existing = updated;
	return true;
}

bool Library::remove(const std::string& id)
{
	for (size_t i = 0; i < entries_.size(); ++i)
	{
		if (entries_[i].id == id)
		{
			entries_.erase(entries_.begin() + static_cast<long>(i));
			return true;
		}
	}
	return false;
}

void Library::touch_last_played(const std::string& id)
{
	if (GameEntry* entry = find(id))
		entry->last_played_ms = clock_util::now_ms();
}

} // namespace games
