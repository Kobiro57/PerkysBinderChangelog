# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r21

## Drogen-Overlay: tatsächliches Serverformat

Der Server meldet eine Einnahme beispielsweise als:

```text
*** KOBiRO nimmt Heroin zu sich.
```

Das führende `*** ` wurde bislang nicht berücksichtigt. Der Parser entfernt
dieses Präfix jetzt genau einmal am Anfang der bereinigten Zeile. Anschließend
müssen weiterhin der vollständige eigene Spielername und die vollständige
Konsum-Aktionszeile stimmen. Der Name wird dynamisch aus dem Spiel gelesen.
Fremde Spieler, Namensvorsilben und zitierte normale Chatnachrichten starten
keine eigenen Timer. Die bisherige Schreibweise ohne Sterne bleibt unterstützt.

Heroin startet gleichzeitig mit 120 Sekunden Cooldown und 60 Sekunden
Abklingzeit. Die bisherigen Zeiten der übrigen Drogen, die lokale einmalige
Bereit-Meldung, Aktivierung sowie Move-/ESC-Verhalten bleiben erhalten.
Der Schalter liegt unter Online → Overlay-Einstellungen → Dauerhaft →
Spielinformationen → Use Cooldown & Nachwirkung anzeigen.

## Installation und Quellcode

GTA vollständig beenden, NLBinder.asi ersetzen und anschließend neu starten.
Der Desktop-Ordner NLBinder-v0.1.2 enthält die fertige ASI, vollständige Quellen,
CHANGELOG-GESAMT.txt sowie die vorhandenen Forumtexte, Runtime-Dateiliste und
FORUM-INSTALLATION-QUELLCODE. Ein bekannter unveränderter vorheriger r20-Ordner
wird als Sicherung erhalten. Bei unerwarteten Änderungen wird stattdessen ein
neuer r21-Ordner angelegt; persönliche Änderungen werden nicht überschrieben.

Die bestehende Browser-Runtime cef156-r1 bleibt unverändert. Es gibt durch
diesen Parserfix keine neue Browserinstallation oder neue Runtime-Dateien.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r21/docs/review/r21-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r21/docs/review/r21-validation.json)
