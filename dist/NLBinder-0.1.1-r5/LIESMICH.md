# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 5

Stand: 25. Oktober 2026. Diese Fassung korrigiert die erneuten Rückmeldungen
zum mehrfachen Motorstart und zur nicht erkannten Statistik. Die vorhandene
Oberfläche sowie frühere Job-, Radar-, Zähler-, Damagelog-, Tacho- und
Reconnect-Anpassungen bleiben enthalten.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI durch `Keybinder.asi` aus diesem Paket ersetzen. Nur eine
   Binder-Fassung laden. Ein vorhandener ASI-Loader wird weiterhin benötigt.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.
   Das Menü öffnet sich wie bisher mit F2, `/kb` oder `/keybinder`, sofern die
   eigene Konfiguration nicht davon abweicht.

Vorhandene Einstellungen werden übernommen. Prüfsumme und abschließende
Buildnachweise gehören zu dieser Binärdatei und stehen in `BUILD-INFO.txt`,
`SHA256SUMS.txt` und `PRUEFBERICHT.json`. Ein echter Ingame-Test mit dem
konkreten Server und Launcher steht aus.

## Motor nur einmal starten

**Einstellungen → General → Gameplay → Automatischer Motorstart** verwendet
weiterhin die vorgesehene Motor-Taste. Pro Fahrer-Einstieg wird sie höchstens
einmal gedrückt. Wiederholungsversuche und die zusätzliche Planung anhand
einer Fahrzeug-Einstiegs-RPC wurden entfernt: Die Motor-Taste ist auf dem
Server ein Umschalter und darf bei verzögerter Motorbestätigung nicht erneut
ausgelöst werden.

Der Binder wartet auf den erkannten Fahrersitz und die bestehende Wartezeit
von 1,5 Sekunden. Offener Chat, Dialog, Binder oder fehlender Spielfokus
blockieren den Tastendruck. Sobald die Eingabe wieder frei ist, erfolgt der
eine geplante Druck; es entsteht keine Reihe von Versuchen.

Ein bereits laufender Motor bekommt keinen Druck. Ein später manuell
ausgeschalteter Motor wird während desselben Einstiegs nicht wieder gestartet.
Auch erneutes Anwenden oder Aus-/Einschalten der Option erzeugt nach dem
verbrauchten Einstieg keinen weiteren Druck. Erst ein echter Ausstieg oder
ein anderes Fahrerfahrzeug erlaubt wieder einen automatischen Start.

## Statistik direkt übernehmen und schließen

Nach dem Login wird im ersten geeigneten Spielzustand einmal `/stats`
angefordert. Es gibt keine zusätzliche feste Wartepause. Der Binder liest den
vollständigen vom Server gesendeten Dialogtext direkt aus der Statistikantwort,
bevor dessen Anzeige den Text kürzen oder anders darstellen kann.

Neben Chatstatistiken werden Statistik-Messageboxen, Listen und Tabellen
unterstützt. Der Parser berücksichtigt bis zu 128 Zeilen, Farbcodes und
Tabulatoren sowie Zahlen in eckigen Klammern, runden Klammern oder ohne
Klammern. Für eine erfolgreiche Übernahme genügen die eigenen Kills und Tode.
Weitere tatsächlich gelieferte Werte wie Bargeld, Kontostand, Vermögen,
Level/XP, Gold, Eventcoins und Wantedpunkte werden ergänzend gespeichert;
fehlende Geldwerte verhindern die Übernahme der Zähler nicht mehr.

Der erste Gesamtstand ist ein Ausgangsstand und erzeugt keinen zusätzlichen
Kill oder Tod. `{total_kills}` und `{total_deaths}` verwenden danach die
übernommenen Zähler, auch bei **Kill simulieren**. Nach dem Lesen wird nur die
identifizierte eigene Startstatistik geschlossen. Import und Schließen
erfolgen im selben Update, ohne dass der Spieler den Dialog manuell bestätigen
muss. Anschließend erscheint ausschließlich lokal:

```text
[NL-BINDER] Synchronisation abgeschlossen.
```

Die Antwortzeit des Servers lässt sich dabei nicht überspringen. Ohne
erkannte eigene Kills und Tode erscheint nach dem begrenzten Wartefenster ein
lokaler Fehlerhinweis statt einer Erfolgsmeldung. Ein eigener manueller
`/stats`-Aufruf kann danach noch nachsynchronisieren; es gibt keine automatische
Abfrageschleife. Nach bereits abgeschlossener Start-Synchronisation bleiben
später manuell geöffnete eigene Statistiken sichtbar und können empfangene
Werte aktualisieren. Fremde Statistiken und andere Dialoge werden nicht
pauschal geschlossen oder übernommen.

## Prüfung

Die vollständigen Quellen, Regressionstests und Prüfnachweise werden im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
zusammen mit diesem Stand bereitgestellt. Frühere r4-Unterlagen beschreiben
weiterhin die damalige Version; ihre Prüfsumme gilt ausschließlich für deren
Binärdatei.
