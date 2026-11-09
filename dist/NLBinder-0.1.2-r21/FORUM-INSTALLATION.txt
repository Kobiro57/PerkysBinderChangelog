# Was wird beim ersten `/forum` eingerichtet?

Diese Beschreibung gehört zu NLBinder v.0.1.2. Der verwendete Browserbestand
bleibt **cef156-r1**. Der eigene Installer, die Browseranbindung und der eigene
Browserhost liegen als lesbarer C++-Quellcode bei. Die komplette Chromium-Engine
ist eine gesonderte Drittanbieterbibliothek und kein selbst geschriebener Binder-Code.

## Ablauf

1. `/forum` öffnet das Forumfenster. Der Binder sendet dabei einmal `/afk`;
   beim Schließen, auch über ESC, einmal `/back`, sofern er den AFK-Befehl selbst
   erfolgreich abgeschickt hat. Der Server bestimmt die Wirkung dieser Befehle.
2. Ein in `NLBinder.asi` eingebetteter nativer Windows-Helfer wird in den
   Browserordner geschrieben und gestartet. Python, PowerShell, ein zusätzlicher
   Entpacker und eine manuelle Browserinstallation werden beim Nutzer nicht benötigt.
3. Ein bereits gültiger Bestand im Dokumente-Ordner wird wiederverwendet.
   Alternativ können passende Dateien aus `NLBinder\Browser` neben der ASI
   nach vollständiger Prüfung übernommen werden. Sonst wird das feste Browser-ZIP
   über HTTPS heruntergeladen. Es wird keine beliebige jeweils neueste Version geladen.
4. Archivgröße und SHA-256 müssen stimmen. Beim Entpacken werden die erwarteten
   Dateinamen, Größen und einzelnen SHA-256-Werte geprüft. Unerwartete ZIP-Einträge,
   Pfadwechsel und Verknüpfungen werden abgelehnt. Erst der geprüfte Dateistand
   wird als aktiver Runtime-Ordner eingesetzt.
5. Der getrennte x64-Browserprozess zeigt `https://nova-legacy.de/forum`
   innerhalb des Spiel-Overlays an. Die x86-ASI lädt `libcef.dll` nicht in GTA.
   Die CEF-Sandbox bleibt eingeschaltet; der Host verweigert einen Start als Administrator.

Nach einer vollständigen Prüfung speichert `.verified-runtime` Dateigrößen und
Änderungszeiten. Unveränderte Dateien können damit beim nächsten Start schneller
wiederverwendet werden. Bei abweichenden Metadaten wird wieder vollständig gehasht.
Dieser lokale Nachweis ist eine Wiederverwendungsprüfung, keine Signatur einer
unabhängigen Prüfstelle und kein Beweis, dass Quelltext und Binärdateien identisch sind.

## Download und Umfang

- Paket: `NLBinder-Browser-cef156-r1.zip`
- Download: **188.258.850 Bytes** (rund 188 MB beziehungsweise 179,5 MiB).
- Entpackter festgelegter Bestand: **236 Dateien, 429.252.102 Bytes**
  (rund 429 MB beziehungsweise 409,4 MiB).
- Archiv-SHA-256:
  `01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45`
- CEF: `156.0.15+gcf671c0+chromium-156.0.8078.80`, Windows x64.

[Festes Browserpaket auf GitHub](https://github.com/Kobiro57/PerkysBinderChangelog/releases/download/browser-cef156-r1/NLBinder-Browser-cef156-r1.zip),
[maschinenlesbares Manifest](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/res/browser-runtime.json),
[vollständige lesbare Dateiliste mit Größen und SHA-256](FORUM-DATEILISTE.txt).

Der Installer akzeptiert für Download/Weiterleitungen ausschließlich HTTPS auf
Port 443 und die Hosts `github.com`, `release-assets.githubusercontent.com` und
`objects.githubusercontent.com`. Die Downloadanfrage geht an GitHub; dabei fallen
die üblichen Verbindungsdaten wie die IP-Adresse beim jeweiligen Anbieter an.

## Wo werden die Daten gespeichert?

Der Dokumente-Ordner wird bei Windows abgefragt. Er kann verschoben oder etwa
durch OneDrive umgeleitet sein; `C:\Users\<Name>\Documents` ist nur der übliche
Beispielpfad, keine fest eingebaute Vorgabe.

| Ort | Inhalt |
| --- | --- |
| `<Windows-Dokumente>\GTA San Andreas User Files\NLBinder\Browser\cef156-r1\` | Aktive Browserprogramme, DLLs, Ressourcen, Sprachdateien und CEF-Lizenz-/README-Dateien aus dem Manifest. |
| `…\Browser\cef156-r1\.verified-runtime` | Lokal erzeugter Prüf-/Wiederverwendungsnachweis; zusätzliche Datei außerhalb des ZIP-Manifests. |
| `…\Browser\Cache\cef156-r1.zip` | Downloadcache des Installers; kann für einen erneuten Entpackversuch wiederverwendet werden. |
| `…\Browser\.setup-<GUID>\Files\` | Getrennter Vorbereitungsordner während Prüfung und Einrichtung. |
| `…\Browser\BrowserSetup-<PID><GUID>.exe` | Temporäre Kopie des in der ASI enthaltenen Installer-Helfers. |
| `…\Browser\cef156-r1.previous-<GUID>\` | Bei einer Reparatur gegebenenfalls erhaltene Sicherung des vorherigen Bestands. |
| `%LOCALAPPDATA%\NLBinder\Browser\Forum\` | Eigenes Browserprofil, unter anderem Cookies, Sitzungsdaten und websiteabhängige lokale Daten. |
| `%LOCALAPPDATA%\NLBinder\Browser\Forum\Cache\` | CEF-Webcache des Profils; nicht mit dem ZIP-Downloadcache unter Dokumente verwechseln. |
| `<Windows-Dokumente>\GTA San Andreas User Files\NLBinder\keybinder.log` | Allgemeines Binder-Log, unter anderem Browserstart und Fehler. |
| `<Windows-Dokumente>\GTA San Andreas User Files\NLBinder\keybinder_crash.log` | Allgemeines Diagnose-/Absturzlog, sofern ein entsprechender Fall auftritt. |

Der Basisordner `NLBinder\Browser` kann bereits beim normalen Binderstart angelegt
werden. Der größere Runtime-Bestand wird bei Bedarf über `/forum` eingerichtet.
Der Installer versucht, seinen Vorbereitungsordner und das ZIP nach erfolgreicher
Aktivierung zu bereinigen; die ASI entfernt ihre temporäre Helferdatei. Nach einem
Abbruch oder frühem Prozessende können Cache-/Hilfsdateien zurückbleiben. Das fertige,
geprüfte ZIP darf einen Abbruch überleben, damit ein neuer Versuch nicht erneut laden
muss. Reparatursicherungen werden nicht automatisch als persönliche Browserprofile
veröffentlicht oder in den Quellcodeordner kopiert. Während Einrichtung oder Reparatur
können ZIP, vorbereitete Dateien und ein alter Bestand gleichzeitig Platz belegen.

## Forumbenutzung, Anmeldung und lokale Daten

Das Forum ist eine echte Webseite. Bei der Anmeldung werden die eingegebenen Daten
normal an den zuständigen Webserver übertragen; Beiträge werden beim Absenden im
Forum veröffentlicht. Das getrennte NLBinder-Profil übernimmt kein vorhandenes
Chrome-/Edge-/Firefox-Profil. **Sitzungscookies bleiben ausdrücklich gespeichert**, damit
die Anmeldung nach Schließen und erneutem Öffnen erhalten bleiben kann. Zusätzlich
kann die Browserengine Website-Daten und Cachedateien im eigenen Profil erzeugen.

Die CEF-eigene Logausgabe ist im Host deaktiviert. Der eigene Tastatur-/Mauskanal
zwischen ASI und Browser verwendet gemeinsamen Arbeitsspeicher; der implementierte
Browser-Eingabepfad schreibt einzelne Tastatureingaben nicht in ein eigenes Log.
Das ist keine pauschale Zusage, dass Webseiten oder die Browserengine keine
Formular-/Sitzungsdaten speichern. Eigene Profile, Cookies, Einstellungen und
persönliche Logs gehören nicht in das veröffentlichte Quellcodepaket.

Die Hauptnavigation und Popup-Ziele sind auf HTTPS-Adressen von `nova-legacy.de`
und dessen Subdomains begrenzt. Browser-Dateidownloads werden blockiert.
**Unterressourcen und Unterframes werden dadurch nicht pauschal auf diese Hosts
begrenzt.** Eine Webseite kann beispielsweise zusätzliche Ressourcen von anderen
Servern laden. Daher wird nicht behauptet, dass ausschließlich Nova-Netzverkehr
stattfindet. Der Projektinstaller richtet keinen Windows-Dienst und keinen
Windows-Autostart ein.

## Welcher Quellcode liegt bei?

Im Ordner `FORUM-INSTALLATION-QUELLCODE` stehen die tatsächlichen Quellen des
eigenen Installers und Hosts, die ASI-Anbindung, Protokolle, feste Manifeste,
Buildskripte und die verwendeten miniz-Quellen mit Lizenz. `SHA256SUMS.txt` und
`SOURCE-MANIFEST.json` dokumentieren den Dateibestand dieses Prüfauszugs.
Der vollständige Binder-Quellbaum liegt zusätzlich im gesamten Release-Quellcodeordner.

Wichtige Einstiegspunkte:

- [Installer: Download, Prüfen, Entpacken und Aktivieren](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/tools/BrowserSetup.cpp)
- [ASI: Helferstart, Fenster, Maus/Tastatur und Browser-Lebenszyklus](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/src/ForumBrowser.cpp)
- [Eigener CEF-Host: Profil, Webseite, Eingaben und Navigationsregeln](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/browser/ForumBrowserHost.cpp)
- [Speicherorte des Binders](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/src/PersistentPaths.cpp)
- [AFK-/Back-Paarung](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/include/ForumAfkSession.h)
- [Manifest des festen Browserpakets](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/res/browser-runtime.json)
- [CEF-SDK-Version und Herkunft für Entwickler](https://github.com/Kobiro57/PerkysBinderChangelog/blob/v0.1.2-r20/tools/cef-runtime.lock.json)

Der eigene Host und der Installer sind vollständig als Projektquellen enthalten.
Der separate Prüfauszug allein ersetzt aber nicht sämtliche Abhängigkeiten des
vollständigen ASI-Projekts. Zum Neubauen des x64-Hosts wird das genau gepinnte CEF-SDK
benötigt; der native x86-Installer verwendet Windows SDK und die mitgelieferten
miniz-Quellen. Die normalen Nutzer benötigen diese Buildwerkzeuge nicht.

Die Runtime enthält vorcompilierte Fremdkomponenten: beispielsweise `libcef.dll`
aus CEF/Chromium und `d3dcompiler_47.dll` aus dem Microsoft-Umfeld. **Es wird nicht
behauptet, dass Quellcode jeder installierten Fremd-DLL beiliegt. Der vollständige
Chromium-/CEF-Upstream-Quellbaum ist ebenfalls nicht im Binderpaket enthalten.**
Herkunft und weiterführender Originalquellcode:
[CEF-Projekt und Quellen](https://github.com/chromiumembedded/cef),
[CEF-Lizenz](https://github.com/chromiumembedded/cef/blob/master/LICENSE.txt),
[Chromium-Quellcode und Bauanleitung](https://www.chromium.org/developers/how-tos/get-the-code/),
[Chromium-Lizenz](https://chromium.googlesource.com/chromium/src/+/main/LICENSE),
[CEF-Buildindex](https://cef-builds.spotifycdn.com/index.json).
Die zum konkret verwendeten Binärbestand mitgelieferten Hinweise stehen in
`CEF-LICENSE.txt` und `CEF-README.txt` der Runtime; weitere Drittanbieteranteile
behalten ihre jeweiligen Hinweise. Die hier verlinkten Upstream-Projektseiten
sind kein Ersatz für die gepinnten Versions- und Datei-Hashes im lokalen Manifest.
