"""Document nearby SAPD wanted labels and checkpoint-correlated police location radio."""
from pathlib import Path
import re

R = Path(__file__).resolve().parents[1]

NOTES = '''# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r23

## Wanted-Label in direkter Nähe

Ist die vorhandene Wanted-Liste unter Online → Overlay-Einstellungen aktiviert
und die eigene SAPD-/LSPD-Mitgliedschaft bestätigt, erhalten unmittelbar
benachbarte gesuchte Spieler ein rotes Label über dem Kopf:

```text
[GESUCHT | 15 Wanteds]
```

Die feste maximale Entfernung beträgt **5 Meter in drei Dimensionen**. Das
Label wird nur für tatsächlich gestreamte, lebende und sichtbare Spieler vor
der Kamera gezeichnet. Eine freie Sichtlinie und die vorhandene serverseitige
Nametag-Freigabe sind erforderlich. Durch Wände, hinter der Kamera, in fremden
Innenräumen und außerhalb dieser unmittelbaren Nähe wird kein Label angezeigt.
Es gibt weder Entfernungswerte noch Richtungs-/Bildschirmrandmarkierungen.

Wantedzahlen stammen aus den vorhandenen frischen Online-Daten und müssen zum
Namen des tatsächlich gestreamten Spielers passen. Alte/offline Daten, eine
abweichende Identität und der eigene Spieler ergeben kein Label. Ohne
aktivierte Wanted-Liste oder bestätigte Polizeimitgliedschaft bleibt diese
zusätzliche Anzeige aus.

Die vorhandene zweidimensionale Wanted-Tabelle zeigt zusätzlich die aktuelle
SA-MP-Spieler-ID in Klammern hinter dem Namen, etwa `Lesch (24)`.
Die Wantedzahl bleibt unverändert. Die ID wird anhand des aktuellen
Spielerpools aufgelöst; eine Account-ID der Online-API wird nicht verwendet.
Ist ein Name gerade nicht eindeutig zuordenbar, bleibt er ohne erfundene ID.

## Ortung automatisch im Funk melden

Die vorhandene Detektiv-Ortung wird anhand der Servermeldungen verfolgt:

```text
Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.
Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.
```

Erst wenn die erfolgreiche Ortung **und ein frisch vom Server gesetzter
Checkpoint** zu diesem Ablauf vorliegen, wird einmalig folgende Funkmeldung
gesendet:

```text
/r [ORTUNG] Gesuchter Lesch wurde zuletzt in [Bezirk des Ortungsmarkers] geortet.
```

Spielername und Bezirk sind dynamisch. Der Bezirk wird aus den tatsächlichen
X-/Y-/Z-Koordinaten des neuen Server-Checkpoints bestimmt, nicht aus der
Position des Polizisten. Die Erfolgsmeldung allein und ein schon vorher
vorhandener Marker lösen keine Meldung aus. Die Reihenfolge von neuem Marker
und Erfolgsmeldung darf wechseln; beide müssen zum laufenden Ortungsvorgang
gehören. Abgebrochene oder veraltete Vorgänge und doppelte Meldungen senden
keinen zusätzlichen Funktext. Die eigene GPS-Navigation setzt diesen Ablauf
nicht in Gang.

Auch diese Funktion steht ausschließlich bestätigten SAPD-/LSPD-Mitgliedern
zur Verfügung. Rollenverlust, Spielerwechsel und Verbindungsende verwerfen
einen laufenden Vorgang. Die Prüfung verwendet die eigene synchronisierte
Fraktion der aktuellen Verbindung und den vorhandenen frischen Online-Abgleich.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r23` enthält `NLBinder.asi`, den
vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`, den
postbaren `FORUM-CHANGELOG.txt` und den vorhandenen separaten
`FORUM-INSTALLATION-QUELLCODE`. Frühere Desktop-Ordner werden erhalten.
Zum Übernehmen GTA vollständig beenden, die ASI ersetzen und neu starten.

Version/Credits bleiben v.0.1.2 by perkys & edited by KOBiRO. Die automatische
Browser-Einrichtung und die feste Runtime cef156-r1 bleiben erhalten.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r23/docs/review/r23-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r23/docs/review/r23-validation.json)
'''

REVIEW = '''# Prüfumfang r23 – 16. November 2026

Die neuen Regressionen prüfen die feste Fünf-Meter-Grenze einschließlich
Höhenunterschied, freie Sicht, Bildschirm-/Kameraprüfung, gestreamte Identität,
aktuelle Wanteddaten, aktivierte Wanted-Liste und bestätigte Polizeifraktion.
Das native Zeichnen verwendet ausschließlich die tatsächlich sichtbaren
Spieler-Peds und beachtet die vorhandene Nametag-Freigabe.
Für die Wanted-Tabelle werden aktuelle SA-MP-IDs einschließlich der gültigen
ID 0 geprüft; fehlende Zuordnungen dürfen keine fremde/API-Account-ID anzeigen.

Die Ortungsregressionen behandeln den dynamischen Zielnamen, Start-/Erfolgs-
meldungen, neue Server-Checkpoints, beide möglichen Ereignisreihenfolgen,
vorhandene alte Marker, Duplikate, Zeitablauf sowie Rollen-/Sitzungswechsel.
Der Bezirk muss aus den Markerkoordinaten statt der eigenen Position stammen.
Ein eigener GPS-Checkpoint darf keine automatische Ortungsfunkmeldung auslösen.
Die RPC-Adapterprüfung muss die Koordinaten vollständig und mit gültigen
Werten lesen und unvollständige/ungültige Daten verwerfen.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build, die neuen
Wantedlabel-/Ortungs-/Checkpoint-Prüfungen sowie den nach dem Helferbau
geprüften nativen Reconnect-Worker. Der PE-Nachweis prüft x86-DLL,
bytegleiche eingebettete Helfer, ZIP-Lizenz und die erhaltene v.0.1.2-Startmeldung.

Unveränderte Browser-, GPS-, Drogenoverlay- und Helferquellen werden gegen den
veröffentlichten r22-Commit 0b93ba47f646b8cfea8da59996a7f170c2d5dfa4
verglichen. Die neue Polizeifunktion samt Bridge-/Main-/Online-UI-Anbindung
ist ausdrücklich nicht als unveränderte Quelle behandelt. Die früheren
Browser-/GPS-Liveprüfungen werden für r23 nicht als erneut ausgeführt dargestellt.

Der fokussierte Forum-Quellcodeauszug wird aus dem aktuellen Baum neu gebildet
und vollständig mit SHA-256 abgeglichen. Die darin enthaltene Main-Anbindung
kann sich ändern, ohne dass eine neue Browser-Runtime installiert wird.

Der Desktop-Export liest die unveränderlichen veröffentlichten Git-Blobs und
prüft jede Datei. Persönliche Konfigurationen, Profile und Logs werden nicht
mitverteilt. Ein erfolgreicher neuer Sichttest im laufenden Spiel wird erst
nach einer tatsächlichen Beobachtung dokumentiert. Die automatischen Tests
allein werden nicht als beobachtetes GTA-Spielverhalten bezeichnet.
'''

HISTORY = '''
## 29. Nachtrag v0.1.2 / r23 – 16. November 2026

- Für bestätigte SAPD-/LSPD-Mitglieder bei aktivierter Wanted-Liste ein rotes
  `[GESUCHT | Zahl Wanteds]`-Label über unmittelbar benachbarten Gesuchten ergänzt.
- Feste Grenze von fünf Metern in drei Dimensionen, freie Sicht, gestreamter
  sichtbarer lebender Spieler und vorhandene Nametag-Freigabe erforderlich.
  Keine Anzeige durch Wände, hinter der Kamera oder außerhalb direkter Nähe.
- Wantedzahlen aus frischen bestehenden Online-Daten mit dem tatsächlichen
  gestreamten Spielernamen abgeglichen. Alte Daten und der eigene Spieler
  erzeugen kein zusätzliches Label. Keine Entfernung oder Richtungsanzeige.
- Bestehende Wanted-Tabelle um die aktuelle SA-MP-Spieler-ID hinter dem Namen
  ergänzt, zum Beispiel `Lesch (24)`; Wantedzahl erhalten. Keine API-Account-ID
  als Spieler-ID verwendet, unbekannte Zuordnungen ohne erfundene ID belassen.
- Dynamischen Zielnamen aus dem serverseitigen Detektiv-Ortungsstart gelesen.
  Nach Erfolgsmeldung und neuem Server-Checkpoint einmalig den Bezirk dieses
  Markers per `/r [ORTUNG] Gesuchter Name wurde zuletzt in [Bezirk] geortet.` melden.
- Alte Marker, unvollständige/ungültige Checkpointdaten, doppelte Meldungen und
  abgelaufene Abläufe verwerfen; eigene GPS-Navigation nicht als Ortung behandeln.
- Beide neuen Funktionen an die eigene bestätigte Polizeifraktion der aktuellen
  Verbindung gebunden und laufende Ortungen bei Rollen-/Sitzungsverlust beendet.
- Regressionen für Nähe/Sichtbarkeit/Wantedzuordnung, Ortungsreihenfolge und
  Checkpointadapter ergänzt. Tatsächliche Ergebnisse stehen im r23-Prüfnachweis;
  ein neuer Ingame-Sichttest wird nicht aus automatischen Tests abgeleitet.
- Neue fertige NLBinder.asi mit vollständigem Quellcode und Changelogs als
  NLBinder-v0.1.2-r23 vorgesehen. Vorherige Exporte und Browser-Runtime erhalten.
'''

README = '''# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r23.

Bei aktivierter Wanted-Liste erhalten bestätigte SAPD-/LSPD-Mitglieder rote
Wanted-Labels über unmittelbar benachbarten, sichtbaren Gesuchten. Die feste
Grenze ist fünf Meter mit freier Sicht und vorhandener Nametag-Freigabe.
Nach erfolgreicher Detektiv-Ortung und neuem Server-Checkpoint wird der Bezirk
des Markers einmalig im Polizeifunk gemeldet. Beide Funktionen sind an die
eigene bestätigte Fraktion der aktuellen Verbindung gebunden.
Die Wanted-Tabelle ergänzt aktuelle SA-MP-Spieler-IDs hinter den Namen.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r23)
- [NLBinder.asi](dist/NLBinder-0.1.2-r23/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r23-DE.md)
- [Wanted-Label und Sichtbarkeitsprüfung](include/NearbyWantedLabels.h)
- [Ortung und Funkmeldung](include/PoliceLocateTracker.h)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext zum Posten](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien](docs/FORUM-DATEILISTE.txt)
- [Eigener Installer-/Browsercode](dist/NLBinder-0.1.2-r23/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r23-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r23-validation.json)
- [Änderungen seit r22](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r22...v0.1.2-r23)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der neue Desktop-Ordner enthält vollständige Binder-Quellen, die fertige ASI
und Changelogs. Frühere Desktop-Ordner und die Git-Historie bleiben erhalten.
GTA vollständig schließen, ASI ersetzen und neu starten. Die Browser-Runtime
cef156-r1 bleibt erhalten. Frühere Liveprüfungen sind von den neuen
automatischen r23-Prüfungen und dem noch ausstehenden Sichttest getrennt.
'''


def main():
    (R / 'docs/NLBinder-0.1.2-r23-DE.md').write_text(NOTES, encoding='utf-8', newline='\n')
    (R / 'docs/review/r23-PRUEFUNG-DE.md').write_text(REVIEW, encoding='utf-8', newline='\n')
    (R / 'build').mkdir(exist_ok=True)
    (R / 'build/README-GITHUB-r23.md').write_text(README, encoding='utf-8', newline='\n')
    (R / 'docs/review/README-GITHUB.md').write_text(
        re.sub(r'\]\((docs/|dist/|res/|include/)', r'](../../\1', README), encoding='utf-8', newline='\n')
    path = R / 'docs/review/CHANGELOG-DE.md'
    text = path.read_text(encoding='utf-8-sig')
    if '## 29. Nachtrag v0.1.2 / r23' not in text:
        path.write_text(text.rstrip() + '\n' + HISTORY, encoding='utf-8', newline='\n')
    path = R / 'docs/review/PRUEFUNG-DE.md'
    text = path.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r23' not in text:
        text = text.replace('## Aktueller Stand v0.1.2 / r22', '## Vorheriger Stand v0.1.2 / r22', 1)
        first, _, rest = text.partition('\n')
        text = first + '\n\n## Aktueller Stand v0.1.2 / r23 – 16. November 2026\n\n' + (
            'Wanted-Labels in direkter Nähe und die Polizeifunk-Ortungsmeldung stehen im\n'
            '[r23-Prüfumfang](r23-PRUEFUNG-DE.md) und\n'
            '[Prüfnachweis](r23-validation.json). Frühere Ergebnisse bleiben erhalten.\n') + rest
        path.write_text(text, encoding='utf-8', newline='\n')
    print('Documented nearby police wanted labels, checkpoint-correlated radio and history section 29')


if __name__ == '__main__':
    main()
