#include <map>

#include "core.h"
#include "settings.h"

namespace {
	const ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
}

void Loader::GUI::begin(const char* name, ImVec2 size)
{
	ImGuiStyle& s = ImGui::GetStyle();

	s.WindowRounding = window::rounding;
	s.Alpha = 1.f;

	ImGui::Begin(name, nullptr, window_flags);
	ImGui::SetWindowSize(size);

	ImGui::GetWindowDrawList()->AddRectFilled(
		{ 0, 0 }, ImVec2(size.x - 1, size.y - 1),
		ImGui::GetColorU32(colors::bg), window::rounding);

	style();
	mw();
}

void Loader::GUI::end()
{
	ImGui::End();
}

void Loader::GUI::style()
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowBorderSize = 0.f;
	style.WindowRounding = window::rounding;
	style.ChildRounding = window::rounding;
	style.FrameBorderSize = 0.f;
	style.FrameRounding = items::rounding;
	style.WindowPadding = ImVec2(0, 0);
	style.Colors[ImGuiCol_ChildBg] = colors::lbg;
	style.ScrollbarSize = 5.f;
	style.ScrollbarRounding = 10;
	style.ItemSpacing = { 10, 15 };
}

void Loader::GUI::mw()
{
	if (!hwnd || IsIconic(hwnd))
		return;

	GetWindowRect(hwnd, &rc);
	MoveWindow(hwnd, rc.left + static_cast<int>(ImGui::GetWindowPos().x),
		rc.top + static_cast<int>(ImGui::GetWindowPos().y),
		static_cast<int>(window::size_max.x), static_cast<int>(window::size_max.y), TRUE);
	ImGui::SetWindowPos(ImVec2(0.f, 0.f));
}

// Loader::CUSTOM ITEMS

struct ButtonState {
	ImVec4 ButtonColor;
	ImVec4 ShadowColor;
	ImVec4 TextColor;
	ButtonState() : ButtonColor(0, 0, 0, 0), ShadowColor(0, 0, 0, 0), TextColor(0, 0, 0, 0) {}
};

bool Loader::CUSTOM::Button(const char* label, const char* text, float posX, float posY, float sizeX, float sizeY)
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiIO& io = ImGui::GetIO();
	style.Alpha = alphaColor;

	ImGui::SetCursorPos({ posX, posY });
	bool result = ImGui::InvisibleButton(label, { sizeX, sizeY });

	ImGuiID id = window->GetID(label);
	static std::map<ImGuiID, ButtonState> anim;
	auto it_anim = anim.find(id);

	if (it_anim == anim.end()) {
		anim.insert({ id, ButtonState() });
		it_anim = anim.find(id);
	}

	float time = io.DeltaTime;

	ImVec4 target = ImGui::IsItemActive() ? colors::MainColor : ImGui::IsItemHovered() ? colors::MainColor : colors::SecondColor;
	ImVec4 ShadowColorTarget = ImGui::IsItemActive() ? colors::MainColor : ImGui::IsItemHovered() ? colors::SecondColor : ImVec4(0, 0, 0, 0);
	ImVec4 targetText = ImGui::IsItemActive() || ImGui::IsItemHovered() ? colors::White : colors::lwhite;

	it_anim->second.ButtonColor = ImVec4(
		ImLerp(it_anim->second.ButtonColor.x, target.x, time * 6),
		ImLerp(it_anim->second.ButtonColor.y, target.y, time * 6),
		ImLerp(it_anim->second.ButtonColor.z, target.z, time * 6),
		ImLerp(it_anim->second.ButtonColor.w, target.w, time * 6));
	it_anim->second.TextColor = ImVec4(
		ImLerp(it_anim->second.TextColor.x, targetText.x, time * 6),
		ImLerp(it_anim->second.TextColor.y, targetText.y, time * 6),
		ImLerp(it_anim->second.TextColor.z, targetText.z, time * 6),
		ImLerp(it_anim->second.TextColor.w, targetText.w, time * 6));
	it_anim->second.ShadowColor = ImVec4(
		ImLerp(it_anim->second.ShadowColor.x, ShadowColorTarget.x, time * 6),
		ImLerp(it_anim->second.ShadowColor.y, ShadowColorTarget.y, time * 6),
		ImLerp(it_anim->second.ShadowColor.z, ShadowColorTarget.z, time * 6),
		ImLerp(it_anim->second.ShadowColor.w, ShadowColorTarget.w / 2, time * 6));

	ImVec2 MIN = ImGui::GetItemRectMin();
	ImVec2 MAX = ImGui::GetItemRectMax();

	window->DrawList->AddRectFilled(MIN, MAX, ImGui::GetColorU32(it_anim->second.ButtonColor), items::rounding, 0);
	window->DrawList->AddShadowRect(MIN, MAX, ImGui::GetColorU32(it_anim->second.ShadowColor), 50, ImVec2(0, 0), 0, 3.f);

	if (text) {
		ImVec2 textSize = ImGui::CalcTextSize(text);
		ImVec2 textPos = { MIN.x + (sizeX - textSize.x) * 0.5f, MIN.y + (sizeY - textSize.y) * 0.5f };
		window->DrawList->AddText(textPos, ImGui::GetColorU32(it_anim->second.TextColor), text);
	}

	return result;
}

bool Loader::CUSTOM::InputText(const char* label, const char* hint, float posX, float posY, float sizeX, char buf[], std::size_t buf_size, ImGuiInputTextFlags flag)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiIO& io = ImGui::GetIO();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 5));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
	ImGui::PushFont(fonts::Inter_Regular);

	ImGui::SetNextItemWidth(sizeX);
	ImGui::SetCursorPos({ posX, posY });

	ImGuiID id = window->GetID(label);
	static std::map<ImGuiID, ImVec4> text_colors;
	auto it_color = text_colors.find(id);
	if (it_color == text_colors.end()) {
		text_colors.insert({ id, colors::Gray });
		it_color = text_colors.find(id);
	}

	ImGui::PushStyleColor(ImGuiCol_TextDisabled, it_color->second);

	ImVec2 MIN = ImVec2(posX, posY);
	ImVec2 MAX = ImVec2(posX + sizeX, posY + ImGui::GetFontSize() + 5.f * 2.f);

	window->DrawList->AddLine({ MIN.x, MAX.y }, { MAX.x, MAX.y }, ImGui::GetColorU32(colors::MainColor), 1.f);

	bool result = ImGui::InputTextEx(label, hint, buf, static_cast<int>(buf_size), ImVec2(0, 0), flag, 0, 0);

	ImVec4 TextTarget = ImGui::IsItemActive() || ImGui::IsItemHovered() ? colors::White : colors::Gray;
	it_color->second = ImVec4(
		ImLerp(it_color->second.x, TextTarget.x, io.DeltaTime * 8.f),
		ImLerp(it_color->second.y, TextTarget.y, io.DeltaTime * 8.f),
		ImLerp(it_color->second.z, TextTarget.z, io.DeltaTime * 8.f),
		ImLerp(it_color->second.w, TextTarget.w, io.DeltaTime * 8.f));

	ImGui::PopStyleColor(2);
	ImGui::PopFont();
	ImGui::PopStyleVar();
	return result;
}

struct IconButtonState {
	ImVec4 TextColor;
};

bool Loader::CUSTOM::IconButton(const char* label, const char* icon, float posX, float posY, int action)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetCursorPos({ posX, posY });
	bool result = ImGui::InvisibleButton(label, { 15, 15 });

	ImGuiID id = window->GetID(label);
	static std::map<ImGuiID, IconButtonState> anim;
	auto it_anim = anim.find(id);
	if (it_anim == anim.end()) {
		anim.insert({ id, IconButtonState() });
		it_anim = anim.find(id);
	}

	ImVec4 target = ImGui::IsItemHovered() ? colors::lwhite : colors::Gray;
	float time = io.DeltaTime;
	it_anim->second.TextColor = ImVec4(
		ImLerp(it_anim->second.TextColor.x, target.x, time * 6),
		ImLerp(it_anim->second.TextColor.y, target.y, time * 6),
		ImLerp(it_anim->second.TextColor.z, target.z, time * 6),
		ImLerp(it_anim->second.TextColor.w, target.w, time * 6));

	ImVec2 MIN = ImGui::GetItemRectMin();
	ImVec2 MAX = ImGui::GetItemRectMax();

	ImGui::PushFont(fonts::FontAwesome);
	ImVec2 textSize = ImGui::CalcTextSize(icon);
	ImVec2 textPos = { MIN.x + (15 - textSize.x) * 0.5f, MIN.y + (15 - textSize.y) * 0.5f };
	window->DrawList->AddText(textPos, ImGui::GetColorU32(it_anim->second.TextColor), icon);
	ImGui::PopFont();

	if (result) {
		if (action == 1)
			PostQuitMessage(0);
		else if (action == 2)
			ShowWindow(hwnd, SW_MINIMIZE);
	}

	return result;
}

void Loader::CUSTOM::AddBorders(ImVec2 pos)
{
	const int vtx_idx_1 = ImGui::GetWindowDrawList()->VtxBuffer.Size;
	ImGui::GetWindowDrawList()->AddRect(ImVec2(1, 1), ImVec2(pos.x - 1, pos.y - 1), ImGui::GetColorU32(colors::MainColor), window::rounding, ImDrawFlags_None, 2.f);

	ImGui::ShadeVertsLinearColorGradientKeepAlpha(ImGui::GetWindowDrawList(), vtx_idx_1, ImGui::GetWindowDrawList()->VtxBuffer.Size, ImVec2(1, 1), ImVec2(pos.x / 4, pos.y / 3), ImGui::GetColorU32(colors::MainColor), ImColor(24, 24, 24, 24));
}

void Loader::CUSTOM::Text(float posX, float posY, ImFont* font, float size, const char* text, ImVec4 col)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	window->DrawList->AddText(font, size, { posX, posY }, ImGui::GetColorU32(col), text);
}

void Loader::CUSTOM::ShadowText(float posX, float posY, ImFont* font, float size, const char* text, ImVec4 col, ImVec4 shadow_col)
{
	ImGui::PushFont(font);
	ImGui::ShadowText(text, ImGui::GetColorU32(col), ImGui::GetColorU32(shadow_col), size, { posX, posY });
	ImGui::PopFont();
}
