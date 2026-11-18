# Backlog

Ordered by priority. Only the top item is being worked on.

## 1. Antialiasing (active)

Add a user-selectable antialiasing option to the Direct3D 12 renderer (a post-process pass such as FXAA
or SMAA, or MSAA where supported), exposed in the launcher, the F1 overlay and `settings.toml`. Keep it
off or balanced by default and measure the cost on the integrated GPU.

## 2. HUD key prompts

Show PC key equivalents (e.g. `[W] Accelerate`) when keyboard/mouse is the active device instead of
gamepad glyphs. The Xbox build only ships gamepad artwork, so this needs glyph substitution or a
separate HUD overlay. `kbm::LastDevice()` already reports the active device, so the input side is ready.

## 3. Optional modern gamepad controls

Offer an optional "modern controls" preset for gamepads that follows a GTA-style layout instead of the
original Xbox bindings: RT accelerate, LT brake/reverse, left stick steer, and the usual twin-stick
camera and aiming conventions. Implement as a settings option, off by default, that remaps the pad
before the game reads it. The original scheme stays available and keyboard/mouse is unaffected.

## 4. DLSS 5 (low priority)

Investigate NVIDIA DLSS 5 upscaling for the Direct3D 12 renderer as an optional quality/performance
setting on supported GPUs, with a clean fallback to the current render path when it is unavailable.
Fully optional: the base renderer must keep working without an NVIDIA card or extra redistributables.

## 5. Experimental unlocked / 60 fps mode (research only)

The game presents at about 30 fps even with the host cap disabled, so the guest's own frame timing is the limit.
Investigate whether that limiter can be lifted for an opt-in experimental mode. First determine whether the
simulation is time-based or frame-count-based: if it is frame-count-based, physics and timers would speed up and
the idea should be dropped. Out of scope for now, well below DLSS 5.

## Later / release prep

- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Automated menu-input test recipe.
- Texture popping during play is reported but explicitly not a priority.
