"""Document police spraytag navigation through temporary existing GPS targets."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.2 by perkys & edited by KOBiRO'
DETAIL='''Mit `/sprays` öffnen bestätigte SAPD-/LSPD-Mitglieder die Navigation zu
100 fest hinterlegten Spraytag-Stellen. Die Liste berechnet die dreidimensionale
Entfernung von der aktuellen Spielerposition zu den Stellen und sortiert sie
aufsteigend. Die nächstgelegene Stelle wird grün hervorgehoben.

Die 100 Einträge verteilen sich auf drei Seiten mit 34, 34 und 32 Stellen.
Bei Auswahl eines Eintrags startet die vorhandene GPS-Navigation zum gewählten
Punkt mit dem roten Checkpoint. Beim Erreichen wird wie bei der bisherigen
GPS-Navigation das Ziel als erreicht gemeldet.

Spray-Ziele sind vorübergehende Navigationsziele. Sie belegen keinen der
20 gespeicherten GPS-Plätze und verändern die gespeicherten Ziele nicht.
Die Koordinaten sind im Quellcode unter `include/SpraySpots.h` nachvollziehbar.
Das System zeigt diese festen Stellen an; es liest keine fremden Spielerorte
oder serverseitigen Spray-Zustände aus.

Die in r28 entfernte automatische Polizeifunk-Ortung bleibt vollständig entfernt.
Wanted-Overlay, native Wanted-Labels, Spieler-IDs, `/speed`, Quick Map,
Browser-Funktionen und die bisherigen übrigen Binder-Funktionen bleiben erhalten.
'''
POST='Neu in r29: Mit /sprays öffnen bestätigte SAPD-/LSPD-Mitglieder eine Liste der 100 fest hinterlegten Spraytag-Stellen. Die Stellen werden nach ihrer dreidimensionalen Entfernung zur aktuellen Spielerposition sortiert; die nächstgelegene Stelle ist grün hervorgehoben. Die Liste hat drei Seiten mit 34, 34 und 32 Einträgen. Ein ausgewählter Eintrag startet die vorhandene rote GPS-Checkpoint-Navigation samt Ziel-erreicht-Meldung. Spray-Ziele sind vorübergehend und verbrauchen keinen der 20 gespeicherten GPS-Plätze. Die automatische Polizeifunk-Ortung bleibt entfernt.'
HISTORY='''
## 35. Nachtrag v0.1.2 / r29 – 17. November 2026

- /sprays für bestätigte SAPD-/LSPD-Mitglieder ergänzt; andere Fraktionen erhalten keinen Zugriff auf die Spraytag-Navigation.
- 100 feste Spraytag-Koordinaten im nachvollziehbaren Quellcode hinterlegt.
- Stellen nach dreidimensionaler Entfernung zur aktuellen Spielerposition sortiert und die nächstgelegene Stelle grün hervorgehoben.
- Liste auf drei Seiten mit 34, 34 und 32 Einträgen verteilt; Auswahl startet die vorhandene rote GPS-Checkpoint-Navigation.
- Vorhandene Ziel-erreicht-Erkennung für Spray-Ziele weiterverwendet.
- Vorübergehende GPS-Ziele ergänzt, ohne einen der 20 gespeicherten GPS-Plätze zu belegen oder gespeicherte Ziele zu verändern.
- Regressionen für Spraytag-Auswahl, Sortierung, Fraktionsprüfung und vorübergehende GPS-Ziele ergänzt. Tatsächliche Ergebnisse stehen im r29-Prüfnachweis.
- Automatische Polizeifunk-Ortung aus r28 weiterhin vollständig entfernt; Wanted-Labels, /speed, Quick Map, Browser und übrige Funktionen erhalten.
- Neue NLBinder.asi, vollständige aktuelle Quellen und gesamte Changelogs als NLBinder-v0.1.2-r29 vorbereitet. Frühere Veröffentlichungen und Desktop-Exporte erhalten; Spielinstallation unverändert.
'''

def update_history_header(text):
    heading='# Änderungen am Perkys Binder – '+TITLE
    status=('Stand: 17. November 2026, **r29 mit Spraytag-Navigation**. '
            'Die Abschnitte 1 bis 34 dokumentieren die früheren Stände; Abschnitt 35 ergänzt r29.')
    text,n=re.subn(r'(?m)^# [^\n]*',lambda _:heading,text,count=1)
    text,m=re.subn(r'(?m)^Stand: [^\n]*',lambda _:status,text,count=1)
    if n!=1 or m!=1:raise ValueError('Expected changelog title and current status')
    return text

def main():
    notes='# '+TITLE+' – r29\n\n## Spraytag-Navigation\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r29` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r29/docs/review/r29-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r29/docs/review/r29-validation.json)
'''
    review='''# Prüfumfang r29 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. SpraysNavigatorTests
prüfen die 100 festen Koordinaten, aufsteigende dreidimensionale Entfernung,
grüne Hervorhebung der nächsten Stelle, die Seitenaufteilung 34/34/32 und die
Zuordnung einer Auswahl zur tatsächlich angezeigten Zielposition. Die bestätigte
SAPD-/LSPD-Mitgliedschaft ist Voraussetzung für die Funktion.

GpsTemporaryTests prüfen die vorübergehende Navigation über das vorhandene
GPS-System. Ein Spray-Ziel darf auch bei 20 gespeicherten Zielen funktionieren,
keinen Speicherplatz verbrauchen und gespeicherte Ziele nicht verändern.
Der vorhandene rote Checkpoint und die Ziel-erreicht-Erkennung werden verwendet.
GPS-Spieladapter, Browser, Wanted-Labels und Kill-Erkennung werden gegen r28
abgeglichen. Angepasste GPS-Verwaltung und Main-Integration werden neu geprüft.

Die automatische Polizeifunk-Ortung bleibt entfernt. Frühere Ortungsquellen dürfen
im aktuellen Quellstand, Projekt und Desktop-Export nicht wieder auftauchen.
Die fertige ASI darf die eindeutige Vorlage `[ORTUNG]` nicht enthalten. Die
Negativregression zu Detektiv-Meldungen und Checkpoints ohne automatischen
Funktext bleibt Teil der Gesamttests.

Der PE-Nachweis prüft die x86-ASI, ihre eingebetteten Helfer und die Startmeldung.
Der Reconnect-Helfer wird mit lokalen Testprozessen geprüft. Der unveränderte
historische Browsernachweis aus r24 wird nicht als neuer r29-Test ausgegeben.
Ein r29-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r29-validation.json. Exportierte Quellen und
ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**, r29.\n\n'+DETAIL+'''
- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r29)
- [NLBinder.asi](dist/NLBinder-0.1.2-r29/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r29-DE.md)
- [Spraytag-Koordinaten](include/SpraySpots.h), [Navigation](src/SpraysNavigator.cpp)
- [Wanted-Labels](src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](dist/NLBinder-0.1.2-r29/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r29-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r29-validation.json)
- [Änderungen seit r28](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r28...v0.1.2-r29)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der vollständige aktuelle Binder-Quellstand und die Changelogs liegen auch im
Desktop-Ordner NLBinder-v0.1.2-r29. Die Browser-Runtime bleibt cef156-r1.
'''
    for name,data in [('docs/NLBinder-0.1.2-r29-DE.md',notes),('docs/review/r29-PRUEFUNG-DE.md',review),('build/README-GITHUB-r29.md',readme)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md';old=p.read_text(encoding='utf-8-sig');s=update_history_header(old)
    if '## 35. Nachtrag v0.1.2 / r29' not in s:s=s.rstrip()+'\n'+HISTORY
    if s!=old:p.write_text(s,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md';s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r29' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r28','## Vorheriger Stand v0.1.2 / r28',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.2 / r29 – 17. November 2026\n\nSpraytag-Navigation: [Prüfumfang](r29-PRUEFUNG-DE.md), [Ergebnisse](r29-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/FORUM-CHANGELOG-v0.1.2.txt';s=p.read_text(encoding='utf-8-sig')
    if 'Neu in r29:' not in s:
        marker='NEU IN DIESEM PAKET\n'
        if marker not in s:raise ValueError('Forum changelog update marker missing')
        s=s.replace(marker,marker+'\n'+POST+'\n\nFrüherer Stand r28 (weiterhin gültig: automatische Polizeifunk-Ortung entfernt):\n',1)
        s=s.replace('/releases/tag/v0.1.2-r28','/releases/tag/v0.1.2-r29')
        p.write_text(s,encoding='utf-8-sig',newline='\n')
    p=R/'docs/BUILD-README.md';old=p.read_text(encoding='utf-8-sig')
    if '**Police Spraytag Navigation**' not in old:
        text=('- **Police Spraytag Navigation** — `/sprays` lists 100 fixed spraytag locations for verified SAPD/LSPD members. '
              'Targets are sorted by current 3D distance; the nearest is green. Three pages hold 34/34/32 entries. '
              'Selection uses the existing red GPS checkpoint and arrival message through a temporary target that preserves all 20 saved GPS slots.\n\n')
        updated=old.replace('## Building',text+'## Building',1)
        if updated==old:raise ValueError('Build README section marker missing')
        p.write_text(updated,encoding='utf-8',newline='\n')
    print('Documented police spraytag navigation; full history section 35')

if __name__=='__main__':main()
