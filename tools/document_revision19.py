"""Write the current r19 usage and review pages without rewriting earlier release notes."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]

NOTES='''# NLBinder v.0.1.1 by KOBiRO – r19

## Use Cooldown & Nachwirkung

Unter Online → Overlay-Einstellungen → Dauerhaft → Spielinformationen steht
direkt unter „Ausdauer anzeigen“ der Schalter „Use Cooldown & Nachwirkung anzeigen“.
Die Anzeige ist zunächst ausgeschaltet. Aktivieren startet die Erkennung für
nachfolgende eigene Einnahmen. Deaktivieren entfernt laufende Timer und ausstehende
Bereit-Meldungen.

Der Binder erkennt die vollständige Aktionszeile „Name nimmt Droge zu sich.“ und
vergleicht den Namen mit dem aktuellen lokalen Spielernamen. Fremde Einnahmen,
zitierte Nachrichten und normaler Spielerchat starten keine eigenen Timer.
Farbcodes werden bereinigt. Der Name ist nicht auf einen bestimmten Nutzer festgelegt.

| Droge | Cooldown | Abklingzeit |
| --- | ---: | ---: |
| Kokain | 120 s | 120 s |
| Heroin | 120 s | 60 s |
| Speed | 180 s | 180 s |
| Ecstasy | 180 s | 60 s |
| Cannabis | keine | 40 s |
| Adrenalin | 45 s | keine |
| Morphium | 45 s | 45 s |

„Ectasy“ wird ebenfalls als Ecstasy erkannt. Cooldown und Abklingzeit beginnen
gleichzeitig bei der Einnahme. Die verbleibenden Zeiten werden in Sekunden
aufgerundet und nutzen eine monotone Uhr; schwankende Bildrate verlängert sie nicht.
Mehrere Drogen bleiben unabhängig voneinander im selben Overlay sichtbar.

Beispiel für Kokain, mit untereinander stehenden Zeilen:

```text
Kokain
Cooldown: 120 s
Abklingzeit: 120 s
```

Bei Cannabis entfällt die Cooldown-Zeile, bei Adrenalin die Abklingzeit-Zeile.
Ein tatsächlich vorhandener Cooldown erzeugt beim Ablauf genau eine lokale
NL-BINDER-Meldung mit der wieder konsumierbaren Droge. Sie wird nicht an den
Serverchat geschickt. Cannabis erzeugt keine Cooldown-Bereit-Meldung.

Ohne aktive Einnahme bleibt das Overlay unsichtbar. Beim vorhandenen
„Move-Modus starten“ erscheint stattdessen eine Vorschau mit „Cooldown: 0 s“
und „Abklingzeit: 0 s“. Mit der Maus verschieben, mit ESC die Position speichern.
Danach verschwindet die Vorschau bis zur nächsten Einnahme. Hintergrund, Schrift,
Skalierung und Farben verwenden denselben Stil wie die übrigen dauerhaften Overlays.
Die Position ist unabhängig von Ausdauer und anderen Anzeigen gespeichert.

Bei Verbindungsende oder Spielerwechsel werden die laufenden Einnahmen verworfen.
Nach einem Relog erfindet der Binder keine bereits vergangenen Konsumereignisse.

## Installation

GTA vollständig schließen, NLBinder.asi ersetzen und das Spiel neu starten.
Browserdateien werden weiterhin bei Bedarf automatisch unter Dokumente/GTA San
Andreas User Files/NLBinder/Browser angelegt. Kein Browserordner muss mitgesendet werden.
Der bisherige Quellcode, alle früheren Änderungen und vollständiger TXT-Changelog
sind enthalten. Im Desktop-Export wird der bisherige r18-Ordner als Sicherung erhalten.

## Prüfung

[Aktueller Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-PRUEFUNG-DE.md),
[maschineller Nachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-validation.json),
[gesamter Changelog](CHANGELOG-GESAMT.txt).
Die automatisierten Prüfungen belegen die getesteten Abläufe. Eine noch nicht
erfolgte Sichtprüfung dieser Erweiterung im laufenden GTA wird nicht behauptet.
'''

REVIEW='''# Prüfumfang r19 – 9. November 2026

r19 ergänzt das eigene Drogen-Cooldown-/Nachwirkungs-Overlay. Alle Zeitangaben
der sieben Drogen werden in Sekunden geprüft. Die Tests behandeln den dynamischen
lokalen Namen, Ablehnung fremder oder zitierter Zeilen, Ecstasy/Ectasy, gleichzeitige
Timer, Grenzzeitpunkte, doppelte Zeilen und einmalige lokale Bereit-Meldungen.

Die Settings-Prüfungen behandeln Default-Aus, JSON-Roundtrip und ungültige
Koordinaten. Der vorhandene ImGui-Test prüft das neue mehrzeilige Panel,
gemeinsamen Stil, unabhängiges Verschieben und Speichern über den vorhandenen
Move-Modus. Vorschau ohne aktive Einnahme und spätere Unsichtbarkeit werden
gesondert berücksichtigt.

Der [maschinelle Nachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r19-validation.json) entsteht erst aus erfolgreichen
Gesamttests und Release-Build. Der PE-Nachweis prüft x86-DLL, eingebettete Helfer
300/301 auf Bytegleichheit mit den gebauten EXE-Dateien, eingebettete ZIP-Lizenz
302 sowie fehlende externe C++-Debug-Runtime und kein libcef im Spielprozess.
Der native Reconnect-Worker wird nach dem finalen Build separat geprüft.

Browser-, GPS-, Installer- und Reconnect-Helferquellen werden vor dem Paketbau
gegen den veröffentlichten r18-Stand b3ca24e43a8603d98215ca6b559adad16f3d41fe
auf unveränderten Inhalt geprüft. Der Browserstand bleibt cef156-r1. Die früheren
[Browser-/GPS-Nachweise](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r18-validation.json) und der genau abgegrenzte
[vorläufige Live-Test](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/r18-live-validation.json) bleiben historisch erhalten.
Diese aufwendigen Download-/Browser-Integrationstests werden für r19 nicht als
erneut ausgeführt dargestellt.

Kein r19-Sichttest im laufenden Spiel wird vor einer tatsächlichen Bestätigung
behauptet. Keine persönlichen Profile, Einstellungen, Chatlogs oder Zugangsdaten
gehören zur Veröffentlichung.

[Bedienung und Zeiten](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/NLBinder-0.1.1-r19-DE.md) ·
[Vollständiger Changelog](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.1-r19/docs/review/CHANGELOG-DE.md)
'''

README='''# PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.1 by KOBiRO, r19**.
Ursprüngliche Urheberhinweise und bisherige Veröffentlichungen bleiben erhalten.

r19 ergänzt unter Online → Overlay-Einstellungen → Dauerhaft direkt unter
Ausdauer ein aktivierbares „Use Cooldown & Nachwirkung“-Overlay. Eigene
Konsumaktionen starten unabhängige Countdown-Zeilen in Sekunden; die Abklingzeit
beginnt parallel zum Cooldown. Nach dem Cooldown kommt einmalig eine lokale
Bereit-Meldung. Die Anzeige nutzt denselben Stil und Move-Modus mit ESC wie die
vorhandenen Overlays. Ohne aktive Timer bleibt sie unsichtbar; im Move-Modus
hilft eine Vorschau beim Positionieren.

## Prüfung und Quellcode

- [Vollständiger Changelog](docs/review/CHANGELOG-DE.md), auch [als TXT](docs/CHANGELOG-GESAMT.txt)
- [Bedienung und alle Zeiten](docs/NLBinder-0.1.1-r19-DE.md)
- [Tatsächlicher r19-Prüfumfang](docs/review/r19-PRUEFUNG-DE.md)
- [Maschinenlesbarer r19-Nachweis](docs/review/r19-validation.json)
- [Dateivergleich zum Original](docs/review/DATEIVERGLEICH.md)
- [Abhängigkeiten und Lizenzen](docs/review/DEPENDENCIES.md)
- [Änderungen seit r18](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r18...v0.1.1-r19)
- [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r19)

Vollständige Quellen unter src/, include/, browser/, res/, tests/, tools/ und
third_party/. Originalimport, SDK-Basis und frühere Revisionspatches bleiben
unverändert nachvollziehbar. Keine persönlichen Laufzeitdateien enthalten.

## Fertiger Binder

- [r19-Paket herunterladen](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r19)
- [NLBinder.asi](dist/NLBinder-0.1.1-r19/NLBinder.asi)
- [Installation und Bedienung](dist/NLBinder-0.1.1-r19/LIESMICH.md)
- [Build- und Prüfangaben](dist/NLBinder-0.1.1-r19/BUILD-INFO.txt)
- [ASI und eingebettete Helfer](dist/NLBinder-0.1.1-r19/PRUEFBERICHT.json)

GTA vollständig beenden, ASI ersetzen und neu starten. Der Browser wird beim
ersten /forum weiterhin automatisch unter Dokumente eingerichtet. Frühere
Browser-/GPS-Integrationstests bleiben getrennt dokumentiert; ein r19-Live-Test
wird erst nach tatsächlicher Beobachtung als bestanden angegeben.

[Ursprüngliche Beschreibung](docs/ORIGINAL-README.md) · [Build-Anleitung](docs/BUILD-README.md)
'''

HISTORY='''
## 25. Nachtrag r19 – 9. November 2026

- Unter Online → Overlay-Einstellungen → Dauerhaft → Spielinformationen direkt
  unter „Ausdauer anzeigen“ den Schalter „Use Cooldown & Nachwirkung anzeigen“ ergänzt.
- Nur die vollständige eigene Konsum-Aktionszeile anhand des aktuellen lokalen
  Spielernamens auswerten. Fremde Namen, normaler Spielerchat und zitierte Texte
  starten keine Timer. Ecstasy und Ectasy bezeichnen denselben Eintrag.
- Cooldown/Abklingzeit in Sekunden: Kokain 120/120, Heroin 120/60,
  Speed 180/180, Ecstasy 180/60, Cannabis ohne Cooldown/40,
  Adrenalin 45/ohne Abklingzeit und Morphium 45/45.
- Beide Zeiten beginnen parallel bei der Einnahme; unabhängige Drogen können
  gleichzeitig laufen. Monotone Zeit und aufgerundete Restsekunden verhindern
  eine Verlängerung durch schwankende Bildraten.
- Cooldown und Abklingzeit pro Droge untereinander im selben Overlay anzeigen.
  Nicht vorhandene Timer weglassen. Nach Ablauf eines vorhandenen Cooldowns
  einmalig lokal melden, welche Droge wieder konsumiert werden kann.
- Gleichen Stil, Farben, Skalierung und bestehenden Move-Modus mit ESC-Speichern
  nutzen; Position unabhängig von Ausdauer und anderen Overlays sichern.
- Ohne aktive Einnahme im normalen Spiel unsichtbar. Im Move-Modus nur eine
  Vorschau mit „Cooldown: 0 s“ und „Abklingzeit: 0 s“ anzeigen; ESC speichert
  und versteckt sie wieder bis zum nächsten aktiven Konsum.
- Deaktivieren, Verbindungsende und Spielerwechsel entfernen laufende Timer
  und ausstehende Bereit-Meldungen. Keine nachträglichen Ereignisse erfinden.
- Gezielte Timer-, Settings- und Overlay-/Move-Prüfungen ergänzt. Tatsächliche
  Prüfergebnisse stehen im separaten r19-Nachweis; früheren Browser-/GPS-Testumfang
  nicht als neu ausgeführten r19-Live-Test darstellen.
'''

def main():
    (R/'docs/NLBinder-0.1.1-r19-DE.md').write_text(NOTES,encoding='utf-8')
    (R/'docs/review/r19-PRUEFUNG-DE.md').write_text(REVIEW,encoding='utf-8')
    (R/'build').mkdir(exist_ok=True)
    (R/'build/README-GITHUB-r19.md').write_text(README,encoding='utf-8')
    (R/'docs/review/README-GITHUB.md').write_text(
        re.sub(r'\]\((docs/|dist/|res/)',r'](../../\1',README),encoding='utf-8')
    review=R/'docs/review/PRUEFUNG-DE.md'
    text=review.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand r19' not in text:
        text=text.replace('## Aktueller Stand r18','## Vorheriger Stand r18',1)
        first,separator,rest=text.partition('\n')
        text=first+'\n\n## Aktueller Stand r19 – 9. November 2026\n\n'+(
            'Der aktuelle Prüfumfang des Drogen-Cooldown-/Nachwirkungs-Overlays\n'
            'steht in [r19-PRUEFUNG-DE.md](r19-PRUEFUNG-DE.md), die Ergebnisse\n'
            'in [r19-validation.json](r19-validation.json). Frühere Prüfungen\n'
            'bleiben nachstehend mit ihrem tatsächlich beobachteten Stand erhalten.\n')+rest
        review.write_text(text,encoding='utf-8')
    p=R/'docs/review/CHANGELOG-DE.md';text=p.read_text(encoding='utf-8-sig')
    if '## 25. Nachtrag r19' not in text:p.write_text(text.rstrip()+'\n'+HISTORY,encoding='utf-8')
    print('Documented r19 usage, exact scope and full history section 25')

if __name__=='__main__':main()
