# Änderungen am Perkys Binder – NLBinder v.0.1.1 by KOBiRO

Stand: 25. Oktober 2026, Arbeitsfassung **r6 mit korrigierter Zuordnung des Statistik-Titels mit `»`**. Die Abschnitte 1 bis 9 dokumentieren die bisherigen Änderungen; Abschnitt 10 beschreibt r4, Abschnitt 11 r5 und Abschnitt 12 die anschließende r6-Korrektur.

Die Grundlage ist der vorhandene Perkys/NLBinder. Die Änderungen erweitern dessen bestehende Oberfläche und verwenden weiterhin die vorhandenen ImGui-Komponenten, Karten, Schalter und Darstellungseinstellungen. Der Ordner KOBiRO_ASI diente als funktionale Referenz für die Overlay-Erweiterung; seine Oberfläche wurde nicht als neue Binder-Oberfläche übernommen.

Dieses Dokument beschreibt die im Bearbeitungsverlauf umgesetzten Änderungen einschließlich späterer Korrekturen. Es rekonstruiert keine ursprüngliche Git-Commitfolge. Maßgeblich für die Prüfung sind der veröffentlichte Quellstand, die zugehörigen Tests und die [Prüfhinweise](PRUEFUNG-DE.md). Die Kurzbezeichnungen r2 und r3 bezeichnen ausgelieferte Arbeitsfassungen, keine behaupteten ursprünglichen Git-Tags.

Als Vergleichsgrundlage wurde das vorhandene ursprüngliche Archiv `NLBinder-master.zip` identifiziert. Sein SHA-256 lautet `a328e78eea63420e9bfe9c084883100177fe0fd9c2d62c4fcaef38c0a44801e9`; der ZIP-Kommentar enthält `92ce21971bee2d166d830e23ad20950f1dc98921`. Ein Import dieses Archivstands und ein anschließender Commit des bearbeiteten Stands erlauben einen echten Dateivergleich. Diese nachträglichen Import-Commits ersetzen keine ursprüngliche Entwicklungshistorie.

## 1. Jobs: Holzfäller und Bauarbeiter

Beide Jobs wurden in die vorhandene Jobregistrierung, Übersicht und Detailansicht eingepflegt. Die eingebettete Registrierung enthält 17 Jobs. Gemeinsame Formate für Joblohn und absoluten Job-Skill werden weiterverwendet. Geldbeträge mit deutschen Tausenderpunkten, beispielsweise `2.940$`, werden berücksichtigt.

Für Holzfäller wurden die belegten Aufgabenformate von Vince Redwood, Baumfäll- und Setzlingsaufgaben sowie zusätzlich gefundenes Holz ergänzt. Für Bauarbeiter lagen in den verwendeten Belegen keine eigenen NPC-/Aufgabentexte vor; die gemeinsamen Lohn- und Skillformate sowie der vorgegebene Skilltree sind unterstützt. Diese Grenze ist in den [Jobhinweisen](../jobs-support.md) ausdrücklich dokumentiert.

### Holzfäller-Skilltree

| Punkte | Freischaltung / Belohnung |
| ---: | --- |
| 50 | 5 Bäume fällen |
| 100 | Holzfäller Skin freischalten |
| 250 | Privatfahrzeug: Bobcat |
| 500 | +10% Lohn |
| 1000 | +25% Lohn |
| 1500 | Job-Achievement freigeschaltet |

### Bauarbeiter-Skilltree

| Punkte | Freischaltung / Belohnung |
| ---: | --- |
| 25 | Großbaustellen werden freigeschaltet |
| 100 | Bauarbeiter Skin freischalten |
| 500 | +10% Lohn |
| 750 | Business: Blueberry Construction |
| 1000 | +25% Lohn |
| 1500 | Job-Achievement freigeschaltet |

Skilltree-Fortschritt und Animation berücksichtigen gespeicherte Skillwerte und manuelle Punktekorrekturen. Die Tabellen zeigen die vorgegebenen Belohnungen an; sie vergeben selbst keine serverseitigen Freischaltungen.

Quellen: [jobs_registry.json](../../res/jobs_registry.json), [JobRegistry.cpp](../../src/JobRegistry.cpp), [JobParser.cpp](../../src/JobParser.cpp), [JobsUI.cpp](../../src/JobsUI.cpp). Tests: [JobRegistryTests.cpp](../../tests/JobRegistryTests.cpp), [JobParserTests.cpp](../../tests/JobParserTests.cpp), [JobAdjustmentTests.cpp](../../tests/JobAdjustmentTests.cpp), [JobsUITests.cpp](../../tests/JobsUITests.cpp).

## 2. Online- und Overlay-Funktionen

### Neue dauerhafte Anzeigen

Ort, Fortbewegungsmittel und Pedslots wurden in den vorhandenen Overlay-Stil integriert. Die Anzeigen verwenden die vorhandenen Spiel-/Spielerdaten und sind einzeln schaltbar und verschiebbar. Bestehende Fraktions-, Gang- und Wanted-Anzeigen bleiben Teil der Oberfläche.

Unter Darstellung heißt das bisher missverständliche gemeinsame Ziel jetzt **Allgemeine Overlays**. Die Beschreibung nennt seinen tatsächlichen Geltungsbereich: eigene Fraktion, Gang-Übersicht, Ort, Fortbewegungsmittel, Pedslots sowie Freunde- und Feinde-Radar. Eigene Gang und Wanted-Liste haben weiterhin ihre eigenen Darstellungseinstellungen.

### Zwei eigenständige Radars

- **Freunde anzeigen** steuert ausschließlich **Freunde-Radar**.
- **Feinde anzeigen** steuert ausschließlich **Feinde-Radar**.
- Beide besitzen eigene Fenster, Inhalte, Anker und gespeicherte Positionen. Sie können gleichzeitig eingeblendet und unabhängig voneinander verschoben werden.
- Bei alten Einstellungen übernimmt das Freunde-Radar die bisherige gemeinsame Position. Das Feinde-Radar beginnt zunächst um 120 Pixel von derselben Bildschirmkante versetzt. Bereits gespeicherte neue Einzelpositionen haben Vorrang.
- Die gemeinsame Verwaltung gespeicherter Spieler heißt **Freunde & Feinde**. Sie erzeugt kein gemeinsames Spieler-Radar mehr.
- Längere Namen und Fraktionsangaben werden bei der Breitenberechnung berücksichtigt; die Anzeigen bleiben an die Bildschirmgröße gebunden.

Gespeicherte Spieler lassen sich über Namen oder aktuelle Spieler-ID hinzufügen. Fraktion, Gang oder Zivilist werden automatisch aus dem vorhandenen Online-Datenbestand zugeordnet. Die manuellen Fraktions-/Notizfelder entfallen. Nicht verfügbare Zugehörigkeiten erscheinen als **Unbekannt**. Das Zeilenformat bleibt beispielsweise:

```text
Freund: Name [Red County Brotherhood] | ID 7
```

**Red County Brotherhood** wurde in die Fraktionszuordnung, Gruppen und relevanten Filter aufgenommen.

### Entfernte Online-Funktionen

Entfernt wurden Entfernungsermittlung und Live-Entfernungsangaben, Online-Watcher, Online-Alarme im Chat, Alarmbanner und Feind-Sound. Alte Einstellungen aktivieren diese entfernten Funktionen nicht wieder.

Die zwischenzeitlich in r2 enthaltene, auf tatsächlich empfangenen `/id`-Antworten beruhende AFK-Anzeige mit 30-Sekunden-Ablauf wurde auf Wunsch vollständig entfernt: kein AFK-Parser, kein AFK-Zwischenspeicher und kein AFK-Zusatz im Radar. Der endgültige r3-Stand enthält diese AFK-Funktion nicht.

### Datenherkunft

Die automatische Online-Zuordnung verwendet lesende HTTP-Abfragen der vorhandenen Nova-Online-Spielerliste unter `https://nova-legacy.de/api/players/online`. Der Online-Datenmanager fragt standardmäßig im Abstand von zehn Sekunden ab und kennzeichnet veraltete beziehungsweise nicht verfügbare Daten. Die bestehende manuelle `/info`-Abfrage verwendet ebenfalls diese API. Die Zustimmung zur Einsicht in ein UCP während der Entwicklung bedeutet nicht, dass der Binder UCP-Anmeldedaten verwendet; diese Online-Funktion benötigt keine UCP-Anmeldung im Plugin.

Die Online-API liefert keine Live-Entfernungen für das Radar. Der Binder sendet für die entfernte AFK-Funktion keine eigenen `/id`-Abfragen.

Quellen: [OnlineOverlayController.h](../../include/OnlineOverlayController.h), [OnlineOverlayController.cpp](../../src/OnlineOverlayController.cpp), [OnlineOverlayRenderer.cpp](../../src/OnlineOverlayRenderer.cpp), [OnlineUI.cpp](../../src/OnlineUI.cpp), [OnlineDataManager.cpp](../../src/OnlineDataManager.cpp), [OnlinePlayerInfoService.cpp](../../src/OnlinePlayerInfoService.cpp), [SettingsManager.cpp](../../src/SettingsManager.cpp). Tests: [OnlineOverlayRendererTests.cpp](../../tests/OnlineOverlayRendererTests.cpp), [OnlineSettingsTests.cpp](../../tests/OnlineSettingsTests.cpp), [OnlineDataManagerTests.cpp](../../tests/OnlineDataManagerTests.cpp).

## 3. Kill- und Todeszähler

Unter **Einstellungen → General** wurde eine Karte **Kill- & Todeszähler** im bestehenden Schalterstil ergänzt. Kills und Tode sind unabhängig aktivierbar. Der Killzähler funktioniert auch bei deaktivierter Schadenstatistik.

Die Erkennung gleicht neue Killfeed-Einträge, empfangene Todesmeldungen und tatsächlich empfangene eigene Server-Statistikstände ab. Bereits vorhandene Killfeed-Zeilen werden beim Aktivieren oder Wiedererscheinen der Anzeige zunächst als Ausgangsstand übernommen und nicht rückwirkend als neue Kills gezählt. Pro Leben wird derselbe Tod nicht durch mehrere Erkennungswege doppelt gezählt. Ein früher als der native Wasted-Zustand eingetroffenes Todesereignis und ein anschließender Respawn werden berücksichtigt; es gibt keine pauschale fünfsekündige Sperre für den nächsten echten Tod.

Eigene Statistikblöcke werden nur mit passendem Spielernamen-Kontext übernommen. Fremde Statistikblöcke oder beliebige Chatzeilen mit Zahlen aktualisieren die eigenen Zähler nicht. Der Kontext läuft ab und wird nach Verwendung verbraucht. Ohne empfangenen Servergesamtstand verwendet der Binder seine gespeicherten und neu erkannten Ereignisse.

Der Binder sendet hierfür **keine versteckten `/stats`- oder `/oldstats`-Befehle**. Eine reine Statistik-Erhöhung enthält keinen sicheren Gegnernamen oder Waffentyp. Werden weder ein geeignetes Ereignis noch neue eigene Statistikstände übermittelt, kann daraus kein zusätzlicher sicherer Kill abgeleitet werden.

Bei aktiviertem Todeszähler erscheint ausschließlich lokal:

```text
[NL-BINDER] Du bist gestorben. Deine aktuelle Tode: 206
```

Die Anzeige des Gesamt-Killstands in der Statistikansicht verwendet den Tracker-Gesamtstand statt einer möglicherweise unvollständigen Summe der Waffenstatistiken.

Quellen: [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [KillTracker.cpp](../../src/KillTracker.cpp), [KillfeedAdapter.cpp](../../src/KillfeedAdapter.cpp), [DeathMessageRPCAdapter.cpp](../../src/DeathMessageRPCAdapter.cpp), [KillTrackerUI.cpp](../../src/KillTrackerUI.cpp), [MainWindowUI.cpp](../../src/MainWindowUI.cpp). Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [RegressionTests.cpp](../../tests/RegressionTests.cpp).

## 4. Frei wählbarer Killtext und Variablen

Neben den Zählerschaltern gibt es ein Textfeld für die Nachricht nach einem erkannten eigenen Kill sowie einen Variablenknopf im vorhandenen Stil. Der Text darf einen beliebigen normalen Chattext oder einen Befehl enthalten, beispielsweise `/f`, `/d` oder `/r`. Der Kanal ist nicht auf `/f` festgelegt. Ein leeres Feld sendet nichts. Für das automatische Senden müssen Killzähler und **Kill-Nachricht an Server senden** aktiviert sein.

Die Hilfe im Killzähler und in den Nachrichteneinstellungen verwendet nun denselben überschaubaren Variablenkatalog. Die direkt verfügbaren Variablen bleiben erhalten:

| Variablen | Inhalt |
| --- | --- |
| `{victim}`, `{killer}`, `{weapon}` | Namen und Waffe des Ereignisses; im eigenen Killtext ist der Killer der eigene Spieler |
| `{zone}`, `{ZONE}` | Eigene aktuelle Zone, normal oder in Großbuchstaben |
| `{hp}`, `{weste}` | Eigene aktuelle Lebenspunkte und Weste |
| `{total_kills}`, `{total_deaths}` | Gespeicherte Zählerstände des Binders |
| `{session_kills}`, `{session_deaths}` | Seit Spielstart erfasste Kills beziehungsweise Tode |
| `{kd}`, `{KD}` | Verhältnis aus den gespeicherten Zählerständen; beide Schreibweisen sind unterstützt |

Die geschweiften Klammern gehören zur Variable. Fehlende Namen und Waffenangaben werden als **Unbekannt** ausgegeben. Diese Variablen benötigen keine zusätzlichen Spieler- oder Statistikabfragen.

`{hp}` und `{weste}` lesen die aktuellen Werte des Spieler-Peds. Die frühere Priorisierung alter In-Car-Synchronisationsdaten wurde entfernt, damit nach dem Aussteigen keine alten Fahrzeugwerte erscheinen. Die Textersetzung liest Zonen- und Fahrzeugwerte nur, wenn die Vorlage sie tatsächlich verwendet.

`{carhp}` und `{carname}` wurden aus der Killtext-Hilfe entfernt, bleiben in der Textbelegung verfügbar und funktionieren aus Kompatibilitätsgründen weiterhin in bereits gespeicherten Vorlagen. Servernachrichten entfernen lokale Farbcodes; die bestehende Begrenzung auf 144 Zeichen nach der Ersetzung bleibt erhalten.

Quellen: [MessageVariables.h](../../include/MessageVariables.h), [MessageVariablesUI.h](../../include/MessageVariablesUI.h), [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [MainWindowUI.cpp](../../src/MainWindowUI.cpp), [KillTrackerUI.cpp](../../src/KillTrackerUI.cpp), [ActionExecutor.cpp](../../src/ActionExecutor.cpp). Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [SourceContractTests.cpp](../../tests/SourceContractTests.cpp), [RegressionTests.cpp](../../tests/RegressionTests.cpp).

## 5. Tacho und automatischer Motorstart

Der eigene Tacho wurde wieder aktivierbar gemacht. Er verwendet den tatsächlichen lokalen Spieler und Fahrerzustand. Fahrzeugwechsel und Verbindungswechsel setzen die Messung zurück. Geschätzte RPM werden nicht als echte Motordrehzahlmessung ausgegeben; fehlende Kraftstoffwerte werden nicht als vorhandene Messwerte erfunden.

Die vorhandenen Server-Textdraws für **TachoV2** werden während der eigenen Anzeige reversibel beim Zeichnen unterdrückt. Sie werden nicht gelöscht, und ihre eingehenden Aktualisierungen bleiben erhalten. Beim Deaktivieren kann die Serveranzeige wieder gezeichnet werden. Die Tacho-Einstellungen weisen weiterhin darauf hin, auf dem Server TachoV2 auszuwählen.

Der automatische Motorstart erkennt den Wechsel auf den Fahrersitz auch ohne die bisher vorausgesetzte Einstiegsmeldung. Er hält Z kurz und versucht den Start nur begrenzt erneut, solange der Motor aus ist. Ein geöffneter Chat, Dialoge, das Binder-Menü, interaktive Zusatzansichten beziehungsweise ein nicht aktives Spielfenster verhindern die Eingabe. Als Beifahrer wird keine Motortaste ausgelöst.

Quellen: [SpeedometerManager.cpp](../../src/SpeedometerManager.cpp), [SpeedometerUI.cpp](../../src/SpeedometerUI.cpp), [TextDrawAdapter.cpp](../../src/TextDrawAdapter.cpp), [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [Main.cpp](../../src/Main.cpp). Tests: [SpeedometerFocusedTests.cpp](../../tests/SpeedometerFocusedTests.cpp), [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp).

## 6. Reconnect

`/reconnect` wurde als lokaler Binder-Befehl ergänzt. Unter General gibt es den Schalter **Benutzt du den Nova Launcher?** sowie optionale Launcher-Pfade. Mit Haken wird Nova gewählt, ohne Haken OpenMP.

Der Binder prüft die Verbindung und den Launcher, schreibt den in der ASI enthaltenen Helfer in seinen Datenordner und startet ihn mit einer Bereitschaftsbestätigung. Erst nach erfolgreicher Vorbereitung und Speicherung der Binder-Daten wird die lokale SA-MP-Beenden-Funktion aufgerufen. Der Helfer wartet auf das tatsächliche Ende des bisherigen Spielprozesses und startet erst danach den Launcher.

Die erste Umsetzung versandte `/q` über den allgemeinen Serverbefehlsweg; dadurch konnte der Helfer auf ein ausbleibendes Spielende warten. In r3 wird stattdessen der lokale Client-Quit-Handler aufgerufen. Dieser kann zunächst zurückkehren und das Beenden asynchron anfordern. Der Binder lässt diesen Ablauf zu. Läuft das Spiel nach acht Sekunden weiterhin, beendet er seinen wartenden Helfer, meldet den Fehlschlag lokal und erlaubt einen neuen Versuch.

OpenMP erhält Serveradresse, Port, Spielernamen und Spielordner als einzeln maskierte Argumente. Es wird keine Shell zur Ausführung dieser Argumente benutzt. Nova wird über den normalen Launcher gestartet. Der Helfer versucht dort per Windows UI Automation ausschließlich den eindeutig gefundenen, aktivierten Knopf **Spiel starten** zu betätigen. Er automatisiert keine Anmeldung, Updates oder Rückfragen. Falls der Knopf nicht verfügbar ist, bleibt ein eigener Spielstart im Launcher erforderlich.

Der Helfer beendet selbst nicht das Spiel. Ein Abbruch des vorbereiteten Ablaufs beendet nur den durch den Binder gestarteten Helfer. Ein ausbleibendes Elternprozessende führt nach begrenzter Wartezeit nicht später unkontrolliert zum Start einer zweiten Spielsitzung.

Quellen: [ReconnectManager.h](../../include/ReconnectManager.h), [ReconnectManager.cpp](../../src/ReconnectManager.cpp), [ReconnectHelper.cpp](../../tools/ReconnectHelper.cpp), [Main.cpp](../../src/Main.cpp), [Keybinder.rc](../../Keybinder.rc). Tests: [ReconnectManagerTests.cpp](../../tests/ReconnectManagerTests.cpp), [ReconnectWorkerFixture.cpp](../../tests/ReconnectWorkerFixture.cpp), [test_reconnect_worker.py](../../tools/test_reconnect_worker.py).

## 7. Hitsound beim Einloggen

Der Lebenswertvergleich berücksichtigt nur den verbundenen, aktiven und gespawnten lokalen SA-MP-Spieler. Ein Login- oder Vorschau-Ped allein reicht nicht mehr aus, um einen Treffer auszulösen. Verbindungsaufbau, Spectating, Klassenauswahl und das Warten auf die Spawn-Antwort setzen die Ausgangswerte zurück. Ein Pedwechsel, ungültige Werte, Respawn und Reconnect setzen sie ebenfalls zurück.

Der erste gültige Wert nach einem solchen Wechsel etabliert den Ausgangsstand. Danach funktionieren echte Treffer ohne zusätzliche mehrsekündige Stummschaltung. Auch ausgehende Hitsounds verwenden die Spielphasenprüfung.

Quellen: [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h). Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [HitsoundTests.cpp](../../tests/HitsoundTests.cpp).

## 8. Damagelog

Unter **Einstellungen → General → Gameplay** wurde direkt unter **Automatischer Motorstart** der Schalter **Damagelog** ergänzt. Er ist standardmäßig aus, lässt sich unabhängig aktivieren und wird gespeichert. Meldungen erscheinen ausschließlich im eigenen Chat:

```text
[NL-BINDER] Du hast -25 HP Schaden erhalten.
[NL-BINDER] Du hast -12,5 HP Schaden erhalten.
```

Erfasst wird der tatsächlich gemessene Verlust an HP und Weste. Eingehende Trefferereignisse werden auf dem Spielthread mit den Messwerten abgeglichen. Passende einzelne RPC-Ereignisse können auch eine gemeinsame Messwertabnahme in mehrere Schusszeilen aufteilen. RPC und Messung erzeugen nicht zwei Meldungen für denselben Treffer. Ein Trefferereignis ohne gemessenen Verlust erfindet keinen Schaden.

Ohne passende Einzelereignisse wird eine geeignete gemessene Abnahme einmal als Gesamtverlust angezeigt. Weicht der rohe Waffenschaden vom tatsächlich abgezogenen Wert ab, etwa durch Serverregeln oder Overkill, wird kein proportionaler Einzelschaden geschätzt. Der letzte gemessene Treffer vor dem Tod wird einmal berücksichtigt; negative verbleibende HP werden nicht als zusätzlicher Overkill gezählt.

Login, Spawn, Heilung und reine unbestätigte Script-Resets erzeugen keine Trefferzeile. Ein veralteter Umweltschaden-Code kann bei serverseitig angewandtem Schaden nur zusammen mit einem passenden tatsächlichen Combat-Ereignis als Treffer bestätigt werden. Ohne solches Ereignis bleibt Umweltschaden still.

Der Log funktioniert auch bei ausgeschalteten Kill-/Todeszählern, Schadenstatistiken und Hitsounds. Die Schadensbeobachtung wurde dafür von der Statistikaktivierung getrennt. Beim Abschalten werden ausstehende Logeinträge verworfen; erneutes Einschalten erzeugt keine nachträglichen Meldungen für Schaden aus der ausgeschalteten Phase. Der RPC-Callback reiht nur Daten ein und schreibt nicht unmittelbar in den Chat.

Grenze: Liefert ein Server ausschließlich eine Änderung der Lebenspunkte ohne brauchbares Combat-Ereignis, ist diese nicht sicher von einem Script-Reset unterscheidbar. Bei mehreren Treffern zwischen zwei Messungen ohne passende Einzelereignisse kann nur die gemessene Summe ausgegeben werden.

Quellen: [IncomingDamageDetector.h](../../include/IncomingDamageDetector.h), [SAMPBridge.h](../../include/SAMPBridge.h), [SAMPBridge.cpp](../../src/SAMPBridge.cpp), [DamageRPCAdapter.cpp](../../src/DamageRPCAdapter.cpp), [MainWindowUI.cpp](../../src/MainWindowUI.cpp), [SettingsManager.cpp](../../src/SettingsManager.cpp). Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), [RegressionTests.cpp](../../tests/RegressionTests.cpp), [SourceContractTests.cpp](../../tests/SourceContractTests.cpp).

## 9. Kennzeichnung, Einstellungen und Build

- Die Menükennzeichnung wurde auf **NLBinder v.0.1.1 by KOBiRO** geändert. Die ursprüngliche Perkys-Grundlage wird in der Dokumentation beibehalten.
- Neue Schalter, Launcher-Auswahl und getrennte Radarpositionen werden mit den vorhandenen Einstellungen gespeichert. Alte Radarpositionen werden migriert; alte entfernte Radar-/Alarmoptionen werden nicht reaktiviert.
- Die Win32-Buildskripte finden Visual Studio Build Tools über `vswhere`, auch bei einer abweichenden Installation. Fehlende Plugin-SDK-Bibliotheken können aus den beigefügten Quellen gebaut werden.
- Die verwendeten Plugin-SDK- und RakHook-Quellen wurden mit den [dokumentierten Revisionen](../build-dependencies.md) wiederhergestellt. Das bestehende RakHook-API wird beibehalten.
- Der Reconnect-Helfer wird aus Quellcode gebaut und als Ressource in die ASI eingebettet. Er muss nicht separat verteilt werden.
- Tests verwenden einen eigenen temporären Datenbereich und bearbeiten keine echte Binder-Konfiguration.

Quellen: [Keybinder.vcxproj](../../Keybinder.vcxproj), [setup_msvc.bat](../../tools/setup_msvc.bat), [build_plugin_sdk.bat](../../tools/build_plugin_sdk.bat), [build_release.bat](../../tools/build_release.bat), [build_reconnect_helper.bat](../../tools/build_reconnect_helper.bat), [run_tests.bat](../../tools/run_tests.bat), [PersistentPaths.cpp](../../src/PersistentPaths.cpp).

Die automatisierten Tests und der Release-Build der ausgelieferten Fassung waren erfolgreich. Der Reconnect-Helfer wurde zusätzlich mit eigenen harmlosen Testprozessen geprüft. **Es wurde kein echter Ingame-Test mit der konkreten Server- und Launcher-Version durchgeführt.** Details und die eindeutige ASI-Prüfsumme stehen in [PRUEFUNG-DE.md](PRUEFUNG-DE.md).

## 10. Nachtrag r4 – 25. Oktober 2026

Die Abschnitte 1 bis 9 halten den veröffentlichten r3-Stand fest. Dieser
Nachtrag beschreibt die darauf folgenden Korrekturen und ersetzt für die
betroffenen Funktionen die damaligen r3-Aussagen. Die ausführliche
[r4-Bedienungsbeschreibung](../NLBinder-0.1.1-r4-DE.md) gehört zum neuen Stand;
eine neue endgültige ASI-Prüfsumme und neue Testergebnisse werden mit dessen
Buildnachweisen dokumentiert.

### Damagelog nach dem AHK-Prinzip

Der Damagelog benötigt keine eingehende Schadens-RPC mehr. HP und Weste werden
getrennt mit der vorigen gültigen Probe verglichen. Sinkt mindestens einer der
Werte um mindestens 1 HP, erscheint der gemessene Verlust unmittelbar nach der
Probe mit einer Nachkommastelle im eigenen Chat. Die Typen heißen **HP Schaden**,
**Weste getroffen** und **Weste + HP Schaden**.

Serverseitige SetHealth-/SetArmour-Abnahmen und Umweltschaden einschließlich Fallschaden werden während
des Spiels mit erfasst. Die Script-/Umweltschaden-Unterdrückung des Hitsounds
blockiert den Log nicht mehr. Gleichzeitige Heilung eines Werts vermindert den
gemessenen Schaden am anderen Wert nicht. Login-/Spawn-/Pedwechsel und erneutes
Einschalten setzen die Ausgangswerte zurück. Bei mehreren Treffern zwischen
zwei Proben wird nur die gemessene Summe ausgegeben; es gibt keine Behauptung
einer garantierten einzelnen Zeile pro Schuss. Die Ausgabe bleibt lokal.

Quellen: [SAMPBridge.cpp](../../src/SAMPBridge.cpp),
[IncomingDamageDetector.h](../../include/IncomingDamageDetector.h).
Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp).

### Getrennte Ortswerte und bereinigte Variablen

Die angebotenen Ortsvariablen heißen **`{city}`** für Stadt/übergeordnetes
Gebiet und **`{bezirk}`** für den aktuellen Bezirk, beispielsweise Los Santos
und Market. Die Daten werden lokal aus den GTA-Zonen gelesen. Großgeschriebene
Doppelungen sowie die unzuverlässigen Ereignisfelder `victim`, `killer` und
`weapon` werden nicht mehr angeboten. Standardtexte verwenden die bereinigten
Orts-/Zählerwerte; vorhandene alte Vorlagen bleiben lesbar.

Quellen: [MessageVariables.h](../../include/MessageVariables.h),
[MessageVariablesUI.h](../../include/MessageVariablesUI.h),
[ActionExecutor.cpp](../../src/ActionExecutor.cpp),
[SAMPBridge.cpp](../../src/SAMPBridge.cpp),
[KillTrackerUI.cpp](../../src/KillTrackerUI.cpp).

### Killtext ohne echten Kill testen

Unter General / Kill- & Todeszähler wurde **Kill simulieren** ergänzt. Der
aktuelle Text wird mit den aktuellen Variablenwerten expandiert und tatsächlich
gesendet. Normaler Chat erhält `[Testkill]` am Anfang; bei Slash-Befehlen steht
der Befehl weiterhin zuerst: `/f [Testkill] …`, `/r [Testkill] …`,
`/d [Testkill] …`. Ein erster beliebiger Befehlstoken wird auf dieselbe Weise
beibehalten. Der Test erhöht keine Kill-/Todes-/Sitzungszähler und benötigt
keine aktivierte automatische Killnachricht. Leere oder mehrzeilige Texte
werden abgelehnt; die Chatlänge wird begrenzt.

Quellen: [MainWindowUI.cpp](../../src/MainWindowUI.cpp),
[KillMessageTest.h](../../include/KillMessageTest.h),
[SAMPBridge.cpp](../../src/SAMPBridge.cpp).
Tests: [KillMessageTestTests.cpp](../../tests/KillMessageTestTests.cpp).

### Einmalige eigene `/stats`-Synchronisation

Auf ausdrücklichen neuen Benutzerwunsch fragt der Binder nach dem sicheren
Loginzustand einmal `/stats` ab. Das ist eine Änderung gegenüber der zuvor
rein passiven Statistikübernahme. Der Versuch startet im ersten geeigneten
Spielframe ohne zusätzliche feste Wartepause; die Serverantwort bleibt
zeitbestimmend. Offener Chat, Dialog, Cursor oder ein noch nicht spielbereiter
Client blockieren den Start.

Die eigene automatisch angeforderte Nova-Chatstatistik wird beim Anzeigen
unterdrückt, während der Parser die normalisierten Zeilen weiterhin erhält.
Leere Zeilen durchlaufen ebenfalls den Sichtbarkeitsfilter. Eine identifizierte
eigene Statistik-Messagebox kann nach dem Lesen geschlossen werden. Andere
Dialoge, fremde Statistikblöcke und normale Chatzeilen werden nicht pauschal
versteckt oder geschlossen.

Der lokale Cache umfasst empfangene Kills/Tode, Bargeld/Kontostand und einen
gegebenenfalls gelieferten Gesamtvermögenswert sowie Level/XP/Levelziel, Gold,
Eventcoins, Wantedpunkte und die Zeit bis zum Zahltag. Ohne eigenen
Gesamtvermögenswert kann Bargeld plus Kontostand als verfügbare Summe dienen.
Nicht gelieferte Zusatzwerte werden nicht erfunden. Die ersten Gesamtzähler
sind ein Ausgangsstand und kein neuer Kill oder Tod.

Nach erfolgreicher Übernahme und lokaler Speicherung meldet der Binder
`[NL-BINDER] Synchronisation abgeschlossen.`. Nachfolgende Zeilen desselben
Blocks können den Cache bis zum Footer beziehungsweise innerhalb von drei
Sekunden ergänzen. Nach acht Sekunden ohne vollständige Antwort erscheint
stattdessen ein lokaler Fehlerhinweis; es gibt weder eine Erfolgsmeldung ohne
Daten noch wiederholten automatischen Abfragespam. Persönliche Cachewerte
bleiben in der lokalen Konfiguration und gehören nicht in GitHub.

Quellen: [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp),
[SAMPBridge.h](../../include/SAMPBridge.h),
[ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h),
[JobChatAdapter.cpp](../../src/JobChatAdapter.cpp),
[SettingsManager.cpp](../../src/SettingsManager.cpp), [Main.cpp](../../src/Main.cpp).
Tests: [ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp),
[SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp),
[JobChatAdapterTests.cpp](../../tests/JobChatAdapterTests.cpp).

Die r4-Änderungen wurden nicht in einer echten Spielsitzung geprüft. Die
abschließenden automatisierten Tests, der neue Release-Build und die
Veröffentlichung erfolgen mit den zugehörigen r4-Prüfnachweisen; die frühere
r3-Prüfsumme beschreibt weiterhin ausschließlich die frühere Binärdatei.

### Automatischer Motorstart erneut korrigiert

Nach der erneuten Fehlermeldung des Benutzers wurde der automatische Motorstart
überarbeitet. GTA kann beim Einsteigen kurz einen laufenden Motor melden,
bevor der Server ihn ausschaltet. Die Entscheidung erfolgt deshalb erst bei
vollständig erkanntem Fahrersitz und nach der bestehenden Wartezeit von
1,5 Sekunden. Ein vorübergehendes EngineOn verwirft den geplanten Start nicht
mehr. Ein alter „Motor Startet“-Chattext zählt nicht als neue Bestätigung;
die begrenzte Wiederholungsphase beginnt erst beim tatsächlichen Tastendruck.
Länger geöffneter Chat oder Binder lassen die Planung nicht vorzeitig ablaufen.
Der Schutz vor Eingaben in Chat, Dialog oder Binder bleibt erhalten.
Die konkrete Änderung und die ergänzten Regressionstests stehen in
[SAMPBridge.cpp](../../src/SAMPBridge.cpp) und
[SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp).

## 11. Nachtrag r5 – 25. Oktober 2026

### Motor-Taste nur einmal pro Einstieg

Die bisherigen Wiederholungsversuche konnten die Motor-Taste erneut auslösen,
bevor der Server einen laufenden Motor meldete. Da die Taste auf dem Server
ein Umschalter ist, führte das zu Starten, Ausschalten und erneutem Starten.
Der Binder verbraucht den Fahrer-Einstieg jetzt vor dem einen Tastendruck.
Eine ausbleibende oder verzögerte Bestätigung erzeugt keine weiteren Versuche.
Der zusätzliche Planungsweg über RPC 26 sowie die alte Textbestätigung wurden
entfernt.

Die bestehende Einstiegswartezeit von 1,5 Sekunden und die Sperren bei Chat,
Dialog, Binder und fehlendem Fokus bleiben erhalten. Ein bereits laufender
Motor bekommt keinen Druck. Ein späterer manueller Stopp und erneutes
Aktivieren der Option lösen im selben Einstieg keinen automatischen Neustart
aus. Ein echter Ausstieg oder ein anderes Fahrerfahrzeug erlaubt den nächsten
einmaligen Start.

Quellen: [SAMPBridge.cpp](../../src/SAMPBridge.cpp),
[SAMPBridge.h](../../include/SAMPBridge.h).
Tests: [SAMPBridgeTests.cpp](../../tests/SAMPBridgeTests.cpp), einschließlich
dauerhaftem EngineOff ohne Retry, Ausstieg/Wiedereinstieg im selben Fahrzeug,
Optionswechsel ohne Doppelstart und manuellem Motorstopp.

### Vollständige Statistikantwort direkt lesen

Die r4-Synchronisation erkannte die tatsächlich geöffnete Statistik nicht
zuverlässig. Sie beschränkte den Dialogpfad auf Messageboxen, verlangte neben
Kills und Toden auch Bargeld/Kontostand und konnte längere oder anders
formatierte Statistiktexte unvollständig auswerten.

Die Korrektur liest den vollständigen RPC-61-Dialogtext direkt aus der
Serverantwort. Messageboxen, Listen und Tabellen sind unterstützt. Der Parser
berücksichtigt bis zu 128 Zeilen, Farbcodes und Tabulatoren sowie Zähler mit
eckigen Klammern, runden Klammern oder ohne Klammern. Die eigenen Kills und
Tode genügen für die Übernahme. Weitere tatsächlich gelieferte Statistikwerte
werden ergänzt; fehlende Geldwerte verhindern die Zählerübernahme nicht mehr.

Nach dem automatischen eigenen `/stats`-Abruf erfolgen Import und Schließen
der identifizierten Startstatistik im selben Update. Es gibt keine zusätzliche
feste Verzögerung; die echte Serverantwort bleibt notwendig. Die gelesenen
Zähler werden als Ausgangsstand gespeichert und stehen anschließend auch
`{total_kills}`, `{total_deaths}` und **Kill simulieren** zur Verfügung, ohne
einen künstlichen Kill oder Tod zu erzeugen. Erst dann erscheint die lokale
Erfolgsmeldung `[NL-BINDER] Synchronisation abgeschlossen.`.

Nach einem Timeout kann ein eigener manueller `/stats`-Aufruf noch
nachsynchronisieren. Es gibt keine automatische Abfrageschleife. Nach bereits
abgeschlossener Start-Synchronisation bleiben spätere manuell geöffnete eigene
Statistiken sichtbar und aktualisieren empfangene Werte. Fremde Statistiken
und andere Dialoge werden nicht pauschal geschlossen oder übernommen.

Quellen: [StartupStatsSync.cpp](../../src/StartupStatsSync.cpp),
[ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h),
[StatsDialogRPCAdapter.cpp](../../src/StatsDialogRPCAdapter.cpp),
[SettingsManager.cpp](../../src/SettingsManager.cpp).
Tests: [StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp),
[ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp),
[StatsDialogRPCAdapterTests.cpp](../../tests/StatsDialogRPCAdapterTests.cpp).

Die endgültige r5-ASI, Prüfsumme und die zugehörigen Prüfnachweise stehen im
[r5-Paket](../../dist/NLBinder-0.1.1-r5/BUILD-INFO.txt). Die früheren r4-Werte
gelten weiterhin ausschließlich für die frühere Binärdatei. Ein echter
Ingame-Test auf dem konkreten Server steht aus.

## 12. Nachtrag r6 – 25. Oktober 2026

### Eigenen Statistik-Titel mit `»` korrekt zuordnen

Das tatsächliche Statistikfenster verwendet einen Titel im Format
`Statistiken » Spielername`. Die bisherige Titelbereinigung entfernte das
Unicode-Trennzeichen `»` vor dem Namen nicht. Der anschließende Namensvergleich
konnte deshalb die eigene Statistik als fremd abweisen, obwohl im Inhalt der
richtige Name sowie Kills und Tode standen. Der Import blieb aus, das Fenster
blieb geöffnet und der automatische Versuch endete im Timeout.

Die Titelbereinigung trennt den Namen jetzt korrekt vom Statistikpräfix.
Die Prüfung auf eine fremde Spieleridentität bleibt erhalten. Nach Erkennung
der eigenen Antwort werden die Gesamtzähler übernommen und die eigene
Startstatistik im selben Update geschlossen. Der importierte Stand steht
anschließend auch den Textvariablen und **Kill simulieren** zur Verfügung.
Eine zusätzliche feste Verzögerung wird nicht eingefügt; die Serverantwort
bleibt erforderlich.

### Vollständigen komprimierten Dialogpfad prüfen

Ein Regressionstest bildet das beobachtete Format mit erfundenem Namen und
erfundenen Statistikwerten nach. Er führt die komprimierte RPC-61-Nachricht
durch Dekodierung, Identitätsprüfung, Parser, Zählerübernahme und eigenen
Dialogabschluss bis zum Testkill mit dem importierten Gesamtstand. Weitere
Prüfungen grenzen fremde Spielernamen ab. Persönliche Screenshots und die
ursprünglichen Statistikwerte gehören nicht zur Veröffentlichung.

Quellen: [ServerStatsSnapshot.h](../../include/ServerStatsSnapshot.h),
[StartupStatsSync.cpp](../../src/StartupStatsSync.cpp),
[StatsDialogRPCAdapter.cpp](../../src/StatsDialogRPCAdapter.cpp).
Tests: [ServerStatsParserTests.cpp](../../tests/ServerStatsParserTests.cpp),
[StartupStatsSyncTests.cpp](../../tests/StartupStatsSyncTests.cpp),
[StatsDialogRPCAdapterTests.cpp](../../tests/StatsDialogRPCAdapterTests.cpp).

Der einmalige Motorstart aus r5 bleibt unverändert. Frühere Job-, Overlay-,
Zähler-, Damagelog-, Tacho- und Reconnect-Anpassungen sind weiterhin enthalten.
Die endgültige r6-ASI und deren eigene Prüfnachweise werden im
[r6-Paket](../../dist/NLBinder-0.1.1-r6/BUILD-INFO.txt) bereitgestellt. Frühere
r5-Prüfwerte gelten ausschließlich für die damalige ASI. Ein echter
Ingame-Test mit dem konkreten Server und Launcher steht aus.
