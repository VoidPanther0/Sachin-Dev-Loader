# PLEASE STAR!! ⭐

## Sachin Dev Loader - The best freemium imgui loader you will ever see.
Don't forget about joining our discord community! https://discord.gg/CsuXTegewj

### Features

<ul>
  <li>Custom items
    <ul>
      <li>Custom Checkbox</li>
      <li>Custom Selectable</li>
      <li>Custom Input text</li>
      <li>Custom Buttons</li>
      <li>Fade In / Out effect</li>
      <li>Blurry Background</li>
    </ul>
  </li>
</ul>
<ul>
  <li>External stuff
    <ul>
      <li>Supabase username and password login</li>
      <li>Font awesome support</li>
    </ul>
  </li>
</ul>

## SHOWCASE 
Here you can take a look of the menu (fully showcase on https://www.youtube.com/watch?v=2gZ4CpatM68)<p align="center">
<div style="display: flex; flex-direction: row;">
  <img src="https://github.com/user-attachments/assets/be85a27f-e945-4406-87f1-ab45a0ab154a" width="750" height="500" style="margin: 10px;" />  
  <img src="https://github.com/user-attachments/assets/b7138de7-f94a-4f27-9471-a3c71e650e34" width="750" height="500" style="margin: 10px;" />
</div>

## BUILD AND RELEASE

GitHub Actions builds the loader on every push to `main` and on every pull request. The workflow file is `.github/workflows/build-release.yml`.

To publish a release, push a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow builds `Release|x64` and attaches `SachinDevLoader-v1.0.0-win64.zip` to the GitHub Release. The zip contains the executable and the DirectX runtime DLLs.

Local build requirements: Visual Studio 2022 with the C++ workload. The workflow installs the legacy DirectX SDK headers and libraries from the `Microsoft.DXSDK.D3DX` NuGet package, so no system-wide DirectX SDK install is needed for CI.
