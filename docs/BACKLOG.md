# Backlog

Ordered by priority. Only the top item is being worked on.

## 1. Intro skip (done)

Skipping the logos now advances the playlist straight to its completion phase, so the boot state
machine resumes in under a second instead of waiting for all five movies.

- The startup state machine (`sub_002317D0`) sets up a five-movie publisher playlist through
  `sub_00028940` and is only resumed after the playlist finishes.
- `src\game\hooks.cpp` forces the manager to phase 5 on its first update; the original phase 5 block
  performs the `[0x4879E8]+0x48` handshake and resets the manager.
- User default remains `skip_intro = false`; the development `settings.toml` keeps it enabled.

## 2. Control schemes and plug-and-play gamepad (done)

Make the game fully playable from the keyboard alone and from any connected pad without touching
the HUD.

- Official PC manual scheme implemented in `src\common\kbm.cpp` and `src\common\game_info.cpp`.
- Dynamic on-foot vs vehicle detection (`InVehicle()`) reading the guest ActionMap stack at `0x471468`
  (`WheeledVehicleActionMap`, `BoatActionMap`, `HelicopterActionMap`, `AirplaneActionMap`).
- In vehicle: W/P gas (Xbox A), S/;/R brake/reverse (Xbox X), A/D steer, Space handbrake (Xbox B).
- On foot: Space/LMB fire (RT), Shift sprint (A), Ctrl crouch (L3), Enter wall cover, Caps/RMB lock-on (LT),
  F/MMB alt fire (Black), 1/2 weapon select, 3 holster, C/Home recenter (R3), T/X provoke (B).
- Menus: WASD/arrows navigation, Enter select (A), Esc cancel/back (B/Start).
- Connected pad seamlessly augments keyboard/mouse; input switches on the fly.
- Verified build and relinked `Scarface.dll`.

## 3. Pause menu white background (active)

The pause menu sometimes renders a white backdrop instead of the red one, making the text hard to
read. Fix after controls.

## 4. HUD key prompts

Show PC key equivalents (e.g. `[W] Accelerate`) when keyboard/mouse is the active device instead of
gamepad glyphs. The Xbox build only ships gamepad artwork, so this needs glyph substitution or a
separate HUD overlay. Start only after items 1-3 are done.

## Later / release prep

- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Public preview repo: install Git, initialize, update `README.md`, extend `.gitignore`
  (`/cap/`, `/dev/`, `/tools/`, `*.log`, `*.bmp`, `/XboxXBERecomp-main/`, `*.iso`), move stray root
  scripts/logs into `dev\legacy\`.
- Automated menu-input test recipe.
- Texture popping during play is reported but explicitly not a priority.
