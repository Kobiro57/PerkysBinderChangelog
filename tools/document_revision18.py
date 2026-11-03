"""Document the render-loop and installer-completion correction in r18."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
notes='''# NLBinder v.0.1.1 by KOBiRO – r18

## Installation

GTA vollständig beenden, NLBinder.asi ersetzen und das Spiel neu starten.
Browserdateien werden weiterhin automatisch unter Dokumente/GTA San Andreas
User Files/NLBinder/Browser eingerichtet. Nur die ASI muss verteilt werden.
Der Chromium-Bestand bleibt derselbe fest geprüfte Stand cef156-r1.

## Automatischer AFK-Modus beim Forum

Beim Öffnen des Forumfensters sendet der Binder einmal `/afk`. Beim Schließen
über ESC, den Schließen-Button, das Fensterkreuz oder erneutes `/forum` folgt
einmal `/back`. Das gilt auch beim Abbrechen der Einrichtung und wenn das
Fenster wegen eines Fehlers geschlossen wird. Der Wechsel von der Einrichtung
zur fertigen Webseite löst kein zusätzliches Befehls-Paar aus. Wiederholte
Render-Aufrufe schicken keine weiteren AFK-Befehle.

Die Umschaltung verwendet die vorhandenen Serverbefehle. Die tatsächlichen
Einschränkungen des AFK-Modus bestimmt weiterhin der Server.

## Browserinstallation und 100-Prozent-Hänger

Der Installer-Abschluss und GPS wurden zuvor ausschließlich über einen
Game-Process-Aufruf verarbeitet. r18 aktualisiert den Status zusätzlich über
den tatsächlich gerenderten Browser und den vorhandenen HUD-Aufruf. Auch ein
bereits geschlossener Browser wird dabei weiter abgewickelt. Ein erneuter
`/forum`-Aufruf räumt einen verlassenen Installerzustand auf.

Sobald alle Dateien geprüft und atomar aktiviert sind, meldet der Installer
Ready. Der Binder öffnet dann sofort Chromium und wartet nicht erst auf
temporäre Dateibereinigung oder das Ende des Hilfsprozesses. Während der
Einrichtung bleibt die Anzeige unter 100 Prozent. Download, Entpacken und
Fertigstellen haben eigene Fortschrittstexte; der Download zeigt die Datenmenge.

Abbrechen beendet ausschließlich den eigenen Installer und setzt das Fenster
sofort zurück. `/forum` kann direkt erneut gestartet werden. Ein vollständig
heruntergeladenes und geprüftes ZIP bleibt bei abgebrochenem Entpacken im
Browsercache und wird beim nächsten Versuch ohne erneuten Download verwendet.
Teildownloads werden nicht als gültiges Paket verwendet. Ein nicht mehr
reagierender Installer bleibt nicht unbegrenzt im Fenster hängen.

Beim Entpacken wird SHA-256 direkt während des Schreibens gebildet. Dadurch
entfallen zusätzliche vollständige Lesedurchläufe. Eine bereits vollständig
geprüfte Installation erhält einen lokalen Nachweis über Dateigrößen und
Änderungszeiten. Unveränderte Dateien können schnell wiederverwendet werden;
bei Änderungen wird erneut geprüft. Der einmalige Download von rund 188 MB
hängt weiterhin von der Verbindung ab.

## Normaler GPS-Checkpoint

Navigieren erzeugt den Marker direkt aus der GPS-Auswahl, ohne auf einen
Game-Process-Tick zu warten. Das Ziel wird anschließend über die vorhandenen
Render-/HUD-Aufrufe gepflegt. Für die Weltmarkierung wird die normale native
GTA-Checkpoint-Funktion mit rotem Zylinder, passender Bodenhöhe und Radius
3 Meter verwendet. Die rote Minimap-Markierung bleibt zusätzlich bestehen.
Der Marker erhält ausdrücklich die Freigabe für den aktuellen Renderdurchlauf.
Das normale Anlegen setzt nur den Belegt-Status; ohne die später von GTA
gesetzte Renderfreigabe kann er vor dem Zeichnen wieder verschwinden.

Beim Einfahren in den Zielbereich kommt genau eine lokale Ankunftsmeldung.
Wer bereits beim Navigieren am Ziel steht, sieht den Marker bis zum Verlassen
und erneuten Betreten. Eigene Markierungen werden beim Beenden der Navigation
entfernt. Fremde Servermarker bleiben über die bestehende Slotprüfung geschützt.

## Was tatsächlich geprüft wurde

Die gesamte C++-Testsuite und der Release-Build sind erfolgreich. Die neuen
GPS-Tests erzeugen das Ziel direkt bei Navigieren und treiben Navigation und
Ankunft ausschließlich über Render-Aufrufe. Zusätzlich führt ein isolierter
Test den produktiven GPS-Adapter mit echten GTA-Strukturen und ersetzten
Engine-Funktionen aus: normaler Checkpoint, Rotfärbung, Bodenhöhe, Minimap,
Bildwechsel, Zielwechsel, Löschen und recycelte fremde Radar-Slots.

Die Installer-Prüfungen verwenden das echte öffentliche HTTPS-Paket, prüfen
alle 236 Dateien sowie Wiederverwendung, Reparatur und Abbruch. Ein harter
Abbruch während des Entpackens mit anschließendem Neustart wurde ebenfalls
geprüft; der erneute Versuch nutzte den vorhandenen Download ohne Netzwerk.

Der echte Chromium-Host wurde viermal mit demselben anonymen Testprofil
geöffnet. Der Controller wurde dabei ausschließlich durch Render-Aufrufe
angetrieben. Der Installer blieb absichtlich nach Ready noch 15 Sekunden
am Leben: Das Forum öffnete trotzdem sofort nach dem fertigen Dateistand.
Abbrechen und unmittelbarer neuer Start wurden zusätzlich geprüft.

Die AFK-/Back-Umschaltung wurde dabei mit sechs vollständigen Befehls-Paaren
über einen ersetzten Befehlsversand geprüft. Dadurch gehen aus dem Testprogramm
keine echten AFK-Befehle an den Server.

Der Nutzer bestätigte den Forum-/GPS-Test mit einer vorläufigen r18-ASI im
Spiel. Die danach ergänzte ausdrückliche Renderfreigabe und AFK-Umschaltung
sind davon getrennt automatisiert geprüft. Die Prüfsumme der tatsächlich im
Spiel getesteten ASI steht im separaten Live-Nachweis. Für einen Sichttest der
finalen Erweiterung muss die neue ASI bei beendetem Spiel eingesetzt werden.
Die Tests verwenden keine persönlichen Browserprofile, Anmeldungen oder Beiträge.
'''
(R/'docs/NLBinder-0.1.1-r18-DE.md').write_text(notes,encoding='utf-8')
p=R/'docs/review/CHANGELOG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '## 24. ' not in t:t+='''
## 24. Nachtrag r18 – 3. November 2026

- Abhängigkeit der Browser-/GPS-Koordination vom Game-Process-Hook entfernt:
  Installer und Browser laufen über Render/HUD weiter, GPS wird direkt beim
  Navigieren erzeugt und über gerenderte Bilder aktualisiert.
- Installer-Ready als fertigen Dateistand auswerten, ohne auf Prozessende und
  temporäre Dateibereinigung zu warten. Anzeige bis dahin höchstens 99 Prozent.
- Abbrechen beendet den eigenen Installer und räumt den Zustand unmittelbar
  auf. Verlassenen versteckten Zustand beim erneuten `/forum` ebenfalls lösen.
- Fertiges geprüftes Downloadarchiv bei abgebrochenem Entpacken aufheben und
  beim nächsten Versuch ohne erneuten Netzdownload verwenden.
- SHA-256 während des Entpackens statt zusätzlicher kompletter Dateileseläufe.
  Unveränderte geprüfte Dateien anhand Größe/Änderungszeit schnell wiederverwenden.
- Normale native rote GTA-Checkpoint-Erzeugung und Bodenplatzierung verwendet.
  Zielwechsel sofort übernehmen, vorhandene Minimap-/Fremdmarkerprüfung erhalten.
- Eigenen Checkpoint ausdrücklich für den aktuellen Renderdurchlauf freigeben.
  GTA setzt diese Freigabe normalerweise erst in einem getrennten Update;
  ein nur als belegt markierter neuer Checkpoint darf davor nicht verschwinden.
- Bisherige Komponententests erfassten die ausschließliche Game-Process-
  Abhängigkeit nicht. Neue Tests treiben Browser/GPS ausdrücklich über Render.
- Tatsächlichen Installer mit 15 Sekunden verzögertem Ende nach Ready getestet:
  Forum öffnet vor diesem Prozessende. Abbruch und unmittelbarer Retry geprüft.
- Produktiven GPS-Adapter mit GTA-Strukturen und ersetzten Engine-Aufrufen
  isoliert geprüft. Ein Sichttest im laufenden Spiel wird getrennt behandelt.
- ZIP-Bibliotheks-Lizenz zusätzlich in der einzelnen ASI als Ressource sowie
  im Paket mitgeliefert; weiterhin ist kein Browserordner manuell erforderlich.
- Auf zusätzlichen Nutzerwunsch `/afk` beim Öffnen des Forumfensters und
  `/back` beim Schließen ergänzt. Fensterwechsel statt jedes Bild auswerten;
  ESC, Button, Wiederöffnen, Einrichtungsabbruch und Fehler ebenfalls behandeln.
'''
p.write_text(t,encoding='utf-8')
p=R/'docs/review/PRUEFUNG-DE.md';t=p.read_text(encoding='utf-8-sig')
if '## Aktueller Stand r18' not in t:
    t=t.replace('## Aktueller Stand r17','## Vorheriger Stand r17',1)
    intro='''## Aktueller Stand r18 – 3. November 2026

[Bedienung und Änderung](../NLBinder-0.1.1-r18-DE.md),
[Gesamtnachweis](r18-validation.json), [gesamter TXT-Changelog](../CHANGELOG-GESAMT.txt).

Die Komponenten wurden bisher direkt durch Test-Updates getrieben. Das belegte
keine korrekte Einbindung des Game-Process-Hooks in die konkrete Clientumgebung.
r18 entfernt diese alleinige Abhängigkeit. Der konkrete Hookzustand des laufenden
Spiels konnte nicht per Prozessspeicher verifiziert werden; die Quellanalyse
und die neuen unabhängigen Abläufe sind davon zu unterscheiden.

Native Tests laden das echte öffentliche Browserpaket und prüfen alle Dateien.
Ein harter Abbruch während des Entpackens wird durch einen netzlosen Retry aus
dem geprüften Cache gefolgt. Der echte x86-Controller öffnet das Forum viermal
über reine Render-Aufrufe, mit sofortigem Cancel/Retry und absichtlich 15 Sekunden
verzögertem Installer-Ende nach Ready. Die Übergabe wartet nicht auf dieses Ende.

Der GPS-Adapter wird zusätzlich als produktiver Code mit den GTA-Strukturen
und ersetzten Engine-Aufrufen geprüft. Das prüft den tatsächlich angeforderten
Checkpoint, Boden, Radar und Slotbesitz, jedoch keine sichtbaren Pixel in GTA.
Die Render-getriebenen GPS-Tests prüfen sofortige Erzeugung und einmalige Ankunft.

Gesamttests/Release-Build und genaue Prüfzahlen stehen im Nachweis. Solange die
neue ASI noch nicht im laufenden Spiel eingesetzt und beobachtet wurde, wird
kein bestandener Sichttest behauptet. Keine Zugangsdaten/Profile veröffentlicht.

'''
    t=t.replace('## Vorheriger Stand r17',intro+'## Vorheriger Stand r17',1)
p.write_text(t,encoding='utf-8')

readme='''# PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.1 by KOBiRO, r18**.
Ursprüngliche Urheberhinweise und alle bisherigen Veröffentlichungen bleiben erhalten.

r18 korrigiert die Browserinstallation bei 100 Prozent, sofortigen Abbruch und
erneuten Start sowie die Erzeugung des normalen roten GPS-Welt-Checkpoints.
Beim Öffnen des Forums wird zusätzlich einmal `/afk` und beim Schließen
einmal `/back` an den Server geschickt.
Die Abläufe hängen zusätzlich an den tatsächlichen Render-/HUD-Aufrufen.
Es muss weiterhin nur **NLBinder.asi** verteilt werden.

## Prüfung und Quellcode

- [Vollständiger Changelog](docs/review/CHANGELOG-DE.md), auch [als TXT](docs/CHANGELOG-GESAMT.txt)
- [Tatsächlicher Prüfumfang und Grenzen](docs/review/PRUEFUNG-DE.md)
- [Maschinenlesbarer r18-Nachweis](docs/review/r18-validation.json)
- [Dateivergleich zum Original](docs/review/DATEIVERGLEICH.md)
- [Abhängigkeiten und Lizenzen](docs/review/DEPENDENCIES.md)
- [Änderungen seit r17](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r17...v0.1.1-r18)
- [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r18)

Vollständige Quellen unter `src/`, `include/`, `browser/`, `res/`, `tests/`,
`tools/` und `third_party/`. Originalimport, getrennte SDK-Basis und frühere
Revisionspatches bleiben unverändert nachvollziehbar.

## Fertiger Binder

- [r18-Paket herunterladen](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r18)
- [NLBinder.asi](dist/NLBinder-0.1.1-r18/NLBinder.asi)
- [Installation und Bedienung](dist/NLBinder-0.1.1-r18/LIESMICH.md)
- [Build- und Prüfangaben](dist/NLBinder-0.1.1-r18/BUILD-INFO.txt)
- [ASI und eingebettete Helfer](dist/NLBinder-0.1.1-r18/PRUEFBERICHT.json)
- [Festes Browser-Downloadmanifest](res/browser-runtime.json)

GTA vollständig beenden, ASI ersetzen und neu starten. Der Browser wird beim
ersten `/forum` unter Dokumente automatisch eingerichtet. Ein bereits fertig
geladener Download wird nach einem Abbruch wiederverwendet. Ready öffnet das
Forum ohne Warten auf Installer-Aufräumen. Geprüfte unveränderte Dateien werden
schnell wiederverwendet; der anfängliche Download hängt von der Verbindung ab.

Automatisierte Gesamttests, echter HTTPS-Download, erzwungener Abbruch/Retry
und vier echte Forum-Öffnungen über reine Render-Aufrufe sind geprüft. Der
isolierte GPS-Adaptertest nutzt GTA-Strukturen mit ersetzten Engine-Aufrufen.
Ein vorläufiger Forum-/GPS-Stand wurde im Spiel bestätigt; der genaue Umfang
steht im [Live-Nachweis](docs/review/r18-live-validation.json). Die nachträgliche
AFK-Erweiterung ist mit sechs Befehls-Paaren automatisiert geprüft.

Keine persönlichen Einstellungen, Profile, Zugangsdaten oder Chatlogs enthalten.
[Ursprüngliche Beschreibung](docs/ORIGINAL-README.md) · [Build-Anleitung](docs/BUILD-README.md)
'''
(R/'build/README-GITHUB-r18.md').write_text(readme,encoding='utf-8')
(R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/)',r'](../../\1',readme),encoding='utf-8')
p=R/'docs/BUILD-README.md';t=p.read_text(encoding='utf-8-sig')
if '## r18 render-driven coordination' not in t:t+='''
## r18 render-driven coordination

Forum setup is polled from Render/HUD as well as the command handler; Ready
hands off immediately without waiting for helper process cleanup. Cancellation
terminates only the owned installer job and resets state immediately. Verified
archives survive interrupted extraction and can be retried without network.
GPS navigation creates the native red checkpoint immediately and refreshes it
through scene/HUD/render callbacks, without requiring a gameProcess callback.

`tools/run_revision18.bat tests` checks the full suite; `native-smoke` runs the
real forum through render-only polling with delayed-exit/cancel fault injection.
`python tools/test_browser_restart.py` interrupts native extraction and verifies
a cached retry. `tools/test_gps_native.bat` executes the production adapter with
actual GTA structures and replaced engine functions, without opening GTA.
'''
p.write_text(t,encoding='utf-8');print('Documented r18 coordination fixes and exact test limits')
