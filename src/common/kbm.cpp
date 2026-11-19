// Keyboard and mouse as an original Xbox pad. Defaults follow the official PC manual:
// On foot: WASD move, mouse look, Space/LMB fire, F/MMB alt fire, R reload, Shift sprint, Ctrl crouch,
// Enter wall cover, Caps/RMB lock-on, C/Home recenter, V rage, T taunt/provoke, E interact, 1/2 weapons, 3 holster,
// Tab SAT phone, Esc pause, mouse wheel cycle weapons. Arrow keys stand in for the pad's d-pad.
// Vehicles: W/P gas, S/;/R brake/reverse, A/D steer, Space handbrake, LMB/Ctrl/F fire, Caps/RMB lock,
// Z/MMB look back, C camera view, E enter/exit, T horn/taunt.
#include "common/kbm.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>

#include <imgui.h>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "game/hud_prompts.h"
#include "gpu/texturedumper.h"
#include "kernel/memory.h"

namespace kbm {

bool enabled = true;
bool invert_y = false;
uint32_t sensitivity_x = 100;  // percent of the base look speed
uint32_t sensitivity_y = 100;  // percent of the base look speed
bool modern_pad = false;
bool hud_prompts = true;

// Private diagnostic sessions only. This is deliberately not a settings option or a UI toggle: a normal player never
// sees it, and it cannot be persisted in settings.toml. The environment variable must be exactly "1".
bool DiagnosticCommandsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("XBE_DIAGNOSTIC_SESSION");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

namespace {

// The pad report gets read from guest threads; the window thread writes it.
std::mutex g_lock;
bool g_active = true;
bool g_keys[256] = {};
bool g_lmb = false, g_rmb = false, g_mmb = false;
int g_dx = 0, g_dy = 0;
int g_wheel = 0;
bool g_cursor_shown = true;
bool g_escape_latched = false;  // one virtual Start/B press per physical Esc press
void* g_hwnd = nullptr;  // the game window, for posting WM_SETCURSOR from the game thread
std::atomic<Device> g_device{Device::None};

// Diagnostic console: the window thread queues a command; a guest thread takes it and runs it through the game's own
// script evaluator. A version counter avoids holding a lock on the hot guest path when no command is pending.
std::atomic<bool> g_diagnostic_open{false};
std::atomic<bool> g_diagnostic_focus{false};  // set by F8 on the window thread, consumed by the render thread
std::mutex g_diagnostic_lock;
uint32_t g_diagnostic_version = 0;
std::atomic<uint32_t> g_diagnostic_queued{0};
std::string g_diagnostic_command;
char g_diagnostic_input[256] = "SGS_ActivateQueue(2,1);";

// Right stick units per mouse pixel at 100%. A flick across the screen is a full deflection.
constexpr int kBaseLook = 650;

// Xbox pad report bits
constexpr uint16_t kDpadUp = 1 << 0, kDpadDown = 1 << 1, kDpadLeft = 1 << 2, kDpadRight = 1 << 3;
constexpr uint16_t kStart = 1 << 4, kBack = 1 << 5, kLeftThumb = 1 << 6, kRightThumb = 1 << 7;
// analog[]: A B X Y black white left trigger right trigger
constexpr int kA = 0, kB = 1, kX = 2, kY = 3, kBlack = 4, kWhite = 5, kLT = 6, kRT = 7;

bool Held(int vk) { return g_keys[vk & 0xFF]; }

int16_t AddStick(int16_t a, int16_t b) {
    return int16_t(std::clamp(int(a) + int(b), -32768, 32767));
}

// Optional GTA-style driving on a physical pad: the triggers are the pedals, the bumpers take over the
// trigger actions, A is the handbrake, B is the horn, and L3 locks on. X stays a secondary brake so the
// classic scheme still works. Back opens the SAT phone. On foot the original layout already follows the
// same twin-stick conventions, so nothing changes there. Called before any keyboard/mouse is merged in.
void ModernDriving(input::Pad& pad) {
    uint8_t gas = pad.analog[kRT], brake = pad.analog[kLT];
    uint8_t fire = pad.analog[kBlack], look = pad.analog[kWhite];
    uint8_t handbrake = pad.analog[kA], horn = pad.analog[kB];
    // The game has no separate horn button: in the Xbox pad report the horn is the same A button as the
    // accelerator. Pressing B therefore also gives a short throttle input; it is unavoidable without
    // changing the guest.
    pad.analog[kA] = std::max(gas, horn);  // RT accelerates, B sounds the horn
    pad.analog[kX] = std::max(pad.analog[kX], brake);
    pad.analog[kB] = handbrake;                               // A handbrakes
    pad.analog[kRT] = fire;                                   // bumper fires
    pad.analog[kBlack] = look;                                // other bumper looks back
    pad.analog[kLT] = (pad.buttons & kLeftThumb) ? 0xFF : 0;  // L3 locks on
    pad.analog[kWhite] = (pad.buttons & kBack) ? 0xFF : 0;    // Back opens the SAT phone
}

// did a real controller do anything since the last poll? stick drift and trigger noise are ignored
bool PadActive(const input::Pad& pad) {
    if (pad.buttons) return true;
    for (uint8_t a : pad.analog)
        if (a > 8) return true;
    return std::abs(int(pad.lx)) > 4000 || std::abs(int(pad.ly)) > 4000 || std::abs(int(pad.rx)) > 4000 ||
           std::abs(int(pad.ry)) > 4000;
}

// one of this port's keys is held, or the mouse moved or was clicked
bool KbmActive() {
    static const int keys[] = {
        'W', 'A', 'S', 'D', 'R', 'T', 'V', 'E', 'F', 'C', 'Q', 'Z', 'P', 'L', 'X',
        '1', '2', '3', VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3,
        VK_RETURN, VK_SPACE, VK_TAB, VK_ESCAPE, VK_HOME, VK_PRIOR, VK_NEXT,
        VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_CAPITAL, VK_DELETE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_OEM_1, VK_OEM_7, VK_OEM_PLUS, VK_OEM_MINUS
    };
    for (int key : keys)
        if (Held(key)) return true;
    return g_lmb || g_rmb || g_mmb || g_dx || g_dy || g_wheel;
}

// What the game's ActionMap stack says about the screen it is on.
struct ActionMaps {
    bool vehicle = false;       // a driving map is on top: steering, not walking
    bool menu = false;          // a front end map is on top: Esc backs out of a menu there, it does not pause
    bool dialog = false;        // a system dialog is on top: Enter dismisses it, like the pad's B
    bool look_tutorial = false; // the tutorial overlay remaps Enter to the pad's Start and Esc to B
};

// Reads the game's ActionMap stack.
// The list head is stored at guest VA 0x471468.
// Each ActionMap node: +0x04 name pointer (const char*), +0x20 prev, +0x24 next, +0x2A active flag (uint8_t).
ActionMaps ReadActionMaps() {
    ActionMaps maps;
    uint8_t* base = mem::Base();
    if (!base) return maps;

    constexpr uint32_t kHeadVA = 0x471468;
    if (!mem::IsCommitted(kHeadVA)) return maps;

    uint32_t node_va = *reinterpret_cast<const uint32_t*>(base + kHeadVA);
    static char s_last_top[64] = {};

    char top_name[64] = {};

    for (int i = 0; i < 32 && node_va != 0; ++i) {
        if (!mem::IsCommitted(node_va) || !mem::IsCommitted(node_va + 0x34)) break;

        uint8_t active = *(base + node_va + 0x2A);
        uint32_t next_va = *reinterpret_cast<const uint32_t*>(base + node_va + 0x24);
        uint32_t name_va = *reinterpret_cast<const uint32_t*>(base + node_va + 0x04);

        if (active && name_va != 0 && mem::IsCommitted(name_va)) {
            char name[64] = {};
            for (size_t c = 0; c < sizeof(name) - 1; ++c) {
                uint32_t char_va = name_va + static_cast<uint32_t>(c);
                if ((char_va & 0xFFF) == 0 && !mem::IsCommitted(char_va)) break;
                char ch = static_cast<char>(base[char_va]);
                name[c] = ch;
                if (ch == '\0') break;
            }
            name[sizeof(name) - 1] = '\0';

            if (top_name[0] == '\0') {
                std::strncpy(top_name, name, sizeof(top_name) - 1);
            }

            if (std::strstr(name, "HUDLookTutorialActionMap")) maps.look_tutorial = true;

            if (std::strstr(name, "WheeledVehicleActionMap") ||
                std::strstr(name, "BoatActionMap") ||
                std::strstr(name, "HelicopterActionMap") ||
                std::strstr(name, "AirplaneActionMap")) {
                maps.vehicle = true;
                break;
            }
            if (std::strstr(name, "CharacterActionMap")) {
                maps.vehicle = false;
                break;
            }
            // Front-end screens push their own maps, but only the topmost map decides how Esc and Enter behave.
            // Do not set maps.menu here: PauseActionMap / UnpauseActionMap can remain below CharacterActionMap,
            // and treating that as a menu would send pad B (taunt) when Esc should send Start (pause).
        }
        node_va = next_va;
    }

    // Only the topmost map decides how Esc and Enter behave. PauseActionMap and its friends make Esc the pad's
    // back (B); in the world it must stay Start. SystemDialogActionMap is pushed for notices such as
    // "Load profile successful"; the game dismisses those with B (V on the PC bindings), not A, so Enter sends
    // B there too. Checking the top avoids leaking those bindings into play when such a map is still in the stack.
    maps.menu = std::strstr(top_name, "Menu") != nullptr || std::strstr(top_name, "FrontEnd") != nullptr ||
                std::strstr(top_name, "Options") != nullptr || std::strstr(top_name, "Pause") != nullptr ||
                std::strstr(top_name, "Dialog") != nullptr;
    maps.dialog = std::strstr(top_name, "Dialog") != nullptr;

    if (top_name[0] != '\0' && std::strncmp(s_last_top, top_name, sizeof(s_last_top)) != 0) {
        std::strncpy(s_last_top, top_name, sizeof(s_last_top) - 1);
        LOGI("kbm: top action map '%s' (in_vehicle=%d menu=%d dialog=%d look_tutorial=%d)", s_last_top, maps.vehicle,
             maps.menu, maps.dialog, maps.look_tutorial);
    }

    return maps;
}

// The game hides the cursor while it plays. WM_SETCURSOR applies the wanted state on the window thread, so this
// only remembers it and posts that message when it changes.
void WantCursor(bool show) {
    if (show == g_cursor_shown) return;
    g_cursor_shown = show;
    // WM_SETCURSOR only arrives on mouse activity: ask the window thread to apply the change now, so a pad only
    // player does not keep a stale arrow on screen after the overlay closes
    if (g_hwnd)
        PostMessageW(static_cast<HWND>(g_hwnd), WM_SETCURSOR, reinterpret_cast<WPARAM>(g_hwnd),
                     MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

void UpdateCursor() {
    // the game owns the cursor while focused; the options overlay takes it back
    bool overlay = app::OverlayOpen() || DiagnosticCommandsOpen();
    if (!overlay && ImGui::GetCurrentContext()) overlay = ImGui::GetIO().WantCaptureMouse;
    WantCursor(!g_active || overlay);
}

void ClearInput() {
    for (bool& k : g_keys) k = false;
    g_lmb = g_rmb = g_mmb = false;
    g_dx = g_dy = 0;
    g_wheel = 0;
}

}  // namespace

namespace {

void QueueDiagnosticCommand(const char* command) {
    if (!command || !*command) return;
    std::lock_guard lock(g_diagnostic_lock);
    g_diagnostic_command = command;
    if (g_diagnostic_command.size() >= 256) g_diagnostic_command.resize(255);
    ++g_diagnostic_version;
    if (!g_diagnostic_version) ++g_diagnostic_version;  // 0 means none queued
    g_diagnostic_queued.store(g_diagnostic_version, std::memory_order_release);
}

// A diagnostic harness can put the first command in this file instead of synthesizing text into the ImGui field. The
// console still has to be opened and triggered with F8/F9, so nothing runs without an explicit in-game action.
void LoadDiagnosticCommandFile() {
    std::ifstream file(app::ExeDir() / "diagnostic_command.txt");
    std::string line;
    if (!std::getline(file, line)) return;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.empty()) return;
    if (line.size() >= sizeof(g_diagnostic_input)) line.resize(sizeof(g_diagnostic_input) - 1);
    {
        std::lock_guard lock(g_diagnostic_lock);
        std::memcpy(g_diagnostic_input, line.c_str(), line.size() + 1);
    }
    LOGI("diagnostic: initial command from diagnostic_command.txt: %s", line.c_str());
}

void ToggleDiagnosticCommands() {
    bool open = g_diagnostic_open.load(std::memory_order_relaxed);
    g_diagnostic_open.store(!open, std::memory_order_relaxed);
    g_diagnostic_focus.store(!open, std::memory_order_relaxed);
}

}  // namespace

bool DiagnosticCommandsOpen() { return g_diagnostic_open.load(std::memory_order_relaxed); }

bool TakeDiagnosticCommand(char (&command)[256]) {
    uint32_t version = g_diagnostic_queued.load(std::memory_order_acquire);
    if (!version) return false;
    std::lock_guard lock(g_diagnostic_lock);
    if (version != g_diagnostic_queued.load(std::memory_order_relaxed)) {
        // A newer command was queued between the atomic read and the lock; take that one on the next expansion.
        return false;
    }
    if (g_diagnostic_command.empty() || g_diagnostic_command.size() >= sizeof(command)) {
        g_diagnostic_queued.store(0, std::memory_order_release);
        return false;
    }
    std::memcpy(command, g_diagnostic_command.c_str(), g_diagnostic_command.size() + 1);
    g_diagnostic_queued.store(0, std::memory_order_release);
    return true;
}

void Ui(double fps) {
    app::Overlay(fps);
    if (!DiagnosticCommandsEnabled()) {
        g_diagnostic_open.store(false, std::memory_order_relaxed);
        g_diagnostic_focus.store(false, std::memory_order_relaxed);
        return;
    }
    if (!g_diagnostic_open.load(std::memory_order_relaxed)) return;

    char input[sizeof(g_diagnostic_input)];
    {
        std::lock_guard lock(g_diagnostic_lock);
        std::memcpy(input, g_diagnostic_input, sizeof(input));
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 170.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Diagnostic script command")) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Runs on the next in-game text update. Guest commands can change or overwrite saves.");
    if (g_diagnostic_focus.exchange(false, std::memory_order_relaxed)) ImGui::SetKeyboardFocusHere();
    ImGui::InputText("Command", input, sizeof(input));
    bool execute = ImGui::Button("Execute");
    ImGui::SameLine();
    bool close = ImGui::Button("Close");
    {
        std::lock_guard lock(g_diagnostic_lock);
        std::memcpy(g_diagnostic_input, input, sizeof(input));
    }
    if (execute) QueueDiagnosticCommand(input);
    if (close) g_diagnostic_open.store(false, std::memory_order_relaxed);
    ImGui::End();
}

void Window(void* hwnd) {
    g_hwnd = hwnd;
    if (DiagnosticCommandsEnabled()) LoadDiagnosticCommandFile();
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;  // generic desktop
    rid.usUsage = 0x02;      // mouse
    rid.dwFlags = 0;         // only while the window is focused
    rid.hwndTarget = static_cast<HWND>(hwnd);
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    switch (msg) {
        case WM_INPUT: {
            RAWINPUT ri{};
            UINT size = sizeof(ri);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) !=
                    UINT(-1) &&
                ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                std::lock_guard lock(g_lock);
                g_dx += ri.data.mouse.lLastX;
                g_dy += ri.data.mouse.lLastY;
            }
            return false;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp < 256) {
                std::lock_guard lock(g_lock);
                g_keys[wp] = true;
            }
            // In a diagnostic session only, F8 opens the script-command console and F9 executes what is in it. The
            // runtime's F1/F2/F3 keys stay untouched. The auto-repeat guard keeps one command per press.
            if (DiagnosticCommandsEnabled() && !(lp & (intptr_t(1) << 30))) {
                if (wp == VK_F8) {
                    ToggleDiagnosticCommands();
                    return true;
                }
                if (wp == VK_F9 && g_diagnostic_open.load(std::memory_order_relaxed)) {
                    char command[sizeof(g_diagnostic_input)];
                    {
                        std::lock_guard lock(g_diagnostic_lock);
                        std::memcpy(command, g_diagnostic_input, sizeof(command));
                    }
                    QueueDiagnosticCommand(command);
                    return true;
                }
            }
            return false;  // the runtime's own keys (F3) still work
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wp < 256) {
                std::lock_guard lock(g_lock);
                g_keys[wp] = false;
            }
            return false;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            std::lock_guard lock(g_lock);
            if (msg == WM_LBUTTONDOWN) g_lmb = true;
            if (msg == WM_LBUTTONUP) g_lmb = false;
            if (msg == WM_RBUTTONDOWN) g_rmb = true;
            if (msg == WM_RBUTTONUP) g_rmb = false;
            if (msg == WM_MBUTTONDOWN) g_mmb = true;
            if (msg == WM_MBUTTONUP) g_mmb = false;
            return false;
        }
        case WM_MOUSEWHEEL: {
            std::lock_guard lock(g_lock);
            g_wheel += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            return false;
        }
        case WM_ACTIVATE:
            {
                std::lock_guard lock(g_lock);
                g_active = LOWORD(wp) != WA_INACTIVE;
                if (!g_active) ClearInput();
            }
            UpdateCursor();
            return false;
        case WM_SETCURSOR:
            UpdateCursor();
            // ImGui is told not to change the OS cursor, so this is now the only place that sets it: hidden
            // during play, plain arrow for the options overlay and whenever the window is not active
            ::SetCursor(g_cursor_shown ? ::LoadCursor(nullptr, IDC_ARROW) : nullptr);
            if (result) *result = TRUE;
            return true;
        default:
            return false;
    }
}

void Pad(int port, input::Pad& pad) {
    if (port != 0) return;
    // the overlay can open or close between two mouse messages: keep the cursor state in step every poll
    UpdateCursor();
    // the options overlay and diagnostic console have the keyboard and mouse while up: a held key must not reach the
    // game, and a mouse movement made in them must not turn into a look when they close
    if (app::OverlayOpen() || DiagnosticCommandsOpen()) {
        std::lock_guard lock(g_lock);
        ClearInput();
        pad.connected = true;
        pad.buttons = 0;
        for (uint8_t& a : pad.analog) a = 0;
        pad.lx = pad.ly = pad.rx = pad.ry = 0;
        g_escape_latched = false;
        return;
    }
    // Remap only the physical pad, before the keyboard/mouse early-out so pad-only players get it too,
    // and before any synthesized input is added. The overlay case above already zeroes everything.
    if (modern_pad && ReadActionMaps().vehicle) ModernDriving(pad);

    std::lock_guard lock(g_lock);
    if (!enabled || !g_active) {
        // do not keep a look delta: turning the keyboard back on must not snap the camera
        g_dx = g_dy = g_wheel = 0;
        g_escape_latched = false;
        return;
    }

    bool pad_active = PadActive(pad);
    bool kbm_active = KbmActive();
    ActionMaps maps = ReadActionMaps();
    bool in_vehicle = maps.vehicle;

    // movement and steering: WASD
    int16_t kx = 0, ky = 0;
    if (in_vehicle) {
        // Vehicles: A / D (also L / ' per PC manual) steer left/right
        float mx = (float(Held('D')) || float(Held(VK_OEM_7))) - (float(Held('A')) || float(Held('L')));
        if (mx != 0.0f) {
            kx = mx > 0.0f ? 32767 : -32768;
        }
        // Gas and brake are mapped to buttons (A/X), so stick Y stays 0
    } else {
        // On foot: WASD analog left stick, normalized so diagonals are not faster
        float mx = float(Held('D')) - float(Held('A'));
        float my = float(Held('W')) - float(Held('S'));
        if (float len = std::sqrt(mx * mx + my * my); len > 0) {
            kx = int16_t(std::lround(mx / len * 32767.0f));
            ky = int16_t(std::lround(my / len * 32767.0f));
        }
    }

    // look: raw mouse delta, consumed here so it is applied once
    int16_t krx = 0, kry = 0;
    int dx = g_dx, dy = g_dy;
    g_dx = g_dy = 0;
    if (dx || dy) {
        float sx = kBaseLook * (float(sensitivity_x) / 100.0f);
        float sy = kBaseLook * (float(sensitivity_y) / 100.0f);
        float rx = float(dx) * sx;
        float ry = invert_y ? float(dy) * sy : -float(dy) * sy;
        krx = int16_t(std::clamp<long>(std::lround(rx), -32768, 32767));
        kry = int16_t(std::clamp<long>(std::lround(ry), -32768, 32767));
    }

    uint16_t buttons = 0;
    uint8_t analog[8] = {};

    bool shift = Held(VK_SHIFT) || Held(VK_LSHIFT) || Held(VK_RSHIFT);
    bool ctrl = Held(VK_CONTROL) || Held(VK_LCONTROL) || Held(VK_RCONTROL);

    if (in_vehicle) {
        // --- Vehicles (Official PC manual page 3) ---
        // Forward / Gas: W or P
        if (Held('W') || Held('P')) analog[kA] = 0xFF;
        // Reverse / Brake: S or ; or R
        if (Held('S') || Held(VK_OEM_1) || Held('R')) analog[kX] = 0xFF;
        // Handbrake: Space (also T)
        if (Held(VK_SPACE) || Held('T')) analog[kB] = 0xFF;
        // Horn: T or X
        if (Held('T') || Held('X')) analog[kA] = 0xFF;
        // Enter / Exit vehicle: E
        if (Held('E')) analog[kY] = 0xFF;
        // Fire from vehicle: LMB or Left Ctrl or F
        if (g_lmb || ctrl || Held('F')) analog[kRT] = 0xFF;
        // Lock target: Caps Lock or RMB or Delete
        if (g_rmb || Held(VK_CAPITAL) || Held(VK_DELETE)) analog[kLT] = 0xFF;
        // Look Back: MMB or Z or Page Down
        if (g_mmb || Held('Z') || Held(VK_NEXT)) analog[kBlack] = 0xFF;
        // Switch Camera: C or Page Up or Home
        if (Held('C') || Held(VK_PRIOR) || Held(VK_HOME)) buttons |= kRightThumb;
    } else {
        // --- On Foot (Official PC manual page 3) ---
        // Fire: Space or LMB
        if (g_lmb || Held(VK_SPACE)) analog[kRT] = 0xFF;
        // Alt fire / melee shove: F or MMB
        if (g_mmb || Held('F')) analog[kBlack] = 0xFF;
        // Reload: R
        if (Held('R')) analog[kX] = 0xFF;
        // Sprint: Left or Right Shift
        if (shift) analog[kA] = 0xFF;
        // Wall cover: Enter
        if (Held(VK_RETURN)) analog[kA] = 0xFF;
        // Crouch: Left or Right Ctrl (L3 thumbstick, without A button combo)
        if (ctrl) buttons |= kLeftThumb;
        // Lock-on: Caps Lock or RMB or Delete
        if (g_rmb || Held(VK_CAPITAL) || Held(VK_DELETE)) analog[kLT] = 0xFF;
        // Recenter camera: C or Home
        if (Held('C') || Held(VK_HOME)) buttons |= kRightThumb;
        // Blind rage: hold V
        if (Held('V')) analog[kB] = 0xFF;
        // Provoke / Talk / Taunt: T or X
        if (Held('T') || Held('X')) analog[kB] = 0xFF;
        // Enter car / Pick up weapon / Interact: E
        if (Held('E')) analog[kY] = 0xFF;
    }

    // --- Weapons & Inventory ---
    // Weapon 1 (prev): 1
    if (Held('1') || Held(VK_NUMPAD1)) buttons |= kDpadLeft;
    // Weapon 2 (next): 2
    if (Held('2') || Held(VK_NUMPAD2)) buttons |= kDpadRight;
    // Holster: 3
    if (Held('3') || Held(VK_NUMPAD3)) buttons |= kDpadDown;

    // SAT phone: Tab
    if (Held(VK_TAB)) analog[kWhite] = 0xFF;

    // Pause / Esc: Start in the world, B in a front end screen (the pad's back). Emit only on the key's rising edge;
    // a held Esc would otherwise send Start, see the pause map on the next poll, then send B and close it again.
    bool escape = Held(VK_ESCAPE);
    if (escape && !g_escape_latched) {
        if (maps.look_tutorial || maps.menu) analog[kB] = 0xFF;
        else buttons |= kStart;
    }
    g_escape_latched = escape;
    // Enter: Start on the look tutorial, Select (A) in menus and wall cover on foot. In a vehicle A is the
    // accelerator and the PC manual gives Enter no driving meaning, so it must not fall through there. The boot's
    // "load or create" dialog needs this Start press to choose LOAD; notices are dismissed with Esc/B instead.
    if (Held(VK_RETURN)) {
        if (maps.look_tutorial) buttons |= kStart;
        else if (maps.dialog) analog[kB] = 0xFF;
        else if (maps.menu || !in_vehicle) analog[kA] = 0xFF;
    }

    // Menus: Arrow keys stand in for Xbox d-pad
    if (Held(VK_UP)) buttons |= kDpadUp;
    if (Held(VK_DOWN)) buttons |= kDpadDown;
    if (Held(VK_LEFT)) buttons |= kDpadLeft;
    if (Held(VK_RIGHT)) buttons |= kDpadRight;

    // Mouse wheel cycles weapons (d-pad left/right)
    if (g_wheel > 0) buttons |= kDpadRight;
    if (g_wheel < 0) buttons |= kDpadLeft;
    g_wheel = 0;

    // either device can hold a button, the larger analog value wins, and the sticks add
    pad.connected = true;
    pad.buttons |= buttons;
    for (int i = 0; i < 8; i++) pad.analog[i] = std::max(pad.analog[i], analog[i]);
    pad.lx = AddStick(pad.lx, kx);
    pad.ly = AddStick(pad.ly, ky);
    pad.rx = AddStick(pad.rx, krx);
    pad.ry = AddStick(pad.ry, kry);

    bool feature_enabled = enabled && hud_prompts;
    if (kbm_active || pad_active) {
        Device now = kbm_active ? Device::KeyboardMouse : Device::Pad;
        if (g_device.exchange(now, std::memory_order_relaxed) != now) {
            LOGI("input: %s is driving", now == Device::KeyboardMouse ? "keyboard and mouse" : "controller");
            // The music library keeps the prompts it had on entry; other screens rewrite cached captions on a
            // device switch so a keyboard label never survives when a pad takes over.
            if (!hud_prompts::Frozen()) ApplyHudDeviceText(feature_enabled && now != Device::Pad);
        }
    }

    // Hide the pad artwork only while bracketed key prompts are driving the HUD. The music library uses the same
    // frozen decision as its text, so switching devices on that page does not half-convert its prompts.
    gpu::texturedumper::SetControllerGlyphOverride(hud_prompts::UseKeyLabels());
}

Device LastDevice() { return g_device.load(std::memory_order_relaxed); }

PromptContext PromptContextNow() {
    ActionMaps maps = ReadActionMaps();
    if (maps.menu) return PromptContext::Menu;
    if (maps.vehicle) return PromptContext::Vehicle;
    return PromptContext::OnFoot;
}

bool LookTutorialActive() { return ReadActionMaps().look_tutorial; }

void Rumble(int, uint16_t& left, uint16_t& right) {
    // A keyboard/mouse player has no pad in their hands: drop the motors so a connected controller does not
    // buzz on the desk. Because the last device to send input decides, picking the pad up restores rumble
    // as soon as a button or stick is used. Zero stops any motors still spinning from an earlier request.
    if (g_device.load(std::memory_order_relaxed) == Device::KeyboardMouse) {
        left = right = 0;
    }
}

}  // namespace kbm
