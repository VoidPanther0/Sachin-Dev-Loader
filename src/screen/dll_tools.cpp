#include "common/dashboard_common.h"

#include <commdlg.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#pragma comment(lib, "comdlg32.lib")

// DLL Tools: reads the PE header of a file you pick and shows what it is. This
// is a read-only check for your own apps and authorized tests. Nothing is
// loaded, started or injected.

namespace dash {

using namespace ui;

namespace {

struct PeInfo
{
	bool checked = false;
	bool ok = false;
	std::string error;
	std::string name;
	std::string path;
	std::string type;
	std::string arch;
	std::string subsystem;
	std::string timestamp;
	long long size = 0;
	int sections = 0;
	unsigned long entry_point = 0;
};

PeInfo pe;

std::string narrow(const std::wstring& value)
{
	if (value.empty())
		return std::string();
	const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
		nullptr, 0, nullptr, nullptr);
	if (length <= 0)
		return std::string();
	std::string out(static_cast<std::size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &out[0], length, nullptr, nullptr);
	return out;
}

std::wstring pick_file()
{
	wchar_t buffer[MAX_PATH] = {};
	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = g_hwnd;
	dialog.lpstrFilter = L"DLL and EXE files\0*.dll;*.exe\0All files\0*.*\0\0";
	dialog.lpstrFile = buffer;
	dialog.nMaxFile = MAX_PATH;
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetOpenFileNameW(&dialog) ? std::wstring(buffer) : std::wstring();
}

const char* subsystem_name(unsigned short value)
{
	switch (value)
	{
	case IMAGE_SUBSYSTEM_NATIVE: return "Native";
	case IMAGE_SUBSYSTEM_WINDOWS_GUI: return "Windows GUI";
	case IMAGE_SUBSYSTEM_WINDOWS_CUI: return "Console";
	case IMAGE_SUBSYSTEM_EFI_APPLICATION: return "EFI application";
	default: return "Unknown";
	}
}

void fail(const char* message)
{
	pe.ok = false;
	pe.error = message;
}

// Reads the first 64 KB and walks the DOS and NT headers with bounds checks.
void inspect(const std::wstring& path)
{
	pe = PeInfo();
	pe.checked = true;
	pe.path = narrow(path);
	const std::size_t slash = path.find_last_of(L"\\/");
	pe.name = narrow(slash == std::wstring::npos ? path : path.substr(slash + 1));

	HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
	{
		fail("The file could not be opened.");
		return;
	}

	LARGE_INTEGER size = {};
	GetFileSizeEx(file, &size);
	pe.size = size.QuadPart;

	std::vector<unsigned char> head(65536);
	DWORD read = 0;
	const BOOL read_ok = ReadFile(file, head.data(), static_cast<DWORD>(head.size()), &read, nullptr);
	CloseHandle(file);
	if (!read_ok || read < sizeof(IMAGE_DOS_HEADER))
	{
		fail("The file is too small to be a PE file.");
		return;
	}
	head.resize(read);

	const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(head.data());
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		fail("Not a PE file (no MZ header).");
		return;
	}

	const std::size_t nt_offset = static_cast<std::size_t>(dos->e_lfanew);
	if (dos->e_lfanew <= 0 || nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + 2 > head.size())
	{
		fail("The PE header is outside the readable range.");
		return;
	}

	const DWORD signature = *reinterpret_cast<const DWORD*>(head.data() + nt_offset);
	if (signature != IMAGE_NT_SIGNATURE)
	{
		fail("Invalid PE signature.");
		return;
	}

	const IMAGE_FILE_HEADER* header = reinterpret_cast<const IMAGE_FILE_HEADER*>(head.data() + nt_offset + sizeof(DWORD));
	switch (header->Machine)
	{
	case IMAGE_FILE_MACHINE_AMD64: pe.arch = "x64"; break;
	case IMAGE_FILE_MACHINE_I386: pe.arch = "x86"; break;
	case IMAGE_FILE_MACHINE_ARM64: pe.arch = "ARM64"; break;
	default:
	{
		char value[24];
		std::snprintf(value, sizeof(value), "0x%04X", static_cast<unsigned>(header->Machine));
		pe.arch = value;
		break;
	}
	}

	pe.type = (header->Characteristics & IMAGE_FILE_DLL) ? "DLL" : "Executable";
	pe.sections = header->NumberOfSections;

	const std::time_t stamp = static_cast<std::time_t>(header->TimeDateStamp);
	std::tm parts = {};
	char when[32] = "-";
	if (header->TimeDateStamp != 0 && gmtime_s(&parts, &stamp) == 0)
		std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M UTC", &parts);
	pe.timestamp = when;

	// The optional header starts right after the file header; its size field
	// layout differs between PE32 and PE32+, but the common prefix is the same.
	const std::size_t optional_offset = nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
	if (header->SizeOfOptionalHeader >= 70 && optional_offset + 70 <= head.size())
	{
		const unsigned char* optional = head.data() + optional_offset;
		pe.entry_point = *reinterpret_cast<const DWORD*>(optional + 16);
		pe.subsystem = subsystem_name(*reinterpret_cast<const unsigned short*>(optional + 68));
	}
	else
	{
		pe.subsystem = "Unknown";
	}

	pe.ok = true;
}

void value_row(const Rect& card, float y, const char* label, const std::string& value)
{
	info_row(card, y, label, value.c_str(), kText);
}

} // namespace

void DrawDllTools()
{
	draw_page_header("DLL Tools", "Check a DLL or EXE file. Read-only, for your own apps.");

	// Left: picker.
	const Rect picker = rect(kContentLeft, kContentTop, 600.0f, kContentBottom);
	card_frame(picker, "Validate a file", "Reads the PE header only. Nothing is loaded or started.");

	const Rect drop = rect(picker.min.x + 28.0f, picker.min.y + 84.0f, picker.max.x - 28.0f, picker.min.y + 236.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRect(drop.min, drop.max, color_u32(fade(accent_color(), 0.35f)), 12.0f, 0, 1.2f);
	draw->AddRectFilled(drop.min, drop.max, color_u32(fade(accent_color(), 0.04f)), 12.0f);

	const ImVec2 c = center(drop);
	icon_dll(ImVec2(c.x, c.y - 30.0f), color_u32(lift(accent_color(), 0.15f)));
	const char* hint = pe.checked ? pe.name.c_str() : "No file selected";
	const float hint_w = text_width(font.semibold, 15.0f, hint);
	text(font.semibold, 15.0f, ImVec2(c.x - hint_w * 0.5f, c.y - 2.0f), kText, hint);

	const char* sub = "Supports .dll and .exe files";
	const float sub_w = text_width(font.caption, 11.0f, sub);
	text(font.caption, 11.0f, ImVec2(c.x - sub_w * 0.5f, c.y + 22.0f), kMuted, sub);

	ImGui::SetCursorScreenPos(ImVec2(picker.min.x + 28.0f, picker.min.y + 256.0f));
	if (ImGui::Button("Choose file...", ImVec2(160.0f, 38.0f)))
	{
		const std::wstring path = pick_file();
		if (!path.empty())
			inspect(path);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	if (pe.checked)
	{
		ImGui::SetCursorScreenPos(ImVec2(picker.min.x + 204.0f, picker.min.y + 256.0f));
		if (secondary_button("Clear", ImVec2(100.0f, 38.0f)))
			pe = PeInfo();
	}

	text(font.caption, 10.0f, ImVec2(picker.min.x + 28.0f, picker.min.y + 320.0f), kDim, "NOTE");
	text_wrapped(font.body, 13.0f, ImVec2(picker.min.x + 28.0f, picker.min.y + 340.0f), kMuted,
		"Use this to confirm a file is a valid 32-bit or 64-bit DLL before you ship or load it in your own "
		"application. It does not inject or start anything.", width(picker) - 56.0f);

	// Right: result.
	const Rect result = rect(616.0f, kContentTop, kContentRight, kContentBottom);
	card_frame(result, "Result", pe.checked ? "PE header details" : "Choose a file to see its details.");

	if (!pe.checked)
	{
		draw_empty_state(rect(result.min.x + 28.0f, result.min.y + 84.0f, result.max.x - 28.0f, result.max.y - 28.0f),
			"Nothing checked", "Pick a DLL or EXE on the left.");
		return;
	}

	pill(rect(result.max.x - 28.0f - 96.0f, result.min.y + 22.0f, result.max.x - 28.0f, result.min.y + 44.0f),
		pe.ok ? "VALID PE" : "INVALID", pe.ok ? kGreen : kRed);

	if (!pe.ok)
	{
		text_wrapped(font.body, 14.0f, ImVec2(result.min.x + 28.0f, result.min.y + 96.0f), kRed,
			pe.error.c_str(), width(result) - 56.0f);
		return;
	}

	char size_text[48];
	if (pe.size >= 1024LL * 1024LL)
		std::snprintf(size_text, sizeof(size_text), "%.2f MB", static_cast<double>(pe.size) / (1024.0 * 1024.0));
	else
		std::snprintf(size_text, sizeof(size_text), "%.1f KB", static_cast<double>(pe.size) / 1024.0);

	char sections[16];
	std::snprintf(sections, sizeof(sections), "%d", pe.sections);
	char entry[24];
	std::snprintf(entry, sizeof(entry), "0x%08lX", pe.entry_point);

	float y = result.min.y + 92.0f;
	value_row(result, y, "NAME", pe.name);
	y += 38.0f;
	value_row(result, y, "TYPE", pe.type);
	y += 38.0f;
	value_row(result, y, "ARCHITECTURE", pe.arch);
	y += 38.0f;
	value_row(result, y, "SIZE", size_text);
	y += 38.0f;
	value_row(result, y, "SECTIONS", sections);
	y += 38.0f;
	value_row(result, y, "SUBSYSTEM", pe.subsystem);
	y += 38.0f;
	value_row(result, y, "ENTRY POINT", entry);
	y += 38.0f;
	value_row(result, y, "BUILT", pe.timestamp);
}

} // namespace dash
