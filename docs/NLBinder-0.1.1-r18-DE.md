# NLBinder v.0.1.1 by KOBiRO – r18

## Installation

GTA vollständig beenden, NLBinder.asi ersetzen und das Spiel neu starten.
Browserdateien werden weiterhin automatisch unter Dokumente/GTA San Andreas
User Files/NLBinder/Browser eingerichtet. Nur die ASI muss verteilt werden.
Der Chromium-Bestand bleibt derselbe fest geprüfte Stand cef156-r1.

## Automatischer AFK-Modus beim Forum

Beim Öffnen des Forumfensters sendet der Binder einmal `/afk`. Beim Schließen
über ESC, den Schließen-Button, das Fensterkreuz oder erneutes `/forum` folgt
einmal `/back`. Das gilt auch beim Abbrechen der Einrichtung und wenn das
Fenster wegen eines Fehlers geschlossen wird. Der Wechsel von der Einrichtung
zur fertigen Webseite löst kein zusätzliches Befehls-Paar aus. Wiederholte
Render-Aufrufe schicken keine weiteren AFK-Befehle.

Die Umschaltung verwendet die vorhandenen Serverbefehle. Die tatsächlichen
Einschränkungen des AFK-Modus bestimmt weiterhin der Server.

## Browserinstallation und 100-Prozent-Hänger

Der Installer-Abschluss und GPS wurden zuvor ausschließlich über einen
Game-Process-Aufruf verarbeitet. r18 aktualisiert den Status zusätzlich über
den tatsächlich gerenderten Browser und den vorhandenen HUD-Aufruf. Auch ein
bereits geschlossener Browser wird dabei weiter abgewickelt. Ein erneuter
`/forum`-Aufruf räumt einen verlassenen Installerzustand auf.

Sobald alle Dateien geprüft und atomar aktiviert sind, meldet der Installer
Ready. Der Binder öffnet dann sofort Chromium und wartet nicht erst auf
temporäre Dateibereinigung oder das Ende des Hilfsprozesses. Während der
Einrichtung bleibt die Anzeige unter 100 Prozent. Download, Entpacken und
Fertigstellen haben eigene Fortschrittstexte; der Download zeigt die Datenmenge.

Abbrechen beendet ausschließlich den eigenen Installer und setzt das Fenster
sofort zurück. `/forum` kann direkt erneut gestartet werden. Ein vollständig
heruntergeladenes und geprüftes ZIP bleibt bei abgebrochenem Entpacken im
Browsercache und wird beim nächsten Versuch ohne erneuten Download verwendet.
Teildownloads werden nicht als gültiges Paket verwendet. Ein nicht mehr
reagierender Installer bleibt nicht unbegrenzt im Fenster hängen.

Beim Entpacken wird SHA-256 direkt während des Schreibens gebildet. Dadurch
entfallen zusätzliche vollständige Lesedurchläufe. Eine bereits vollständig
geprüfte Installation erhält einen lokalen Nachweis über Dateigrößen und
Änderungszeiten. Unveränderte Dateien können schnell wiederverwendet werden;
bei Änderungen wird erneut geprüft. Der einmalige Download von rund 188 MB
hängt weiterhin von der Verbindung ab.

## Normaler GPS-Checkpoint

Navigieren erzeugt den Marker direkt aus der GPS-Auswahl, ohne auf einen
Game-Process-Tick zu warten. Das Ziel wird anschließend über die vorhandenen
Render-/HUD-Aufrufe gepflegt. Für die Weltmarkierung wird die normale native
GTA-Checkpoint-Funktion mit rotem Zylinder, passender Bodenhöhe und Radius
3 Meter verwendet. Die rote Minimap-Markierung bleibt zusätzlich bestehen.
Der Marker erhält ausdrücklich die Freigabe für den aktuellen Renderdurchlauf.
Das normale Anlegen setzt nur den Belegt-Status; ohne die später von GTA
gesetzte Renderfreigabe kann er vor dem Zeichnen wieder verschwinden.

Beim Einfahren in den Zielbereich kommt genau eine lokale Ankunftsmeldung.
Wer bereits beim Navigieren am Ziel steht, sieht den Marker bis zum Verlassen
und erneuten Betreten. Eigene Markierungen werden beim Beenden der Navigation
entfernt. Fremde Servermarker bleiben über die bestehende Slotprüfung geschützt.

## Was tatsächlich geprüft wurde

Die gesamte C++-Testsuite und der Release-Build sind erfolgreich. Die neuen
GPS-Tests erzeugen das Ziel direkt bei Navigieren und treiben Navigation und
Ankunft ausschließlich über Render-Aufrufe. Zusätzlich führt ein isolierter
Test den produktiven GPS-Adapter mit echten GTA-Strukturen und ersetzten
Engine-Funktionen aus: normaler Checkpoint, Rotfärbung, Bodenhöhe, Minimap,
Bildwechsel, Zielwechsel, Löschen und recycelte fremde Radar-Slots.

Die Installer-Prüfungen verwenden das echte öffentliche HTTPS-Paket, prüfen
alle 236 Dateien sowie Wiederverwendung, Reparatur und Abbruch. Ein harter
Abbruch während des Entpackens mit anschließendem Neustart wurde ebenfalls
geprüft; der erneute Versuch nutzte den vorhandenen Download ohne Netzwerk.

Der echte Chromium-Host wurde viermal mit demselben anonymen Testprofil
geöffnet. Der Controller wurde dabei ausschließlich durch Render-Aufrufe
angetrieben. Der Installer blieb absichtlich nach Ready noch 15 Sekunden
am Leben: Das Forum öffnete trotzdem sofort nach dem fertigen Dateistand.
Abbrechen und unmittelbarer neuer Start wurden zusätzlich geprüft.

Die AFK-/Back-Umschaltung wurde dabei mit sechs vollständigen Befehls-Paaren
über einen ersetzten Befehlsversand geprüft. Dadurch gehen aus dem Testprogramm
keine echten AFK-Befehle an den Server.

Der Nutzer bestätigte den Forum-/GPS-Test mit einer vorläufigen r18-ASI im
Spiel. Die danach ergänzte ausdrückliche Renderfreigabe und AFK-Umschaltung
sind davon getrennt automatisiert geprüft. Die Prüfsumme der tatsächlich im
Spiel getesteten ASI steht im separaten Live-Nachweis. Für einen Sichttest der
finalen Erweiterung muss die neue ASI bei beendetem Spiel eingesetzt werden.
Die Tests verwenden keine persönlichen Browserprofile, Anmeldungen oder Beiträge.
