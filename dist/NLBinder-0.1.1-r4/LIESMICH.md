# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 4

Stand: 25. Oktober 2026. Diese Fassung ergänzt die bisherigen Binder-Anpassungen
um den Damagelog nach dem vorgegebenen AHK-Prinzip, getrennte Ortsvariablen,
einen Testknopf für den Killtext und die einmalige Statistik-Synchronisation.
Die vorhandene Oberfläche und die bisherigen Job-, Radar-, Tacho-, Motorstart-
und Reconnect-Anpassungen bleiben die Grundlage.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI durch `Keybinder.asi` aus diesem Paket ersetzen. Nur eine
   Binder-Fassung laden. Ein vorhandener ASI-Loader wird weiterhin benötigt.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.
   Das Menü öffnet sich wie bisher mit F2, `/kb` oder `/keybinder`, sofern die
   eigene Konfiguration nicht davon abweicht.

Vorhandene Einstellungen werden übernommen. Die genaue Prüfsumme und die
Ergebnisse des finalen Builds stehen in `BUILD-INFO.txt` und `SHA256SUMS.txt`
des Pakets. Ein echter Ingame-Test mit der konkreten Server- und
Launcher-Version steht aus.

## Damagelog

Unter **Einstellungen → General → Gameplay**, direkt unter
**Automatischer Motorstart**, lässt sich **Damagelog** einschalten.

Der Log vergleicht die aktuellen HP und die aktuelle Weste jeweils mit der
vorigen gültigen Messung. Sinkt mindestens einer der beiden Werte um mindestens
1 HP, erscheint unmittelbar nach der Messung eine lokale Chatzeile, zum Beispiel:

```text
[NL-BINDER] -25,0 HP Schaden (HP Schaden)
[NL-BINDER] -12,5 HP Schaden (Weste getroffen)
[NL-BINDER] -30,0 HP Schaden (Weste + HP Schaden)
```

Die Anzeige verwendet eine Nachkommastelle und den tatsächlich gemessenen
Verlust. Eine gleichzeitige Heilung in einem Wert zieht keinen Schaden am
anderen Wert ab. HP unter null werden als null behandelt, damit kein
zusätzlicher Overkill ausgegeben wird.

Anders als in r3 braucht dieser Log keine bestätigende Schadens-RPC und keine
passende Waffenangabe. Er erfasst auch serverseitig gesetzte HP-/Westenabnahmen
und Umweltschaden einschließlich Fallschaden während des Spiels. Die Hitsound-Unterdrückung wirkt nicht
auf den Log. Kill-/Todeszähler, Schadenstatistik und Hitsounds können unabhängig
ausgeschaltet bleiben.

Login, Spawn, Pedwechsel, ungültige Messwerte und erneutes Einschalten setzen
die Vergleichswerte neu. Heilung allein erzeugt keine Schadensmeldung. Mehrere
Schüsse zwischen zwei Messungen können als eine gemeinsame Abnahme erscheinen;
aus dieser Summe werden keine einzelnen Schusswerte erfunden. Der Log sendet
keine dieser Meldungen an den Server.

## Automatischer Motorstart

Der vorhandene Schalter unter **General → Gameplay** bleibt bestehen. Der
Motorzustand wird erst bei vollständig erkanntem Fahrersitz und nach der
bestehenden Wartezeit von 1,5 Sekunden bewertet. Ein kurzzeitig von GTA als
laufend gemeldeter Motor beim Einsteigen verwirft den geplanten Start nicht
mehr, wenn der Server ihn danach ausschaltet.

Ein alter Eintrag „Motor Startet“ im Chatverlauf zählt nicht als neue
Startbestätigung. Die begrenzte Wiederholungsphase beginnt erst mit dem
tatsächlichen Tastendruck; länger geöffneter Chat oder Binder lassen den
geplanten Start nicht vorher ablaufen. Offene Chat-, Dialog- oder
Binder-Eingaben erhalten dabei keine simulierte Motor-Taste.
Die automatisierten Prüfungen ersetzen keinen Fahrversuch auf dem Server.

## Stadt und Bezirk

Es gibt je Wert eine angebotene Schreibweise:

| Variable | Bedeutung | Beispiel |
| --- | --- | --- |
| `{city}` | Stadt beziehungsweise übergeordnetes Gebiet | Los Santos |
| `{bezirk}` | Aktueller Bezirk | Market |

Die Ortswerte stammen aus den lokalen GTA-Zonen. Großgeschriebene Doppelungen
werden in der Hilfe nicht mehr angeboten. Auch die unzuverlässigen
Ereignisvariablen `{victim}`, `{killer}` und `{weapon}` entfallen dort und aus
den Standardvorlagen. Bestehende Vorlagen mit alten Schreibweisen bleiben
lesbar; neue Texte sollten die bereinigte Liste verwenden.

Im Killtext bleiben außerdem `{hp}`, `{weste}`, `{total_kills}`,
`{total_deaths}`, `{session_kills}`, `{session_deaths}` und `{kd}` verfügbar.
Fahrzeugwerte bleiben bei der Textbelegung. Die geschweiften Klammern gehören
zur Variable.

## Kill simulieren

Unter **Einstellungen → General → Kill- & Todeszähler** sendet
**Kill simulieren** den gerade eingetragenen Killtext mit den aktuellen
Variablenwerten. Der Knopf erzeugt keinen echten Kill und verändert keine
Kill-, Todes- oder Sitzungszähler. Er ist auch zum Testen bei ausgeschalteter
automatischer Killnachricht vorgesehen.

Der Chatkanal bleibt am Anfang erhalten:

```text
Eingabe: Kill in {city}, {bezirk}. Kills: {total_kills}
Ausgabe: [Testkill] Kill in Los Santos, Market. Kills: 64

Eingabe: /f Kill in {bezirk}. Kills: {total_kills}
Ausgabe: /f [Testkill] Kill in Market. Kills: 64
```

Dasselbe Prinzip gilt für `/r`, `/d` und andere Slash-Befehle. Der erste
Befehlstoken bleibt vorne; der Binder ergänzt danach die Testmarkierung.
Ein leerer oder mehrzeiliger Text wird nicht gesendet. Die maximale Chatlänge
wird eingehalten. Der Testknopf sendet tatsächlich in den gewählten Chatkanal;
die Meldung kann deshalb für die jeweiligen Empfänger sichtbar sein.

## Einmalige Synchronisation nach dem Einloggen

Sobald der Client verbunden, die eigene Spielfigur bereit und keine Chat-,
Dialog- oder Cursor-Eingabe offen ist, fordert der Binder einmal `/stats` an.
Eine zusätzliche feste Wartepause oder ein blockierendes Sleep wird nicht
eingefügt. Die Dauer hängt trotzdem von der Serverantwort ab.

Der Binder übernimmt nur den erkannten eigenen Statistikblock. Die Zeilen der
automatisch angeforderten Nova-Chatstatistik werden beim Anzeigen unterdrückt,
aber weiter ausgewertet. Eine als eigene Statistikantwort identifizierte
Messagebox wird nach dem Lesen geschlossen. Andere Dialoge und normale
Chatnachrichten werden nicht geschlossen oder pauschal ausgeblendet.

Gespeichert werden die tatsächlich gelieferten Werte: Kills, Tode, Bargeld,
Kontostand, gegebenenfalls Gesamtvermögen, Level, Erfahrungspunkte und nächstes
Levelziel, Gold, Eventcoins, Wantedpunkte und verbleibende Zeit bis zum
Zahltag. Liefert der Server keinen gesonderten Gesamtvermögenswert, wird
Bargeld plus Kontostand als verfügbare Summe verwendet. Fehlende Zusatzwerte
werden nicht erfunden.

Die gelesenen Gesamtzähler bilden einen Ausgangsstand und erzeugen keinen
zusätzlichen Kill oder Tod. Der Cache wird lokal in den Binder-Einstellungen
gespeichert. Nach erfolgreicher Übernahme erscheint ausschließlich lokal:

```text
[NL-BINDER] Synchronisation abgeschlossen.
```

Nachfolgende Zeilen desselben Statistikblocks können den Cache bis zur
Abschlusszeile beziehungsweise innerhalb des begrenzten Drei-Sekunden-
Kontexts ergänzen. Ohne vollständige Antwort endet der automatische Versuch
nach acht Sekunden mit einem lokalen Hinweis statt einer Erfolgsmeldung.
Es gibt keine wiederholten automatischen `/stats`-Abfragen in einer Schleife.

## Quellen und Prüfung

Die Implementierungen und zugehörigen Tests stehen im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog):

- Damagelog und Spielanbindung: `src/SAMPBridge.cpp`,
  `include/IncomingDamageDetector.h`, `tests/SAMPBridgeTests.cpp`.
- Ortswerte und Variablen: `include/MessageVariables.h`,
  `src/ActionExecutor.cpp` und die jeweiligen Orts-/Variablentests.
- Testkill: `include/KillMessageTest.h`, `tests/KillMessageTestTests.cpp`.
- Synchronisation: `src/StartupStatsSync.cpp`, `include/SAMPBridge.h`,
  `include/ServerStatsSnapshot.h`, `tests/ServerStatsParserTests.cpp`.
- Sichtbarkeit eigener Statistikzeilen: `src/JobChatAdapter.cpp` und
  `tests/JobChatAdapterTests.cpp`.

Die früheren r3-Unterlagen dokumentieren den damaligen Stand. Für die hier
genannten Änderungen ist der r4-Quellstand mit den zugehörigen finalen
Buildnachweisen maßgeblich.
