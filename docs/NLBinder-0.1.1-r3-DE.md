# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 3

Diese Fassung enthält alle bisherigen Anpassungen am Perkys Binder und die
Korrekturen für getrennte Radars, Login-Hitsound und Reconnect sowie den
zusätzlichen Damagelog. Die vorhandene Oberfläche bleibt die Grundlage.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI durch die neue `Keybinder.asi` ersetzen. Nur eine Fassung
   des Binders laden. Ein vorhandener ASI-Loader wird weiterhin benötigt.
3. SA-MP 0.3.DL-R1 starten; das Menü wie bisher mit F2, `/kb` oder `/keybinder`
   öffnen, sofern in der eigenen Konfiguration nichts anderes eingestellt ist.

Vorhandene Einstellungen werden weiterverwendet. Diese ASI wurde nicht in die
laufende Spielsitzung installiert. Ein echter Ingame-Test steht aus.

## Freunde-Radar und Feinde-Radar

- „Freunde anzeigen“ aktiviert ausschließlich das Overlay „Freunde-Radar“.
- „Feinde anzeigen“ aktiviert ausschließlich das Overlay „Feinde-Radar“.
- Beide Overlays können gleichzeitig angezeigt und unabhängig voneinander
  verschoben werden. Ihre Positionen werden getrennt gespeichert.
- Die bisherige gemeinsame Position wird bei vorhandenen Einstellungen
  übernommen; das zweite Radar wird zunächst versetzt platziert.
- Name, Fraktion/Gang/Zivilist und Spieler-ID werden weiterhin automatisch
  zugeordnet. Manuelle Fraktions-/Notizfelder entfallen.
- AFK-Abfrage, AFK-Zwischenspeicher und AFK-Anzeige sind vollständig entfernt.
- Entfernungsermittlung, Online-Watcher, Chat-Alarme, Alarmbanner und Feind-Sound
  bleiben entfernt. Alte Einstellungen aktivieren diese Funktionen nicht erneut.

Die Darstellungseinstellungen „Allgemeine Overlays“ gelten weiterhin für eigene
Fraktion, Gang-Übersicht, Ort, Fortbewegungsmittel, Pedslots und beide Radars.
Eigene Gang und Wanted-Liste behalten ihre eigenen Darstellungseinstellungen.
Red County Brotherhood ist weiterhin in Fraktionen, Zuordnung und Filtern enthalten.

## Hitsound beim Einloggen

Treffergeräusche berücksichtigen den aktiven, gespawnten lokalen Spieler.
Login-, Verbindungs-, Zuschauer-, Klassenauswahl- und Spawn-Wartephasen setzen
die Lebenswert-Vergleichsbasis zurück. Ein Pedwechsel oder ungültige Messwerte
setzen sie ebenfalls zurück. So wird der erste Wert nach diesen Übergängen
nicht als Treffer gewertet. Echte Treffer nach der ersten gültigen Messung
bleiben ohne zusätzliche Wartezeit hörbar.

## /reconnect

Unter Einstellungen / General festlegen, ob der Nova Launcher benutzt wird.
Ohne Haken wird OpenMP verwendet. Bei einer abweichenden Installation kann der
jeweilige Launcher-Pfad eingetragen werden; sonst wird er automatisch gesucht.

`/reconnect` bereitet zunächst den enthaltenen Neustart-Helfer vor und speichert
die Binder-Daten. Anschließend ruft er die lokale Beenden-Funktion des Clients
auf. Der Helfer wartet auf das tatsächliche Prozessende und startet erst danach
den Launcher. Der frühere Versand von `/q` als Serverbefehl entfällt.

Scheitert die Vorbereitung oder das Speichern, bleibt das Spiel geöffnet.
Läuft das Spiel acht Sekunden nach dem Beenden-Aufruf weiterhin, bricht der
Binder den wartenden Helfer ab und meldet dies ausschließlich lokal. Danach
kann `/reconnect` erneut versucht werden.

OpenMP erhält aktuellen Server, Port, Spielernamen und Spielordner. Beim Nova
Launcher versucht der Helfer den normalen, aktivierten Knopf „Spiel starten“ zu
betätigen. Anmeldung, Updates und Rückfragen bleiben beim Launcher; falls der
Knopf nicht verfügbar ist, muss das Spiel dort selbst gestartet werden.

## Damagelog

Unter Einstellungen / General / Gameplay steht direkt unter „Automatischer
Motorstart“ der neue Schalter „Damagelog“. Er ist standardmäßig ausgeschaltet
und wird mit den anderen Einstellungen gespeichert.

Bei aktiviertem Log erscheint erhaltener Trefferschaden ausschließlich im
eigenen Chat, beispielsweise:

```text
[NL-BINDER] Du hast -25 HP Schaden erhalten.
```

Das Log funktioniert unabhängig von den Kill-/Todeszählern, Schadenstatistiken
und Hitsounds. Login- und Spawnwechsel sowie Heilung erzeugen keine Meldung.
Erfasst wird der tatsächlich gemessene Verlust an HP und Weste. Passende
Trefferereignisse ermöglichen einzelne Zeilen auch bei mehreren Schüssen
zwischen zwei Messungen. Fehlen solche Ereignisse oder unterscheiden sich die
Schadenswerte des Servers, erscheint die gemessene Gesamtabnahme. Eine reine
serverseitige Lebenswertänderung ohne Trefferereignis lässt sich nicht sicher
von einem Script-Reset unterscheiden und wird nicht als Treffer ausgegeben.

## Killtext-Variablen

Die Hilfe im Killzähler und die Nachrichtenansicht verwenden dieselbe
bereinigte Liste. Fahrzeugwerte stehen weiterhin bei der Textbelegung zur
Verfügung. Bereits gespeicherte Vorlagen mit Fahrzeugvariablen bleiben lesbar.
HP und Weste werden direkt vom aktuellen Spieler gelesen; alte Werte aus
einer vorherigen Autofahrt werden nicht mehr verwendet.

Verfügbare Variablen im Killtext:

- `{victim}`, `{killer}`, `{weapon}` für Namen und Waffe des Ereignisses.
  Im eigenen Killtext ist `{killer}` der eigene Spielername. Fehlende
  Ereignisangaben werden als „Unbekannt“ ausgegeben.
- `{zone}`, `{ZONE}` für die eigene aktuelle Zone.
- `{hp}`, `{weste}` für die eigenen aktuellen Werte.
- `{total_kills}`, `{total_deaths}`, `{kd}` und Alias `{KD}` für den
  gespeicherten Binderstand und das daraus berechnete Verhältnis.
- `{session_kills}`, `{session_deaths}` für die seit Spielstart erfassten Ereignisse.

Die geschweiften Klammern gehören zur Variable. Diese Werte benötigen keine
zusätzlichen Spieler- oder Statistikabfragen. Ein Kill, der ausschließlich aus
einer erhöhten Statistik erkannt wird, enthält keinen sicheren Gegnernamen
oder Waffentyp; dafür werden keine Angaben erfunden.

## Bisherige Anpassungen

Die Skilltrees für Holzfäller und Bauarbeiter mit den vorgegebenen Punktestufen,
Kill- und Todeszähler, frei wählbare Killtexte mit Variablen, die lokale
Todesbenachrichtigung sowie die Tacho- und Motorstart-Korrekturen sind enthalten.
Weitere Angaben zu den Jobs stehen in `JOB-HINWEISE.md`.

Eigene Server-Gesamtstände werden nur aus tatsächlich empfangenen eigenen
Statistikblöcken übernommen. Der Binder sendet keine versteckten `/stats`-
oder `/oldstats`-Abfragen. Ohne empfangene Gesamtstände verwendet er seine
gespeicherten und neu erkannten Ereignisse.

Der Killtext unterstützt Befehle wie `/f`, `/d`, `/r` und normalen Chat ohne
Schrägstrich. Beispiele für Variablen sind `{victim}`, `{zone}`, `{total_kills}`,
`{total_deaths}` und `{kd}`. Ein leeres Textfeld sendet nichts. Für automatischen
Text müssen Killzähler und „Kill-Nachricht an Server senden“ eingeschaltet sein.

Automatisierte Tests und der Release-Build prüfen die Implementierung. Der
Reconnect-Helfer wird zusätzlich mit harmlosen Testprozessen geprüft. Diese
Prüfungen ersetzen keinen Ingame-Test mit der konkreten Server- und
Launcher-Version. Die genaue Build-Prüfung steht in `BUILD-INFO.txt`.
