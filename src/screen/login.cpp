#include "../ui.h"
#include "../Resources/resource.h"
#include "auth/supabase_auth.h"
#include "imgui_internal.h"

#include <wincodec.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace screen_login {

namespace {

using namespace ui;

// ------------------------------------------------------------------ theme ----

const ImVec4 kLoginCard(0.020f, 0.031f, 0.067f, 1.0f);
const ImVec4 kLoginBorder(0.24f, 0.36f, 0.78f, 0.45f);
const ImVec4 kLoginField(0.035f, 0.051f, 0.094f, 1.0f);
const ImVec4 kLoginFieldBorder(0.30f, 0.38f, 0.60f, 0.30f);
const ImVec4 kLoginIcon(0.62f, 0.68f, 0.82f, 1.0f);
const ImVec4 kLoginHint(0.52f, 0.58f, 0.72f, 1.0f);
const ImVec4 kLoginBlue(0.17f, 0.33f, 0.86f, 1.0f);
const ImVec4 kLoginPurple(0.52f, 0.29f, 0.93f, 1.0f);
const ImVec4 kLoginSubtitle(0.36f, 0.53f, 0.95f, 1.0f);
const ImVec4 kLoginLink(0.58f, 0.48f, 0.96f, 1.0f);

constexpr int kLogoSize = 120;

// Startup check, then the form, then the login check. The dashboard opens only
// after the server answered every login step.
enum class Phase { Startup, Form, Signing };

// Visual pacing for a loading screen. The worker reports the real stage; each
// step stays on screen for at least kStepSeconds so the user can read it.
struct LoadingAnim
{
	int shown = 0;           // index of the active step; == count when all done
	double step_time = 0.0;  // when the shown step became active
	float progress = 0.0f;
	double done_time = -1.0; // when the bar reached 100%
};

constexpr double kStepSeconds = 0.55;
constexpr double kDoneHoldSeconds = 0.35;

const char* const kStartupSteps[] = {
	"Connecting to server...",
	"Checking loader integrity...",
	"Downloading components...",
};

const char* const kLoginSteps[] = {
	"Authenticating...",
	"Verifying account and device...",
	"Running security checks...",
};

struct LoginState
{
	char username[64] = "";
	char password[64] = "";
	bool show_password = false;
	bool remember = false;
	bool logged_in = false;
	bool focus_username = true;
	bool focus_password = false;
	std::string message;
	bool message_is_error = false;
	Phase phase = Phase::Startup;
	LoadingAnim anim;
};

LoginState login;
ID3D11ShaderResourceView* logo_texture = nullptr;

// Remembering a username only. Passwords and session tokens never touch disk.
std::wstring login_settings_path(bool create_directory)
{
	wchar_t appdata[MAX_PATH] = {};
	const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return std::wstring();

	std::wstring directory = std::wstring(appdata) + L"\\SachinDevLoader";
	if (create_directory)
		CreateDirectoryW(directory.c_str(), nullptr);

	return directory + L"\\settings.ini";
}

void load_login_settings()
{
	const std::wstring path = login_settings_path(false);
	if (path.empty())
		return;

	wchar_t stored[64] = {};
	GetPrivateProfileStringW(L"login", L"username", L"", stored, 64, path.c_str());
	if (stored[0] == L'\0')
		return;

	char username[64] = {};
	WideCharToMultiByte(CP_UTF8, 0, stored, -1, username, static_cast<int>(sizeof(username)), nullptr, nullptr);
	if (username[0] == '\0')
		return;

	std::snprintf(login.username, sizeof(login.username), "%s", username);
	login.remember = true;
	login.focus_username = false;
	login.focus_password = true;
}

void store_remembered_username(const char* username, bool remember)
{
	const std::wstring path = login_settings_path(true);
	if (path.empty())
		return;

	if (!remember)
	{
		WritePrivateProfileStringW(L"login", L"username", nullptr, path.c_str());
		return;
	}

	const int length = MultiByteToWideChar(CP_UTF8, 0, username, -1, nullptr, 0);
	if (length <= 1)
		return;

	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, username, -1, &wide[0], length);
	wide.resize(static_cast<std::size_t>(length - 1));

	WritePrivateProfileStringW(L"login", L"username", wide.c_str(), path.c_str());
}

// Decodes the embedded Slogo.png (IDR_LOGO) with WIC, scales it to kLogoSize
// and uploads it as a Direct3D 11 texture.
bool load_logo()
{
	HMODULE module = GetModuleHandleW(nullptr);
	HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LOGO), RT_RCDATA);
	if (!resource)
		return false;

	HGLOBAL handle = LoadResource(module, resource);
	void* bytes = handle ? LockResource(handle) : nullptr;
	const DWORD size = SizeofResource(module, resource);
	if (!bytes || size == 0)
		return false;

	const bool com_ready = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

	IWICImagingFactory* factory = nullptr;
	IWICStream* stream = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;
	IWICBitmapScaler* scaler = nullptr;
	IWICFormatConverter* output = nullptr;

	const UINT side = static_cast<UINT>(kLogoSize);
	std::vector<unsigned char> pixels(static_cast<size_t>(side) * side * 4);

	const bool decoded =
		SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		SUCCEEDED(factory->CreateStream(&stream)) &&
		SUCCEEDED(stream->InitializeFromMemory(static_cast<BYTE*>(bytes), size)) &&
		SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
		SUCCEEDED(decoder->GetFrame(0, &frame)) &&
		SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
		SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone,
			nullptr, 0.0, WICBitmapPaletteTypeCustom)) &&
		SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
		SUCCEEDED(scaler->Initialize(converter, side, side, WICBitmapInterpolationModeFant)) &&
		SUCCEEDED(factory->CreateFormatConverter(&output)) &&
		SUCCEEDED(output->Initialize(scaler, GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone,
			nullptr, 0.0, WICBitmapPaletteTypeCustom)) &&
		SUCCEEDED(output->CopyPixels(nullptr, side * 4, static_cast<UINT>(pixels.size()), pixels.data()));

	if (output) output->Release();
	if (scaler) scaler->Release();
	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (stream) stream->Release();
	if (factory) factory->Release();
	if (com_ready)
		CoUninitialize();

	if (!decoded)
		return false;

	// Scaled premultiplied so edges stay clean; ImGui blends straight alpha.
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		const unsigned alpha = pixels[i + 3];
		if (alpha == 0 || alpha == 255)
			continue;
		for (size_t c = 0; c < 3; ++c)
		{
			const unsigned value = pixels[i + c] * 255u / alpha;
			pixels[i + c] = static_cast<unsigned char>(value > 255u ? 255u : value);
		}
	}

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = side;
	desc.Height = side;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = pixels.data();
	data.SysMemPitch = side * 4;

	ID3D11Texture2D* texture = nullptr;
	if (!g_pd3dDevice || FAILED(g_pd3dDevice->CreateTexture2D(&desc, &data, &texture)) || !texture)
		return false;

	const HRESULT result = g_pd3dDevice->CreateShaderResourceView(texture, nullptr, &logo_texture);
	texture->Release();
	return SUCCEEDED(result);
}

void gradient_text(ImFont* f, float size, ImVec2 position, const char* value, const ImVec4& from, const ImVec4& to)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const int start = draw->VtxBuffer.Size;
	draw->AddText(f, size, position, IM_COL32_WHITE, value);
	const float w = text_width(f, size, value);
	ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, start, draw->VtxBuffer.Size,
		position, ImVec2(position.x + w, position.y), color_u32(from), color_u32(to));
}

void gradient_box(const Rect& r, const ImVec4& from, const ImVec4& to, float rounding)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const int start = draw->VtxBuffer.Size;
	draw->AddRectFilled(r.min, r.max, IM_COL32_WHITE, rounding);
	ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, start, draw->VtxBuffer.Size,
		r.min, ImVec2(r.max.x, r.min.y), color_u32(from), color_u32(to));
}

void icon_user(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddCircle(ImVec2(c.x, c.y - 5.0f), 5.0f, color, 24, 1.7f);
	draw->PathArcTo(ImVec2(c.x, c.y + 11.0f), 9.0f, IM_PI, IM_PI * 2.0f, 20);
	draw->PathStroke(color, ImDrawFlags_Closed, 1.7f);
}

void icon_lock(ImVec2 c, ImU32 color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PathArcTo(ImVec2(c.x, c.y - 4.0f), 5.0f, IM_PI, IM_PI * 2.0f, 16);
	draw->PathStroke(color, 0, 1.7f);
	draw->AddLine(ImVec2(c.x - 5.0f, c.y - 4.0f), ImVec2(c.x - 5.0f, c.y), color, 1.7f);
	draw->AddLine(ImVec2(c.x + 5.0f, c.y - 4.0f), ImVec2(c.x + 5.0f, c.y), color, 1.7f);
	draw->AddRect(ImVec2(c.x - 8.0f, c.y), ImVec2(c.x + 8.0f, c.y + 11.0f), color, 2.5f, 0, 1.7f);
}

void icon_eye(ImVec2 c, ImU32 color, bool crossed)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddBezierQuadratic(ImVec2(c.x - 11.0f, c.y), ImVec2(c.x, c.y - 10.0f), ImVec2(c.x + 11.0f, c.y), color, 1.7f);
	draw->AddBezierQuadratic(ImVec2(c.x - 11.0f, c.y), ImVec2(c.x, c.y + 10.0f), ImVec2(c.x + 11.0f, c.y), color, 1.7f);
	draw->AddCircle(c, 3.5f, color, 16, 1.7f);
	if (crossed)
		draw->AddLine(ImVec2(c.x - 9.0f, c.y + 7.0f), ImVec2(c.x + 9.0f, c.y - 7.0f), color, 1.7f);
}

enum class FieldIcon { User, Lock };

// Rounded field with a leading icon. Returns true when Enter is pressed.
bool login_field(const char* id, const Rect& r, char* buffer, size_t size, const char* hint,
	FieldIcon icon, bool* reveal, bool focus)
{
	box(r, kLoginField, 10.0f);

	const float icon_x = r.min.x + 30.0f;
	const float mid_y = r.min.y + height(r) * 0.5f;
	const ImU32 icon_color = color_u32(kLoginIcon);
	if (icon == FieldIcon::User)
		icon_user(ImVec2(icon_x, mid_y), icon_color);
	else
		icon_lock(ImVec2(icon_x, mid_y), icon_color);

	const float font_size = 18.0f;
	const float input_x = r.min.x + 56.0f;
	const float input_w = width(r) - 56.0f - (reveal ? 56.0f : 18.0f);

	ImGui::PushFont(font.body, font_size);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, (height(r) - font_size) * 0.5f - 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, kLoginHint);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
	if (reveal && !*reveal)
		flags |= ImGuiInputTextFlags_Password;

	if (focus)
		ImGui::SetKeyboardFocusHere();
	ImGui::SetCursorScreenPos(ImVec2(input_x, r.min.y));
	ImGui::SetNextItemWidth(input_w);
	const bool submitted = ImGui::InputTextWithHint(id, hint, buffer, size, flags);
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
	ImGui::PopFont();

	if (reveal)
	{
		const Rect eye = rect(r.max.x - 50.0f, r.min.y + 6.0f, r.max.x - 8.0f, r.max.y - 6.0f);
		ImGui::SetCursorScreenPos(eye.min);
		ImGui::PushID(id);
		if (ImGui::InvisibleButton("##reveal", ImVec2(width(eye), height(eye))))
			*reveal = !*reveal;
		const bool eye_hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (eye_hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		icon_eye(center(eye), color_u32(eye_hovered ? kText : kLoginIcon), *reveal);
	}

	const ImVec4 border = active ? fade(kLoginPurple, 0.85f) : (hovered ? fade(kLoginBorder, 0.9f) : kLoginFieldBorder);
	ImGui::GetWindowDrawList()->AddRect(r.min, r.max, color_u32(border), 10.0f, 0, active ? 1.5f : 1.0f);
	return submitted;
}

bool login_checkbox(const char* id, ImVec2 position, const char* value, bool* checked)
{
	const float side = 22.0f;
	const float label_w = text_width(font.body, 17.0f, value);

	ImGui::SetCursorScreenPos(position);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(side + 12.0f + label_w, side));
	const bool hovered = ImGui::IsItemHovered();
	if (clicked)
		*checked = !*checked;
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	ImDrawList* draw = ImGui::GetWindowDrawList();
	const Rect r = rect(position.x, position.y, position.x + side, position.y + side);
	if (*checked)
	{
		gradient_box(r, kLoginBlue, kLoginPurple, 5.0f);
		draw->AddLine(ImVec2(r.min.x + 6.0f, r.min.y + 11.5f), ImVec2(r.min.x + 9.5f, r.min.y + 15.0f), IM_COL32_WHITE, 2.0f);
		draw->AddLine(ImVec2(r.min.x + 9.5f, r.min.y + 15.0f), ImVec2(r.min.x + 16.5f, r.min.y + 7.5f), IM_COL32_WHITE, 2.0f);
	}
	else
	{
		box(r, kLoginField, 5.0f);
		draw->AddRect(r.min, r.max, color_u32(hovered ? kLoginBorder : fade(kLoginIcon, 0.55f)), 5.0f, 0, 1.4f);
	}

	text(font.body, 17.0f, ImVec2(r.max.x + 12.0f, position.y), hovered ? kText : lift(kLoginIcon, 0.18f), value);
	return clicked;
}

bool login_link(const char* id, ImVec2 position, const char* value)
{
	const float size = 17.0f;
	const float w = text_width(font.body, size, value);

	ImGui::SetCursorScreenPos(position);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(w, size + 4.0f));
	const bool hovered = ImGui::IsItemHovered();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const ImVec4 color = hovered ? lift(kLoginLink, 0.12f) : kLoginLink;
	text(font.body, size, position, color, value);
	if (hovered)
		ImGui::GetWindowDrawList()->AddLine(ImVec2(position.x, position.y + size + 2.0f),
			ImVec2(position.x + w, position.y + size + 2.0f), color_u32(fade(color, 0.7f)), 1.0f);
	return clicked;
}

bool login_button(const char* id, const Rect& r, const char* value)
{
	ImGui::SetCursorScreenPos(r.min);
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(width(r), height(r)));
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	if (hovered)
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

	const float shift = held ? -0.04f : (hovered ? 0.06f : 0.0f);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	if (hovered)
		draw->AddRect(ImVec2(r.min.x - 2.0f, r.min.y - 2.0f), ImVec2(r.max.x + 2.0f, r.max.y + 2.0f),
			color_u32(fade(kLoginPurple, 0.30f)), 12.0f, 0, 3.0f);
	gradient_box(r, lift(kLoginBlue, shift), lift(kLoginPurple, shift), 10.0f);

	const float size = 20.0f;
	const float label_w = text_width(font.semibold, size, value);
	const float arrow_w = 16.0f;
	const float total = label_w + 12.0f + arrow_w;
	const float x = r.min.x + (width(r) - total) * 0.5f;
	const float y = r.min.y + (height(r) - size) * 0.5f - 1.0f;
	text(font.semibold, size, ImVec2(x, y), kText, value);

	const ImU32 white = IM_COL32_WHITE;
	const float ax = x + label_w + 12.0f;
	const float ay = r.min.y + height(r) * 0.5f;
	draw->AddLine(ImVec2(ax, ay), ImVec2(ax + arrow_w, ay), white, 2.0f);
	draw->AddLine(ImVec2(ax + arrow_w - 6.0f, ay - 6.0f), ImVec2(ax + arrow_w, ay), white, 2.0f);
	draw->AddLine(ImVec2(ax + arrow_w - 6.0f, ay + 6.0f), ImVec2(ax + arrow_w, ay), white, 2.0f);
	return clicked;
}

void fill_wave(ImVec2 start, ImVec2 c1, ImVec2 c2, ImVec2 end, float bottom, const ImVec4& color)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->PathLineTo(ImVec2(start.x, bottom));
	draw->PathLineTo(start);
	draw->PathBezierCubicCurveTo(c1, c2, end, 32);
	draw->PathLineTo(ImVec2(end.x, bottom));
	draw->PathFillConcave(color_u32(color));
}

void draw_login_waves(float w, float h, float time)
{
	const float s = std::sin(time * 0.5f) * 8.0f;

	fill_wave(ImVec2(0.0f, h - 150.0f), ImVec2(90.0f, h - 160.0f + s), ImVec2(170.0f, h - 30.0f),
		ImVec2(330.0f, h), h, fade(kLoginBlue, 0.10f));
	fill_wave(ImVec2(w, h - 140.0f), ImVec2(w - 90.0f, h - 150.0f - s), ImVec2(w - 180.0f, h - 20.0f),
		ImVec2(w - 340.0f, h), h, fade(kLoginBlue, 0.09f));
	fill_wave(ImVec2(0.0f, h - 70.0f), ImVec2(w * 0.35f, h - 20.0f + s), ImVec2(w * 0.65f, h - 95.0f - s),
		ImVec2(w, h - 55.0f), h, fade(kLoginPurple, 0.06f));
}

void submit_login()
{
	if (sdauth::LoginStatus() == sdauth::StatusWorking)
		return;

	const std::string username = sdauth::NormalizeUsername(login.username);
	std::string error;

	if (!sdauth::ValidateUsername(username, error))
	{
		login.message = error;
		login.message_is_error = true;
		login.focus_username = true;
		return;
	}

	if (login.password[0] == '\0')
	{
		login.message = "Enter your password.";
		login.message_is_error = true;
		login.focus_password = true;
		return;
	}

	std::snprintf(login.username, sizeof(login.username), "%s", username.c_str());
	login.message.clear();
	login.message_is_error = false;

	sdauth::StartLogin(username, login.password);
	if (sdauth::LoginStatus() == sdauth::StatusWorking)
	{
		login.phase = Phase::Signing;
		login.anim = LoadingAnim();
		login.anim.step_time = ImGui::GetTime();
	}
	else
	{
		login.message = sdauth::LoginMessage();
		if (login.message.empty())
			login.message = "Login could not start. Try again.";
		login.message_is_error = true;
		sdauth::ResetStatus();
	}
}

void finish_login()
{
	const sdauth::Session session = sdauth::GetSession();
	login.logged_in = true;
	login.phase = Phase::Form;
	login.message.clear();
	login.message_is_error = false;
	SecureZeroMemory(login.password, sizeof(login.password));
	login.show_password = false;
	store_remembered_username(session.username.c_str(), login.remember);
	state.status = std::string("Signed in as ") + session.username + ".";
	sdauth::ResetStatus();
	ResizeWindow(static_cast<int>(app::kWidth), static_cast<int>(app::kHeight));
}

// Card background, window buttons, logo, title and subtitle. Shared by the
// form and both loading screens.
void draw_frame()
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float w = app::kLoginWidth;
	const float h = app::kLoginHeight;
	const float cx = w * 0.5f;

	draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h), color_u32(kLoginCard));
	draw_login_waves(w, h, static_cast<float>(ImGui::GetTime()));
	draw->AddRect(ImVec2(0.5f, 0.5f), ImVec2(w - 0.5f, h - 0.5f), color_u32(kLoginBorder), 8.0f, 0, 1.0f);

	// Drag strip and window buttons.
	ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
	ImGui::InvisibleButton("##login_drag", ImVec2(w - 84.0f, 44.0f));
	if (ImGui::IsItemClicked())
		StartWindowDrag();

	if (window_button("##login_minimize", rect(w - 78.0f, 8.0f, w - 46.0f, 40.0f), false))
		ShowWindow(g_hwnd, SW_MINIMIZE);
	if (window_button("##login_close", rect(w - 40.0f, 8.0f, w - 8.0f, 40.0f), true))
		PostQuitMessage(0);

	// Logo.
	const float half = kLogoSize * 0.5f;
	const float logo_y = 98.0f;
	if (logo_texture)
		draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(logo_texture)),
			ImVec2(cx - half, logo_y - half), ImVec2(cx + half, logo_y + half));
	else
		gradient_text(font.title, 72.0f, ImVec2(cx - 20.0f, logo_y - 40.0f), "S", kLoginPurple, kLoginBlue);

	// Title.
	const float title_size = 40.0f;
	const float first_w = text_width(font.title, title_size, "Sachin ");
	const float title_w = first_w + text_width(font.title, title_size, "Loader");
	const ImVec2 title_pos(cx - title_w * 0.5f, 158.0f);
	text(font.title, title_size, title_pos, kText, "Sachin ");
	gradient_text(font.title, title_size, ImVec2(title_pos.x + first_w, title_pos.y), "Loader",
		kLoginPurple, lift(kLoginBlue, 0.12f));

	const char* subtitle = "Software Apps, Game Loader";
	const float subtitle_w = text_width(font.body, 18.0f, subtitle);
	text(font.body, 18.0f, ImVec2(cx - subtitle_w * 0.5f, 208.0f), kLoginSubtitle, subtitle);

	const char* footer = "Secure  -  Private  -  Fast";
	const float footer_w = text_width(font.body, 16.0f, footer);
	const float footer_y = h - 46.0f < 540.0f ? 540.0f : h - 46.0f;
	text(font.body, 16.0f, ImVec2(cx - footer_w * 0.5f, footer_y), fade(kLoginSubtitle, 0.85f), footer);
}

// ---------------------------------------------------------------- loading ----

// Moves the visible step toward the real one, one step per kStepSeconds.
// real_active is the index of the step the worker is on; count means all done.
void advance_loading(LoadingAnim& anim, int real_active, int count)
{
	const double now = ImGui::GetTime();
	if (anim.shown < real_active && now - anim.step_time >= kStepSeconds)
	{
		++anim.shown;
		anim.step_time = now;
	}

	// The bar creeps inside the active step so it never looks frozen.
	float target = static_cast<float>(anim.shown) / static_cast<float>(count);
	if (anim.shown < count)
	{
		const float creep = clampf(static_cast<float>(now - anim.step_time) / 1.6f, 0.0f, 0.8f);
		target += creep / static_cast<float>(count);
	}

	anim.progress += (target - anim.progress) * clampf(ImGui::GetIO().DeltaTime * 6.0f, 0.0f, 1.0f);
	if (anim.shown >= count && anim.progress > 0.995f)
	{
		anim.progress = 1.0f;
		if (anim.done_time < 0.0)
			anim.done_time = now;
	}
}

bool loading_finished(const LoadingAnim& anim)
{
	return anim.done_time >= 0.0 && ImGui::GetTime() - anim.done_time >= kDoneHoldSeconds;
}

enum class StepState { Pending, Active, Done, Failed };

void step_indicator(ImVec2 c, StepState step)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float radius = 10.0f;

	switch (step)
	{
	case StepState::Done:
	{
		const int start = draw->VtxBuffer.Size;
		draw->AddCircleFilled(c, radius, IM_COL32_WHITE, 24);
		ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, start, draw->VtxBuffer.Size,
			ImVec2(c.x - radius, c.y), ImVec2(c.x + radius, c.y), color_u32(kLoginBlue), color_u32(kLoginPurple));
		draw->AddLine(ImVec2(c.x - 4.5f, c.y + 0.5f), ImVec2(c.x - 1.5f, c.y + 3.5f), IM_COL32_WHITE, 2.0f);
		draw->AddLine(ImVec2(c.x - 1.5f, c.y + 3.5f), ImVec2(c.x + 4.5f, c.y - 3.5f), IM_COL32_WHITE, 2.0f);
		break;
	}
	case StepState::Active:
	{
		const float t = static_cast<float>(ImGui::GetTime());
		draw->AddCircle(c, radius, color_u32(fade(kLoginPurple, 0.35f)), 24, 2.0f);
		draw->PathArcTo(c, radius, t * 6.0f, t * 6.0f + IM_PI * 0.7f, 16);
		draw->PathStroke(color_u32(lift(kLoginPurple, 0.15f)), 0, 2.0f);
		const float pulse = 0.75f + 0.25f * std::sin(t * 5.0f);
		draw->AddCircleFilled(c, 4.0f * pulse, color_u32(kLoginPurple), 16);
		break;
	}
	case StepState::Failed:
		draw->AddCircleFilled(c, radius, color_u32(kRed), 24);
		draw->AddLine(ImVec2(c.x - 3.5f, c.y - 3.5f), ImVec2(c.x + 3.5f, c.y + 3.5f), IM_COL32_WHITE, 2.0f);
		draw->AddLine(ImVec2(c.x + 3.5f, c.y - 3.5f), ImVec2(c.x - 3.5f, c.y + 3.5f), IM_COL32_WHITE, 2.0f);
		break;
	default:
		draw->AddCircle(c, radius, color_u32(fade(kLoginIcon, 0.45f)), 24, 1.6f);
		break;
	}
}

// Step list and progress bar. failed_step < 0 means no failure.
void draw_loading(const char* const* steps, int count, const LoadingAnim& anim, int failed_step,
	const std::string& caption, bool caption_is_error)
{
	const float w = app::kLoginWidth;
	const float cx = w * 0.5f;
	const float fx = 70.0f;

	for (int i = 0; i < count; ++i)
	{
		StepState step = StepState::Pending;
		if (i == failed_step)
			step = StepState::Failed;
		else if (i < anim.shown)
			step = StepState::Done;
		else if (i == anim.shown && failed_step < 0)
			step = StepState::Active;

		const float y = 262.0f + i * 42.0f;
		step_indicator(ImVec2(fx + 10.0f, y + 10.0f), step);

		ImVec4 color = lift(kLoginIcon, 0.18f);
		if (step == StepState::Active)
			color = kText;
		else if (step == StepState::Pending)
			color = fade(kLoginIcon, 0.6f);
		else if (step == StepState::Failed)
			color = kRed;
		text(font.body, 18.0f, ImVec2(fx + 34.0f, y), color, steps[i]);
	}

	// Progress bar.
	const Rect track = rect(60.0f, 410.0f, w - 60.0f, 418.0f);
	box(track, kLoginField, 4.0f);
	const float fill = clampf(anim.progress, 0.0f, 1.0f);
	if (fill > 0.01f)
	{
		const Rect bar = rect(track.min.x, track.min.y, track.min.x + width(track) * fill, track.max.y);
		gradient_box(bar, failed_step >= 0 ? kRed : kLoginBlue, failed_step >= 0 ? kRed : kLoginPurple, 4.0f);
	}

	char percent[16];
	std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(fill * 100.0f + 0.5f));
	const float pw = text_width(font.body, 15.0f, percent);
	text(font.body, 15.0f, ImVec2(track.max.x - pw, 426.0f), kLoginHint, percent);

	if (!caption.empty())
	{
		const ImVec4 color = caption_is_error ? kRed : kLoginSubtitle;
		const float mw = text_width(font.body, 15.0f, caption.c_str());
		const float x = caption_is_error ? cx - mw * 0.5f : track.min.x;
		text(font.body, 15.0f, ImVec2(x, caption_is_error ? 446.0f : 426.0f), color, caption.c_str());
	}
}

// Faded button shown while a loading screen runs.
void busy_button(const Rect& r, const char* value)
{
	gradient_box(r, fade(kLoginBlue, 0.45f), fade(kLoginPurple, 0.45f), 10.0f);
	text_center(font.semibold, 20.0f, r, fade(kText, 0.75f), value);
}

void draw_startup()
{
	const int count = static_cast<int>(sizeof(kStartupSteps) / sizeof(kStartupSteps[0]));
	const int status = sdauth::StartupStatus();
	const int stage = sdauth::StartupStageValue();
	const bool failed = status == sdauth::StatusError;

	// StartupConnecting (1) is step 0; StartupDone means every step is done.
	const int real_active = status == sdauth::StatusOk ? count : stage - 1;
	if (!failed)
		advance_loading(login.anim, real_active, count);

	const int failed_step = failed ? (login.anim.shown < count ? login.anim.shown : count - 1) : -1;
	std::string caption = failed ? sdauth::StartupMessage() : std::string("Preparing loader...");
	if (caption.empty())
		caption = "Startup check failed. Retry.";
	draw_loading(kStartupSteps, count, login.anim, failed_step, caption, failed);

	const float fx = 60.0f;
	const Rect button = rect(fx, 472.0f, app::kLoginWidth - fx, 530.0f);
	if (failed)
	{
		if (login_button("##startup_retry", button, "Retry"))
		{
			login.anim = LoadingAnim();
			login.anim.step_time = ImGui::GetTime();
			sdauth::StartStartup();
		}
		return;
	}

	busy_button(button, "Loading...");

	if (loading_finished(login.anim))
	{
		login.phase = Phase::Form;
		login.anim = LoadingAnim();
	}
}

void draw_signing()
{
	const int count = static_cast<int>(sizeof(kLoginSteps) / sizeof(kLoginSteps[0]));
	const int status = sdauth::LoginStatus();

	if (status == sdauth::StatusError)
	{
		// Back to the form with the server's reason. Nothing was unlocked.
		login.phase = Phase::Form;
		login.message = sdauth::LoginMessage();
		if (login.message.empty())
			login.message = "Sign-in failed. Try again.";
		login.message_is_error = true;
		login.focus_password = true;
		SecureZeroMemory(login.password, sizeof(login.password));
		sdauth::ResetStatus();
		return;
	}

	const bool ok = status == sdauth::StatusOk && sdauth::HasSession();
	const int real_active = ok ? count : sdauth::LoginStage() - 1;
	advance_loading(login.anim, real_active, count);

	draw_loading(kLoginSteps, count, login.anim, -1, "Signing in...", false);
	busy_button(rect(60.0f, 472.0f, app::kLoginWidth - 60.0f, 530.0f), "Please wait");

	if (ok && loading_finished(login.anim))
		finish_login();
}

// ------------------------------------------------------------------- form ----

void draw_form()
{
	const float w = app::kLoginWidth;
	const float cx = w * 0.5f;

	// Fields.
	const float fx = 60.0f;
	const float fw = w - 120.0f;

	const bool focus_user = login.focus_username;
	login.focus_username = false;
	if (login_field("##username", rect(fx, 258.0f, fx + fw, 316.0f), login.username, sizeof(login.username),
		"Username", FieldIcon::User, nullptr, focus_user))
		login.focus_password = true;

	const bool focus_pass = login.focus_password;
	login.focus_password = false;
	const bool enter = login_field("##password", rect(fx, 332.0f, fx + fw, 390.0f), login.password,
		sizeof(login.password), "Password", FieldIcon::Lock, &login.show_password, focus_pass);

	login_checkbox("##remember", ImVec2(fx, 410.0f), "Remember username", &login.remember);

	const char* forgot = "Forgot password?";
	if (login_link("##forgot", ImVec2(fx + fw - text_width(font.body, 17.0f, forgot), 410.0f), forgot))
	{
		login.message = "Ask the admin to reset your password.";
		login.message_is_error = false;
	}

	if (!login.message.empty())
	{
		const ImVec4 color = login.message_is_error ? kRed : kLoginSubtitle;
		const float mw = text_width(font.body, 15.0f, login.message.c_str());
		text(font.body, 15.0f, ImVec2(cx - mw * 0.5f, 446.0f), color, login.message.c_str());
	}

	const bool submitted = login_button("##login", rect(fx, 472.0f, fx + fw, 530.0f), "Login");
	if (submitted || enter)
		submit_login();
}

void draw_login_screen()
{
	draw_frame();

	switch (login.phase)
	{
	case Phase::Startup: draw_startup(); break;
	case Phase::Signing: draw_signing(); break;
	default: draw_form(); break;
	}
}

} // namespace

void Init()
{
	load_logo();
	load_login_settings();

	// First loading screen: build check and components before the form.
	login.phase = Phase::Startup;
	login.anim = LoadingAnim();
	sdauth::StartStartup();
}

void Shutdown()
{
	if (logo_texture)
	{
		logo_texture->Release();
		logo_texture = nullptr;
	}
}

void Render()
{
	draw_login_screen();
}

bool IsLoggedIn()
{
	return login.logged_in;
}

void SignOut()
{
	sdauth::SignOut();
	sdauth::ResetStatus();

	login.logged_in = false;
	login.message.clear();
	login.message_is_error = false;
	SecureZeroMemory(login.password, sizeof(login.password));
	login.show_password = false;
	login.focus_username = true;
	login.focus_password = false;

	ResizeWindow(static_cast<int>(app::kLoginWidth), static_cast<int>(app::kLoginHeight));
}

void SessionLost()
{
	login.logged_in = false;
	login.message = sdauth::LoginStatus() == sdauth::StatusError
		? sdauth::LoginMessage()
		: "Session ended. Sign in again.";
	login.message_is_error = true;
	sdauth::ResetStatus();
	SecureZeroMemory(login.password, sizeof(login.password));
	login.show_password = false;
	login.focus_username = true;
	ResizeWindow(static_cast<int>(app::kLoginWidth), static_cast<int>(app::kLoginHeight));
}

} // namespace screen_login
