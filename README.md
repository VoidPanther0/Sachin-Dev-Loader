# Sachin Dev Loader - ImGui Template

A Windows desktop UI starter built with C++20, Win32, DirectX 11, and Dear ImGui.

Use this repository as a template for a dark, frameless launcher interface. Click
**Use this template** on GitHub to start a new repository from it. The current
build is a self-contained UI preview: every control is local and visual.

## Features

- Frameless 1024x720 window with a custom title bar and rounded corners
- Animated splash screen with the custom Sachin Loader mark
- Login screen with username/password fields, password reveal, remember-me
  checkbox, inline error banner, and an authenticating state
- Loader screen with a session summary, launch progress, status card, and log out
- Custom-drawn brand mark, waves, constellation background, fields, and buttons
- Segoe UI fonts loaded at runtime from the system font directory
- Static C++ runtime, so the executable does not need the VC++ Redistributable
- Dear ImGui 1.93.1 WIP vendored in `src/gui` (no submodules, no package manager)

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with the **Desktop development with C++** workload
- Windows 10/11 SDK

No legacy DirectX SDK is required. The Direct3D 11 backend links the Windows SDK
copy of `d3dcompiler`.

## Build

Open `Loader.sln` in Visual Studio, choose `Release | x64`, and build.

From a terminal:

```powershell
msbuild Loader.sln /m /p:Configuration=Release /p:Platform=x64
```

The executable is written to `x64\Release\Loader.exe`.

For a debug build:

```powershell
msbuild Loader.sln /m /p:Configuration=Debug /p:Platform=x64
```

## Layout

```
src/
  main.cpp            Win32 window, Direct3D 11 device, frame loop
  ui.cpp              Fonts, theme, layout, and every drawn widget
  app.h               Shared globals and app entry points
  resource.h          Icon resource identifier
  Resources/          app.ico, version.rc
  gui/                Vendored Dear ImGui
    backends/         Win32 + DirectX 11 platform/renderer backends
Loader.sln
Loader.vcxproj
```

`main.cpp` owns the window and rendering. `ui.cpp` owns the interface.
`app::Init`, `app::Render`, and `app::Shutdown` are the only UI entry points.

## Customize

| What                | Where                                              |
| ------------------- | -------------------------------------------------- |
| Window title        | `app::kTitle` in `src/app.h`                       |
| Window size         | `app::kWidth`, `app::kHeight` in `src/app.h`       |
| Version label       | `app::kVersion` in `src/app.h`                     |
| Brand mark          | `draw_logo()` in `src/ui.cpp`                      |
| Wordmark colors     | `draw_wordmark_left()` in `src/ui.cpp`             |
| Splash screen       | `page_splash()` in `src/ui.cpp`                    |
| Login screen        | `page_login_form()`, `page_login_loading()`        |
| Loader screen       | `page_loader()` in `src/ui.cpp`                    |
| Background and waves| `draw_background()`, `draw_waves()` in `src/ui.cpp`|
| Fonts and sizes     | `load_fonts()` in `src/ui.cpp`                     |
| Executable icon     | `src/Resources/app.ico`                            |
| Version resource    | `src/Resources/version.rc`                         |

To rename the project, rename `Loader.sln`, `Loader.vcxproj`, and
`Loader.vcxproj.filters`, update the `Project(` line in the solution, and update
the build commands in `.github/workflows/build-release.yml`.

## What it does not do

- No real authentication, session validation, or saved credentials. Login is a
  local UI flow: any non-empty username and password reaches the loader screen.
- No HTTP, WinHTTP, sockets, or remote API calls
- No hardware IDs, MAC addresses, disk serials, or machine fingerprints
- No process enumeration, injection, memory access, or game interaction
- No shell execution, services, registry writes, or startup persistence
- No elevation request

## Release

Push a version tag:

```bash
git tag v2.1.0
git push origin v2.1.0
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches
`Loader-v2.1.0-win64.zip` to the GitHub Release. The zip contains the executable,
this README, and `SHA256SUMS.txt`.

## Unsigned builds

Release builds are unsigned by default. Windows Defender and SmartScreen may
treat a new unsigned executable cautiously. Sign with an OV or EV code-signing
certificate before public distribution:

```powershell
signtool sign /fd SHA256 /f certificate.pfx /p "<password>" `
  /tr http://timestamp.digicert.com /td SHA256 `
  Loader.exe
```

A self-signed certificate is suitable only for internal test machines that trust
the root certificate. It does not establish public publisher reputation.
