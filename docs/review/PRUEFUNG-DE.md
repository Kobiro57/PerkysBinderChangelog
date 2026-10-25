# Hinweise zur Prüfung durch die Projektleitung

## Aktueller Stand r7 – 25. Oktober 2026

Der aktuelle Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r7**.
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
