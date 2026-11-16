# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r23

## Wanted-Label in direkter Nähe

Ist die vorhandene Wanted-Liste unter Online → Overlay-Einstellungen aktiviert
und die eigene SAPD-/LSPD-Mitgliedschaft bestätigt, erhalten unmittelbar
benachbarte gesuchte Spieler ein rotes Label über dem Kopf:

```text
[GESUCHT | 15 Wanteds]
```

Die feste maximale Entfernung beträgt **5 Meter in drei Dimensionen**. Das
Label wird nur für tatsächlich gestreamte, lebende und sichtbare Spieler vor
der Kamera gezeichnet. Eine freie Sichtlinie und die vorhandene serverseitige
Nametag-Freigabe sind erforderlich. Durch Wände, hinter der Kamera, in fremden
Innenräumen und außerhalb dieser unmittelbaren Nähe wird kein Label angezeigt.
Es gibt weder Entfernungswerte noch Richtungs-/Bildschirmrandmarkierungen.

Wantedzahlen stammen aus den vorhandenen frischen Online-Daten und müssen zum
Namen des tatsächlich gestreamten Spielers passen. Alte/offline Daten, eine
abweichende Identität und der eigene Spieler ergeben kein Label. Ohne
aktivierte Wanted-Liste oder bestätigte Polizeimitgliedschaft bleibt diese
zusätzliche Anzeige aus.

Die vorhandene zweidimensionale Wanted-Tabelle zeigt zusätzlich die aktuelle
SA-MP-Spieler-ID in Klammern hinter dem Namen, etwa `Lesch (24)`.
Die Wantedzahl bleibt unverändert. Die ID wird anhand des aktuellen
Spielerpools aufgelöst; eine Account-ID der Online-API wird nicht verwendet.
Ist ein Name gerade nicht eindeutig zuordenbar, bleibt er ohne erfundene ID.

## Ortung automatisch im Funk melden

Die vorhandene Detektiv-Ortung wird anhand der Servermeldungen verfolgt:

```text
Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.
Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.
```

Erst wenn die erfolgreiche Ortung **und ein frisch vom Server gesetzter
Checkpoint** zu diesem Ablauf vorliegen, wird einmalig folgende Funkmeldung
gesendet:

```text
/r [ORTUNG] Gesuchter Lesch wurde zuletzt in [Bezirk des Ortungsmarkers] geortet.
```

Spielername und Bezirk sind dynamisch. Der Bezirk wird aus den tatsächlichen
X-/Y-/Z-Koordinaten des neuen Server-Checkpoints bestimmt, nicht aus der
Position des Polizisten. Die Erfolgsmeldung allein und ein schon vorher
vorhandener Marker lösen keine Meldung aus. Die Reihenfolge von neuem Marker
und Erfolgsmeldung darf wechseln; beide müssen zum laufenden Ortungsvorgang
gehören. Abgebrochene oder veraltete Vorgänge und doppelte Meldungen senden
keinen zusätzlichen Funktext. Die eigene GPS-Navigation setzt diesen Ablauf
nicht in Gang.

Auch diese Funktion steht ausschließlich bestätigten SAPD-/LSPD-Mitgliedern
zur Verfügung. Rollenverlust, Spielerwechsel und Verbindungsende verwerfen
einen laufenden Vorgang. Die Prüfung verwendet die eigene synchronisierte
Fraktion der aktuellen Verbindung und den vorhandenen frischen Online-Abgleich.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r23` enthält `NLBinder.asi`, den
vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`, den
postbaren `FORUM-CHANGELOG.txt` und den vorhandenen separaten
`FORUM-INSTALLATION-QUELLCODE`. Frühere Desktop-Ordner werden erhalten.
Zum Übernehmen GTA vollständig beenden, die ASI ersetzen und neu starten.

Version/Credits bleiben v.0.1.2 by perkys & edited by KOBiRO. Die automatische
Browser-Einrichtung und die feste Runtime cef156-r1 bleiben erhalten.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r23/docs/review/r23-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r23/docs/review/r23-validation.json)
