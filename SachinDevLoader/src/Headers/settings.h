#pragma once
#include "core.h"

inline ID3D11Device* g_pd3dDevice = nullptr;
inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
inline IDXGISwapChain* g_pSwapChain = nullptr;
inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

const wchar_t project_name[] = L"Sachin Dev";

inline int TAB = 0;

inline char LoginUsername[32];
inline char LoginPassword[64];
inline bool SavePassword = false;

inline float alphaColor = 0.f;
