# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r28.

Die vom Binder ergänzte automatische Polizeifunk-Ortung ist vollständig
entfernt. Der Binder verarbeitet die Detektiv-Start- und Erfolgsmeldungen nicht
mehr für eine Ortung, wertet dazu keine Ziel-Checkpoints aus und sendet keine
automatische `[ORTUNG]`-Meldung im Radio. Die ausschließlich dafür verwendeten
Quellen, RPC-Beobachtung, Zustände, Hilfsfunktionen und Tests sind entfernt.

Die normale serverseitige Ortung kann weiterhin manuell benutzt werden.
Wanted-Overlay, Wanted-Labels einschließlich Fahrern und Mitfahrern, aktuelle
Spieler-IDs in der Wantedtabelle und `/speed` bleiben erhalten. Die normale
GPS-Navigation, Quick Map, Stadt-/Bezirksvariablen sowie alle übrigen Funktionen
werden von dieser Entfernung nicht verändert.

Frühere Beschreibungen der automatischen Ortung bleiben im vollständigen
Changelog und in alten Veröffentlichungen als Entwicklungshistorie erhalten;
für den aktuellen Stand gilt deren Entfernung in r28.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r28)
- [NLBinder.asi](../../dist/NLBinder-0.1.2-r28/NLBinder.asi)
- [Bedienung](../../docs/NLBinder-0.1.2-r28-DE.md)
- [Wanted-Labels](../../src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](../../docs/review/CHANGELOG-DE.md), [TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Forumtext](../../docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](../../docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](../../docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](../../dist/NLBinder-0.1.2-r28/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](../../docs/review/r28-PRUEFUNG-DE.md), [Ergebnisse](../../docs/review/r28-validation.json)
- [Änderungen seit r27](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r27...v0.1.2-r28)
- [Originalvergleich](../../docs/review/DATEIVERGLEICH.md)

Der vollständige aktuelle Binder-Quellstand und die Changelogs liegen auch im
Desktop-Ordner NLBinder-v0.1.2-r28. Die Browser-Runtime bleibt cef156-r1.
