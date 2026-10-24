# Abhängigkeiten und Herkunft des veröffentlichten Quellstands

Dieser Prüfstand baut auf dem vorhandenen Perkys-/NLBinder-Projekt auf. Die
ursprünglichen Quellen und Hinweise bleiben erhalten. Eine neue pauschale
Projektlizenz wird durch diese Dokumentation nicht vergeben.

## Originalarchiv als Vergleichsbasis

Für den Vergleich steht das lokal bereitgestellte Archiv `NLBinder-master.zip`
zur Verfügung. Sein SHA-256 lautet:

```text
a328e78eea63420e9bfe9c084883100177fe0fd9c2d62c4fcaef38c0a44801e9
```

Der ZIP-Kommentar enthält die Commitkennung
`92ce21971bee2d166d830e23ad20950f1dc98921`. Das ist eine Herkunftsangabe aus dem
Archiv, keine nachträglich erfundene lokale Entwicklungshistorie. Der lokale
Arbeitsordner hatte vor der Veröffentlichung keine eigene Git-Historie.

Ohne Unterschiede allein durch CRLF/LF zu zählen, ergab der Vergleich vor dem
Hinzufügen dieser Veröffentlichungsunterlagen 59 geänderte und 16 neue Dateien
in den Projektquellen, Tests, Ressourcen und Buildanleitungen; keine dieser
Dateien wurde entfernt. Die zusätzlich wiederhergestellten Vendor-Bibliotheken
werden dabei getrennt betrachtet. Insbesondere enthielt das Originalarchiv
keine Dateien unter `include/plugin-sdk/` und `include/RakHook/`.

## Verwendete Bibliotheken

| Bestandteil | Stand / Herkunft | Enthaltener Hinweis |
| --- | --- | --- |
| Plugin-SDK | `DK22Pac/plugin-sdk`, Commit `15f15b60bbf74c106e1b496ff92c98764abf4605` | [LICENSE](../../include/plugin-sdk/LICENSE) |
| RakHook | `imring/RakHook`, Commit `109daae3031cd030d272fa35407412443f03dede`; kompatible 1.0-API | [LICENSE](../../include/RakHook/LICENSE) |
| SAMP-API | Vorhandener Stand für SA-MP 0.3.DL-R1 aus dem Ausgangsprojekt | [LICENSE](../../include/SAMP-API/LICENSE) |
| Dear ImGui | Vorhandener Ordner `imgui-1.92.7` | [LICENSE.txt](../../include/imgui-1.92.7/LICENSE.txt) |
| SafetyHook / Zydis | Mit dem gepinnten Plugin-SDK geliefert | [LICENSE.txt](../../include/plugin-sdk/safetyhook/LICENSE.txt) |
| pugixml | 1.14; MIT-Hinweis im Quelltext | [pugixml.hpp](../../include/pugixml/pugixml.hpp) |
| FreeType | 2.13.3; direkt mitkompiliert | [LICENSE.TXT](../../third_party/freetype/LICENSE.TXT), [FTL.TXT](../../third_party/freetype/docs/FTL.TXT) |
| Inter | Mitgelieferte Schrift | [OFL 1.1](../../third_party/assets/LICENSES/Inter-OFL-1.1.txt) |
| Material Symbols | Mitgelieferte Google-Icons | [Vorhandener Apache-2.0-Hinweis](../../third_party/assets/LICENSES/MaterialSymbols-Apache-2.0.txt) |

Der ursprüngliche verkürzte Material-Symbols-Hinweis bleibt erhalten. Für diese
Veröffentlichung wurde der vollständige offizielle Lizenztext als
[Apache-2.0-full.txt](../../third_party/assets/LICENSES/Apache-2.0-full.txt)
ergänzt (Quelle: https://www.apache.org/licenses/LICENSE-2.0.txt).

Die kleinen vorhandenen Adapter unter `include/cyanide/` und
`include/polyhook2/` verwenden die Plugin-SDK-SafetyHook-Anbindung. Sie wurden
nicht durch eine neuere, inkompatible RakHook-API ersetzt.

Die DirectX-9-Header und Importbibliotheken stammen aus dem mitgelieferten
SDK-Bestand. Sie behalten ihre vorhandenen Copyright-Hinweise. Der Binder
verlinkt `d3d9.lib` und `d3dx9.lib`; die Windows-Systembibliotheken liefert das
installierte Windows SDK.

## Für einen neuen Build benötigte Dateien

Benötigt werden `src/`, die Projekt-Header und Vendor-Quellen unter `include/`,
`third_party/`, die Ressourcen unter `res/`, `Keybinder.rc`,
`Keybinder.vcxproj`, `Keybinder.sln` und die Buildskripte unter `tools/`.
Die Tests stehen unter `tests/`.

Visual Studio 2022 Build Tools mit v143 und Windows SDK 10.0 werden lokal
vorausgesetzt. Die Anleitung und gepinnten Quellen stehen zusätzlich in
[build-dependencies.md](../build-dependencies.md).

Die allgemeinen Ausschlüsse für `*.exe` und `*.lib` dürfen benötigte
Abhängigkeitsdateien nicht versehentlich entfernen: Für den aktuellen
Release-Build sind mindestens `include/plugin-sdk/shared/dxsdk/d3d9.lib`,
`include/plugin-sdk/shared/dxsdk/d3dx9.lib` und für die automatische
SDK-Projekterzeugung `include/plugin-sdk/tools/premake/premake5.exe` relevant.
`plugin.lib` wird aus den SDK-Quellen neu gebaut.

Nicht zu den Quellen gehören verschachtelte `.git/`-Verzeichnisse, erzeugte
SDK-`output/`-Ordner, Objektdateien, IDE-Zustand oder lokale Laufzeitdaten.
Prüfberichte und die ausdrücklich veröffentlichte ASI werden als ausgewählte
Prüfartefakte geführt; persönliche Binder-Konfigurationen und Chatlogs gehören
nicht zum veröffentlichten Quellstand.
