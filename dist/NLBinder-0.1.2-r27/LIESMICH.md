# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r27

## Ortung mit Stadt und Bezirk

Die automatische Polizeifunk-Ortung verwendet den zum aktuellen
Detektivvorgang gehörenden Ziel-Checkpoint. Bereits vorher aktive normale oder
Renn-Checkpoints werden nicht allein deshalb als Ziel gewählt, weil sie noch
angezeigt werden. Neu gesetzte beziehungsweise geänderte Marker werden dem
laufenden Vorgang zugeordnet. Eine persönliche Erfolgsmeldung und ein passender
verfügbarer Zielmarker bleiben erforderlich.

Die Ortsangabe wird aus den Koordinaten dieses Zielmarkers bestimmt und enthält
Stadt und Bezirk, zum Beispiel `Los Santos Market`. Die nativen Zonentabellen
werden mit ihrem tatsächlichen 32-Byte-Aufbau gelesen; die bisherige falsche
Strukturgröße des eingebundenen SDK wird für diese Abfrage nicht verwendet.
Stadt und Bezirk werden anhand der Ziel-X/Y-Koordinaten zugeordnet:

```text
/r [ORTUNG] Gesuchter NAME wurde zuletzt in [Los Santos Market] geortet.
```

Die eigene Spielerposition dient nicht als Ersatz. Fehlt eine verlässliche
Zuordnung, wird keine Ortsangabe von einem beliebigen alten Marker gesendet.
Doppelte Erfolgsmeldungen erzeugen keinen weiteren Funktext. Rollen- und
Sitzungswechsel verwerfen einen offenen Vorgang. Die Funktion bleibt auf
bestätigte SAPD-/LSPD-Mitglieder begrenzt.

Die native Sichtbarkeitsbindung der Wanted-Labels aus r26 bleibt erhalten,
einschließlich Fahrern und Mitfahrern. Die vorhandene TakeOver-Killerkennung,
Forum-Sonderzeicheneingabe und Drogenzeiten bleiben erhalten. Morphium behält
45 Sekunden Cooldown und 45 Sekunden Nachwirkung.

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r27` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r27/docs/review/r27-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r27/docs/review/r27-validation.json)
