# Skyliner 700 — Xbox 360 Edition

[Русская версия](README.ru.md)

This branch contains the native **Xbox 360** port of Skyliner 700: a small flight simulator written in C. It is a separate codebase branch from the Windows release (`main`) and is built as an Xbox 360 `.xex`.

![Language: C](https://img.shields.io/badge/language-C-00599C?logo=c&logoColor=white)
![Platform: Xbox 360](https://img.shields.io/badge/platform-Xbox%20360-107C10?logo=xbox&logoColor=white)
![Renderer: Xenos](https://img.shields.io/badge/renderer-Xenos-4B0082)

## Features

- Simplified flight model with takeoff, stalls, banking turns, landing, and terrain contact
- Controller-first Xbox 360 input, main menu, pause menu, HUD, telemetry, automated flight test, and free camera
- Procedural terrain with hills, runway, taxiway, and low-poly hangars
- DDS runtime textures: DXT1 terrain, DXT5 clouds, and a 1024×1024 aircraft livery
- Sky dome, scrolling clouds, fog, anisotropic texture filtering, and 720p rendering

## Xbox 360 enhancements

In addition to the shared game and flight code, this port has an Xbox-specific renderer and presentation path:

- Native 1280×720 rendering with 4× MSAA using the Xenos 10 MB eDRAM through explicit predicated tiling and resolves
- Gamepad-native controls and menu navigation
- DDS assets and platform-oriented filtering for sharper terrain and aircraft textures
- Uses the Xbox 360 GPU and memory architecture directly; it is not the Windows executable running in compatibility mode

## Controls

| Controller input | Action |
| --- | --- |
| Left stick | Pitch and roll |
| Right stick | Look around in free-camera mode |
| Left / right trigger | Decrease / increase throttle |
| Left / right bumper | Yaw |
| `Back` | Toggle chase and free camera |
| `D-pad Down` | Toggle detailed telemetry |
| Press both sticks | Start the automated flight test |
| `Start` | Open or close the pause menu |
| `D-pad Up` / `D-pad Down` or left stick | Navigate menus |
| `A` | Confirm a menu action |

## Project layout

```
src/common/        Shared C gameplay: flight model, scripted flight, terrain, aircraft data
src/win32/         Windows implementation retained for source sharing; not used by the Xbox project
platforms/xbox360/ Xbox 360 Visual Studio 2010 solution and platform bootstrap
assets/source/     Editable source artwork
assets/xbox/       Xbox-ready DDS runtime assets
tests/             Headless flight-model checks
```

Open the solution at `platforms/xbox360/Skyliner700Xbox360/Skyliner700xbox360.sln`.

## Building and running

Building this project requires a **properly licensed Microsoft Xbox 360 XDK** with its supported Visual Studio 2010 toolchain. XDK headers, libraries, tools, samples, and deployment files are proprietary and are intentionally not included in this repository.

The supported runtime target is an official Xbox 360 development kit. Build the `Release | Xbox 360` configuration in Visual Studio and deploy the generated `.xex` together with the `assets` directory to the devkit.

The executable may run on a modified JTAG/RGH/BadUpdate/BadAvatar retail console, but that configuration is untested and unsupported. This project does not provide instructions, files, or guarantees for bypassing platform security; use legitimate development hardware and software.

## Technology

- C / C++ Xbox 360 application bootstrap
- Xbox 360 XDK + Visual Studio 2010
- Xenos renderer with explicit 720p 4× MSAA predicated tiling
- Shared deterministic flight-model tests

## License

The project source is licensed under the [MIT License](LICENSE). This license does not grant rights to Microsoft XDK components or any other proprietary SDK material.

Project feedback and bug reports are welcome in [Issues](../../issues).
