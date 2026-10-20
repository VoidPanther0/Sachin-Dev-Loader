#include "core.h"
#include "settings.h"
#include "resource.h"

#include <exception>

// Data
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
bool CreateRenderTarget();
void CleanupRenderTarget();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Loader::ui u;

namespace {

void ShowError(const wchar_t* message)
{
	MessageBoxW(nullptr, message, project_name, MB_ICONERROR | MB_OK);
}

} // namespace

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nShowCmd;

	try
	{
		HINSTANCE instance = hInstance;
		HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
		if (!icon)
			icon = LoadIconW(nullptr, IDI_APPLICATION);

		WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance, icon,
			LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, project_name, icon };

		if (!RegisterClassExW(&wc))
		{
			ShowError(L"Could not register the window class.");
			return 1;
		}

		hwnd = CreateWindowExW(NULL, wc.lpszClassName, project_name, WS_POPUP,
			(GetSystemMetrics(SM_CXSCREEN) / 2) - static_cast<int>(window::size_max.x / 2),
			(GetSystemMetrics(SM_CYSCREEN) / 2) - static_cast<int>(window::size_max.y / 2),
			static_cast<int>(window::size_max.x), static_cast<int>(window::size_max.y),
			0, 0, hInstance, 0);

		if (!hwnd)
		{
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			ShowError(L"Could not create the main window.");
			return 1;
		}

		SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

		MARGINS margins = { -1, -1, -1, -1 };
		DwmExtendFrameIntoClientArea(hwnd, &margins);

		rc = { 0, 0, 0, 0 };
		GetWindowRect(hwnd, &rc);

		if (!CreateDeviceD3D(hwnd))
		{
			CleanupDeviceD3D();
			DestroyWindow(hwnd);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			ShowError(L"Could not initialize Direct3D 11.");
			return 1;
		}

		::ShowWindow(hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(hwnd);

		IMGUI_CHECKVERSION();
		ImGuiContext* context = ImGui::CreateContext();
		if (!context)
		{
			CleanupDeviceD3D();
			DestroyWindow(hwnd);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			ShowError(L"Could not initialize the user interface.");
			return 1;
		}

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.IniFilename = nullptr;

		u.BeforeLoop();

		ImGui::StyleColorsDark();

		if (!ImGui_ImplWin32_Init(hwnd))
		{
			ImGui::DestroyContext();
			CleanupDeviceD3D();
			DestroyWindow(hwnd);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			ShowError(L"Could not initialize the Windows backend.");
			return 1;
		}

		if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext))
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			CleanupDeviceD3D();
			DestroyWindow(hwnd);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			ShowError(L"Could not initialize the Direct3D 11 backend.");
			return 1;
		}

		ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 0.f);

		bool done = false;
		while (!done)
		{
			MSG msg;
			ZeroMemory(&msg, sizeof(msg));
			while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);
				if (msg.message == WM_QUIT)
					done = true;
			}
			if (done)
				break;

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			u.Render();

			ImGui::Render();
			const float clear_color_with_alpha[4] = {
				clear_color.x * clear_color.w, clear_color.y * clear_color.w,
				clear_color.z * clear_color.w, clear_color.w };
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			const HRESULT present = g_pSwapChain->Present(1, 0);
			if (FAILED(present))
				done = true;
		}

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();
		DestroyWindow(hwnd);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);

		return 0;
	}
	catch (const std::exception& error)
	{
		MessageBoxA(nullptr, error.what(), "Sachin Dev Loader", MB_ICONERROR | MB_OK);
		return 1;
	}
	catch (...)
	{
		ShowError(L"An unexpected error stopped the application.");
		return 1;
	}
}

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	return CreateRenderTarget();
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

bool CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	if (!g_pSwapChain || FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) || !pBackBuffer)
		return false;

	const HRESULT result = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
	return SUCCEEDED(result);
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().BackendPlatformUserData != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;
	}

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
