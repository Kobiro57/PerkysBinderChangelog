# Hinweise zur Prüfung durch die Projektleitung

## Aktueller Stand r15 – 30. Oktober 2026

[Bedienung](../NLBinder-0.1.1-r15-DE.md),
[r15-Nachtrag](CHANGELOG-DE.md#21-nachtrag-r15--30-oktober-2026) und
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt) beschreiben den Stand.

### Belegte Ursache und Korrektur

Eine rein lesende Live-Aufzeichnung mit der installierten r14-ASI erfasste
einen vom Nutzer ausgelösten Explosionstod. Native HP: 8.000.094 vor dem
Ereignis, zuletzt 7.999.945 und 8.000.000, nach etwa drei Sekunden 8.000.100.
Der native Pedzustand blieb 1, SA-MP Active blieb 1 und Wasted blieb 0.
Diese Werte belegen den Schutzaufschlag und erklären das Ausbleiben der
bisherigen Todeserkennung. Die frühere Annahme eines zwingenden normalen
GTA-/SA-MP-Todesflags trifft auf diesen Serverablauf nicht zu.

`NativePlayerHealth.h` dekodiert den belegten Bereich vor der weiteren
Auswertung. Geschützte physische Todesübergänge benötigen anschließend
einen neuen eigenen Server-Todesstand. Höchstens zwei eigene Statistikabrufe
vermeiden eine dauerhafte Abfrageschleife. Die Gesamtzahl wird absolut
importiert; Sessioneintrag und lokale Todesmeldung entstehen nur einmal.
Kaltes Einloggen, Friedhofs-Relog und reine Korrekturen alter gespeicherter
Werte sind keine neuen Ereignisse. Friedhofs-Chat wird nicht dafür ausgewertet.

Der Damagelog verwendet gemessene logische HP-/Westenverluste und tatsächliche
serverseitige HP-/Westenänderungen. TakeDamage-Rohbeträge sind kein zusätzlicher
Verlust. Die übertragene uint8-HP kann unterlaufen und wird nicht als
HP-Ersatz verwendet. Überkill wird pro Pool auf den vorherigen Rest begrenzt.

Die Live-Aufzeichnung bleibt privat im lokalen Buildordner. Nur anonymisierte
Messwerte und Reihenfolgen sind in Regressionstests enthalten; keine
Spielerkennung, Prozessadressen, Sitzungschatlogs oder Kontodateien werden
veröffentlicht. Der nachfolgende Live-Test der r15-ASI ist vom Ursachenbeleg mit r14 zu
unterscheiden: Der Nutzer bestätigte einen korrekten Explosionstod mit einer
lokalen Meldung, anschließend einen Friedhofs-Relog ohne Doppelzählung. Der
eigene Serverstand erhöhte sich nur beim Tod um eins.
[Datensparsamer Live-Prüfnachweis](r15-live-validation.json).

### Ausgeführte Prüfungen für r15

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Erfolgreiche Prüfungen: DamageLogRegression: 210, ActionExecutor: 143, Reconnect: 116, SAMPBridge: 563, StartupStatsSync: 367, StatsDialogRPCAdapter: 727, JobSkillsImport: 17, CombatChatParser: 190, CombatChatBridge: 238, native_reconnect_helper: 83.

Die Friedhofszeile löst keine eigene Todeszählung mehr aus.
Integrationstests prüfen echte Alive-zu-Dead-Übergänge, lokale Meldung
und Baselineübernahme bei Login/Relog ohne rückwirkenden Zusatz-Tod.
Die Damagelog-Regressionssuite prüft Fallschaden und Explosionsübergänge
getrennt von den Umweltfiltern des Hitsounds. Bestehende Statistik- und
Jobskill-Abläufe werden weiterhin gemeinsam geprüft.
Die Ausgabe des finalen Builds heißt NLBinder.asi. Die geprüfte Datei
umfasst 18.510.848 Bytes; SHA-256:
`9d2139f9889fe5368030f505e515575ef0acbb83d374046d42dddd3aff929018`.

PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der
eingebettete Reconnect-Helfer stimmt mit dem frisch geprüften Helfer
überein. Die Helferprüfung nutzt harmlose Testprozesse; das echte Spiel
und der installierte Launcher werden dabei nicht gestartet oder beendet.

Der vollständige TXT-Changelog umfasst die Abschnitte 1–21 und wird
zusammen mit der ASI ausgeliefert.
Separater ASI-Stand und ZIP-Inhalt stimmen bytegleich überein. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, den vollständigen
TXT-Changelog, 43 explizite leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r15-validation.json) ergänzen die Paketnachweise.
Die neue r15-ASI wurde zusätzlich im Spiel geprüft: Explosionstod mit genau einer bestätigten Todesmeldung und einem zusätzlichen Server-Tod, anschließend Friedhofs-Relog ohne weitere Zählung. Der Nutzer führte die Spielaktionen aus und bestätigte beide Ergebnisse. Beschuss und Fallschaden wurden automatisiert geprüft; ein separater Live-Test dieser beiden Ursachen wird nicht behauptet.

## Historischer Prüfstand r14 – 30. Oktober 2026

Der aktuelle Stand ist **NLBinder v.0.1.1 by KOBiRO, r14**.
[Bedienung](../NLBinder-0.1.1-r14-DE.md),
[r14-Nachtrag](CHANGELOG-DE.md#20-nachtrag-r14--30-oktober-2026) und
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt) beschreiben den Umfang.

### Verhalten und prüfbarer Umfang

Der Binder bestätigt zunächst eine eigene lebendige Spielphase: derselbe
Spieler muss gespawnt, steuerbar und mit positiven gültigen Lebenswerten
mindestens 750 ms über drei Messungen beobachtet werden. Erst danach kann
ein Übergang zu 0 bzw. negativen nativen HP, GTA-DIE/DEAD oder SA-MP-Wasted
einen Tod zählen. Ein positiver Schutzwert oder ein hoher Schadensbetrag
allein gilt nicht als Tod.

Zusätzlich wird die echte eigene Client-Todesmeldung ausgewertet.
Für den unterstützten DL-R1-Client wurde der Weg von
CLocalPlayer::WastedNotification (RVA 0x5660) zu RPC 53 mit
reason:uint8 und killer:uint16 lokal geprüft. Der Hook liest und
restauriert den Datenstrom und lässt die Meldung unverändert passieren.
Er erfasst nur eine zuvor bestätigte eigene Spielphase; die Zählung und
lokale Ausgabe erfolgen im Spielthread. Dadurch bleibt eine Todesmeldung
erhalten, wenn der nächste Messdurchlauf bereits einen neuen Friedhofs-Ped
oder wieder positive HP vorfindet. Ein Verbindungs- oder Accountwechsel
verwirft alte ausstehende Meldungen.

Login, Relog und ein bereits toter Spieler beginnen als Ausgangslage ohne
zusätzlichen Tod und ohne neue lokale Todesmeldung. Friedhofs-Chat ist
vollständig als Zählquelle entfernt. Kurze gesund aussehende Zwischenphasen
geben die Zählung nicht wieder frei; dafür ist erneut die bestätigte,
steuerbare lebendige Spielphase erforderlich. Wiederholte native Signale
des gleichen Todes erzeugen nur einen Eintrag.

Die eigene Statistik bleibt die Quelle der absoluten Gesamtstände.
Ein Login-Import erzeugt kein künstliches Todesereignis. Ein zeitnah vor
der nativen Todesbestätigung eingetroffener neuer Serverstand wird mit
dieser Bestätigung abgeglichen, damit der Gesamtstand nicht doppelt steigt.
Die manuelle Statistik-Synchronisation setzt die Lebenssperre nicht zurück.
Einstellungen und lokale Todesmeldung bleiben erhalten.

Der r13-Damagelog mit TakeDamage-RPC 115, tatsächlichen Client-Syncwerten nur
bei unmessbaren nativen Werten, Restwertbegrenzung, Quellen-/Poolabgleich und
getrennter Hitsound-Verarbeitung bleibt unverändert. Seine Beschreibung und
Grenzen stehen im [r13-Nachtrag](CHANGELOG-DE.md#19-nachtrag-r13--30-oktober-2026).
Ein echter Ingame-Test auf dem konkreten Server wird nicht behauptet.

### r14-Prüfpfade

| Bereich | Implementierung | Tests / Nachweise |
| --- | --- | --- |
| Todeslebenszyklus und entfernte Chatquelle | [LocalDeathLifecycle.h](../../include/LocalDeathLifecycle.h), [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [CombatChatParser.cpp](../../src/CombatChatParser.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [CombatChatBridgeTests.cpp](../../tests/CombatChatBridgeTests.cpp), [CombatChatParserTests.cpp](../../tests/CombatChatParserTests.cpp) |
| Unveränderter Damagelog | [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [DamageLogRegressionTests.cpp](../../tests/DamageLogRegressionTests.cpp) |
| ASI und Paket | [verify_revision14_artifact.py](../../tools/verify_revision14_artifact.py), [package_revision14.ps1](../../tools/package_revision14.ps1) | PE32/x86, Ressource 300, ASI-/Paketprüfsummen |

### Zugehörige finale Paketnachweise

- [NLBinder.asi](../../dist/NLBinder-0.1.1-r14/NLBinder.asi)
- [Vollständiges ZIP](../../dist/NLBinder-v0.1.1-KOBiRO-r14.zip)
- [Gesamter TXT-Changelog](../../dist/NLBinder-0.1.1-r14/CHANGELOG-GESAMT.txt)
- [SHA-256](../../dist/NLBinder-0.1.1-r14/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r14/BUILD-INFO.txt)
- [PE-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r14/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r14/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r14

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Erfolgreiche Prüfungen: DamageLogRegression: 152, ActionExecutor: 143, Reconnect: 116, SAMPBridge: 563, StartupStatsSync: 367, StatsDialogRPCAdapter: 727, JobSkillsImport: 17, CombatChatParser: 190, CombatChatBridge: 238, native_reconnect_helper: 83.

Die Friedhofszeile löst keine eigene Todeszählung mehr aus.
Integrationstests prüfen echte Alive-zu-Dead-Übergänge, lokale Meldung
und Baselineübernahme bei Login/Relog ohne rückwirkenden Zusatz-Tod.
Die Damagelog-Regressionssuite prüft Fallschaden und Explosionsübergänge
getrennt von den Umweltfiltern des Hitsounds. Bestehende Statistik- und
Jobskill-Abläufe werden weiterhin gemeinsam geprüft.
Die Ausgabe des finalen Builds heißt NLBinder.asi. Die geprüfte Datei
umfasst 18.514.432 Bytes; SHA-256:
`91b1b816e67acec1993e06c03ba8e98216535169293eeae543a054c564a393ab`.

PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der
eingebettete Reconnect-Helfer stimmt mit dem frisch geprüften Helfer
überein. Die Helferprüfung nutzt harmlose Testprozesse; das echte Spiel
und der installierte Launcher werden dabei nicht gestartet oder beendet.

Der vollständige TXT-Changelog umfasst die Abschnitte 1–20 und wird
zusammen mit der ASI ausgeliefert.
Separater ASI-Stand und ZIP-Inhalt stimmen bytegleich überein. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, den vollständigen
TXT-Changelog, 43 explizite leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r14-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test auf dem konkreten Server steht aus.

## Historischer Prüfstand r13 – 30. Oktober 2026

Der aktuelle Stand ist **NLBinder v.0.1.1 by KOBiRO, r13**.
[Bedienung](../NLBinder-0.1.1-r13-DE.md),
[r13-Nachtrag](CHANGELOG-DE.md#19-nachtrag-r13--30-oktober-2026) und
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt) dokumentieren diesen Stand.

### Verhalten und prüfbarer Umfang

Auf Wunsch des Benutzers wird die eigene Friedhofsmeldung wieder als
maßgeblicher Auslöser der Todeszählung ausgewertet. Ein nativer 0-HP-Zustand
erhöht den Todeszähler nicht mehr. Die lokale Todesbenachrichtigung
bleibt an den erkannten eigenen Tod gebunden. Diese Änderung ersetzt das
in r12 dokumentierte Verhalten, bei dem die Friedhofszeile ignoriert wurde.

Wiederholte Friedhofsbestätigungen innerhalb von fünf Sekunden zählen nicht
erneut. Ist der neue Gesamt-Todesstand bereits aus der eigenen Statistik
übernommen, bestätigt die Friedhofszeile diesen Stand und erhöht ihn nicht
noch einmal. Das gilt auch bei der ersten automatischen Synchronisation.

Der Damagelog verarbeitet zusätzlich echte eingehende Umwelt-/Explosions-
Ereignisse aus dem clientseitig gesendeten TakeDamage-RPC 115. Das führende
Bool `true` kennzeichnet eingehenden Schaden; ein Angreiferwert 65535 ist bei
Umweltschaden erlaubt. Der Schadenbetrag aus dem Payload wird unverändert
verwendet und nicht nochmals skaliert. Normale ausgehende Treffer werden
nicht als eigener erlittener Schaden ausgegeben.

Sind native HP-/Westenwerte unmessbar, kann der Binder ersatzweise die
wirklich vom Client gesendeten Werte aus Onfoot-/Fahrer-/Passagier-Sync
(Pakete 207/200/211) lesen. Diese Ergänzung gilt nur für die unmessbaren
Werte und verlangt denselben Ped, Modus und gegebenenfalls dasselbe Fahrzeug
sowie ein höchstens 500 ms altes Paket. Es wird kein Schutzwert durch einen
geratenen Zahlenoffset dekodiert.

Der Fallback wird auf die bekannten verbleibenden HP-/Westenwerte begrenzt.
Ohne vertrauenswürdigen Reststand wartet das Ereignis höchstens 1500 ms auf
echte Sync-/Serverwerte; ohne diese Basis wird keine unbestätigte Überkillzahl
ausgegeben. Echte Server-HP-/Westenwerte aus RPC 14/66 erneuern das Budget auch
bei unverändertem sichtbarem Stand. Quellfamilien verhindern, dass vorheriger
Gewehrschaden einen späteren Fall mit gleichem Betrag verschluckt.
Gemessene HP- und Westenanteile werden getrennt zugeordnet. Eine echte
HP-Heilung entfernt alte HP-Zuordnungen in beiden Abgleichsrichtungen;
eine reine Westenheilung erhält eine ausstehende Fall-HP-Zuordnung.

Native Messung, Serverwerte und TakeDamage-Ereignis werden im kurzen
Abgleichsfenster zusammengeführt. Ein Guthaben verfällt erst nach mehr als
750 ms und mehr als drei tatsächlichen Zwischenpolls. Ein einzelner langsamer
Frame verliert dadurch nicht sofort die Zuordnung. Login, fehlender oder
gewechselter Ped und deaktivierter Damagelog verwerfen fremde Altzustände.
Die bestehenden Umweltfilter des Hitsounds bleiben unabhängig davon.

Die End-to-End-Suite `DamageLogRegressionTests` enthält 26 Fälle: die zwölf
bisherigen Fälle sowie vierzehn zusätzliche RPC-, Sync-, Abgleichs- und
Schutzfälle, einschließlich nativer und serverseitiger Heilungen.
Das ersetzt keinen echten Ingame-Test auf dem konkreten Server.

### r13-Prüfpfade

| Bereich | Implementierung | Tests / Nachweise |
| --- | --- | --- |
| Eigene Friedhofsmeldung und Todeszählung | [CombatChatParser.cpp](../../src/CombatChatParser.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [CombatChatParserTests.cpp](../../tests/CombatChatParserTests.cpp), [CombatChatBridgeTests.cpp](../../tests/CombatChatBridgeTests.cpp), [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Damagelog für Fall und Explosion | [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h) | [DamageLogRegressionTests.cpp](../../tests/DamageLogRegressionTests.cpp) |
| ASI und Paket | [verify_revision13_artifact.py](../../tools/verify_revision13_artifact.py), [package_revision13.ps1](../../tools/package_revision13.ps1) | PE32/x86, Ressource 300, ASI-/Paketprüfsummen |

### Zugehörige finale Paketnachweise

- [NLBinder.asi](../../dist/NLBinder-0.1.1-r13/NLBinder.asi)
- [Vollständiges ZIP](../../dist/NLBinder-v0.1.1-KOBiRO-r13.zip)
- [Gesamter TXT-Changelog](../../dist/NLBinder-0.1.1-r13/CHANGELOG-GESAMT.txt)
- [SHA-256](../../dist/NLBinder-0.1.1-r13/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r13/BUILD-INFO.txt)
- [PE-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r13/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r13/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r13

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Erfolgreiche Prüfungen: DamageLogRegression: 152, ActionExecutor: 143, Reconnect: 116, SAMPBridge: 576, StartupStatsSync: 366, StatsDialogRPCAdapter: 727, JobSkillsImport: 17, CombatChatParser: 190, CombatChatBridge: 175, native_reconnect_helper: 83.

Die Friedhofszeile ist der maßgebliche Auslöser der eigenen Todeszählung.
Parser- und Integrationstests prüfen die Zuordnung sowie die lokale Meldung.
Ein nativer 0-HP-Zustand erhöht den Todeszähler nicht mehr.
Die Damagelog-Regressionssuite prüft Fallschaden und Explosionsübergänge
getrennt von den Umweltfiltern des Hitsounds. Bestehende Statistik- und
Jobskill-Abläufe werden weiterhin gemeinsam geprüft.
Die Ausgabe des finalen Builds heißt NLBinder.asi. Die geprüfte Datei
umfasst 18.510.848 Bytes; SHA-256:
`674f86449f92c46f9d95fce05ae99b4ac561f0ac39d82d61b5518a41f38b89d2`.

PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der
eingebettete Reconnect-Helfer stimmt mit dem frisch geprüften Helfer
überein. Die Helferprüfung nutzt harmlose Testprozesse; das echte Spiel
und der installierte Launcher werden dabei nicht gestartet oder beendet.

Der vollständige TXT-Changelog umfasst die Abschnitte 1–19 und wird
zusammen mit der ASI ausgeliefert.
Separater ASI-Stand und ZIP-Inhalt stimmen bytegleich überein. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, den vollständigen
TXT-Changelog, 43 explizite leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r13-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test auf dem konkreten Server steht aus.

## Historischer Prüfstand r12 – 30. Oktober 2026

Der aktuelle Stand ist **NLBinder v.0.1.1 by KOBiRO, r12**.
[Bedienung](../NLBinder-0.1.1-r12-DE.md),
[r12-Nachtrag](CHANGELOG-DE.md#18-nachtrag-r12--30-oktober-2026) und
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt) beschreiben die Änderungen.

### Verhalten und prüfbarer Umfang

Die Friedhofsmeldung ist vollständig aus der Todesauswertung des Chatparsers
entfernt. Sie darf alleine, nach einer nativen Todesmeldung oder nach einem
zwischenzeitlichen nativen Respawn keinen zusätzlichen Tod erzeugen.
Die native 0-HP-/Todeserkennung bleibt enthalten. Andere Killformate und die
bisherige Opferzuordnung der gesuchten-Verbrecher-Meldung bleiben erhalten.
Dieser Stand ersetzt den früheren Abgleich der Friedhofszeile aus r11.

Der Damagelog behandelt Fallschaden getrennt vom Umweltfilter des Hitsounds.
Messbare Explosionsverluste bleiben enthalten, wobei tödlicher Überkill keinen
größeren Verlust als die zuvor verfügbaren gültigen HP-/Westenwerte erzeugt.
Kurze unplausible Zwischenwerte desselben Peds ersetzen den letzten gültigen
HP-/Westenstand nicht. Serverseitige HP-/Westenänderungen werden beim Empfang
auch zwischen zwei nativen Messungen ausgewertet. Der Abgleich mit dem
nachfolgenden nativen Stand verhindert, dass derselbe Verlust doppelt im
Damagelog erscheint. Die End-to-End-Suite `DamageLogRegressionTests` prüft
die Schadensverarbeitung bis zur erzeugten lokalen Chatmeldung.
Ein konkretes Antworttempo des Servers und ein echter Ingame-Test werden
durch die automatisierten Tests nicht behauptet.

### r12-Prüfpfade

| Bereich | Implementierung | Tests / Nachweise |
| --- | --- | --- |
| Entfernte Friedhofsauswertung | [CombatChatParser.cpp](../../src/CombatChatParser.cpp) | [CombatChatParserTests.cpp](../../tests/CombatChatParserTests.cpp), [CombatChatBridgeTests.cpp](../../tests/CombatChatBridgeTests.cpp), [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Fallschaden und Explosion | [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h) | [DamageLogRegressionTests.cpp](../../tests/DamageLogRegressionTests.cpp) |
| ASI und Paket | [verify_revision12_artifact.py](../../tools/verify_revision12_artifact.py), [package_revision12.ps1](../../tools/package_revision12.ps1) | PE32/x86, Ressource 300, ASI-/Paketprüfsummen |

### Zugehörige finale Paketnachweise

- [NLBinder.asi](../../dist/NLBinder-0.1.1-r12/NLBinder.asi)
- [Vollständiges ZIP](../../dist/NLBinder-v0.1.1-KOBiRO-r12.zip)
- [Gesamter TXT-Changelog](../../dist/NLBinder-0.1.1-r12/CHANGELOG-GESAMT.txt)
- [SHA-256](../../dist/NLBinder-0.1.1-r12/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r12/BUILD-INFO.txt)
- [PE-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r12/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r12/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r12

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Erfolgreiche Prüfungen: DamageLogRegression: 66, ActionExecutor: 143, Reconnect: 116, SAMPBridge: 571, StartupStatsSync: 363, StatsDialogRPCAdapter: 727, JobSkillsImport: 17, CombatChatParser: 190, CombatChatBridge: 116, native_reconnect_helper: 83.

Die Friedhofszeile wird als normaler Servertext behandelt. Parser- und
Integrationstests prüfen, dass sie weder alleine noch nach einem nativen
Tod einen weiteren Tod oder eine zusätzliche lokale Todesmeldung erzeugt.
Die Damagelog-Regressionssuite prüft Fallschaden und Explosionsübergänge
getrennt von den Umweltfiltern des Hitsounds. Bestehende Statistik- und
Jobskill-Abläufe werden weiterhin gemeinsam geprüft.
Die Ausgabe des finalen Builds heißt NLBinder.asi. Die geprüfte Datei
umfasst 18.502.656 Bytes; SHA-256:
`27add66ee5630b500025dcb8af5e975f5ddadfc994afa3148c3793cf6df1d21c`.

PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der
eingebettete Reconnect-Helfer stimmt mit dem frisch geprüften Helfer
überein. Die Helferprüfung nutzt harmlose Testprozesse; das echte Spiel
und der installierte Launcher werden dabei nicht gestartet oder beendet.

Der vollständige TXT-Changelog umfasst die Abschnitte 1–18 und wird
zusammen mit der ASI ausgeliefert.
Separater ASI-Stand und ZIP-Inhalt stimmen bytegleich überein. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, den vollständigen
TXT-Changelog, 43 explizite leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r12-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test auf dem konkreten Server steht aus.

## Historischer Prüfstand r11 – 29. Oktober 2026

Der aktuelle Stand ist **NLBinder v.0.1.1 by KOBiRO, r11**.
[Bedienung](../NLBinder-0.1.1-r11-DE.md),
[r11-Nachtrag](CHANGELOG-DE.md#17-nachtrag-r11--29-oktober-2026) und
[vollständiger TXT-Changelog](../CHANGELOG-GESAMT.txt) beschreiben die
Änderungen. Frühere Prüfstände bleiben ihren ursprünglichen Binärdateien
zugeordnet.

### Verhalten und prüfbarer Umfang

Der Button **Aktuellen Spielstand synchronisieren** unter Tacho verwendet
erneut den bestehenden Ablauf zum Lesen eigener Stats und Jobskills.
Erfolgreich empfangene Werte werden in die vorhandenen Datenmodelle und
Variablen übernommen und gespeichert. Der Abschluss ist erst bestätigt,
wenn beide frischen Antworten erfolgreich übernommen sind. Doppelte
Anforderungen werden abgefangen; fehlende oder unpassende Antworten
dürfen keine erfolgreiche manuelle Synchronisation melden.

Eine Synchronisation zählt importierte Gesamtstände nicht als neue
Sitzungsereignisse. Der Abruf beim Einloggen bleibt vorhanden. Die
Serverantwort ist weiterhin erforderlich; ein bestimmtes Antworttempo
wird nicht vorausgesetzt.

Der lokale Killhinweis nutzt die einheitliche Vorlage
`[NL-BINDER] Du hast einen Kill gemacht! Aktuell hast du: {kills} Kills.`.
Die Gesamtzahl wird aus demselben aktuellen Killstand gelesen wie bisher;
die Vorlage ist von der frei gewählten Servernachricht getrennt. Die
gespeicherten lokalen Altvorlagen werden einmalig aktualisiert. Der Alias
`{kills}` ergänzt keinen doppelten Eintrag in der Variablenhilfe.

Die Todeserkennung führt die native HP-/Todesbestätigung und die
Friedhofsmeldung desselben eigenen Todes zusammen. Der Lebenszyklus bis
zum Respawn begrenzt die Zuordnung; die zusätzliche Meldung darf keinen
zweiten Zähleranstieg verursachen. Explosionen und ungültige
Lebenswertübergänge dürfen keine unrealistischen Millionen-Schadenswerte
erzeugen. Gültige echte Verluste werden weiterhin verarbeitet. Das
Damagelog-Präfix nutzt `{33CCFF}`.

Für die Schadensmessung werden nicht-endliche Werte und einzelne HP- oder
Westenwerte ab 1.000.000 verworfen; der Messvergleich startet danach mit
einer frischen Basis. Endliche negative Überkillwerte werden für den
Vergleich auf null begrenzt. Ein Explosionsübergang von 50 HP auf einen
internen Wert von -8.000.000 entspricht damit 50 tatsächlich verlorenen HP.
Regulär erhöhte Lebens-/Westenwerte oberhalb 100 bleiben messbar; es gibt
keine pauschale Begrenzung aller gültigen Messwerte auf 100.

Die konkrete Regression bestätigt denselben Tod trotz einer zehn Sekunden
späteren Friedhofsmeldung nur einmal. Ein nachweisbar steuerbarer Respawn
ermöglicht anschließend die Zählung des nächsten eigenständigen Todes.

Die Ausgabe des Win32-Release-Builds heißt **NLBinder.asi**. Die ASI liegt
auch im Paket und im angeforderten vollständigen Desktop-Quellordner
**NLBinder-masterNEW**. Der TXT-Changelog enthält den gesamten bisherigen
deutschen Änderungsverlauf. Compiler-Zwischenstände und persönliche
Laufzeitdaten werden nicht in die Weitergabe kopiert.

### r11-Prüfpfade

| Bereich | Implementierung | Tests / Nachweise |
| --- | --- | --- |
| Manuelle vollständige Synchronisation | [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp), [JobSkillsSync.cpp](../../src/JobSkillsSync.cpp), [MainWindowUI.cpp](../../src/MainWindowUI.cpp), [SAMPBridge.h](../../include/SAMPBridge.h) | [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| ASI-Name und Paket | [Keybinder.vcxproj](../../Keybinder.vcxproj), [verify_revision11_artifact.py](../../tools/verify_revision11_artifact.py), [package_revision11.ps1](../../tools/package_revision11.ps1) | PE32/x86, Ressource 300, Paketdateien und ASI-Prüfsummen |

### Zugehörige finale Paketnachweise

- [NLBinder.asi](../../dist/NLBinder-0.1.1-r11/NLBinder.asi)
- [Vollständiges ZIP](../../dist/NLBinder-v0.1.1-KOBiRO-r11.zip)
- [Gesamter TXT-Changelog](../../dist/NLBinder-0.1.1-r11/CHANGELOG-GESAMT.txt)
- [SHA-256](../../dist/NLBinder-0.1.1-r11/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r11/BUILD-INFO.txt)
- [PE-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r11/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r11/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r11

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Erfolgreiche Prüfungen: ActionExecutor: 143, Reconnect: 116, SAMPBridge: 555, StartupStatsSync: 363, StatsDialogRPCAdapter: 727, JobSkillsImport: 17, CombatChatParser: 181, CombatChatBridge: 83, native_reconnect_helper: 83.

Der Statistik-/Jobskill-Ablauf und seine bisherigen Identitäts- und
Dialogpfade werden gemeinsam mit den neuen manuellen Abruf-Fällen geprüft.
Die Ausgabe des finalen Builds heißt NLBinder.asi. Die geprüfte Datei
umfasst 18.500.608 Bytes; SHA-256:
`204fba919b3b211a884841a099463389c981cd626e6927a10eb2ac1cccc29b2e`.

PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der
eingebettete Reconnect-Helfer stimmt mit dem frisch geprüften Helfer
überein. Die Helferprüfung nutzt harmlose Testprozesse; das echte Spiel
und der installierte Launcher werden dabei nicht gestartet oder beendet.

Der vollständige TXT-Changelog umfasst die Abschnitte 1–17 und wird
zusammen mit der ASI ausgeliefert.
Separater ASI-Stand und ZIP-Inhalt stimmen bytegleich überein. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, den vollständigen
TXT-Changelog, 43 explizite leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r11-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test auf dem konkreten Server steht aus.

## Historischer Prüfstand r10 – 29. Oktober 2026

Der aktuelle Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r10**. Die
[Bedienung](../NLBinder-0.1.1-r10-DE.md) und der
[r10-Nachtrag](CHANGELOG-DE.md#16-nachtrag-r10--29-oktober-2026) beschreiben
den Nova-Spielstart, den schnelleren Motorstart und die ergänzte
Server-Chat-Erkennung sowie die neue manuelle Variable `{input}`. Frühere Prüfstände bleiben nachfolgend ihren
jeweiligen ASI-Dateien zugeordnet.

### Verhalten und prüfbarer Umfang

Der Nova-Reconnect nutzt den normalen Startweg im ausgewählten Launcher und
beobachtet den zugehörigen Spielstart. Der Helfer wartet zuvor auf das
Ende des bisherigen Spielprozesses. Die bestehende Bereitschaftsmeldung
zwischen Binder und Helfer verhindert ein Beenden ohne vorbereiteten
Neustart. Verifikation startet ausschließlich harmlose Testprogramme;
der installierte Launcher und das echte Spiel werden dafür nicht geöffnet
oder beendet.

Der automatische Motorstart reagiert schneller auf einen neuen Einstieg
als Fahrer. Die vorhandene Zuordnung der Server-Addon-Meldung zum aktuellen
Fahrzeug und Einstieg bleibt erhalten. Laufende Motoren, bereits verbrauchte
Einstiege und blockierte Benutzereingaben dürfen keine zusätzlichen
Motor-Tastendrücke verursachen.

Bei Kill-/Todesmeldungen ist der angemeldete eigene Spielername maßgeblich.
Öffentliche Meldungen werden anhand ihrer Täter-/Opferposition ausgewertet.
Persönliche Servermeldungen mit „Du“ beziehen sich auf den Empfänger.
Chatzeilen mit einem Spielerpräfix dürfen diese Erkennung nicht auslösen.
Verschiedene Servertexte, Todes-RPC, Killfeed und Statistikabgleich müssen
bei demselben Ereignis gemeinsam gezählt werden. Die bestehenden Ein-/Aus-
Schalter und die Killtext-Einstellungen bleiben wirksam.

### Konkrete Start- und Erkennungsregeln

Wenn das installierte Nova-Programm laut seinem Manifest Administratorrechte
benötigt und der laufende Binder nicht bereits mit diesen Rechten arbeitet,
wird der Helfer über die normale Windows-Freigabe vorbereitet, bevor das
Spiel beendet wird. Abgelehnte Freigabe, fehlende Bereitschaft oder ein
fehlgeschlagenes Speichern lassen den alten Spielprozess geöffnet. Ein
eigenes Abbruchereignis verhindert einen späteren Neustart auch dann, wenn
der vorbereitete Helfer wegen unterschiedlicher Rechte nicht direkt
beendet werden kann. Windows-Freigaben werden nicht automatisch bestätigt.

Der Nova-Helfer sucht nur Fenster des eingestellten Launcher-Programms.
Er aktiviert den eindeutig zugeordneten, aktiven Button „Spiel starten“ über
die Windows-Zugänglichkeitsaktion (Invoke oder normale Standardaktion).
Er klickt nicht anhand von Bildschirmkoordinaten. Die Beschriftungen
„Wird gestartet…“ und „GTA beenden“ sind keine Startschaltflächen. Ein
vorhandener Launcher wird wiederverwendet. Solange noch kein zugehöriger
Spielprozess vorhanden ist, sind weitere Startanforderungen im Abstand von
2,5 Sekunden möglich; das Gesamtfenster beträgt regulär 90 Sekunden.

Die Prüfung des neuen Spielprozesses nutzt den vollständigen Pfad zum zuvor
verwendeten Spielprogramm. Ein gleichnamiges Programm aus einem anderen
Ordner bestätigt keinen Reconnect. Sobald der passende Prozess läuft,
unterbleiben weitere Startaktionen. Erfolg verlangt zusätzlich eine
Bestätigung des neu geladenen Binders: Verbindung hergestellt, eigener
Spieler aktiv und vorhanden, weder Wasted noch Spectating. Die Bestätigung
ist an den vollständigen Spielpfad und die neue Prozess-ID gebunden. Ein
bereits vorhandener weiterer Spielprozess desselben Pfads verhindert schon
die Bereitschaft des Helfers, sodass der alte Client geöffnet bleibt.

Die Wartezeit vor einem Binder-Start bei gestopptem Motor beträgt 100 ms
statt zuvor 1.500 ms. Wenn der native Motorzustand beim Einsteigen zunächst
„an“ meldet, bleibt eine Bestätigung über bis zu 1.500 ms erhalten, um einen
kurzen GTA-Übergang nicht als endgültigen Motorstart zu behandeln. Eine
bereits eingetroffene Server-Addon-Meldung wird fahrzeug- und einstiegsbezogen
zugeordnet. Der Binder kann einen noch nicht empfangenen Serverzustand
nicht vorwegnehmen; trifft die Addon-Bestätigung erst nach dem eigenen
Tastendruck ein und war der native Zustand zugleich veraltet, bleibt eine
zeitliche Grenze dieser Beobachtung. Die Tests prüfen die bestätigten
Meldungs- und Zustandsfolgen; sie ersetzen keinen Test mit Serverlatenz im
laufenden Spiel.

Die Gefängnis-gesprengt-Zeile nennt nur die Person, die nicht inhaftiert
werden konnte. Sie enthält keinen Täter. Sie wird deshalb nur einem bereits
bekannten eigenen Kill dieser Person zugeordnet und schreibt für sich
alleine keinen fremden Kill gut. Bei den ausdrücklich adressierten
„Du hast …“-Meldungen ist der eigene Spieler der Empfänger. Die öffentlichen
Formate verlangen dagegen die vollständige Übereinstimmung des eigenen
Namens in der jeweiligen Täter- oder Opferrolle.

Bekannte Opfernamen werden innerhalb des vorhandenen Fünf-Sekunden-
Abgleichs mit Chat, Killfeed und Todes-RPC zusammengeführt. Verschiedene
Opfer bleiben getrennt. Bei nativen Meldungen haben zusätzlich vorhandene
Spieler-IDs und die native Waffenidentität Vorrang: Unterschiedliche
bekannte Opfer-IDs oder widersprechende bekannte native Waffen dürfen
nicht wegen eines gleichen Namens als dasselbe Ereignis verschwinden.
Ein technischer Ersatzname „Player_ID“ bei noch nicht aufgelöstem Spielernamen
gilt nicht als bestätigter Opfername; der spätere richtige Name ergänzt
dieselbe Beobachtung. Bei persönlichen Hinweisen ohne Opfername werden
unterschiedliche Textfamilien innerhalb von 1,2 Sekunden einem noch nicht
mit dieser Familie belegten Ereignis zugeordnet. Eine weitere Meldung
derselben persönlichen Textfamilie zählt als neues Ereignis, auch wenn
mehrere solche Meldungen im selben Frame eintreffen. Ohne Opfer oder
serverseitige Ereignis-ID ist dieser Abgleich zeitlich und anhand der
Meldungsfamilie begrenzt. Später bekannte Namen und Waffendaten ergänzen die
vorhandene Beobachtung, ohne noch einmal einen Bind zu senden. Unbekannte
Waffen legen keinen Statistik-Eintrag mit Waffen-ID -1 an.

Die bestehenden Gesamtzähler werden weiterhin mit den autoritativen
Server-Statistiken abgeglichen. Wenn der Server für seine Gesamtstatistik
andere Regeln verwendet, etwa Teamkills nicht im Lebenszeitwert mitzählt,
bleibt dessen später gelesener Gesamtwert maßgeblich. Die Sitzungszähler
halten die im aktuellen Lauf erkannten Ereignisse fest. Ein kurzzeitig
älterer Statistikstand überschreibt einen gerade erkannten Kill oder Tod
nicht unmittelbar.

### Unterstützte Servertexte und Zuordnung

`EIGENER_NAME` steht für den dynamisch gelesenen eigenen Spielernamen;
`OPFER` und sonstige Platzhalter sind variable Serverwerte.

| Textform | Zuordnung |
| --- | --- |
| Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym | eigener Kill |
| Du hast einen Spieler getötet. | eigener Kill |
| GANGWAR (...): Du hast einen Feind ausgeschaltet! | eigener Kill |
| << EIGENER_NAME hat den gesuchten Verbrecher OPFER getötet. >> | eigener Kill |
| << TÄTER hat den gesuchten Verbrecher EIGENER_NAME getötet. >> | eigener Tod |
| << OPFER konnte nicht inhaftiert werden, da das Gefängnis gesprengt wurde. >> | Ergänzung zu bereits bekanntem eigenem Kill dieses Opfers |
| REVIERKAMPF: EIGENER_NAME hat OPFER getötet. Neuer Spielstand: Angreifer: ZAHL - Verteidiger: ZAHL | eigener Kill |
| REVIERKAMPF: EIGENER_NAME hat OPFER getötet (Teamkill). Neuer Spielstand: Angreifer: ZAHL - Verteidiger: ZAHL | eigener Kill |
| REVIERKAMPF: EIGENER_NAME hat OPFER getötet. | eigener Kill |
| REVIERKAMPF: EIGENER_NAME hat einen Teamkill begangen. | eigener Kill |
| STREETWAR: EIGENER_NAME hat OPFER (...) getötet. Aktuelle Kills: ZAHL | eigener Kill |
| DEATHZONE: EIGENER_NAME hat OPFER mit einer WAFFE getötet. | eigener Kill |
| Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft. | eigener Tod |

### Manuelle Texteingabe für Binds

`{input}` wird vor der ersten Aktion eines Tasten- oder Befehlsbinds einmal
abgefragt. Der native Chat öffnet erst im nächsten Frame, nachdem die
ursprüngliche Slash-Eingabe abgeschlossen ist. Der lokale Hinweis lautet
**Gib hier deinen Input-Text ein:**. Die nächste native Eingabe wird vor
der Slash-Befehlsverarbeitung abgefangen und in UTF-8 übernommen. Die
Antwort selbst wird nicht an den Server weitergegeben. Erst ein späterer
Frame führt die fertig ersetzten Aktionen aus. Mehrere Platzhalter
verwenden dieselbe Antwort; darin enthaltene Platzhalter bleiben wörtlich.

Esc, eine leere Eingabe bzw. das Schließen des Chats, ein Serverdialog, Tod, Verbindungsende,
ein ersetzter Bind oder explizites Stoppen verwirft die wartende Sequenz.
Ohne installierten Abfang-Hook wird keine Eingabeaufforderung geöffnet.
Ein führender `/` in einer Nachrichtenaktion nutzt den Befehlskanal; ohne
Schrägstrich bleibt es normaler Chat. `{input}` steht nur in der Hilfe für
Textbindungen, direkt unter `{kd}`. Automatische Killtexte fragen nichts ab.

### r10-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Reconnect-Plan und Helfer | [ReconnectManager.cpp](../../src/ReconnectManager.cpp), [ReconnectHelper.cpp](../../tools/ReconnectHelper.cpp), [NovaReconnectFlow.h](../../include/NovaReconnectFlow.h), [Main.cpp](../../src/Main.cpp) | [ReconnectManagerTests.cpp](../../tests/ReconnectManagerTests.cpp), [test_reconnect_worker.py](../../tools/test_reconnect_worker.py) |
| Schneller Motorstart und Addon-Schutz | [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Server-Chat und Spielerrollen | [CombatChatParser.cpp](../../src/CombatChatParser.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [CombatChatParserTests.cpp](../../tests/CombatChatParserTests.cpp), [CombatChatBridgeTests.cpp](../../tests/CombatChatBridgeTests.cpp) |
| Manuelle Texteingabe und Befehlsrouting | [ActionExecutor.cpp](../../src/ActionExecutor.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [MessageVariables.h](../../include/MessageVariables.h) | [ActionExecutorTests.cpp](../../tests/ActionExecutorTests.cpp), [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| ASI, Helfer und Paket | [verify_revision10_artifact.py](../../tools/verify_revision10_artifact.py), [package_revision10.ps1](../../tools/package_revision10.ps1) | PE32/x86, Ressource 300, Helfer-/Paket-SHA-256 und 43 Waffenordner |

### Zugehörige finale Paketnachweise

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r10/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r10/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r10/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r10/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r10/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r10

Gesamttests und Win32-Release-Build endeten mit Exitcode 0.
Prüfungen des finalen Stands: ActionExecutor: 143, Reconnect: 116, SAMPBridge: 508, CombatChatParser: 181, CombatChatBridge: 83, StartupStatsSync: 252, StatsDialogRPCAdapter: 727, native_reconnect_helper: 83.

Die Chat-Integration prüft die eigenen Täter-/Opferrollen, die vorgegebenen
Textfamilien, CP1252/UTF-8, Spielerpräfixe und lokale Binder-Echos. Persönliche
Hinweise, benannte Meldungen, Todes-RPC und Statistikabgleich werden in
verschiedenen Reihenfolgen gemeinsam geprüft. Gleichzeitige persönliche
Kills bleiben getrennt; native IDs sowie fehlende Namen/Waffen werden
beim späteren Ergänzen ohne zweiten Bind berücksichtigt.

Die Texteingabe-Tests prüfen das Pausieren der gesamten Sequenz, Enter,
Esc/Stop/Ersetzen, eine neue Abfrage beim nächsten Aufruf, gemeinsame
wörtliche Ersetzung mehrerer Platzhalter und Umlaut-Konvertierung.
Der vollständige /News-In-BSN-Fall bestätigt, dass die rohe Antwort
nicht separat gesendet wird und der fertige Befehl den Befehlskanal nutzt.

Der native Helfer wird mit harmlosen GUI-/Spielprozess-Fixtures geprüft.
Sechs Nova-Fälle umfassen wiederholtes Play, verzögerten Start, ein laufendes
Spiel ohne Beitrittsbestätigung, Startabbruch mit erneutem Versuch, einen
gleichnamigen Prozess aus einem anderen Ordner und eine bereits laufende
zweite Instanz desselben Pfads. Zusätzlich wird der Abbruch nach Bereitschaft
vor dem späteren Ende des Elternprozesses geprüft. Installierter Launcher
und echtes GTA wurden dafür weder gestartet noch beendet.

Die geprüfte ASI umfasst 18.493.952 Bytes; SHA-256:
`0607a1d303333bc09e625f76b296f781a0ca71d48c9a494a1aefc32e0c0ab3d0`.
PE32/x86, Windows-Imports und Ressource 300 sind geprüft. Der eingebettete
Reconnect-Helfer stimmt bytegleich mit dem frisch geprüften Helfer überein.
Die separate ASI und die ASI im ZIP haben dieselbe Prüfsumme. Ein
unabhängiger Abgleich bestätigt alle Paketdateien, 43 explizite leere
Waffenordner und keine persönlichen PNGs im ZIP.
[Maschinenlesbare Ergebnisse](r10-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test mit dem konkreten Server und installierten Nova
Launcher steht aus. Ein noch nicht empfangener Server-Addon-Zustand kann
nicht vorweggenommen werden.

## Historischer Prüfstand r9 – 27. Oktober 2026

Der aktuelle Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r9**. Die
[Bedienung](../NLBinder-0.1.1-r9-DE.md) und der
[r9-Nachtrag](CHANGELOG-DE.md#15-nachtrag-r9--27-oktober-2026) beschreiben
die vier verbleibenden Interface-Bereiche und den korrigierten Mausmodus.
Frühere Prüfstände bleiben nachfolgend ihren damaligen ASIs zugeordnet.

### Verhalten und prüfbarer Umfang

Unter **Mods → Interface** bleiben **Geldanzeige, Waffenicon, Munition und
Minimap** mit eigenen Positions- und Farbauswahlen. Die sieben entfernten
Bereiche sind nicht mehr im Editor auswählbar; ihre früher gespeicherten
Interface-Anpassungen werden nicht weiter angewendet. Das entfernt keine
normalen Spielanzeigen.

Der Fehler beim Verschieben lag in der Trennung zwischen sichtbarem
Binder-Menü und aktivem Verschiebemodus. Nach dem Ausblenden des Menüs war
der Interface-Modus weiter aktiv, einzelne native Maus-Hooks prüften jedoch
nur die Menüsichtbarkeit. Dadurch konnte GTA den Mauszeiger weiterhin zur
Bildschirmmitte zurücksetzen. Die gemeinsame Mauszuständigkeit berücksichtigt
nun auch die aktiven Verschiebemodi der dauerhaften Online-Overlays und des
Interface-Editors. Mauszustände und Mausereignisse werden währenddessen vom
Spiel abgeschirmt. Esc beendet den Modus und speichert. Die Maus wird wieder
an das Spiel übergeben, sobald auch kein Binder-Menü oder anderer UI-Modus
sie mehr benötigt.

Positions- und Farbwerte gelten ausschließlich für die vier nativen GTA-
HUD-Bereiche. Eigene Server-Textdraws und Datum gehören nicht dazu. Die
Waffenicon-Auswahl und früheren Binder-Funktionen bleiben enthalten. Ein
echter Ingame-Test auf dem konkreten Server steht aus.

Die native Waffenicon-Zuordnung im allgemeinen Spieler-HUD hat eigene
Ausschlussbereiche für die unveränderten Lebens-, Rüstungs- und Atembalken.
Diese Bereiche reichen den ursprünglichen Zeichenaufruf durch; dadurch
werden die entfernten Balken nicht versehentlich als Waffenicon behandelt.
Wanted-, Fahrzeugnamen- und Gebietsnamen-Hooks sowie die Uhrklassifizierung
gehören nicht mehr zur Interface-Anpassung.

### r9-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Vier Interface-Elemente | [InterfaceLayout.h](../../include/InterfaceLayout.h), [NativeHudEditor.cpp](../../src/NativeHudEditor.cpp), [InterfaceEditorUI.cpp](../../src/InterfaceEditorUI.cpp) | [NativeHudEditorTests.cpp](../../tests/NativeHudEditorTests.cpp) |
| Gemeinsame Mauszuständigkeit | [ImGuiOverlay.cpp](../../src/ImGuiOverlay.cpp), [Main.cpp](../../src/Main.cpp) | [ImGuiOverlayTests.cpp](../../tests/ImGuiOverlayTests.cpp) |
| Ziehen und Speichern | [InterfaceEditor.cpp](../../src/InterfaceEditor.cpp) | [InterfaceEditorTests.cpp](../../tests/InterfaceEditorTests.cpp) |
| Persistenz und alte Einstellungen | [SettingsManager.cpp](../../src/SettingsManager.cpp) | [InterfaceLayoutSettingsTests.cpp](../../tests/InterfaceLayoutSettingsTests.cpp) |
| ASI, Helfer und Paket | [verify_revision9_artifact.py](../../tools/verify_revision9_artifact.py), [package_revision9.ps1](../../tools/package_revision9.ps1) | PE32/x86, Ressource 300, Helfer-/Paket-SHA-256 und 43 Waffenordner |

### Zugehörige finale Paketnachweise

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r9/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r9/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r9/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r9/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r9/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r9

Gesamttests und Win32-Release-Build endeten mit Exitcode 0. Die
InterfaceEditor-Suite meldet 322 erfolgreiche Prüfungen einschließlich
wirklicher Mauspositionen, Maustasten-Eingaben und Esc-Speicherung im
isolierten ImGui-Kontext. Der native Transformationsbereich meldet 54
Prüfungen für die vier verbleibenden Elemente, Standard-Durchreichen,
Farben, Positionsversätze und die Ausschlussbereiche der unveränderten
Balken. Die Interface-Einstellungen melden 123 Prüfungen für die vier
Kategorien und Presets, erhaltene Positionen/Farben, ignorierte entfernte
Felder sowie Speichern/Laden.

Drei neue ImGuiOverlay-Regressionen prüfen die gemeinsame Mauszuständigkeit:
Verschieben bei intern ausgeblendetem Menü, den bisherigen Online-Ablauf mit
Menürückkehr nach Esc und die getrennte QuickMap-Mausradbehandlung. Die
Prüfungen verwenden dieselben Filter- und Zuständigkeitsfunktionen wie die
nativen Zeiger-/DirectInput-Hooks. Sie umfassen Mauszustände, gepufferte
Mausereignisse und die einmalige Freigabe nach dem letzten UI-Besitzer.
Die vorherige Prüfung des unveränderten Online-Ablaufs wird beibehalten.
Die Produktionsquellen Main.cpp und ImGuiOverlay.cpp bestanden zusätzlich
die gezielte Syntaxprüfung. Die automatisierten Tests verwenden synthetische
Eingaben und ersetzen keinen Test der tatsächlichen Darstellung im Spiel.

Die geprüfte ASI umfasst 18.442.240 Bytes; SHA-256:
`41a1e6d772f6c68b5541b4a7ea4ec1f997d5d97a55ffdbe446d0d89117734964`.
PE32/x86, Windows-Imports und der bytegleiche eingebettete Helfer sind
geprüft. Der native Helfertest umfasst 18 erfolgreiche Prüfungen mit
harmlosen Testprozessen. Die separate ASI und die ASI im ZIP haben dieselbe
Prüfsumme; ein unabhängiger Abgleich bestätigt alle Paketdateien sowie
43 leere Waffenordner und keine persönlichen PNGs im ZIP.
[Maschinenlesbare Ergebnisse](r9-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test mit dem konkreten Server und Launcher steht aus.

## Historischer Prüfstand r8 – 27. Oktober 2026

Der aktuelle Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r8**. Die
[Bedienung](../NLBinder-0.1.1-r8-DE.md) und der
[r8-Nachtrag](CHANGELOG-DE.md#14-nachtrag-r8--27-oktober-2026) beschreiben die
neue Interface-Kategorie. Ältere Nachweise bleiben nachfolgend ihren damaligen
ASI-Dateien zugeordnet.

### Verhalten und prüfbarer Umfang

Elf native HUD-Bereiche haben eigene auflösungsunabhängige Positionsversätze
und optionale Farben: Leben, Rüstung, Atemluft, Geld, Uhrzeit, Waffenicon,
Munition, Wanted-Sterne, Fahrzeugname, Gebietsname und Minimap. Der eigene
Farbmodus ist getrennt vom Positionsversatz. Der normale Zeichenpfad wird
beibehalten. Ohne Anpassung reicht der Binder die native Darstellung durch.

Der Verschiebemodus entspricht dem bestehenden Overlay-Ablauf: ein Element
wählen, mit der Maus ziehen und mit Esc speichern. Die Konfiguration wird im
aktiven Preset gespeichert. Farben und Positionen lassen sich getrennt
zurücksetzen. Die bisherigen drei HUD-Farben werden beim Laden alter
Einstellungen übernommen. Der Chat und andere Elemente dürfen nicht durch
eine dauerhafte Änderung der gemeinsamen HUD-Palette umgefärbt werden.

Der Umfang ist das lokale GTA-Interface. Eigene Server-Textdraws und ein vom
Server gezeichnetes Datum werden nicht als native Uhr oder native HUD-
Elemente ausgegeben. Ein echter Ingame-Test steht aus.

### r8-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Elementmodell und native Zeichnung | [InterfaceLayout.h](../../include/InterfaceLayout.h), [NativeHudEditor.cpp](../../src/NativeHudEditor.cpp) | [NativeHudEditorTests.cpp](../../tests/NativeHudEditorTests.cpp) |
| Verschieben und Speichern | [InterfaceEditor.cpp](../../src/InterfaceEditor.cpp), [InterfaceEditorUI.cpp](../../src/InterfaceEditorUI.cpp), [Main.cpp](../../src/Main.cpp) | [InterfaceEditorTests.cpp](../../tests/InterfaceEditorTests.cpp) |
| Eigenständige Mods-Kategorie | [MainWindowUI.cpp](../../src/MainWindowUI.cpp) | Navigation und UI-Bestand im Gesamttest |
| Persistenz und Migration | [SettingsManager.cpp](../../src/SettingsManager.cpp) | [InterfaceLayoutSettingsTests.cpp](../../tests/InterfaceLayoutSettingsTests.cpp) |
| ASI, Helfer und Paket | [verify_revision8_artifact.py](../../tools/verify_revision8_artifact.py), [package_revision8.ps1](../../tools/package_revision8.ps1) | PE32/x86, Ressource 300, Helfer-/Paket-SHA-256 und 43 Waffenordner |

### Zugehörige finale Paketnachweise

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r8/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r8/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r8/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r8/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r8/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r8

Gesamttests und Win32-Release-Build endeten mit Exitcode 0. Die
InterfaceEditor-Suite meldet 319 erfolgreiche Prüfungen für Presets,
unabhängige Elementpositionen, Start/Ende des Verschiebens und Speichern. Der
native Transformationsbereich meldet 62 erfolgreiche Prüfungen: Standard-
Durchreichen, getrennte Versätze bei verschiedenen Auflösungen, Farben,
Wanted-Helligkeit, Radar-Tönung, Textklassifizierung, ungültige Werte sowie
das Verwerfen veralteter Griffrahmen nach einem Positionswechsel.

Die Interface-Einstellungen melden 248 erfolgreiche Prüfungen für alle
Elemente und Presets, Speichern/Laden, Migration der bisherigen drei Farben,
ausdrücklich gewählte Originalfarben und ungültige Zahlen. Kleine Versätze
bleiben auch bei wissenschaftlicher Zahlenschreibweise erhalten. Die
bestehenden Waffenicon-Suiten melden 245 und 29 Prüfungen; die Statistik-
Synchronisation 252 und der Dialog-RPC-Adapter 727, jeweils 0 Fehler.

Die geprüfte ASI umfasst 18.443.776 Bytes; SHA-256:
`02401c76a69e40631698ac2e1a5ea5855f69fcbb3d9c0a9f7ee1909c28a30e3b`.
PE32/x86, Windows-Imports und der bytegleiche eingebettete Helfer sind
geprüft. Der native Helfertest umfasst 18 erfolgreiche Prüfungen mit
harmlosen Testprozessen. Die separate ASI und die ASI im ZIP haben dieselbe
Prüfsumme; das ZIP enthält 43 leere Waffenordner und keine persönlichen PNGs.
[Maschinenlesbare Ergebnisse](r8-validation.json) ergänzen die Paketnachweise.
Ein echter Ingame-Test mit dem konkreten Server und Launcher steht aus.

### Anbindung an den nativen Zeichenpfad

Die CHud-Adressen und Aufrufsignaturen wurden mit dem mitgelieferten
plugin-sdk und den Primärquellen
[GTA-Reversed: Hud.cpp](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Hud.cpp)
und [Font.cpp](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Font.cpp)
abgeglichen. Textposition und Textfarbe werden beim jeweiligen PrintString-
Aufruf in den Font-Puffer übernommen. Der vorherige Font-Farbzustand wird
direkt danach wiederhergestellt. Die spätere Font-Puffer-Ausgabe wird nicht
nochmals als Sprite verschoben.

Sprites und Minimap verwenden Kopien der übergebenen 2D-Vertices. Die
Anbindung ermittelt die tatsächlich aktiven RenderWare-Gerätefunktionen
zur Laufzeit. Der Radar-Zeichenbereich umfasst Karte, Ring, Höhenanzeige und
Blips; die Pausenmenü-Karte bleibt außerhalb dieses Bereichs. Die Minimap-
Farbe ist eine Tönung, die Kartendetails und schwarze Umrisse erhält. Die
Wanted-Farbe behält Helligkeitsunterschiede zwischen ausgefüllten und leeren
Sternen bei. Ein weiterer Hook auf DrawWeaponIcon ist dafür nicht nötig;
die vorhandene Waffenicon-Auswahl bleibt eigenständig.

Die automatisierten Prüfungen des nativen Editors verwenden das gemeinsame
Transformations- und Farbmodell mit synthetischen Eingaben. Sie ersetzen
keinen Test der tatsächlichen Darstellung im laufenden GTA-Prozess.


## Historischer Prüfstand r7 – 25. Oktober 2026

Der damalige Quellstand war **NLBinder v.0.1.1 by KOBiRO, r7**.
[Bedienung und Installation](../NLBinder-0.1.1-r7-DE.md) sowie der
[r7-Nachtrag](CHANGELOG-DE.md#13-nachtrag-r7--25-oktober-2026) beschreiben
Waffenicons, Wanted-Dialogfolge und die ergänzten Startabgleiche. Die früheren
r6-/r5-Nachweise bleiben als historische Prüfstände erhalten.

### Verhalten und prüfbarer Umfang

- **Waffenicons:** 43 Waffenordner, PNG-Auswahl pro Waffe und globale sowie
  einzelne Aktivierung unter `Dokumente/GTA San Andreas User Files/NLBinder/Waffenicons/`.
  Der Button zum Ordneröffnen berücksichtigt die tatsächliche Windows-
  Dokumente-Umleitung, etwa OneDrive. Die HUD-Geometrie bleibt Grundlage; es werden keine
  Spielarchive verändert. Ohne ladbares Bild bleibt das Original erhalten.
  Gespeicherte Pfade sind auf den passenden Waffenordner begrenzt. Persönliche
  PNGs gehören nicht in das Repository oder das vorbereitete Ordnerpaket.
- **`/speed ID`:** Die drei gezeigten Dialoge werden anhand von Titeln und
  Eintragstexten verarbeitet. Auf ausdrücklichen Benutzerwunsch wird die
  abschließende Eingabe über **Vergeben** genau einmal abgesendet. Der Vorgang
  vergibt beim passenden Server-Tatbestand 15 Wantedpunkte und misst selbst
  keine Geschwindigkeit. Fehler, Unterbrechungen und unerwartete Dialoge
  dürfen keine weiteren Bestätigungen auslösen.
- **Startabgleich:** Nach `/stats` werden die vorhandenen `/jobskills`
  eingelesen. Bekannte Job-Skillstände werden absolut übernommen und im Feed
  sichtbar gemacht, ohne daraus neue Einkünfte oder doppelte Verdienst-
  Ereignisse abzuleiten. Unbekannte Namen werden nicht erfunden.
- **Motorstart:** Die Einmal-Begrenzung pro Einstieg bleibt erhalten. Eine
  bestätigte serverseitige Addon-Motorstartmeldung verhindert einen weiteren
  Motor-Toggle durch den Binder.

### r7-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Waffenbibliothek und native Anzeige | [WeaponIconManager.cpp](../../src/WeaponIconManager.cpp), [WeaponIconCatalog.h](../../include/WeaponIconCatalog.h) | [WeaponIconManagerTests.cpp](../../tests/WeaponIconManagerTests.cpp) |
| Vorschau, Auswahl und Einstellungen | [WeaponIconsUI.cpp](../../src/WeaponIconsUI.cpp), [SettingsManager.cpp](../../src/SettingsManager.cpp) | [WeaponIconSettingsTests.cpp](../../tests/WeaponIconSettingsTests.cpp) |
| Wanted-Zustandsfolge und Einmal-Absendung | [SpeedTicketFlow.cpp](../../src/SpeedTicketFlow.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [SpeedTicketFlowTests.cpp](../../tests/SpeedTicketFlowTests.cpp) |
| Job-Skillpunkte als Ausgangsstände | [JobSkillsSnapshot.h](../../include/JobSkillsSnapshot.h), [JobTracker.cpp](../../src/JobTracker.cpp), [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp) | [JobSkillsSnapshotTests.cpp](../../tests/JobSkillsSnapshotTests.cpp) |
| Bestätigter Motorstart | [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| ASI, Helfer und Paket | [verify_revision7_artifact.py](../../tools/verify_revision7_artifact.py), [package_revision7.ps1](../../tools/package_revision7.ps1) | PE32/x86, Ressource 300, Helfer-/Paket-SHA-256 und 43 Ordner |

### Zugehörige finale Paketnachweise

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r7/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r7/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r7/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r7/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r7/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r7

Gesamttests und Win32-Release-Build endeten mit Exitcode 0. Die SAMPBridge-
Suite meldet 435 erfolgreiche Prüfungen, einschließlich sechs Fällen zur
Server-Addon-Motorstartmeldung. Die kombinierte Dialog-/Synchronisationssuite
meldet 252 Prüfungen; sie dekodiert die drei komprimierten Wanted-Dialoge und
kontrolliert die tatsächlichen drei Antwortpakete einschließlich Spieler-ID
und genau einer finalen Bestätigung. Sie prüft außerdem die Reihenfolge
`/stats` → `/jobskills`, den absoluten Jobstand im Feed, erhaltene Einnahmen
und ausbleibende Duplikate beim wiederholten Import.

Weitere erfolgreiche Prüfungen: Dialog-RPC 727, Wanted-Zustandsfolge 109,
Waffenicons-Laufzeit/Bibliothek 245, Waffenicon-Einstellungen 29, Jobskills-
Parser 64 und Jobskills-Import/Persistenz 17. Alle meldeten 0 Fehler.

Die geprüfte ASI umfasst 18.420.224 Bytes; SHA-256:
`abdcca5a0f89ea6e2391834199eef83f267dfee4ac05e28cff7a946b3db9341d`.
PE32/x86, Windows-Imports und der bytegleiche eingebettete Helfer sind geprüft.
Der native Helfertest umfasst 18 erfolgreiche Prüfungen mit harmlosen
Testprozessen. Die ASI im ZIP entspricht der separaten Datei; das ZIP enthält
43 leere Waffenordner und deren Anleitung. [Maschinenlesbare Ergebnisse](r7-validation.json)
ergänzen die Paketnachweise.

Die Dialogantwortstruktur richtet sich nach der
[offiziellen open.mp-Protokollquelle](https://github.com/openmultiplayer/open.mp/blob/master/Shared/NetCode/dialog.hpp).
Der native Zeichnungspfad wurde anhand der lokalen plugin-sdk-Quellen und
[GTA-Reversed: Hud.cpp](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Hud.cpp)
geprüft. Die ursprünglichen Lizenz- und Urheberhinweise bleiben erhalten.

Frühere Ergebnisse gelten ausschließlich für ihre älteren ASIs. Ein echter
Ingame-Test mit dem konkreten Server und Launcher steht aus. Die Tests
verwenden synthetische Daten statt persönlicher Screenshots, Kontowerte oder
PNG-Sammlungen.

## Historischer Prüfstand r6 – 25. Oktober 2026

Der damalige Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r6**. Die
[Bedienungsbeschreibung](../NLBinder-0.1.1-r6-DE.md) und der
[r6-Nachtrag der Änderungsübersicht](CHANGELOG-DE.md#12-nachtrag-r6--25-oktober-2026)
beschreiben die Korrektur seit r5. Die historischen Prüfwerte der früheren
Stände bleiben in den nachfolgenden Abschnitten erhalten.

### Konkreter Fehler und Korrektur

Der tatsächlich beobachtete Statistik-Titel hat das Format
`Statistiken » Spielername`. `TrimCaptionPart` entfernte das Unicode-Trennzeichen
`»` bisher nicht. Dadurch wurde beim anschließenden Identitätsvergleich ein
falscher Spielername verglichen und die eigene Antwort konnte als fremd
abgelehnt werden. Die Korrektur bereinigt das Trennzeichen vor diesem
Vergleich. Fremde Spieleridentitäten bleiben ausgeschlossen.

Nach Erkennung der eigenen Antwort laufen Zählerimport und Schließen der
eigenen Startstatistik im selben Update. Der importierte Gesamtstand wird
anschließend auch in Textvariablen und Testkill verwendet. Es gibt keine
zusätzliche feste Wartepause, aber weiterhin die notwendige Serverantwort.
Der einmalige Motorstart und die übrigen r5-Funktionen sind unverändert.

### r6-Prüfpfade und Datenschutz

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Titeltrennzeichen und eigene/fremde Identität | [ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h) | [ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp) |
| Komprimierter vollständiger RPC-61-Dialog | [StatsDialogRPCAdapter.cpp](../../src/StatsDialogRPCAdapter.cpp) | [StatsDialogRPCAdapterTests.cpp](../../tests/StatsDialogRPCAdapterTests.cpp), [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| Zählerimport, eigener Dialogabschluss und Testkill | [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| Binär- und Paketgleichheit | [verify_revision6_artifact.py](../../tools/verify_revision6_artifact.py), [package_revision6.ps1](../../tools/package_revision6.ps1) | PE32/x86, eingebetteter Helfer, SHA-256 |

Der vollständige Dialogtest verwendet dasselbe Nachrichtenformat mit
erfundenem Spielernamen und erfundenen Werten. Er verbindet die komprimierte
RPC-Dekodierung mit Parser, Zählerimport, eigenem Dialogabschluss und Testkill.
Persönliche Screenshots und die tatsächlichen Statistikwerte werden nicht in
das Repository übernommen.

Die finale r6-Datei wird mit ihren eigenen Nachweisen bereitgestellt:

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r6/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r6/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r6/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r6/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r6/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r6

Die Titelablehnung wurde zunächst mit dem bisherigen r5-Parser reproduziert:
`Statistiken » Fixture_Player` wurde trotz passender eigener Namenszeile und
Zähler abgelehnt. Mit dem korrigierten Parser wird dieselbe Eingabe angenommen.

Die vollständige Testsuite und der Win32-Release-Build endeten mit Exitcode 0.
Der Statistikparser meldet 156 erfolgreiche Prüfungen, die Synchronisation
171 und der Dialog-RPC-Adapter 724, jeweils ohne Fehler. Der neue vollständige
Test verwendet zwei unterschiedliche Ingame-Namen, darunter einen Namen mit
Clanpräfix, einen komprimierten CP1252-Dialog im beobachteten Layout und keine
vorab aufbereiteten Dialogereignisse. Er bestätigt ohne Zeitfortschritt:
Zählerimport, lokales Speichern, Dialogabschluss, Erfolgsmeldung und Testkill
mit den tatsächlich importierten Gesamtständen. Fremde Namen bleiben in den
Regressionstests ausgeschlossen.

Die geprüfte ASI umfasst 18.347.008 Bytes; SHA-256:
`8c489fe45c14a9f45ae9228acd8107538c5323bf6af116d8401cd0cbc7d17620`.
PE32/x86, Windows-Imports und der eingebettete Helfer sind geprüft. Der native
Helfertest besteht aus 18 erfolgreichen Prüfungen mit harmlosen Testprozessen.
Die separat ausgelieferte ASI und die ASI im ZIP haben dieselbe Prüfsumme.
[Maschinenlesbare Ergebnisse](r6-validation.json) ergänzen die Paketnachweise.

Die folgenden r5-Werte gelten nur für deren frühere Binärdatei. Ein echter
Ingame-Test mit dem konkreten Server und Launcher steht aus.

## Historischer Prüfstand r5 – 25. Oktober 2026

Der damalige Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r5**. Die
[Bedienungsbeschreibung](../NLBinder-0.1.1-r5-DE.md) und der
[r5-Nachtrag der Änderungsübersicht](CHANGELOG-DE.md#11-nachtrag-r5--25-oktober-2026)
beschreiben die Änderungen seit r4. Die folgenden r4- und r3-Abschnitte
dokumentieren weiterhin deren frühere Dateien und Prüfergebnisse.

### Geändertes Verhalten gegenüber r4

- Der automatische Motorstart verbraucht den Fahrer-Einstieg vor dem einen
  Tastendruck. Es gibt keinen Retry und keinen zusätzlichen RPC-Planungsweg.
  Ein verzögertes EngineOn oder eine fehlende Textbestätigung kann deshalb
  keinen zweiten Toggle auslösen. Ein bereits laufender oder später manuell
  gestoppter Motor wird im selben Einstieg nicht automatisch erneut bedient.
  Optionswechsel setzen einen verbrauchten Einstieg nicht zurück; ein echter
  Ausstieg oder ein anderes Fahrerfahrzeug schon. Eingabesperren und die
  bestehende Einstiegswartezeit bleiben erhalten.
- Die Statistik wird direkt aus dem vollständigen RPC-61-Text gelesen.
  Messageboxen, Listen und Tabellen sind berücksichtigt. Native Anzeigepuffer
  dürfen nicht mehr die einzige Quelle für längere Statistikdialoge sein.
  Der Parser verarbeitet bis zu 128 Zeilen und Zähler mit eckigen Klammern,
  runden Klammern oder ohne Klammern, einschließlich Farbcodes/Tabulatoren.
- Eigene Kills und Tode genügen für den erfolgreichen Import. Geldwerte und
  weitere Zusatzdaten bleiben optional. Der Import setzt einen Ausgangsstand
  und erzeugt keinen zusätzlichen Kill oder Tod. Anschließend verwenden auch
  die Textvariablen und Testkill die übernommenen Gesamtzähler.
- Nur die identifizierte eigene Startstatistik wird automatisch geschlossen,
  im selben Update wie die Übernahme. Ein eigener manueller `/stats`-Aufruf
  kann nach einem Timeout nachsynchronisieren, ohne automatische Abfrageschleife.
  Nach erfolgreicher Start-Synchronisation bleiben spätere manuelle eigene
  Statistiken sichtbar. Fremde Statistikdialoge und andere Dialoge werden
  nicht pauschal übernommen oder geschlossen.

### r5-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Ein Tastendruck je Fahrer-Einstieg | [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [SAMPBridge.h](../../include/SAMPBridge.h) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Vollständiger Serverdialog | [StatsDialogRPCAdapter.cpp](../../src/StatsDialogRPCAdapter.cpp), [StatsDialogRPCAdapter.h](../../include/StatsDialogRPCAdapter.h) | [StatsDialogRPCAdapterTests.cpp](../../tests/StatsDialogRPCAdapterTests.cpp) |
| Tabellen, Klammern und lange Statistiktexte | [ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h) | [ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp) |
| Übernehmen, eigenes Fenster schließen und manuell nachsynchronisieren | [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp), [SettingsManager.cpp](../../src/SettingsManager.cpp) | [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| Binär- und Paketprüfung | [verify_revision5_artifact.py](../../tools/verify_revision5_artifact.py), [package_revision5.ps1](../../tools/package_revision5.ps1) | PE32/x86, Ressource 300, Helfer- und Paket-SHA-256 |

Die finale r5-Datei ist über ihre eigenen Paketnachweise zugeordnet:

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r5/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r5/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r5/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r5/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r5/RECONNECT-TEST.json)

### Ausgeführte Prüfungen für r5

Die Gesamttests und der anschließende Win32-Release-Build sind mit Exitcode 0
abgeschlossen. Die SAMPBridge-Suite meldet 411 erfolgreiche Prüfungen, der
Statistikparser 150, die Synchronisation 137 und der Dialog-RPC-Adapter 724,
jeweils ohne Fehler. Geprüft sind insbesondere genau ein Motor-Tastendruck,
alle vier unterstützten Dialogtypen, vollständige Tabellen mit mehr als
16 Zeilen, ein Testkill mit dem importierten Gesamtstand sowie die manuelle
Erholung nach einem Timeout und das Speichern ohne Geldangaben.

Die ASI umfasst 18.345.984 Bytes. SHA-256:
`bd2774897c3ae9db23e79b781682708c016de60f716f9476408a05dc017c14f7`.
PE32/x86, Windows-Imports und der bytegleiche eingebettete Helfer wurden
geprüft. Der native Helfertest umfasst 18 erfolgreiche Prüfungen mit harmlosen
Testprozessen. Die ASI im ZIP entspricht der separat ausgelieferten Datei.
[Maschinenlesbare Ergebnisse](r5-validation.json) ergänzen die Paketnachweise.

Frühere r4-Ergebnisse gelten ausschließlich für die damalige Binärdatei. Ein
echter Ingame-Test mit dem konkreten Server und Launcher steht aus. Auch der
neue Ablauf muss die echte Serverantwort abwarten; er fügt keine feste
Wartepause nach deren Empfang ein.

## Historischer Prüfstand r4 – 25. Oktober 2026

Der damalige Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r4**. Die
[Bedienungsbeschreibung](../NLBinder-0.1.1-r4-DE.md) und der
[r4-Nachtrag der Änderungsübersicht](CHANGELOG-DE.md#10-nachtrag-r4--25-oktober-2026)
beschreiben die Änderungen seit r3. Die historischen r3-Angaben weiter unten
gelten weiterhin für die dort ausdrücklich genannte frühere Binärdatei.

Die eindeutig ausgelieferte r4-Datei und ihre Messwerte werden zusammen
bereitgestellt:

- [Keybinder.asi](../../dist/NLBinder-0.1.1-r4/Keybinder.asi)
- [SHA-256](../../dist/NLBinder-0.1.1-r4/SHA256SUMS.txt)
- [Build- und Prüfangaben](../../dist/NLBinder-0.1.1-r4/BUILD-INFO.txt)
- [PE-/Import-/Ressourcenprüfung](../../dist/NLBinder-0.1.1-r4/PRUEFBERICHT.json)
- [Nativer Helfertest](../../dist/NLBinder-0.1.1-r4/RECONNECT-TEST.json)

### Geändertes Verhalten gegenüber r3

- Der Damagelog misst HP und Weste direkt und getrennt. Eine Abnahme von
  mindestens 1 HP in einem der Werte wird lokal ausgegeben, auch bei
  Fallschaden oder serverseitigem SetHealth/SetArmour. Es wird kein bestätigendes
  Damage-RPC vorausgesetzt. Die getrennte Hitsound-Unterdrückung bleibt erhalten.
- `{city}` bezeichnet die GTA-Stadt beziehungsweise das übergeordnete Gebiet,
  `{bezirk}` den Bezirk. Die Hilfe bietet je Wert eine kleine Schreibweise an;
  unzuverlässige Ereignisvariablen werden dort nicht mehr angeboten.
- **Kill simulieren** sendet den aktuellen Killtext tatsächlich im gewählten
  Chatkanal, mit `[Testkill]` nach einem gegebenenfalls vorhandenen Slash-Befehl.
  Der Knopf verändert keine Zähler und braucht keinen echten Kill.
- Auf ausdrücklichen Benutzerwunsch wird nach dem Login im ersten sicheren
  Spielzustand einmal `/stats` gesendet. Die eigene Antwort wird gelesen und
  lokal gespeichert. Der eigene automatisch angeforderte Chatblock wird
  verborgen; nur eine identifizierte eigene Statistik-Messagebox wird
  geschlossen. Fremde Statistikblöcke und normale Chatnachrichten bleiben
  sichtbar. Es gibt keine Abfrageschleife und keine Erfolgsmeldung ohne Daten.
- Der automatische Motorstart bewertet den Motor erst nach vollständig
  erkanntem Fahrersitz und der bestehenden Wartezeit von 1,5 Sekunden. Ein
  vorübergehendes EngineOn beim Einsteigen verwirft die Planung nicht mehr.
  Alte „Motor Startet“-Chattexte gelten nicht als neue Bestätigung. Die
  Wiederholungsphase beginnt erst mit dem echten Tastendruck; Chat-/Binder-
  Eingaben bleiben geschützt.

### Zusätzliche r4-Prüfpfade

| Bereich | Implementierung | Tests |
| --- | --- | --- |
| Direkte HP-/Westenabnahme und Spielphasen | [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h), [SAMPBridge.cpp](../../src/SAMPBridge.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Stadt/Bezirk und Variablenkatalog | [LocationNames.h](../../include/LocationNames.h), [MessageVariables.h](../../include/MessageVariables.h), [ActionExecutor.cpp](../../src/ActionExecutor.cpp) | [LocationVariableTests.cpp](../../tests/LocationVariableTests.cpp) |
| Testkill und Chatkanal | [KillMessageTest.h](../../include/KillMessageTest.h), [MainWindowUI.cpp](../../src/MainWindowUI.cpp) | [KillMessageTestTests.cpp](../../tests/KillMessageTestTests.cpp), [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| Eigene Statistik, Cache und Dialogabschluss | [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp), [ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h), [SettingsManager.cpp](../../src/SettingsManager.cpp) | [ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp), [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp) |
| Nur den angeforderten Statistikblock verbergen | [JobChatAdapter.cpp](../../src/JobChatAdapter.cpp) | [JobChatAdapterTests.cpp](../../tests/JobChatAdapterTests.cpp) |
| Binärprüfung und Paketgleichheit | [verify_revision4_artifact.py](../../tools/verify_revision4_artifact.py), [package_revision4.ps1](../../tools/package_revision4.ps1) | PE32/x86, Ressource 300, Helfer- und Paket-SHA-256 |

### Ausgeführte Prüfungen für r4

Die vollständige Testsuite und der anschließende Win32-Release-Build sind mit
Exitcode 0 abgeschlossen. Die SAMPBridge-Suite meldet 401 erfolgreiche
Prüfungen und 0 Fehler, darunter Fallschaden und die drei neuen Motorstartfälle.
Die zusätzlichen Tests melden 53 Prüfungen für Testkill, 88 für Ortsvariablen,
110 für den Statistikparser und 57 für die Synchronisation, jeweils ohne Fehler.

Die geprüfte ASI umfasst 18.324.992 Bytes. Ihre SHA-256 lautet
`b9344521a38573a9252879bb362c354b85561fc3647f53a8a36f97f9b5b44450`.
PE32/x86, Windows-Imports und die eingebettete Helferressource 300 wurden
geprüft. Der enthaltene Helfer entspricht bytegenau dem geprüften Build;
der native Helfertest umfasst 18 erfolgreiche Prüfungen mit harmlosen
Testprozessen. Die ASI im ZIP und die separat ausgelieferte Datei haben dieselbe
Prüfsumme. [Maschinenlesbare Ergebnisse](r4-validation.json) ergänzen die
verlinkten Paketnachweise.

Ein echter Ingame-Test auf dem konkreten Server mit dem installierten Launcher
steht aus. Die früheren Testergebnisse gelten ausschließlich für ihren
jeweiligen älteren Binärstand.

## Historischer Prüfstand r3

Diese Übersicht bezieht sich auf **NLBinder v.0.1.1 by KOBiRO, Arbeitsfassung r3 mit Damagelog und bereinigten Killtext-Variablen**, Stand 24. Oktober 2026. Die ursprüngliche Grundlage ist der Perkys/NLBinder.

Die [vollständige Änderungsübersicht](CHANGELOG-DE.md) erklärt alle im Bearbeitungsverlauf umgesetzten Erweiterungen und Korrekturen. Für die eigene Beurteilung können die verlinkten Implementierungen und Tests direkt im Repository gelesen werden.

## Vergleichsgrundlage

Der ursprüngliche Archivstand `NLBinder-master.zip` wurde als Basis für den Dateivergleich identifiziert:

| Merkmal | Wert |
| --- | --- |
| Archiv-SHA-256 | `a328e78eea63420e9bfe9c084883100177fe0fd9c2d62c4fcaef38c0a44801e9` |
| ZIP-Kommentar | `92ce21971bee2d166d830e23ad20950f1dc98921` |

Der Vergleich zwischen dem importierten Originalarchiv und dem bearbeiteten Stand zeigt tatsächlich geänderte Dateien. Die dafür neu angelegten Import-/Änderungscommits werden als nachträgliche Veröffentlichung behandelt; sie behaupten keine rückwirkende ursprüngliche Autoren- oder Commitfolge. Die originale Projektbeschreibung wird als `docs/ORIGINAL-README.md` erhalten.

Die wiederhergestellten Bibliotheken haben einen eigenen Commit und den Tag
`build-dependencies-baseline`. Der Vergleich dieses Tags mit `v0.1.1-r3` zeigt
die Binder-Änderungen ohne die Masse der SDK-Neuzugänge. Die gesamte
Originalbasis bleibt über `original-archive-92ce219` erhalten.

## Eindeutiger ausgelieferter Binärstand

| Merkmal | Geprüfter Wert |
| --- | --- |
| Dateiname | `Keybinder.asi` |
| Größe | `18.295.296` Bytes |
| Format | PE32, x86-DLL / Win32 Release |
| Ziel | GTA San Andreas 1.0 US, SA-MP 0.3.DL-R1 |
| SHA-256 | `fc27a8a39fc2f78c40f1b686d5a71dcc0c2956854d1250278a84bd79200c7cef` |
| Eingebetteter Reconnect-Helfer | Ressource 300, 101.376 Bytes |
| Helfer-SHA-256 | `0f4c59362d56f7e38e66c71333d7f1b2c5cf099888113e4184b12056dfa9ed25` |

Die Formatprüfung bestätigte außerdem, dass die eingebetteten Helferbytes mit dem separat gebauten Helfer übereinstimmen. Die Windows-Importliste enthielt `ADVAPI32.dll`, `IMM32.dll`, `KERNEL32.dll`, `ntdll.dll`, `ole32.dll`, `SHELL32.dll`, `USER32.dll`, `WINHTTP.dll` und `WS2_32.dll`. Es wurde keine zusätzliche dynamische Visual-C++-Runtime-Abhängigkeit festgestellt.

Die Prüfsumme identifiziert genau die ausgelieferte Datei. Sie ist keine Behauptung, dass ein erneuter Build bytegleich sein muss: Compiler-/SDK-Versionen und Buildmetadaten können sich unterscheiden.

## Sinnvolle Reihenfolge beim Lesen des Codes

| Bereich | Einstieg | Ergänzende Tests |
| --- | --- | --- |
| Start, Rückrufe und `/reconnect`-Registrierung | [Main.cpp](../../src/Main.cpp) | [SourceContractTests.cpp](../../tests/SourceContractTests.cpp) |
| Spiel-/Chat-Anbindung, Motorstart, Kill-/Todeserkennung, Variablen und Damagelog | [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [SAMPBridge.h](../../include/SAMPBridge.h) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Messwertabgleich für Hitsound und Damagelog | [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [HitsoundTests.cpp](../../tests/HitsoundTests.cpp) |
| Einzelne lokale Give/Take-Damage-Ereignisse | [DamageRPCAdapter.cpp](../../src/DamageRPCAdapter.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Killfeed und Statistik-Deduplizierung | [KillfeedAdapter.cpp](../../src/KillfeedAdapter.cpp), [KillTracker.cpp](../../src/KillTracker.cpp) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |
| Online-Datenherkunft | [OnlineDataManager.cpp](../../src/OnlineDataManager.cpp), [OnlinePlayerInfoService.cpp](../../src/OnlinePlayerInfoService.cpp) | [OnlineDataManagerTests.cpp](../../tests/OnlineDataManagerTests.cpp), [OnlinePlayerInfoTests.cpp](../../tests/OnlinePlayerInfoTests.cpp) |
| Inhalte und Fenster der Overlays | [OnlineOverlayController.cpp](../../src/OnlineOverlayController.cpp), [OnlineOverlayRenderer.cpp](../../src/OnlineOverlayRenderer.cpp) | [OnlineOverlayRendererTests.cpp](../../tests/OnlineOverlayRendererTests.cpp) |
| Oberfläche, Fraktionen und Schalter | [OnlineUI.cpp](../../src/OnlineUI.cpp), [MainWindowUI.cpp](../../src/MainWindowUI.cpp), [KillTrackerUI.cpp](../../src/KillTrackerUI.cpp) | [OnlineUITests.cpp](../../tests/OnlineUITests.cpp), [SourceContractTests.cpp](../../tests/SourceContractTests.cpp) |
| Speicherung und Migration | [SettingsManager.cpp](../../src/SettingsManager.cpp), [PersistentPaths.cpp](../../src/PersistentPaths.cpp) | [OnlineSettingsTests.cpp](../../tests/OnlineSettingsTests.cpp), [RegressionTests.cpp](../../tests/RegressionTests.cpp) |
| Jobdaten und Skilltrees | [jobs_registry.json](../../res/jobs_registry.json), [JobParser.cpp](../../src/JobParser.cpp), [JobsUI.cpp](../../src/JobsUI.cpp) | [JobRegistryTests.cpp](../../tests/JobRegistryTests.cpp), [JobParserTests.cpp](../../tests/JobParserTests.cpp), [JobsUITests.cpp](../../tests/JobsUITests.cpp) |
| Tacho und Server-Textdraws | [SpeedometerManager.cpp](../../src/SpeedometerManager.cpp), [TextDrawAdapter.cpp](../../src/TextDrawAdapter.cpp) | [SpeedometerFocusedTests.cpp](../../tests/SpeedometerFocusedTests.cpp) |
| Vorbereitung und lokales Beenden für Reconnect | [ReconnectManager.cpp](../../src/ReconnectManager.cpp) | [ReconnectManagerTests.cpp](../../tests/ReconnectManagerTests.cpp) |
| Reconnect-Helfer und Launcher-Start | [ReconnectHelper.cpp](../../tools/ReconnectHelper.cpp) | [ReconnectWorkerFixture.cpp](../../tests/ReconnectWorkerFixture.cpp), [test_reconnect_worker.py](../../tools/test_reconnect_worker.py) |
| Gemeinsamer Variablenkatalog | [MessageVariables.h](../../include/MessageVariables.h), [MessageVariablesUI.h](../../include/MessageVariablesUI.h) | [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp) |

## Verhalten gegenüber Server, Betriebssystem und Launcher

- Die Online-Zuordnung liest die Nova-API `https://nova-legacy.de/api/players/online`; der Datenmanager verwendet standardmäßig zehn Sekunden Abfrageabstand. Es handelt sich um HTTP-Abfragen, nicht um eine rein offline arbeitende Zuordnung.
- Es werden für die neuen Zähler keine versteckten `/stats`-/`/oldstats`-Befehle und für die entfernte AFK-Anzeige keine eigenen `/id`-Abfragen gesendet. Bereits empfangene eigene Statistikblöcke können die gespeicherten Zähler aktualisieren.
- Der frei konfigurierte Killtext kann bei eingeschalteter Sendefunktion eine normale Chatnachricht oder einen selbst gewählten Serverbefehl senden. Das ist eine ausdrücklich aktivierbare automatische Ausgabe nach einem erkannten eigenen Kill.
- Todesbenachrichtigung und Damagelog schreiben ausschließlich in den eigenen Chat. Sie senden diese Texte nicht an den Server.
- Der automatische Motorstart betätigt Z als simulierte Eingabe nur im vorgesehenen Fahrer-/Eingabezustand und versucht den Start begrenzt erneut.
- Der Tacho unterdrückt die TachoV2-Serveranzeige beim Zeichnen reversibel; die Textdraws und ihre Aktualisierungen werden nicht zerstört.
- `/reconnect` schreibt und startet einen in der ASI eingebetteten Helfer im Binder-Datenbereich. Nach Bereitschaftsbestätigung und Speicherung fordert der Binder das lokale Spielende an. Der Helfer wartet auf das tatsächliche Prozessende und startet dann den gewählten Launcher.
- Für Nova versucht der Helfer ausschließlich den normalen aktivierten **Spiel starten**-Knopf per Windows UI Automation. Anmeldung, Updates, Einstellungen und Rückfragen werden nicht automatisiert. Bei fehlendem Knopf ist ein eigener Klick erforderlich.
- Es sind weiterhin ein ASI-Loader und die im Projekt vorhandenen nativen Spiel-/SA-MP-Hooks nötig. Diese Beschreibung stellt die technischen Änderungen dar; sie ersetzt nicht die Prüfung der Projektleitung für den konkreten Server.

## Durchgeführte automatisierte Prüfungen

Der abschließende vollständige Testlauf der ausgelieferten r3-Fassung endete mit **Exitcode 0**. Die vier ausgeführten Programme waren der Binder-Testverbund, RSS-Parser-Tests, Online-Player-Info-Tests und Online-Data-Manager-Tests. Der anschließende Win32-Release-Build war erfolgreich.

Ausgewählte nachweisbare Teilprüfungen im finalen Testlauf:

| Teilbereich | Ergebnis |
| --- | --- |
| SAMPBridge, Damagelog, Spielphasen, Zähler, Variablen und Motorstart | 395 Assertions bestanden, 0 Fehler |
| Reconnect-Planung und Ablauf | 89 Prüfungen, 0 Fehler |
| Online-Overlay-Fenster und Positionen | 214 Assertions bestanden, 0 Fehler |
| Online-Einstellungen und Migration | 245 Assertions bestanden, 0 Fehler |
| Jobregistrierung | 107 Assertions bestanden, 0 Fehler |
| Jobparser | 175 Assertions bestanden, 0 Fehler |
| Hitsounds | 142 Assertions bestanden, 0 Fehler |
| Zusätzlicher nativer Reconnect-Helfertest | 18 Prüfungen, 0 Fehler |

Der native Helfertest verwendete ausschließlich eigene harmlose Testprozesse: einen wartenden Elternprozess und einen Dummy-Launcher, der empfangene Argumente protokolliert. Er prüfte die Bereitschaftsbestätigung vor dem Elternprozessende, das Ausbleiben eines vorzeitigen Launcher-Starts, den Start nach tatsächlichem Ende, die unveränderte Argumentübergabe einschließlich Leerzeichen/Anführungszeichen und den Abbruch bei fehlendem Bereitschaftsereignis. Weder das echte Spiel noch ein installierter Launcher wurde dabei gestartet oder beendet.

Die Damagelog-Prüfungen umfassen unter anderem mehrere Treffer in einer Messung, späte RPCs, serverseitige Gesundheitsänderungen, geänderte tatsächliche Schadenshöhen, fehlenden messbaren Schaden, den letzten Treffer vor dem Tod, Bruchteile, Login/Respawn, Umweltfilter, ausgeschaltete Zähler/Hitsounds sowie Ein-/Ausschalten ohne nachträgliche Meldungen. Die Variablenprüfung ersetzt alle im Killkontext angebotenen Tags und prüft aktuelle Pedwerte anstelle alter In-Car-/On-Foot-Synchronisationswerte.

Diese Angaben bedeuten **keinen erfolgreich durchgeführten Ingame-Test**. Ein echter Login, Kampf, Motorstart, Tacho und kompletter Reconnect mit der konkreten Server-/Launcher-Version wurden nicht als Live-Test bestätigt.

## Nachbauen und selbst prüfen

Benötigt werden Visual Studio 2022 Build Tools mit C++/v143, Windows SDK 10.0 und die [dokumentierten Abhängigkeiten](../build-dependencies.md). Das Projekt zielt auf Win32. Die Skripte verwenden `vswhere`, um die lokale Visual-Studio-Installation zu finden.

Für einen final zu verteilenden Stand ist folgende Reihenfolge sinnvoll:

```bat
tools\run_tests.bat
tools\build_release.bat
```

Die Tests bauen auch den eingebetteten Helfer. Deshalb sollte die abschließend zu prüfende ASI nach dem Testlauf gebaut werden. Ausgabe: `build\Release\Keybinder.asi`.

Die Dateiprüfsumme kann anschließend beispielsweise in PowerShell ermittelt werden:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath .\build\Release\Keybinder.asi
```

Für den gesonderten Helfertest wird zunächst [ReconnectWorkerFixture.cpp](../../tests/ReconnectWorkerFixture.cpp) mit demselben Win32-C++-Werkzeug als `build\ReconnectWorkerFixture.exe` gebaut; danach wird [test_reconnect_worker.py](../../tools/test_reconnect_worker.py) mit Python gestartet. Er verwendet nur diese Testdatei und den gebauten Helfer, keine installierten Spiele oder Launcher.

## Grenzen der Änderungs- und Funktionsaussagen

1. Die Änderungsübersicht ist eine nachträgliche Dokumentation des zusammen erarbeiteten Stands. Die bekannte Archivbasis ermöglicht einen Dateivergleich. Sie rekonstruiert keine vollständige historische Einzelcommitfolge zwischen den Arbeitsfassungen.
2. Zugehörigkeit und Online-Status hängen von der verfügbaren Nova-API ab. Bei fehlenden Daten kann keine sichere Zugehörigkeit ergänzt werden. Eine Live-Entfernung oder AFK-Auswertung ist nicht enthalten.
3. Server-Gesamtstände der Zähler müssen tatsächlich empfangen werden. Ein Statistik-Kill kann als Ereignis erkannt werden, ohne einen sicheren Namen oder Waffentyp zu liefern.
4. Ohne passende Einzelereignisse kann der Damagelog mehrere zwischen zwei Messungen eingetroffene Treffer nur als gemessenen Gesamtverlust ausgeben. Ein bloßes SetHealth ohne Combat-Bestätigung bleibt als Scriptänderung unprotokolliert.
5. Nova-Anmeldung, Updates und der zugängliche normale Spielstartknopf beeinflussen den automatischen Neustart. Der Helfer umgeht diese Abläufe nicht.
6. Für Bauarbeiter waren keine spezifischen NPC-/Aufgabentexte belegt; gemeinsame Lohn-/Skillmeldungen und der vorgegebene Skilltree sind vorhanden.
7. Automatisierte Prüfungen und Quellenprüfung belegen die geprüften Logikpfade und den Build. Die abschließende Bewertung auf dem realen Server muss die dort beobachteten Nachrichten, Einstellungen und Regeln berücksichtigen.

Spätere Erweiterungen sollten mit ihrem jeweiligen Quellcommit, angepasster Änderungsübersicht, Testnachweisen und neuer ASI-Prüfsumme veröffentlicht werden. Ein neuer Build sollte nicht unter der Prüfsumme dieser r3-Fassung beschrieben werden.
