"""Document the TakeOver kill confirmation and preserve all earlier history."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.2 by perkys & edited by KOBiRO'
DETAIL='''Die Servermeldung `[TakeOver]: NAME hat GEGNER getötet.` wird jetzt als
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
'''
POST='Neu in r25: Die Meldung [TakeOver]: NAME hat GEGNER getötet. wird jetzt für eigene Kills erkannt. NAME wird mit dem aktuellen eigenen Spielernamen verglichen. Dadurch werden wie gewohnt der Killzähler aktualisiert und der eingestellte Killtext im gewählten Chatkanal gesendet. Farben und Zeitstempel werden berücksichtigt; fremde Täter oder zitierte Nachrichten lösen keinen eigenen Killbind aus. Die vorhandene Zuordnung verhindert eine zweite Wertung desselben Kills durch zusätzliche Bestätigungen.'
HISTORY='''
## 31. Nachtrag v0.1.2 / r25 – 16. November 2026

- TakeOver-Servermeldung `[TakeOver]: NAME hat GEGNER getötet.` in die vorhandene Killerkennung aufgenommen.
- Aktuellen vollständigen eigenen Namen als Täter geprüft und das Opfer dynamisch übernommen; kein fest eingetragener Spielername.
- Eigenen Killzähler und konfigurierten Killtext einschließlich gewähltem Chatkanal wie bei bisherigen Killmeldungen verwendet.
- Farben, Zeitstempel und vorhandene Zeichenkodierungsbehandlung berücksichtigt. Fremde Täter, eigene Opferrolle und zitierter Spielerchat erzeugen keinen eigenen Kill.
- Regressionen für Parser, vollständige Chat-Anbindung, lokale Meldung, normale/radio/dienst/fraktionsbezogene Chattexte und Zusammenführung mit nativen sowie anderen Serverbestätigungen ergänzt.
- Vorhandene Forum-Sonderzeichenkorrektur und Drogenzeiten erhalten. Historischen r24-Browsernachweis getrennt übernommen; kein erneuter r25-Browser- oder GTA-Livetest behauptet.
- Neue NLBinder.asi, vollständigen Quellcode, gesamten Changelog und Forumtext als NLBinder-v0.1.2-r25 bereitgestellt; bisherige Exporte erhalten.
'''

def main():
    notes='# '+TITLE+' – r25\n\n## TakeOver-Killmeldung\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r25` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r25/docs/review/r25-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r25/docs/review/r25-validation.json)
'''
    review='''# Prüfumfang r25 – 16. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die erweiterten
CombatChatParserTests und CombatChatBridgeTests prüfen die TakeOver-Meldung mit
dynamischen Namen, Farben, Zeitstempeln, fremden Tätern, Spielerpräfixen,
Chatkanälen und Mehrfachbestätigungen. Die übrigen Binder-Prüfungen gelten weiter.

Der vorhandene Killzähler-/Killbind-Ablauf wird weiterverwendet; die neue
Nachrichtenfamilie hat einen eigenen Wert und nutzt die vorhandene Zuordnung.
Die ASI wird als x86-PE geprüft, einschließlich ihrer eingebetteten Helfer und
der erhaltenen Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Die Browserquellen, der Sonderzeichenfix und die gepinnte Runtime werden
gegen r24 abgeglichen. FORUM-KEYBOARD-TEST.json ist der unveränderte historische
Browsernachweis aus r24; dieser Test wird für r25 nicht erneut behauptet.

Ein r25-GTA-Livetest wurde nicht durchgeführt. Die tatsächlichen automatischen
Ergebnisse stehen in r25-validation.json. Der exportierte Quellcode und die ASI
werden mit dem veröffentlichten Commit abgeglichen. Private Profile, Logs,
Spielstände und Zugangsdaten gehören nicht zum Paket.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**, r25.\n\n'+DETAIL+'''
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
'''
    for name,data in [('docs/NLBinder-0.1.2-r25-DE.md',notes),('docs/review/r25-PRUEFUNG-DE.md',review),('build/README-GITHUB-r25.md',readme)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md'; s=p.read_text(encoding='utf-8-sig')
    if '## 31. Nachtrag v0.1.2 / r25' not in s: p.write_text(s.rstrip()+'\n'+HISTORY,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md'; s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r25' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r24','## Vorheriger Stand v0.1.2 / r24',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.2 / r25 – 16. November 2026\n\nTakeOver-Killmeldung: [Prüfumfang](r25-PRUEFUNG-DE.md), [Ergebnisse](r25-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/FORUM-CHANGELOG-v0.1.2.txt'; s=p.read_text(encoding='utf-8-sig')
    if 'Neu in r25:' not in s:
        s=s.replace('NEU IN DIESEM PAKET\n','NEU IN DIESEM PAKET\n\n'+POST+'\n',1).replace('/releases/tag/v0.1.2-r24','/releases/tag/v0.1.2-r25')
        p.write_text(s,encoding='utf-8-sig',newline='\n')
    p=R/'docs/BUILD-README.md'; s=p.read_text(encoding='utf-8-sig')
    if '**TakeOver Kills**' not in s:
        p.write_text(s.replace('## Building','- **TakeOver Kills** — Own `[TakeOver]: NAME hat ENEMY getötet.` confirmations use the existing kill counter and configured killbind, with whole-name attribution and existing duplicate correlation.\n\n## Building',1),encoding='utf-8',newline='\n')
    print('Documented TakeOver kill confirmation and full history section 31')

if __name__=='__main__': main()
