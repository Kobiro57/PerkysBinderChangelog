"""Document removal of the complete Binder-added automatic police locator."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.2 by perkys & edited by KOBiRO'
DETAIL='''Die vom Binder ergänzte automatische Polizeifunk-Ortung ist vollständig
entfernt. Der Binder verarbeitet die Detektiv-Start- und Erfolgsmeldungen nicht
mehr für eine Ortung, wertet dazu keine Ziel-Checkpoints aus und sendet keine
automatische `[ORTUNG]`-Meldung im Radio. Die ausschließlich dafür verwendeten
Quellen, RPC-Beobachtung, Zustände, Hilfsfunktionen und Tests sind entfernt.

Die normale serverseitige Ortung kann weiterhin manuell benutzt werden.
Wanted-Overlay, Wanted-Labels einschließlich Fahrern und Mitfahrern, aktuelle
Spieler-IDs in der Wantedtabelle und `/speed` bleiben erhalten. Die normale
GPS-Navigation, Quick Map, Stadt-/Bezirksvariablen sowie alle übrigen Funktionen
werden von dieser Entfernung nicht verändert.

Frühere Beschreibungen der automatischen Ortung bleiben im vollständigen
Changelog und in alten Veröffentlichungen als Entwicklungshistorie erhalten;
für den aktuellen Stand gilt deren Entfernung in r28.
'''
POST='Neu in r28: Die gesamte vom Binder ergänzte automatische Polizeifunk-Ortung wurde auf Wunsch entfernt. Der Binder liest Detektiv-Meldungen nicht mehr für eine Ortung aus, wertet keine Ortungs-Checkpoints mehr aus und sendet keine automatische Ortungsmeldung im Radio. Die zugehörigen ausschließlich verwendeten Quellen, Hooks, Zustände und Tests wurden ebenfalls entfernt. Wanted-Overlay, Gesucht-Labels, aktuelle Spieler-IDs, /speed, GPS und Quick Map bleiben erhalten. Die serverseitige Ortung kann weiterhin normal manuell benutzt werden.'
HISTORY='''
## 34. Nachtrag v0.1.2 / r28 – 17. November 2026

- Gesamte vom Binder ergänzte automatische Polizeifunk-Ortung auf Wunsch entfernt.
- Mitlesen der Detektiv-Start- und Erfolgsmeldungen für den Ortungsablauf entfernt; keine automatische Ortungsmeldung im Radio mehr.
- Ausschließlich dafür verwendete Checkpoint-RPC-Beobachtung, Zielmarkerauswahl, Stadt-/Bezirksauflösung, Zustände, Quellen und Tests entfernt.
- Normale manuelle serverseitige Ortung, Wanted-Overlay, native Wanted-Labels und Spieler-IDs sowie /speed beibehalten.
- GPS-Navigation, Quick Map, allgemeine Stadt-/Bezirksvariablen und alle übrigen Binder-Funktionen erhalten.
- Gesamttests und Release-Build sowie das Fehlen der Ortungsvorlage in der fertigen ASI werden geprüft. Tatsächliche Ergebnisse stehen im r28-Prüfnachweis.
- Frühere Beschreibungen und Veröffentlichungen einschließlich r27 als Entwicklungshistorie erhalten; aktuelle Dokumentation weist die Entfernung aus.
- Neue NLBinder.asi, vollständige aktuelle Quellen und gesamte Changelogs als NLBinder-v0.1.2-r28 vorbereitet. Frühere Desktop-Exporte und Spielinstallation unverändert.
'''

def update_history_header(text):
    heading='# Änderungen am Perkys Binder – '+TITLE
    status=('Stand: 17. November 2026, **r28 ohne automatische Polizeifunk-Ortung**. '
            'Die Abschnitte 1 bis 33 dokumentieren die früheren Stände; Abschnitt 34 ergänzt r28.')
    text,n=re.subn(r'(?m)^# [^\n]*',lambda _:heading,text,count=1)
    text,m=re.subn(r'(?m)^Stand: [^\n]*',lambda _:status,text,count=1)
    if n!=1 or m!=1:raise ValueError('Expected changelog title and current status')
    return text

def main():
    notes='# '+TITLE+' – r28\n\n## Automatische Ortung entfernt\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r28` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r28/docs/review/r28-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r28/docs/review/r28-validation.json)
'''
    review='''# Prüfumfang r28 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die entfernten
Ortungsquellen und ausschließlich zugehörigen Tests dürfen im aktuellen
Quellstand, Projekt, Testlauf und Desktop-Export nicht mehr enthalten sein.
Die aktive SAMPBridge darf keinen Ortungsablauf mehr initialisieren, aktualisieren
oder mit Detektiv-Meldungen versorgen. In der fertigen ASI darf die eindeutige
Vorlage `[ORTUNG]` nicht vorkommen.

Die bestehenden Tests für Wanted-Overlay und native Gesucht-Labels, /speed,
Spieler-IDs, GPS, Quick Map, Fraktionsprüfung sowie die übrigen Funktionen
bleiben maßgeblich. Die normalen serverseitigen Chatmeldungen dürfen keine
automatische Ortungs-Funkmeldung auslösen. Die frühere Ortungsimplementierung
wird nicht durch inaktive Quellen oder neue Tests für entfernten Code erhalten.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Unveränderte Browser-, GPS-, Wanted- und Kill-Quellen werden gegen r27
abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte historische
Browsernachweis aus r24 und wird nicht als neuer r28-Test ausgegeben.

Ein r28-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r28-validation.json. Exportierte Quellen und
ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket. Historische
Veröffentlichungen und ihre Prüfdateien bleiben erhalten.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**, r28.\n\n'+DETAIL+'''
- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r28)
- [NLBinder.asi](dist/NLBinder-0.1.2-r28/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r28-DE.md)
- [Wanted-Labels](src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](dist/NLBinder-0.1.2-r28/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r28-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r28-validation.json)
- [Änderungen seit r27](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r27...v0.1.2-r28)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der vollständige aktuelle Binder-Quellstand und die Changelogs liegen auch im
Desktop-Ordner NLBinder-v0.1.2-r28. Die Browser-Runtime bleibt cef156-r1.
'''
    for name,data in [('docs/NLBinder-0.1.2-r28-DE.md',notes),('docs/review/r28-PRUEFUNG-DE.md',review),('build/README-GITHUB-r28.md',readme)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md';old=p.read_text(encoding='utf-8-sig');s=update_history_header(old)
    if '## 34. Nachtrag v0.1.2 / r28' not in s:s=s.rstrip()+'\n'+HISTORY
    if s!=old:p.write_text(s,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md';s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r28' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r27','## Vorheriger Stand v0.1.2 / r27',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.2 / r28 – 17. November 2026\n\nAutomatische Ortung vollständig entfernt: [Prüfumfang](r28-PRUEFUNG-DE.md), [Ergebnisse](r28-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/FORUM-CHANGELOG-v0.1.2.txt';s=p.read_text(encoding='utf-8-sig')
    if 'Neu in r28:' not in s:
        marker='NEU IN DIESEM PAKET\n'
        if marker not in s:raise ValueError('Forum changelog update marker missing')
        historical=('FRÜHERE ÄNDERUNGEN – HISTORISCHE BESCHREIBUNGEN\n'
                    'Die folgenden Abschnitte beschreiben frühere Pakete. Die dort genannte automatische Ortung ist seit r28 entfernt.\n')
        s=s.replace(marker,marker+'\n'+POST+'\n\n'+historical,1).replace('/releases/tag/v0.1.2-r27','/releases/tag/v0.1.2-r28')
        p.write_text(s,encoding='utf-8-sig',newline='\n')
    p=R/'docs/BUILD-README.md';old=p.read_text(encoding='utf-8-sig');lines=[]
    for line in old.splitlines():
        lower=line.lower()
        if line.startswith('- **Police Wanted and Location**'):
            line='- **Police Wanted Labels** — Confirmed SAPD/LSPD members with the Wanted overlay enabled see red wanted labels alongside actually drawn native player names, including drivers and passengers. Visibility and range follow the native nametag. The Wanted table includes current SA-MP player IDs when resolved.'
        elif line.startswith('- **') and ('police locate' in lower or 'police location' in lower or 'operation-specific police location' in lower):continue
        lines.append(line)
    updated='\n'.join(lines)+'\n'
    if updated!=old:p.write_text(updated,encoding='utf-8',newline='\n')
    print('Documented complete automatic locator removal; full history section 34')

if __name__=='__main__':main()
