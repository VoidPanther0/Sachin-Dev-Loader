# Sachin Game Loader

A Windows desktop game-launcher utility built with C++17/20, Win32, DirectX 11, and
Dear ImGui. Dark/navy UI with a swappable accent theme, a local games library,
per-game configuration, a DLL/file validation tool, logs, and local-only sign-in.

## Features

- Frameless, **resizable** window (drag any edge; double-click the top bar to
  maximize/restore) with a custom title bar and rounded corners
- Animated splash screen and a local sign-in / create-account screen
  (demo authentication - see **Authentication** below)
- **Dashboard** - welcome header, quick stats, search/favorites filter, a game
  grid, recent activity, and quick links to the other screens
- **Games Library** - add/edit/remove games (name, executable, icon path,
  version), browse-for-file pickers, search, sort, launch, and a "missing
  path" indicator for entries whose executable can no longer be found
- **DLL Management** - browse/validate a `.dll` file (exists, PE/DLL check,
  x86/x64/ARM64 architecture, size) and keep a reference list of your own
  authorized target applications. **No live code injection is implemented in
  this build** - see **DLL Management** below
- **Game Configuration** - per-game launch arguments, working directory, an
  optional accent-color override, and free-form notes, with Save / Reload /
  Reset
- **Logs** - timestamped, leveled (Info/Success/Warning/Error) activity log
  with search, level filter, auto-scroll, clear, and export to a text file
- **Theme Customizer** - five built-in presets (Purple, Blue, Green, Red,
  Light) plus full custom themes: accent/background/panel/border/text/status
  colors, corner rounding, spacing, window padding, font scale, animation
  intensity, and optional glow. Save, duplicate, delete, reset, and
  import/export themes as `.json` files. The chosen theme persists across
  restarts and applies to every screen immediately
- **Settings** - default game directory, notifications on/off, skip-splash,
  with Save / Cancel / Reset. Anything not actually wired up (startup
  registration, cloud sync, update downloads) is labeled as unavailable
  rather than silently doing nothing
- **About** - version, build date, renderer/windowing info, and third-party
  credits
- Segoe UI fonts loaded at runtime from the system font directory
- Static C++ runtime, so the executable does not need the VC++ Redistributable
- Dear ImGui vendored in `src/gui` (no submodules, no package manager, no
  CMake)

## Requirements

- Windows 10 or Windows 11 (x64)
- Visual Studio 2022 with the **Desktop development with C++** workload
- Windows 10/11 SDK

No legacy DirectX SDK is required. The Direct3D 11 backend links the Windows
SDK copy of `d3dcompiler`.

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
  main.cpp              Win32 window, Direct3D 11 device, resize handling, frame loop
  app.h                 Shared globals, window constants, app entry points
  ui.cpp                app::Init/Render/Shutdown, window chrome, sidebar, routing
  resource.h            Icon resource identifier
  Resources/            app.ico, version.rc
  gui/                  Vendored Dear ImGui
    backends/           Win32 + DirectX 11 platform/renderer backends
  core/                 No ImGui here - persistence, auth, logging, theme data
    json.h/.cpp         Minimal JSON reader/writer (no third-party dependency)
    paths.h/.cpp        %LOCALAPPDATA%\SachinLoader\ layout
    settings.h/.cpp     General app settings (settings.json)
    games.h/.cpp        Games library + per-game config (games.json)
    theme.h/.cpp        Theme presets, custom themes, persistence (themes/*.json)
    auth.h/.cpp         Local demo account store (accounts.json)
    sha256.h/.cpp       Password hashing for the local demo account store
    dllmgr.h/.cpp       DLL file validation + authorized-target list (no injection)
    launcher.h/.cpp     Starts a user-configured game executable
    dialogs.h/.cpp      Native open/save/folder pickers
    clock.h             Millisecond timestamp helper
    logs.h/.cpp         Thread-safe activity log
  ui/                   Theme-aware rendering shared by every screen
    palette.h/.cpp      Per-frame ImU32 palette derived from the active theme
    widgets.h/.cpp      Buttons, fields, cards, tabs, toggles, modal, toasts, icons
    fonts.h/.cpp        Runtime font loading
    branding.h/.cpp     Logo, wordmark, starfield/background
    state.h/.cpp        Shared navigation + session state
    layout.h            Sidebar/topbar size constants
    screens.h           Declares every page_*() screen entry point
  screens/              One file per screen (splash, login, dashboard, library,
                         dllmanagement, gameconfig, logs_screen,
                         thememanager_screen, settings_screen, about)
Loader.sln
Loader.vcxproj
config/games.example.json   Reference only - schema sample, never loaded
themes/example-theme.json   Reference only - a theme file you can Import
```

Actual user data (settings, games, themes, accounts, logs) lives under
`%LOCALAPPDATA%\SachinLoader\`, not in the repository.

## Customize

| What                 | Where                                          |
| -------------------- | ----------------------------------------------- |
| Window title         | `app::kTitle` in `src/app.h`                    |
| Default window size  | `app::kDefaultWidth/Height` in `src/app.h`      |
| Minimum window size  | `app::kMinWidth/Height` in `src/app.h`          |
| Version label        | `app::kVersion` in `src/app.h`                  |
| Brand mark/wordmark  | `src/ui/branding.cpp`                           |
| Sidebar items        | `nav_items()` in `src/ui.cpp`                   |
| Built-in theme colors| `make_builtins()` in `src/core/theme.cpp`       |
| A specific screen    | the matching file under `src/screens/`          |
| Reusable widgets     | `src/ui/widgets.cpp`                            |
| Fonts and sizes      | `src/ui/fonts.cpp`                              |
| Executable icon      | `src/Resources/app.ico`                         |
| Version resource     | `src/Resources/version.rc`                      |

To rename the project, rename `Loader.sln`, `Loader.vcxproj`, and
`Loader.vcxproj.filters`, update the `Project(` line in the solution, and
update the build commands in `.github/workflows/build-release.yml`.

## Authentication

Sign-in is a **local-only demo**: accounts are created and checked against a
salted-hash file on this machine (`accounts.json`), with no server, no
network calls, and no session tokens. It is intentionally separated behind
`core/auth.h` so a real HTTPS backend can be swapped in later without
touching any screen code. Do not treat this as production authentication.

## DLL Management

This build validates files and keeps a reference list only:

- **Validate** reads the target `.dll`'s own PE header (existence,
  architecture, DLL flag, size) - it never opens or touches another process
- **Authorized targets** is just a named list you maintain for your own
  reference
- The **Inject** action is intentionally not implemented in this build. The
  screen says so and suggests the safe alternative: call `LoadLibrary` for
  the DLL from inside your own application's own startup code

No process enumeration, process memory access, anti-cheat bypass, or stealth
behavior is implemented anywhere in this app.

## What it does

- Launches an executable the user explicitly added to their own Games
  Library or configured as a DLL-management target, via a plain
  `CreateProcess` - the same as double-clicking the file. No elevation.

## What it does not do

- No real server-side authentication (see **Authentication**)
- No HTTP, WinHTTP, sockets, or remote API calls of any kind
- No hardware IDs, MAC addresses, disk serials, or machine fingerprints
- No process enumeration, code injection, or memory access in another process
- No Windows-startup registration, services, or registry writes
- No elevation request
- No telemetry

## Troubleshooting

- **Fonts look like the ImGui default font.** The app loads Segoe UI from
  `%WINDIR%\Fonts` at runtime; if that file is missing or blocked, it falls
  back to ImGui's built-in font automatically.
- **A theme I imported didn't show up.** Import renames on a name collision
  (e.g. `MyTheme (2)`) rather than overwriting - check the theme list for a
  suffixed name.
- **"Could not initialize Direct3D 11."** on launch usually means the
  machine has no usable GPU driver; the app also tries the software (WARP)
  renderer automatically before giving up.
- **A game won't launch.** The Library screen marks a card "MISSING" when its
  executable path no longer resolves; re-edit the entry with the correct
  path.

## Release

Push a version tag:

```bash
git tag v3.0.0
git push origin v3.0.0
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches
`Loader-v3.0.0-win64.zip` to the GitHub Release. The zip contains the
executable, this README, and `SHA256SUMS.txt`.

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
