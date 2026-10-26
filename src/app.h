#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

inline HWND g_hwnd = nullptr;

inline ID3D11Device* g_pd3dDevice = nullptr;
inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
inline IDXGISwapChain* g_pSwapChain = nullptr;
inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

namespace app {

constexpr float kWidth = 1160.0f;
constexpr float kHeight = 700.0f;
constexpr float kLoginWidth = 430.0f;
constexpr float kLoginHeight = 560.0f;
constexpr wchar_t kTitle[] = L"Dear ImGui Template";
constexpr char kVersion[] = "1.0.0";

void Init();
void Render();
void Shutdown();

} // namespace app

void StartWindowDrag();
void ResizeWindow(int width, int height);
