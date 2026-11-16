# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r24.

Die Tastaturweiterleitung des über `/forum` geöffneten Quick Forums ist für
Sonderzeichen überarbeitet, insbesondere deutsche AltGr-Zeichen wie `[ ]`.
Die Korrektur liegt in der ASI; die vorhandene Browser-Runtime cef156-r1 wird
weiterverwendet. Die r23-Funktionen mit aktuellen Spieler-IDs in der
Wanted-Tabelle, nahen sichtbaren Wanted-Labels und Ortungsmeldung bleiben
enthalten.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r24)
- [NLBinder.asi](dist/NLBinder-0.1.2-r24/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r24-DE.md)
- [Tastaturweiterleitung](include/ForumKeyboardInput.h)
- [Quick Forum](src/ForumBrowser.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext zum Posten](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien](docs/FORUM-DATEILISTE.txt)
- [Eigener Installer-/Browsercode](dist/NLBinder-0.1.2-r24/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r24-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r24-validation.json)
- [Nativer Browser-Eingabetest](dist/NLBinder-0.1.2-r24/FORUM-KEYBOARD-TEST.json)
- [Änderungen seit r23](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r23...v0.1.2-r24)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der neue Desktop-Ordner enthält vollständige Binder-Quellen, die fertige ASI
und Changelogs. Frühere Desktop-Ordner und die Git-Historie bleiben erhalten.
GTA vollständig schließen, ASI ersetzen und neu starten. Automatische Tests,
der native Browser-Eingabetest und tatsächliche
Ingame-Beobachtungen werden im Prüfnachweis getrennt ausgewiesen.
