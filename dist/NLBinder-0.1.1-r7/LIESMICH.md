# NLBinder v.0.1.1 by KOBiRO – Überarbeitung 7

Stand: 25. Oktober 2026. Diese Fassung ergänzt eigene Waffenicons, den
Dialogablauf für `/speed ID` und die Übernahme bestehender Job-Skillpunkte.
Die Oberfläche verwendet weiterhin die bisherigen Binder-Komponenten.

## Installation

1. GTA/SA-MP schließen und die bisherige Binder-ASI sichern.
2. Die bisherige ASI an ihrem bisherigen ASI-Installationsort im Spiel durch
   `Keybinder.asi` ersetzen. Nur eine Binder-Fassung laden; der vorhandene
   ASI-Loader bleibt erforderlich.
3. Den mitgelieferten Vorlagenordner `NLBinder` nach
   **Dokumente → GTA San Andreas User Files** kopieren und dort mit dem
   bestehenden `NLBinder`-Ordner zusammenführen. Vorhandene eigene PNGs
   beibehalten.
4. GTA San Andreas 1.0 US mit SA-MP 0.3.DL-R1 starten und normal einloggen.

Prüfsumme und endgültige Buildnachweise gehören zu dieser ASI und stehen in
`BUILD-INFO.txt`, `SHA256SUMS.txt` und `PRUEFBERICHT.json`. Ein echter
Ingame-Test auf dem konkreten Server steht aus.

## Eigene Waffenicons

Unter **Mods → Waffenicons**, direkt neben **Hud**, gibt es eine eigene
Bildauswahl im bisherigen Stil. **Waffenicons aktivieren** schaltet die
Funktion insgesamt ein oder aus. Über die Waffen-Auswahl wird die jeweilige
Waffe ausgewählt; darunter erscheinen deren PNGs als Vorschau.

Die Bilder liegen unter **Dokumente → GTA San Andreas User Files → NLBinder →
Waffenicons**. Am einfachsten öffnet **Mods → Waffenicons → Waffenicons-Ordner
öffnen** den richtigen Ordner. Das berücksichtigt auch einen verschobenen
Windows-Dokumente-Ordner, beispielsweise unter OneDrive.

Dort werden 43 passende Waffen-Unterordner angelegt, zum Beispiel:

```text
Dokumente/GTA San Andreas User Files/NLBinder/
  Waffenicons/
    Desert Eagle/
      mein-icon.png
    M4/
      blau.png
      rot.png
    AK-47/
```

Für eine Waffe können mehrere PNGs abgelegt werden. Nach neuen oder geänderten
Dateien **Ordner neu einlesen** verwenden. Ein Klick auf die Vorschau wählt
das Bild für diese Waffe aus. Ohne ausdrückliche Bildauswahl verwendet die
automatische Auswahl die erste PNG nach Dateinamen. **Original für diese
Waffe** schaltet nur deren Ersetzung aus; die globale Aktivierung bleibt für
andere Waffen erhalten. Die Auswahl und Schalter werden gespeichert.

Ohne passende oder ladbare PNG bleibt das originale Waffenicon sichtbar.
Eine ausdrücklich gespeicherte, inzwischen fehlende Datei wird nicht
unbemerkt durch eine andere PNG ersetzt. Die Fist bleibt wie bisher unter
**Mods → Hud**. Es werden keine Spieldateien überschrieben; die Ersetzung
betrifft die lokale HUD-Anzeige. Das Paket enthält keine fremden Iconbilder
und übernimmt keine persönlichen PNGs in die Veröffentlichung.

## `/speed ID`

`/speed 123` verwendet die angegebene Spieler-ID für genau einen Vorgang.
Der Binder öffnet `/su`, wählt im Wantedkatalog **Ordnungswidrigkeit**, danach
**Geschwindigkeitsüberschreitung ab 20km/h** und trägt im passenden Fenster
**Wanteds vergeben** die ID ein. Auf ausdrücklichen Benutzerwunsch wird dort
auch **Vergeben** einmal ausgelöst. Der gezeigte Tatbestand umfasst
15 Wantedpunkte. Der Vorgang dient der Bedienung des vorhandenen
Serverdialogs; er misst keine Geschwindigkeit und erkennt keinen Verstoß.

Die Auswahl orientiert sich an den Dialogtiteln und Eintragstexten. Ändert
sich die Reihenfolge, wird weiterhin der passende Eintrag gesucht. Ein
unpassender Dialog, ein abgebrochener Ablauf, ein Verbindungswechsel oder ein
Timeout darf keine weitere Bestätigung auslösen. Die ID wird beim Start
geprüft; ein neuer Vorgang überschreibt keinen laufenden Vorgang.

## Statistik und vorhandene Job-Skillpunkte

Nach dem Login wird zunächst die eigene Statistik über `/stats` gelesen.
Spielername und Zähler stammen weiterhin aus dem aktuellen Account und sind
nicht auf einen bestimmten Spielernamen festgelegt. Die r6-Korrektur für den
Titel `Statistiken » Spielername` bleibt enthalten.

Anschließend wird `/jobskills` für die bestehenden Skillstände ausgewertet.
Erkannte Punkte werden als **absolute Ausgangsstände** in die vorhandenen
Jobs übernommen und im Job-Feed sichtbar gemacht. Ein vorhandener Stand von
500 Punkten wird dadurch nicht als neu erspielter Zuwachs von 500 Punkten
gezählt. Wiederholt gelieferte gleiche Stände erzeugen keine doppelten
Verdienstereignisse. Unbekannte Server-Jobnamen werden nicht erfunden oder
automatisch einem anderen Job zugeordnet.

Der Abruf erfolgt ohne zusätzliche feste Wartepause, wartet aber auf die
echten Serverantworten. Nur erkannte eigene Antworten werden übernommen;
andere Dialoge werden nicht pauschal geschlossen.

## Automatischer Motorstart

Der Motorstart bleibt auf höchstens einen Motor-Tastendruck pro
Fahrer-Einstieg begrenzt. Eine bestätigte serverseitige Addon-Motorstartmeldung
wird berücksichtigt, damit der Binder danach keinen eigenen Motor-Toggle
mehr sendet. Bereits laufende oder später manuell gestoppte Motoren werden
im selben Einstieg nicht erneut bedient. Eingaben in Chat, Dialog oder Binder
bleiben geschützt.

## Quellen und Prüfung

Quellcode, Änderungsübersicht und die zum endgültigen Binärstand gehörenden
Prüfnachweise stehen im
[GitHub-Prüfrepository](https://github.com/Kobiro57/PerkysBinderChangelog).
Persönliche Screenshots, Accountwerte, Laufzeitkonfigurationen und eigene
PNG-Sammlungen werden nicht veröffentlicht. Frühere r6-/r5-Stände bleiben
als frühere Prüfstände erhalten.
