#include "app.h"
#include "resource.h"

#include <dwmapi.h>
#include <exception>

namespace {

bool CreateDeviceD3D(HWND window)
{
	DXGI_SWAP_CHAIN_DESC desc{};
	desc.BufferCount = 2;
	desc.BufferDesc.Width = 0;
	desc.BufferDesc.Height = 0;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = window;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	D3D_FEATURE_LEVEL level{};

	HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		levels, 2, D3D11_SDK_VERSION, &desc,
		&g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);

	if (result == DXGI_ERROR_UNSUPPORTED)
	{
		result = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
			levels, 2, D3D11_SDK_VERSION, &desc,
			&g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
	}

	return result == S_OK;
}

bool CreateRenderTarget()
{
	ID3D11Texture2D* back_buffer = nullptr;
	if (!g_pSwapChain || FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) || !back_buffer)
		return false;

	const HRESULT result = g_pd3dDevice->CreateRenderTargetView(back_buffer, nullptr, &g_mainRenderTargetView);
	back_buffer->Release();
	return SUCCEEDED(result);
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView)
	{
		g_mainRenderTargetView->Release();
		g_mainRenderTargetView = nullptr;
	}
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void ShowError(const wchar_t* message)
{
	MessageBoxW(nullptr, message, app::kTitle, MB_ICONERROR | MB_OK);
}

} // namespace

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().BackendPlatformUserData != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam))
			return 1;
	}

	switch (message)
	{
	case WM_SYSCOMMAND:
		if ((wparam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	default:
		break;
	}

	return DefWindowProcW(window, message, wparam, lparam);
}

void StartWindowDrag()
{
	ReleaseCapture();
	SendMessageW(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command)
{
	(void)previous;
	(void)command_line;
	(void)show_command;

	try
	{
		HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
		if (!icon)
			icon = LoadIconW(nullptr, IDI_APPLICATION);

		WNDCLASSEXW window_class{};
		window_class.cbSize = sizeof(window_class);
		window_class.style = CS_CLASSDC;
		window_class.lpfnWndProc = WndProc;
		window_class.hInstance = instance;
		window_class.hIcon = icon;
		window_class.hIconSm = icon;
		window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		window_class.lpszClassName = L"LoaderWindow";

		if (!RegisterClassExW(&window_class))
		{
			ShowError(L"Could not register the window class.");
			return 1;
		}

		const int screen_width = GetSystemMetrics(SM_CXSCREEN);
		const int screen_height = GetSystemMetrics(SM_CYSCREEN);
		const int width = static_cast<int>(app::kWidth);
		const int height = static_cast<int>(app::kHeight);

		g_hwnd = CreateWindowExW(
			WS_EX_APPWINDOW,
			window_class.lpszClassName,
			app::kTitle,
			WS_POPUP,
			(screen_width - width) / 2,
			(screen_height - height) / 2,
			width, height,
			nullptr, nullptr, instance, nullptr);

		if (!g_hwnd)
		{
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not create the main window.");
			return 1;
		}

		BOOL dark = TRUE;
		DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark));

		const int corner = 2;
		DwmSetWindowAttribute(g_hwnd, 33, &corner, sizeof(corner));

		if (!CreateDeviceD3D(g_hwnd))
		{
			CleanupDeviceD3D();
			DestroyWindow(g_hwnd);
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not initialize Direct3D 11.");
			return 1;
		}

		if (!CreateRenderTarget())
		{
			CleanupDeviceD3D();
			DestroyWindow(g_hwnd);
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not create the Direct3D render target.");
			return 1;
		}

		ShowWindow(g_hwnd, SW_SHOWDEFAULT);
		UpdateWindow(g_hwnd);

		IMGUI_CHECKVERSION();
		ImGuiContext* context = ImGui::CreateContext();
		if (!context)
		{
			CleanupDeviceD3D();
			DestroyWindow(g_hwnd);
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not create the interface context.");
			return 1;
		}

		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		app::Init();

		if (!ImGui_ImplWin32_Init(g_hwnd))
		{
			app::Shutdown();
			ImGui::DestroyContext();
			CleanupDeviceD3D();
			DestroyWindow(g_hwnd);
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not initialize the Windows backend.");
			return 1;
		}

		if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext))
		{
			ImGui_ImplWin32_Shutdown();
			app::Shutdown();
			ImGui::DestroyContext();
			CleanupDeviceD3D();
			DestroyWindow(g_hwnd);
			UnregisterClassW(window_class.lpszClassName, instance);
			ShowError(L"Could not initialize the Direct3D backend.");
			return 1;
		}

		bool running = true;
		while (running)
		{
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
				if (message.message == WM_QUIT)
					running = false;
			}

			if (!running)
				break;

			if (IsIconic(g_hwnd))
			{
				Sleep(10);
				continue;
			}

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			app::Render();

			ImGui::Render();

			const float clear[4] = { 0.027f, 0.027f, 0.035f, 1.0f };
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			if (FAILED(g_pSwapChain->Present(1, 0)))
				running = false;
		}

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		app::Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();
		DestroyWindow(g_hwnd);
		UnregisterClassW(window_class.lpszClassName, instance);
		return 0;
	}
	catch (const std::exception& error)
	{
		MessageBoxA(nullptr, error.what(), "Loader", MB_ICONERROR | MB_OK);
		return 1;
	}
	catch (...)
	{
		ShowError(L"An unexpected error stopped the application.");
		return 1;
	}
}
