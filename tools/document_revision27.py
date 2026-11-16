"""Document operation-specific detective marker selection and city/district reporting."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.2 by perkys & edited by KOBiRO'
DETAIL='''Die automatische Polizeifunk-Ortung verwendet den zum aktuellen
Detektivvorgang gehörenden Ziel-Checkpoint. Bereits vorher aktive normale oder
Renn-Checkpoints werden nicht allein deshalb als Ziel gewählt, weil sie noch
angezeigt werden. Neu gesetzte beziehungsweise geänderte Marker werden dem
laufenden Vorgang zugeordnet. Eine persönliche Erfolgsmeldung und ein passender
verfügbarer Zielmarker bleiben erforderlich.

Die Ortsangabe wird aus den Koordinaten dieses Zielmarkers bestimmt und enthält
Stadt und Bezirk, zum Beispiel `Los Santos Market`. Die nativen Zonentabellen
werden mit ihrem tatsächlichen 32-Byte-Aufbau gelesen; die bisherige falsche
Strukturgröße des eingebundenen SDK wird für diese Abfrage nicht verwendet.
Stadt und Bezirk werden anhand der Ziel-X/Y-Koordinaten zugeordnet:

```text
/r [ORTUNG] Gesuchter NAME wurde zuletzt in [Los Santos Market] geortet.
```

Die eigene Spielerposition dient nicht als Ersatz. Fehlt eine verlässliche
Zuordnung, wird keine Ortsangabe von einem beliebigen alten Marker gesendet.
Doppelte Erfolgsmeldungen erzeugen keinen weiteren Funktext. Rollen- und
Sitzungswechsel verwerfen einen offenen Vorgang. Die Funktion bleibt auf
bestätigte SAPD-/LSPD-Mitglieder begrenzt.

Die native Sichtbarkeitsbindung der Wanted-Labels aus r26 bleibt erhalten,
einschließlich Fahrern und Mitfahrern. Die vorhandene TakeOver-Killerkennung,
Forum-Sonderzeicheneingabe und Drogenzeiten bleiben erhalten. Morphium behält
45 Sekunden Cooldown und 45 Sekunden Nachwirkung.
'''
POST='Neu in r27: Die automatische Polizeifunk-Ortung liest den Ziel-Checkpoint des aktuellen Detektivvorgangs aus. Bereits vorher aktive normale oder Renn-Checkpoints werden nicht mehr als beliebiger Ersatz verwendet. Die Funkmeldung enthält jetzt Stadt und Bezirk anhand der tatsächlichen Zielkoordinaten, zum Beispiel /r [ORTUNG] Gesuchter NAME wurde zuletzt in [Los Santos Market] geortet. Die eigene Position ersetzt die Zielposition nicht. Erfolgsmeldung, bestätigte SAPD-/LSPD-Mitgliedschaft und einmalige Ausgabe bleiben erforderlich. Die native Wanted-Label-Sichtbarkeit aus r26 sowie TakeOver-Killerkennung, Forum-Sonderzeichen und bisherige Drogenzeiten bleiben erhalten.'
HISTORY='''
## 33. Nachtrag v0.1.2 / r27 – 17. November 2026

- Auswahl des Ortungsmarkers auf den laufenden Detektivvorgang bezogen; bereits vorher aktive normale beziehungsweise Renn-Checkpoints nicht mehr als beliebigen Ziel-Ersatz verwendet.
- Neu gesetzte und geänderte Marker bei der Zuordnung berücksichtigt, auch wenn beide Checkpoint-Arten gleichzeitig aktiv sind.
- Polizeifunk-Ortsangabe um Stadt und Bezirk ergänzt, zum Beispiel Los Santos Market. Maßgeblich sind die Koordinaten des Ziel-Checkpoints.
- Native Zonentabellen mit ihrem tatsächlichen 32-Byte-Aufbau ausgewertet und die abweichende alte SDK-Struktur bei dieser Abfrage umgangen. Stadt und Bezirk anhand der Ziel-X/Y-Koordinaten bestimmt.
- Eigene Spielerposition nicht als Ersatz verwendet; bei fehlender verlässlicher Zuordnung keine Ortsangabe eines beliebigen alten Markers gesendet.
- Erfolgsmeldung, einmalige Ausgabe sowie bestätigte SAPD-/LSPD-Mitgliedschaft und Sitzungsprüfung beibehalten.
- Regressionen für alte und neue normale beziehungsweise Renn-Checkpoints sowie Stadt-/Bezirksauflösung ergänzt. Tatsächliche Ergebnisse stehen im r27-Prüfnachweis.
- Native Wanted-Labels aus r26, TakeOver-Killerkennung, Forum-Sonderzeichenkorrektur und bestehende Drogenzeiten erhalten. Browsernachweis aus r24 ausdrücklich als historisch übernommen.
- Neue NLBinder.asi, vollständige Quellen und gesamte Changelogs als NLBinder-v0.1.2-r27 vorbereitet. Frühere Exporte und Git-Historie erhalten; Spielinstallation unverändert.
'''

def update_history_header(text):
    heading='# Änderungen am Perkys Binder – '+TITLE
    status=('Stand: 17. November 2026, **r27 mit Ortung sowie Stadt und Bezirk**. '
            'Die Abschnitte 1 bis 32 dokumentieren die früheren Stände; Abschnitt 33 ergänzt r27.')
    text,headings=re.subn(r'(?m)^# [^\n]*',lambda _:heading,text,count=1)
    text,statuses=re.subn(r'(?m)^Stand: [^\n]*',lambda _:status,text,count=1)
    if headings != 1 or statuses != 1:
        raise ValueError('Expected one changelog title and current-status paragraph')
    return text


def main():
    notes='# '+TITLE+' – r27\n\n## Ortung mit Stadt und Bezirk\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.2-r27` enthält die fertige `NLBinder.asi`,
vollständige Binder-Quellen, den gesamten Changelog und `FORUM-CHANGELOG.txt`.
GTA vollständig beenden, die bisherige ASI ersetzen und neu starten.
Die Browser-Runtime cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r27/docs/review/r27-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r27/docs/review/r27-validation.json)
'''
    review='''# Prüfumfang r27 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die
PoliceLocateBridgeTests prüfen die Zuordnung des aktuellen Detektivvorgangs
mit alten und neuen normalen beziehungsweise Renn-Checkpoints. Insbesondere
müssen gleichzeitig aktive Marker auseinandergehalten werden. Ein unverändert
alter Marker darf nicht als beliebiges Ziel verwendet werden.

Die Ortsauflösung muss Stadt und Bezirk aus der Zielmarkerposition bilden.
CheckpointLocationTests prüfen die native 32-Byte-Zonenansicht und die
Stadt-/Bezirksauswahl aus den Ziel-X/Y-Koordinaten. Dabei wird der kleinste
passende Bezirk gewählt; auch Marker in Dachhöhe bleiben über X/Y zugeordnet.
Die Marker-Zeitfolge vor und nach der Erfolgsmeldung wird nach der behobenen
Zuordnungsregression geprüft. Los Santos Market ist der gemeldete Fehlerfall.
Die eigene Spielerposition ist
kein Ersatz. Die r26-Regressionen für persönliche Erfolgsmeldung, verzögerte
Marker, doppelte Meldungen, Rollen- und Sitzungswechsel gelten weiter.
Die bisherigen Wanted-Labels und übrigen Binder-Regressionssuiten bleiben Teil
der Gesamttests.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Browserquellen, Forum-Tastaturfix, Drogenzeiten und gepinnte Runtime
werden gegen r26 abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte
historische Browsernachweis aus r24; er wird nicht als neuer r27-Test ausgegeben.

Ein r27-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r27-validation.json. Der exportierte Quellcode
und die ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**, r27.\n\n'+DETAIL+'''
- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r27)
- [NLBinder.asi](dist/NLBinder-0.1.2-r27/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r27-DE.md)
- [Ortungsablauf](src/PoliceLocateBridge.cpp)
- [Stadt-/Bezirksauflösung](include/CheckpointLocation.h)
- [Wanted-Labels](src/NearbyWantedLabels.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Forum-/Installationsquellcode](dist/NLBinder-0.1.2-r27/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r27-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r27-validation.json)
- [Änderungen seit r26](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r26...v0.1.2-r27)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Alle früheren Binder-Funktionen sind enthalten. Neue Quellen und Changelogs
liegen zusätzlich im Desktop-Ordner NLBinder-v0.1.2-r27. Die Browser-Runtime
bleibt cef156-r1. Der frühere Browsernachweis wird als historisch ausgewiesen.
'''
    for name,data in [('docs/NLBinder-0.1.2-r27-DE.md',notes),('docs/review/r27-PRUEFUNG-DE.md',review),('build/README-GITHUB-r27.md',readme)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md'; s=p.read_text(encoding='utf-8-sig')
    updated=update_history_header(s)
    if '## 33. Nachtrag v0.1.2 / r27' not in updated:updated=updated.rstrip()+'\n'+HISTORY
    if updated != s:p.write_text(updated,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md';s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r27' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r26','## Vorheriger Stand v0.1.2 / r26',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.2 / r27 – 17. November 2026\n\nOrtung mit Stadt und Bezirk: [Prüfumfang](r27-PRUEFUNG-DE.md), [Ergebnisse](r27-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/FORUM-CHANGELOG-v0.1.2.txt';s=p.read_text(encoding='utf-8-sig')
    if 'Neu in r27:' not in s:
        s=s.replace('NEU IN DIESEM PAKET\n','NEU IN DIESEM PAKET\n\n'+POST+'\n',1).replace('/releases/tag/v0.1.2-r26','/releases/tag/v0.1.2-r27')
        p.write_text(s,encoding='utf-8-sig',newline='\n')
    p=R/'docs/BUILD-README.md';s=p.read_text(encoding='utf-8-sig')
    if '**Operation-specific police location**' not in s:
        s=s.replace('## Building','- **Operation-specific police location** — Select the target marker for the current detective operation and resolve its city plus district; unrelated pre-existing checkpoints are not substituted.\n\n## Building',1)
        p.write_text(s,encoding='utf-8',newline='\n')
    print('Documented operation-specific target marker and city/district reporting; full history section 33')

if __name__=='__main__':main()
