# Theme UI QA Scenarios

Runtime QA scenarios define the expected behavior when the plugin is loaded
inside SA-MP 0.3.DL. Each scenario maps to a `-Scenario` value in both
`launch_samp_03dl.ps1` and `premium_ui_qa.ps1`.

## Persistence Root

Runtime QA deploys and verifies live config data under:

`GTA San Andreas User Files\NLBinder\keybinder.json`

Do not use the legacy `GTA San Andreas User Files\keybinder.json`
root for fixture deployment or persistence verification.

## overlay-toggle

**Trigger:** F2 key press (VK_F2 = 113)  
**Expected:**
1. First F2 press → overlay becomes visible, cursor shown, game mouse suppressed
2. Second F2 press → overlay hidden, cursor hidden, game mouse restored
3. ESC while overlay visible → overlay hidden (unless key capture active)

**Evidence:** Plugin log should show `Toggle()` calls. No crash on rapid toggling.

## screen-navigate

**Trigger:** Click each sidebar category while overlay is open  
**Expected:**
1. Keybinds (default) → shows keybind list/editor split view
2. KillTracker → shows kill/death stats and weapon breakdown
3. Jobs → shows job overview/correction flows
4. Mods → shows Hitmarker, Hitsounds, Crosshair, Interface subtabs
5. Settings → shows General (including Tacho), Appearance, QuickMap, RSS Tracker subtabs
6. Help → shows usage instructions
7. Online → shows online overview/overlay settings when configured

**Evidence:** Each category renders without crash. NavCategory state transitions correctly.

## accent-customize

**Trigger:** Access Settings > Appearance tab, use custom color picker  
**Expected:**
1. Color picker allows selecting any custom accent color
2. Accent applies immediately to UI elements (nav, tabs, buttons)
3. Theme selector persists `theme_name` for the five built-in themes from Settings > Appearance
4. Preview shows live accent state
5. Reset button restores default accent
6. Changes survive overlay close/reopen cycle

**Evidence:** Settings persisted in the `NLBinder\keybinder.json` persistence root after modification.

## interface-editor-toggle

**Trigger:** Launch QA with the dedicated Interface Editor fixture, then open `Mods > Interface Editor`  
**Expected:**
1. The active saved Interface Editor preset applies on launch without manual prep
2. The active preset persists HUD styling metadata for HP / armor / money colors
3. The active preset also persists a custom zone text override and saved ZoneText position
4. Opening the overlay and switching to `Mods > Interface Editor` requires no ad-hoc steps
5. Evidence lands under `.sisyphus/evidence/` with deterministic `task-<n>-...` naming when deterministic mode is enabled

**Evidence:**
- `interface-editor-launch-state` screenshot/json proves the persisted preset was deployed and the active slot resolves to HUD color fields plus a non-empty `zone_text` and saved `zone_text_pos_x/y`
- `interface-editor-tab` screenshot/json proves the Interface Editor tab was opened through the real runtime navigation flow

## device-reset

**Trigger:** Alt-tab out of fullscreen GTA:SA and back  
**Expected:**
1. `OnDeviceLost()` called → ImGui device objects invalidated
2. `OnDeviceReset()` called → ImGui device objects recreated
3. Overlay state (visible/hidden) preserved across reset
4. No texture corruption or crash

**Evidence:** Plugin log shows device lost/reset sequence without errors.

## quickmap-toggle

**Trigger:** Press configured QuickMap hotkey (default M) in toggle mode  
**Expected:**
1. First key press → QuickMap overlay becomes visible with embedded map texture
2. Second key press → QuickMap overlay hidden
3. Player marker, checkpoints, and gang zones render on the map when visible
4. Main overlay is hidden while QuickMap is visible (mutual exclusivity)
5. Settings persist across restart

**Evidence:** Plugin log shows QuickMap visibility state changes. Map texture renders without errors.

## quickmap-hold

**Trigger:** Hold configured QuickMap hotkey (default M) in hold mode  
**Expected:**
1. Key held down → QuickMap overlay visible
2. Key released → QuickMap overlay hidden
3. Continuous updates show player position moving on map
4. No stuck visibility when key is released

**Evidence:** Plugin log shows QuickMap opened on key-down and closed on key-up.

## quickmap-markers

**Trigger:** Open QuickMap while in-game with active checkpoints/map icons  
**Expected:**
1. Local player marker shows current position and heading
2. Active checkpoints render as circles on map
3. Active race checkpoints render as diamonds on map
4. Map icons from server render as squares
5. Gang zones render as translucent rectangles

**Evidence:** Screenshot shows all marker types positioned correctly on map.

## quickmap-device-reset

**Trigger:** Alt-tab while QuickMap is visible  
**Expected:**
1. QuickMap survives device lost/reset cycle
2. Map texture reloads correctly after reset
3. QuickMap visibility state preserved
4. No crash or texture corruption

**Evidence:** Plugin log shows QuickMap handled device reset without errors.

## input-label-layout

**Trigger:** Open overlay and navigate to each of the six input-label regression surfaces  
**Expected:**

1. **Hitmarker Eigenschaften card** (Settings > Hitmarker):
   - Größe input has visible label with proper alignment
   - Deckkraft input has visible label with proper alignment
   - Anzeigedauer input has visible label with proper alignment

2. **Hitmarker Preset-Name input** (Settings > Hitmarker):
   - Preset name input field has visible label
   - Label aligns correctly with input control

3. **Keybind Editor Name field** (Keybinds tab > Add/Edit):
   - Name field receives auto-focus when editor opens
   - Label is visible and properly aligned above input

4. **Keybind Editor Slash Command label** (Keybinds tab > Add/Edit > Slash Command):
   - Label "Befehl" visible above input (hidden ID suffix "##SlashCommand" not shown)
   - Label text does not overlap with input field

5. **Keybind List Search Row** (Keybinds tab):
   - Search input stays on same line as "+" button (hidden label "##Search" adds no visible text)
   - No extra gap or line break introduced

6. **Settings GUI öffnen inputs** (Settings > Steuerung):
   - Befehl 1 input has visible label and tooltip on hover
   - Befehl 2 input has visible label and tooltip on hover
   - Save behavior persists values across overlay close/reopen

**Evidence:** Screenshot of each regression surface showing visible, properly aligned labels. Settings persist in the `NLBinder\keybinder.json` persistence root after modification.
