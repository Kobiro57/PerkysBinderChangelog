"""Document r17 without modifying historical releases or private user data."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
notes='''# NLBinder v.0.1.1 by KOBiRO – r17

## Installation

Bei beendetem Spiel die bisherige Binder-ASI durch **NLBinder.asi** ersetzen.
Nur eine Binder-ASI laden. Ab r17 muss kein Browserordner mitgeschickt oder
manuell in den GTA-Ordner kopiert werden.

Der Ordner **Dokumente/GTA San Andreas User Files/NLBinder/Browser** wird beim
Start des Binders angelegt. Beim ersten `/forum` werden dort die benötigten
Browserdateien automatisch eingerichtet. Bereits mitgelieferte passende
r16-Dateien neben der ASI werden geprüft und ohne Download übernommen.
Andernfalls lädt der Binder einmalig das versionierte Browserpaket von GitHub
(188 MB komprimiert, rund 429 MB entpackt), prüft SHA-256 für Archiv und jede
Datei und öffnet anschließend das echte Forum.

Im Fenster erscheinen Fortschritt und ein Abbrechen-Button. ESC bricht die
Einrichtung ebenfalls ab. Bei einem Fehler kommt ein lokaler Hinweis;
`/forum` kann erneut aufgerufen werden. Eine beschädigte oder unvollständige
Installation wird repariert. Bereits gültige Dateien werden weiterverwendet.
Die Einrichtung läuft in einem kleinen separaten Prozess mit normalen
Benutzerrechten; sie blockiert nicht den Spielablauf. Python, PowerShell,
ein externes Entpackprogramm und Administratorrechte sind nicht erforderlich.

Die Browserdateien liegen in einem eigenen versionierten Unterordner.
Anmeldecookies bleiben im bisherigen separaten lokalen CEF-Profil unter
`%LOCALAPPDATA%/NLBinder/Browser/Forum`; diese werden nicht als Download verteilt.
Unvollständige Downloads werden nicht als funktionierende Installation benutzt.
Das heruntergeladene Archiv wird nach erfolgreicher Einrichtung entfernt.

## Forum wieder öffnen

`/forum`, ESC und Schließen funktionieren weiterhin. Beim erneuten Öffnen
wartet der Binder gegebenenfalls, bis die vorherigen eigenen Browserprozesse
vollständig beendet sind und das Profil freigeben. Ein schneller erneuter
Aufruf wird vorgemerkt und öffnet das Fenster automatisch wieder. Der
Wartezustand wird angezeigt und kann abgebrochen werden. Beim Schließen vor
dem vollständigen Chromium-Start bleibt der nächste Aufruf ebenfalls möglich.

## GPS-Markierung

`/setgps Bemerkung`, `/gpslist`, `/delgpsall` und `/gps` bleiben erhalten.
Navigieren erzeugt einen roten Welt-Checkpoint und eine größere rote Markierung
auf der Minimap. Sie ist nicht auf den Nahbereich begrenzt. Ein vom Spiel
entfernter eigener Radar-Eintrag wird während der Navigation wieder angelegt.
Wurde dessen Slot zwischenzeitlich vom Server wiederverwendet, wird dieser
fremde Marker weder verändert noch gelöscht. Auch die QuickMap berücksichtigt
die korrekten Radar-Anzeigeflags.

Wenn du direkt am gespeicherten Ziel navigierst, bleibt der Checkpoint jetzt
sichtbar. Die Ankunft wird erst nach Verlassen und erneutem Betreten des
Zielbereichs ausgelöst. Bei Navigation von außerhalb bleibt die einmalige
Ankunftsmeldung wie gewohnt. Die serverseitigen Job-Checkpoints bleiben unabhängig.

## Prüfung und Build

Gesamte vorhandene C++-Testsuite und Win32-Release-Build erfolgreich. Neue
GPS-Prüfungen sichern den Start am Ziel, genau eine Ankunft und fremde
Radar-Slots ab. Der native Browserinstaller wurde mit echtem öffentlichen
HTTPS-Download, allen 236 Datei-Prüfsummen, erneuter Verwendung, Übernahme,
Reparatur, Abbruch und Pfaden mit Leerzeichen/Umlauten geprüft.

Ein weiterer Test verwendet den produktiven x86-Controller samt eingebettetem
Installer, lädt das echte Forum mit frischem anonymen Profil und öffnet es
viermal mit demselben Profil: normal, erneut nach Schließen, sofort während
des Schließens und nach Abbruch während des Starts. Alle Prozesse endeten
sauber. Kein automatischer Accountlogin, Beitrag oder Nachrichtenversand.
Ein zusätzlicher Live-Test in einer laufenden GTA-Sitzung wird nicht behauptet.

Die Quellen des Installers stehen unter `tools/BrowserSetup.cpp`, die
ZIP-Bibliothek samt Lizenz und Herkunft unter `third_party/miniz`. Das feste
Downloadziel und alle Hashes stehen in `res/browser-runtime.json` und dem
daraus generierten `include/BrowserRuntimeLock.h`. Der Helfer wird automatisch
gebaut und als Ressource 301 in die ASI eingebettet. Die Browser-Runtime ist
unverändert gegenüber dem geprüften r16-CEF-Host. Die Sandbox bleibt aktiv.
'''
(R/'docs/NLBinder-0.1.1-r17-DE.md').write_text(notes,encoding='utf-8')
p=R/'docs/review/CHANGELOG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '## 23. ' not in t:t+='''
## 23. Nachtrag r17 – 3. November 2026

- Browserordner automatisch unter Dokumente/GTA San Andreas User Files/NLBinder
  angelegt. Nur NLBinder.asi muss weitergegeben werden.
- Eingebetteter Installer richtet beim ersten `/forum` die echte Chromium-
  Runtime im Hintergrund ein. Passende r16-Dateien werden ohne Download
  übernommen; sonst einmaliger HTTPS-Download aus dem eigenen GitHub-Release.
- Downloadquelle, Archivgröße und SHA-256 festgelegt. Jede entpackte Datei
  zusätzlich gegen das eingebettete Manifest geprüft. ZIP-Pfade, doppelte
  Einträge, Verknüpfungen und fremde Dateien werden abgelehnt.
- Fortschrittsfenster, Abbruch, erneuter Versuch und Reparatur ergänzt.
  Installation zunächst separat vorbereitet und erst vollständig aktiviert.
  Vorherige beschädigte Runtime bleibt bei Reparatur als Sicherung erhalten.
- Keine Python-/PowerShell-/Entpackerinstallation beim Nutzer nötig. Separater
  nativer Helfer mit asInvoker-Manifest; keine UAC-Abfrage. Chromium weiterhin
  als eigener x64-Prozess mit aktivierter Sandbox und eigenem Cookieprofil.
- Wiederöffnen des Forums abgesichert: vollständiges Ende der eigenen
  Prozessgruppe abwarten, erneuten Aufruf vormerken und Wartezustand anzeigen.
  Auch schnelles Wiederöffnen und Schließen während des Starts geprüft.
- GPS-Ziel am eigenen Standort bleibt sichtbar, bis der Zielbereich verlassen
  und erneut betreten wird. Bisherige sofortige Entfernung dadurch behoben.
- Rote Minimap-Markierung größer und dauerhaft sichtbar konfiguriert.
  Entfernte eigene Blips wiederhergestellt; recycelte Server-Radarslots anhand
  ihrer Generation und Position geschützt. QuickMap-Anzeigeflags korrigiert.
- Neue GPS-/Radar-Regressionsprüfungen, native Installer-Tests mit echtem
  HTTPS-Download und vier echte Forum-Öffnungen desselben anonymen Profils.
  Keine Anmeldung/Beiträge automatisch ausgeführt, kein GTA-Live-Test behauptet.
'''
p.write_text(t,encoding='utf-8')

p=R/'docs/review/PRUEFUNG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '## Aktueller Stand r17' not in t:
    t=t.replace('## Aktueller Stand r16','## Vorheriger Stand r16',1)
    pre='''## Aktueller Stand r17 – 3. November 2026

[Bedienung und Installation](../NLBinder-0.1.1-r17-DE.md),
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt),
[Prüfnachweis](r17-validation.json).

Die Browserdateien werden bei Bedarf automatisch unter Dokumente installiert.
Der eingebettete Win32-Helfer arbeitet außerhalb des Spielprozesses, verwendet
WinHTTP mit regulärer HTTPS-Prüfung und fest begrenzten GitHub-Downloadhosts.
Dateigrößen, Archiv-SHA-256 und alle 236 Datei-Hashes sind in der ASI bzw. im
eingebetteten Helfer fixiert. Die ZIP-Bibliothek liest Daten stromweise;
ungeprüfte Dateien werden nicht ausgeführt. Vor Aktivierung wird das ganze
Dateiset geprüft. Der Installer benötigt keine Adminrechte oder Skriptlaufzeit.

Der native Test hat den echten öffentlichen Download ausgeführt, ein bereits
gültiges Dateiset ohne Netz wiederverwendet, vorhandene Dateien übernommen,
eine gleich große beschädigte Datei repariert und Abbruch/falsche Archive
geprüft. Private Testverzeichnisse mit Umlauten und Leerzeichen wurden benutzt.
Die persönlichen Dokumente und Nutzerprofile waren nicht Testziel.

Der produktive x86-Browsercontroller hat aus seiner eigenen Ressource den
Installer gestartet, die Runtime in einem isolierten Testordner eingerichtet
und das echte Forum viermal mit demselben frischen Profil geladen. Darunter
Wiederöffnen während des Schließens und Schließen vor dem vollständigen Start.
Der Controller wartet auf alle eigenen CEF-Prozesse, bevor er das Profil erneut
verwendet. Tastatur-/Mausbesitz hängt weiterhin am sichtbaren Forumfenster.

Die GPS-Tests prüfen den früheren sofortigen Ankunftsfall sowie genau eine
Ankunft nach Verlassen/Wiederbetreten. Generation, Typ und Position des
Radareintrags verhindern das Löschen eines recycelten Servermarkers. Minimap-
und Weltmarkierung verwenden eigene Einträge und ersetzen keine SA-MP-Slots.

Gesamte Testsuite und Release-Build erfolgreich; genaue Zahlen und SHA-256
stehen im Prüfnachweis. Kein automatischer Login/Beitrag/Nachrichtenversand.
Ein Live-Test der neuen Fassung innerhalb von GTA wird nicht behauptet.

'''
    t=t.replace('## Vorheriger Stand r16',pre+'## Vorheriger Stand r16',1)
p.write_text(t,encoding='utf-8')

p=R/'docs/review/DEPENDENCIES.md';t=p.read_text(encoding='utf-8-sig')
if '## Automatische Browserinstallation ab r17' not in t:t+='''
## Automatische Browserinstallation ab r17

Der bereits geprüfte CEF-Bestand wird als eigenständiges, unveränderlich
gepinntes [Browser-Release](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/browser-cef156-r1)
bereitgestellt. [Manifest](../../res/browser-runtime.json) mit URL, Version,
Archivgröße, SHA-256 und allen Datei-Hashes; [Generator](../../tools/prepare_browser_autoinstall.py).
Ein neuer Host-/CEF-Stand bekommt eine neue Runtime-ID; alte ASIs verwenden
weiterhin ihr eigenes geprüftes Paket.

miniz 3.1.2, Commit `77d0dce8627735138c51770d1799a1ef48f2117d`, wird statisch
im Installer zum Lesen/Entpacken benutzt. Originalquellen bleiben unverändert;
das Exportheader für den statischen Build ist dokumentiert.
[Offizielle Veröffentlichung](https://github.com/richgel999/miniz/releases/tag/3.1.2),
[Lizenz](../../third_party/miniz/LICENSE),
[Dateiherkunft und Hashes](../../third_party/miniz/NLBINDER-PROVENANCE.json).
ZIP-Schreib- und Deflate-Kompressions-APIs sind im Installer deaktiviert.
WinHTTP und BCrypt stammen von Windows; zusätzliche Nutzerinstallationen
werden dafür nicht benötigt. [Installerquelle](../../tools/BrowserSetup.cpp).
'''
p.write_text(t,encoding='utf-8')

readme='''# PerkysBinderChangelog

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
'''
(R/'build/README-GITHUB-r17.md').write_text(readme,encoding='utf-8')
(R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/)',r'](../../\1',readme),encoding='utf-8')

p=R/'docs/BUILD-README.md';t=p.read_text(encoding='utf-8-sig')
if '## r17 automatic browser setup' not in t:t+='''
## r17 automatic browser setup

Distribute only NLBinder.asi. The embedded native BrowserSetup.exe installs
the pinned runtime into the NLBinder Browser folder in Documents on first
/forum. The browser cookie profile remains separate in LocalAppData.
The old r16 instruction to copy the Browser directory beside the ASI is
historical; r17 can migrate such a supplied folder without downloading it.

The installer sources are tools/BrowserSetup.cpp and third_party/miniz.
BuildBrowserSetup runs automatically before ResourceCompile and embeds the
result as RCDATA 301. The immutable archive lock and per-file hashes are
res/browser-runtime.json and include/BrowserRuntimeLock.h. Building the ASI
does not require re-downloading or re-building CEF when using this pinned host.
The existing CEF host build instructions remain valid for a future host update.

tools/run_revision17.bat tests runs the full suite. setup-tests additionally
tests the native installer against the actual public pinned HTTPS download in
private fixture directories. native-smoke installs from the embedded resource,
opens the real forum four times with one anonymous profile and verifies clean
shutdown and queued/early reopen. No account login or post is automated.
'''
p.write_text(t,encoding='utf-8')
print('Documented automatic browser setup, repeated forum opens and GPS checkpoint fixes')
