"""Document fixed RCB claims and local faction guards without rewriting past releases."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
NOTES = '''# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r22

## RCB-Flaggen auf der Quick Map

`/claims` schaltet die 28 vom Nutzer vorgegebenen Flaggenorte der
Red County Brotherhood auf der vorhandenen Quick Map ein. Ein erneuter Aufruf
blendet sie wieder aus. Die Quick Map wird weiterhin mit ihrem bisherigen
Hotkey geöffnet und bedient; die Flaggen folgen dem vorhandenen Zoom und
Kartenausschnitt. In beiden Radar-Designs wird das vorhandene rote
`radar_enemyAttack`-Flaggensymbol verwendet.

Die gespeicherten Werte X und Y bestimmen den Kartenort. Z bleibt für die
Nachvollziehbarkeit in `include/ClaimSpots.h` erhalten. Ped-Modell, Blickrichtung
und Waffenangaben der ursprünglichen AddPlayerClass-Zeilen werden dafür nicht
benötigt. Es werden keine Spielerklassen oder Weltobjekte erzeugt.

Zum Einschalten muss die eigene Fraktion als Red County Brotherhood erkannt
sein. Für andere oder nicht bestätigte Mitglieder erscheint ausschließlich lokal:

```text
Du bist kein Mitglied der Red County Brotherhood.
```

Ohne Einschalten gibt es keine Claims-Markierungen. Rollenverlust oder
Verbindungsende entziehen die Freigabe; ein erneutes Einschalten ist dann nur
mit bestätigter RCB-Mitgliedschaft möglich. Die bestehende Karte, Servermarker,
Spieleranzeige und Navigation bleiben bedienbar.

## /speed nur als Polizist

Der automatische Wanted-Dialogablauf ist auf die eigene bestätigte
SAPD-/LSPD-Mitgliedschaft begrenzt. Bei anderen oder nicht bekannten Fraktionen
kommt lokal:

```text
Du bist kein Polizist.
```

Dabei wird weder `/su` gesendet noch ein Wanted-Dialog beantwortet. Der
bisherige `/speed ID`- beziehungsweise eindeutige Spielernamen-Ablauf bleibt
für berechtigte Polizisten erhalten. Bei Verlust der Berechtigung darf ein
bereits laufender Ablauf keine weiteren Wanted-Antworten absenden.

Die Fraktion benötigt die synchronisierte eigene Statistik der aktuellen Verbindung.
Ein frischer Online-Eintrag desselben Namens allein erteilt keine Freigabe.
Zusätzlich werden vorhandene frische Online-Fraktionsinformationen für den
eigenen Spielernamen zum Abgleich verwendet. Es gibt
keine manuelle Fraktionsfreigabe über ein frei editierbares Notizfeld und keine
Berechtigung aus dem gewählten Skin.

Wenn bekannte eigene Statistik und frische Online-Zuordnung einander widersprechen,
wird keine Fraktionsfreigabe erteilt. Fehlen die Online-Daten, kann die gültige
eigene Statistik verwendet werden. Nach einem Fraktionswechsel lässt sich der
aktuelle Spielstand über den vorhandenen Synchronisationsbutton neu einlesen.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r22` enthält `NLBinder.asi`, den
vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`, den
postbaren `FORUM-CHANGELOG.txt` und die bestehenden Unterlagen einschließlich
`FORUM-INSTALLATION-QUELLCODE`. Vorhandene Desktop-Ordner werden erhalten.
Der Spielordner wird durch diesen Export nicht geändert. Zum Übernehmen GTA
vollständig beenden, die ASI ersetzen und das Spiel neu starten.

Version und Credits bleiben v.0.1.2 by perkys & edited by KOBiRO. Die feste
Browser-Runtime cef156-r1 bleibt erhalten; für die Claims-Funktion wird kein
neues Browserpaket eingerichtet.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r22/docs/review/r22-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r22/docs/review/r22-validation.json)
'''

REVIEW = '''# Prüfumfang r22 – 9. November 2026

Die neuen Regressionen prüfen die 28 festen Flaggenkoordinaten, ausgeschalteten
Anfangszustand, berechtigtes Umschalten, Entzug der Freigabe, bestehende
Quick-Map-Projektion sowie unveränderte fremde Kartenmarker. Die verwendete
Bildressource ist das bereits vorhandene rote ENEMYATTACK-Flaggensymbol aus
beiden eingebetteten Radar-Paketen.

Die Fraktionsprüfungen behandeln die eigene Identität, bestätigte und fehlende
Fraktion, RCB sowie SAPD-/LSPD-Aliase, Fraktionswerte aus eigenen Statistiken
und Abweisung fremder Statistiken. Zusätzliche reine Tests behandeln frische
Online-Zuordnung, Stats-Fallback und Ablehnung widersprüchlicher bekannter Rollen. Negative /speed-Fälle dürfen weder den
Wantedkatalog öffnen noch Dialogantworten auslösen; positive Fälle erhalten den
bisherigen automatischen Ablauf. Neue Claims-Markierungen entstehen nur im
Quick-Map-Zeichenpfad und nicht über GTA-/Server-Radar- oder Checkpoint-APIs.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build, zusätzliche
Quick-Map- und Statistikparser-Prüfungen sowie den nach dem Helferbau geprüften
nativen Reconnect-Worker. Der PE-Nachweis prüft x86-DLL, eingebettete Helfer auf
Bytegleichheit, ZIP-Lizenz und die erhaltene v.0.1.2-Startmeldung.

Unveränderte Browser-, GPS-, Drogenoverlay- und Helferquellen werden gegen den
veröffentlichten r21-Commit c7b4cb0a55d80cc7a32582cf9d08ed4bb8566f44
verglichen. Die neue Fraktions-/Speed-/Quick-Map-Integration ist ausdrücklich
nicht als unveränderte Quelle behandelt. Die frühere aufwendige Browser-
Download-/Liveprüfung wird für r22 nicht als erneut ausgeführt dargestellt.

Der fokussierte Forum-Quellcodeauszug wird aus dem aktuellen Quellbaum neu
gebildet und vollständig mit seinen SHA-256-Werten abgeglichen. Die Main-Datei
enthält nun auch die neue Befehlsanbindung; dies ist eine Quelländerung des
Prüfauszugs, kein neues Browser-Runtimepaket.

Der neue Desktop-Ordner wird aus den unveränderlichen veröffentlichten Git-Blobs
gelesen und dateiweise geprüft. Persönliche Konfigurationen, Profile und Logs
werden nicht mitverteilt. Ein erfolgreicher neuer Sichttest im laufenden Spiel
wird erst nach einer entsprechenden tatsächlichen Beobachtung dokumentiert.
'''

HISTORY = '''
## 28. Nachtrag v0.1.2 / r22 – 9. November 2026

- `/claims` zum Ein-/Ausblenden von 28 vorgegebenen RCB-Flaggenorten auf der
  vorhandenen Quick Map ergänzt. Alle X/Y/Z-Koordinaten in ClaimSpots.h erhalten;
  X/Y werden projiziert, AddPlayerClass-Modell/Blickrichtung/Waffen nicht benutzt.
- Vorhandenes rotes `radar_enemyAttack`-Flaggensymbol in beiden Radar-Designs
  verwendet. Zoom, Kartenausschnitt und bestehende Marker bleiben erhalten.
- Aktivierung an die eigene bestätigte Red County Brotherhood-Mitgliedschaft
  gebunden; andernfalls lokal „Du bist kein Mitglied der Red County Brotherhood.“.
- Claims standardmäßig unsichtbar und nur nach explizitem Umschalten angezeigt.
  Rollen-/Verbindungsverlust entzieht die Anzeige. Keine GTA-/Serverblips erzeugt.
- `/speed` auf bestätigte SAPD-/LSPD-Mitglieder begrenzt; unberechtigt nur lokal
  „Du bist kein Polizist.“, ohne /su oder Wanted-Dialogantwort. Laufenden Ablauf
  bei Berechtigungsverlust stoppen.
- Eigene synchronisierte Fraktionsinformation ergänzt und für die lokale
  Berechtigungsprüfung verwendet; fremde Stats und Skin-Raten ausgeschlossen.
- Zusätzliche Koordinaten-, Quick-Map-, Fraktions-/Stats- und Speed-Prüfungen
  aufgenommen. Tatsächliche Zahlen stehen im r22-Nachweis; kein Live-Test erfunden.
- Gesamten Quellcode, ASI und Texte als neuen Desktop-Ordner NLBinder-v0.1.2-r22
  vorgesehen. Version/Credits und Browser-Runtime bleiben unverändert.
'''

README = '''# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r22.

`/claims` schaltet für RCB-Mitglieder 28 feste rote Flaggen auf der bestehenden
Quick Map ein oder aus. `/speed` ist auf bestätigte SAPD-/LSPD-Mitglieder
begrenzt; Abweisungen bleiben lokal. Die eigene Fraktionszuordnung wird für
beide Befehle geprüft.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r22)
- [NLBinder.asi](dist/NLBinder-0.1.2-r22/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r22-DE.md)
- [28 Flaggenorte](include/ClaimSpots.h)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext zum Posten](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien](docs/FORUM-DATEILISTE.txt)
- [Eigener Installer-/Browsercode](dist/NLBinder-0.1.2-r22/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r22-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r22-validation.json)
- [Änderungen seit r21](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r21...v0.1.2-r22)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der neue Desktop-Ordner enthält vollständige Binder-Quellen, die fertige ASI
und Changelogs. Vorherige Ordner und die ursprüngliche Git-Historie bleiben
erhalten. Der Spielordner wird durch den Export nicht verändert.

GTA vollständig schließen, ASI ersetzen und neu starten. Die Browser-Runtime
cef156-r1 bleibt erhalten. Historische Browser-/GPS-Nachweise sind von den
neuen r22-Prüfungen und einem noch ausstehenden Sichttest getrennt dokumentiert.
'''

def main():
    (R/'docs/NLBinder-0.1.2-r22-DE.md').write_text(NOTES,encoding='utf-8',newline='\n')
    (R/'docs/review/r22-PRUEFUNG-DE.md').write_text(REVIEW,encoding='utf-8',newline='\n')
    (R/'build').mkdir(exist_ok=True)
    (R/'build/README-GITHUB-r22.md').write_text(README,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(re.sub(r'\]\((docs/|dist/|res/|include/)',r'](../../\1',README),encoding='utf-8',newline='\n')
    path=R/'docs/review/CHANGELOG-DE.md';text=path.read_text(encoding='utf-8-sig')
    if '## 28. Nachtrag v0.1.2 / r22' not in text:
        path.write_text(text.rstrip()+'\n'+HISTORY,encoding='utf-8',newline='\n')
    path=R/'docs/review/PRUEFUNG-DE.md';text=path.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r22' not in text:
        text=text.replace('## Aktueller Stand v0.1.2 / r21','## Vorheriger Stand v0.1.2 / r21',1)
        first,_,rest=text.partition('\n')
        text=first+'\n\n## Aktueller Stand v0.1.2 / r22 – 9. November 2026\n\n'+(
            'RCB-Claims und die lokale Fraktionsprüfung für /speed stehen im\n'
            '[r22-Prüfumfang](r22-PRUEFUNG-DE.md) und\n'
            '[Prüfnachweis](r22-validation.json). Frühere Ergebnisse bleiben erhalten.\n')+rest
        path.write_text(text,encoding='utf-8',newline='\n')
    print('Documented RCB claims, police speed guard and history section 28')

if __name__=='__main__':main()
