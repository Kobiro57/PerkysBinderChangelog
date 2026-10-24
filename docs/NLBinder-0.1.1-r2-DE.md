# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 2

Die vorhandene Oberfläche des Perkys Binders bleibt die Grundlage. Diese Fassung
enthält auch die zuletzt gewünschten Skilltrees, automatischen Fraktionsangaben
und die überarbeiteten Darstellungseinstellungen.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI durch die neue `Keybinder.asi` ersetzen. Nur eine Fassung
   des Binders laden. Ein vorhandener ASI-Loader wird weiterhin benötigt.
3. SA-MP 0.3.DL-R1 starten; das Menü wie bisher mit F2, `/kb` oder `/keybinder`
   öffnen, sofern in der eigenen Konfiguration nichts anderes eingestellt ist.

Vorhandene NLBinder-Einstellungen werden weiterverwendet. Der Quellordner
KOBiRO_ASI wurde nicht verändert. Diese ASI wurde nicht in die laufende
Spielsitzung installiert; ein echter Ingame-Test steht aus.

## Online und Spieler-Radar

- „Freunde anzeigen“ und „Feinde anzeigen“ sind unabhängig schaltbar.
- Entfernungsermittlung, Online-Watcher, Online-Chat-Alarme, Alarmbanner und
  Feind-Sound sind aus dieser Fassung entfernt. Alte Einstellungen können diese
  Funktionen nicht wieder aktivieren.
- Spieler lassen sich weiterhin per Name oder aktueller Spieler-ID hinzufügen.
  Fraktion, Gang oder Zivilist werden automatisch aus den vorhandenen
  Spielerdaten zugeordnet und aktualisiert. Die manuellen Fraktions-/Notizfelder
  entfallen. Bei fehlenden Daten wird „Unbekannt“ angezeigt.
- Red County Brotherhood ist in Fraktionen, Zuordnung und Filtern ergänzt.
- Das Overlay zeigt beispielsweise `Freund: Name [Red County Brotherhood] | ID 7`.
  Ein bestätigter AFK-Status ergänzt `| AFK`.

AFK wird ausschließlich aus tatsächlich empfangenen Serverantworten auf `/id`
übernommen. Der Binder sendet keine eigenen `/id`-Abfragen. Eine Bestätigung gilt
höchstens 30 Sekunden; eine aktive Antwort, ein ID-Wechsel oder das Verlassen des
Servers entfernt den Hinweis. Eine Pause des Clients wird nicht als AFK gewertet.
Ohne aktuelle Serverbestätigung erscheint kein AFK-Zusatz.

Unter Darstellung heißt das gemeinsame Ziel jetzt „Allgemeine Overlays“.
Die Beschreibung nennt die betroffenen Anzeigen: eigene Fraktion,
Gang-Übersicht, Ort, Fortbewegungsmittel, Pedslots und Spieler-Radar. Eigene Gang
und Wanted-Liste behalten ihre getrennten Darstellungseinstellungen.

## Jobs und Skilltrees

Beide Jobs verwenden den vorhandenen Jobstil. Der Fortschritt berücksichtigt
gespeicherte Skillwerte und manuelle Punktekorrekturen.

| Punkte | Holzfäller |
| ---: | --- |
| 50 | 5 Bäume fällen |
| 100 | Holzfäller Skin freischalten |
| 250 | Privatfahrzeug: Bobcat |
| 500 | +10% Lohn |
| 1000 | +25% Lohn |
| 1500 | Job-Achievement freigeschaltet |

| Punkte | Bauarbeiter |
| ---: | --- |
| 25 | Großbaustellen werden freigeschaltet |
| 100 | Bauarbeiter Skin freischalten |
| 500 | +10% Lohn |
| 750 | Business: Blueberry Construction |
| 1000 | +25% Lohn |
| 1500 | Job-Achievement freigeschaltet |

## Kill- und Todeszähler

Die Kill-Erkennung funktioniert unabhängig von der Schadenserfassung. Bereits
vorhandene Killfeed-Zeilen werden beim Aktivieren nicht nachträglich als neue
Kills gezählt. Killfeed, Todesmeldungen und eigene Statistikstände werden
abgeglichen, damit dasselbe Ereignis nicht doppelt zählt.

Bei aktiviertem Todeszähler erscheint ausschließlich lokal:

```text
[NL-BINDER] Du bist gestorben. Deine aktuelle Tode: 206
```

Die Zahl wird beim erkannten Tod aktualisiert. Echte Servergesamtstände werden
übernommen, sobald der Client einen eigenen Statistikblock empfängt. Fremde
Statistikblöcke werden verworfen. Der Binder sendet keine versteckten `/stats`-
oder `/oldstats`-Abfragen. Ohne empfangene Servergesamtstände verwendet er seine
gespeicherten und neu erkannten Ereignisse. Falls der Server weder eine
Todes-/Killmeldung noch einen neuen Statistikstand übermittelt, kann der Binder
daraus keinen sicheren Kill ableiten.

Der eigene Killtext bleibt frei wählbar: `/f`, `/d`, `/r`, andere Befehle oder
normaler Chat ohne Schrägstrich. Variablen wie `{victim}`, `{zone}`,
`{total_kills}`, `{total_deaths}` und `{kd}` werden ersetzt. Ein leeres Textfeld
sendet nichts. Für den automatischen Text müssen Killzähler und
„Kill-Nachricht an Server senden“ eingeschaltet sein.

## Tacho und Motorstart

Der Tacho lässt sich unter Einstellungen aktivieren. Unter den Server-
Einstellungen `/settings` weiterhin TachoV2 auswählen. Der Binder blendet diesen
während seiner eigenen Anzeige aus und stellt ihn beim Deaktivieren wieder dar.
Die eigenen Anzeigen erscheinen nur als Fahrer; Fahrzeugwechsel und erneutes
Verbinden setzen die Messung zurück. RPM werden geschätzt; nicht verfügbare
Kraftstoffwerte werden nicht als echte Messwerte ausgegeben.

Automatischer Motorstart erkennt das Einsteigen auf den Fahrersitz auch dann,
wenn die bisherige Einstiegsmeldung fehlt. Er betätigt Z kurzzeitig und versucht
es begrenzt erneut, solange der Motor aus ist. Chat, Dialoge, fremde Fenster und
das geöffnete Binder-Menü blockieren die Eingabe. Als Beifahrer wird keine
Motortaste betätigt.

## /reconnect

Unter Einstellungen / General festlegen, ob der Nova Launcher benutzt wird.
Ohne Haken wird OpenMP verwendet. Bei einer abweichenden Installation kann der
jeweilige Launcher-Pfad eingetragen werden; sonst versucht der Binder die
automatische Erkennung.

`/reconnect` bereitet zuerst den Neustart vor, speichert die Binder-Daten und
beendet anschließend das Spiel mit dem normalen Clientbefehl `/q`. Der in der
ASI enthaltene Helfer wartet auf das tatsächliche Prozessende, bevor er den
Launcher startet. Fehlt der Launcher oder schlägt die Vorbereitung fehl,
bleibt das Spiel geöffnet.

OpenMP erhält aktuellen Server, Port, Spielernamen und Spielordner. Beim Nova
Launcher versucht der Helfer den normalen, aktivierten Knopf „Spiel starten“ zu
betätigen. Anmeldung, Updates und Rückfragen bleiben beim Launcher; falls der
Knopf nicht verfügbar ist, muss der Spielstart dort selbst bestätigt werden.

Automatisierte Tests prüfen die Logik und den Build. Sie ersetzen keinen
Ingame-Test mit der konkreten Server- und Launcher-Version.
