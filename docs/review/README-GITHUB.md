# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r29.

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

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r29)
- [NLBinder.asi](../../dist/NLBinder-0.1.2-r29/NLBinder.asi)
- [Bedienung](../../docs/NLBinder-0.1.2-r29-DE.md)
- [Spraytag-Koordinaten](../../include/SpraySpots.h), [Navigation](../../src/SpraysNavigator.cpp)
- [Wanted-Labels](../../src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](../../docs/review/CHANGELOG-DE.md), [TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Forumtext](../../docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](../../docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](../../docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](../../dist/NLBinder-0.1.2-r29/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](../../docs/review/r29-PRUEFUNG-DE.md), [Ergebnisse](../../docs/review/r29-validation.json)
- [Änderungen seit r28](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r28...v0.1.2-r29)
- [Originalvergleich](../../docs/review/DATEIVERGLEICH.md)

Der vollständige aktuelle Binder-Quellstand und die Changelogs liegen auch im
Desktop-Ordner NLBinder-v0.1.2-r29. Die Browser-Runtime bleibt cef156-r1.
