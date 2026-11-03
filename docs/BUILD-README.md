# NLBinder

Advanced keybinder and combat-assist plugin for GTA San Andreas Multiplayer (SA-MP) 0.3.DL.

## Features

- **Keybind System** — Create keybinds triggered by single keys, key combinations, or slash commands. Each keybind runs a sequence of actions: chat messages, SA-MP commands, delays, and simulated key presses. Placeholder expansion (`{zone}`, `{hp}`, `{weste}`, `{total_kills}`, etc.) injects live game state into your commands.
- **Kill Tracker** — Track weapon statistics, kill/death ratios, and damage per victim in real time. Supports local and server-side kill/death message templates with customizable formatting. History for recent kills and deaths is persisted across sessions.
- **Hitmarker** — Visual combat feedback overlay with multiple shapes (Cross-X, Plus, Circle, Diamond, Custom PNG). Separate colors for regular hits and kill-confirmations. Preset system for quick switching, plus configurable size, opacity, duration, and fade-out.
- **QuickMap** — Full GTA:SA world map overlay with classic and satellite variants. Shows player position, checkpoints, race checkpoints, and gang zones. Toggle or hold-to-show modes with adjustable transparency.
- **Interface Editor** — Move and recolor the money display, weapon icon, ammunition and radar using the shared overlay move mode. Esc saves the layout. Five preset slots allow quick configuration switching.
- **Crosshair Switcher** — Replace the native crosshair with a custom PNG texture. Textures are scanned from a `crosshairs/` directory and selectable through the UI.
- **Hitsounds** — Configurable audio feedback for combat events. Separate sounds for outgoing hits (when you damage another player) and incoming hits (when you are damaged). Includes embedded defaults and supports custom WAV files from a `hitsounds/` folder.
- **Slash Command Keybinds** — Bind keybind actions to slash commands (e.g., `/heal` executes your heal keybind). Configurable primary/secondary UI open commands (`/kb`, `/keybinder` by default).
- **Auto-HUD Scale Fix** — Automatically sends `/hudscalefix` on SA-MP startup to correct HUD scaling on high-DPI displays.
- **Settings Management** — All configuration is unified in a single `keybinder.json` file: keybinds, kill tracker data, hitmarker presets, interface editor presets, theme settings, and QuickMap options.
- **State Synchronization** — Stats and job skills synchronize after login. **Einstellungen → General → Tacho → Aktuellen Spielstand synchronisieren** repeats the same import manually and confirms completion locally after the new values are saved.

## Building

### Requirements

- Visual Studio 2022 with v143 toolset
- Windows SDK 10.0
- Plugin-SDK sources and its Release Win32 `plugin.lib` in `include/plugin-sdk` (see [build dependency versions](docs/build-dependencies.md))

> **Note:** DirectX 9 headers and libraries (`d3d9.lib`, `d3dx9.lib`) are bundled under `include/plugin-sdk/shared/dxsdk`. A separate DirectX SDK installation is not required.

### Build Instructions

**Visual Studio:**

1. Open `Keybinder.sln` in Visual Studio 2022
2. Select `Release|Win32` configuration
3. Build the solution (F7)

**Command Line:**

```powershell
tools\build_release.bat
```

Output: `build\Release\NLBinder.asi`

The script locates Visual Studio Build Tools through `vswhere`, including custom installation paths, and builds the Plugin-SDK library if it is missing.

Debug builds output to `build\Debug\NLBinder.asi`. The solution and project filenames remain `Keybinder.sln` and `Keybinder.vcxproj`.

### Running Tests

```powershell
tools\build_and_test.bat
```

Or manually:

```powershell
tools\run_tests.bat
powershell -ExecutionPolicy Bypass -File "build\run_tests.ps1"
```

## Installation

1. Close GTA and back up the former `Keybinder.asi` outside the game's ASI directories, then copy `NLBinder.asi` to your existing Binder installation location. Load only one Binder ASI.
2. Install an ASI loader (if not already present)
3. Launch SA-MP 0.3.DL

The plugin creates a `NLBinder` folder under the user's GTA San Andreas user files directory containing:
- `keybinder.json` - configuration file
- `keybinder.log` - runtime log
- `keybinder_crash.log` - crash dump
- `hitmarkers\` - custom hitmarker textures
- `crosshairs\` - custom crosshair textures
- `hitsounds\` - custom hitsound WAV files

## Usage

| Action | Control |
|--------|---------|
| Open/close overlay | **F2** (default) or `/kb`, `/keybinder` |
| Close overlay | **ESC** |
| Toggle QuickMap | **M** (default) |
| Navigation | Mouse and keyboard within the ImGui interface |

All hotkeys are configurable in settings. The QuickMap mode (toggle vs. hold) is also configurable.

## Project Structure

```
├── include/              # First-party headers (top-level .h) + vendored dependencies
│   ├── plugin-sdk/       # GTA SA plugin SDK + safetyhook inline hooking
│   ├── SAMP-API/         # SA-MP 0.3.DL API definitions
│   ├── RakHook/          # RakNet hooking library
│   ├── imgui-1.92.7/     # Dear ImGui + DX9/Win32 backends + FreeType integration
│   ├── cyanide/          # Cyanide hooking utilities
│   └── *.h               # First-party public headers (pair with src/*.cpp)
├── src/                  # First-party implementation (flat layout)
├── third_party/
│   ├── freetype/         # FreeType 2.13.3 (compiled directly)
│   └── assets/          # Bundled fonts (Inter, Material Symbols) + license files
├── res/                  # Embedded resource images (map textures, logo, player blip)
├── tests/                # Standalone C++ test harness
├── tools/                # Build, test, and QA automation scripts
├── build/                # Build output (binaries, object files, logs)
├── Keybinder.sln         # Visual Studio solution
├── Keybinder.vcxproj     # Visual Studio project
└── Keybinder.rc          # Resource script (embeds fonts and images into the ASI)
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) | — | GTA SA plugin SDK, hooking, and game interfaces |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.7 | Immediate mode GUI with DX9/Win32 backends |
| [SAMP-API](https://github.com/BlastHackNet/SAMP-API) | 0.3.DL-1 | SA-MP API and structure definitions |
| [RakHook](https://github.com/imring/RakHook) | 1.0 API | RakNet hooking for network event interception |
| [FreeType](https://freetype.org) | 2.13.3 | Font rendering for ImGui (compiled directly) |
| [safetyhook](https://github.com/nicemicro/safetyhook) | — | Inline hooking (bundled with plugin-sdk) |
| Inter | OFL 1.1 | UI typeface (embedded in ASI resources) |
| Material Symbols | Apache 2.0 | Icon font (embedded in ASI resources) |

## License

This project is provided as-is for educational purposes.

## r16 additions

Stamina sidebar, Akkord earnings, 20 persistent local GPS destinations,
player-name resolution for `/speed`, and a real CEF offscreen forum browser.
See [German r16 instructions](NLBinder-0.1.1-r16-DE.md).

The ASI remains x86. The browser runs separately as x64 using the original CEF
sandbox bootstrap. Fetch the pinned SDK with `python tools/fetch_cef_runtime.py`,
build with `tools/build_forum_browser.bat`, and assemble the required runtime
with `python tools/prepare_browser_runtime.py`. Install `build/NLBinder/Browser`
next to `NLBinder.asi`; the ASI alone does not contain Chromium. Windows x64,
VS 2022 x86/x64 v143 tools, Windows SDK and CMake are required.

`tools/run_revision16.bat tests` runs the complete suite. `smoke` loads the
real forum anonymously; `native-smoke` exercises x86-to-x64 IPC with a fresh
test profile. No automated account login or post submission is performed.

## r17 automatic browser setup

Distribute only NLBinder.asi. The embedded native BrowserSetup.exe installs
the pinned runtime into the NLBinder Browser folder in Documents on first
/forum. The browser cookie profile remains separate in LocalAppData.
The old r16 instruction to copy the Browser directory beside the ASI is
historical; r17 can migrate such a supplied folder without downloading it.

The installer sources are tools/BrowserSetup.cpp and third_party/miniz.
BuildBrowserSetup runs automatically before ResourceCompile and embeds the
result as RCDATA 301. The immutable archive lock and per-file hashes are
res/browser-runtime.json and include/BrowserRuntimeLock.h. Building the ASI
does not require re-downloading or re-building CEF when using this pinned host.
The existing CEF host build instructions remain valid for a future host update.

tools/run_revision17.bat tests runs the full suite. setup-tests additionally
tests the native installer against the actual public pinned HTTPS download in
private fixture directories. native-smoke installs from the embedded resource,
opens the real forum four times with one anonymous profile and verifies clean
shutdown and queued/early reopen. No account login or post is automated.
