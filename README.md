# Sachin Dev Loader

A Windows desktop interface built with C++20, Win32, DirectX 11, and Dear ImGui.

This repository is a **UI-only build**. It contains no account system, network client, hardware fingerprinting, process interaction, or game integration. Every control is local and visual.

## Interface

- Frameless 1120x700 dark dashboard
- Dashboard, Loader, and Settings pages
- Custom-drawn navigation, cards, buttons, toggles, and load animation
- Four selectable accent colors
- System Segoe UI fonts loaded at runtime (no embedded font blobs)
- Mock load progress for interface preview only

## What it does not do

- No login, registration, session, or saved credentials
- No HTTP, WinHTTP, sockets, or Supabase calls
- No HWID, MAC address, disk serial, or machine fingerprint reads
- No process enumeration, injection, memory access, or game interaction
- No shell execution, services, registry writes, or startup persistence
- No elevation request

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with the Desktop development with C++ workload
- Windows 10/11 SDK

No legacy DirectX SDK is required. Dear ImGui's Direct3D 11 backend links against the Windows SDK copy of `d3dcompiler`.

Release builds use the static C++ runtime, so the executable does not need the Visual C++ Redistributable on the target machine.

## Build

```powershell
msbuild SachinDevLoader.sln /m /p:Configuration=Release /p:Platform=x64
```

The executable is written under `x64\Release\` at the repository root.

For a debug build:

```powershell
msbuild SachinDevLoader.sln /m /p:Configuration=Debug /p:Platform=x64
```

## Layout

```
src/
  Headers/       core.h, resource.h
  Resources/     app.ico, version.rc
  Sources/       main.cpp, ui.cpp
ext/ImGui/       vendored Dear ImGui 1.89.4
```

`main.cpp` owns the Win32 window, DirectX 11 device, and render loop. `ui.cpp` owns fonts, colors, layout, and every drawn widget.

## Release

Push a version tag:

```bash
git tag v2.0.0
git push origin v2.0.0
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches `SachinDevLoader-v2.0.0-win64.zip` to the GitHub Release. The zip contains the executable, this README, and `SHA256SUMS.txt`.

## Unsigned builds

Release builds are unsigned by default. Windows Defender and SmartScreen may treat a new unsigned executable cautiously. Sign with an OV or EV code-signing certificate before public distribution:

```powershell
signtool sign /fd SHA256 /f certificate.pfx /p "<password>" `
  /tr http://timestamp.digicert.com /td SHA256 `
  SachinDevLoader.exe
```

A self-signed certificate is suitable only for internal test machines that trust the root certificate. It does not establish public publisher reputation.
