# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r25

## TakeOver-Killmeldung

Die Servermeldung `[TakeOver]: NAME hat GEGNER getötet.` wird jetzt als
eigener Kill erkannt, wenn NAME dem aktuellen eigenen Ingame-Namen entspricht.
Der vollständige Name wird verglichen; der eigene Name nur als Teil eines
fremden Namens oder als Opfer reicht nicht aus. Der Gegner wird dynamisch aus
der Meldung übernommen. Farben und Zeitstempel werden wie bisher verarbeitet.

Der Kill läuft durch die vorhandene Killerkennung: Killzähler und eingestellter
Killtext funktionieren wie bei den bisherigen Meldungen. Der gewählte Chatkanal
bleibt erhalten, etwa normaler Chat, /r, /d oder /f. Die vorhandenen Schalter
für Zähler und lokale beziehungsweise serverseitige Killmeldungen gelten weiter.
Mehrere Bestätigungen desselben Kills werden über die vorhandene Zuordnung
zusammengeführt. Andere Opfer bleiben eigenständige Kills. Zitierter Spielerchat
und Meldungen über fremde Täter lösen keinen eigenen Killbind aus.

## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r25` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r25/docs/review/r25-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r25/docs/review/r25-validation.json)
