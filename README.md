# Dear ImGui Template

A Windows desktop loader UI built with C++20, Win32, DirectX 11, and Dear ImGui.
It uses a frameless dark window, a Supabase login screen, and a dashboard with
the common ImGui widgets: buttons, text fields, numeric inputs, sliders,
checkboxes, radio buttons, combo boxes, color editing, progress, and tree nodes.

Use this repository as a starting point for your own loader. The interface is
local. Supabase is used only for authentication, build checks and the component
list; no database key is embedded in the executable.

## Features

- Frameless 1160x700 window with a custom title bar and rounded corners
- Login screen separate from a three-column dashboard
- Custom dark theme with a live accent color picker
- Supabase username and password login before the main screen
- Segoe UI fonts loaded from the system font directory
- Static C++ runtime, so the executable does not need the VC++ Redistributable
- Embedded `asInvoker` application manifest, so the app never requests elevation
- Dear ImGui 1.93.1 WIP vendored in `src/gui` (no submodules, no package manager)

## Login

The login screen checks credentials against the `sachin-dev-loader` Supabase
project before it shows the main window.

- The client calls the `loader-auth` Edge Function. The function verifies the
  password with Supabase Auth, then calls `public.account_login`.
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

### Loading screens and server checks

The loader shows two loading screens. Every step is decided by the server.

1. **Startup** (when the exe opens): *Connecting to server*, *Checking loader
   integrity*, *Downloading components*. The client sends its version and the
   SHA-256 of its own exe to `loader-auth` (`action: "startup"`), which calls
   `public.loader_startup()`. Maintenance mode, an old version or an unknown
   exe hash stops the loader with a Retry button.
2. **Login** (after the Login button): *Authenticating*, *Verifying account
   and device*, *Running security checks*. After the password check,
   `action: "bootstrap"` calls `public.loader_bootstrap()` with the user token.
   It re-runs `account_status()` (ban, HWID) and the build checks. If any check
   fails, the token is revoked and the form shows the reason. The dashboard
   opens only after every step passes.

Apply `supabase/migrations/20261026000000_loader_checks.sql`, then redeploy the
function:

```bash
supabase db push
supabase functions deploy loader-auth --no-verify-jwt
```

Manage the checks from the SQL editor:

```sql
-- Register a release build (hash from SHA256SUMS.txt), then enforce hashes.
insert into public.loader_builds (exe_hash, version) values ('<sha256>', '0.1');
update public.loader_config set enforce_build_hash = true;

-- Block old clients or turn on maintenance. Rejected clients receive release_url.
update public.loader_config set min_version = '0.1.1';
update public.loader_config set maintenance = true, maintenance_message = 'Back at 6 PM';
update public.loader_config set release_url = 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest';

-- Automatic update: direct executable URL and its SHA-256.
update public.loader_config
set update_url = 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest/download/ImGuiTemplate.exe',
    update_sha256 = '<sha256>';

-- Components shown to the client. requires_login = true hides one until login.
insert into public.loader_components (name, version, description, requires_login)
values ('Core', '0.1', 'Base runtime', false);

-- Audit log of every startup and bootstrap check.
select * from public.loader_check_log order by id desc limit 50;
```

Keep `enforce_build_hash = false` while developing: every rebuild changes the
exe hash.

The client uses a publishable key in `src/auth/supabase_auth.cpp`. That key is safe to
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
  ui.cpp              Shared UI helpers and app lifecycle
  ui.h                Shared UI API and screen entry points
  screen/
    login.cpp         Startup, login form, signing phases and Supabase auth
    home.cpp          Main dashboard, title bar and footer
  auth/
    supabase_auth.cpp Supabase login, session refresh, HWID check
    supabase_auth.h   Auth API used by the login screen
    json_lite.h       Minimal JSON parser for Supabase responses
  app.h               Shared globals and app entry points
  Resources/          app.ico, app.manifest, version.rc, resource.h
  gui/                Vendored Dear ImGui
    backends/         Win32 + DirectX 11 platform/renderer backends
ImGuiTemplate.sln
ImGuiTemplate.vcxproj
```

`main.cpp` owns the window and rendering. `ui.cpp` implements the shared
helpers and the app lifecycle, then dispatches to one screen at a time. Each
screen lives in its own file under `src/screen`.

## Customize

| What                | Where                                              |
| ------------------- | -------------------------------------------------- |
| Window title        | `app::kTitle` in `src/app.h`                       |
| Window size         | `app::kWidth`, `app::kHeight` in `src/app.h`       |
| Version label       | `app::kVersion` in `src/app.h`                     |
| Theme colors        | `kBg`, `kPanel`, `kText`, and related constants in `src/ui.h` |
| Login screen        | `src/screen/login.cpp`                             |
| Main dashboard      | `src/screen/home.cpp`                              |
| Dashboard cards     | `draw_inputs_card()`, `draw_selection_card()`, `draw_actions_card()` in `src/screen/home.cpp` |
| Demo state          | `ui::State` and `ui::reset_state()` in `src/ui.cpp` |
| Fonts and sizes     | `ui::load_fonts()` in `src/ui.cpp`                 |
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
git tag v0.1
git push origin v0.1
```

The workflow builds `Release|x64` and uploads a two-file artifact:
`ImGuiTemplate.exe` and `SHA256SUMS.txt`. The GitHub Release contains only
`ImGuiTemplate.exe` so the direct download URL can update old clients.

When the server rejects an old or modified build it returns `update_url` and
`sha256`. The loader downloads the executable to `%TEMP%`, verifies the hash,
and restarts through a small `--apply-update` helper that replaces the old file.
No browser or release page is opened.

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
