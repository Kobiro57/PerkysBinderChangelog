# PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.1 by KOBiRO, r18**.
Ursprüngliche Urheberhinweise und alle bisherigen Veröffentlichungen bleiben erhalten.

r18 korrigiert die Browserinstallation bei 100 Prozent, sofortigen Abbruch und
erneuten Start sowie die Erzeugung des normalen roten GPS-Welt-Checkpoints.
Beim Öffnen des Forums wird zusätzlich einmal `/afk` und beim Schließen
einmal `/back` an den Server geschickt.
Die Abläufe hängen zusätzlich an den tatsächlichen Render-/HUD-Aufrufen.
Es muss weiterhin nur **NLBinder.asi** verteilt werden.

## Prüfung und Quellcode

- [Vollständiger Changelog](../../docs/review/CHANGELOG-DE.md), auch [als TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Tatsächlicher Prüfumfang und Grenzen](../../docs/review/PRUEFUNG-DE.md)
- [Maschinenlesbarer r18-Nachweis](../../docs/review/r18-validation.json)
- [Dateivergleich zum Original](../../docs/review/DATEIVERGLEICH.md)
- [Abhängigkeiten und Lizenzen](../../docs/review/DEPENDENCIES.md)
- [Änderungen seit r17](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r17...v0.1.1-r18)
- [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r18)

Vollständige Quellen unter `src/`, `include/`, `browser/`, `res/`, `tests/`,
`tools/` und `third_party/`. Originalimport, getrennte SDK-Basis und frühere
Revisionspatches bleiben unverändert nachvollziehbar.

## Fertiger Binder

- [r18-Paket herunterladen](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r18)
- [NLBinder.asi](../../dist/NLBinder-0.1.1-r18/NLBinder.asi)
- [Installation und Bedienung](../../dist/NLBinder-0.1.1-r18/LIESMICH.md)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r18/BUILD-INFO.txt)
- [ASI und eingebettete Helfer](../../dist/NLBinder-0.1.1-r18/PRUEFBERICHT.json)
- [Festes Browser-Downloadmanifest](../../res/browser-runtime.json)

GTA vollständig beenden, ASI ersetzen und neu starten. Der Browser wird beim
ersten `/forum` unter Dokumente automatisch eingerichtet. Ein bereits fertig
geladener Download wird nach einem Abbruch wiederverwendet. Ready öffnet das
Forum ohne Warten auf Installer-Aufräumen. Geprüfte unveränderte Dateien werden
schnell wiederverwendet; der anfängliche Download hängt von der Verbindung ab.

Automatisierte Gesamttests, echter HTTPS-Download, erzwungener Abbruch/Retry
und vier echte Forum-Öffnungen über reine Render-Aufrufe sind geprüft. Der
isolierte GPS-Adaptertest nutzt GTA-Strukturen mit ersetzten Engine-Aufrufen.
Ein vorläufiger Forum-/GPS-Stand wurde im Spiel bestätigt; der genaue Umfang
steht im [Live-Nachweis](../../docs/review/r18-live-validation.json). Die nachträgliche
AFK-Erweiterung ist mit sechs Befehls-Paaren automatisiert geprüft.

Keine persönlichen Einstellungen, Profile, Zugangsdaten oder Chatlogs enthalten.
[Ursprüngliche Beschreibung](../../docs/ORIGINAL-README.md) · [Build-Anleitung](../../docs/BUILD-README.md)
