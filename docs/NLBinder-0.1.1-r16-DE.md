# NLBinder v.0.1.1 by KOBiRO – r16

## Installation

Das vollständige ZIP enthält `NLBinder.asi` und den Ordner `NLBinder`.
Kopiere beide bei beendetem Spiel in denselben GTA-Ordner. Für das Forum ist
`NLBinder/Browser` mit den mitgelieferten CEF-Dateien erforderlich. Die ASI
allein enthält den Browser nicht. Bestehende persönliche Einstellungen,
Waffenbilder, Jobdaten und Browserprofile werden nicht mitgeliefert.

## Ausdauer

Unter **Online → Overlay-Einstellungen → Dauerhaft → Spielinformationen**
steht direkt unter Pedslots die Option **Ausdauer anzeigen**. Die senkrechte
Leiste liest die tatsächliche GTA-Sprintreserve und deren spielseitiges Maximum.
Sie zeigt den verbleibenden Anteil und anhand gemessener Änderungen
„Verbrauch“, „Erholt sich“, „Voll“ oder „Pause“. Unbegrenztes Sprinten wird
als solches angezeigt. Die Anzeige gilt für die vom Spiel verwendete
Sprint-/Schwimmreserve; sie verändert keine Spielerwerte und ist kein eigener
nachgebauter Sprint-Timer. Sie lässt sich mit dem normalen Move-Modus ziehen;
ESC beendet den Modus und speichert die Position wie bei den anderen Overlays.

## Akkord-Anteil

Auch die Meldung `JOBNAME: Dein Akkord-Anteil von 2.896$ wird dir zum nächsten
Zahltag ausgezahlt` wird nun als Verdienst des jeweiligen bekannten Jobs erfasst.
Der Betrag erscheint in der bisherigen lokalen Verdienstmeldung, in den
Jobstatistiken und im Tagesverdienst. Normales Gehalt bleibt erhalten. Eine
einzelne Akkord-Meldung erzeugt einen Verdienst, keinen zusätzlichen zweiten
Eintrag durch andere Parserregeln. Deutsche Tausenderpunkte werden unterstützt.

## Gespeicherte GPS-Ziele

- `/setgps Bemerkung` speichert die aktuelle Position und das Interior unter
  einem Namen mit bis zu 64 Zeichen. Maximal 20 Ziele sind möglich.
- `/gpslist` öffnet die Liste im Binder-Stil. Wähle einen Eintrag und dann
  **Navigieren** oder **Löschen**.
- Ein Navigationsziel erscheint als roter Marker und roter Kartenpunkt.
  Im Zielbereich meldet der Binder lokal, dass du angekommen bist. Der
  gespeicherte Eintrag bleibt für eine spätere Navigation erhalten.
- `/delgpsall` löscht alle gespeicherten Ziele.
- `/gps` zeigt die Befehle nur lokal an.

Die Ziele liegen unter Dokumente/GTA San Andreas User Files/NLBinder/gps.xml.
Die eigenen Marker ersetzen keine serverseitigen Job- oder Missionscheckpoint-
Slots. Es wird kein serverseitiger GPS-Befehl ausgeführt.

## Echtes Forum im Spiel

`/forum` öffnet https://nova-legacy.de/forum als echte Chromium-Webseite
innerhalb eines verschiebbaren und skalierbaren Binder-Fensters. Ein erneuter
Aufruf, ESC, das Fensterkreuz oder **Schließen** beendet das Fenster.
Zurück, Vorwärts, Neu laden, Startseite und ein Vollbildmodus sind vorhanden.
Die Fensterposition und Größe werden gespeichert.

Maus, Scrollrad und Texteingaben gehen an den Browser. Kopieren/Einfügen
funktioniert mit Strg+C/Strg+V sowie über das Kontextmenü. Während das Fenster
die Eingabe übernimmt, werden Binder-Hotkeys und die Spielsteuerung gesperrt;
nach dem Schließen wird die bisherige Maus-/Tastaturbehandlung wieder aktiv.

Anmeldung, Beiträge, Nachrichten, Suche und Benachrichtigungen sind die
Originalfunktionen der Webseite. Es gibt kein nachgebautes Forum und keine
eigene Anmeldung. CEF verwaltet Cookies und Sitzung im separaten lokalen
Profil `%LOCALAPPDATA%/NLBinder/Browser/Forum`. Der Binder liest keine
Passwortfelder oder Cookies aus und protokolliert keine Texteingaben.

Chromium läuft als isolierter x64-Prozess mit aktivierter Sandbox. Die
32-Bit-ASI übernimmt das echte Offscreen-Bild als DirectX-9-Textur. Das hält
Chromium aus dem begrenzten GTA-Prozessspeicher heraus. Beim Schließen wird
der Browser beendet; im Notfall verhindert ein Prozess-Job zurückbleibende
Browserkinder. Auch bei als Administrator gestartetem GTA wird der Browser
mit den normalen Rechten desselben Windows-Benutzers gestartet.

Navigation innerhalb von Nova ist erlaubt. Neue Nova-Fenster öffnen im
vorhandenen Fenster; fremde Hauptseiten und ungefragte Downloads sind gesperrt.
Captchas, TLS-Prüfungen und das originale Login-System werden nicht umgangen.

## /speed mit Namen

Neben `/speed 57` werden vollständige Spielernamen und eindeutige Namensteile
ab drei Zeichen akzeptiert, beispielsweise `/speed Spielername` oder
`/speed Spi`. Groß-/Kleinschreibung spielt keine Rolle. Ein exakter Name hat
Vorrang vor Teiltreffern. Bei mehreren passenden Spielern wird der Vorgang
mit einem lokalen Hinweis abgelehnt. Der erkannte Spieler wird über seine
ID an den bestehenden automatischen Wanted-Dialogablauf übergeben.

## Build und Prüfung

Die ASI bleibt Win32/x86, GTA SA 1.0 US / der vorhandene SA-MP-DL-1-Client.
Der Browser benötigt Windows x64. Das SDK ist in
`tools/cef-runtime.lock.json` fixiert und wird mit
`python tools/fetch_cef_runtime.py` anhand der offiziellen Prüfsumme geladen.
`tools/build_forum_browser.bat` baut den x64-Host; `tools/build_release.bat`
baut die ASI. Beide Ausgaben plus die Runtime gehören zum vollständigen Paket.

Die Tests prüfen unter anderem 20 GPS-Einträge, Persistenz, einmalige Ankunft,
Interior-Grenzen, Akkordlohn für alle registrierten Jobs und die native
Ausdauerreserve. Zusätzliche Tests laden das echte Forum mit frischem anonymen
Profil, prüfen gerenderte Pixel/Scrollen und die native x86-zu-x64-Anbindung
einschließlich sauberem Beenden. Eine Anmeldung oder ein Beitrag wird dabei
nicht automatisch abgeschickt. Ein gesonderter Live-Test in einer laufenden
GTA-Sitzung wurde nicht durchgeführt.

[CEF-Architektur und Sandbox](https://chromiumembedded.github.io/cef/sandbox_setup),
[CEF-Offscreen-Rendering](https://chromiumembedded.github.io/cef/general_usage#off-screen-rendering).
