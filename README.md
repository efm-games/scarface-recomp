# Scarface: The World Is Yours — XBE Recomp

An unofficial native 64-bit PC port of the original Xbox version of *Scarface: The World Is Yours*, built with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

The game's x86 code is statically recompiled ahead of time and the Xbox kernel/XDK APIs are implemented natively on Windows (Direct3D 12 renderer, XAudio2 audio). The result runs from the user's own copy of the Xbox game.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to play or build. The disc image is **not** part of this repository or its releases.

## Installation

- Put your Xbox disc image in the `assets` folder beside `Scarface.exe` (a game-partition `.xiso` or a whole-disc dump both work), or extract the game's files there with `default.xbe` beside its data folders.
- Run `Scarface.exe`.
- On a release build the first start compiles the game's code for your PC once, using the bundled recompiler and Clang.

## Features

### Display
- Windowed, borderless and fullscreen modes, internal resolution scaling, anisotropic filtering, FPS cap and vsync through `settings.toml`.
- Widescreen output.

### Graphics
- Native Direct3D 12 renderer translating the Xbox's Direct3D 8 pipeline, vertex programs and pixel combiners to HLSL.

### Controller
- Xbox, PlayStation and Switch controllers through SDL3, with hot plugging and rumble.
- Keyboard and mouse support is planned as a PC-oriented addition on top of the pad layer.

### Audio
- DirectSound mixed in software through XAudio2, including Xbox ADPCM.

### General
- Reads the game from an ISO in `assets` or from extracted files.
- Bink movie fixes, crash dumps and a settings overlay.

## Building

### 1. Requirements
- Windows 10/11 64-bit
- Visual Studio 2022 with **Desktop development with C++**
- Clang/LLVM with `clang++` and `lld-link` on `PATH`
- CMake 3.25+, Ninja and Git on `PATH`

### 2. Game files
Put the Xbox version's `default.xbe` in `assets` with its data folders beside it, or put the `.iso` there and extract `default.xbe` from it for the build.

### 3. Build
Run `build.bat`. The first build recompiles the game and takes a while; the executable is written to `out\build\release`.

The XBE used for the build must be the same release as the one in the image you run: the executable checks its SHA-1.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) — recompiler, runtime and build system.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis), [toml++](https://github.com/marzer/tomlplusplus), [Dear ImGui](https://github.com/ocornut/imgui) and [SDL](https://github.com/libsdl-org/SDL).

*Scarface: The World Is Yours* is a trademark of its respective owners. This is a non-commercial fan project and is not affiliated with or endorsed by them.
