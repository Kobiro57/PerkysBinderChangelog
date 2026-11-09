"""Document the observed server action prefix without rewriting prior releases."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
TAG='v0.1.2-r21'
URL='https://github.com/Kobiro57/PerkysBinderChangelog'
NOTES='''# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r21

## Drogen-Overlay: tatsächliches Serverformat

Der Server meldet eine Einnahme beispielsweise als:

```text
*** KOBiRO nimmt Heroin zu sich.
```

Das führende `*** ` wurde bislang nicht berücksichtigt. Der Parser entfernt
dieses Präfix jetzt genau einmal am Anfang der bereinigten Zeile. Anschließend
müssen weiterhin der vollständige eigene Spielername und die vollständige
Konsum-Aktionszeile stimmen. Der Name wird dynamisch aus dem Spiel gelesen.
Fremde Spieler, Namensvorsilben und zitierte normale Chatnachrichten starten
keine eigenen Timer. Die bisherige Schreibweise ohne Sterne bleibt unterstützt.

Heroin startet gleichzeitig mit 120 Sekunden Cooldown und 60 Sekunden
Abklingzeit. Die bisherigen Zeiten der übrigen Drogen, die lokale einmalige
Bereit-Meldung, Aktivierung sowie Move-/ESC-Verhalten bleiben erhalten.
Der Schalter liegt unter Online → Overlay-Einstellungen → Dauerhaft →
Spielinformationen → Use Cooldown & Nachwirkung anzeigen.

## Installation und Quellcode

GTA vollständig beenden, NLBinder.asi ersetzen und anschließend neu starten.
Der Desktop-Ordner NLBinder-v0.1.2 enthält die fertige ASI, vollständige Quellen,
CHANGELOG-GESAMT.txt sowie die vorhandenen Forumtexte, Runtime-Dateiliste und
FORUM-INSTALLATION-QUELLCODE. Ein bekannter unveränderter vorheriger r20-Ordner
wird als Sicherung erhalten. Bei unerwarteten Änderungen wird stattdessen ein
neuer r21-Ordner angelegt; persönliche Änderungen werden nicht überschrieben.

Die bestehende Browser-Runtime cef156-r1 bleibt unverändert. Es gibt durch
diesen Parserfix keine neue Browserinstallation oder neue Runtime-Dateien.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r21/docs/review/r21-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r21/docs/review/r21-validation.json)
'''
REVIEW='''# Prüfumfang r21 – 9. November 2026

Auslöser ist die tatsächliche vom Nutzer gemeldete Zeile
`*** KOBiRO nimmt Heroin zu sich.`. Der bisherige Parser erwartete den Namen
direkt am Zeilenanfang. Die zusätzlichen Regressionen prüfen genau diese Zeile
mit 120/60 Sekunden sowie alle Drogen mit und ohne Präfix, dynamische eigene
Namen, fremde Namen, Namensvorsilben, Chat-Zitate, Farben, Zeitstempel und
einmalige Bereit-Ereignisse. Die vollständige vorhandene Testsuite bleibt Teil
des Release-Nachweises; die genaue Prüfzahl entsteht aus dem tatsächlichen Log.

Der Paketbau verlangt erfolgreiche Gesamttests, einen erfolgreichen Release-Build,
zusätzliche Drogenparser-Prüfungen und den nach dem finalen Helferbau ausgeführten
nativen Reconnect-Worker-Test. PE32/x86, exakte eingebettete Helfer und die unveränderte
gewünschte v0.1.2-Startmeldung werden an der fertigen ASI geprüft.

Browser-, GPS-, UI-, Versions- und Helferquellen werden gegen den veröffentlichten
r20-Commit 9a728abaf27b1784cf3b6a65beff4746add2120e auf unveränderte Bytes geprüft.
Der Drogenparser ist ausdrücklich eine beabsichtigte Änderung. Browser-Download-
und Browser-/GPS-Livetests werden nicht erneut als ausgeführt behauptet.

Alle 41 mitgelieferten Forum-Prüfauszugsdateien werden mit ihren aufgezeichneten
SHA-256-Werten verglichen. Vollständige Quellen und Texte des Desktop-Exports
werden aus dem veröffentlichten Git-Commit gelesen und geprüft.

Die Nutzerbeobachtung belegt den früheren Fehler. Eine erfolgreiche erneute
Einnahme im laufenden GTA ist erst nach einer tatsächlichen Rückmeldung ein
Live-Nachweis; die automatischen Parser-/UI-Tests werden davon getrennt.
'''
HISTORY='''
## 27. Nachtrag v0.1.2 / r21 – 9. November 2026

- Tatsächliches Serverformat `*** Name nimmt Droge zu sich.` berücksichtigt:
  das führende Aktionspräfix `*** ` vor der Namensprüfung genau einmal entfernen.
- Dynamische eigene Namensprüfung und vollständige Aktionszeile beibehalten;
  fremde Spieler, Namensvorsilben und zitierte Chatzeilen bleiben ausgeschlossen.
- Konkreten Fehler mit `*** KOBiRO nimmt Heroin zu sich.` abgesichert; Heroin
  startet mit 120 Sekunden Cooldown und parallelen 60 Sekunden Abklingzeit.
- Bisherige Schreibweise ohne Sterne, alle sieben Drogen und Ecstasy/Ectasy,
  Farb-/Zeitstempelbereinigung und einmalige Bereit-Ereignisse nachgeprüft.
- Version 0.1.2, Credits, übrige Overlay-Funktionen, Forum-Installationsunterlagen
  und die Browser-Runtime bleiben erhalten. Source/ASI/Texte als r21 aktualisiert.
- Automatisierte Prüfergebnisse getrennt von einer noch ausstehenden erneuten
  Nutzerbeobachtung im laufenden Spiel dokumentiert.
'''
README=f'''# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r21.

r21 erkennt das tatsächliche Drogen-Serverformat `*** Name nimmt Droge zu sich.`.
Nur der vollständige eigene Name startet die Timer. Heroin beginnt mit 120 s
Cooldown und 60 s paralleler Abklingzeit; die bisherigen Zeiten bleiben erhalten.

- [Fertige ASI und Dokumentation]({URL}/releases/tag/{TAG})
- [NLBinder.asi](dist/NLBinder-0.1.2-r21/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r21-DE.md)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext zum Posten](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien](docs/FORUM-DATEILISTE.txt)
- [Eigener Installer-/Browsercode](dist/NLBinder-0.1.2-r21/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r21-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r21-validation.json)
- [Änderungen seit r20]({URL}/compare/v0.1.2-r20...{TAG})
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der gesamte Binder-Quellcode und die ursprüngliche Historie bleiben erhalten.
GTA vollständig schließen, ASI ersetzen und neu starten. Die Browser-Runtime
cef156-r1 ist unverändert; ein neuer Download ist durch den Parserfix nicht nötig.
'''

def main():
    (R/'docs/NLBinder-0.1.2-r21-DE.md').write_text(NOTES,encoding='utf-8',newline='\n')
    (R/'docs/review/r21-PRUEFUNG-DE.md').write_text(REVIEW,encoding='utf-8',newline='\n')
    (R/'build').mkdir(exist_ok=True)
    (R/'build/README-GITHUB-r21.md').write_text(README,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/)',r'](../../\1',README),encoding='utf-8',newline='\n')
    path=R/'docs/review/CHANGELOG-DE.md';text=path.read_text(encoding='utf-8-sig')
    if '## 27. Nachtrag v0.1.2 / r21' not in text:
        path.write_text(text.rstrip()+'\n'+HISTORY,encoding='utf-8',newline='\n')
    path=R/'docs/review/PRUEFUNG-DE.md';text=path.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r21' not in text:
        text=text.replace('## Aktueller Stand v0.1.2 / r20','## Vorheriger Stand v0.1.2 / r20',1)
        first,_,rest=text.partition('\n')
        text=first+'\n\n## Aktueller Stand v0.1.2 / r21 – 9. November 2026\n\n'+(
            'Der konkrete Serverpräfix-Fehler ist im [r21-Prüfumfang](r21-PRUEFUNG-DE.md)\n'
            'und [Prüfnachweis](r21-validation.json) dokumentiert. Frühere Ergebnisse\n'
            'bleiben nachstehend erhalten.\n')+rest
        path.write_text(text,encoding='utf-8',newline='\n')
    print('Documented the exact server action prefix fix and history section 27')

if __name__=='__main__':main()
