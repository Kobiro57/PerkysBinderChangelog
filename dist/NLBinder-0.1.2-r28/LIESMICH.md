# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r28

## Automatische Ortung entfernt

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

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r28` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r28/docs/review/r28-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r28/docs/review/r28-validation.json)
