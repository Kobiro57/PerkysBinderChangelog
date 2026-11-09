# NLBinder v.0.1.2 by perkys & edited by KOBiRO

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
