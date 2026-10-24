/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Keyboard layout aware key capture and display helpers
*/
#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

namespace keybinder {

inline bool IsMouseVirtualKeyCode(int vk) {
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
           vk == VK_XBUTTON1 || vk == VK_XBUTTON2;
}

inline bool IsExtendedVirtualKeyCode(int vk) {
    switch (vk) {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        case VK_RCONTROL: case VK_RMENU:
            return true;
        default:
            return false;
    }
}

inline bool IsCaptureVirtualKeyCandidate(int vk) {
    switch (vk) {
        case 0:
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_PACKET:
            return false;
        default:
            return true;
    }
}

inline std::string GetLocalizedVirtualKeyName(int vk) {
    if (vk == 0) return "Nicht gesetzt";

    if (vk == VK_LBUTTON) return "Linke Maustaste";
    if (vk == VK_RBUTTON) return "Rechte Maustaste";
    if (vk == VK_MBUTTON) return "Mittlere Maustaste";
    if (vk == VK_XBUTTON1) return "Maustaste 4";
    if (vk == VK_XBUTTON2) return "Maustaste 5";

    const HKL layout = GetKeyboardLayout(0);
    UINT scanCode = MapVirtualKeyExA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC, layout);
    if (scanCode == 0) {
        scanCode = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    }

    if (scanCode != 0) {
        LONG lParam = static_cast<LONG>((scanCode & 0xffu) << 16);
        if (IsExtendedVirtualKeyCode(vk)) {
            lParam |= static_cast<LONG>(1u << 24);
        }

        char name[128] = {0};
        const int result = GetKeyNameTextA(lParam, name, sizeof(name));
        if (result > 0) {
            return std::string(name);
        }
    }

    return "Taste " + std::to_string(vk);
}

inline void CollectPressedVirtualKeys(std::vector<int>& keys) {
    keys.clear();
    for (int vk = 1; vk <= 254; ++vk) {
        if (!IsCaptureVirtualKeyCandidate(vk)) {
            continue;
        }

        if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
            keys.push_back(vk);
        }
    }
}

inline int FindPressedVirtualKey(const std::vector<int>& ignoredKeys) {
    for (int vk = 1; vk <= 254; ++vk) {
        if (!IsCaptureVirtualKeyCandidate(vk)) {
            continue;
        }

        if ((GetAsyncKeyState(vk) & 0x8000) == 0) {
            continue;
        }

        if (std::find(ignoredKeys.begin(), ignoredKeys.end(), vk) != ignoredKeys.end()) {
            continue;
        }

        return vk;
    }

    return 0;
}

} // namespace keybinder
