# NLBinder v.0.1.1 by KOBiRO

Anpassung des ursprünglichen Perkys Binders. Die vorhandene Oberfläche, Themes,
Bedienelemente und Fraktionsanzeigen werden weiterverwendet. Aus KOBiRO_ASI
wurde das Verhalten der Overlay-Funktionen übernommen.

## Neu

- Jobs: Holzfäller und Bauarbeiter in der vorhandenen Jobübersicht, mit
  Verdienst- und Skill-Erfassung. Verifizierte Holzfäller-Meldungen von Vince
  Redwood werden ebenfalls erkannt. Beträge wie `2.940$` werden korrekt gelesen.
- Online / Overlay: Ort mit Stadt und Zone, Fortbewegungsmittel und Pedslots.
  Pedslots zählt die anderen Spieler, die der eigene Client aktuell geladen hat.
- Spieler-Radar: bis zu 20 Freunde oder Feinde per Name oder aktueller
  Spieler-ID speichern. Anzeige mit Fraktion/Notiz, Online-Status und Live-ID.
  Entfernungen stehen nur für Spieler im eigenen Stream zur Verfügung.
- Online-Watcher mit einstellbarem Intervall, Online-Alarm und optionalem
  Chat-Hinweis sowie Feind-Sound. Die erste Erfassung und Wiederverbindung
  lösen keine nachträglichen Alarme aus.
- Einstellungen / General: Killzähler und Todeszähler getrennt ein-/ausschalten,
  im vorhandenen Schalterstil.
- Eigene automatische Kill-Nachricht mit einem Variablen-Button wie bei den
  Textbelegungen. Die Hilfe steht auch bei den bisherigen Killtracker-Vorlagen.
- Hauptmenü und Startmeldung: `NLBinder v.0.1.1 by KOBiRO`.

## Installation

1. GTA/SA-MP vollständig schließen.
2. Die bisherige Binder-ASI sichern und anschließend durch `Keybinder.asi`
   aus diesem Paket ersetzen. Es soll nur eine Version des Binders geladen werden.
3. SA-MP 0.3.DL-R1 starten. Ein vorhandener ASI-Loader wird weiterhin benötigt.
4. Das Menü mit F2, `/kb` oder `/keybinder` öffnen, sofern die eigenen
   Einstellungen keine andere Taste beziehungsweise andere Befehle vorgeben.

Vorhandene Konfigurationen bleiben unter dem bisherigen NLBinder-Datenordner
kompatibel. Die neuen Overlay-Anzeigen sind zunächst deaktiviert und lassen sich
unter Online / Overlay einschalten. Über den vorhandenen Verschiebemodus können
sie platziert werden. Fraktions-, Gang- und Wanted-Anzeigen bleiben verfügbar.

## Automatische Kill-Nachricht

Unter Einstellungen / General den Killzähler und den Schalter
„Kill-Nachricht an Server senden“ aktivieren. In das Textfeld beispielsweise:

```text
/f Ich habe einen Kill in {zone}. Meine Kills: {total_kills}
```

Nach einem neu erkannten eigenen Kill wird die Vorlage mit dem bereits erhöhten
Killstand gesendet. Ein führender Schrägstrich führt den entsprechenden Befehl
aus, zum Beispiel /f, /d oder /r; sonst wird normaler Chattext gesendet.
Der Kanal ist frei wählbar und nicht auf /f festgelegt. Ein leeres Textfeld
sendet nichts.
Die Erkennung verwendet die vorhandenen SA-MP-Todesmeldungen und den Killfeed.

Verfügbare Variablen für automatische Kill-/Tod-Nachrichten:
`{victim}`, `{killer}`, `{weapon}`, `{zone}`, `{ZONE}`, `{hp}`, `{weste}`,
`{carhp}`, `{carname}`, `{total_kills}`, `{total_deaths}`, `{session_kills}`,
`{session_deaths}`, `{kd}` und `{KD}`. Die Chatlänge bleibt auf 144 Zeichen
begrenzt. Statistikwerte werden beim Deaktivieren eines Zählers beibehalten.

## Prüfstand

Die ASI wird für GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 als
32-Bit-Release gebaut. Sie wurde nicht in die laufende Spielsitzung installiert.
Ein vollständiger Funktionstest im Spiel bleibt erforderlich.

Die geprüften Job-Meldungen und noch fehlenden Belege zu speziellen
Bauarbeiter-Aufgaben beziehungsweise Belohnungsbäumen stehen in
`docs/jobs-support.md`. Verdienst- und Skill-Erfassung sind für beide Jobs
eingepflegt; unbelegte Belohnungen werden nicht als freigeschaltet angezeigt.
