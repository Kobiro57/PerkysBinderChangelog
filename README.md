# PerkysBinderChangelog

Vollständiger Prüfstand der Änderungen am **Perkys / NLBinder**, angepasst für
KOBiRO. Grundlage und ursprüngliche Urheberschaft bleiben beim Originalprojekt.
Der Stand lautet **NLBinder v.0.1.1 by KOBiRO, r13**.
Die eigene Friedhofsmeldung löst den Todeszähler aus. Native 0-HP-Zustände
zählen dafür nicht mehr. Die Datei heißt weiterhin **NLBinder.asi**;
der vollständige Änderungsverlauf liegt auch als TXT bei.

Der Damagelog nutzt zusätzlich echte eingehende Umwelt-/Explosionsereignisse
aus TakeDamage-RPC 115. Bei unmessbaren nativen HP-/Westenwerten können die
tatsächlich gesendeten Client-Syncwerte als Ersatz dienen, gebunden an Ped,
Modus, Fahrzeug und ein Alter von höchstens 500 ms. Die Ausgabe wird auf die
bekannten Restwerte begrenzt. Fehlende Restwerte führen zu höchstens 1500 ms
Wartezeit und keiner unbestätigten Überkillzahl. Ein Abgleich nach Quelle,
Zeit und beobachteten Frames verbindet die verschiedenen Meldungen eines
Treffers. Ein echter Ingame-Test auf dem konkreten Server steht aus.

Eigene Waffenbilder liegen unter **Dokumente → GTA San Andreas User Files →
NLBinder → Waffenicons**. Der Button **Waffenicons-Ordner öffnen** im Binder
öffnet den tatsächlich verwendeten Windows-Dokumente-Pfad auch bei OneDrive.

## Einstieg für die Projektleitung

1. [Alle Änderungen und Korrekturen](docs/review/CHANGELOG-DE.md)
2. [Technisches Verhalten, Prüfungen und Grenzen](docs/review/PRUEFUNG-DE.md)
3. [Dateien im Vergleich zum Original](docs/review/DATEIVERGLEICH.md)
4. [Herkunft und Abhängigkeiten](docs/review/DEPENDENCIES.md)
5. [Programmänderungen als GitHub-Vergleich](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r13)
   oder als [kompakter Quellcode-Patch](docs/review/first-party-changes.patch)
6. [Änderungen seit r12](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r12...v0.1.1-r13)

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

Die [ursprüngliche Projektbeschreibung](docs/ORIGINAL-README.md) bleibt erhalten;
die [aktuelle Build-Anleitung](docs/BUILD-README.md) beschreibt das Bauen des Binders.

## Geprüfte ASI

- [NLBinder.asi herunterladen](dist/NLBinder-0.1.1-r13/NLBinder.asi)
- [Gesamter Changelog als TXT](docs/CHANGELOG-GESAMT.txt)
- [Komplettes ZIP-Paket herunterladen](dist/NLBinder-v0.1.1-KOBiRO-r13.zip)
- [Installation und Bedienung](dist/NLBinder-0.1.1-r13/LIESMICH.md)
- [Build- und Prüfangaben](dist/NLBinder-0.1.1-r13/BUILD-INFO.txt)
- [Maschinenlesbarer Prüfnachweis](dist/NLBinder-0.1.1-r13/PRUEFBERICHT.json)
- [SHA-256 der ausgelieferten ASI](dist/NLBinder-0.1.1-r13/SHA256SUMS.txt)

Die Nachweise gehören zum jeweiligen Binärstand: automatisierte Gesamttests,
Win32-Release-Build, PE-/Ressourcenvergleich und Reconnect-Helfertest mit
harmlosen Testprozessen. Ein echter Ingame-Test mit dem konkreten Server und
Launcher steht aus. Die [vorherige r12-Fassung](dist/NLBinder-0.1.1-r12/BUILD-INFO.txt)
und ihre Prüfsumme bleiben zur Nachverfolgung erhalten.

## Weitere Updates

Weitere Änderungen werden mit Beschreibung, zugehörigem Code, passenden
Prüfungen und eindeutiger ASI-Prüfsumme als neue Commits hier veröffentlicht.
Damit bleiben ältere Stände und Vergleiche erreichbar. Die Veröffentlichung
enthält keine persönlichen Binder-Einstellungen, UCP-Zugangsdaten oder Chatlogs.
