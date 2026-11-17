"""Document v0.1.3, editable spray targets and arrival-driven navigation tours."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]
TITLE='NLBinder v.0.1.3 by perkys & edited by KOBiRO'
DETAIL='''Die Startmeldung und die Versionsanzeige verwenden jetzt Version 0.1.3.
Die bisherigen Credits bleiben `perkys & edited by KOBiRO`.

Mit `/sprays` öffnen bestätigte Nicht-Zivilisten die Spray-Navigation.
Mitglieder von Fraktionen und Gangs haben Zugriff; Zivilisten sind ausgeschlossen.
Die zunächst hinterlegten 100 Graffitipunkte lassen sich einzeln entfernen.
Eigene Punkte können mit einem Namen an der aktuellen Spielerposition ergänzt
werden. Die bearbeitete Liste wird lokal unter
`Dokumente\\GTA San Andreas User Files\\NLBinder\\sprays.xml` gespeichert und beim nächsten Start geladen.
Diese Datei enthält nur die eigenen Navigationspunkte und gehört nicht zum
öffentlichen Quellcodepaket oder zu GitHub.

Die Punkte werden nach ihrer dreidimensionalen Entfernung sortiert; die nächste
Stelle wird grün hervorgehoben. Je Seite werden bis zu 34 Punkte dargestellt.
Die Liste wächst bedarfsgerecht auf höchstens zehn Seiten beziehungsweise
340 Punkte. Beim Auswählen startet die vorhandene rote GPS-Checkpoint-Navigation
mit der bisherigen Ziel-erreicht-Meldung. Temporäre Spray-Ziele belegen keinen
der 20 gespeicherten normalen GPS-Plätze.

Das Dialogfenster ist größer und innerhalb der Bildschirmgröße veränderbar.
Der Inhaltsbereich mit Eingabe und Punktliste kann separat scrollen.
Seitenwahl und Schließen bleiben als feste Bedienelemente auch bei kleinerem
Fenster sichtbar.

`/spraytour` startet eine Tour durch die persönliche Spray-Liste. Nach dem
Erreichen eines Checkpoints wird der nächste noch nicht besuchte Punkt anhand
der aktuellen Entfernung als GPS-Ziel gewählt. `/sprayend` beendet die Tour.
Mit `/claimstour` können ausschließlich Mitglieder der Red County Brotherhood
die 28 hinterlegten Flaggenpunkte auf dieselbe Weise anfahren; `/claimsend`
beendet diese Tour. Die Touren steuern die Navigation. Das Entfernen von Sprays
und die Flaggenaktionen werden weiterhin wie gewohnt im Spiel ausgeführt.

Die automatische Polizeifunk-Ortung bleibt entfernt. Wanted-Overlay,
Gesucht-Labels, aktuelle Spieler-IDs, `/speed`, Quick Map und die übrigen
Binder-Funktionen bleiben erhalten.
'''
POST='''NLBinder v.0.1.3 by perkys & edited by KOBiRO – Kurz-Changelog
- Versionsanzeige und Startmeldung auf 0.1.3 aktualisiert.
- /sprays für Fraktionen und Gangs: Entfernungssortierung, nächster Punkt grün und rote GPS-Navigation; Zivilisten ausgeschlossen.
- Graffitipunkte einzeln entfernen und eigene Punkte mit Namen an der aktuellen Position hinzufügen.
- Persönliche Spray-Liste wird lokal in Dokumente\\GTA San Andreas User Files\\NLBinder\\sprays.xml gespeichert; bis zu zehn Seiten mit jeweils 34 Punkten.
- Größeres, anpassbares Spray-Fenster mit separat scrollender Liste und sichtbaren Bedienelementen.
- /spraytour und /sprayend: Tour zum jeweils nächsten unbesuchten Spray-Punkt, automatisch weiter nach Checkpointankunft.
- /claimstour und /claimsend: entsprechende Navigation durch die 28 RCB-Flaggenpunkte, nur für RCB-Mitglieder. Spray- und Flaggenaktionen bleiben im Spiel manuell.
- Normale GPS-Speicherplätze bleiben frei; automatische Polizeifunk-Ortung bleibt entfernt.
'''
HISTORY='''
## 36. Veröffentlichung v0.1.3 – 17. November 2026

- Angezeigte Version und Startmeldung von 0.1.2 auf 0.1.3 aktualisiert; Credits perkys & edited by KOBiRO beibehalten.
- Spray-Liste um das Entfernen einzelner vorhandener Punkte und das Hinzufügen eigener benannter Punkte an der aktuellen Position erweitert.
- Spray-Navigation für bestätigte Fraktions- und Gangmitglieder freigegeben; Zivilisten bleiben ausgeschlossen. Die eigenen SAPD-/LSPD- beziehungsweise RCB-Berechtigungen anderer Funktionen bleiben erhalten.
- Persönliche Spray-Liste lokal unter Dokumente\\GTA San Andreas User Files\\NLBinder\\sprays.xml gespeichert und beim nächsten Start wieder geladen; persönliche Datei nicht veröffentlicht.
- Seitenzahl bedarfsgerecht erweitert: 34 Punkte je Seite, höchstens zehn Seiten beziehungsweise 340 Punkte.
- Spray-Fenster vergrößert und innerhalb der Bildschirmgröße veränderbar gemacht; Bedienelemente bleiben außerhalb der separat scrollenden Liste sichtbar.
- Entfernungssortierung, grüne Hervorhebung des nächsten Punkts, rote GPS-Navigation und Ziel-erreicht-Meldung beibehalten.
- /spraytour und /sprayend ergänzt: persönliche Spray-Liste abfahren und nach Checkpointankunft automatisch zum nächstgelegenen noch nicht besuchten Ziel navigieren.
- /claimstour und /claimsend für RCB-Mitglieder ergänzt: entsprechende Navigation durch die 28 hinterlegten Flaggenpunkte.
- Touren steuern ausschließlich die Navigation; Spray-Entfernen und Flaggenaktionen werden weiterhin im Spiel ausgeführt.
- Spray-Ziele verbrauchen weiterhin keinen der 20 gespeicherten GPS-Plätze. Automatische Polizeifunk-Ortung bleibt entfernt.
- Neue Gesamttests, Release-Build, eingebettete Helfer und aktuelle Versionsmeldung werden geprüft; tatsächliche Ergebnisse stehen im v0.1.3-Prüfnachweis.
- Neue NLBinder.asi, vollständige aktuelle Quellen und Changelogs als NLBinder-v0.1.3 vorbereitet; frühere Veröffentlichungen und Desktop-Exporte erhalten.
'''

def update_history_header(text):
    text,n=re.subn(r'(?m)^# [^\n]*',lambda _:'# Änderungen am Perkys Binder – '+TITLE,text,count=1)
    status=('Stand: 17. November 2026, **v0.1.3 mit bearbeitbaren Spray-Punkten und Navigationstouren**. '
            'Die Abschnitte 1 bis 35 dokumentieren die früheren Stände; Abschnitt 36 ergänzt v0.1.3.')
    text,m=re.subn(r'(?m)^Stand: [^\n]*',lambda _:status,text,count=1)
    if n!=1 or m!=1:raise ValueError('Expected changelog title and status')
    return text

def main():
    notes='# '+TITLE+'\n\n'+DETAIL+'''
## Paket übernehmen

Der neue Desktop-Ordner `NLBinder-v0.1.3` enthält die fertige `NLBinder.asi`,
vollständige aktuelle Quellen, den Gesamt-Changelog und den kurzen Forumtext.
Die ASI bei vollständig beendetem GTA ersetzen. Vorhandene persönliche GPS-
und Spray-Dateien verbleiben unter Dokumente\\GTA San Andreas User Files\\NLBinder. Die Browser-Runtime
cef156-r1 benötigt für dieses Update keinen neuen Download.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.3/docs/review/r30-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.3/docs/review/r30-validation.json)
'''
    review='''# Prüfumfang v0.1.3 – 17. November 2026

Ein neuer vollständiger Testlauf und der aktuelle Release-Build müssen bestehen.
Die Spray-Tests prüfen bearbeitbare Punkte, lokale Speicherung und Laden,
Kapazität bis zu 340 Punkten, bedarfsgerechte Seiten mit 34 Punkten,
Entfernungssortierung, Berechtigungen und unveränderte normale GPS-Speicherplätze.
SprayCatalogTests prüfen außerdem das dauerhafte Entfernen von Standardpunkten
und das Laden einer absichtlich leeren Liste ohne erneute Standardbefüllung.
SprayAccessTests prüfen den bestätigten Zugriff von Fraktionen und Gangs sowie
den Ausschluss von Zivilisten. Die bisherigen Polizei- und RCB-Prüfungen anderer
Funktionen bleiben maßgeblich. WaypointTourTests prüfen das nächste unbesuchte
Ziel, Ankunftsübergänge, Beenden und den sicheren Wechsel zwischen Navigationen.
Die bestehenden Regressionssuiten für Kampf, Schadenslog, Zähler, Statistiken,
Jobskills, Wanted-Labels, GPS, Browser und übrige Funktionen gelten weiter.

Der PE-Nachweis prüft die x86-ASI und ihre tatsächliche v0.1.3-Startmeldung sowie
die eingebetteten Helfer. Der aktuelle Reconnect-Helfer wird mit lokalen
Fixture-Prozessen geprüft. Die frühere automatische Ortung darf weder in den
aktiven Quellen noch als [ORTUNG]-Vorlage in der gebauten ASI vorhanden sein.

Der unveränderte CEF-Eingabenachweis aus r24 bleibt ausdrücklich historisch.
Ein erneuter Browser- oder GTA-Livetest wird nicht behauptet. Tatsächliche
Ergebnisse stehen in r30-validation.json. Persönliche Konfiguration, GPS- und
Spray-Dateien, Logs und Zugangsdaten werden nicht veröffentlicht. Die Desktop-
Quellen und die ASI werden gegen den veröffentlichten Commit geprüft.
'''
    readme='# NLBinder – PerkysBinderChangelog\n\nAktueller Stand: **'+TITLE+'**.\n\n'+DETAIL+'''
- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.3)
- [NLBinder.asi](dist/NLBinder-0.1.3/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.3-DE.md)
- [Spray-Navigation](src/SpraysNavigator.cpp), [Punktverwaltung](src/SprayCatalog.cpp), [Standardpunkte](include/SpraySpots.h)
- [Navigationstouren](src/WaypointTour.cpp), [Spray-Berechtigung](include/SprayAccess.h)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Kurzer Changelog](docs/FORUM-CHANGELOG-v0.1.3.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md), [Dateiliste](docs/FORUM-DATEILISTE.txt)
- [Eigener Installationsquellcode](dist/NLBinder-0.1.3/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r30-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r30-validation.json)
- [Änderungen seit r29](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r29...v0.1.3)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)
'''
    for name,data in [('docs/NLBinder-0.1.3-DE.md',notes),('docs/review/r30-PRUEFUNG-DE.md',review),('build/README-GITHUB-r30.md',readme),('docs/FORUM-CHANGELOG-v0.1.3.txt',POST)]:
        (R/name).write_text(data,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',readme),encoding='utf-8',newline='\n')
    p=R/'docs/review/CHANGELOG-DE.md';s=p.read_text(encoding='utf-8-sig')
    s=update_history_header(s)
    if '## 36. Veröffentlichung v0.1.3' not in s:s=s.rstrip()+'\n'+HISTORY
    p.write_text(s,encoding='utf-8',newline='\n')
    p=R/'docs/review/PRUEFUNG-DE.md';s=p.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.3' not in s:
        s=s.replace('## Aktueller Stand v0.1.2 / r29','## Vorheriger Stand v0.1.2 / r29',1)
        first,_,rest=s.partition('\n')
        p.write_text(first+'\n\n## Aktueller Stand v0.1.3 – 17. November 2026\n\nBearbeitbare Spray-Liste, Navigationstouren und neue Versionsanzeige: [Prüfumfang](r30-PRUEFUNG-DE.md), [Ergebnisse](r30-validation.json).\n'+rest,encoding='utf-8',newline='\n')
    p=R/'docs/BUILD-README.md';s=p.read_text(encoding='utf-8-sig')
    replacement='- **Spraytag Navigation** — `/sprays` provides verified faction and gang members with editable local targets, initially 100 supplied points; civilians are excluded. Add named targets at the current position or remove entries. Distances sort nearest first with a green nearest row. Up to ten pages of 34 points are supported. Local Documents\\GTA San Andreas User Files\\NLBinder\\sprays.xml persists the list; temporary red GPS targets preserve the 20 normal saved slots.'
    s=re.sub(r'(?m)^- \*\*(?:Police )?Spraytag Navigation\*\*[^\n]*',lambda _:replacement,s)
    tour='- **Navigation Tours** — `/spraytour` visits the next nearest unvisited configured spray point after checkpoint arrival; `/sprayend` stops. RCB-only `/claimstour` and `/claimsend` provide the same navigation for the 28 static flag points. Tours navigate; in-game spray and flag actions remain manual.'
    if '- **Navigation Tours**' not in s:s=s.replace(replacement,replacement+'\n\n'+tour,1)
    p.write_text(s,encoding='utf-8',newline='\n')
    print('Documented v0.1.3 with editable persisted spray points; full history section 36')

if __name__=='__main__':main()
