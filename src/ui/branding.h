#pragma once

#include "imgui.h"

namespace ui {

void draw_logo(ImVec2 c, float size, float opacity = 1.0f);
float draw_wordmark_left(ImFont* wordmark_font, float x, float y, float size, float opacity);
void draw_wordmark_center(float cx, float y, float size, float opacity);
void draw_background(float w, float h, bool grid);

} // namespace ui
