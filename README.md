# Skyliner 700

[Русская версия](README.ru.md)

A small flight simulator written in pure **C** with a **Direct3D 9 fixed-function** renderer. It uses no game engine, C++, D3DX, or shaders, and is designed to remain compatible with older DirectX 9 hardware.

![Language: C](https://img.shields.io/badge/language-C-00599C?logo=c&logoColor=white)
![Graphics: Direct3D 9](https://img.shields.io/badge/graphics-Direct3D%209-107C10)
![Platform: Windows](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)

## Features

- Simplified flight model with thrust, lift, drag, stalls, takeoff, landing, and terrain contact
- Procedural terrain with hills, a runway, taxiway, and low-poly hangars
- Textured aircraft, sky dome, scrolling clouds, fog, and HUD
- Third-person chase camera and free camera
- Main menu, pause menu, automated test flight, and telemetry mode
- Deterministic CTest flight-physics tests that run without a window or D3D9

## Controls

| Key | Action |
| --- | --- |
| `Up` / `Down` | Pitch |
| `Left` / `Right` | Roll and turn |
| `Shift` / `Ctrl` | Increase / decrease throttle |
| `Q` / `E` | Yaw |
| `R` | Reset to the runway |
| `C` | Toggle chase and free camera |
| `W` `A` `S` `D` + mouse | Move and look in free camera |
| `Page Up` / `Page Down` | Move up / down in free camera |
| `F5` | Automated takeoff, circuit, and landing test |
| `F6` | Detailed flight telemetry |
| `Esc` | Pause menu |

## Downloads

Get the latest build from [Releases](../../releases).

| Package | Target |
| --- | --- |
| `Skyliner700-1.0.0-win32-xp.zip` | Windows XP SP2/SP3 and later 32-bit Windows |
| `Skyliner700-1.0.0-win64.zip` | 64-bit Windows |

Extract the complete archive before running the game. The `assets` directory must stay next to `Skyliner700.exe`.

## Requirements

- Windows XP SP2/SP3 or newer
- DirectX 9.0c-compatible GPU with Shader Model 2.0 and 64 MB VRAM
- 512 MB RAM
- 1024×768 display
- 100 MB free storage

The x86 build was validated in a Windows XP SP2 VMware guest. Real Radeon Xpress 200 hardware remains the final performance target.

## Building

Install CMake, Ninja, and MSYS2 MinGW. Run these commands from the repository root.

### Windows x64 (Clang)

```powershell
C:\msys64\mingw64\bin\cmake.exe --preset clang-release
C:\msys64\mingw64\bin\cmake.exe --build --preset clang-release
```

Output: `cmake-build-release\Skyliner700.exe`.

### Windows XP x86 (GCC)

```powershell
C:\msys64\mingw32\bin\cmake.exe --preset gcc-x86-xp-release
C:\msys64\mingw32\bin\cmake.exe --build --preset gcc-x86-xp-release
C:\msys64\mingw32\bin\ctest.exe --preset gcc-x86-xp-release --output-on-failure
```

Output: `cmake-build-gcc-x86-xp-release\Skyliner700.exe`. This build targets `pei-i386`, subsystem version 5.1, and has no D3DX dependency. See [Windows XP notes](docs/windows-xp.md) for deployment details.

## Linux

The Windows build should run through Wine with DXVK, which translates D3D9 to Vulkan. Install Wine, DXVK, and the 32-bit Vulkan driver for your GPU, then run the game together with its `assets` directory.

```bash
WINEPREFIX="$HOME/.wine-skyliner700" wine Skyliner700.exe
```

## Technology

- C11
- Win32 API
- Direct3D 9 fixed-function pipeline
- CMake + Ninja
- CTest

## License

Licensed under the [MIT License](LICENSE).

Project feedback and bug reports are welcome in [Issues](../../issues).
