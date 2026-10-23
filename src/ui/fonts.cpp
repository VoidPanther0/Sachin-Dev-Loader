#include "fonts.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace ui {

Fonts F;

namespace {

std::string font_path(const char* file)
{
	char directory[MAX_PATH] = {};
	const DWORD length = GetWindowsDirectoryA(directory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return std::string();

	std::string path = directory;
	path += "\\Fonts\\";
	path += file;

	if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return std::string();

	return path;
}

ImFont* load_font(const char* file, float size)
{
	const std::string path = font_path(file);
	if (!path.empty())
	{
		ImFontConfig config;
		config.OversampleH = 2;
		config.OversampleV = 2;
		config.PixelSnapH = true;

		if (ImFont* loaded = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config))
			return loaded;
	}

	return ImGui::GetIO().Fonts->AddFontDefault();
}

} // namespace

void LoadFonts()
{
	F.micro = load_font("segoeui.ttf", 10.0f);
	F.tiny = load_font("segoeui.ttf", 11.0f);
	F.small_font = load_font("segoeui.ttf", 13.0f);
	F.body = load_font("segoeui.ttf", 15.0f);
	F.hero = load_font("segoeuib.ttf", 40.0f);
	F.brand = load_font("segoeuib.ttf", 42.0f);
	F.brand_small = load_font("segoeuib.ttf", 22.0f);
	F.subtitle = load_font("segoeui.ttf", 19.0f);
	F.input = load_font("segoeui.ttf", 17.0f);
	F.button = load_font("segoeuib.ttf", 17.0f);
	F.link = load_font("segoeui.ttf", 14.0f);
}

} // namespace ui
