# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r26.

Die Polizeifunk-Ortung wird durch die persönliche Erfolgsmeldung
`Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.`
ausgelöst. Der Zielname stammt aus der vorherigen persönlichen Startmeldung.
Der Binder liest die Position des aktuell gesetzten normalen oder Renn-
Checkpoints und meldet dessen Bezirk einmalig im Radio:

```text
/r [ORTUNG] Gesuchter NAME wurde zuletzt in [BEZIRK] geortet.
```

Ein noch nicht verfügbarer Marker wird für eine begrenzte Zeit abgewartet.
Die frühere zusätzliche Mindestlaufzeit und die zwingende Beobachtung eines
einzelnen Checkpoint-RPCs entfallen. Die eigene Spielerposition wird nicht als
Ersatz für den Zielbezirk verwendet. Doppelte Erfolgsmeldungen erzeugen keinen
weiteren Funktext. Rollen- und Sitzungswechsel verwerfen einen offenen Vorgang.

Das rote Wanted-Label folgt jetzt dem tatsächlich gezeichneten normalen
Spielernamen. Es wird auch bei Fahrern und Mitfahrern dargestellt, wenn deren
nativer Nametag sichtbar ist. Verschwindet dieser, verschwindet auch das
zusätzliche Label. Reichweite und Sichtbarkeit folgen dem nativen Nametag;
die bisherige zusätzliche Grenze von fünf Metern entfällt. Der Binder hebt
keine serverseitige Ausblendung auf und zeichnet keine eigenen Fernmarkierungen.

Beide Funktionen bleiben an die bestätigte eigene SAPD-/LSPD-Mitgliedschaft
gebunden. Das Gesucht-Label setzt außerdem die aktivierte Wanted-Liste und
passende frische Wantedzahlen des tatsächlichen Spielers voraus.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r26)
- [NLBinder.asi](../../dist/NLBinder-0.1.2-r26/NLBinder.asi)
- [Bedienung](../../docs/NLBinder-0.1.2-r26-DE.md)
- [Ortungsablauf](../../src/PoliceLocateBridge.cpp)
- [Wanted-Labels](../../src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](../../docs/review/CHANGELOG-DE.md), [TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Forumtext](../../docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](../../docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](../../docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](../../dist/NLBinder-0.1.2-r26/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](../../docs/review/r26-PRUEFUNG-DE.md), [Ergebnisse](../../docs/review/r26-validation.json)
- [Änderungen seit r25](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r25...v0.1.2-r26)
- [Originalvergleich](../../docs/review/DATEIVERGLEICH.md)

Alle früheren Binder-Funktionen sind enthalten. Neue Quellen und Changelogs
liegen zusätzlich im Desktop-Ordner NLBinder-v0.1.2-r26. Die Browser-Runtime
bleibt cef156-r1. Der frühere Browsernachweis wird als historisch ausgewiesen.
