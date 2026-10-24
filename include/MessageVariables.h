#pragma once

namespace keybinder {

struct MessageVariableInfo {
    const char* tag;
    const char* description;
};

// These values are available directly from the current game or Binder state.
// Keep combat help separate from text bindings; vehicle fields are only useful
// in the latter. Legacy templates can still resolve the vehicle fields.
inline constexpr MessageVariableInfo kCombatMessageVariables[] = {
    {"victim", "Getöteter Spieler; bei unbekanntem Namen: Unbekannt"},
    {"killer", "Killername; im eigenen Killtext ist das dein Name"},
    {"weapon", "Waffe; ohne empfangene Waffenangabe: Unbekannt"},
    {"zone", "Deine aktuelle Zone"},
    {"ZONE", "Deine aktuelle Zone in Großbuchstaben"},
    {"hp", "Deine aktuellen Lebenspunkte"},
    {"weste", "Deine aktuelle Weste"},
    {"total_kills", "Gespeicherter Killstand des Binders"},
    {"total_deaths", "Gespeicherter Todesstand des Binders"},
    {"session_kills", "Erfasste Kills seit dem Spielstart"},
    {"session_deaths", "Erfasste Tode seit dem Spielstart"},
    {"kd", "K/D aus den gespeicherten Zählerständen"},
    {"KD", "Gleicher Wert wie {kd}"},
};

inline constexpr MessageVariableInfo kTextBindingVariables[] = {
    {"zone", "Aktueller Zonenname"},
    {"ZONE", "Aktueller Zonenname in Großbuchstaben"},
    {"hp", "Aktuelle Lebenspunkte"},
    {"weste", "Aktuelle Weste (Rüstung)"},
    {"carhp", "Fahrzeug-Lebenspunkte (0 ohne Fahrzeug)"},
    {"carname", "Fahrzeugname oder Zu Fuß"},
    {"total_kills", "Gespeicherter Killstand des Binders"},
    {"total_deaths", "Gespeicherter Todesstand des Binders"},
    {"kd", "K/D aus den gespeicherten Zählerständen"},
};

} // namespace keybinder
