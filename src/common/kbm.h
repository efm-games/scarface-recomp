// Keyboard and mouse as an original Xbox pad, merged with a real controller when one is connected. Defaults copy the
// PC version's bindings: WASD move/steer, mouse look, LMB fire, RMB lock/strafe, MMB alt fire/look back, Enter
// sprint/gas/horn, R reload/brake, T handbrake/taunt/tape, V blind rage, Ctrl crouch, E use, 3 equip, Tab SAT phone,
// Esc pause, Home camera, mouse wheel cycle weapons, arrow keys for the pad's d-pad.
#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace kbm {

// shown in the settings file by app::kGame.options
extern bool enabled;
extern bool invert_y;
extern uint32_t sensitivity;  // percent of the base look speed
extern bool modern_pad;       // GTA-style driving on a physical pad, off by default

// the device that last did something, for prompts that name a button
enum class Device : uint32_t { None, Pad, KeyboardMouse };
Device LastDevice();

void Window(void* hwnd);  // kGame.window: raw mouse input is registered on the game window
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void Pad(int port, input::Pad& pad);
void Rumble(int port, uint16_t& left, uint16_t& right);  // kGame.rumble: silence a pad the player is not using

}  // namespace kbm
