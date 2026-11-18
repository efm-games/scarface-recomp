# Default keyboard and mouse controls

The recomp targets the Xbox version's game logic, so the keyboard/mouse layer synthesizes the
equivalent original-Xbox pad state on the fly. Defaults follow the official PC manual of
*Scarface: The World Is Yours* (page 3), with context-sensitive detection (on foot vs vehicle)
driven by reading the active ActionMap stack from guest memory (`0x471468`).

## On foot

| PC manual default | Action | Xbox pad equivalent |
| --- | --- | --- |
| W / A / S / D | Character movement | Left stick |
| Mouse move | Look / camera | Right stick |
| Space / Left click | Fire weapon; punch/melee when unarmed | RT |
| Caps Lock / Right click / Delete | Lock target / strafe | LT |
| F / Middle click | Alternative fire / shove / melee counter | Black |
| Shift (Left/Right) | Sprint | A |
| Left Ctrl | Crouch | Left thumb (L3) |
| Enter | Wall cover | A |
| E | Get into car / pick up weapon / interact | Y |
| R | Reload | X |
| T / X | Talk / taunt / provoke | B |
| V (hold) | Blind rage | B |
| 1 / 2 | Weapon select (prev / next) | D-pad left / right |
| 3 | Equip / conceal weapon | D-pad down |
| Mouse wheel | Cycle weapons | D-pad left / right |
| Tab | SAT phone menu | White |
| Esc | Pause | Start |
| C / Home | Centre camera | Right thumb (R3) |

## Vehicles

Context is automatically detected via `WheeledVehicleActionMap`, `BoatActionMap`, `HelicopterActionMap`, or `AirplaneActionMap`.

| PC manual default | Action | Xbox pad equivalent |
| --- | --- | --- |
| W / P | Accelerate / forward | A (analog) |
| S / ; / R | Brake / reverse | X (analog) |
| A / D (or L / ') | Steer | Left stick X |
| Space / T | Handbrake / boat tight turn | B (analog) |
| Mouse move | Look / camera | Right stick |
| Left click / Left Ctrl / F | Fire weapon from vehicle | RT |
| Caps Lock / Right click / Delete | Lock target | LT |
| Z / Middle click / Page Down | Look back | Black |
| C / Page Up / Home | Switch camera view | Right thumb (R3) |
| E | Enter / exit vehicle | Y |
| T / X | Horn | A |
| 1 / 2 / Mouse wheel | Cycle weapons | D-pad left / right |
| 3 | Equip / conceal weapon | D-pad down |
| Tab | SAT phone menu | White |
| Esc | Pause | Start |

## Menus

| PC manual default | Action | Xbox pad equivalent |
| --- | --- | --- |
| Arrow keys / W A S D | Move selection | D-pad |
| Enter | Select / Confirm | A |
| Esc | Back out / cancel / pause | B / Start |
| Mouse wheel | Change page / cycle | D-pad left / right |

## Any time

| Key | Action |
| --- | --- |
| F1 | Open or close the options overlay |
| F2 | Save the current frame as a BMP in the `screenshots` folder beside the executable |
| F3 | Capture the next frame in full under the `capture` folder, for debugging (see [debugging.md](debugging.md)) |

## Modern gamepad controls (optional)

Set `modern_pad = true` in `settings.toml`, or tick **Modern gamepad controls (GTA-style)** in the Controls tab
of the launcher or the F1 overlay. It is off by default and remaps only a physical pad; the keyboard and mouse
bindings above are unchanged.

| Pad input | Action while driving |
| --- | --- |
| RT | Accelerate |
| LT | Brake / reverse |
| A | Handbrake |
| B | Horn |
| RB | Fire from the vehicle |
| LB | Look back |
| L3 | Lock target |
| Back | SAT phone menu |
| X | Original brake, still available as a secondary pedal |
| Left stick | Steer |
| Right stick | Camera |
| Y / d-pad / R3 | Enter or exit / weapons / camera view as before |

On foot the original layout already follows the twin-stick convention (left stick move, right stick camera,
RT fire, LT lock), so it is left unchanged. The game has no separate horn button: the horn is the same pad
input as accelerate, so pressing B also gives a brief throttle input.

## Implementation Notes

- **Context-sensitive detection (`InVehicle()`):** The guest ActionMap stack is tracked at guest
  VA `0x471468`. When Tony gets into a car, boat, plane, or helicopter, the corresponding vehicle
  action map is pushed to the stack. `kbm::Pad()` checks this stack on each input poll and remaps
  `W`/`S` from stick movement to throttle (`A`) and brake/reverse (`X`), and `Space` from fire (`RT`)
  to handbrake (`B`).
- **Simultaneous gamepad + keyboard/mouse:** Real controller inputs and keyboard/mouse inputs augment
  each other without conflict. Digital buttons are OR'd, analog triggers take `max()`, and stick inputs
  are clamped additions. Active input device transitions on the fly without disabling either.
- **HUD key prompts:** With `hud_key_prompts = true`, pad glyph tokens in HUD text become bracketed key
  labels while keyboard/mouse is active (or before the first input). The substitution is
  context-sensitive, and the words on the prompt line settle shared meanings: Start, Load, Accept and
  Cover and dialog `[CLOSE]` are `[Enter]`, sprint is `[Shift]`, cancel/back/create are `[Esc]`,
  taunt is `[T]`, rage is `[V]` and pick up/interact is `[E]`. A bare A token otherwise means Shift on foot and W while
  driving. Plain triangle/Y captions get `[E] ` in front of their original wording; the tape-player glyph before
  `PLAY`/`STOP` is replaced with `[T] `, and the Shuffle caption becomes `[Enter] SHUFFLE ON/OFF`, matching the key
  that toggles it on that screen and dropping "TURN" so it does not overlap the adjacent tip. Gamepad prompts keep the
  original wording. Outside the music library, using a controller removes injected labels and restores the original
  glyphs. The music library snapshots the active input
  when its page opens and keeps that style until you leave, so switching devices on that screen does not mix labels
  and glyphs. See [hud-glyphs-hotkeys.md](hud-glyphs-hotkeys.md) for the technical details.
- **Mouse look:** Raw X and Y deltas have separate `kbm_sensitivity_x` and `kbm_sensitivity_y` sliders
  in the launcher/F1 options, and `invert_y` flips only the vertical sign. The old single
  `kbm_sensitivity` setting is migrated to both axes.
- **Overlay capture:** When the ImGui options overlay (F1) is open, keyboard and mouse inputs are
  neutralized to prevent unintended in-game actions.

Sources: Official *Scarface: The World Is Yours* PC Manual (page 3, "Default Controls").
