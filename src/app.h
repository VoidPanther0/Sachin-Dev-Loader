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

constexpr float kDefaultWidth = 1280.0f;
constexpr float kDefaultHeight = 800.0f;
constexpr float kMinWidth = 1080.0f;
constexpr float kMinHeight = 680.0f;
constexpr wchar_t kTitle[] = L"Sachin Game Loader";
constexpr char kVersion[] = "3.0.0";

// Current client-area size. Valid only once the DX11/Win32 backends are
// initialized; falls back to the default size before that.
float ViewportWidth();
float ViewportHeight();

void Init();
void Render();
void Shutdown();

} // namespace app

void StartWindowDrag();
