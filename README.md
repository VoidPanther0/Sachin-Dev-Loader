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
- Supabase username and password login before the main screen
- Segoe UI fonts loaded from the system font directory
- Static C++ runtime, so the executable does not need the VC++ Redistributable
- Embedded `asInvoker` application manifest, so the app never requests elevation
- Dear ImGui 1.93.1 WIP vendored in `src/gui` (no submodules, no package manager)

## Login

The login screen checks credentials against the `sachin-dev-loader` Supabase
project before it shows the main window.

- The client sends `username` and `password` to Supabase Auth. The account email
  is `<username>@loader.local`.
- After a successful password check, the client calls `public.account_login`.
  That function rejects banned accounts and accounts bound to another device.
  The first successful login binds the current hardware ID.
- Session tokens stay in memory. The refresh token is used by a background
  heartbeat, and the account is re-checked every five minutes.
- *Remember username* stores only the username in
  `%APPDATA%\SachinDevLoader\settings.ini`. Passwords and tokens are never
  written to disk.

Create an account from the Supabase SQL editor:

```sql
select public.admin_create_account('username', 'password');
```

Reset a password or clear a hardware binding:

```sql
select public.admin_set_password('username', 'new-password');
select public.admin_reset_hwid('username');
```

The client uses a publishable key in `src/supabase_auth.cpp`. That key is safe to
ship. Never put the `service_role` key in the application.

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
  ui.cpp              Theme, layout, login screen, and widget gallery
  supabase_auth.cpp   Supabase login, session refresh, HWID check
  supabase_auth.h     Auth API used by ui.cpp
  json_lite.h         Minimal JSON parser for Supabase responses
  app.h               Shared globals and app entry points
  resource.h          Icon resource identifier
  Resources/          app.ico, app.manifest, version.rc
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
update the build commands in `.github/workflows/build-release.yml`. Also update
`assemblyIdentity` in `src/Resources/app.manifest` and the strings in
`src/Resources/version.rc` so the binary metadata matches the new name.

## Release

Push a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches
`ImGuiTemplate-v1.0.0-win64.zip` to the GitHub Release. The zip contains the
executable, this README, and `SHA256SUMS.txt`.

## Code signing and antivirus false positives

Release builds are unsigned by default. Windows Defender, SmartScreen, and other
engines treat new unsigned executables cautiously. Heuristic detections can name a
clean build as a trojan. This template has been reported as
`Trojan:Script/Conteban.A!ml` by Defender while every file in this repository
scanned clean; that name is a known machine-learning false positive on unsigned
archives and installers.

If one engine flags a release that you built yourself:

1. Confirm the SHA-256 value matches `SHA256SUMS.txt` from the release, and scan
   the file with the Microsoft Safety Scanner and a second engine.
2. Submit the file to Microsoft as an incorrect detection:
   <https://www.microsoft.com/en-us/wdsi/filesubmission>. Include the detection
   name, the Defender security intelligence version, the file hash, and the public
   repository URL. Microsoft usually corrects a false positive in one to three
   days.
3. Update Defender, rebuild, and scan the new binary again.

Code signing is the durable fix. Sign with an OV or EV code-signing certificate
before public distribution:

```powershell
signtool sign /fd SHA256 /f certificate.pfx /p "<password>" `
  /tr http://timestamp.digicert.com /td SHA256 `
  ImGuiTemplate.exe
```

The release workflow signs the executable automatically when these repository
secrets exist:

| Secret                       | Value                                |
| ---------------------------- | ------------------------------------ |
| `WINDOWS_CERT_PFX_BASE64`    | Base64 of the `.pfx` file            |
| `WINDOWS_CERT_PFX_PASSWORD`  | Password for the `.pfx` file         |

A self-signed certificate is suitable only for internal test machines that trust
the root certificate. It does not establish public publisher reputation.

For local development only, you can exclude the build output directory so Defender
does not delete each new build:

```powershell
Add-MpPreference -ExclusionPath "C:\path\to\repo\x64\Release"
```

Remove the exclusion when you are done:

```powershell
Remove-MpPreference -ExclusionPath "C:\path\to\repo\x64\Release"
```

Never add an exclusion for files downloaded from other people, and never ask users
to disable antivirus protection to run a release.
