# NLBinder ImGui UI Architecture

This document describes the current NLBinder UI architecture and implementation around Dear ImGui. It is intended for external review, especially for evaluating whether the project should keep ImGui, refactor the current UI layer, or replace it with a retained UI library such as RmlUi.

## Project Context

NLBinder is a Windows C++ SA-MP 0.3.DL plugin built as `Keybinder.asi`. It runs inside the game process and renders an in-game overlay using Direct3D 9 and Dear ImGui.

Important constraints:

- The plugin is injected into an existing game process.
- The renderer is Direct3D 9.
- The UI must survive device lost/reset, alt-tab, shutdown, and game input focus changes.
- Input handling must coexist with SA-MP/GTA input and keybind execution.
- Most first-party code is flat under `src/` with public headers in top-level `include/`.

## High-Level UI Stack

The UI stack can be understood as three layers.

```text
Layer 3: Feature UI modules
  KeybindEditorUI, KeybindListUI, KillTrackerUI, HitmarkerUI,
  CrosshairUI, SquareMapUI, HudSpriteUI, HitsoundUI,
  InterfaceEditorUI, RssTrackerUI, OnlineUI, JobsUI, SpeedometerUI

Layer 2: Composition and design system
  MainWindowUI, UIComponents, ThemeManager, ToastNotification,
  AnimationHelper

Layer 1: Overlay, renderer, and platform lifecycle
  ImGuiOverlay, TextureManager, HitmarkerRenderer,
  QuickMap, OnlineOverlayRenderer
```

## Core Files

### Overlay and Lifecycle

| File | Role |
| --- | --- |
| `src/ImGuiOverlay.cpp` / `include/ImGuiOverlay.h` | Owns ImGui context lifecycle, D3D9 backend calls, Win32 input routing, overlay visibility, device lost/reset handling, and render-frame orchestration. |
| `src/Main.cpp` | Wires the overlay into plugin/game lifecycle events such as render, D3D lost/reset, shutdown, and initialization. |
| `src/InputManager.cpp` / `include/InputManager.h` | Gates keybind/input behavior when the overlay or ImGui wants keyboard/text input. |
| `src/TextureManager.cpp` / `include/TextureManager.h` | Manages `IDirect3DTexture9*` resources used by UI rendering. Mostly D3D9-oriented rather than ImGui-specific. |

### Composition and Design System

| File | Role |
| --- | --- |
| `src/MainWindowUI.cpp` / `include/MainWindowUI.h` | Main window shell, navigation/sidebar, layout composition, and dispatch to feature panels. |
| `src/UIComponents.cpp` / `include/UIComponents.h` | Reusable UI primitives: buttons, cards, tabs, toggles, selectable rows, inputs, sliders, color controls, panel chrome, and decorative effects. |
| `src/ThemeManager.cpp` / `include/ThemeManager.h` | Theme tokens and ImGui style application. Uses ImGui color/vector types in parts of the public surface. |
| `src/ToastNotification.cpp` / `include/ToastNotification.h` | Toast notification drawing using ImGui draw lists. |
| `include/AnimationHelper.h` | Pure animation/easing helpers. Not tightly coupled to ImGui. |

### Feature UI Modules

Feature screens are implemented as C++ classes with `Render()` methods. They generally call `ImGui::*` and/or `UIComponents::*` directly.

Representative modules:

- `KeybindEditorUI`
- `KeybindListUI`
- `KillTrackerUI`
- `HitmarkerUI`
- `CrosshairUI`
- `SquareMapUI`
- `HudSpriteUI`
- `HitsoundUI`
- `InterfaceEditorUI`
- `RssTrackerUI`
- `OnlineUI`
- `JobsUI`
- `SpeedometerUI`

## Render Frame Lifecycle

The main frame is orchestrated by `ImGuiOverlay::RenderFrame()`.

Conceptually the flow is:

```text
1. Start ImGui DirectX9 backend frame
2. Start ImGui Win32 backend frame
3. Start ImGui context frame
4. Draw overlay-level backdrop/decorations
5. Render MainWindowUI
6. Render callback-driven HUD/overlay elements
   - hitmarker
   - toasts
   - quick map
   - speedometer
   - online overlay
7. End ImGui frame
8. Build ImGui draw data
9. Submit ImGui draw data through DirectX9 backend
```

The current implementation relies on the standard Dear ImGui backends:

- `imgui_impl_dx9`
- `imgui_impl_win32`

These are vendored under `include/imgui-1.92.7/`.

## Device Lifecycle

Because this is a D3D9 injected overlay, device lifecycle is a critical part of the design.

The overlay handles:

- D3D9 device discovery.
- Device lost events.
- Device reset events.
- ImGui device object invalidation and recreation.
- Font atlas rebuilds when needed.
- Theme reapplication after reset.
- Shutdown ordering.

This behavior is central to plugin stability. Any replacement UI stack must provide equivalent handling for D3D9 resources, font/texture resources, and render-state safety.

## Input Lifecycle

Input is shared between the game, SA-MP, keybind execution, and the overlay UI.

The current ImGui path handles input through several mechanisms:

- Win32 messages are routed through the ImGui Win32 backend.
- Mouse position is adjusted/overridden when the game attempts to keep the cursor centered.
- Keyboard capture is checked through ImGui IO flags such as `WantCaptureKeyboard` and `WantTextInput`.
- Game mouse input is suppressed when the overlay owns the cursor.
- DirectInput mouse hooks prevent the game from consuming mouse state while the overlay is active.
- Cursor positioning hooks prevent the game from warping the cursor during overlay interaction.

Any UI replacement needs an equivalent concept of:

- overlay visible/hidden,
- wants keyboard,
- wants text input,
- wants mouse,
- mouse position ownership,
- safe interaction with keybind dispatch.

## ImGui Coupling Surface

The project is not only using basic ImGui widgets. It also uses lower-level ImGui primitives and internals.

### Public ImGui API Usage

Common usage includes:

- windows and child windows,
- text rendering,
- buttons,
- checkboxes,
- combo boxes,
- sliders,
- inputs,
- color controls,
- image rendering,
- style push/pop,
- layout helpers,
- item hover/active checks,
- tooltips.

This category is the easiest to migrate or wrap.

### ImDrawList Custom Rendering

Several modules draw directly with `ImDrawList` primitives.

Examples include:

- glass/backdrop effects,
- glow rectangles,
- arcs and gauges,
- hitmarker shapes,
- map markers,
- custom toast effects,
- decorative panel chrome,
- online overlay rendering.

This is one of the hardest migration areas because immediate draw-list commands do not directly map to document-based UI libraries. A replacement would need one of:

1. custom render elements/decorators in the new UI library,
2. a separate D3D9 draw pass outside the UI library,
3. off-screen texture generation,
4. rewritten visual designs that use native retained-layout primitives.

### ImGui Internals

`UIComponents` uses ImGui internals for custom widget behavior and styling. This includes patterns around:

- item layout,
- hit testing,
- ID generation,
- item state,
- animation state storage,
- direct draw-list rendering,
- internal geometry helpers.

This makes `UIComponents` the hardest single area to replace cleanly.

## Approximate Coupling Inventory

Content searches show substantial ImGui coupling across first-party UI code:

- 24 `src/` files include or reference ImGui-related APIs/patterns.
- High-count files include:
  - `src/UIComponents.cpp`
  - `src/KillTrackerUI.cpp`
  - `src/JobsUI.cpp`
  - `src/OnlineUI.cpp`
  - `src/MainWindowUI.cpp`
  - `src/OnlineOverlayRenderer.cpp`
  - `src/KeybindEditorUI.cpp`
  - `src/RssTrackerUI.cpp`
  - `src/HitmarkerUI.cpp`
  - `src/ImGuiOverlay.cpp`

Headers that expose ImGui-related types include, among others:

- `include/UIComponents.h`
- `include/ThemeManager.h`
- `include/QuickMap.h`
- `include/HitmarkerRenderer.h`
- `include/ToastNotification.h`
- `include/SpeedometerUI.h`
- `include/OnlineOverlayRenderer.h`
- `include/ImGuiOverlay.h`

This means ImGui is part of both implementation and public first-party interfaces.

## Subsystems with Low or No UI Coupling

Many core plugin subsystems are mostly independent from ImGui and should not require major changes in a UI migration.

Likely low-impact areas:

- `ActionExecutor`
- `KeybindManager`
- `SAMPBridge`
- `KillTracker`
- `StatsStorage`
- RPC adapters
- `SettingsManager` data model, except where UI-facing theme/vector types leak in
- `HitsoundPlayer`
- `CrosshairSwitcher`
- `RssFeedManager` / parser / HTTP client
- job tracking logic
- online/player data logic

The main migration impact is concentrated in rendering, input ownership, UI composition, reusable UI components, and visual widgets.

## Current Strengths of the ImGui Architecture

- Works naturally with a C++ game plugin.
- Direct3D9 and Win32 backends already exist and are integrated.
- Immediate-mode rendering is simple for plugin/debug-style UI.
- Custom HUD rendering is straightforward with `ImDrawList`.
- Device lost/reset handling is already implemented around ImGui's backend lifecycle.
- UI code can call directly into C++ state without a binding layer.
- Fast iteration for feature panels and debugging.

## Current Weaknesses

- Visual structure and logic are often mixed in C++ draw functions.
- Heavy use of ImGui types in headers increases coupling.
- `UIComponents` uses ImGui internals, increasing maintenance and migration cost.
- Styling is code-driven rather than document/stylesheet-driven.
- Complex polished layouts are harder to author than in a retained/document UI model.
- Custom draw-list effects are powerful but lock the project further into ImGui's rendering model.

## Replacement Considerations

Any full replacement of ImGui must account for at least these areas:

1. D3D9 renderer backend.
2. Win32/game input bridge.
3. Device lost/reset resource lifecycle.
4. Font and texture asset lifecycle.
5. Equivalent input-capture signals for keybind suppression.
6. Reimplementation of `UIComponents`.
7. Rewrite of all `Render()` feature panels.
8. Strategy for current `ImDrawList` custom rendering.
9. Shutdown ordering inside the injected process.
10. Visual regression testing in actual SA-MP runtime conditions.

## Specific RmlUi Notes

RmlUi may provide a cleaner long-term authoring model for menus and settings because it supports HTML/CSS-like documents, stylesheets, retained layout, and richer presentation separation.

However, for this project:

- RmlUi does not appear to provide an official Direct3D9 backend in the current upstream backend set.
- A custom D3D9 render interface would be required.
- Input events would need to be manually translated from the current Win32/game input path into RmlUi.
- Existing custom `ImDrawList` HUD elements need a new rendering strategy.
- The migration would be a UI architecture project, not a drop-in library swap.

## Recommended Review Questions

For outside reviewers, the useful questions are:

1. Is the long-term benefit of a retained/document UI worth a full D3D9 backend and input bridge?
2. Should only the main menu/settings UI move to a retained UI system while HUD/custom rendering stays separate?
3. Would it be better to first decouple ImGui types behind project-local UI types?
4. Can `UIComponents` be rewritten into a cleaner design-system boundary while still using ImGui?
5. What is the safest migration strategy for an injected D3D9 plugin where stability is more important than visual architecture purity?
6. How should current draw-list features such as hitmarkers, quick map, speedometer, and toasts be represented in a retained UI model?

## Recommended Migration Strategy If Replacement Is Pursued

A safe migration should be staged:

1. Introduce project-local UI primitive types for vectors, colors, rectangles, IDs, and draw colors.
2. Remove ImGui types from first-party public headers where possible.
3. Wrap input-capture state behind a project-local interface.
4. Refactor `UIComponents` into a clearer design-system boundary while still backed by ImGui.
5. Build a minimal RmlUi proof of concept in a non-critical panel.
6. Prove D3D9 device lost/reset, alt-tab, shutdown, and input capture stability.
7. Migrate one document-like panel, such as an about/settings page.
8. Decide separately how to handle HUD/custom draw-list renderers.
9. Only consider full replacement after the prototype is stable in-game.

## Summary Verdict

The current ImGui implementation is deeply integrated but coherent for a D3D9 injected C++ plugin. It is well-suited for immediate-mode menus, fast iteration, and custom HUD drawing. Its biggest architectural drawback is that styling, layout, rendering, and state are strongly coupled through C++ and ImGui types.

A full replacement with RmlUi or a similar retained UI system could produce a cleaner and more designer-friendly UI architecture in the long run, but only if the project is willing to own a custom D3D9 backend, input bridge, and a broad rewrite of UI components and custom renderers. For near-term stability, the safest path is incremental decoupling and targeted prototyping rather than a full replacement.
