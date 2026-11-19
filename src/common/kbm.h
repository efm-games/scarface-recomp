// Keyboard and mouse as an original Xbox pad, merged with a real controller when one is connected. Defaults copy the
// PC version's bindings: WASD move/steer, mouse look, Space/LMB fire, F/MMB alt fire, R reload, Shift sprint,
// Ctrl crouch, Enter wall cover, Caps/RMB lock-on, V rage, T taunt/provoke, E interact, 1/2 weapons, 3 holster,
// Tab SAT phone, Esc pause, mouse wheel cycle weapons, arrow keys for the pad's d-pad.
#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace kbm {

// shown in the settings file by app::kGame.options
extern bool enabled;
extern bool invert_y;
extern uint32_t sensitivity_x;  // percent of the base look speed, horizontal
extern uint32_t sensitivity_y;  // percent of the base look speed, vertical
extern bool modern_pad;         // GTA-style driving on a physical pad, off by default
extern bool hud_prompts;        // show PC key labels in HUD prompts while keyboard/mouse drives the game

// Private diagnostic session hook. It is not a player option: only XBE_DIAGNOSTIC_SESSION=1 enables it, and it is
// never written to settings.toml or shown in the options overlay. Commands are queued on the window thread and
// executed by the guest hook on the next text expansion, never from the window thread itself.
bool DiagnosticCommandsEnabled();
void Ui(double fps);  // kGame.ui: options overlay; diagnostic console only in a diagnostic session
bool DiagnosticCommandsOpen();
bool TakeDiagnosticCommand(char (&command)[256]);

// the device that last did something, for prompts that name a button
enum class Device : uint32_t { None, Pad, KeyboardMouse };
Device LastDevice();

// Which pad action a HUD prompt is currently describing, so one pad token can map to the right PC key
// in each situation (X is Enter in a menu, Shift on foot, W while driving).
enum class PromptContext : uint32_t { OnFoot, Vehicle, Menu };
PromptContext PromptContextNow();

// The look tutorial is a separate ActionMap, not a PromptContext. Its Start/B glyphs have meanings that the generic
// context labels cannot express, so prompt expansion checks this when the source line is unavailable.
bool LookTutorialActive();

void Window(void* hwnd);  // kGame.window: raw mouse input is registered on the game window
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void Pad(int port, input::Pad& pad);
void Rumble(int port, uint16_t& left, uint16_t& right);  // kGame.rumble: silence a pad the player is not using

}  // namespace kbm
