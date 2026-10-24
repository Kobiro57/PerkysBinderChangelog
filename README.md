# NLBinder

Advanced keybinder and combat-assist plugin for GTA San Andreas Multiplayer (SA-MP) 0.3.DL.

## Features

- **Keybind System** — Create keybinds triggered by single keys, key combinations, or slash commands. Each keybind runs a sequence of actions: chat messages, SA-MP commands, delays, and simulated key presses. Placeholder expansion (`{zone}`, `{hp}`, `{weste}`, `{total_kills}`, etc.) injects live game state into your commands.
- **Kill Tracker** — Track weapon statistics, kill/death ratios, and damage per victim in real time. Supports local and server-side kill/death message templates with customizable formatting. History for recent kills and deaths is persisted across sessions.
- **Hitmarker** — Visual combat feedback overlay with multiple shapes (Cross-X, Plus, Circle, Diamond, Custom PNG). Separate colors for regular hits and kill-confirmations. Preset system for quick switching, plus configurable size, opacity, duration, and fade-out.
- **QuickMap** — Full GTA:SA world map overlay with classic and satellite variants. Shows player position, checkpoints, race checkpoints, and gang zones. Toggle or hold-to-show modes with adjustable transparency.
- **Interface Editor** — Customize HUD element visibility (radar, area names, clock, vital stats, 3D markers), money display color and font style, and zone text overrides. Five preset slots for quick configuration switching.
- **Crosshair Switcher** — Replace the native crosshair with a custom PNG texture. Textures are scanned from a `crosshairs/` directory and selectable through the UI.
- **Hitsounds** — Configurable audio feedback for combat events. Separate sounds for outgoing hits (when you damage another player) and incoming hits (when you are damaged). Includes embedded defaults and supports custom WAV files from a `hitsounds/` folder.
- **Slash Command Keybinds** — Bind keybind actions to slash commands (e.g., `/heal` executes your heal keybind). Configurable primary/secondary UI open commands (`/kb`, `/keybinder` by default).
- **Auto-HUD Scale Fix** — Automatically sends `/hudscalefix` on SA-MP startup to correct HUD scaling on high-DPI displays.
- **Settings Management** — All configuration is unified in a single `keybinder.json` file: keybinds, kill tracker data, hitmarker presets, interface editor presets, theme settings, and QuickMap options.

## Building

### Requirements

- Visual Studio 2022 with v143 toolset
- Windows SDK 10.0

> **Note:** DirectX 9 headers and libraries (`d3d9.lib`, `d3dx9.lib`) are bundled under `include/plugin-sdk/shared/dxsdk`. A separate DirectX SDK installation is not required.

### Build Instructions

**Visual Studio:**

1. Open `Keybinder.sln` in Visual Studio 2022
2. Select `Release|Win32` configuration
3. Build the solution (F7)

**Command Line:**

```powershell
MSBuild "Keybinder.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal
```

Output: `build\Release\Keybinder.asi`

Debug builds output to `build\Debug\Keybinder.asi`.

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

1. Copy `Keybinder.asi` to your GTA San Andreas root directory
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
| [RakHook](https://github.com/Whitetigerswt/rakhook) | — | RakNet hooking for network event interception |
| [FreeType](https://freetype.org) | 2.13.3 | Font rendering for ImGui (compiled directly) |
| [safetyhook](https://github.com/nicemicro/safetyhook) | — | Inline hooking (bundled with plugin-sdk) |
| Inter | OFL 1.1 | UI typeface (embedded in ASI resources) |
| Material Symbols | Apache 2.0 | Icon font (embedded in ASI resources) |

## License

This project is provided as-is for educational purposes.
