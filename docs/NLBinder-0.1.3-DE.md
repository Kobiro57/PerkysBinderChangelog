# NLBinder v.0.1.3 by perkys & edited by KOBiRO

Die Startmeldung und die Versionsanzeige verwenden jetzt Version 0.1.3.
Die bisherigen Credits bleiben `perkys & edited by KOBiRO`.

Mit `/sprays` öffnen bestätigte Nicht-Zivilisten die Spray-Navigation.
Mitglieder von Fraktionen und Gangs haben Zugriff; Zivilisten sind ausgeschlossen.
Die zunächst hinterlegten 100 Graffitipunkte lassen sich einzeln entfernen.
Eigene Punkte können mit einem Namen an der aktuellen Spielerposition ergänzt
werden. Die bearbeitete Liste wird lokal unter
`Dokumente\GTA San Andreas User Files\NLBinder\sprays.xml` gespeichert und beim nächsten Start geladen.
Diese Datei enthält nur die eigenen Navigationspunkte und gehört nicht zum
öffentlichen Quellcodepaket oder zu GitHub.

Die Punkte werden nach ihrer dreidimensionalen Entfernung sortiert; die nächste
Stelle wird grün hervorgehoben. Je Seite werden bis zu 34 Punkte dargestellt.
Die Liste wächst bedarfsgerecht auf höchstens zehn Seiten beziehungsweise
340 Punkte. Beim Auswählen startet die vorhandene rote GPS-Checkpoint-Navigation
mit der bisherigen Ziel-erreicht-Meldung. Temporäre Spray-Ziele belegen keinen
der 20 gespeicherten normalen GPS-Plätze.

Das Dialogfenster ist größer und innerhalb der Bildschirmgröße veränderbar.
Der Inhaltsbereich mit Eingabe und Punktliste kann separat scrollen.
Seitenwahl und Schließen bleiben als feste Bedienelemente auch bei kleinerem
Fenster sichtbar.

`/spraytour` startet eine Tour durch die persönliche Spray-Liste. Nach dem
Erreichen eines Checkpoints wird der nächste noch nicht besuchte Punkt anhand
der aktuellen Entfernung als GPS-Ziel gewählt. `/sprayend` beendet die Tour.
Mit `/claimstour` können ausschließlich Mitglieder der Red County Brotherhood
die 28 hinterlegten Flaggenpunkte auf dieselbe Weise anfahren; `/claimsend`
beendet diese Tour. Die Touren steuern die Navigation. Das Entfernen von Sprays
und die Flaggenaktionen werden weiterhin wie gewohnt im Spiel ausgeführt.

Die automatische Polizeifunk-Ortung bleibt entfernt. Wanted-Overlay,
Gesucht-Labels, aktuelle Spieler-IDs, `/speed`, Quick Map und die übrigen
Binder-Funktionen bleiben erhalten.

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.3` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Quellen, den Gesamt-Changelog und den kurzen Forumtext.
Die ASI bei vollständig beendetem GTA ersetzen. Vorhandene persönliche GPS-
und Spray-Dateien verbleiben unter Dokumente\GTA San Andreas User Files\NLBinder. Die Browser-Runtime
cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.3/docs/review/r30-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.3/docs/review/r30-validation.json)
