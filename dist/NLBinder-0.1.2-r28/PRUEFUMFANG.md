# Prüfumfang r28 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die entfernten
Ortungsquellen und ausschließlich zugehörigen Tests dürfen im aktuellen
Quellstand, Projekt, Testlauf und Desktop-Export nicht mehr enthalten sein.
Die aktive SAMPBridge darf keinen Ortungsablauf mehr initialisieren, aktualisieren
oder mit Detektiv-Meldungen versorgen. In der fertigen ASI darf die eindeutige
Vorlage `[ORTUNG]` nicht vorkommen.

Die bestehenden Tests für Wanted-Overlay und native Gesucht-Labels, /speed,
Spieler-IDs, GPS, Quick Map, Fraktionsprüfung sowie die übrigen Funktionen
bleiben maßgeblich. Die normalen serverseitigen Chatmeldungen dürfen keine
automatische Ortungs-Funkmeldung auslösen. Die frühere Ortungsimplementierung
wird nicht durch inaktive Quellen oder neue Tests für entfernten Code erhalten.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Unveränderte Browser-, GPS-, Wanted- und Kill-Quellen werden gegen r27
abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte historische
Browsernachweis aus r24 und wird nicht als neuer r28-Test ausgegeben.

Ein r28-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r28-validation.json. Exportierte Quellen und
ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket. Historische
Veröffentlichungen und ihre Prüfdateien bleiben erhalten.
