# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r29

## Spraytag-Navigation

Mit `/sprays` öffnen bestätigte SAPD-/LSPD-Mitglieder die Navigation zu
100 fest hinterlegten Spraytag-Stellen. Die Liste berechnet die dreidimensionale
Entfernung von der aktuellen Spielerposition zu den Stellen und sortiert sie
aufsteigend. Die nächstgelegene Stelle wird grün hervorgehoben.

Die 100 Einträge verteilen sich auf drei Seiten mit 34, 34 und 32 Stellen.
Bei Auswahl eines Eintrags startet die vorhandene GPS-Navigation zum gewählten
Punkt mit dem roten Checkpoint. Beim Erreichen wird wie bei der bisherigen
GPS-Navigation das Ziel als erreicht gemeldet.

Spray-Ziele sind vorübergehende Navigationsziele. Sie belegen keinen der
20 gespeicherten GPS-Plätze und verändern die gespeicherten Ziele nicht.
Die Koordinaten sind im Quellcode unter `include/SpraySpots.h` nachvollziehbar.
Das System zeigt diese festen Stellen an; es liest keine fremden Spielerorte
oder serverseitigen Spray-Zustände aus.

Die in r28 entfernte automatische Polizeifunk-Ortung bleibt vollständig entfernt.
Wanted-Overlay, native Wanted-Labels, Spieler-IDs, `/speed`, Quick Map,
Browser-Funktionen und die bisherigen übrigen Binder-Funktionen bleiben erhalten.

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r29` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r29/docs/review/r29-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r29/docs/review/r29-validation.json)
