#pragma once

#include <string>
#include <vector>

namespace games {

struct GameConfig
{
	std::string launch_arguments;
	std::string working_directory;
	std::string notes;
	bool use_accent_override = false;
	float accent_override[3] = { 0.49f, 0.30f, 1.0f };
};

struct GameEntry
{
	std::string id;
	std::string name;
	std::string exe_path;
	std::string icon_path;
	std::string version;
	bool favorite = false;
	long long last_played_ms = 0;
	GameConfig config;
};

// True when exe_path points at a file that currently exists.
bool path_is_valid(const GameEntry& entry);

class Library
{
public:
	static Library& instance();

	bool load();
	bool save(std::string* error = nullptr) const;

	const std::vector<GameEntry>& entries() const { return entries_; }
	GameEntry* find(const std::string& id);

	std::string add(const GameEntry& entry);
	bool update(const std::string& id, const GameEntry& entry);
	bool remove(const std::string& id);
	void touch_last_played(const std::string& id);

private:
	std::vector<GameEntry> entries_;
};

} // namespace games
