# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 11

Stand: 29. Oktober 2026. Diese Fassung ergänzt eine manuelle Synchronisation
des aktuellen Spielstands und verwendet den Dateinamen **NLBinder.asi**.
Alle bisherigen Binder-Anpassungen bleiben enthalten.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige **Keybinder.asi** durch **NLBinder.asi** am bisherigen
   Installationsort ersetzen. Die alte ASI aus dem Ladeordner entfernen,
   damit nur eine Binder-Fassung geladen wird.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 normal starten und einloggen.

Der vorhandene ASI-Loader bleibt erforderlich. Eigene Einstellungen und
Waffenbilder bleiben im bisherigen NLBinder-Ordner unter
**Dokumente → GTA San Andreas User Files → NLBinder**. Die mitgelieferte
leere Waffenicon-Ordnerstruktur kann dort zusammengeführt werden.

## Aktuellen Spielstand manuell synchronisieren

Unter **Einstellungen**, direkt unter **Tacho**, auf
**Aktuellen Spielstand synchronisieren** klicken. Der Binder liest die
eigenen `/stats` und danach `/jobskills` erneut ein. Die empfangenen
Gesamtstände und Skillpunkte werden direkt für die bisherigen Anzeigen,
Skill-Feeds und Textvariablen übernommen und gespeichert.
Das Bindermenü schließt sich für den Abruf.

Nach erfolgreichem Abschluss beider Abrufe erscheint nur lokal:

`[NL-BINDER] Du hast deinen Spielestand manuell synchronisiert. Deine Werte sind jetzt im aktuellsten Stand.`

Eine laufende Synchronisation wird nicht nochmals gestartet. Fehlende oder
unpassende Antworten führen nicht zu einer Erfolgsmeldung. Der Abruf
benötigt echte Serverantworten; wie beim Einloggen schließt der Binder die
zugehörigen eigenen Dialoge nach dem Lesen. Die automatische Synchronisation
beim Einloggen bleibt bestehen.

## Lokale Killmeldung

Die eigene lokale Killmeldung lautet nun
`[NL-BINDER] Du hast einen Kill gemacht! Aktuell hast du: {kills} Kills.`.
Anstelle von `{kills}` erscheint der aktuelle Gesamt-Killstand. Der
frei eingestellte Killbind für den Server bleibt erhalten und wird nicht
zusätzlich als lokaler Hinweis wiederholt. Bestehende lokale Altvorlagen
werden einmalig auf den neuen Text umgestellt.

## Todeszählung und Damagelog

Die native Todeserkennung und die Friedhofsmeldung werden für dasselbe
Ereignis zusammengeführt, damit der Todeszähler nur einmal steigt.
Explosionen und ungültige Lebenswertübergänge erzeugen keine
Millionenwerte mehr im Damagelog. Tatsächliche gültige Schadensänderungen bleiben sichtbar.
Das Damagelog-Präfix verwendet nun die Farbe `{33CCFF}`.

## Vollständiger Quellordner und Changelog

Der angeforderte Desktop-Ordner **NLBinder-masterNEW** enthält den gesamten
Quellcode einschließlich der benötigten Abhängigkeiten, die fertige
**NLBinder.asi** und **CHANGELOG-GESAMT.txt**. Die Projektdateien heißen
weiterhin **Keybinder.sln** und **Keybinder.vcxproj**; der Release-Build
erzeugt **NLBinder.asi**.

Die Textdatei beschreibt den gesamten dokumentierten Änderungsverlauf,
einschließlich der früheren Job-, Overlay-, Tacho-, Kill-/Todes-, Reconnect-,
Interface-, Waffenicon- und Textvariablen-Anpassungen. Sie liegt auch
diesem Paket bei. Persönliche Laufzeitkonfigurationen, Zugangsdaten und
Compiler-Zwischendateien gehören nicht zur Quellcode-Weitergabe.

Der Quellordner enthält die aktuelle ASI. Frühere Binärpakete bleiben über
die GitHub-Historie erreichbar und werden dort nicht als zusätzliche alte
Binder-Kopien beigelegt. Die **README.md** im Quellordner beschreibt das
Bauen des Projekts; die Übersicht für die Projektleitung liegt zusätzlich
unter **docs/review/README-GITHUB.md**.

## Prüfung und frühere Funktionen

Die Paketnachweise stehen in **BUILD-INFO.txt**, **SHA256SUMS.txt** und
**PRUEFBERICHT.json**. Der Reconnect-Helfer bleibt in der ASI enthalten.
Automatisierte Helfertests verwenden ausschließlich harmlose Testprozesse;
der installierte Nova Launcher und das echte Spiel werden dafür nicht
gestartet oder beendet. Ein echter Ingame-Test steht aus.

Jobs und Skilltrees, Fraktionen, getrennte Freunde-/Feinde-Overlays,
Damagelog, Kill-/Todeszähler, frei gewählte Killtexte, `/speed ID`, der
Nova-/OpenMP-Reconnect, Waffenicons, die vier Interface-Bereiche und
`{input}` für manuelle Textbindungen bleiben enthalten. Die Bedienung
dieser früheren Änderungen steht im vollständigen Changelog.

Quellcode und Prüfstand werden im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
bereitgestellt. Frühere Stände und ursprüngliche Urheberhinweise bleiben
erhalten.
