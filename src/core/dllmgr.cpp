#include "dllmgr.h"

#include "clock.h"
#include "json.h"
#include "paths.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace dllmgr {

DllInfo inspect(const std::string& path)
{
	DllInfo info;
	info.path = path;
	info.exists = paths::file_exists(path);
	if (!info.exists)
		return info;

	HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return info;

	LARGE_INTEGER size{};
	if (GetFileSizeEx(file, &size))
		info.size_bytes = static_cast<unsigned long long>(size.QuadPart);

	std::vector<unsigned char> header(4096, 0);
	DWORD read = 0;
	const bool ok = ReadFile(file, header.data(), static_cast<DWORD>(header.size()), &read, nullptr) != 0;
	CloseHandle(file);
	if (!ok || read < sizeof(IMAGE_DOS_HEADER))
		return info;

	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(header.data());
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return info;

	const long nt_offset = dos->e_lfanew;
	if (nt_offset < 0 || static_cast<size_t>(nt_offset) + sizeof(IMAGE_NT_HEADERS) > read)
		return info;

	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(header.data() + nt_offset);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return info;

	info.is_valid_pe = true;
	info.is_dll = (nt->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0;

	switch (nt->FileHeader.Machine)
	{
	case IMAGE_FILE_MACHINE_AMD64: info.architecture = "x64"; break;
	case IMAGE_FILE_MACHINE_I386: info.architecture = "x86"; break;
	case IMAGE_FILE_MACHINE_ARM64: info.architecture = "ARM64"; break;
	default: info.architecture = "unknown"; break;
	}

	return info;
}

namespace {

std::string make_id()
{
	char buffer[48];
	std::snprintf(buffer, sizeof(buffer), "t_%llx_%04x",
		static_cast<unsigned long long>(clock_util::now_ms()), static_cast<unsigned int>(std::rand() & 0xFFFF));
	return buffer;
}

} // namespace

TargetRegistry& TargetRegistry::instance()
{
	static TargetRegistry registry;
	return registry;
}

bool TargetRegistry::load()
{
	targets_.clear();
	if (!paths::file_exists(paths::authorized_targets_file()))
		return false;

	json::Value root;
	if (!json::load_file(paths::authorized_targets_file(), root))
		return false;

	for (const json::Value& item : root.get("targets").as_array())
	{
		Target target;
		target.id = item.get("id").as_string();
		target.display_name = item.get("display_name").as_string();
		target.exe_name = item.get("exe_name").as_string();
		target.notes = item.get("notes").as_string();
		target.created_ms = static_cast<long long>(item.get("created_ms").as_double(0.0));
		if (!target.id.empty())
			targets_.push_back(target);
	}
	return true;
}

bool TargetRegistry::save(std::string* error) const
{
	json::Value root = json::Value::MakeObject();
	json::Value list = json::Value::MakeArray();
	for (const Target& target : targets_)
	{
		json::Value entry = json::Value::MakeObject();
		entry.set("id", target.id);
		entry.set("display_name", target.display_name);
		entry.set("exe_name", target.exe_name);
		entry.set("notes", target.notes);
		entry.set("created_ms", static_cast<double>(target.created_ms));
		list.push_back(entry);
	}
	root.set("targets", list);
	return json::save_file(paths::authorized_targets_file(), root, error);
}

std::string TargetRegistry::add(const std::string& display_name, const std::string& exe_name, const std::string& notes)
{
	Target target;
	target.id = make_id();
	target.display_name = display_name;
	target.exe_name = exe_name;
	target.notes = notes;
	target.created_ms = clock_util::now_ms();
	targets_.push_back(target);
	return target.id;
}

bool TargetRegistry::remove(const std::string& id)
{
	for (size_t i = 0; i < targets_.size(); ++i)
	{
		if (targets_[i].id == id)
		{
			targets_.erase(targets_.begin() + static_cast<long>(i));
			return true;
		}
	}
	return false;
}

} // namespace dllmgr
