# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r25.

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

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r25)
- [NLBinder.asi](dist/NLBinder-0.1.2-r25/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r25-DE.md)
- [Killmeldungsparser](src/CombatChatParser.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](dist/NLBinder-0.1.2-r25/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r25-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r25-validation.json)
- [Änderungen seit r24](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r24...v0.1.2-r25)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Alle früheren Binder-Funktionen sind enthalten. Neue Quellen und Changelogs
liegen zusätzlich im Desktop-Ordner NLBinder-v0.1.2-r25. Die Browser-Runtime
bleibt cef156-r1. Der frühere Browsernachweis wird als historisch ausgewiesen.
