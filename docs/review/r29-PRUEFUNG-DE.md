# Prüfumfang r29 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. SpraysNavigatorTests
prüfen die 100 festen Koordinaten, aufsteigende dreidimensionale Entfernung,
grüne Hervorhebung der nächsten Stelle, die Seitenaufteilung 34/34/32 und die
Zuordnung einer Auswahl zur tatsächlich angezeigten Zielposition. Die bestätigte
SAPD-/LSPD-Mitgliedschaft ist Voraussetzung für die Funktion.

GpsTemporaryTests prüfen die vorübergehende Navigation über das vorhandene
GPS-System. Ein Spray-Ziel darf auch bei 20 gespeicherten Zielen funktionieren,
keinen Speicherplatz verbrauchen und gespeicherte Ziele nicht verändern.
Der vorhandene rote Checkpoint und die Ziel-erreicht-Erkennung werden verwendet.
GPS-Spieladapter, Browser, Wanted-Labels und Kill-Erkennung werden gegen r28
abgeglichen. Angepasste GPS-Verwaltung und Main-Integration werden neu geprüft.

Die automatische Polizeifunk-Ortung bleibt entfernt. Frühere Ortungsquellen dürfen
im aktuellen Quellstand, Projekt und Desktop-Export nicht wieder auftauchen.
Die fertige ASI darf die eindeutige Vorlage `[ORTUNG]` nicht enthalten. Die
Negativregression zu Detektiv-Meldungen und Checkpoints ohne automatischen
Funktext bleibt Teil der Gesamttests.

Der PE-Nachweis prüft die x86-ASI, ihre eingebetteten Helfer und die Startmeldung.
Der Reconnect-Helfer wird mit lokalen Testprozessen geprüft. Der unveränderte
historische Browsernachweis aus r24 wird nicht als neuer r29-Test ausgegeben.
Ein r29-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r29-validation.json. Exportierte Quellen und
ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
