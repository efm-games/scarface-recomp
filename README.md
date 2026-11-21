# Scarface: The World Is Yours — XBE Recomp

SAY HELLO TO MY LITTLE RECOMP!

<a href="https://recomplauncher.com/game/scarface-recomp"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

An unofficial native 64-bit Windows port of the original Xbox version of *Scarface: The World Is Yours*, built with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

The game's x86 code is statically recompiled ahead of time and the Xbox kernel/XDK APIs are implemented natively on Windows (Direct3D 12 renderer, XAudio2 audio). The result runs from the user's own copy of the Xbox game.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to play or build. The disc image is **not** part of this repository or its releases.

> [!NOTE]
> This project was built with AI assistance using OpenCode and the Big Pickle agent. The AI helped with reverse-engineering analysis, implementation, documentation and release preparation under human direction and testing. No AI-generated game assets are included; see [AI assistance](#ai-assistance) for details.

## Release

This is the `v1.3.1` release. The game boots from your own Xbox disc image, reaches the menus and is playable start to finish with a gamepad or with opt-in keyboard and mouse controls. See [Known issues](#known-issues) for the remaining rough edges.

### What's new in v1.3.1

- **Save manager overhaul.** One four-row table shows all four in-game profiles, with **Import From File** and **Import From Folder** on empty rows. Source detection is automatic for Xbox folders, raw PC/PS2 saves, CodeBreaker `.cbs`, Max Drive `.max`, SharkPort/X-Port `.sps`/`.xps`, EMS `.psu` and virtual-memory-card `.zip` archives. Imports target an explicit profile and no prepared Xbox profile is needed.
- **PS2 conversion is more complete.** The fourth profile id is supported, and the 332-variable CodeBreaker save that crashed after "Profile loads successfully" now imports and loads correctly.
- **Safer in-game options.** The Saves tab remains in the F1 overlay, but shows a warning and disables import, open and delete while the game is running. The pre-game launcher can still manage saves.
- Command-line import now requires an explicit `--save-slot 1|2|3|4`, and occupied profiles are rejected.

### What's new in v1.3.0

- **Gamepad is now the default.** Keyboard and mouse are opt-in. The Controls tab in the launcher and the F1 overlay now includes a **Keyboard to Xbox pad bindings** screen: each key or mouse button sends one fixed Xbox pad control, the game keeps its own context-sensitive handling, and mouse look always drives the right stick. Bindings are saved in `settings.toml`; older keyboard settings files keep keyboard enabled.
- **Multi-monitor cursor fix.** The pointer is clipped to the game window while playing, so it cannot leave the window and click another monitor, which previously stopped mouse look. The clip is released for menus, the overlay and minimize.
- **Experimental options are clearer and off by default.** The GTA-style gamepad preset and PC key prompts are labelled experimental. Key prompts now follow the current bindings instead of a fixed PC-manual table, and the original controller glyphs remain the default.
- No gameplay, graphics or save-format changes. RecompLauncher support from v1.2.1 remains.

### What's new in v1.2.1

- Added [RecompLauncher](https://recomplauncher.com/game/scarface-recomp) support. The archive now ships a `recomplauncher.json` manifest, so the launcher can set up your own copy of the game, show display and control settings on the game's page, and start the recomp directly. Playing without the launcher is unchanged.

### What's new in v1.2.0

- Fixed the first-mission lighting/texture flash at Tony's mansion. The world now keeps its normal diffuse lighting during the effect instead of drawing the raw unlit texture. The diagnosis and fix are in [docs/PROJECT-NOTES.md](docs/PROJECT-NOTES.md).

Earlier work is described in the v1.1.1 release notes (PC/PS2 save conversion and tutorial prompts) and [docs/hud-glyphs-hotkeys.md](docs/hud-glyphs-hotkeys.md) (HUD key prompts).

## Installation

1. Download `Scarface-v1.3.1-Windows-x64.zip` from the [Releases](https://github.com/efm-games/scarface-recomp/releases) page and extract it anywhere. The archive contains no game data.
2. Run `Scarface.exe`.
3. Click **Browse for the disc image...** and pick your Xbox `.iso`/`.xiso` (a whole-disc dump also works), or **Use an extracted folder...** and pick the folder that holds `default.xbe`. The launcher remembers the path.
4. Click **Play (A)** or press gamepad **A**. The first start compiles the game's code for your PC once with the bundled recompiler and Clang; it takes a few minutes and about 1 GB of free disk space. Every later start goes straight into the game.

With [RecompLauncher](https://recomplauncher.com/game/scarface-recomp), the setup screen handles the copy selection and first start: add your disc image or folder on the game's page and press Play. Settings are available there too.

The executable checks the game's `default.xbe` SHA-1 (`9367786d1b3bc9b35208a29a7ddca29487af0f5d`). If your image is a different release, the launcher warns you and lets you try it anyway.

Settings are written to `settings.toml` beside the executable. Press **F1** in game to open the same options and controls the launcher offers.

## Requirements

- Windows 10 or 11, 64-bit.
- A GPU with Direct3D 12 support (feature level 11.0). The included Direct3D 12 driver is the only renderer.
- Your own copy of the Xbox release of the game. A game-partition `.xiso`, a whole-disc image, or extracted files all work.
- ~1 GB of free disk space for the generated code beside the executable, plus space for saves.

The Visual C++ runtime is included in the release archive; nothing else needs to be installed.

## Controls

A gamepad is the default. Xbox, PlayStation and Switch controllers are detected automatically, with hot plugging and rumble. Keyboard and mouse are opt-in: turn on **Keyboard and mouse (opt-in)** in the launcher, the F1 options or `settings.toml`, then use the **Keyboard to Xbox pad bindings** screen in the Controls tab to change keys.

The keyboard layer does not reinterpret keys for each screen. Each key sends one fixed Xbox pad control, and the game decides what that control means in a menu, on foot or in a vehicle. Mouse look always drives the right stick. This is the safest layout because the game keeps its own context-sensitive input handling.

| Xbox pad control | Default key | Typical use |
| --- | --- | --- |
| Left stick | W/A/S/D | Move / steer |
| Right stick | Mouse | Look / camera |
| D-pad | Arrow keys | Weapon selection and menus |
| A | Space | Confirm, sprint, accelerate |
| B | T | Back, taunt, handbrake |
| X | R | Reload, brake |
| Y | E | Interact, enter or exit a vehicle |
| LB / RB | Tab / F | SAT phone / alternate fire, look back |
| LT / RT | Caps Lock / Left click | Lock-on / fire |
| L3 / R3 | Left Ctrl / C | Crouch / recentre or change camera |
| Back / Start | Backspace / Esc | Back / pause |

Click a pad control in the binding screen and press a key or mouse button to rebind it; Esc cancels, and each row has a reset. Bindings are saved in `settings.toml` under `[keybindings]`. Existing keyboard settings files keep keyboard enabled and their old sensitivity values.

The GTA-style gamepad preset and PC key prompts are **experimental** and off by default; see [Features](#features). See [docs/controls.md](docs/controls.md) for more detail.

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
- Gamepad first: Xbox, PlayStation and Switch controllers through SDL3, with hot plugging and rumble.
- Keyboard and mouse are opt-in. The Controls tab maps each key or mouse button to a fixed Xbox pad control, with a reset for each row and a full reset; mouse look drives the right stick. This avoids the ambiguity of reinterpreting a key for each game context.
- **Experimental:** HUD key prompts (off by default) replace pad glyphs with the key bound to that pad control. They still depend on the game's context-sensitive prompt text and can occasionally be misleading; touching a controller restores the original glyphs. The music library snapshots the active input when its page opens.
- **Experimental:** the GTA-style gamepad preset (RT accelerate, LT brake) is off by default and remaps only a physical pad while driving. Original Xbox controls remain the default.

### Launcher, options and saves
- Launcher with a disc image picker, an extracted-folder picker, display/input options, the keyboard-to-Xbox binding screen, the intro-skip toggle and an **Unlock all cheats** option that makes every pause-menu cheat code selectable from a new game (effects are still chosen manually). Gamepad **A** activates the **Play (A)** button just like the ready screen's Start (A) button.
- In-game options overlay (**F1**) covering display, the same controls and binding screen, advanced rendering and game settings.
- Save manager that keeps the game's Xbox-format saves in `hdd\E\UDATA\5655003F` beside the executable. One four-row table shows all in-game profiles: empty rows offer **Import From File** and **Import From Folder**, while occupied rows offer open and delete. The source type is auto-detected: original Xbox folders, raw PC/PS2 saves, CodeBreaker `.cbs`, Max Drive `.max`, SharkPort/X-Port `.sps`/`.xps`, EMS `.psu`, and virtual-memory-card `.zip` archives. Xbox saves are rebound and re-signed to the chosen profile; PC or PS2 saves are converted into it without needing an existing Xbox profile. All four rows use the game's own save slots, including the fourth id confirmed from a game-created save. There is no first-free-profile fallback. Save-manager commands are disabled in the in-game overlay; the pre-game launcher can manage saves.
- Screenshots with **F2**, saved as BMP files in `screenshots` beside the executable.
- Optional intro skip (off by default), crash dumps and log files.
- Frame and state captures with **F2**/**F3** and the debug variables in [docs/debugging.md](docs/debugging.md).
- History of the fixed white UI panel bug in [docs/white-ui-panels.md](docs/white-ui-panels.md).
- Technical notes on HUD glyphs and key prompts in [docs/hud-glyphs-hotkeys.md](docs/hud-glyphs-hotkeys.md).
- Project notes on the first-mission lighting investigation and the DLSS decision in [docs/PROJECT-NOTES.md](docs/PROJECT-NOTES.md).

## Known issues

- Texture pop-in during play.
- Experimental PC key prompts follow the game's context-sensitive prompt text, so a prompt can still name the wrong action; controller glyphs remain the default.
- Save import and conversion are new; back up unusual save folders before importing or converting.
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

`release.bat` builds the same executable and packages a release archive (executable, empty `assets` folder, compiler, runtime DLLs, this README, `recomplauncher.json` and the controls page) using Windows' built-in `tar`.

The XBE used for the build must be the same release as the one in the image you run: the executable checks its SHA-1.

## AI assistance

This project was developed with AI assistance using OpenCode and the Big Pickle agent. The AI helped with reverse-engineering analysis, native runtime, renderer and input work, documentation, build and release preparation, and testing support. The maintainer directed the work, made the design decisions, integrated the changes and playtested the result. No AI-generated art, music, sound effects or placeholder game assets are included; the game uses the original Xbox assets supplied by the player.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) — recompiler, runtime and build system.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis), [toml++](https://github.com/marzer/tomlplusplus), [Dear ImGui](https://github.com/ocornut/imgui) and [SDL](https://github.com/libsdl-org/SDL).

*Scarface: The World Is Yours* is a trademark of its respective owners. This is a non-commercial fan project and is not affiliated with or endorsed by them.
