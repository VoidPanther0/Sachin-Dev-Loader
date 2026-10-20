# Sachin Dev Loader

A small Windows desktop client built with C++20, Win32, DirectX 11, and Dear ImGui.

## What it does

- Username and password sign-in through Supabase Auth
- Optional saved login, encrypted with Windows DPAPI for the current Windows user
- Background session refresh and account status check
- A simple account dashboard

## What it does not do

- No game integration
- No process injection, memory reading, or memory writing
- No MAC address, disk serial, or hardware ID modification
- No shell execution, WMI, service changes, or registry writes

The application requests no elevation. It runs as the signed-in user.

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

## Release

Push a version tag:

```bash
git tag v1.0.2
git push origin v1.0.2
```

The workflow builds `Release|x64`, writes a SHA-256 checksum, and attaches `SachinDevLoader-v1.0.2-win64.zip` to the GitHub Release. The zip contains the executable, this README, and `SHA256SUMS.txt`.

## Security notes

- Saved credentials are encrypted with `CryptProtectData` and can only be decrypted by the same Windows user on the same machine.
- Network traffic uses WinHTTP with the system certificate store and TLS 1.2.
- The Supabase publishable key is a public client key, not a service-role secret.
- A best-effort machine fingerprint is used for account binding. Failed hardware reads are treated as empty fields instead of aborting sign-in.

## Code signing

Release builds are unsigned by default. For public distribution, sign the executable with an OV or EV code-signing certificate from a public CA:

```powershell
signtool sign /fd SHA256 /f certificate.pfx /p "<password>" `
  /tr http://timestamp.digicert.com /td SHA256 `
  SachinDevLoader.exe
```

A self-signed certificate is suitable only for internal test machines that trust the root certificate. It does not establish public publisher reputation.
