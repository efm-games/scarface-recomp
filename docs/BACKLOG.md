# Backlog

Ordered by priority. Only the top item is being worked on. `v1.1.0` is the current release; the items below are
post-release work.

## Delivered in v1.1.0

- **HUD key prompts** are complete. The HUD token dispatcher substitutes pad button tokens with bracketed PC key
  labels while `kbm::LastDevice()` reports keyboard/mouse. Gamepad players keep the original glyphs, and the
  `hud_key_prompts` option can turn substitution off. Labels are context-sensitive: the same pad token maps to the
  menu, on-foot or driving key from the official PC manual. Line-level rules keyed on both the token and the words
  around it settle shared meanings, so Start/Load/Accept/Cover and dialog `[CLOSE]` are `[Enter]`, sprint is `[Shift]`,
  cancel/back/create are `[Esc]`, taunt is `[T]`, rage is `[V]` and pickup/interact is `[E]`. Plain triangle/Y captions
  receive `[E] ` before the unchanged wording; the tape player uses `[T] ` for Play/Stop and `[Enter] SHUFFLE ON/OFF`
  for Shuffle. The music library snapshots the active input when its page opens and keeps that style until you leave.
  See [hud-glyphs-hotkeys.md](hud-glyphs-hotkeys.md) for implementation details.
- **All cheats unlocked from a new game** is available through the `all_cheats_unlocked` launcher/F1 option. It forces
  the pause-menu reward unlock query to report every code as available, including in a new game. Codes are only made
  selectable; the player still chooses which effects to apply.
- **Esc pause** is fixed with a one-shot edge latch, so one physical press opens the pause menu without immediately
  closing it. Only the topmost ActionMap decides whether Esc means back or pause.
- **Launcher gamepad A** activates **Play (A)**, matching the ready screen's Start (A), and SDL controller input is now
  initialized in the launcher itself.
- **Mouse sensitivity** has separate horizontal and vertical sliders with migration of the old single setting.

## 1. DLSS 5 (lowest priority)

Investigate NVIDIA DLSS 5 upscaling for the Direct3D 12 renderer as an optional quality/performance
setting on supported GPUs, with a clean fallback to the current render path when it is unavailable.
Fully optional: the base renderer must keep working without an NVIDIA card or extra redistributables.

## Later

- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Texture popping during play is reported but explicitly not a priority.
