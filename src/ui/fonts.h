#pragma once

#include "imgui.h"

namespace ui {

struct Fonts
{
	ImFont* micro = nullptr;
	ImFont* tiny = nullptr;
	ImFont* small_font = nullptr;
	ImFont* body = nullptr;
	ImFont* hero = nullptr;
	ImFont* brand = nullptr;
	ImFont* brand_small = nullptr;
	ImFont* subtitle = nullptr;
	ImFont* input = nullptr;
	ImFont* button = nullptr;
	ImFont* link = nullptr;
};

extern Fonts F;

void LoadFonts();

} // namespace ui
