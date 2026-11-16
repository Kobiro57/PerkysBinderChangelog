# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r24

## Sonderzeichen im Quick Forum

Die Tastatureingabe im über `/forum` geöffneten Quick Forum ist für
Sonderzeichen überarbeitet. Auf einer deutschen Tastatur betrifft das
insbesondere Zeichen mit AltGr wie `[ ] { } @ € \ | ~`.
Die Texteingabe wird als Texteingabe an den Browser weitergereicht;
Tastenkombinationen zum Kopieren und Einfügen bleiben erhalten.

Die Eingabe ist auf das geöffnete, fokussierte Forum beschränkt. ESC behält
seine vorhandene Schließen-Funktion. Das Schreiben im Forum darf keine
Binder-Aktion auslösen. Das vorhandene AFK-/Back-Verhalten wird beibehalten.

Der Fix liegt in der ASI und ihrer Tastaturanbindung. Die Browser-Runtime
**cef156-r1** und ihr Download bleiben erhalten; ein erneutes Herunterladen
des Browserpakets ist für diese Korrektur nicht erforderlich. Persönliche
Browserprofile, Anmeldungen und Binder-Einstellungen gehören nicht zum Paket.

## Weiterhin enthalten

- Wanted-Tabelle mit tatsächlicher SA-MP-Spieler-ID hinter dem Namen,
  etwa `Lesch (24)`, und der bisherigen Wantedzahl.
- Rote Wanted-Labels für bestätigte SAPD-/LSPD-Mitglieder bei aktivierter
  Wanted-Liste, maximal fünf Meter entfernt, sichtbar und mit freier Sicht.
- Einmalige Polizeifunkmeldung nach erfolgreicher Detektiv-Ortung und neuem
  Server-Checkpoint mit dem Bezirk des gesetzten Markers.
- Die bisherigen Jobs, Overlays, Drogen-Timer, Zähler, GPS- und Forumfunktionen.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r24` enthält die fertige `NLBinder.asi`,
den vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`,
den postbaren `FORUM-CHANGELOG.txt` und den gesonderten
`FORUM-INSTALLATION-QUELLCODE` mit nachvollziehbaren Quellcode-Prüfsummen.
Frühere Desktop-Ordner werden erhalten.

Zum Übernehmen GTA vollständig schließen, die bisherige Binder-ASI ersetzen
und neu starten. Nur eine Binder-ASI laden. Anschließend `/forum` öffnen und
die gewünschten Zeichen in einem Texteingabefeld verwenden.

Version/Credits bleiben v.0.1.2 by perkys & edited by KOBiRO.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r24/docs/review/r24-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r24/docs/review/r24-validation.json)
