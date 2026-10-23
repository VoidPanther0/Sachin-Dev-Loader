#pragma once

#include <string>
#include <vector>

// DLL Management support: validates files the user points at and keeps a
// list of the user's own applications for reference. This build does not
// open, write to, or inject code into any other running process - it only
// reads the header of the DLL file the user selected on disk.
namespace dllmgr {

struct DllInfo
{
	std::string path;
	bool exists = false;
	bool is_valid_pe = false;
	bool is_dll = false;
	std::string architecture; // "x64", "x86", "ARM64", or "unknown"
	unsigned long long size_bytes = 0;
};

DllInfo inspect(const std::string& path);

struct Target
{
	std::string id;
	std::string display_name;
	std::string exe_name;
	std::string notes;
	long long created_ms = 0;
};

class TargetRegistry
{
public:
	static TargetRegistry& instance();

	bool load();
	bool save(std::string* error = nullptr) const;

	const std::vector<Target>& targets() const { return targets_; }
	std::string add(const std::string& display_name, const std::string& exe_name, const std::string& notes);
	bool remove(const std::string& id);

private:
	std::vector<Target> targets_;
};

} // namespace dllmgr
