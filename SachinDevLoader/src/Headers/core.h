#pragma once
#include <d3d11.h>
#include <windows.h>
#include <dwmapi.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

inline HWND hwnd;
inline RECT rc;

namespace window {
	const ImVec2 size_max = { 625, 425 };
	const float rounding = 6.f;
}

namespace fonts {
	inline ImFont* Inter_Regular = nullptr;
	inline ImFont* Sansation_Light = nullptr;
	inline ImFont* Sansation_Regular = nullptr;
	inline ImFont* Sansation_Bold = nullptr;
	inline ImFont* FontAwesome = nullptr;
}

namespace items {
	const float rounding = 2.f;
}

class Loader {
public:
	class ui {
	public:
		void Render();
		void BeforeLoop();
	};

	class GUI
	{
	public:
		void begin(const char* name, ImVec2 size);
		void end();
		void mw();
		void style();
		void AddBorders(ImVec2 pos);
	};

	class CUSTOM
	{
	public:
		bool Button(const char* label, const char* text, float posX, float posY, float sizeX, float sizeY);
		bool InputText(const char* label, const char* hint, float posX, float posY, float sizeX, char buf[], size_t buf_size, ImGuiInputTextFlags flag);
		bool IconButton(const char* label, const char* icon, float posX, float posY, int action);
		void Text(float posX, float posY, ImFont* font, float size, const char* text, ImVec4 col);
		void ShadowText(float posX, float posY, ImFont* font, float size, const char* text, ImVec4 col, ImVec4 shadow_col);
	};
};

namespace colors {
	const ImVec4 bg = { 0.059f, 0.059f, 0.059f, .8f };
	const ImVec4 lbg = { 0.086f, 0.086f, 0.086f, .95f };

	const ImVec4 SecondColor = { 1.0f, 0.384f, 0.016f, .8f };
	const ImVec4 MainColor = { SecondColor.x, SecondColor.y, SecondColor.z, SecondColor.w * 3 };

	const ImVec4 White = { 1, 1, 1, 1 };
	const ImVec4 lwhite = { 1, 1, 1, 0.8f };
	const ImVec4 Gray = { 0.235f, 0.235f, 0.235f, 1 };
	const ImVec4 DarkGray = { 0.137f, 0.137f, 0.137f, 1 };
	const ImVec4 Green = { 0.247f, 1.0f, 0.247f, 1.0f };
	const ImVec4 Red = { 0.6824f, 0.1608f, 0.1608f, 1.0f };
}
