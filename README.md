# PerkysBinderChangelog

Aktueller Prüfstand: **NLBinder v.0.1.1 by KOBiRO, r17**.
Grundlage und ursprüngliche Urheberhinweise von Perkys bleiben erhalten.
Alle bisherigen Änderungen und Veröffentlichungen sind in der Historie erhalten.

r17 installiert den Browser beim ersten **`/forum` automatisch unter Dokumente**.
Es muss nur noch **NLBinder.asi** verteilt werden. Der eigene native Installer
prüft das versionierte Downloadpaket und jede enthaltene Datei. Vorhandene
passende r16-Dateien werden ohne Download übernommen. Fortschritt, Abbruch,
Reparatur und erneuter Versuch sind vorhanden.

Das Forum kann mehrfach und auch direkt während des Schließens wieder geöffnet
werden. Die rote GPS-Markierung bleibt beim Start direkt am Ziel sichtbar;
entfernte eigene Radar-Einträge werden wiederhergestellt und fremde geschont.

## Prüfung durch die Projektleitung

- [Gesamter Changelog](docs/review/CHANGELOG-DE.md), auch [als TXT](docs/CHANGELOG-GESAMT.txt)
- [Verhalten und tatsächlicher Prüfumfang](docs/review/PRUEFUNG-DE.md)
- [Dateivergleich zum Original](docs/review/DATEIVERGLEICH.md)
- [Bibliotheken, Downloadherkunft und Lizenzen](docs/review/DEPENDENCIES.md)
- [Änderungen seit r16](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r16...v0.1.1-r17)
- [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r17)

Der vollständige Code steht unter `src/`, `include/`, `browser/`, `tests/`,
`tools/`, `res/` und `third_party/`. Original-Tag `original-archive-92ce219`,
getrennter SDK-Import und frühere Revisionspatches bleiben unverändert.

## Fertiger Binder

- [r17-Download](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r17)
- [NLBinder.asi](dist/NLBinder-0.1.1-r17/NLBinder.asi)
- [Installation und Bedienung](dist/NLBinder-0.1.1-r17/LIESMICH.md)
- [Build und Prüfung](dist/NLBinder-0.1.1-r17/BUILD-INFO.txt)
- [Prüfsumme / eingebettete Helfer](dist/NLBinder-0.1.1-r17/PRUEFBERICHT.json)
- [Downloadmanifest](res/browser-runtime.json)
- [Gesamtnachweis r17](docs/review/r17-validation.json)

Bei beendetem Spiel die bisherige Binder-ASI ersetzen. Ein manueller
Browserordner ist ab r17 nicht mehr nötig. Der erste Download umfasst etwa
188 MB; entpackt werden rund 429 MB. Originales Forum, Sandbox und getrenntes
Cookieprofil bleiben erhalten. Persönliche Einstellungen, Zugangsdaten,
Browserprofile und Chatlogs werden nicht veröffentlicht.

Gesamttests, Release-Build, echter Installer-Download und vier Öffnungen des
Forums mit demselben anonymen Testprofil sind erfolgreich geprüft. Ein
zusätzlicher Live-Test der neuen Fassung innerhalb von GTA steht noch aus.

[Ursprüngliche Beschreibung](docs/ORIGINAL-README.md) · [Build-Anleitung](docs/BUILD-README.md)
