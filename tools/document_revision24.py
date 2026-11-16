"""Document the Quick Forum keyboard repair and preserve the full release history."""
from pathlib import Path
import re

R=Path(__file__).resolve().parents[1]

NOTES=r'''# NLBinder v.0.1.2 by perkys & edited by KOBiRO – r24

## Sonderzeichen im Quick Forum

Die Tastatureingabe im über `/forum` geöffneten Quick Forum ist für
Sonderzeichen überarbeitet. Auf einer deutschen Tastatur betrifft das
insbesondere Zeichen mit AltGr wie `[ ] { } @ € \ | ~`.
Die Texteingabe wird als Texteingabe an den Browser weitergereicht;
Tastenkombinationen zum Kopieren und Einfügen bleiben erhalten.

Die Eingabe ist auf das geöffnete, fokussierte Forum beschränkt. ESC behält
seine vorhandene Schließen-Funktion. Das Schreiben im Forum darf keine
Binder-Aktion auslösen. Das vorhandene AFK-/Back-Verhalten wird beibehalten.

Der Fix liegt in der ASI und ihrer Tastaturanbindung. Die Browser-Runtime
**cef156-r1** und ihr Download bleiben erhalten; ein erneutes Herunterladen
des Browserpakets ist für diese Korrektur nicht erforderlich. Persönliche
Browserprofile, Anmeldungen und Binder-Einstellungen gehören nicht zum Paket.

## Weiterhin enthalten

- Wanted-Tabelle mit tatsächlicher SA-MP-Spieler-ID hinter dem Namen,
  etwa `Lesch (24)`, und der bisherigen Wantedzahl.
- Rote Wanted-Labels für bestätigte SAPD-/LSPD-Mitglieder bei aktivierter
  Wanted-Liste, maximal fünf Meter entfernt, sichtbar und mit freier Sicht.
- Einmalige Polizeifunkmeldung nach erfolgreicher Detektiv-Ortung und neuem
  Server-Checkpoint mit dem Bezirk des gesetzten Markers.
- Die bisherigen Jobs, Overlays, Drogen-Timer, Zähler, GPS- und Forumfunktionen.

## Fertige ASI und Quellcode

Der neue Desktop-Ordner `NLBinder-v0.1.2-r24` enthält die fertige `NLBinder.asi`,
den vollständigen veröffentlichten Binder-Quellcode, `CHANGELOG-GESAMT.txt`,
den postbaren `FORUM-CHANGELOG.txt` und den gesonderten
`FORUM-INSTALLATION-QUELLCODE` mit nachvollziehbaren Quellcode-Prüfsummen.
Frühere Desktop-Ordner werden erhalten.

Zum Übernehmen GTA vollständig schließen, die bisherige Binder-ASI ersetzen
und neu starten. Nur eine Binder-ASI laden. Anschließend `/forum` öffnen und
die gewünschten Zeichen in einem Texteingabefeld verwenden.

Version/Credits bleiben v.0.1.2 by perkys & edited by KOBiRO.

[Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r24/docs/review/r24-PRUEFUNG-DE.md) ·
[Prüfnachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r24/docs/review/r24-validation.json)
'''

REVIEW=r'''# Prüfumfang r24 – 16. November 2026

Die Tastaturregressionen prüfen die Weitergabe von Zeichen im geöffneten,
fokussierten Quick Forum, einschließlich der Sonderzeichen über AltGr.
Normale Texteingabe, die vorhandenen Kopieren-/Einfügen-Kombinationen,
Tastenfreigaben und das Beenden der Eingabe müssen erhalten bleiben.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build sowie eine
erfolgreiche neue Suite `ForumKeyboardInputTests` und die Anbindung über
`ForumKeyboardRoutingTests`. Die bisherigen Prüfungen
für SAPD-/LSPD-Wanted-Labels, Spieler-IDs, Ortung und Server-Checkpoints müssen
ebenfalls weiterhin bestehen. Der PE-Nachweis prüft die x86-ASI, bytegleiche
eingebettete Helfer, Lizenz und erhaltene v.0.1.2-Startmeldung.

Der Tastaturfix liegt in der ASI. Der Browserhost und die 236 Dateien des
gepinnten CEF-Browserpakets werden gegen den vorherigen Stand verifiziert.
Die Browser-Runtime cef156-r1 und ihr Archiv bleiben unverändert. Die
veränderte `src/ForumBrowser.cpp` wird ausdrücklich nicht als unveränderte
Quelle behandelt. Die vorherigen Polizei-, GPS-, Zähler- und Overlayprüfungen
sind von den neuen Tastaturregressionen getrennt dokumentiert.

Ein zusätzlicher nativer Browser-Eingabetest verwendet den bestehenden
gepinnten CEF-Host mit einem isolierten anonymen Profil und synthetischen
Texten. Sein Ergebnis ist in `FORUM-KEYBOARD-TEST.json` im Release-Paket und
im r24-Prüfnachweis enthalten. Es wird kein Forumbeitrag abgeschickt.
Der Fall `legacy_altgr_characters` verwendet absichtlich die frühere fehlerhafte
Weiterleitung. Sein `passed: false` belegt die nachgestellte Ursache. Die
korrigierten Fälle müssen vollständig erfolgreich sein. Vor jeder Eingabe
werden der gezeichnete Browserinhalt, der Fokus und die Verarbeitung der
Tastenereignisse geprüft; ausbleibende Eingabe beim Start gilt nicht als
erfolgreich nachgestellter AltGr-Fehler.
Automatische Prüfungen und dieser native Test außerhalb von GTA werden nicht
als beobachteter Ingame-Sichttest bezeichnet. Ein neuer GTA-Live-Test wird
erst nach tatsächlicher Beobachtung dokumentiert.

Der fokussierte Forum-Quellcodeauszug wird aus den aktuellen eigenen Quellen
einschließlich der benötigten Tastaturanbindung neu erstellt und vollständig
mit SHA-256 abgeglichen. Der Desktop-Export liest die unveränderlichen
veröffentlichten Git-Blobs und prüft jede exportierte Datei. Persönliche
Konfigurationen, Profile und Logs werden nicht mitverteilt.
'''

HISTORY=r'''
## 30. Nachtrag v0.1.2 / r24 – 16. November 2026

- Tastatureingabe des über `/forum` geöffneten Quick Forums für Sonderzeichen
  überarbeitet, insbesondere die deutsche AltGr-Eingabe von `[ ] { } @ € \ | ~`.
- Texteingabe von Tastaturkürzeln unterschieden; normales Schreiben sowie
  Kopieren und Einfügen bleiben im vorhandenen Browserfenster vorgesehen.
- Eingabe weiterhin auf das geöffnete, fokussierte Forum beschränkt; die
  vorhandene ESC-Schließen-Funktion und AFK-/Back-Anbindung beibehalten.
- Korrektur in der ASI mit nachvollziehbarem eigenen Tastaturquellcode;
  vorhandene Browser-Runtime cef156-r1 und ihr Download werden beibehalten.
- Tastaturregressionen ergänzt und die bisherigen Prüfungen für Wanted-IDs,
  nahe sichtbare Wanted-Labels und die checkpointgebundene Polizeifunk-Ortung
  weiterhin verlangt. Tatsächliche Ergebnisse stehen im r24-Prüfnachweis.
- Neue NLBinder.asi, vollständige Binder-Quellen, gesamter Changelog und
  postbarer Forumtext als NLBinder-v0.1.2-r24 zusammengestellt. Frühere
  Desktop-Exporte und die veröffentlichte Git-Historie werden erhalten.
'''

POST=r'''Neu in r24: Die Tastatureingabe im über /forum geöffneten Quick Forum ist für Sonderzeichen überarbeitet. Besonders die deutschen AltGr-Zeichen [ ] { } @ € \ | ~ werden als Texteingabe weitergereicht. Normales Schreiben sowie Kopieren und Einfügen bleiben erhalten; ESC schließt das Forum wie gewohnt. Der Fix liegt in der ASI. Die vorhandene Browser-Runtime cef156-r1 muss dafür nicht erneut heruntergeladen werden. Vollständige Quellen, der eigene Forum-/Installer-Quellcode und der gesamte Changelog sind im Paket beziehungsweise im Repository nachvollziehbar enthalten.'''

README=r'''# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, r24.

Die Tastaturweiterleitung des über `/forum` geöffneten Quick Forums ist für
Sonderzeichen überarbeitet, insbesondere deutsche AltGr-Zeichen wie `[ ]`.
Die Korrektur liegt in der ASI; die vorhandene Browser-Runtime cef156-r1 wird
weiterverwendet. Die r23-Funktionen mit aktuellen Spieler-IDs in der
Wanted-Tabelle, nahen sichtbaren Wanted-Labels und Ortungsmeldung bleiben
enthalten.

- [Fertige ASI und Dokumentation](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r24)
- [NLBinder.asi](dist/NLBinder-0.1.2-r24/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r24-DE.md)
- [Tastaturweiterleitung](include/ForumKeyboardInput.h)
- [Quick Forum](src/ForumBrowser.cpp)
- [Gesamt-Changelog](docs/review/CHANGELOG-DE.md), [TXT](docs/CHANGELOG-GESAMT.txt)
- [Forumtext zum Posten](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Forum-Installation und Speicherorte](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien](docs/FORUM-DATEILISTE.txt)
- [Eigener Installer-/Browsercode](dist/NLBinder-0.1.2-r24/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Prüfumfang](docs/review/r24-PRUEFUNG-DE.md), [Ergebnisse](docs/review/r24-validation.json)
- [Nativer Browser-Eingabetest](dist/NLBinder-0.1.2-r24/FORUM-KEYBOARD-TEST.json)
- [Änderungen seit r23](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.2-r23...v0.1.2-r24)
- [Originalvergleich](docs/review/DATEIVERGLEICH.md)

Der neue Desktop-Ordner enthält vollständige Binder-Quellen, die fertige ASI
und Changelogs. Frühere Desktop-Ordner und die Git-Historie bleiben erhalten.
GTA vollständig schließen, ASI ersetzen und neu starten. Automatische Tests,
der native Browser-Eingabetest und tatsächliche
Ingame-Beobachtungen werden im Prüfnachweis getrennt ausgewiesen.
'''

def main():
    (R/'build').mkdir(exist_ok=True)
    for path,text in [('docs/NLBinder-0.1.2-r24-DE.md',NOTES),
                      ('docs/review/r24-PRUEFUNG-DE.md',REVIEW),
                      ('build/README-GITHUB-r24.md',README)]:
        (R/path).write_text(text,encoding='utf-8',newline='\n')
    (R/'docs/review/README-GITHUB.md').write_text(
        re.sub(r'\]\((docs/|dist/|res/|include/|src/)',r'](../../\1',README),encoding='utf-8',newline='\n')
    path=R/'docs/review/CHANGELOG-DE.md'
    text=path.read_text(encoding='utf-8-sig')
    if '## 30. Nachtrag v0.1.2 / r24' not in text:
        path.write_text(text.rstrip()+'\n'+HISTORY,encoding='utf-8',newline='\n')
    path=R/'docs/review/PRUEFUNG-DE.md'
    text=path.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r24' not in text:
        text=text.replace('## Aktueller Stand v0.1.2 / r23','## Vorheriger Stand v0.1.2 / r23',1)
        first,_,rest=text.partition('\n')
        text=first+'\n\n## Aktueller Stand v0.1.2 / r24 – 16. November 2026\n\n'+(
            'Die Quick-Forum-Tastaturkorrektur wird im [r24-Prüfumfang](r24-PRUEFUNG-DE.md)\n'
            'und [Prüfnachweis](r24-validation.json) getrennt von früheren Ergebnissen dokumentiert.\n')+rest
        path.write_text(text,encoding='utf-8',newline='\n')
    path=R/'docs/FORUM-CHANGELOG-v0.1.2.txt'
    text=path.read_text(encoding='utf-8-sig')
    if 'Neu in r24:' not in text:
        text=text.replace('NEU IN DIESEM PAKET\n','NEU IN DIESEM PAKET\n\n'+POST+'\n',1)
        text=text.replace('/releases/tag/v0.1.2-r23','/releases/tag/v0.1.2-r24')
        path.write_text(text,encoding='utf-8-sig',newline='\n')
    path=R/'docs/BUILD-README.md'
    text=path.read_text(encoding='utf-8-sig')
    if '**Quick Forum Keyboard**' not in text:
        text=text.replace('## Building','- **Quick Forum Keyboard** — The in-game `/forum` keyboard bridge supports text input including German AltGr special characters, preserving browser copy/paste shortcuts and Esc-close behavior. The pinned browser runtime cef156-r1 remains in use.\n\n## Building',1)
        path.write_text(text,encoding='utf-8',newline='\n')
    print('Documented Quick Forum special-character input and history section 30')

if __name__=='__main__':main()
