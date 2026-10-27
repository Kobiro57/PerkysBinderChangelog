# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 8

Stand: 27. Oktober 2026. Unter **Mods → Interface**, direkt neben
**Waffenicons**, werden Position und Farbe der einzelnen GTA-HUD-Elemente
getrennt eingerichtet. Die bisherigen Binder-Komponenten und Presets bleiben
Grundlage der Oberfläche.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI am bisherigen Installationsort durch `Keybinder.asi`
   ersetzen. Nur eine Binder-Fassung laden; der vorhandene ASI-Loader bleibt
   erforderlich.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.

Die mitgelieferte Waffenicon-Ordnerstruktur kann wie bisher mit
**Dokumente → GTA San Andreas User Files → NLBinder** zusammengeführt werden.
Eigene Bilder und Einstellungen beibehalten. Der Interface-Editor benötigt
keine zusätzliche INI-Datei.

Prüfsumme und Buildnachweise gehören zur gelieferten ASI und stehen in
`BUILD-INFO.txt`, `SHA256SUMS.txt` und `PRUEFBERICHT.json`. Ein echter
Ingame-Test auf dem konkreten Server steht aus.

## Einzelne HUD-Elemente bearbeiten

Unter **Mods → Interface** gibt es getrennte Einstellungen für:

- Leben und Rüstung
- Atemluft
- Geldanzeige und Uhrzeit
- Waffenicon und Munition
- Wanted-Sterne
- Fahrzeugname und Gebietsname
- Minimap

Jedes Element behält seine ursprüngliche Position und Farbe, solange keine
entsprechende Änderung eingestellt ist. Positionsänderungen werden relativ
zur Bildschirmgröße gespeichert. Eigene Farben werden für das ausgewählte
Element aktiviert; andere HUD-Elemente und der Chat verwenden weiterhin ihre
eigenen Farben.

**Verschieben** startet den Mausmodus für das gewählte Element. Das Binder-
Menü wird ausgeblendet; der angezeigte Rahmen dient als Griff zum Ziehen.
**Esc** beendet das Verschieben und speichert die Position. So muss das
Element nicht über X-/Y-Werte eingerichtet werden. Position und Farbe lassen
sich getrennt zurücksetzen. Die Preset-Slots speichern die Einstellungen.

Es werden die lokalen GTA-HUD-Elemente angepasst. Vom Server gesendete eigene
Textanzeigen, etwa dessen Datum, Ziele oder sonstige Textdraws, sind eigene
Serverelemente und gehören nicht zum nativen GTA-Interface. Die vom Benutzer
gezeigte InterfaceEditor-INI dient als Funktionsvorlage; sie wird nicht
installiert oder unverändert übernommen.

## Vorhandene Funktionen

Waffenicons, `/speed ID`, Statistik- und Jobskill-Synchronisation sowie der
Schutz vor einem zweiten Motorstart nach einer bestätigten Server-Addon-
Meldung bleiben enthalten. Eigene Waffenbilder liegen weiterhin unter
**Dokumente → GTA San Andreas User Files → NLBinder → Waffenicons**.
**Waffenicons-Ordner öffnen** verwendet den tatsächlichen Windows-Pfad,
einschließlich einer möglichen OneDrive-Umleitung.

## Quellen und Prüfung

Die Änderungen und die zum ausgelieferten Binärstand gehörenden Nachweise
werden im [GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
bereitgestellt. Frühere Stände bleiben erhalten. Persönliche Screenshots,
Accountwerte, Laufzeitkonfigurationen und eigene PNG-Sammlungen gehören nicht
zur Veröffentlichung.
