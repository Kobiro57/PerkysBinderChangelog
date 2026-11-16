# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r26

## Polizeifunk-Ortung und Gesucht-Label

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

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r26` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r26/docs/review/r26-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r26/docs/review/r26-validation.json)
