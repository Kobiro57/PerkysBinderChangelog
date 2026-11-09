# Prüfumfang r19 – 9. November 2026

r19 ergänzt das eigene Drogen-Cooldown-/Nachwirkungs-Overlay. Alle Zeitangaben
der sieben Drogen werden in Sekunden geprüft. Die Tests behandeln den dynamischen
lokalen Namen, Ablehnung fremder oder zitierter Zeilen, Ecstasy/Ectasy, gleichzeitige
Timer, Grenzzeitpunkte, doppelte Zeilen und einmalige lokale Bereit-Meldungen.

Die Settings-Prüfungen behandeln Default-Aus, JSON-Roundtrip und ungültige
Koordinaten. Der vorhandene ImGui-Test prüft das neue mehrzeilige Panel,
gemeinsamen Stil, unabhängiges Verschieben und Speichern über den vorhandenen
Move-Modus. Vorschau ohne aktive Einnahme und spätere Unsichtbarkeit werden
gesondert berücksichtigt.

Der [maschinelle Nachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-validation.json) entsteht erst aus erfolgreichen
Gesamttests und Release-Build. Der PE-Nachweis prüft x86-DLL, eingebettete Helfer
300/301 auf Bytegleichheit mit den gebauten EXE-Dateien, eingebettete ZIP-Lizenz
302 sowie fehlende externe C++-Debug-Runtime und kein libcef im Spielprozess.
Der native Reconnect-Worker wird nach dem finalen Build separat geprüft.

Browser-, GPS-, Installer- und Reconnect-Helferquellen werden vor dem Paketbau
gegen den veröffentlichten r18-Stand b3ca24e43a8603d98215ca6b559adad16f3d41fe
auf unveränderten Inhalt geprüft. Der Browserstand bleibt cef156-r1. Die früheren
[Browser-/GPS-Nachweise](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r18-validation.json) und der genau abgegrenzte
[vorläufige Live-Test](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r18-live-validation.json) bleiben historisch erhalten.
Diese aufwendigen Download-/Browser-Integrationstests werden für r19 nicht als
erneut ausgeführt dargestellt.

Kein r19-Sichttest im laufenden Spiel wird vor einer tatsächlichen Bestätigung
behauptet. Keine persönlichen Profile, Einstellungen, Chatlogs oder Zugangsdaten
gehören zur Veröffentlichung.

[Bedienung und Zeiten](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/NLBinder-0.1.1-r19-DE.md) ·
[Vollständiger Changelog](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/CHANGELOG-DE.md)
