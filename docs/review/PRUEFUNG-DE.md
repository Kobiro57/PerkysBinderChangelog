# Hinweise zur Prüfung durch die Projektleitung

## Aktueller Stand r4 – 25. Oktober 2026

Der aktuelle Quellstand ist **NLBinder v.0.1.1 by KOBiRO, r4**. Die
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
