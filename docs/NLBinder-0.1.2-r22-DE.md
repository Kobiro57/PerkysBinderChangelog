# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r22

## RCB-Flaggen auf der Quick Map

`/claims` schaltet die 28 vom Nutzer vorgegebenen Flaggenorte der
Red County Brotherhood auf der vorhandenen Quick Map ein. Ein erneuter Aufruf
blendet sie wieder aus. Die Quick Map wird weiterhin mit ihrem bisherigen
Hotkey geöffnet und bedient; die Flaggen folgen dem vorhandenen Zoom und
Kartenausschnitt. In beiden Radar-Designs wird das vorhandene rote
`radar_enemyAttack`-Flaggensymbol verwendet.

Die gespeicherten Werte X und Y bestimmen den Kartenort. Z bleibt für die
Nachvollziehbarkeit in `include/ClaimSpots.h` erhalten. Ped-Modell, Blickrichtung
und Waffenangaben der ursprünglichen AddPlayerClass-Zeilen werden dafür nicht
benötigt. Es werden keine Spielerklassen oder Weltobjekte erzeugt.

Zum Einschalten muss die eigene Fraktion als Red County Brotherhood erkannt
sein. Für andere oder nicht bestätigte Mitglieder erscheint ausschließlich lokal:

```text
Du bist kein Mitglied der Red County Brotherhood.
```

Ohne Einschalten gibt es keine Claims-Markierungen. Rollenverlust oder
Verbindungsende entziehen die Freigabe; ein erneutes Einschalten ist dann nur
mit bestätigter RCB-Mitgliedschaft möglich. Die bestehende Karte, Servermarker,
Spieleranzeige und Navigation bleiben bedienbar.

## /speed nur als Polizist

Der automatische Wanted-Dialogablauf ist auf die eigene bestätigte
SAPD-/LSPD-Mitgliedschaft begrenzt. Bei anderen oder nicht bekannten Fraktionen
kommt lokal:

```text
Du bist kein Polizist.
```

Dabei wird weder `/su` gesendet noch ein Wanted-Dialog beantwortet. Der
bisherige `/speed ID`- beziehungsweise eindeutige Spielernamen-Ablauf bleibt
für berechtigte Polizisten erhalten. Bei Verlust der Berechtigung darf ein
bereits laufender Ablauf keine weiteren Wanted-Antworten absenden.

Die Fraktion benötigt die synchronisierte eigene Statistik der aktuellen Verbindung.
Ein frischer Online-Eintrag desselben Namens allein erteilt keine Freigabe.
Zusätzlich werden vorhandene frische Online-Fraktionsinformationen für den
eigenen Spielernamen zum Abgleich verwendet. Es gibt
keine manuelle Fraktionsfreigabe über ein frei editierbares Notizfeld und keine
Berechtigung aus dem gewählten Skin.

Wenn bekannte eigene Statistik und frische Online-Zuordnung einander widersprechen,
wird keine Fraktionsfreigabe erteilt. Fehlen die Online-Daten, kann die gültige
eigene Statistik verwendet werden. Nach einem Fraktionswechsel lässt sich der
aktuelle Spielstand über den vorhandenen Synchronisationsbutton neu einlesen.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r22` enthält `NLBinder.asi`, den
vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`, den
postbaren `FORUM-CHANGELOG.txt` und die bestehenden Unterlagen einschließlich
`FORUM-INSTALLATION-QUELLCODE`. Vorhandene Desktop-Ordner werden erhalten.
Der Spielordner wird durch diesen Export nicht geändert. Zum Übernehmen GTA
vollständig beenden, die ASI ersetzen und das Spiel neu starten.

Version und Credits bleiben v.0.1.2 by perkys & edited by KOBiRO. Die feste
Browser-Runtime cef156-r1 bleibt erhalten; für die Claims-Funktion wird kein
neues Browserpaket eingerichtet.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r22/docs/review/r22-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r22/docs/review/r22-validation.json)
