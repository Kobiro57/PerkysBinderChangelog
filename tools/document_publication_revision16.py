from pathlib import Path
import json

R=Path(__file__).resolve().parents[1]
p=R/'docs/review/CHANGELOG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '`/speed` akzeptiert zusätzlich vollständige Spielernamen' not in t:
    t+='\n- `/speed` akzeptiert zusätzlich vollständige Spielernamen und eindeutige Namensteile ab drei Zeichen. Exakte Namen haben Vorrang; mehrdeutige Eingaben vergeben keine Wanteds.\n'
p.write_text(t,encoding='utf-8')
intro='''# Hinweise zur Prüfung durch die Projektleitung

## Aktueller Stand r16 – 3. November 2026

Grundlage ist der zuletzt vom Nutzer bereitgestellte Ordner NLBinder-masterNEW
mit den live bestätigten r15-Todes-/Damagelog-Korrekturen. Diese bleiben erhalten.

[Bedienung und Installation](../NLBinder-0.1.1-r16-DE.md),
[gesamter TXT-Changelog](../CHANGELOG-GESAMT.txt) und
[maschinenlesbarer Prüfnachweis](r16-validation.json).

### Neue Funktionen und Eingriffe

- Native Ausdauerreserve als verschiebbare senkrechte Sidebar. Keine Änderung
  an Ausdauerwerten und kein nachgebauter Timer. Vorhandener Overlay-Move-Modus.
- Akkord-Anteil wird als bestehendes Job-Verdienstereignis erfasst. Keine
  zusätzliche Auszahlung oder Veränderung serverseitiger Daten.
- Bis zu 20 lokale GPS-Ziele mit eigenen GTA-Markern und Kartenpunkten.
  Eigene IDs, keine Übernahme serverseitiger Checkpoint-Slots.
- `/speed` löst Namen und eindeutige Teile ab drei Zeichen aus dem verbundenen
  Spielerpool auf. Exakte Namen bevorzugt; Mehrdeutigkeit bricht lokal ab.
- `/forum` rendert das echte Forum mit CEF-Offscreen-Rendering. Ein eigener
  x64-Prozess führt Chromium mit Sandbox aus; die x86-ASI überträgt Bild und
  Bedienung über begrenzte gemeinsame Speicherstrukturen.

Die Login- und Schreibfunktionen stammen unverändert von der Originalwebseite.
Der Binder liest weder DOM-Passwortfelder noch Cookieinhalte. Getippte Eingaben
werden nicht protokolliert; abgearbeitete IPC-Eingabefelder werden gelöscht.
CEF verwaltet sein separates lokales Profil. Externe Hauptnavigation,
Popups und Downloads werden begrenzt. Das Paket enthält keine Nutzerprofile.

### Tatsächlich ausgeführte Prüfungen

Gesamte C++-Testsuite und Release-Build: Exitcode 0. Enthalten sind 127 neue
Ausdauer-/GPS-/Akkord-/Einstellungsprüfungen sowie zusätzliche Namensauflösungs-
und Dialog-Integrationstests. Der native Reconnect-Helfer wird nach dem letzten
Build mit 83 Prüfungen und sechs Nova-Testfällen geprüft. Testprozesse führen
weder ein echtes Spiel noch den installierten Launcher aus.

Die offizielle CEF-Runtime hat das echte Forum in einem frischen anonymen
Testprofil gerendert. Scrollen änderte die tatsächlichen Webseitenpixel;
der Host beendete sich sauber. Separat startete der produktive x86-Controller
den x64-Browser, empfing echte Frames und schloss ihn über den normalen
asynchronen Ablauf. Es wurden keine Anmeldung, Beiträge oder Nachrichten
automatisch abgeschickt. Diese neuen Funktionen wurden nicht in einer
laufenden GTA-Sitzung live getestet; das wird nicht als bereits belegt behauptet.

Chromium wird wegen seiner Größe im vollständigen GitHub-Release-ZIP
mitgeliefert. Quellcode, fixiertes SDK, Runtime-Prüfsummen und die ASI liegen
im Repository. `NLBinder/Browser` muss bei der Installation neben die ASI.

## Vorheriger Stand r15 – 30. Oktober 2026
'''
p=R/'docs/review/PRUEFUNG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '## Aktueller Stand r16' not in t:t=intro+t.split('## Aktueller Stand r15 – 30. Oktober 2026',1)[1]
p.write_text(t,encoding='utf-8')

p=R/'docs/review/DEPENDENCIES.md';t=p.read_text(encoding='utf-8-sig')
if '## Chromium/CEF für r16' not in t:
    t+='''
## Chromium/CEF für r16

CEF `156.0.15+gcf671c0+chromium-156.0.8078.80`, stable, Windows x64.
Quelle: [offizieller CEF-Buildindex](https://cef-builds.spotifycdn.com/index.json).
Der SDK-Download ist in [cef-runtime.lock.json](../../tools/cef-runtime.lock.json)
mit Größe, offizieller SHA-1 und SHA-256 fixiert. Der Downloader prüft beide
Prüfsummen. Ein x86-SDK existiert ebenfalls; der getrennte x64-Prozess wurde
wegen des knappen Adressraums von GTA und zur Prozessisolation gewählt.

Die ursprüngliche offizielle `bootstrap.exe` lädt die selbst gebaute
`NLBinderBrowser.dll`. Die moderne Chromium-Sandbox bleibt aktiv; es wird kein
`--no-sandbox` verwendet. Siehe [offizielle Sandbox-Anleitung](https://chromiumembedded.github.io/cef/sandbox_setup).
Der Windows-Launcher startet den Host mit normalen Benutzerrechten und setzt
die notwendigen LPAC-Leserechte ausschließlich auf den dedizierten Runtime-Ordner.

Das vollständige Release enthält CEF-LICENSE.txt und CEF-README.txt neben den
Originalbibliotheken, Ressourcen und Sprachpaketen. Die dateigenauen Hashes
stehen in [BROWSER-RUNTIME.json](../../dist/NLBinder-0.1.1-r16/BROWSER-RUNTIME.json).
Quellen/Buildskripte des eigenen Hosts stehen unter `browser/` und `tools/`.
Der große CEF-Binärbestand gehört zum Release-ZIP und wird nicht als einzelne
Datei in das Git-Repository eingecheckt. CEF-SDK und Zwischenstände bleiben
lokal unter `build/`; es werden keine Browserprofile veröffentlicht.
'''
p.write_text(t,encoding='utf-8')

for name in ['docs/BUILD-README.md','README.md']:
    p=R/name;t=p.read_text(encoding='utf-8-sig')
    if '## r16 additions' not in t:
        t+='''
## r16 additions

Stamina sidebar, Akkord earnings, 20 persistent local GPS destinations,
player-name resolution for `/speed`, and a real CEF offscreen forum browser.
See [German r16 instructions](docs/NLBinder-0.1.1-r16-DE.md).

The ASI remains x86. The browser runs separately as x64 using the original CEF
sandbox bootstrap. Fetch the pinned SDK with `python tools/fetch_cef_runtime.py`,
build with `tools/build_forum_browser.bat`, and assemble the required runtime
with `python tools/prepare_browser_runtime.py`. Install `build/NLBinder/Browser`
next to `NLBinder.asi`; the ASI alone does not contain Chromium. Windows x64,
VS 2022 x86/x64 v143 tools, Windows SDK and CMake are required.

`tools/run_revision16.bat tests` runs the complete suite. `smoke` loads the
real forum anonymously; `native-smoke` exercises x86-to-x64 IPC with a fresh
test profile. No automated account login or post submission is performed.
'''
    if name.startswith('docs/'):
        t=t.replace('(docs/NLBinder-0.1.1-r16-DE.md)','(NLBinder-0.1.1-r16-DE.md)')
    p.write_text(t,encoding='utf-8')

readme='''# PerkysBinderChangelog

Vollständiger Prüfstand von **NLBinder v.0.1.1 by KOBiRO, r16**.
Grundlage und ursprüngliche Urheberhinweise von Perkys bleiben erhalten.
Der aktuelle Quellstand basiert auf dem zuletzt bereitgestellten NLBinder-masterNEW.

r16 ergänzt die verschiebbare native **Ausdauer-Sidebar**, **Akkordverdienste**,
**20 gespeicherte GPS-Ziele**, **Spielernamen für `/speed`** und das echte
**Nova-Forum im Spiel** über Chromium/CEF. Die bisherigen Todes-/Damagelog-
Korrekturen und alle anderen Binder-Funktionen bleiben enthalten.

## Einstieg für die Projektleitung

1. [Gesamter Changelog](docs/review/CHANGELOG-DE.md), auch [als TXT](docs/CHANGELOG-GESAMT.txt)
2. [Verhalten, tatsächliche Prüfungen und Grenzen](docs/review/PRUEFUNG-DE.md)
3. [Dateivergleich zum unveränderten Original](docs/review/DATEIVERGLEICH.md)
4. [Abhängigkeiten und Herkunft](docs/review/DEPENDENCIES.md)
5. [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r16)
6. [Nur Änderungen von r15 nach r16](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r15...v0.1.1-r16)

Der vollständige bearbeitete Code liegt in `src/`, `include/`, `browser/`,
`tests/`, `res/`, `tools/` und `third_party/`. Die ursprüngliche Importhistorie,
der Tag `original-archive-92ce219`, der getrennte Abhängigkeitsimport und alle
früheren Veröffentlichungen bleiben nachvollziehbar erhalten. Die ursprüngliche
ZIP-Commitkennung lautet `92ce21971bee2d166d830e23ad20950f1dc98921`.

## Fertiges Paket

- [Vollständiges r16-Release mit ASI und Browser-Runtime](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r16)
- [NLBinder.asi](dist/NLBinder-0.1.1-r16/NLBinder.asi)
- [Installation und neue Befehle](dist/NLBinder-0.1.1-r16/LIESMICH.md)
- [Build-/Prüfangaben](dist/NLBinder-0.1.1-r16/BUILD-INFO.txt)
- [ASI-Prüfbericht](dist/NLBinder-0.1.1-r16/PRUEFBERICHT.json)
- [Browser-Dateien und Prüfsummen](dist/NLBinder-0.1.1-r16/BROWSER-RUNTIME.json)
- [Gesamtnachweis r16](docs/review/r16-validation.json)

Für `/forum` muss der Ordner **NLBinder/Browser** aus dem vollständigen ZIP
neben die ASI kopiert werden. Die große Runtime liegt als GitHub-Release-Anhang;
die ASI allein enthält Chromium nicht. Es werden keine persönlichen
Einstellungen, Chatlogs, UCP-Zugangsdaten oder Browserprofile veröffentlicht.

Die gesamte automatisierte Testsuite und der Release-Build sind erfolgreich.
Das echte Forum wurde anonym gerendert und gescrollt; die native x86-Anbindung
hat den x64-Browser mit Sandbox gestartet und sauber beendet. Diese neuen
Funktionen wurden noch nicht in einer laufenden GTA-Sitzung live getestet.
Die frühere r15-Todeskorrektur war bereits durch Explosion und Friedhofs-Relog
im Spiel bestätigt: [damaliger Live-Prüfumfang](docs/review/r15-live-validation.json).

[Ursprüngliche Projektbeschreibung](docs/ORIGINAL-README.md) ·
[Aktuelle Build-Anleitung](docs/BUILD-README.md)

Weitere Updates erscheinen als neue Commits mit Changelog, Quellcode,
passenden Prüfungen und eindeutiger ASI-Prüfsumme. Ältere Stände bleiben erhalten.
'''
(R/'build/README-GITHUB-r16.md').write_text(readme,encoding='utf-8')
import re
nested=re.sub(r'\]\((docs/|dist/)',r'](../../\1',readme)
(R/'docs/review/README-GITHUB.md').write_text(nested,encoding='utf-8')
print('Updated root review entry, dependencies and full build instructions')
