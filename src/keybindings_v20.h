#pragma once

// Remote physical inputs are Steam USB HID keyboard codes or 256 + mouse button bit.
constexpr int kBindingCount = 22;
constexpr LONG kMouseBindingBase = 256;
constexpr LONG kDefaultBindings[kBindingCount] = {
    26, 22, 4, 7,       // movement: W S A D
    20, 8, 44, 57,      // left grab, right grab, jump, sprint
    224, 226, 27, 10,   // crouch, thrust, swap, give up
    9, 21, 25, 29,      // slow motion, inventory, camera, camera lock
    41, 11,             // pause (Escape), undress
    257, 258, 272,      // left mouse/right hand, right mouse/left hand, middle mouse
    19                   // secondary pause key (P)
};
constexpr int kCanonicalBindings[kBindingCount] = {
    26, 22, 4, 7, 20, 8, 44, 57, 224, 226, 27, 10,
    9, 21, 25, 29, 41, 11, 257, 258, 272, 41
};
constexpr const wchar_t *kBindingActions[kBindingCount] = {
    L"Move forward", L"Move backward", L"Move left", L"Move right",
    L"Grab left", L"Grab right", L"Jump", L"Sprint",
    L"Crouch", L"Thrust / Classic Alt", L"Swap hands", L"Give up / taunt",
    L"Slow motion", L"Inventory", L"Change camera", L"Camera lock",
    L"Pause", L"Undress", L"Right-hand mouse", L"Left-hand mouse",
    L"Middle-mouse action", L"Pause (second key)"
};
struct BridgeSettings {
    LONG sensitivity_per_thousand;
    LONG alt_native_mouse;
    LONG recording_player;
    LONG bindings[kBindingCount];
};
