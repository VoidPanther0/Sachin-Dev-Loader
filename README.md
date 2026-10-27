# Sachin Dev Loader

A Windows desktop loader built with C++20, Win32, DirectX 11, and Dear ImGui.
It has a frameless dark window, loading screens, a Supabase login and a
dashboard with eight pages: Home, Applications, Downloads, DLL Tools, Theme,
Account, Settings and About.

Supabase is used only for authentication, build checks, components and the
update version; no database key is embedded in the executable.

## Features

- Frameless window with a custom title bar and rounded corners
- One source file per screen in `src/screen`: loading, login, dashboard shell
  and each dashboard page
- Update offered inside the loader (Downloads page): download, SHA-256 verify,
  replace the running exe and restart
- Maintenance mode with a custom message, set from the database
- Custom dark theme with a selectable accent color
- Supabase username and password login with hardware binding
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
   integrity*, *Checking for update*. The client sends its version and the
   SHA-256 of its own exe to `loader-auth` (`action: "startup"`), which calls
   `public.loader_startup()`. Maintenance mode or an unknown exe hash stops the
   loader with the server's message and a Retry button. A build below the
   server `version` gets a fourth step, *Downloading update*, on this same
   screen: the bar follows the download, then the loader restarts by itself
   after a 3, 2, 1 countdown. This is the only place the loader updates.
2. **Login** (after the Login button): *Authenticating*, *Verifying account
   and device*, *Running security checks*. After the password check,
   `action: "bootstrap"` calls `public.loader_bootstrap()` with the user token.
   It re-runs `account_status()` (ban, HWID) and the build checks. If any check
   fails, the token is revoked and the form shows the reason. The dashboard
   opens only after every step passes.

Apply the schema once and deploy the function:

```bash
supabase db push
supabase functions deploy loader-auth --no-verify-jwt
```

`supabase/migrations/20261027140000_loader_v1.sql` is the whole schema. It is
safe to run again; accounts, passwords and hardware bindings are kept.

## Database

Three tables, nothing else.

| Table               | Purpose                                                       |
| ------------------- | ------------------------------------------------------------- |
| `profiles`          | One row per account: ban, hardware ID, expiry, login count    |
| `loader_config`     | One row: `maintenance`, `maintenance_message`, `version` |
| `loader_components` | Apps, games and tools released to the client                  |

Run the `admin_*` helpers as the `postgres` role; they are not reachable from
the client.

```sql
-- Maintenance mode stops every client and shows the message.
update public.loader_config set maintenance = true, maintenance_message = 'Back at 6 PM' where id = 1;
update public.loader_config set maintenance = false where id = 1;

-- Launchable component. The loader downloads the exe, checks the SHA-256,
-- caches it in %APPDATA%\SachinDevLoader\components and starts it.
-- status: online, updating, offline or beta. Only online/beta can launch.
select public.admin_upsert_component(
  'MyTool', '1.0.0', 'What it does',
  'https://example.com/MyTool.exe', '<sha256>', 'online');
select public.admin_set_component_status('MyTool', 'updating');
select public.admin_remove_component('MyTool');

-- Accounts.
select public.admin_create_account('username', 'password');
select public.admin_set_password('username', 'new-password');
select public.admin_reset_hwid('username');
select public.admin_set_banned('username', true);
select public.admin_set_expiry('username', 30);    -- 30 days from now
select public.admin_extend_expiry('username', 30); -- add 30 days
select public.admin_set_expiry('username', null);  -- lifetime
select * from public.admin_list_accounts();
```

Download URLs and hashes are only sent to signed-in accounts.

## Updates

To ship a new version:

1. Bump `app::kVersion` in `src/app.h` and the version strings in
   `src/Resources/version.rc`.
2. Push a matching tag, for example `v1.0.1`. The workflow builds the exe and
   publishes it as a GitHub Release with `SHA256SUMS.txt`.
3. When the release is published, change the version in the database:

```sql
update public.loader_config set version = '1.0.1' where id = 1;
```

`loader-auth` finds the GitHub release tagged `v1.0.1` and sends its download
URL and SHA-256 to the client. Every loader below that version updates on the
startup loading screen: it downloads, verifies the SHA-256, replaces the
running exe (the old one becomes `<exe>.old`) and restarts. Do not change the
database version before the release exists.

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
  main.cpp                  Win32 window, Direct3D 11 device, frame loop
  ui.cpp                    Shared UI helpers and app lifecycle
  ui.h                      Shared UI API and screen entry points
  screen/
    loading.cpp             Startup check and sign-in progress
    login.cpp               Login form
    common/login_common.{h,cpp} Logo, card frame, step list, gradient buttons
    dashboard.cpp           Shell: title bar, sidebar, footer, page switch
    common/dashboard_common.{h,cpp}Theme, icons, widgets and shared state
    home.cpp                Home page
    applications.cpp        Applications page
    downloads.cpp           Downloads page (loader update and component files)
    dll_tools.cpp           DLL Tools page (read-only PE check)
    theme.cpp               Theme page
    account.cpp             Account page
    settings.cpp            Settings page
    about.cpp               About page
  auth/
    supabase_auth.cpp       Supabase login, session refresh, HWID, build check, update
    supabase_auth.h         Auth and update API used by the screens
    json_lite.h             Minimal JSON parser for Supabase responses
  app.h                     Shared globals and app entry points
  Resources/                app.ico, Slogo.png, app.manifest, version.rc, resource.h
  gui/                      Vendored Dear ImGui
    backends/               Win32 + DirectX 11 platform/renderer backends
supabase/
  migrations/               One schema file
  functions/loader-auth/    Edge Function
ImGuiTemplate.sln
ImGuiTemplate.vcxproj
```

`main.cpp` owns the window and rendering. `ui.cpp` implements the shared
helpers and the app lifecycle, then shows the login window or the dashboard.
Each screen lives in its own file under `src/screen`.

## Customize

| What                | Where                                              |
| ------------------- | -------------------------------------------------- |
| Window title        | `app::kTitle` in `src/app.h`                       |
| Window size         | `app::kWidth`, `app::kHeight` in `src/app.h`       |
| Version label       | `app::kVersion` in `src/app.h`                     |
| Theme colors        | `kBg`, `kPanel`, `kText`, and related constants in `src/ui.h` |
| Login form          | `src/screen/login.cpp`                             |
| Loading screens     | `src/screen/loading.cpp`                           |
| Dashboard pages     | `src/screen/<page>.cpp`, listed in `dash::Page` in `src/screen/common/dashboard_common.h` |
| Sidebar and footer  | `draw_sidebar()`, `draw_footer()` in `src/screen/dashboard.cpp` |
| Shared page state   | `ui::State` in `src/ui.h`, `dash::Shared` in `src/screen/common/dashboard_common.h` |
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
git tag v1.0.1
git push origin v1.0.1
```

The workflow builds `Release|x64` and uploads a two-file artifact:
`ImGuiTemplate.exe` and `SHA256SUMS.txt`. The GitHub Release contains both;
`loader-auth` reads the hash from `SHA256SUMS.txt`.

The startup check returns `version`, `update_url` and `sha256` on both
success and rejection. The loader downloads the executable, verifies the hash
and swaps it in place of the running exe (the old one becomes `<exe>.old`).
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
