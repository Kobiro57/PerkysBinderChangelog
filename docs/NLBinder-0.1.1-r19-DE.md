# NLBinder v.0.1.1 by KOBiRO – r19

## Use Cooldown & Nachwirkung

Unter Online → Overlay-Einstellungen → Dauerhaft → Spielinformationen steht
direkt unter „Ausdauer anzeigen“ der Schalter „Use Cooldown & Nachwirkung anzeigen“.
Die Anzeige ist zunächst ausgeschaltet. Aktivieren startet die Erkennung für
nachfolgende eigene Einnahmen. Deaktivieren entfernt laufende Timer und ausstehende
Bereit-Meldungen.

Der Binder erkennt die vollständige Aktionszeile „Name nimmt Droge zu sich.“ und
vergleicht den Namen mit dem aktuellen lokalen Spielernamen. Fremde Einnahmen,
zitierte Nachrichten und normaler Spielerchat starten keine eigenen Timer.
Farbcodes werden bereinigt. Der Name ist nicht auf einen bestimmten Nutzer festgelegt.

| Droge | Cooldown | Abklingzeit |
| --- | ---: | ---: |
| Kokain | 120 s | 120 s |
| Heroin | 120 s | 60 s |
| Speed | 180 s | 180 s |
| Ecstasy | 180 s | 60 s |
| Cannabis | keine | 40 s |
| Adrenalin | 45 s | keine |
| Morphium | 45 s | 45 s |

„Ectasy“ wird ebenfalls als Ecstasy erkannt. Cooldown und Abklingzeit beginnen
gleichzeitig bei der Einnahme. Die verbleibenden Zeiten werden in Sekunden
aufgerundet und nutzen eine monotone Uhr; schwankende Bildrate verlängert sie nicht.
Mehrere Drogen bleiben unabhängig voneinander im selben Overlay sichtbar.

Beispiel für Kokain, mit untereinander stehenden Zeilen:

```text
Kokain
Cooldown: 120 s
Abklingzeit: 120 s
```

Bei Cannabis entfällt die Cooldown-Zeile, bei Adrenalin die Abklingzeit-Zeile.
Ein tatsächlich vorhandener Cooldown erzeugt beim Ablauf genau eine lokale
NL-BINDER-Meldung mit der wieder konsumierbaren Droge. Sie wird nicht an den
Serverchat geschickt. Cannabis erzeugt keine Cooldown-Bereit-Meldung.

Ohne aktive Einnahme bleibt das Overlay unsichtbar. Beim vorhandenen
„Move-Modus starten“ erscheint stattdessen eine Vorschau mit „Cooldown: 0 s“
und „Abklingzeit: 0 s“. Mit der Maus verschieben, mit ESC die Position speichern.
Danach verschwindet die Vorschau bis zur nächsten Einnahme. Hintergrund, Schrift,
Skalierung und Farben verwenden denselben Stil wie die übrigen dauerhaften Overlays.
Die Position ist unabhängig von Ausdauer und anderen Anzeigen gespeichert.

Bei Verbindungsende oder Spielerwechsel werden die laufenden Einnahmen verworfen.
Nach einem Relog erfindet der Binder keine bereits vergangenen Konsumereignisse.

## Installation

GTA vollständig schließen, NLBinder.asi ersetzen und das Spiel neu starten.
Browserdateien werden weiterhin bei Bedarf automatisch unter Dokumente/GTA San
Andreas User Files/NLBinder/Browser angelegt. Kein Browserordner muss mitgesendet werden.
Der bisherige Quellcode, alle früheren Änderungen und vollständiger TXT-Changelog
sind enthalten. Im Desktop-Export wird der bisherige r18-Ordner als Sicherung erhalten.

## Prüfung

[Aktueller Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-PRUEFUNG-DE.md),
[maschineller Nachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-validation.json),
[gesamter Changelog](CHANGELOG-GESAMT.txt).
Die automatisierten Prüfungen belegen die getesteten Abläufe. Eine noch nicht
erfolgte Sichtprüfung dieser Erweiterung im laufenden GTA wird nicht behauptet.
