"""Document the detective radio repair and native wanted-name visibility."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.2 by perkys & edited by KOBiRO'
DETAIL='''Die Polizeifunk-Ortung wird durch die persönliche Erfolgsmeldung
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
'''
POST='Neu in r26: Die automatische Polizeifunk-Ortungsmeldung wurde überarbeitet. Sobald die persönliche Detektiv-Erfolgsmeldung erscheint, liest der Binder den aktuell gesetzten normalen oder Renn-Checkpoint und meldet einmalig den Bezirk dieses Markers mit /r [ORTUNG] Gesuchter NAME wurde zuletzt in [BEZIRK] geortet. Ein noch nicht verfügbarer Marker wird kurz abgewartet; die bisherige zusätzliche Mindestlaufzeit und zwingende Checkpoint-RPC-Erkennung entfallen. Das rote Gesucht-Label folgt jetzt dem tatsächlich sichtbaren normalen Spielernamen, einschließlich Fahrern und Mitfahrern. Die zusätzliche Fünf-Meter-Grenze entfällt; Reichweite und Sichtbarkeit richten sich nach dem nativen Nametag. Verschwindet der normale Name, verschwindet auch das Zusatzlabel. Beide Funktionen bleiben auf bestätigte SAPD-/LSPD-Mitglieder begrenzt.'
HISTORY='''
## 32. Nachtrag v0.1.2 / r26 – 16. November 2026

- Automatische Polizeifunk-Ortung an die persönliche Detektiv-Erfolgsmeldung gebunden und den Zielnamen aus der Startmeldung beibehalten.
- Tatsächlich gesetzten normalen oder Renn-Checkpoint ausgelesen; kurze begrenzte Wartezeit für später verfügbare Marker vorgesehen.
- Frühere zusätzliche Mindestlaufzeit und zwingende Beobachtung von RPC 107 entfernt. Zielbezirk anhand der Markerposition bestimmt, nicht anhand der Position des Polizisten.
- Einmalige Ausgabe, Sitzungs-/Fraktionsprüfung und Verwerfen unvollständiger beziehungsweise abgelaufener Vorgänge beibehalten.
- Gesucht-Label an den tatsächlich gezeichneten nativen Spielernamen gekoppelt, einschließlich Fahrern und Mitfahrern.
- Zusätzliche Fünf-Meter-Grenze und eigene Fahrzeug-Sichtblockade entfernt. Reichweite und Sichtbarkeit folgen der nativen Namensanzeige; keine serverseitige Ausblendung aufgehoben.
- Regressionen für Erfolgsmeldung, aktuelle Checkpoints, versetzte Verfügbarkeit, doppelte Meldungen und native Label-Sichtbarkeit ergänzt. Tatsächliche Ergebnisse stehen im r26-Prüfnachweis.
- Vorhandene TakeOver-Killerkennung, Forum-Sonderzeichenkorrektur und Drogenzeiten erhalten. Browsernachweis aus r24 ausdrücklich als historisch übernommen.
- Neue NLBinder.asi, vollständige Quellen und Changelogs als NLBinder-v0.1.2-r26 vorbereitet. Frühere Exporte und Git-Historie erhalten.
'''

def main():
    notes='# '+TITLE+' – r26\n\n## Polizeifunk-Ortung und Gesucht-Label\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r26` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r26/docs/review/r26-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r26/docs/review/r26-validation.json)
'''
    review='''# Prüfumfang r26 – 16. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die erweiterten
PoliceLocateTrackerTests und PoliceLocateBridgeTests prüfen die persönliche
Erfolgsmeldung, Zielnamen, aktuelle normale und Renn-Checkpoints, später
verfügbare Marker, doppelte Meldungen und Rollen-/Sitzungswechsel. Die Ausgabe
muss den Bezirk des Markers verwenden; die eigene Position ist kein Ersatz.

NearbyWantedLabelsTests prüfen die native Sichtbarkeitsbindung. Die zusätzliche
alte Fünf-Meter-Grenze wird nicht mehr verlangt. Der Produktionspfad verwendet
die tatsächliche native Namenszeichnung und damit deren Sichtbarkeit und
Reichweite, einschließlich Spielern im Fahrzeug. Die freigegebene serverseitige
Namensanzeige wird nicht verändert. Ein sichtbares Zusatzlabel ohne nativen
Nametag ist nicht vorgesehen. Die übrigen Binder-Regressionssuiten gelten weiter.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Browserquellen, Forum-Tastaturfix, Drogenzeiten und gepinnte Runtime
werden gegen r25 abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte
historische Browsernachweis aus r24; er wird nicht als neuer r26-Test ausgegeben.

Ein r26-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r26-validation.json. Der exportierte Quellcode
und die ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**, r26.\n\n'+DETAIL+'''
- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r26)
- [NLBinder.asi](dist/NLBinder-0.1.2-r26/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r26-DE.md)
- [Ortungsablauf](src/PoliceLocateBridge.cpp)
- [Wanted-Labels](src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](dist/NLBinder-0.1.2-r26/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r26-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r26-validation.json)
- [Änderungen seit r25](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r25...v0.1.2-r26)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Alle früheren Binder-Funktionen sind enthalten. Neue Quellen und Changelogs
liegen zusätzlich im Desktop-Ordner NLBinder-v0.1.2-r26. Die Browser-Runtime
bleibt cef156-r1. Der frühere Browsernachweis wird als historisch ausgewiesen.
'''
    for name,data in [('docs/NLBinder-0.1.2-r26-DE.md',notes),('docs/review/r26-PRUEFUNG-DE.md',review),('build/README-GITHUB-r26.md',readme)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md'; s=p.read_text(encoding='utf-8-sig')
    if '## 32. Nachtrag v0.1.2 / r26' not in s:p.write_text(s.rstrip()+'\n'+HISTORY,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md';s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r26' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r25','## Vorheriger Stand v0.1.2 / r25',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.2 / r26 – 16. November 2026\n\nOrtung und native Wanted-Labels: [Prüfumfang](r26-PRUEFUNG-DE.md), [Ergebnisse](r26-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/FORUM-CHANGELOG-v0.1.2.txt';s=p.read_text(encoding='utf-8-sig')
    if 'Neu in r26:' not in s:
        s=s.replace('NEU IN DIESEM PAKET\n','NEU IN DIESEM PAKET\n\n'+POST+'\n',1).replace('/releases/tag/v0.1.2-r25','/releases/tag/v0.1.2-r26')
        p.write_text(s,encoding='utf-8-sig',newline='\n')
    p=R/'docs/BUILD-README.md';s=p.read_text(encoding='utf-8-sig')
    if '**Police Locate and Native Wanted Labels**' not in s:
        s=s.replace('## Building','- **Police Locate and Native Wanted Labels** — Detective success messages trigger current normal/racing-checkpoint lookup and one radio report. Wanted labels follow actual native nametag draw calls, including vehicle occupants and native visibility/range.\n\n## Building',1)
        p.write_text(s,encoding='utf-8',newline='\n')
    print('Documented current-checkpoint police radio and native wanted labels; full history section 32')

if __name__=='__main__':main()
