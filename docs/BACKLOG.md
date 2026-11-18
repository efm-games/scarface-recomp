# Backlog

Ordered by priority. Only the top item is being worked on.

## 1. HUD key prompts

Show PC key equivalents (e.g. `[W] Accelerate`) when keyboard/mouse is the active device instead of
gamepad glyphs. The Xbox build only ships gamepad artwork, so this needs glyph substitution or a
separate HUD overlay. `kbm::LastDevice()` already reports the active device, so the input side is ready.

## 2. Optional modern gamepad controls

Offer an optional "modern controls" preset for gamepads that follows a GTA-style layout instead of the
original Xbox bindings: RT accelerate, LT brake/reverse, left stick steer, and the usual twin-stick
camera and aiming conventions. Implement as a settings option, off by default, that remaps the pad
before the game reads it. The original scheme stays available and keyboard/mouse is unaffected.

## 3. Antialiasing

Add a user-selectable antialiasing option to the Direct3D 12 renderer (a post-process pass such as FXAA
or SMAA, or MSAA where supported), exposed in the launcher, the F1 overlay and `settings.toml`. Keep it
off or balanced by default and measure the cost on the integrated GPU.

## 4. DLSS 5

Investigate NVIDIA DLSS 5 upscaling for the Direct3D 12 renderer as an optional quality/performance
setting on supported GPUs, with a clean fallback to the current render path when it is unavailable.
Fully optional: the base renderer must keep working without an NVIDIA card or extra redistributables.

## Later / release prep

- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Automated menu-input test recipe.
- Texture popping during play is reported but explicitly not a priority.
