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

## Later

- **PC/PS2 save conversion** is in the save manager. It maps named variables from a PC or PS2 save
  into a free or chosen Xbox profile folder (`save.dat`, `SaveImage.xbx`, `SaveMeta.xbx`), using an
  existing Xbox profile as the template and a neutral placeholder image. The in-app path was rebuilt and
  smoke-checked: a 100% PC save produced `E96EA342EF133018E2CC57D35CA0F3057253DF6F0F897E8045463D081341FE75`
  and a PS2 save into profile 3 produced `9B91DAE910A251B074A736889D4EA8DBCD364AAB8327F9F5E5B87CBB407A5F60`;
  both were accepted by the game through `Load Successful!`.
- Test the save importer with real Xbox-format saves (`hdd\E\UDATA\5655003F`,
  `hdd\E\TDATA\5655003F`).
- Texture/lighting flash in the first mission at Tony's mansion is reported but explicitly not a priority. The user's
  `20261118_232001_827.bmp` is the bright frame and `20261118_232002_978.bmp` is the same view looking normal a second
  later. Their histories show the bright frame drawing the world with texture factors that sweep through `B6...` to
  `CE...` while the normal frame holds `FE000000`; moving Tony can bring the steady value back. This looks like a
  scene-tint state leaking into world draws, not a missing texture.

## Declined: DLSS 5

DLSS 5 is not planned for this project. The official SDK is not publicly available, and a proper
integration would require renderer changes this project does not intend to make, including engine
motion vectors, jitter, exposure/HDR data, semantic and engine masks, NVIDIA runtime packaging, and
hardware gating. External user-installed overlay solutions are outside this project: they are not
bundled, supported, or required, and the base renderer keeps working without an NVIDIA card or extra
redistributables.
