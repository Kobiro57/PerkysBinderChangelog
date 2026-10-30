# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 13

Stand: 30. Oktober 2026. Die eigene Friedhofsmeldung löst
den Todeszähler aus; native 0-HP-Zustände zählen nicht mehr eigenständig. Die Ausgabe heißt weiterhin **NLBinder.asi**.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige **Keybinder.asi** durch **NLBinder.asi** am bisherigen
   Installationsort ersetzen. Die alte ASI aus dem Ladeordner entfernen,
   damit nur eine Binder-Fassung geladen wird.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 normal starten und einloggen.

Der vorhandene ASI-Loader bleibt erforderlich. Eigene Einstellungen und
Waffenbilder bleiben im bisherigen NLBinder-Ordner unter
**Dokumente → GTA San Andreas User Files → NLBinder**. Die mitgelieferte
leere Waffenicon-Ordnerstruktur kann dort zusammengeführt werden.

## Aktuellen Spielstand manuell synchronisieren

Unter **Einstellungen**, direkt unter **Tacho**, auf
**Aktuellen Spielstand synchronisieren** klicken. Der Binder liest die
eigenen `/stats` und danach `/jobskills` erneut ein. Die empfangenen
Gesamtstände und Skillpunkte werden direkt für die bisherigen Anzeigen,
Skill-Feeds und Textvariablen übernommen und gespeichert.
Das Bindermenü schließt sich für den Abruf.

Nach erfolgreichem Abschluss beider Abrufe erscheint nur lokal:

`[NL-BINDER] Du hast deinen Spielestand manuell synchronisiert. Deine Werte sind jetzt im aktuellsten Stand.`

Eine laufende Synchronisation wird nicht nochmals gestartet. Fehlende oder
unpassende Antworten führen nicht zu einer Erfolgsmeldung. Der Abruf
benötigt echte Serverantworten; wie beim Einloggen schließt der Binder die
zugehörigen eigenen Dialoge nach dem Lesen. Die automatische Synchronisation
beim Einloggen bleibt bestehen.

## Lokale Killmeldung

Die eigene lokale Killmeldung lautet nun
`[NL-BINDER] Du hast einen Kill gemacht! Aktuell hast du: {kills} Kills.`.
Anstelle von `{kills}` erscheint der aktuelle Gesamt-Killstand. Der
frei eingestellte Killbind für den Server bleibt erhalten und wird nicht
zusätzlich als lokaler Hinweis wiederholt. Bestehende lokale Altvorlagen
werden einmalig auf den neuen Text umgestellt.

## Todeszählung und Damagelog

Die Meldung `Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder
reanimiert wirst oder deine Zeit abläuft.` wird wieder als maßgeblicher
Auslöser des eigenen Todes ausgewertet. Die native 0-HP-Erkennung erhöht den
Todeszähler nicht mehr. Die lokale Benachrichtigung über den eigenen
Tod bleibt erhalten. Damit wird die in r12 gewählte Todesquelle geändert.

Doppelte Friedhofsbestätigungen innerhalb von fünf Sekunden zählen nicht
noch einmal. Wurde der neue Gesamtstand vorher bereits aus der eigenen
Statistik gelesen, wird er durch die Friedhofszeile bestätigt und nicht ein
zweites Mal erhöht. Die lokale Meldung zeigt den übernommenen Gesamtstand.

Der Damagelog nutzt zusätzlich die echten eingehenden Umwelt-/Explosions-
Schadensmeldungen des Clients. So kann ein Fall auch erkannt werden, wenn
der Server die HP vor der nächsten Messung bereits wieder verändert hat.
Unmessbare native HP-/Westenwerte können durch die tatsächlich gesendeten
Clientwerte ergänzt werden. Dafür müssen Ped, Fortbewegungsmodus und Fahrzeug
passen; ein solcher Syncwert darf höchstens 500 ms alt sein.

Der gemeldete Fallback-Schaden wird auf die bekannten Rest-HP bzw. die
Restweste begrenzt. Ohne zuverlässige Restwerte wartet er höchstens 1500 ms
auf echte Daten und gibt keine unbestätigte Überkillzahl aus. Messwerte und
Schadensmeldungen werden nach Quelle und zeitlichem Zusammenhang abgeglichen,
damit derselbe Treffer nicht doppelt erscheint. Die bestehende Trennung zum
Hitsound bleibt erhalten. Das Damagelog-Präfix lautet `{33CCFF}`.
Ein echter Ingame-Test auf dem konkreten Server steht aus.

**Aktuellen Spielstand synchronisieren** unter Tacho kann die aktuellen
Server-Gesamtstände erneut übernehmen. Der Binder zieht keinen Tod pauschal
ab; der persönliche Gesamtstand wird aus der eigenen Serverstatistik gelesen.

## Vollständiger Quellordner und Changelog

Der angeforderte Desktop-Ordner **NLBinder-masterNEW** enthält den gesamten
Quellcode einschließlich der benötigten Abhängigkeiten, die fertige
**NLBinder.asi** und **CHANGELOG-GESAMT.txt**. Die Projektdateien heißen
weiterhin **Keybinder.sln** und **Keybinder.vcxproj**; der Release-Build
erzeugt **NLBinder.asi**.

Die Textdatei beschreibt den gesamten dokumentierten Änderungsverlauf,
einschließlich der früheren Job-, Overlay-, Tacho-, Kill-/Todes-, Reconnect-,
Interface-, Waffenicon- und Textvariablen-Anpassungen. Sie liegt auch
diesem Paket bei. Persönliche Laufzeitkonfigurationen, Zugangsdaten und
Compiler-Zwischendateien gehören nicht zur Quellcode-Weitergabe.

Der Quellordner enthält die aktuelle ASI. Frühere Binärpakete bleiben über
die GitHub-Historie erreichbar und werden dort nicht als zusätzliche alte
Binder-Kopien beigelegt. Die **README.md** im Quellordner beschreibt das
Bauen des Projekts; die Übersicht für die Projektleitung liegt zusätzlich
unter **docs/review/README-GITHUB.md**.

## Prüfung und frühere Funktionen

Die Paketnachweise stehen in **BUILD-INFO.txt**, **SHA256SUMS.txt** und
**PRUEFBERICHT.json**. Der Reconnect-Helfer bleibt in der ASI enthalten.
Automatisierte Helfertests verwenden ausschließlich harmlose Testprozesse;
der installierte Nova Launcher und das echte Spiel werden dafür nicht
gestartet oder beendet. Ein echter Ingame-Test steht aus.

Jobs und Skilltrees, Fraktionen, getrennte Freunde-/Feinde-Overlays,
Damagelog, Kill-/Todeszähler, frei gewählte Killtexte, `/speed ID`, der
Nova-/OpenMP-Reconnect, Waffenicons, die vier Interface-Bereiche und
`{input}` für manuelle Textbindungen bleiben enthalten. Die Bedienung
dieser früheren Änderungen steht im vollständigen Changelog.

Quellcode und Prüfstand werden im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
bereitgestellt. Frühere Stände und ursprüngliche Urheberhinweise bleiben
erhalten.
