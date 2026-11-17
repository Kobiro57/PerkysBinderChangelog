# Quellcode zur automatischen /forum-Einrichtung

Dieser Ordner ist ein gezielter Prüfauszug aus dem mitgelieferten vollständigen
NLBinder-Quellcode. Er enthält keine Browserprogramme, DLLs, privaten Profile,
Cookies, Zugangsdaten oder persönlichen Chatlogs. Textdateien sind für einen
stabilen Vergleich auf LF-Zeilenenden vereinheitlicht.

## Zuerst lesen

- [Ablauf, Downloadquelle, Installationspfade und lokale Browserdaten](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien mit Größe und SHA-256](docs/FORUM-DATEILISTE.txt)
- [Originales maschinenlesbares Runtime-Manifest](res/browser-runtime.json)
- `SOURCE-MANIFEST.json`: Größen und SHA-256 der enthaltenen Prüfauszugsdateien.
- `SHA256SUMS.txt`: Prüfsummen aller Nutzdateien einschließlich des Quellmanifests.

## Der eigentliche Code

`tools/BrowserSetup.cpp` enthält den nativen x86-Installer. Er verwendet Windows
WinHTTP/BCrypt und die enthaltenen miniz-Quellen unter `third_party/miniz/`.
`src/ForumBrowser.cpp` steuert ihn aus der ASI und verbindet Maus, Tastatur und Bild.
`browser/ForumBrowserHost.cpp` ist der selbst gebaute x64-CEF-Host. `src/Main.cpp`
zeigt die Registrierung von `/forum` und die AFK-/Back-Anbindung. Protokolle,
Ressourceneinbettung und Prüfmanifeste liegen in den jeweiligen Unterordnern bei.

Zum Bauen des nativen Installers: Visual Studio C++ Build Tools v143 und Windows
SDK, dann `tools/build_browser_setup.bat`. Das erzeugt `build/BrowserSetup.exe`.
Zum Bauen des Hosts: Python nur als Entwicklerwerkzeug für den gepinnten SDK-Abruf,
CMake, Windows-x64-C++-Toolchain und das passende CEF-SDK aus `tools/cef-runtime.lock.json`.
`tools/fetch_cef_runtime.py` lädt das SDK; `tools/build_forum_browser.bat` baut den Host.
Diese Skripte werden durch das Lesen/Entpacken dieses Ordners nicht ausgeführt.
Der Prüfauszug enthält nicht alle Abhängigkeiten/Ressourcen zum Bauen der ASI;
dafür den zusätzlich mitgelieferten vollständigen Binder-Quellbaum verwenden.

Der eigene Installer und der eigene Host liegen als tatsächlicher Quellcode bei.
Die komplette Chromium-/CEF-Engine liegt nicht als Quellbaum in diesem Paket.
Die installierte Runtime enthält vorcompilierte Drittanbieterdateien. Offizielle
Originalquellen und Lizenzen sind in der Installationsbeschreibung verlinkt;
die genaue verwendete Version und Binärdatei-Hashes stehen im Runtime-Manifest.
Eine veröffentlichte Prüfsumme beweist Dateigleichheit, nicht automatisch einen
reproduzierbaren Build oder die Fehlerfreiheit eines Browsers.
