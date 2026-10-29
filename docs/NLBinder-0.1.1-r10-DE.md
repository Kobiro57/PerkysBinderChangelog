# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 10

Stand: 29. Oktober 2026. Diese Fassung verbessert **/reconnect** über den
Nova Launcher, den automatischen Motorstart und die Erkennung eigener
Kills/Tode aus den bestätigten Servermeldungen. Textbindungen unterstützen
zusätzlich die manuelle Eingabevariable `{input}`.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI am bisherigen Installationsort durch `Keybinder.asi`
   ersetzen. Nur eine Binder-Fassung laden; der vorhandene ASI-Loader bleibt
   erforderlich.
3. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.

Eigene Einstellungen und Waffenbilder bleiben im bisherigen NLBinder-Ordner
unter **Dokumente → GTA San Andreas User Files → NLBinder**. Die mitgelieferte
leere Waffenicon-Ordnerstruktur kann dort zusammengeführt werden.

## Reconnect

Unter **Einstellungen → General** den Nova Launcher auswählen und bei Bedarf
den vorhandenen Launcher-Pfad eintragen. `/reconnect` bereitet den Helfer
vor, speichert den Binder-Stand und beendet den bisherigen Spielprozess.
Der Helfer übernimmt anschließend den normalen Nova-Spielstart und prüft
den Startfortschritt. Ein bloßes Öffnen des Launchers oder ein einzelner
Button-Aufruf gilt noch nicht als bestätigter Spielstart. Ohne laufenden
Spielprozess versucht er den normalen Start im Abstand von 2,5 Sekunden
erneut, regulär höchstens 90 Sekunden lang. Sobald das passende Spiel läuft,
unterbleiben weitere Startaktionen. Der neu geladene Binder bestätigt den
erfolgreichen Einstieg mit aktivem eigenen Spieler.

Falls Nova Administratorrechte benötigt, kann Windows vor dem Beenden des
Spiels eine Freigabe für den Reconnect-Helfer anzeigen. Ohne bestätigte
Bereitschaft wird das bisherige Spiel nicht geschlossen.

Nova muss weiterhin normal eingerichtet und angemeldet sein. Anmeldungen,
Berechtigungsabfragen und andere Launcher-Einstellungen werden nicht für
den Benutzer beantwortet. Der alternative OpenMP-Start bleibt enthalten.

## Automatischer Motorstart

Der Start reagiert beim Erreichen des Fahrersitzes schneller. Pro Einstieg
bleibt es bei einem Startversuch. Ein bereits laufender Motor und die
bestätigte Startmeldung des serverseitigen Fahrzeug-Addons verhindern einen
weiteren Binder-Tastendruck. Nach manuellem Ausschalten wird nicht erneut
gestartet. Die Ein-/Aus-Einstellung im Binder bleibt wirksam.

## Kill- und Todesmeldungen

Der Binder erkennt die bestätigten persönlichen Servermeldungen sowie die
angegebenen GANGWAR-, REVIERKAMPF-, STREETWAR- und DEATHZONE-Formate.
Öffentliche Meldungen zählen einen Kill nur bei eigenem Namen an der
Täterposition. Bei der gesuchten-Verbrecher-Meldung zählt der eigene Name
an der Opferposition als Tod. Die Friedhofsmeldung ergänzt die eigene
Todeserkennung. Der Name wird aus dem aktuellen Spieler gelesen. Die Gefängnis-gesprengt-
Meldung nennt alleine keinen Täter und wird deshalb nur einem bereits
bekannten eigenen Kill zugeordnet.

Mehrere Hinweise auf denselben Kill oder Tod werden mit der vorhandenen
Killfeed-/RPC-/Statistikerkennung abgeglichen. Die Einstellungen für
Kill-/Todeszähler, lokale Meldungen und den frei gewählten Killtext bleiben
bestehen. Normale Spielernachrichten sind keine Serverbestätigungen.

## Manuelle Texteingabe mit {input}

In **Tastenbelegung** einen Bind mit dem Befehl `/blitzer` anlegen und als
Nachricht oder Befehl beispielsweise diese Aktion eintragen:

`/News In {input} befindet sich ein Blitzer. Bitte vorsichtig fahren.`

Beim Aufruf von `/blitzer` öffnet der Binder den normalen Ingame-Chat und
zeigt lokal **Gib hier deinen Input-Text ein:**. `BSN` eingeben und Enter
drücken. Gesendet wird dann:

`/News In BSN befindet sich ein Blitzer. Bitte vorsichtig fahren.`

Die Antwort wird nur in den Bind eingesetzt und nicht zusätzlich als eigene
Chatnachricht gesendet. Esc oder eine leere Eingabe bricht den gesamten Bind ab. Pro Aufruf wird
einmal gefragt; mehrere `{input}`-Stellen desselben Binds verwenden dieselbe
Antwort. Der nächste Aufruf fragt neu. Andere Variablen bleiben verwendbar;
geschweifte Klammern im eingegebenen Text werden wörtlich übernommen.
Ohne führenden Schrägstrich entsteht eine normale Chatnachricht. Die
Variablenhilfe führt `{input}` direkt unter `{kd}` als **Manuelle Textabfrage**.

## Prüfung und frühere Funktionen

Die Paketnachweise stehen in `BUILD-INFO.txt`, `SHA256SUMS.txt` und
`PRUEFBERICHT.json`. Der Reconnect-Helfer ist in der ASI enthalten und wird
mit harmlosen Testprozessen geprüft. Der installierte Nova Launcher und das
echte Spiel wurden für diese automatisierte Prüfung nicht gestartet oder
beendet. Ein echter Ingame-Test steht aus.

Jobs, Skilltrees, Fraktionen, getrennte Freunde-/Feinde-Overlays,
Waffenicons, `/speed ID`, Statistik-/Jobskill-Synchronisation und die vier
Interface-Bereiche Geldanzeige, Waffenicon, Munition und Minimap bleiben
enthalten. Die Maussteuerung des Interface-Editors entspricht den
dauerhaften Online-Overlays; Esc speichert.

Quellcode, Änderungsübersicht und Prüfstand werden im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog)
bereitgestellt. Frühere Stände und ursprüngliche Urheberhinweise bleiben
erhalten. Persönliche Screenshots, Laufzeitkonfigurationen, Accountwerte und
eigene PNGs gehören nicht zur Veröffentlichung.
