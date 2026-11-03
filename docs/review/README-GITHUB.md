# PerkysBinderChangelog

Vollständiger Prüfstand von **NLBinder v.0.1.1 by KOBiRO, r16**.
Grundlage und ursprüngliche Urheberhinweise von Perkys bleiben erhalten.
Der aktuelle Quellstand basiert auf dem zuletzt bereitgestellten NLBinder-masterNEW.

r16 ergänzt die verschiebbare native **Ausdauer-Sidebar**, **Akkordverdienste**,
**20 gespeicherte GPS-Ziele**, **Spielernamen für `/speed`** und das echte
**Nova-Forum im Spiel** über Chromium/CEF. Die bisherigen Todes-/Damagelog-
Korrekturen und alle anderen Binder-Funktionen bleiben enthalten.

## Einstieg für die Projektleitung

1. [Gesamter Changelog](../../docs/review/CHANGELOG-DE.md), auch [als TXT](../../docs/CHANGELOG-GESAMT.txt)
2. [Verhalten, tatsächliche Prüfungen und Grenzen](../../docs/review/PRUEFUNG-DE.md)
3. [Dateivergleich zum unveränderten Original](../../docs/review/DATEIVERGLEICH.md)
4. [Abhängigkeiten und Herkunft](../../docs/review/DEPENDENCIES.md)
5. [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r16)
6. [Nur Änderungen von r15 nach r16](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r15...v0.1.1-r16)

Der vollständige bearbeitete Code liegt in `src/`, `include/`, `browser/`,
`tests/`, `res/`, `tools/` und `third_party/`. Die ursprüngliche Importhistorie,
der Tag `original-archive-92ce219`, der getrennte Abhängigkeitsimport und alle
früheren Veröffentlichungen bleiben nachvollziehbar erhalten. Die ursprüngliche
ZIP-Commitkennung lautet `92ce21971bee2d166d830e23ad20950f1dc98921`.

## Fertiges Paket

- [Vollständiges r16-Release mit ASI und Browser-Runtime](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r16)
- [NLBinder.asi](../../dist/NLBinder-0.1.1-r16/NLBinder.asi)
- [Installation und neue Befehle](../../dist/NLBinder-0.1.1-r16/LIESMICH.md)
- [Build-/Prüfangaben](../../dist/NLBinder-0.1.1-r16/BUILD-INFO.txt)
- [ASI-Prüfbericht](../../dist/NLBinder-0.1.1-r16/PRUEFBERICHT.json)
- [Browser-Dateien und Prüfsummen](../../dist/NLBinder-0.1.1-r16/BROWSER-RUNTIME.json)
- [Gesamtnachweis r16](../../docs/review/r16-validation.json)

Für `/forum` muss der Ordner **NLBinder/Browser** aus dem vollständigen ZIP
neben die ASI kopiert werden. Die große Runtime liegt als GitHub-Release-Anhang;
die ASI allein enthält Chromium nicht. Es werden keine persönlichen
Einstellungen, Chatlogs, UCP-Zugangsdaten oder Browserprofile veröffentlicht.

Die gesamte automatisierte Testsuite und der Release-Build sind erfolgreich.
Das echte Forum wurde anonym gerendert und gescrollt; die native x86-Anbindung
hat den x64-Browser mit Sandbox gestartet und sauber beendet. Diese neuen
Funktionen wurden noch nicht in einer laufenden GTA-Sitzung live getestet.
Die frühere r15-Todeskorrektur war bereits durch Explosion und Friedhofs-Relog
im Spiel bestätigt: [damaliger Live-Prüfumfang](../../docs/review/r15-live-validation.json).

[Ursprüngliche Projektbeschreibung](../../docs/ORIGINAL-README.md) ·
[Aktuelle Build-Anleitung](../../docs/BUILD-README.md)

Weitere Updates erscheinen als neue Commits mit Changelog, Quellcode,
passenden Prüfungen und eindeutiger ASI-Prüfsumme. Ältere Stände bleiben erhalten.
