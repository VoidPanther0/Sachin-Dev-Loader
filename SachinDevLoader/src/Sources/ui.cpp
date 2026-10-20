#include "core.h"
#include "settings.h"
#include "font_awesome.h"
#include "fonts.h"
#include "images.h"
#include "particles.h"
#include "supabase_auth.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

Loader::CUSTOM c;
Loader::GUI g;

std::string LowerUsername(const char* input)
{
    std::string value = input ? input : "";
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

void Loader::ui::BeforeLoop()
{
    sdauth::Init();

    sdauth::SavedLogin saved;
    if (sdauth::LoadSavedLogin(saved))
    {
        strncpy_s(LoginUsername, saved.username.c_str(), _TRUNCATE);
        strncpy_s(LoginPassword, saved.password.c_str(), _TRUNCATE);
        SavePassword = true;
    }

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true; icons_config.OversampleH = 3; icons_config.OversampleV = 3;

    if (images::valo == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, valo_p, sizeof(valo_p), nullptr, nullptr, &images::valo, 0);
    if (images::rust == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, rust_p, sizeof(rust_p), nullptr, nullptr, &images::rust, 0);
    if (images::fn == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, fn_p, sizeof(fn_p), nullptr, nullptr, &images::fn, 0);
    if (images::eft == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, EFT_p, sizeof(EFT_p), nullptr, nullptr, &images::eft, 0);
    if (images::mw == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, mw_p, sizeof(mw_p), nullptr, nullptr, &images::mw, 0);
    if (images::circle == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, circle_p, sizeof(circle_p), nullptr, nullptr, &images::circle, 0);
    
    if (fonts::Inter_Regular == nullptr) fonts::Inter_Regular = io.Fonts->AddFontFromMemoryTTF(inter_regular_p, sizeof(inter_regular_p), 16);
    if (fonts::Sansation_Light== nullptr) fonts::Sansation_Light = io.Fonts->AddFontFromMemoryTTF(sansation_light_p, sizeof(sansation_light_p), 13);
    if (fonts::Sansation_Regular == nullptr) fonts::Sansation_Regular = io.Fonts->AddFontFromMemoryTTF(sansation_regular_p, sizeof(sansation_regular_p), 25);
    if (fonts::Sansation_Bold == nullptr) fonts::Sansation_Bold = io.Fonts->AddFontFromMemoryTTF(sansation_bold_p, sizeof(sansation_bold_p), 28);
    if (fonts::FontAwesome == nullptr) fonts::FontAwesome = io.Fonts->AddFontFromMemoryCompressedTTF(fa6_solid_compressed_data, fa6_solid_compressed_size, 14.f, &icons_config, icons_ranges);

    InitializeParticles();
    g.Blur(hwnd);

}

void Loader::ui::Render()
{
    sdauth::Tick();

    if (TAB == 1 && sdauth::LoginStatus() == sdauth::StatusError)
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
        float time = io.DeltaTime * 5;
        static float timer = io.DeltaTime;

        c.IconButton("###Close", ICON_FA_XMARK, window::size_max.x - 20, 18 / 2, 1);
        c.Text(15, 22, fonts::Sansation_Regular, 24, "SACHIN DEV", colors::White);
        c.Text(160, 28, fonts::Sansation_Light, 15, "LOADER", colors::MainColor);
        c.Text(window::size_max.x - 150, window::size_max.y - 30, fonts::Inter_Regular, 15, "Made by Sachin Dev", colors::White);
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

            alphaColor = std::clamp(alphaColor + (1.f * ImGui::GetIO().DeltaTime * 1.f), 0.0f, 1.f);
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

        if (TAB == 1)
        {
            if (timer <= 5) { timer += 0.5; return; }

            alphaColor = std::clamp(alphaColor + (1.f * ImGui::GetIO().DeltaTime * 1.f), 0.0f, 1.f);
            window->DrawList->AddLine({ 165, 28 }, { 165, 42 }, ImGui::GetColorU32(colors::White));

            if (sdauth::HasSession())
            {
                const sdauth::Session session = sdauth::GetSession();
                c.Text(window::size_max.x - 200, 64, fonts::Inter_Regular, 14, session.username.c_str(), colors::lwhite);
            }

            if (c.Tab("HOME", "Home", NULL, 180, 28, ImGui::CalcTextSize("Home").x, ImGui::CalcTextSize("Home").y, subtab == 0)) { subtab = 0; }

            if (subtab == 0)
            {
                SelectablesPos = ImLerp(SelectablesPos, 15.f, time);
                ButtonPos = ImLerp(ButtonPos, 245.f, time);
                ParametersPos = ImLerp(ParametersPos, window::size_max.x - 15, time);

                c.Text(SelectablesPos, 87, fonts::Inter_Regular, 16, "Select a game", colors::White);
                c.Text(ParametersPos - 272, 87, fonts::Inter_Regular, 16, "Optional", colors::White);

                g.ChildBegin("Selectable items", SelectablesPos, 106, 315, 300);
                {
                    if (c.SelectableItem("Valo", images::valo, 12.5, 15, 290, 80, selected == 1)) { selected = 1; }
                    if (c.SelectableItem("Rust", images::rust, 12.5, 110, 290, 80, selected == 2)) { selected = 2; }
                    if (c.SelectableItem("FN", images::fn, 12.5, 205, 290, 80, selected == 3)) { selected = 3; }
                    if (c.SelectableItem("EFT", images::eft, 12.5, 300, 290, 80, selected == 4)) { selected = 4; }
                    if (c.SelectableItem("MW", images::mw, 12.5, 395, 290, 80, selected == 5)) { selected = 5; }
                    ImGui::Spacing();
                }
                g.ChildEnd();

                g.ChildBegin("Parameters", ParametersPos - 272, 105, 275, 115);
                {
                    c.Checkbox2("MAC", "Mac Changer", 235, 15, &MAC, 190);
                    c.Checkbox2("Cleaner", "Cleaner", 235, 40, &Clean, 208);
                    c.Checkbox2("Disk", "Disk Spoof", 235, 65, &Disk, 198);
                    c.Checkbox2("Save", "Save Config", 235, 90, &Save, 195);
                }
                g.ChildEnd();

                if (c.Button("Spoof", "Spoof", 340, ButtonPos, 270, 30)) { subtab = 5; alphaColor = 0; }
                if (c.Button("SpoofASUS", "Asus Spoof", 340, ButtonPos + 37, 132, 30)) selected = 0;
                if (c.Button("TEMPSpoof", "Temp Spoof", 478, ButtonPos + 37, 132, 30)) selected = 0;
            }

            if (subtab == 5)
            {
                alphaColor = std::clamp(alphaColor + (1.f * ImGui::GetIO().DeltaTime * 1.f), 0.0f, 1.f);

                c.Text((window::size_max.x - ImGui::CalcTextSize("Spoofig").x) / 2, (window::size_max.y - ImGui::CalcTextSize("Spoofig").y) / 2, fonts::Inter_Regular, 17, "Spoofing", colors::White);
                g.AddImageRotated(images::circle, (window::size_max.x - 20) / 2, (window::size_max.y - 35) * 0.6, 35, 35);

                timer += 0.1;
                if (timer > 100) { timer = 0; alphaColor = 0; ButtonPos = window::size_max.y + 5; SelectablesPos = -335; ParametersPos = window::size_max.x + 340; subtab = 0; }
            }
        }
        c.AddBorders(window::size_max);
    }
    g.end();
}