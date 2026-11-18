#include "app/app.h"

#include "common/kbm.h"

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Scarface: The World Is Yours",  // window and splash
    .short_title = L"Scarface",                // message boxes
    .name = L"Scarface",                       // window classes, crash dumps, handover event
    .log = "scarface",                         // log file prefix
    .settings = "settings.toml",               // settings file beside the exe
    .splash = L"splash",                       // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "Scarface"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"kbm", &kbm::enabled, nullptr, "Keyboard and mouse"},
                {"kbm_invert_y", &kbm::invert_y, nullptr, "Invert mouse Y"},
                {"kbm_sensitivity", nullptr, &kbm::sensitivity, "Mouse sensitivity", 10, 300},
                {"modern_pad", &kbm::modern_pad, nullptr, "Modern gamepad controls (GTA-style)"}},
    // reference only: the bindings live in kbm.cpp, and these rows must stay in step with it
    .controls =
        {{"On foot",
          {{"W A S D", "Move"},
           {"Mouse", "Look / camera"},
           {"Space / Left click", "Fire; punch or melee when unarmed"},
           {"Caps / Right click / Del", "Lock target / strafe"},
           {"F / Middle click", "Alternative fire / shove"},
           {"Shift (hold)", "Sprint"},
           {"Left Ctrl", "Crouch"},
           {"Enter", "Wall cover"},
           {"E", "Get into a car / pick up a weapon"},
           {"R", "Reload"},
           {"T / X", "Talk / taunt / provoke"},
           {"V (hold)", "Blind rage"},
           {"1 / 2", "Select weapon (prev / next)"},
           {"3", "Equip / conceal weapon"},
           {"Tab", "SAT phone"},
           {"Esc", "Pause"},
           {"C / Home", "Centre the camera"}}},
         {"In a vehicle",
          {{"W / P", "Accelerate / forward"},
           {"S / ; / R", "Brake / reverse"},
           {"A / D", "Steer"},
           {"Space / T", "Handbrake / tight turn"},
           {"Mouse", "Look / camera"},
           {"Left click / Ctrl / F", "Fire"},
           {"Caps / Right click / Del", "Lock target"},
           {"Z / Middle click / PgDn", "Look back"},
           {"E", "Enter / exit the vehicle"},
           {"T / X", "Horn"},
           {"1 / 2 / Mouse wheel", "Cycle weapons"},
           {"C / PgUp / Home", "Change camera view"},
           {"Esc", "Pause"}}},
         {"Menus",
          {{"Arrow keys / WASD", "Move the selection"},
           {"Enter", "Select"},
           {"Esc", "Back out / pause"},
           {"Mouse wheel", "Change page"}}}},
    .load = nullptr,                          // reads more from the settings file's toml::table
    .save = nullptr,                          // writes more into it
    .init = nullptr,                          // after the settings load, before the window and boot
    .loaded = nullptr,                        // image in memory, before its entry point runs
    .window = kbm::Window,                    // the game window is up
    .message = kbm::Message,                  // window messages first; true takes one
    .ui = app::Overlay,                       // F1: the in-game options overlay
    .pad = kbm::Pad,                          // changes a polled pad
    .rumble = kbm::Rumble,                    // changes a rumble before it is set: quiet it for keyboard players
    .title_id = "5655003F",                   // E:\UDATA save folder
};
