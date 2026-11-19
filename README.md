# Scarface: The World Is Yours — XBE Recomp

An unofficial native 64-bit Windows port of the original Xbox version of *Scarface: The World Is Yours*, built with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

The game's x86 code is statically recompiled ahead of time and the Xbox kernel/XDK APIs are implemented natively on Windows (Direct3D 12 renderer, XAudio2 audio). The result runs from the user's own copy of the Xbox game.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to play or build. The disc image is **not** part of this repository or its releases.

> [!NOTE]
> This project was built with AI assistance using OpenCode and the Big Pickle agent. The AI helped with reverse-engineering analysis, implementation, documentation and release preparation under human direction and testing. No AI-generated game assets are included; see [AI assistance](#ai-assistance) for details.

## Release

This is the `v1.2.0` release. The game boots from your own Xbox disc image, reaches the menus and is playable start to finish with a keyboard and mouse or a gamepad. See [Known issues](#known-issues) for the remaining rough edges.

### What's new in v1.2.0

- Fixed the first-mission lighting/texture flash at Tony's mansion. The world now keeps its normal diffuse lighting during the effect instead of drawing the raw unlit texture. The diagnosis and fix are in [docs/PROJECT-NOTES.md](docs/PROJECT-NOTES.md).

Earlier work is described in the v1.1.1 release notes (PC/PS2 save conversion and tutorial prompts) and [docs/hud-glyphs-hotkeys.md](docs/hud-glyphs-hotkeys.md) (HUD key prompts).

## Installation

1. Download `Scarface-v1.2.0.zip` from the [Releases](https://github.com/efm-games/scarface-recomp/releases) page and extract it anywhere. The archive contains no game data.
2. Run `Scarface.exe`.
3. Click **Browse for the disc image...** and pick your Xbox `.iso`/`.xiso` (a whole-disc dump also works), or **Use an extracted folder...** and pick the folder that holds `default.xbe`. The launcher remembers the path.
4. Click **Play (A)** or press gamepad **A**. The first start compiles the game's code for your PC once with the bundled recompiler and Clang; it takes a few minutes and about 1 GB of free disk space. Every later start goes straight into the game.

The executable checks the game's `default.xbe` SHA-1 (`9367786d1b3bc9b35208a29a7ddca29487af0f5d`). If your image is a different release, the launcher warns you and lets you try it anyway.

Settings are written to `settings.toml` beside the executable. Press **F1** in game to open the same options and controls the launcher offers.

## Requirements

- Windows 10 or 11, 64-bit.
- A GPU with Direct3D 12 support (feature level 11.0). The included Direct3D 12 driver is the only renderer.
- Your own copy of the Xbox release of the game. A game-partition `.xiso`, a whole-disc image, or extracted files all work.
- ~1 GB of free disk space for the generated code beside the executable, plus space for saves.

The Visual C++ runtime is included in the release archive; nothing else needs to be installed.

## Controls

Defaults follow the official PC manual. The keyboard and mouse layer synthesizes the Xbox pad state, so it works with the game's unmodified menus and HUD.

| On foot | Vehicle |
| --- | --- |
| WASD move, mouse look | W gas, S brake/reverse, A/D steer |
| Space / left click fire | Space handbrake |
| Caps Lock / right click lock-on | Caps Lock / right click lock-on |
| F / middle click alternate fire | LMB / Ctrl / F fire |
| Shift sprint, Ctrl crouch, Enter wall cover | Z / middle click look back, C camera |
| E interact, R reload, T taunt, V rage | E enter/exit, T horn/taunt |
| 1/2/3 and mouse wheel weapons, Tab SAT, Esc pause | 1/2/3 and mouse wheel weapons, Tab SAT, Esc pause |

Menus work with the mouse or the arrow keys, Enter (select) and Esc (back). Xbox, PlayStation and Switch controllers are detected automatically and can be used alongside the keyboard and mouse; the active device switches on the fly. See [docs/controls.md](docs/controls.md) for the complete tables.

Mouse sensitivity X and Y are separate options, along with Y inversion. The old single `kbm_sensitivity` value is migrated to both axes.

## Features

### Display
- Windowed, borderless and fullscreen modes, resolution choice, internal render scale, anisotropic filtering, optional FXAA antialiasing, FPS cap, vsync and an optional FPS counter, from the launcher, the F1 overlay or `settings.toml`.
- Experimental 60 fps mode (off by default) that skips the game's own 30 Hz wait; see [docs/experimental-fps.md](docs/experimental-fps.md).
- Widescreen output.

### Graphics
- Native Direct3D 12 renderer translating the Xbox's Direct3D 8 pipeline, vertex programs and pixel combiners to HLSL.
- Optional replacement textures and texture dumping.

### Audio
- DirectSound mixed in software through XAudio2, including Xbox ADPCM.

### Controller and keyboard/mouse
- Xbox, PlayStation and Switch controllers through SDL3, with hot plugging and rumble.
- Keyboard and mouse support following the official PC manual, with automatic on-foot/vehicle bindings.
- Optional HUD key prompts replace pad glyphs with plain-text key labels while keyboard/mouse is active (or before any input). Line-specific rules follow the PC manual: Start/Load/Accept/Cover are `[Enter]`, dialog `[CLOSE]` and cancel/back/create are `[Esc]`, sprint is `[Shift]`, taunt is `[T]`, rage is `[V]` and pick up/interact is `[E]`. Plain triangle/Y captions get an `[E] ` prefix; the tape-player glyph before `PLAY`/`STOP` is replaced by `[T] `, and the Shuffle caption becomes `[Enter] SHUFFLE ON/OFF` (the original "TURN" wording is kept for gamepad prompts). Outside the music library, the original glyphs return as soon as a controller is used. The music library snapshots the active input when its page opens and keeps that style until you leave.
- Optional GTA-style gamepad preset (RT accelerate, LT brake), off by default.

### Launcher, options and saves
- Launcher with a disc image picker, an extracted-folder picker, display/input options, the intro-skip toggle and an **Unlock all cheats** option that makes every pause-menu cheat code selectable from a new game (effects are still chosen manually). Gamepad **A** activates the **Play (A)** button just like the ready screen's Start (A) button.
- In-game options overlay (**F1**) covering display, controls, advanced rendering and game settings.
- Save manager that keeps the game's Xbox-format saves in `hdd\E\UDATA\5655003F` beside the executable, imports Xbox save folders, and converts PC or PS2 saves into a free or chosen Xbox profile.
- Screenshots with **F2**, saved as BMP files in `screenshots` beside the executable.
- Optional intro skip (off by default), crash dumps and log files.
- Frame and state captures with **F2**/**F3** and the debug variables in [docs/debugging.md](docs/debugging.md).
- History of the fixed white UI panel bug in [docs/white-ui-panels.md](docs/white-ui-panels.md).
- Technical notes on HUD glyphs and key prompts in [docs/hud-glyphs-hotkeys.md](docs/hud-glyphs-hotkeys.md).
- Project notes on the first-mission lighting investigation and the DLSS decision in [docs/PROJECT-NOTES.md](docs/PROJECT-NOTES.md).

## Known issues

- Texture pop-in during play.
- Save import and conversion are new; keep an Xbox profile as the template and back up unusual save folders before importing or converting.
- Not every Xbox feature has been exercised.

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

`release.bat` builds the same executable and packages a release archive (executable, empty `assets` folder, compiler, runtime DLLs, this README and the controls page) using Windows' built-in `tar`.

The XBE used for the build must be the same release as the one in the image you run: the executable checks its SHA-1.

## AI assistance

This project was developed with AI assistance using OpenCode and the Big Pickle agent. The AI helped with reverse-engineering analysis, native runtime, renderer and input work, documentation, build and release preparation, and testing support. The maintainer directed the work, made the design decisions, integrated the changes and playtested the result. No AI-generated art, music, sound effects or placeholder game assets are included; the game uses the original Xbox assets supplied by the player.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) — recompiler, runtime and build system.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis), [toml++](https://github.com/marzer/tomlplusplus), [Dear ImGui](https://github.com/ocornut/imgui) and [SDL](https://github.com/libsdl-org/SDL).

*Scarface: The World Is Yours* is a trademark of its respective owners. This is a non-commercial fan project and is not affiliated with or endorsed by them.
