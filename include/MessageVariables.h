#pragma once

namespace keybinder {

struct MessageVariableInfo {
    const char* tag;
    const char* description;
};

// Offer one spelling per value, using directly available local data. Legacy
// aliases may still be read by the runtime, but are not duplicated in the help.
inline constexpr MessageVariableInfo kCombatMessageVariables[] = {
    {"city", "Deine Stadt, z.B. Los Santos"},
    {"bezirk", "Dein Bezirk, z.B. Market"},
    {"hp", "Deine aktuellen Lebenspunkte"},
    {"weste", "Deine aktuelle Weste"},
    {"total_kills", "Gespeicherter Killstand des Binders"},
    {"total_deaths", "Gespeicherter Todesstand des Binders"},
    {"session_kills", "Erfasste Kills seit dem Spielstart"},
    {"session_deaths", "Erfasste Tode seit dem Spielstart"},
    {"kd", "K/D aus den gespeicherten Zählerständen"},
};

inline constexpr MessageVariableInfo kTextBindingVariables[] = {
    {"city", "Aktuelle Stadt, z.B. Los Santos"},
    {"bezirk", "Aktueller Bezirk, z.B. Market"},
    {"hp", "Aktuelle Lebenspunkte"},
    {"weste", "Aktuelle Weste (Rüstung)"},
    {"carhp", "Fahrzeug-Lebenspunkte (0 ohne Fahrzeug)"},
    {"carname", "Fahrzeugname oder Zu Fuß"},
    {"total_kills", "Gespeicherter Killstand des Binders"},
    {"total_deaths", "Gespeicherter Todesstand des Binders"},
    {"kd", "K/D aus den gespeicherten Zählerständen"},
    {"input", "Manuelle Textabfrage"},
};

} // namespace keybinder
