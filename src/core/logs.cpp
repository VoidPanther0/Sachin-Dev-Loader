#include "logs.h"

#include "clock.h"

#include <cstdio>
#include <ctime>
#include <fstream>

namespace logs {

const char* level_name(Level level)
{
	switch (level)
	{
	case Level::Info: return "INFO";
	case Level::Success: return "SUCCESS";
	case Level::Warning: return "WARNING";
	case Level::Error: return "ERROR";
	}
	return "INFO";
}

std::string format_timestamp(long long timestamp_ms)
{
	const time_t seconds = static_cast<time_t>(timestamp_ms / 1000);
	std::tm local{};
	localtime_s(&local, &seconds);

	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
		local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		local.tm_hour, local.tm_min, local.tm_sec);
	return buffer;
}

namespace {

constexpr size_t kMaxEntries = 2000;

} // namespace

Logger& Logger::instance()
{
	static Logger logger;
	return logger;
}

void Logger::add(Level level, const std::string& text)
{
	std::lock_guard<std::mutex> lock(mutex_);
	entries_.push_back(Entry{ clock_util::now_ms(), level, text });
	if (entries_.size() > kMaxEntries)
		entries_.erase(entries_.begin(), entries_.begin() + static_cast<long>(entries_.size() - kMaxEntries));
}

std::vector<Entry> Logger::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return entries_;
}

void Logger::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	entries_.clear();
}

bool Logger::export_to_file(const std::string& path, std::string* error) const
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		if (error) *error = "could not open file for writing";
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex_);
	for (const Entry& entry : entries_)
	{
		file << "[" << format_timestamp(entry.timestamp_ms) << "] "
			<< level_name(entry.level) << "  " << entry.text << "\r\n";
	}
	return file.good();
}

} // namespace logs
