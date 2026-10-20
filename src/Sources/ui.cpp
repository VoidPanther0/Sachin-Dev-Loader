#include "core.h"
#include "settings.h"
#include "font_awesome.h"
#include "fonts.h"
#include "particles.h"
#include "supabase_auth.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

Loader::CUSTOM c;
Loader::GUI g;

namespace {

std::string LowerUsername(const char* input)
{
	std::string value = input ? input : "";
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return value;
}

std::string MaskHwid(const std::string& hwid)
{
	if (hwid.empty())
		return "Unavailable";
	if (hwid.size() <= 12)
		return hwid;
	return hwid.substr(0, 12) + "...";
}

} // namespace

void Loader::ui::BeforeLoop()
{
	sdauth::Init();

	sdauth::SavedLogin saved;
	if (sdauth::LoadSavedLogin(saved))
	{
		strncpy_s(LoginUsername, sizeof(LoginUsername), saved.username.c_str(), _TRUNCATE);
		strncpy_s(LoginPassword, sizeof(LoginPassword), saved.password.c_str(), _TRUNCATE);
		SavePassword = true;
	}

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
	ImFontConfig icons_config;
	icons_config.PixelSnapH = true;
	icons_config.OversampleH = 3;
	icons_config.OversampleV = 3;

	// The TTF byte arrays are static, so the atlas must copy them instead of
	// taking ownership and freeing them during shutdown.
	ImFontConfig text_config;
	text_config.FontDataOwnedByAtlas = false;

	if (fonts::Inter_Regular == nullptr) fonts::Inter_Regular = io.Fonts->AddFontFromMemoryTTF(inter_regular_p, static_cast<int>(sizeof(inter_regular_p)), 16, &text_config);
	if (fonts::Sansation_Light == nullptr) fonts::Sansation_Light = io.Fonts->AddFontFromMemoryTTF(sansation_light_p, static_cast<int>(sizeof(sansation_light_p)), 13, &text_config);
	if (fonts::Sansation_Regular == nullptr) fonts::Sansation_Regular = io.Fonts->AddFontFromMemoryTTF(sansation_regular_p, static_cast<int>(sizeof(sansation_regular_p)), 25, &text_config);
	if (fonts::Sansation_Bold == nullptr) fonts::Sansation_Bold = io.Fonts->AddFontFromMemoryTTF(sansation_bold_p, static_cast<int>(sizeof(sansation_bold_p)), 28, &text_config);
	if (fonts::FontAwesome == nullptr) fonts::FontAwesome = io.Fonts->AddFontFromMemoryCompressedTTF(fa6_solid_compressed_data, static_cast<int>(fa6_solid_compressed_size), 14.f, &icons_config, icons_ranges);

	InitializeParticles();
}

void Loader::ui::Render()
{
	sdauth::Tick();

	if (TAB == 1 && (!sdauth::HasSession() || sdauth::LoginStatus() == sdauth::StatusError))
	{
		sdauth::SignOut();
		TAB = 0;
		alphaColor = 0.f;
	}

	g.begin("Main Window", window::size_max);
	{
		UpdateParticles(0.03);
		RenderParticles();

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& s = ImGui::GetStyle();
		ImGuiWindow* window = ImGui::GetCurrentWindow();

		c.IconButton("###Minimize", ICON_FA_MINUS, window::size_max.x - 50, 9, 2);
		c.IconButton("###Close", ICON_FA_XMARK, window::size_max.x - 20, 9, 1);
		c.Text(15, 22, fonts::Sansation_Regular, 24, "SACHIN DEV", colors::White);
		c.Text(160, 28, fonts::Sansation_Light, 15, "LOADER", colors::MainColor);
		c.Text(window::size_max.x - 150, window::size_max.y - 30, fonts::Inter_Regular, 15, "Made by Sachin Dev", colors::White);

		alphaColor = std::clamp(alphaColor + io.DeltaTime * 2.f, 0.0f, 1.0f);
		s.Alpha = alphaColor;

		if (TAB == 0)
		{
			if (sdauth::LoginStatus() == sdauth::StatusOk && sdauth::HasSession())
			{
				if (SavePassword)
				{
					const std::string username = LowerUsername(LoginUsername);
					sdauth::SaveLogin(username, LoginPassword);
				}
				else
					sdauth::ForgetSavedLogin();

				sdauth::ResetStatus();
				TAB = 1;
				alphaColor = 0.f;
			}

			c.ShadowText(135, 120, fonts::Sansation_Bold, 65, "SACHIN DEV", colors::White, colors::MainColor);
			c.ShadowText(135, 195, fonts::Sansation_Bold, 40, "LOGIN", colors::White, colors::MainColor);

			c.InputText("USERNAME", "USERNAME", 135, 246, 250, LoginUsername, sizeof(LoginUsername), 0);
			c.InputText("PASSWORD", "PASSWORD", 135, 282, 250, LoginPassword, sizeof(LoginPassword), ImGuiInputTextFlags_Password);

			ImGui::PushFont(fonts::Inter_Regular);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.07f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.12f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.12f, 0.12f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_CheckMark, colors::MainColor);
			ImGui::PushStyleColor(ImGuiCol_Text, colors::Gray);
			ImGui::SetCursorPos({ 135, 322 });
			ImGui::Checkbox("Save password", &SavePassword);
			ImGui::PopStyleColor(5);
			ImGui::PopFont();

			const bool working = sdauth::LoginStatus() == sdauth::StatusWorking;
			if (c.Button("Login", working ? "Signing in..." : "Sign in", 135, 346, 250, 30) && !working)
			{
				const std::string username = LowerUsername(LoginUsername);
				std::string error;

				if (!sdauth::ValidateUsername(username, error))
					sdauth::ReportLocalError(error);
				else if (std::string(LoginPassword).size() < 3)
					sdauth::ReportLocalError("Password must be at least 3 characters");
				else
					sdauth::StartLogin(username, LoginPassword);
			}

			const int status = sdauth::LoginStatus();
			const std::string message = sdauth::LoginMessage();
			if (!message.empty() && (status == sdauth::StatusError || status == sdauth::StatusWorking))
				c.Text(135, 390, fonts::Inter_Regular, 14, message.c_str(), status == sdauth::StatusError ? colors::Red : colors::lwhite);
		}
		else
		{
			const sdauth::Session session = sdauth::GetSession();
			const std::string username = session.username.empty() ? "Unknown" : session.username;
			const std::string hwid = MaskHwid(sdauth::MachineHwid());

			window->DrawList->AddRectFilled({ 15, 100 }, { 610, 390 }, ImGui::GetColorU32(colors::lbg), window::rounding);
			window->DrawList->AddRect({ 15, 100 }, { 610, 390 }, ImGui::GetColorU32(colors::SecondColor), window::rounding, ImDrawFlags_None, 1.f);

			c.Text(35, 118, fonts::Sansation_Regular, 20, "Dashboard", colors::White);
			c.Text(35, 152, fonts::Inter_Regular, 15, "Account", colors::Gray);
			c.Text(150, 152, fonts::Inter_Regular, 15, username.c_str(), colors::White);
			c.Text(35, 178, fonts::Inter_Regular, 15, "Session", colors::Gray);
			c.Text(150, 178, fonts::Inter_Regular, 15, "Active", colors::Green);
			c.Text(35, 204, fonts::Inter_Regular, 15, "Hardware ID", colors::Gray);
			c.Text(150, 204, fonts::Inter_Regular, 15, hwid.c_str(), colors::lwhite);

			window->DrawList->AddLine({ 35, 236 }, { 590, 236 }, ImGui::GetColorU32(colors::DarkGray), 1.f);

			c.Text(35, 252, fonts::Inter_Regular, 14, "This build contains no game integration,", colors::lwhite);
			c.Text(35, 272, fonts::Inter_Regular, 14, "no memory access, and no hardware modification.", colors::lwhite);

			if (c.Button("SignOut", "Sign out", 35, 330, 120, 30))
			{
				sdauth::SignOut();
				sdauth::ResetStatus();
				TAB = 0;
				alphaColor = 0.f;
			}

			c.Text(400, 340, fonts::Inter_Regular, 13, "Session check: automatic", colors::Gray);
		}

		c.AddBorders(window::size_max);
	}
	g.end();
}
