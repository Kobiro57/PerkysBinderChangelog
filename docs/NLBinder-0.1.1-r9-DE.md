# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 9

Stand: 27. Oktober 2026. Unter **Mods → Interface** bleiben vier getrennt
einstellbare Bereiche: **Geldanzeige, Waffenicon, Munition und Minimap**.
Die Maussteuerung beim Verschieben verwendet die gemeinsame Eingabebehandlung
der dauerhaften Online-Overlays. Esc beendet den Modus und speichert.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI am bisherigen Installationsort durch `Keybinder.asi`
   ersetzen. Nur eine Binder-Fassung laden; der vorhandene ASI-Loader bleibt
   erforderlich.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.

Eigene Einstellungen und Waffenbilder bleiben im bisherigen NLBinder-Ordner
unter **Dokumente → GTA San Andreas User Files → NLBinder**. Die mitgelieferte
leere Waffenicon-Ordnerstruktur kann dort zusammengeführt werden. Der
Interface-Editor benötigt keine weitere INI-Datei.

## Interface einrichten

Jeder der vier Bereiche bietet eine eigene Positions- und Farbauswahl.
**Mit der Maus verschieben** startet den Verschiebemodus für das gewählte
Element. Das Binder-Menü wird ausgeblendet, der Mauszeiger bleibt frei
beweglich und ein Rahmen markiert den Griff zum Ziehen. **Esc speichert**
den aktiven Preset-Stand und beendet das Verschieben. Position und Farbe
lassen sich einzeln zurücksetzen; die bisherigen fünf Preset-Slots bleiben
vorhanden.

Gebietsname, Fahrzeugname, Wanted-Sterne, Uhrzeit, Atemluft, Rüstung und
Leben haben keine Interface-Einstellungen mehr. Ihre früheren Interface-
Anpassungen werden nicht weiter angewendet. Die normalen Spielanzeigen
bleiben sichtbar und werden vom Spiel beziehungsweise Server gestaltet.

Die Anpassung betrifft das lokale native GTA-HUD. Eigene Server-Textdraws,
Datum und sonstige Serveranzeigen gehören nicht dazu. Die Minimap-Farbe
ist eine Tönung, die Kartendetails erhält. Die vorhandene Waffenicon-Auswahl
arbeitet weiterhin mit den Interface-Einstellungen zusammen.

## Prüfung und frühere Funktionen

Die Paketnachweise stehen in `BUILD-INFO.txt`, `SHA256SUMS.txt` und
`PRUEFBERICHT.json`. Der Reconnect-Helfer ist in der ASI enthalten und wird
mit harmlosen Testprozessen geprüft. Ein echter Ingame-Test auf dem
konkreten Server steht aus.

Jobs, Skilltrees, Fraktionen, getrennte Freunde-/Feinde-Overlays,
Waffenicons, `/speed ID`, Statistik-/Jobskill-Synchronisation und der Schutz
vor einem zweiten Motorstart nach einer Server-Addon-Meldung bleiben
enthalten. **Waffenicons-Ordner öffnen** verwendet den tatsächlichen
Windows-Dokumente-Pfad auch bei OneDrive-Umleitung.

Quellcode, Änderungsübersicht und Prüfstand werden im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
bereitgestellt. Frühere Stände und ursprüngliche Urheberhinweise bleiben
erhalten. Persönliche Screenshots, Laufzeitkonfigurationen, Accountwerte und
eigene PNGs gehören nicht zur Veröffentlichung.
