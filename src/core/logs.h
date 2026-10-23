#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace logs {

enum class Level
{
	Info,
	Success,
	Warning,
	Error
};

struct Entry
{
	long long timestamp_ms = 0;
	Level level = Level::Info;
	std::string text;
};

const char* level_name(Level level);
std::string format_timestamp(long long timestamp_ms);

// Thread-safe. Call Add from any thread (DLL operations run off the UI thread);
// the UI thread reads a stable snapshot each frame with Snapshot().
class Logger
{
public:
	static Logger& instance();

	void add(Level level, const std::string& text);
	std::vector<Entry> snapshot() const;
	void clear();
	bool export_to_file(const std::string& path, std::string* error = nullptr) const;

private:
	mutable std::mutex mutex_;
	std::vector<Entry> entries_;
};

inline void info(const std::string& text) { Logger::instance().add(Level::Info, text); }
inline void success(const std::string& text) { Logger::instance().add(Level::Success, text); }
inline void warning(const std::string& text) { Logger::instance().add(Level::Warning, text); }
inline void error(const std::string& text) { Logger::instance().add(Level::Error, text); }

} // namespace logs
