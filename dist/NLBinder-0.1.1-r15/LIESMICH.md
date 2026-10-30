# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 14

Stand: 30. Oktober 2026. Der Todeszähler nutzt echte eigene Todesübergänge
und die direkte Client-Todesmeldung. Friedhofs-Chat erzeugt keinen weiteren
Tod beim Relog. Die Ausgabe heißt weiterhin **NLBinder.asi**.

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

Eine lesende Live-Aufzeichnung hat den Fehler belegt: Der verwendete Client
speichert die eigenen HP mit einem Schutzaufschlag von 8.000.000. Beispielsweise
bedeutet 8.000.094 tatsächlich 94 HP. Beim beobachteten Explosionstod waren
7.999.945 anschließend 8.000.000 gespeichert, während GTA-/SA-MP-Todesflags
unverändert blieben. r14 konnte diesen Tod deshalb nicht erkennen.

r15 entfernt den belegten Aufschlag vor der Auswertung. Negative logische HP
werden auf null begrenzt. Nach einer bestätigten lebendigen Spielphase löst
ein solcher Übergang die Prüfung der eigenen Statistik aus. Erst ein neuer
serverseitiger Todesstand bestätigt das Ereignis. Der absolute Gesamtstand
wird übernommen und genau eine lokale Todesmeldung erzeugt. Ein schon
übernommener Tod wird nicht noch einmal zum Gesamtstand addiert.

Es gibt einen sofortigen Statistikabruf und bei noch unverändertem Stand
höchstens einen weiteren nach vier Sekunden. Das berücksichtigt den beobachteten
dreisekündigen serverseitigen Todesübergang. Es werden dabei keine Jobskills
angefordert und keine zusätzlichen Synchronisations-Erfolgsmeldungen erzeugt.
Die eigenen zugehörigen Statistikdialoge schließen nach dem Lesen. Ohne
verifizierbare Antwort wird kein zusätzlicher Tod erfunden.

Die Friedhofs-Chatmeldung ist kein Todesauslöser. Login und Relog übernehmen
einen Ausgangszustand. Auch wenn das Friedhofsscript HP und Steuerung wieder
herstellt, reicht ein weiterer Null-HP-Zustand ohne neuen Server-Todesstand
nicht zum Zählen. Es gibt keine pauschale dreiminütige Sperre für echte neue
Todesfälle. Normale native Todeszustände bleiben für ungeschützte HP unterstützt.

Der Damagelog verwendet die tatsächlichen bereinigten HP-/Westenverluste sowie
die eigenen serverseitigen HP-/Westenänderungen. Rohbeträge aus TakeDamage
werden nicht zusätzlich ausgegeben. Überkill verbraucht nur die zuvor
vorhandenen HP und die Weste. Wiederholte negative Werte erzeugen keinen
weiteren Verlust. Die übertragenen 8-Bit-HP werden nicht als Ersatz verwendet:
Sie können beispielsweise aus minus acht fälschlich 248 machen.
Fallschaden bleibt enthalten. Der Präfix verwendet weiterhin `{33CCFF}`.

Regressionstests spielen den anonymisierten echten Explosionsablauf nach und
prüfen Beschuss mit negativem logischem Restwert, Fallschaden, verzögerte
Statistikantworten, bereits importierte Zähler und Friedhofs-Relog.
Die neue r15-ASI wurde danach direkt im Spiel geprüft: Der Nutzer bestätigte
einen korrekten Explosionstod mit einer lokalen Todesmeldung und anschließend
den Friedhofs-Relog ohne Doppelzählung. Der Serverstand stieg beim Tod um eins
und blieb beim Relog gleich. Beschuss und Fallschaden bestehen die
automatisierten Tests; ein separater Live-Test dieser beiden Ursachen wurde
nicht durchgeführt. Der Prüfnachweis trennt diese Prüfarten ausdrücklich.

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
