# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 6

Stand: 25. Oktober 2026. Diese Fassung korrigiert einen konkreten Fehler bei
der Zuordnung des Statistikfensters. Die übrigen Binder-Anpassungen und der
einmalige Motorstart aus r5 bleiben enthalten.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI durch `Keybinder.asi` aus diesem Paket ersetzen. Nur eine
   Binder-Fassung laden. Ein vorhandener ASI-Loader wird weiterhin benötigt.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.
   Das Menü öffnet sich wie bisher mit F2, `/kb` oder `/keybinder`, sofern die
   eigene Konfiguration nicht davon abweicht.

Vorhandene Einstellungen werden übernommen. Die Prüfsumme dieser Binärdatei
und die abschließenden Prüfnachweise stehen in `BUILD-INFO.txt`,
`SHA256SUMS.txt` und `PRUEFBERICHT.json`. Die früheren r5-Prüfwerte beschreiben
ausschließlich die frühere ASI.

## Statistik mit dem tatsächlichen Fenstertitel erkennen

Der Server verwendet einen Titel im Format `Statistiken » Spielername`.
Das Unicode-Trennzeichen `»` blieb bisher beim Herauslösen des Spielernamens
stehen. Dadurch konnte der Binder die eigene Statistik als fremd abweisen,
obwohl im Inhalt der richtige Name sowie Kills und Tode standen. Das führte
zum offenen Statistikfenster, ausbleibendem Import und anschließendem Timeout.

Der Titel wird jetzt vor dem Namensvergleich korrekt getrennt. Eine fremde
Spieleridentität bleibt ausgeschlossen. Der vollständige Statistiktext wird
weiterhin direkt aus der Serverantwort gelesen; Messageboxen, Listen und
Tabellen sowie Klammern, Farbcodes und Tabulatoren werden unterstützt.

Nach dem Login wird im ersten geeigneten Spielzustand einmal `/stats`
angefordert. Sobald die eigene Antwort erkannt ist, übernimmt der Binder
Kills und Tode als Ausgangsstand, ergänzt weitere gelieferte Werte und
schließt die eigene Startstatistik im selben Update. Danach erscheint nur
lokal:

```text
[NL-BINDER] Synchronisation abgeschlossen.
```

`{total_kills}` und `{total_deaths}` verwenden danach diese Gesamtstände,
auch bei **Kill simulieren**. Der Import erzeugt keinen zusätzlichen Kill
oder Tod. Es gibt keine zusätzliche feste Wartepause; die tatsächliche
Antwortzeit des Servers bleibt notwendig.

Ein eigener manueller `/stats`-Aufruf kann nach einem Timeout weiterhin
nachsynchronisieren. Nach erfolgreicher Start-Synchronisation bleiben später
manuell geöffnete eigene Statistiken sichtbar. Andere Dialoge und fremde
Statistiken werden nicht pauschal geschlossen.

## Motorstart

Die r5-Korrektur bleibt unverändert: höchstens ein Motor-Tastendruck pro
Fahrer-Einstieg, ohne Wiederholungsversuche. Ein bereits laufender oder später
manuell gestoppter Motor wird im selben Einstieg nicht erneut bedient.
Eingabesperren bei offenem Chat, Dialog und Binder bleiben erhalten.

## Prüfung und veröffentlichte Daten

Der neue Regressionstest bildet das beobachtete Dialogformat mit erfundenem
Spielernamen und erfundenen Statistikwerten nach. Er prüft den vollständigen
komprimierten RPC-61-Pfad bis zur Übernahme, zum Schließen des eigenen
Startdialogs und zum Testkill mit den importierten Zählern. Persönliche
Screenshots und die tatsächlichen Statistikwerte werden nicht veröffentlicht.

Quellen und zugehörige Prüfnachweise stehen im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog).
Ein echter Ingame-Test mit dem konkreten Server und Launcher steht aus.
