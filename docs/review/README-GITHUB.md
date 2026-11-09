# PerkysBinderChangelog

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

- [Vollständiger Changelog](../../docs/review/CHANGELOG-DE.md), auch [als TXT](../../docs/CHANGELOG-GESAMT.txt)
- [Bedienung und alle Zeiten](../../docs/NLBinder-0.1.1-r19-DE.md)
- [Tatsächlicher r19-Prüfumfang](../../docs/review/r19-PRUEFUNG-DE.md)
- [Maschinenlesbarer r19-Nachweis](../../docs/review/r19-validation.json)
- [Dateivergleich zum Original](../../docs/review/DATEIVERGLEICH.md)
- [Abhängigkeiten und Lizenzen](../../docs/review/DEPENDENCIES.md)
- [Änderungen seit r18](https://github.com/Kobiro57/PerkysBinderChangelog/compare/v0.1.1-r18...v0.1.1-r19)
- [Alle Programmänderungen](https://github.com/Kobiro57/PerkysBinderChangelog/compare/build-dependencies-baseline...v0.1.1-r19)

Vollständige Quellen unter src/, include/, browser/, res/, tests/, tools/ und
third_party/. Originalimport, SDK-Basis und frühere Revisionspatches bleiben
unverändert nachvollziehbar. Keine persönlichen Laufzeitdateien enthalten.

## Fertiger Binder

- [r19-Paket herunterladen](https://github.com/Kobiro57/PerkysBinderChangelog/releases/tag/v0.1.1-r19)
- [NLBinder.asi](../../dist/NLBinder-0.1.1-r19/NLBinder.asi)
- [Installation und Bedienung](../../dist/NLBinder-0.1.1-r19/LIESMICH.md)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r19/BUILD-INFO.txt)
- [ASI und eingebettete Helfer](../../dist/NLBinder-0.1.1-r19/PRUEFBERICHT.json)

GTA vollständig beenden, ASI ersetzen und neu starten. Der Browser wird beim
ersten /forum weiterhin automatisch unter Dokumente eingerichtet. Frühere
Browser-/GPS-Integrationstests bleiben getrennt dokumentiert; ein r19-Live-Test
wird erst nach tatsächlicher Beobachtung als bestanden angegeben.

[Ursprüngliche Beschreibung](../../docs/ORIGINAL-README.md) · [Build-Anleitung](../../docs/BUILD-README.md)
