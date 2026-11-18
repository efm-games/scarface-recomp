# Backlog

Ordered by priority. Only the top item is being worked on. `v1.0.0` is the current release; the items below are
post-release work.

## 1. HUD key prompts (active)

Show PC key equivalents (e.g. `[W] Accelerate`) when keyboard/mouse is the active device instead of
gamepad glyphs. The Xbox build only ships gamepad artwork, so this needs glyph substitution or a
separate HUD overlay. `kbm::LastDevice()` already reports the active device, so the input side is ready.

## 2. DLSS 5 (lowest priority)

Investigate NVIDIA DLSS 5 upscaling for the Direct3D 12 renderer as an optional quality/performance
setting on supported GPUs, with a clean fallback to the current render path when it is unavailable.
Fully optional: the base renderer must keep working without an NVIDIA card or extra redistributables.
Revisit only after the items below are done.

## Later

- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Texture popping during play is reported but explicitly not a priority.
