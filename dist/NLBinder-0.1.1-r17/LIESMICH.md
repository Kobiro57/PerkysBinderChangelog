# NLBinder v.0.1.1 by KOBiRO – r17

## Installation

Bei beendetem Spiel die bisherige Binder-ASI durch **NLBinder.asi** ersetzen.
Nur eine Binder-ASI laden. Ab r17 muss kein Browserordner mitgeschickt oder
manuell in den GTA-Ordner kopiert werden.

Der Ordner **Dokumente/GTA San Andreas User Files/NLBinder/Browser** wird beim
Start des Binders angelegt. Beim ersten `/forum` werden dort die benötigten
Browserdateien automatisch eingerichtet. Bereits mitgelieferte passende
r16-Dateien neben der ASI werden geprüft und ohne Download übernommen.
Andernfalls lädt der Binder einmalig das versionierte Browserpaket von GitHub
(188 MB komprimiert, rund 429 MB entpackt), prüft SHA-256 für Archiv und jede
Datei und öffnet anschließend das echte Forum.

Im Fenster erscheinen Fortschritt und ein Abbrechen-Button. ESC bricht die
Einrichtung ebenfalls ab. Bei einem Fehler kommt ein lokaler Hinweis;
`/forum` kann erneut aufgerufen werden. Eine beschädigte oder unvollständige
Installation wird repariert. Bereits gültige Dateien werden weiterverwendet.
Die Einrichtung läuft in einem kleinen separaten Prozess mit normalen
Benutzerrechten; sie blockiert nicht den Spielablauf. Python, PowerShell,
ein externes Entpackprogramm und Administratorrechte sind nicht erforderlich.

Die Browserdateien liegen in einem eigenen versionierten Unterordner.
Anmeldecookies bleiben im bisherigen separaten lokalen CEF-Profil unter
`%LOCALAPPDATA%/NLBinder/Browser/Forum`; diese werden nicht als Download verteilt.
Unvollständige Downloads werden nicht als funktionierende Installation benutzt.
Das heruntergeladene Archiv wird nach erfolgreicher Einrichtung entfernt.

## Forum wieder öffnen

`/forum`, ESC und Schließen funktionieren weiterhin. Beim erneuten Öffnen
wartet der Binder gegebenenfalls, bis die vorherigen eigenen Browserprozesse
vollständig beendet sind und das Profil freigeben. Ein schneller erneuter
Aufruf wird vorgemerkt und öffnet das Fenster automatisch wieder. Der
Wartezustand wird angezeigt und kann abgebrochen werden. Beim Schließen vor
dem vollständigen Chromium-Start bleibt der nächste Aufruf ebenfalls möglich.

## GPS-Markierung

`/setgps Bemerkung`, `/gpslist`, `/delgpsall` und `/gps` bleiben erhalten.
Navigieren erzeugt einen roten Welt-Checkpoint und eine größere rote Markierung
auf der Minimap. Sie ist nicht auf den Nahbereich begrenzt. Ein vom Spiel
entfernter eigener Radar-Eintrag wird während der Navigation wieder angelegt.
Wurde dessen Slot zwischenzeitlich vom Server wiederverwendet, wird dieser
fremde Marker weder verändert noch gelöscht. Auch die QuickMap berücksichtigt
die korrekten Radar-Anzeigeflags.

Wenn du direkt am gespeicherten Ziel navigierst, bleibt der Checkpoint jetzt
sichtbar. Die Ankunft wird erst nach Verlassen und erneutem Betreten des
Zielbereichs ausgelöst. Bei Navigation von außerhalb bleibt die einmalige
Ankunftsmeldung wie gewohnt. Die serverseitigen Job-Checkpoints bleiben unabhängig.

## Prüfung und Build

Gesamte vorhandene C++-Testsuite und Win32-Release-Build erfolgreich. Neue
GPS-Prüfungen sichern den Start am Ziel, genau eine Ankunft und fremde
Radar-Slots ab. Der native Browserinstaller wurde mit echtem öffentlichen
HTTPS-Download, allen 236 Datei-Prüfsummen, erneuter Verwendung, Übernahme,
Reparatur, Abbruch und Pfaden mit Leerzeichen/Umlauten geprüft.

Ein weiterer Test verwendet den produktiven x86-Controller samt eingebettetem
Installer, lädt das echte Forum mit frischem anonymen Profil und öffnet es
viermal mit demselben Profil: normal, erneut nach Schließen, sofort während
des Schließens und nach Abbruch während des Starts. Alle Prozesse endeten
sauber. Kein automatischer Accountlogin, Beitrag oder Nachrichtenversand.
Ein zusätzlicher Live-Test in einer laufenden GTA-Sitzung wird nicht behauptet.

Die Quellen des Installers stehen unter `tools/BrowserSetup.cpp`, die
ZIP-Bibliothek samt Lizenz und Herkunft unter `third_party/miniz`. Das feste
Downloadziel und alle Hashes stehen in `res/browser-runtime.json` und dem
daraus generierten `include/BrowserRuntimeLock.h`. Der Helfer wird automatisch
gebaut und als Ressource 301 in die ASI eingebettet. Die Browser-Runtime ist
unverändert gegenüber dem geprüften r16-CEF-Host. Die Sandbox bleibt aktiv.
