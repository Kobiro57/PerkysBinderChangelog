# PerkysBinderChangelog

Vollständiger Prüfstand der Änderungen am **Perkys / NLBinder**, angepasst für
KOBiRO. Grundlage und ursprüngliche Urheberschaft bleiben beim Originalprojekt.
Der Stand lautet **NLBinder v.0.1.1 by KOBiRO, r6**. Die Statistik erkennt
jetzt den tatsächlichen Titel im Format `Statistiken » Spielername`; das
Trennzeichen führte zuvor zur falschen Ablehnung der eigenen Statistik.
Ein anonymisierter Test prüft den vollständigen komprimierten Dialogpfad bis
zum Import, Dialogabschluss und Testkill. Der einmalige Motorstart aus r5 und
alle früheren Binder-Anpassungen bleiben enthalten.

## Einstieg für die Projektleitung

1. [Alle Änderungen und Korrekturen](../../docs/review/CHANGELOG-DE.md)
2. [Technisches Verhalten, Prüfungen und Grenzen](../../docs/review/PRUEFUNG-DE.md)
3. [Dateien im Vergleich zum Original](../../docs/review/DATEIVERGLEICH.md)
4. [Herkunft und Abhängigkeiten](../../docs/review/DEPENDENCIES.md)
5. [Programmänderungen als GitHub-Vergleich](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r6)
   oder als [kompakter Quellcode-Patch](../../docs/review/first-party-changes.patch)
6. [Änderungen seit r5](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r5...v0.1.1-r6)

Der vollständige bearbeitete Code liegt in `src/`, `include/`, `res/`,
`tests/`, `tools/` und `third_party/`. Die Übersicht verlinkt direkt zu den
betroffenen Implementierungen und ihren Tests.

## Nachvollziehbarer Vergleich

Die Import-Historie unterscheidet das unveränderte Originalarchiv von diesem
bearbeiteten Stand. Die Kennung im Original-ZIP lautet
`92ce21971bee2d166d830e23ad20950f1dc98921`.

Die Veröffentlichungscommits wurden nachträglich erstellt. Sie behaupten keine
ursprüngliche Entwicklungshistorie. Die Grundlage lässt sich über den Tag
`original-archive-92ce219` und den Datei-/Hashvergleich nachvollziehen. Alle
wiederhergestellten Abhängigkeiten sind ebenfalls enthalten und dokumentiert.
Sie stehen in einem eigenen Commit und im Tag `build-dependencies-baseline`,
damit der Programmvergleich nicht von den importierten SDK-Dateien überlagert wird.

Die [ursprüngliche Projektbeschreibung](../../docs/ORIGINAL-README.md) bleibt erhalten;
die [aktuelle Build-Anleitung](../../docs/BUILD-README.md) beschreibt das Bauen des Binders.

## Geprüfte ASI

- [Keybinder.asi herunterladen](../../dist/NLBinder-0.1.1-r6/Keybinder.asi)
- [Komplettes ZIP-Paket herunterladen](../../dist/NLBinder-v0.1.1-KOBiRO-r6.zip)
- [Installation und Bedienung](../../dist/NLBinder-0.1.1-r6/LIESMICH.md)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r6/BUILD-INFO.txt)
- [Maschinenlesbarer Prüfnachweis](../../dist/NLBinder-0.1.1-r6/PRUEFBERICHT.json)
- [SHA-256 der ausgelieferten ASI](../../dist/NLBinder-0.1.1-r6/SHA256SUMS.txt)

Die Nachweise gehören zum jeweiligen Binärstand: automatisierte Gesamttests,
Win32-Release-Build, PE-/Ressourcenvergleich und Reconnect-Helfertest mit
harmlosen Testprozessen. Ein echter Ingame-Test mit dem konkreten Server und
Launcher steht aus. Die [vorherige r5-Fassung](../../dist/NLBinder-0.1.1-r5/BUILD-INFO.txt)
und ihre Prüfsumme bleiben zur Nachverfolgung erhalten.

## Weitere Updates

Weitere Änderungen werden mit Beschreibung, zugehörigem Code, passenden
Prüfungen und eindeutiger ASI-Prüfsumme als neue Commits hier veröffentlicht.
Damit bleiben ältere Stände und Vergleiche erreichbar. Die Veröffentlichung
enthält keine persönlichen Binder-Einstellungen, UCP-Zugangsdaten oder Chatlogs.
