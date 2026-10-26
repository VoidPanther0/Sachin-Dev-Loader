# Dear ImGui Template

A single-screen Windows desktop UI starter built with C++20, Win32, DirectX 11,
and Dear ImGui. It uses a frameless dark window and shows the common ImGui
widgets on one screen: buttons, text fields, numeric inputs, sliders, checkboxes,
radio buttons, combo boxes, color editing, progress, and tree nodes.

Use this repository as a starting point for your own tool, or click **Use this
template** on GitHub to create a new repository. The UI is local and
self-contained. There is no backend, network layer, or saved settings.

## Features

- Frameless 1160x700 window with a custom title bar and rounded corners
- One screen with a three-column widget gallery
- Custom dark theme with a live accent color picker
- Segoe UI fonts loaded from the system font directory
- Static C++ runtime, so the executable does not need the VC++ Redistributable
- Dear ImGui 1.93.1 WIP vendored in `src/gui` (no submodules, no package manager)

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with the **Desktop development with C++** workload
- Windows 10/11 SDK

No legacy DirectX SDK is required. The Direct3D 11 backend links the Windows SDK
copy of `d3dcompiler`.

## Build

Open `ImGuiTemplate.sln` in Visual Studio, choose `Release | x64`, and build.

From a terminal:

```powershell
msbuild ImGuiTemplate.sln /m /p:Configuration=Release /p:Platform=x64
```

The executable is written to `x64\Release\ImGuiTemplate.exe`.

For a debug build:

```powershell
msbuild ImGuiTemplate.sln /m /p:Configuration=Debug /p:Platform=x64
```

## Layout

```
src/
  main.cpp            Win32 window, Direct3D 11 device, frame loop
  ui.cpp              Theme, layout, and the single-screen widget gallery
  app.h               Shared globals and app entry points
  resource.h          Icon resource identifier
  Resources/          app.ico, version.rc
  gui/                Vendored Dear ImGui
    backends/         Win32 + DirectX 11 platform/renderer backends
ImGuiTemplate.sln
ImGuiTemplate.vcxproj
```

`main.cpp` owns the window and rendering. `ui.cpp` owns the interface.
`app::Init`, `app::Render`, and `app::Shutdown` are the only UI entry points.

## Customize

| What                | Where                                              |
| ------------------- | -------------------------------------------------- |
| Window title        | `app::kTitle` in `src/app.h`                       |
| Window size         | `app::kWidth`, `app::kHeight` in `src/app.h`       |
| Version label       | `app::kVersion` in `src/app.h`                     |
| Theme colors        | `kBg`, `kPanel`, `kText`, and related constants in `src/ui.cpp` |
| Widget gallery      | `draw_inputs_card()`, `draw_selection_card()`, `draw_actions_card()` |
| Demo state          | `AppState` and `reset_state()` in `src/ui.cpp`     |
| Fonts and sizes     | `load_fonts()` in `src/ui.cpp`                     |
| Executable icon     | `src/Resources/app.ico`                            |
| Version resource    | `src/Resources/version.rc`                         |

To rename the project, rename `ImGuiTemplate.sln`, `ImGuiTemplate.vcxproj`, and
`ImGuiTemplate.vcxproj.filters`, update the `Project(` line in the solution, and
update the build commands in `.github/workflows/build-release.yml`.

## Release

Push a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches
`ImGuiTemplate-v1.0.0-win64.zip` to the GitHub Release. The zip contains the
executable, this README, and `SHA256SUMS.txt`.

## Unsigned builds

Release builds are unsigned by default. Windows Defender and SmartScreen may
treat a new unsigned executable cautiously. Sign with an OV or EV code-signing
certificate before public distribution:

```powershell
signtool sign /fd SHA256 /f certificate.pfx /p "<password>" `
  /tr http://timestamp.digicert.com /td SHA256 `
  ImGuiTemplate.exe
```

A self-signed certificate is suitable only for internal test machines that trust
the root certificate. It does not establish public publisher reputation.
