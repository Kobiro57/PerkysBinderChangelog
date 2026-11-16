# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r27.

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

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r27)
- [NLBinder.asi](../../dist/NLBinder-0.1.2-r27/NLBinder.asi)
- [Bedienung](../../docs/NLBinder-0.1.2-r27-DE.md)
- [Ortungsablauf](../../src/PoliceLocateBridge.cpp)
- [Stadt-/Bezirksauflösung](../../include/CheckpointLocation.h)
- [Wanted-Labels](../../src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](../../docs/review/CHANGELOG-DE.md), [TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Forumtext](../../docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](../../docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](../../docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](../../dist/NLBinder-0.1.2-r27/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](../../docs/review/r27-PRUEFUNG-DE.md), [Ergebnisse](../../docs/review/r27-validation.json)
- [Änderungen seit r26](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r26...v0.1.2-r27)
- [Originalvergleich](../../docs/review/DATEIVERGLEICH.md)

Alle früheren Binder-Funktionen sind enthalten. Neue Quellen und Changelogs
liegen zusätzlich im Desktop-Ordner NLBinder-v0.1.2-r27. Die Browser-Runtime
bleibt cef156-r1. Der frühere Browsernachweis wird als historisch ausgewiesen.
