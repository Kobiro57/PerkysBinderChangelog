"""Document v0.1.2 branding and the inspectable forum installation without changing old notes."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
URL='https://github.com/Kobiro57/PerkysBinderChangelog'
TAG='v0.1.2-r20'
NOTES='''# NLBinder v.0.1.2 by perkys & edited by KOBiRO

## Startmeldung und Urheberhinweis

Beim Einloggen lautet die lokale Startmeldung:

NLBinder v.0.1.2 by perkys & edited by KOBiRO wurde erfolgreich gestartet!

Die bestehenden Farben bleiben erhalten. Hauptmenü und Startprotokoll nutzen
dieselbe zentrale Versions- und Urheberdefinition in include/Version.h.
Der gesamte bisherige Funktionsstand einschließlich des Drogen-Cooldown-/
Nachwirkungs-Overlays mit verschiebbarer Nullwert-Vorschau ist enthalten.

## Fertiges Paket

NLBinder.asi ist die fertige x86-ASI. GTA vor dem Ersetzen vollständig schließen
und danach neu starten. Im neuen Desktop-Ordner NLBinder-v0.1.2 liegen der gesamte
Binder-Quellcode, die ASI und folgende Texte:

- CHANGELOG-GESAMT.txt: die vollständige Änderungshistorie.
- FORUM-CHANGELOG.txt: fertiger Veröffentlichungstext zum Posten im Forum.
- FORUM-INSTALLATION.txt: Ablauf, Speicherorte, Daten und Grenzen der Browser-Einrichtung.
- FORUM-DATEILISTE.txt: alle festgelegten Runtime-Dateien mit Größe und SHA-256.
- FORUM-INSTALLATION-QUELLCODE/: separat zusammengestellte, prüfbare Originalquellen
  der eigenen Browser-Einbindung, des Installers und der verwendeten ZIP-Bibliothek.

Die bisherige Desktop-Quelle bleibt erhalten. Der vollständige Drittanbieter-
Quellcode von Chromium/CEF ist kein Bestandteil des Binder-Pakets; seine Herkunft
und offiziellen Quellcode- und Lizenzverweise stehen in der Installationsdokumentation.

## Was /forum einrichtet

Die Browser-Runtime bleibt der vorhandene festgelegte Stand cef156-r1. v0.1.2
ändert daran nichts. Vorhandene passende Dateien werden weiterverwendet; bei
Bedarf übernimmt der native Installer passende Dateien oder lädt das geprüfte
Paket nach. Die vollständige Dateiliste beschreibt den Runtime-Bestand, nicht
später vom Browser erzeugte Profildateien. Profil, Cookies und temporäre Dateien
sind in der separaten Dokumentation mit ihren jeweiligen Orten beschrieben.

## Prüfung und Einsicht

[Installer und Speicherorte](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/FORUM-INSTALLATION-DE.md),
[Runtime-Dateiliste](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/FORUM-DATEILISTE.txt),
[aktueller Prüfumfang](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/review/r20-PRUEFUNG-DE.md),
[maschineller Nachweis](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/review/r20-validation.json).

Es werden keine persönlichen Browserprofile, Einstellungen, Chatlogs oder
Zugangsdaten mitgeliefert. Die neuen Unterlagen machen den vorhandenen eigenen
Installationscode und die festgelegten Runtime-Dateien nachvollziehbar.
'''
REVIEW='''# Prüfumfang v0.1.2 / r20 – 9. November 2026

Die Programmänderung setzt Version und Credits zentral auf 0.1.2 beziehungsweise
„perkys & edited by KOBiRO“. Startmeldung, Hauptmenü und Startprotokoll verwenden
die zentrale Definition. Die PE-Prüfung sucht zusätzlich die angeforderte vollständige
farbige Startmeldung und den passenden Menütitel in der tatsächlich gebauten ASI.

Die vollständige vorhandene C++-Testsuite und der Release-Build werden für diesen
Stand ausgeführt; der Paketnachweis wird nur aus erfolgreichen Logs erzeugt.
Der native Reconnect-Worker wird nach dem finalen Build separat geprüft.
Ressourcen 300/301 müssen mit den gebauten Helfern bytegleich sein; Ressource 302
muss die ZIP-Bibliotheks-Lizenz enthalten. PE32/x86 und externe Imports werden geprüft.

Die Browser-/GPS-/Installer-/Reconnect-Helferquellen und die Drogen-Timer-Komponenten
werden gegen den veröffentlichten r19-Commit fc32a3de870a00c48630bb1b5a4f6071f8d5808b
auf unveränderten Inhalt geprüft. Eine aktualisierte Versionsmeldung ist keine
neue Runtime oder neue Browserinstallation. Frühere aufwendige Browser-Download-
und GPS-Integrationstests werden für diesen Stand nicht als erneut ausgeführt
behauptet; ihre Nachweise bleiben historisch erhalten.

Der neue fokussierte Quellcodeordner wird aus den aktuellen eigenen Originalquellen
und der bereits verwendeten ZIP-Bibliothek erstellt. SHA-256-Listen und ein Manifest
beschreiben seine Nutzdateien. Runtime-Dateiliste und Downloadmanifest stimmen
mit allen 236 festgelegten Runtime-Dateien überein. Der Desktop-Export wird nach
Veröffentlichung aus dem konkreten Git-Commit gelesen und vollständig verifiziert.
Der fokussierte Ordner und die Texte werden dabei ebenfalls aus Git-Blobs übernommen.

Ein neuer GTA-Live-Test wird vor einer tatsächlichen Beobachtung nicht behauptet.
Persönliche Profile, Zugangsdaten und lokale Chatlogs sind nicht Teil der Veröffentlichung.

[Bedienung](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/NLBinder-0.1.2-r20-DE.md) ·
[Installationsdaten](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/FORUM-INSTALLATION-DE.md) ·
[Gesamt-Changelog](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/docs/review/CHANGELOG-DE.md)
'''
README='''# NLBinder – PerkysBinderChangelog

Aktueller Stand: **NLBinder v.0.1.2 by perkys & edited by KOBiRO**, interne Revision r20.
Originalimport, bisherige Änderungen und frühere Revisionspatches bleiben erhalten.

Version und Urheberhinweise sind in Startmeldung, Hauptmenü und Startprotokoll
vereinheitlicht. Der bisherige Funktionsstand ist enthalten. Zusätzlich liegen
ein fertiger Forum-Veröffentlichungstext, ein vollständiger Gesamt-Changelog
und die gesondert prüfbaren Quellen und Dateilisten der /forum-Einrichtung bei.

## Herunterladen und prüfen

- [Fertiges v0.1.2-Paket](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.2-r20)
- [NLBinder.asi](dist/NLBinder-0.1.2-r20/NLBinder.asi)
- [Bedienung](docs/NLBinder-0.1.2-r20-DE.md)
- [Fertiger Text zum Posten im Forum](docs/FORUM-CHANGELOG-v0.1.2.txt)
- [Vollständiger Changelog](docs/review/CHANGELOG-DE.md), auch [als TXT](docs/CHANGELOG-GESAMT.txt)
- [Was /forum installiert und wo Daten liegen](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien mit Größe und SHA-256](docs/FORUM-DATEILISTE.txt)
- [Fokussierter eigener Installer-/Browser-Quellcode](dist/NLBinder-0.1.2-r20/FORUM-INSTALLATION-QUELLCODE/README.md)
- [Tatsächlicher aktueller Prüfumfang](docs/review/r20-PRUEFUNG-DE.md)
- [Maschineller Prüfnachweis](docs/review/r20-validation.json)
- [ASI und eingebettete Helfer](dist/NLBinder-0.1.2-r20/PRUEFBERICHT.json)
- [Änderungen seit r19](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r19...v0.1.2-r20)
- [Dateivergleich zum Original](docs/review/DATEIVERGLEICH.md)
- [Abhängigkeiten und Lizenzen](docs/review/DEPENDENCIES.md)

Alle Binder-Quellen stehen unter src/, include/, browser/, res/, tests/, tools/
und third_party/. Der gesonderte Prüfordner enthält die eigenen Installations-
und Browserquellen samt Herkunfts-/Lizenzangaben; vollständiger Chromium-/CEF-
Upstreamcode wird nicht als beigefügter Binder-Code ausgegeben.

GTA vollständig schließen, ASI ersetzen und neu starten. Der festgelegte
Browserstand cef156-r1 wird weiterverwendet. Persönliche Laufzeitdaten werden
nicht mitgeliefert. Ein noch nicht erfolgter neuer GTA-Live-Test wird nicht behauptet.

[Ursprüngliche Beschreibung](docs/ORIGINAL-README.md) · [Build-Anleitung](docs/BUILD-README.md)
'''
HISTORY='''
## 26. Nachtrag v0.1.2 / r20 – 9. November 2026

- Lokale Startmeldung auf „NLBinder v.0.1.2 by perkys & edited by KOBiRO wurde
  erfolgreich gestartet!“ geändert; bestehende Farben beibehalten.
- Version und Credits zentral definiert und konsistent auch im Hauptmenü und
  Startprotokoll verwendet. Bisherigen Funktionsstand einschließlich r19 erhalten.
- Neuen vollständigen Desktop-Export NLBinder-v0.1.2 mit kompletter Binder-Quelle,
  fertiger NLBinder.asi und Gesamt-Changelog vorgesehen; ältere Ordner erhalten.
- Postbaren Forum-Changelog, ausführliche Beschreibung der /forum-Einrichtung,
  Speicherorte und vollständige Runtime-Dateiliste mit Größen und SHA-256 ergänzt.
- Fokussierten Prüfordner mit den eigenen Browser-/Installer-Originalquellen,
  Protokollen, Buildhilfen, Downloadmanifest und ZIP-Bibliothek zusammengestellt.
- Eigenen Binder-Code klar von vorcompilierter Chromium-/CEF-Runtime abgegrenzt;
  Herkunft, offizielle Quellcode-Verweise und Lizenzangaben dokumentiert.
- Der Browserstand cef156-r1 bleibt unverändert. Persönliche Browserprofile,
  Einstellungen, Zugangsdaten und Chatlogs werden nicht mitverteilt.
- Tatsächlich ausgeführte automatisierte Prüfungen und ASI-Prüfsummen getrennt
  dokumentiert; keinen noch nicht erfolgten GTA-Live-Test behauptet.
'''

def main():
    (R/'docs/NLBinder-0.1.2-r20-DE.md').write_text(NOTES,encoding='utf-8')
    (R/'docs/review/r20-PRUEFUNG-DE.md').write_text(REVIEW,encoding='utf-8')
    (R/'build').mkdir(exist_ok=True)
    (R/'build/README-GITHUB-r20.md').write_text(README,encoding='utf-8')
    (R/'docs/review/README-GITHUB.md').write_text(
        re.sub(r'\]\((docs/|dist/|res/)',r'](../../\1',README),encoding='utf-8')
    review=R/'docs/review/PRUEFUNG-DE.md'
    text=review.read_text(encoding='utf-8-sig')
    if '## Aktueller Stand v0.1.2 / r20' not in text:
        text=text.replace('## Aktueller Stand r19','## Vorheriger Stand r19',1)
        first,separator,rest=text.partition('\n')
        text=first+'\n\n## Aktueller Stand v0.1.2 / r20 – 9. November 2026\n\n'+(
            'Version, Urheberhinweise und transparente Forum-Einrichtung sind im\n'
            '[r20-Prüfumfang](r20-PRUEFUNG-DE.md) und\n'
            '[maschinellen Nachweis](r20-validation.json) dokumentiert.\n'
            'Die historischen Testergebnisse bleiben nachstehend erhalten.\n')+rest
        review.write_text(text,encoding='utf-8')
    path=R/'docs/review/CHANGELOG-DE.md'
    text=path.read_text(encoding='utf-8-sig')
    if '## 26. Nachtrag v0.1.2 / r20' not in text:
        path.write_text(text.rstrip()+'\n'+HISTORY,encoding='utf-8')
    print('Documented v0.1.2 credits, forum disclosure, new Desktop export and full history section 26')

if __name__=='__main__':main()
