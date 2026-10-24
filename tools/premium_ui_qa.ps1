<#
.SYNOPSIS
    Theme UI QA automation harness for NLBinder.

.DESCRIPTION
    Executes deterministic QA scenarios for the themed Binder UI. Supports two
    modes:

      1) Static mode (default) - No game environment required. Validates build,
         source contracts, config fixtures, and evidence structure. Agents can
         run this in any CI-like context.

      2) Runtime mode (-Runtime) - Requires SA-MP environment variables.
         Builds, deploys the plugin, launches SA-MP, and uses config fixtures +
         input simulation to exercise overlay toggle, screen navigation, and
         theme/accent switching. Captures structured evidence.

    All evidence is written to .sisyphus/evidence/ using the naming convention:
        task-{TaskId}-{ScenarioSlug}.{ext}

.PARAMETER Scenario
    Which QA scenario to run. Valid values:
        all               - Run all scenarios (default)
        build-verify      - Build the plugin and verify binary output
        config-roundtrip  - Validate settings load/save roundtrip with fixture
        overlay-toggle    - Toggle overlay via F2 simulation (runtime only)
        screen-navigate   - Navigate all sidebar categories (runtime only)
        accent-customize  - Customize accent color via picker (runtime only)
        interface-editor-toggle - Verify persisted Interface Editor preset + live utility action (runtime only)
        device-reset      - Simulate D3D device lost/reset (runtime only)
        full-runtime      - All runtime scenarios sequentially

.PARAMETER TaskId
    Task identifier for evidence naming. Defaults to "T5b".

.PARAMETER Runtime
    Enable runtime mode (requires SA-MP environment).

.PARAMETER NoBuild
    Skip the MSBuild step.

.PARAMETER FixtureDir
    Path to config fixtures. Defaults to tools\qa\fixtures.

.EXAMPLE
    .\premium_ui_qa.ps1
    .\premium_ui_qa.ps1 -Scenario build-verify
    .\premium_ui_qa.ps1 -Scenario full-runtime -Runtime
    .\premium_ui_qa.ps1 -Scenario config-roundtrip -TaskId T8

.NOTES
    Static scenarios can run without any game environment.
    Runtime scenarios need the same env vars as launch_samp_03dl.ps1.
#>

[CmdletBinding()]
param(
    [ValidateSet("all", "build-verify", "config-roundtrip", "source-contracts",
                 "unit-tests", "overlay-toggle", "screen-navigate", "accent-customize",
                 "interface-editor-toggle", "device-reset", "input-label-layout",
                 "quickmap-toggle", "quickmap-hold", "quickmap-markers", "quickmap-device-reset", "full-runtime")]
    [string]$Scenario = "all",
    [string]$TaskId = "T5b",
    [switch]$Runtime,
    [switch]$NoBuild,
    [string]$FixtureDir = ""
)

$ErrorActionPreference = "Stop"

# ── Paths ────────────────────────────────────────────────────────
$ScriptRoot  = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptRoot
$EvidenceDir = Join-Path $ProjectRoot ".sisyphus\evidence"
$BuildDir    = Join-Path $ProjectRoot "build\Release"
$PluginBinary = Join-Path $BuildDir "Keybinder.asi"
$TestExe     = Join-Path $ProjectRoot "build\KeybinderTests.exe"
$NlbinderRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "GTA San Andreas User Files\NLBinder"
$LiveConfigDir = $NlbinderRoot
$LiveConfigFile = Join-Path $NlbinderRoot "keybinder.json"

if ([string]::IsNullOrWhiteSpace($FixtureDir)) {
    $FixtureDir = Join-Path $ProjectRoot "tools\qa\fixtures"
}

# ── Ensure evidence directory ────────────────────────────────────
if (-not (Test-Path $EvidenceDir)) {
    New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null
}

# ── Ensure fixtures directory ────────────────────────────────────
if (-not (Test-Path $FixtureDir)) {
    New-Item -ItemType Directory -Path $FixtureDir -Force | Out-Null
}

# ── Helpers ──────────────────────────────────────────────────────

function Write-Evidence {
    <#
    .SYNOPSIS
        Write structured evidence to .sisyphus/evidence/.
    .PARAMETER Slug
        Scenario slug (e.g. "build-verify", "overlay-toggle").
    .PARAMETER Content
        Text content to write.
    .PARAMETER Extension
        File extension (default: "log").
    #>
    param(
        [Parameter(Mandatory)] [string]$Slug,
        [Parameter(Mandatory)] [string]$Content,
        [string]$Extension = "log"
    )
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $filename  = "task-${TaskId}-${Slug}-${timestamp}.${Extension}"
    $filepath  = Join-Path $EvidenceDir $filename
    $Content | Out-File -FilePath $filepath -Encoding utf8 -Force
    Write-Host "[evidence] $filepath"
}

function Write-EvidenceSummary {
    <#
    .SYNOPSIS
        Write a structured QA summary for this run.
    #>
    param(
        [Parameter(Mandatory)] [hashtable[]]$Results
    )

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $passed  = @($Results | Where-Object { $_.Status -eq "PASS" }).Count
    $failed  = @($Results | Where-Object { $_.Status -eq "FAIL" }).Count
    $skipped = @($Results | Where-Object { $_.Status -eq "SKIP" }).Count
    $total   = @($Results).Count

    $lines = @()
    $lines += "=== Theme UI QA Summary ==="
    $lines += "Timestamp:  $timestamp"
    $lines += "TaskId:     $TaskId"
    $lines += "Scenario:   $Scenario"
    $lines += "Runtime:    $($Runtime.IsPresent)"
    $lines += "Results:    $passed/$total passed, $failed failed, $skipped skipped"
    $lines += ""
    $lines += "--- Scenario Results ---"

    foreach ($r in $Results) {
        $icon = switch ($r.Status) {
            "PASS" { "[OK]" }
            "FAIL" { "[!!]" }
            "SKIP" { "[--]" }
            default { "[??]" }
        }
        $lines += "  $icon $($r.Name)"
        if ($r.Detail) {
            $lines += "      $($r.Detail)"
        }
    }

    $lines += ""
    $lines += "--- Evidence Files ---"
    $evidenceFiles = Get-ChildItem -Path $EvidenceDir -Filter "task-${TaskId}-*" -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 20
    foreach ($f in $evidenceFiles) {
        $lines += "  $($f.Name)"
    }

    $summary = $lines -join "`n"
    Write-Evidence -Slug "summary" -Content $summary -Extension "txt"
    Write-Host ""
    Write-Host $summary
    return ($failed -eq 0)
}

function Test-RuntimeEnv {
    <#
    .SYNOPSIS
        Check if SA-MP runtime environment variables are set.
    #>
    $vars = @("GTA_SA_DIR", "SAMP_03DL_DIR", "SAMP_QA_HOST", "SAMP_QA_PORT", "SAMP_QA_NICK")
    $missing = @()
    foreach ($v in $vars) {
        if ([string]::IsNullOrWhiteSpace([System.Environment]::GetEnvironmentVariable($v))) {
            $missing += $v
        }
    }
    return $missing
}

function Get-LauncherTaskNumber {
    param(
        [Parameter(Mandatory)] [string]$Value
    )

    $match = [regex]::Match($Value, "\d+")
    if ($match.Success) {
        return [int]$match.Value
    }

    return 2
}

# ── Scenario: build-verify ───────────────────────────────────────

function Invoke-BuildVerify {
    Write-Host "`n[scenario] build-verify"
    $result = @{ Name = "build-verify"; Status = "FAIL"; Detail = "" }

    if ($NoBuild -and (Test-Path $PluginBinary)) {
        $result.Status = "PASS"
        $result.Detail = "Skipped build; existing binary found: $PluginBinary"
        $info = Get-Item $PluginBinary
        $content = @(
            "Build: SKIPPED (NoBuild flag)",
            "Binary: $PluginBinary",
            "Size: $($info.Length) bytes",
            "Modified: $($info.LastWriteTime)"
        ) -join "`n"
        Write-Evidence -Slug "build-verify" -Content $content
        return $result
    }

    if ($NoBuild) {
        $result.Status = "FAIL"
        $result.Detail = "NoBuild set but no binary at $PluginBinary"
        Write-Evidence -Slug "build-verify" -Content "FAIL: NoBuild but no binary"
        return $result
    }

    try {
        Write-Host "[*] Building Keybinder (Release|Win32)..."
        $buildOutput = & MSBuild.exe "$ProjectRoot\Keybinder.vcxproj" `
            /p:Configuration=Release /p:Platform=Win32 `
            /verbosity:minimal /nologo 2>&1 | Out-String

        if ($LASTEXITCODE -ne 0) {
            $result.Detail = "MSBuild failed with exit code $LASTEXITCODE"
            Write-Evidence -Slug "build-verify" -Content "FAIL: $($result.Detail)`n`n$buildOutput"
            return $result
        }

        if (-not (Test-Path $PluginBinary)) {
            $result.Detail = "Build succeeded but binary not found at $PluginBinary"
            Write-Evidence -Slug "build-verify" -Content "FAIL: $($result.Detail)`n`n$buildOutput"
            return $result
        }

        $info = Get-Item $PluginBinary
        $result.Status = "PASS"
        $result.Detail = "Binary: $($info.Length) bytes"
        $content = @(
            "Build: PASS",
            "Binary: $PluginBinary",
            "Size: $($info.Length) bytes",
            "Modified: $($info.LastWriteTime)",
            "",
            "--- Build Output ---",
            $buildOutput
        ) -join "`n"
        Write-Evidence -Slug "build-verify" -Content $content
    }
    catch {
        $result.Detail = "Exception: $($_.Exception.Message)"
        Write-Evidence -Slug "build-verify" -Content "FAIL: $($result.Detail)"
    }

    return $result
}

# ── Scenario: config-roundtrip ───────────────────────────────────

function Invoke-ConfigRoundtrip {
    Write-Host "`n[scenario] config-roundtrip"
    $result = @{ Name = "config-roundtrip"; Status = "FAIL"; Detail = "" }

    # Generate the generic QA fixture if none exists
    $fixture = Join-Path $FixtureDir "keybinder_qa.json"
    $interfaceFixture = Join-Path $FixtureDir "interface_editor_qa.json"
    if (-not (Test-Path $fixture)) {
        $fixtureContent = @"
{
    "settings": {
        "ui_open_command_primary": "kb",
        "ui_open_command_secondary": "keybinder",
        "ui_toggle_hotkey": 113,
        "theme_name": "Dark",
        "theme_custom_accent": "#7B2CBF",
        "kill_tracker_enabled": true,
        "auto_hudscalefix": true,
        "hitmarker_enabled": true,
        "hitmarker_size": 25.0,
        "hitmarker_color_r": 0.9,
        "hitmarker_color_g": 0.9,
        "hitmarker_color_b": 0.1,
        "crosshair_enabled": false
    },
    "keybinds": [
        {
            "name": "QA Test Bind",
            "key": 75,
            "actions": [
                { "type": "chat", "value": "/qa test passed" }
            ]
        }
    ]
}
"@
        $fixtureContent | Out-File -FilePath $fixture -Encoding utf8 -Force
        Write-Host "[+] Created QA fixture: $fixture"
    }

    try {
        $json = Get-Content -Path $fixture -Raw | ConvertFrom-Json
        $checks = @()

        # Check required settings keys
        $requiredKeys = @("ui_toggle_hotkey", "theme_name", "theme_custom_accent")
        foreach ($key in $requiredKeys) {
            $settingProperty = $json.settings.PSObject.Properties[$key]
            if ($null -ne $settingProperty) {
                $checks += "  [OK] settings.$key = $($settingProperty.Value)"
            } else {
                $checks += "  [!!] settings.$key MISSING"
                $result.Detail = "Missing key: settings.$key"
                $content = ($checks -join "`n")
                Write-Evidence -Slug "config-roundtrip" -Content "FAIL`n$content"
                return $result
            }
        }

        # Check keybinds array
        if ($json.keybinds -and $json.keybinds.Count -gt 0) {
            $checks += "  [OK] keybinds: $($json.keybinds.Count) entries"
        } else {
            $checks += "  [!!] keybinds array empty or missing"
        }

        # Validate hotkey is VK_F2 (113)
        if ($json.settings.ui_toggle_hotkey -eq 113) {
            $checks += "  [OK] ui_toggle_hotkey is VK_F2 (113)"
        } else {
            $checks += "  [WARN] ui_toggle_hotkey is $($json.settings.ui_toggle_hotkey), expected 113 (VK_F2)"
        }

        $allowedThemeNames = @("Dark", "Nova", "Ember", "Ocean", "Forest")
        if ($json.settings.theme_name -in $allowedThemeNames) {
            $checks += "  [OK] theme_name is deterministic built-in theme '$($json.settings.theme_name)'"
        } else {
            $checks += "  [!!] settings.theme_name '$($json.settings.theme_name)' is not a supported built-in theme"
            $result.Detail = "Invalid settings.theme_name"
            $content = ($checks -join "`n")
            Write-Evidence -Slug "config-roundtrip" -Content "FAIL`n$content"
            return $result
        }

        if ((Split-Path -Parent $LiveConfigFile) -eq $NlbinderRoot -and (Split-Path -Leaf $LiveConfigFile) -eq "keybinder.json") {
            $checks += "  [OK] Live config target resolves to NLBinder\\keybinder.json"
        } else {
            $checks += "  [!!] Live config target is not NLBinder\\keybinder.json"
            $result.Detail = "Live config target does not resolve to NLBinder root"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Expected Live Root: $NlbinderRoot",
                "Expected Live Config: $LiveConfigFile",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        if (-not (Test-Path $interfaceFixture)) {
            $result.Detail = "Missing fixture: $interfaceFixture"
            $checks += "  [!!] interface_editor_qa.json missing"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        $interfaceJson = Get-Content -Path $interfaceFixture -Raw | ConvertFrom-Json
        $interfaceSettings = $interfaceJson.settings
        $checks += ""
        $checks += "Interface Editor fixture: $interfaceFixture"

        if ($null -ne $interfaceSettings.interface_editor_active_preset_index) {
            $checks += "  [OK] settings.interface_editor_active_preset_index = $($interfaceSettings.interface_editor_active_preset_index)"
        } else {
            $checks += "  [!!] settings.interface_editor_active_preset_index MISSING"
            $result.Detail = "Missing key: settings.interface_editor_active_preset_index"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        $savedPresets = @($interfaceSettings.interface_editor_saved_presets)
        if ($savedPresets.Count -gt 0) {
            $checks += "  [OK] settings.interface_editor_saved_presets has $($savedPresets.Count) slot(s)"
        } else {
            $checks += "  [!!] settings.interface_editor_saved_presets empty or missing"
            $result.Detail = "Missing or empty: settings.interface_editor_saved_presets"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        $activePresetIndex = [int]$interfaceSettings.interface_editor_active_preset_index
        if ($activePresetIndex -ge 0 -and $activePresetIndex -lt $savedPresets.Count) {
            $checks += "  [OK] Active preset index $activePresetIndex resolves within saved preset slots"
        } else {
            $checks += "  [!!] Active preset index $activePresetIndex is outside saved preset range"
            $result.Detail = "Invalid interface editor active preset index"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        $activePreset = $savedPresets[$activePresetIndex]
        if ($null -ne $activePreset.hp_color_r -and $null -ne $activePreset.armor_color_r -and $null -ne $activePreset.money_color_r) {
            $checks += "  [OK] Active interface editor preset defines HP, armor, and money colors"
        } else {
            $checks += "  [!!] Active interface editor preset missing HUD color fields"
            $result.Detail = "Interface editor preset missing HUD color fields"
            $content = @(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"
            Write-Evidence -Slug "config-roundtrip" -Content $content
            return $result
        }

        if (-not [string]::IsNullOrWhiteSpace([string]$activePreset.name)) {
            $checks += "  [OK] Active interface editor preset name = $($activePreset.name)"
        } else {
            $checks += "  [WARN] Active interface editor preset is unnamed"
        }

        if ($interfaceSettings.theme_name -in $allowedThemeNames) {
            $checks += "  [OK] interface fixture theme_name = $($interfaceSettings.theme_name)"
        } else {
            $checks += "  [!!] interface fixture theme_name missing or invalid"
            $result.Detail = "Invalid interface fixture theme_name"
            Write-Evidence -Slug "config-roundtrip" -Content ((@(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"))
            return $result
        }

        if ($activePreset.override_zone_text -eq $true -and -not [string]::IsNullOrWhiteSpace([string]$activePreset.zone_text)) {
            $checks += "  [OK] Active interface editor preset overrides zone text with '$($activePreset.zone_text)'"
        } else {
            $checks += "  [!!] Active interface editor preset missing zone text override"
            $result.Detail = "Interface editor preset missing override_zone_text/zone_text"
            Write-Evidence -Slug "config-roundtrip" -Content ((@(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"))
            return $result
        }

        if ($null -ne $activePreset.zone_text_pos_x -and $null -ne $activePreset.zone_text_pos_y) {
            $checks += "  [OK] Active interface editor preset defines zone text position = ($($activePreset.zone_text_pos_x), $($activePreset.zone_text_pos_y))"
        } else {
            $checks += "  [!!] Active interface editor preset missing zone text position"
            $result.Detail = "Interface editor preset missing zone_text_pos_x / zone_text_pos_y"
            Write-Evidence -Slug "config-roundtrip" -Content ((@(
                "Config Roundtrip: FAIL",
                "Fixture: $fixture",
                "Interface Fixture: $interfaceFixture",
                "",
                "--- Checks ---",
                ($checks -join "`n")
            ) -join "`n"))
            return $result
        }

        $themeFixtureSpecs = @(
            @{ File = "nova-theme.json"; ThemeName = "Nova"; Accent = "" },
            @{ File = "dark-theme.json"; ThemeName = "Dark"; Accent = "" }
        )

        foreach ($spec in $themeFixtureSpecs) {
            $themeFixture = Join-Path $FixtureDir $spec.File
            if (-not (Test-Path $themeFixture)) {
                $checks += "  [!!] Missing deterministic theme fixture: $($spec.File)"
                $result.Detail = "Missing deterministic theme fixture: $($spec.File)"
                Write-Evidence -Slug "config-roundtrip" -Content ((@(
                    "Config Roundtrip: FAIL",
                    "Fixture: $fixture",
                    "Interface Fixture: $interfaceFixture",
                    "Theme Fixture: $themeFixture",
                    "",
                    "--- Checks ---",
                    ($checks -join "`n")
                ) -join "`n"))
                return $result
            }

            $themeJson = Get-Content -Path $themeFixture -Raw | ConvertFrom-Json
            if ($themeJson.settings.theme_name -ne $spec.ThemeName) {
                $checks += "  [!!] $($spec.File) theme_name=$($themeJson.settings.theme_name), expected $($spec.ThemeName)"
                $result.Detail = "Invalid theme_name in $($spec.File)"
                Write-Evidence -Slug "config-roundtrip" -Content ((@(
                    "Config Roundtrip: FAIL",
                    "Theme Fixture: $themeFixture",
                    "",
                    "--- Checks ---",
                    ($checks -join "`n")
                ) -join "`n"))
                return $result
            }

            if ($themeJson.settings.theme_custom_accent -ne $spec.Accent) {
                $checks += "  [!!] $($spec.File) theme_custom_accent=$($themeJson.settings.theme_custom_accent), expected $($spec.Accent)"
                $result.Detail = "Invalid theme_custom_accent in $($spec.File)"
                Write-Evidence -Slug "config-roundtrip" -Content ((@(
                    "Config Roundtrip: FAIL",
                    "Theme Fixture: $themeFixture",
                    "",
                    "--- Checks ---",
                    ($checks -join "`n")
                ) -join "`n"))
                return $result
            }

            if ($null -ne $themeJson.settings.ui_toggle_hotkey -and $themeJson.keybinds) {
                $checks += "  [OK] $($spec.File) pins $($spec.ThemeName) + $($spec.Accent) with runtime hotkey/keybind data"
            } else {
                $checks += "  [!!] $($spec.File) missing runtime hotkey or keybind fixture data"
                $result.Detail = "Incomplete deterministic theme fixture: $($spec.File)"
                Write-Evidence -Slug "config-roundtrip" -Content ((@(
                    "Config Roundtrip: FAIL",
                    "Theme Fixture: $themeFixture",
                    "",
                    "--- Checks ---",
                    ($checks -join "`n")
                ) -join "`n"))
                return $result
            }
        }

        $result.Status = "PASS"
        $result.Detail = "Fixtures valid: keybinder_qa + interface_editor_qa + Nova/Dark themes"
        $content = @(
            "Config Roundtrip: PASS",
            "Fixture: $fixture",
            "Interface Fixture: $interfaceFixture",
            "Expected Live Root: $NlbinderRoot",
            "Expected Live Config: $LiveConfigFile",
            "",
            "--- Checks ---",
            ($checks -join "`n")
        ) -join "`n"
        Write-Evidence -Slug "config-roundtrip" -Content $content
    }
    catch {
        $result.Detail = "JSON parse error: $($_.Exception.Message)"
        Write-Evidence -Slug "config-roundtrip" -Content "FAIL: $($result.Detail)"
    }

    return $result
}

# ── Scenario: source-contracts ───────────────────────────────────

function Invoke-SourceContracts {
    <#
    .SYNOPSIS
        Validate that first-party source files meet expected contracts for
        themed UI: navigation, settings fields, overlay lifecycle, and theme routing.
    #>
    Write-Host "`n[scenario] source-contracts"
    $result = @{ Name = "source-contracts"; Status = "FAIL"; Detail = "" }
    $checks = @()

    # Check legacy UI component naming does not reappear in first-party contracts.
    $legacyTerms = @("PremiumUIComponents", "PremiumUI")
    $legacyScanFiles = @()
    foreach ($dir in @("src", "tests", "tools")) {
        $fullDir = Join-Path $ProjectRoot $dir
        if (Test-Path $fullDir) {
            $legacyScanFiles += Get-ChildItem -Path $fullDir -Recurse -File -Include *.cpp,*.h,*.ps1,*.bat,*.md,*.json
        }
    }
    $includeDir = Join-Path $ProjectRoot "include"
    if (Test-Path $includeDir) {
        $legacyScanFiles += Get-ChildItem -Path $includeDir -File -Filter *.h
    }
    $vcxproj = Join-Path $ProjectRoot "Keybinder.vcxproj"
    if (Test-Path $vcxproj) {
        $legacyScanFiles += Get-Item $vcxproj
    }

    $legacyHits = @()
    $excludedSelfNames = @("SourceContractTests.cpp", "premium_ui_qa.ps1")
    foreach ($file in $legacyScanFiles) {
        if ($excludedSelfNames -contains $file.Name) { continue }
        $content = Get-Content -Path $file.FullName -Raw
        foreach ($term in $legacyTerms) {
            if ($content.Contains($term)) {
                $legacyHits += "$($file.FullName.Replace($ProjectRoot + '\', ''))::$term"
            }
        }
    }
    if ($legacyHits.Count -eq 0) {
        $checks += "  [OK] Legacy UI component naming absent from first-party contracts"
    } else {
        $checks += "  [!!] Legacy UI component naming found: $($legacyHits -join ', ')"
    }

    # Check NavCategory enum exists with expected members
    $mainWindowH = Join-Path $ProjectRoot "include\MainWindowUI.h"
    if (Test-Path $mainWindowH) {
        $content = Get-Content -Path $mainWindowH -Raw
        $categories = @("Keybinds", "KillTracker", "Jobs", "Mods", "Settings", "Help", "Online")
        $found = @()
        foreach ($cat in $categories) {
            if ($content -match $cat) {
                $found += $cat
            }
        }
        if ($found.Count -eq $categories.Count) {
            $checks += "  [OK] NavCategory enum: all $($categories.Count) categories present"
        } else {
            $missing = $categories | Where-Object { $_ -notin $found }
            $checks += "  [!!] NavCategory missing: $($missing -join ', ')"
        }
    } else {
        $checks += "  [!!] MainWindowUI.h not found"
    }

    # Check SettingsManager has key fields
    $settingsH = Join-Path $ProjectRoot "include\SettingsManager.h"
    if (Test-Path $settingsH) {
        $content = Get-Content -Path $settingsH -Raw
        $fields = @("ui_toggle_hotkey", "theme_name", "theme_custom_accent", "hitmarker_enabled", "crosshair_enabled", "interface_editor_active_preset_index", "interface_editor_saved_presets")
        $found = @()
        foreach ($f in $fields) {
            if ($content -match $f) {
                $found += $f
            }
        }
        if ($found.Count -eq $fields.Count) {
            $checks += "  [OK] Settings struct: all $($fields.Count) key fields present"
        } else {
            $missing = $fields | Where-Object { $_ -notin $found }
            $checks += "  [!!] Settings missing fields: $($missing -join ', ')"
        }
    } else {
        $checks += "  [!!] SettingsManager.h not found"
    }

    $themeFixtureContracts = @(
        @{ File = "nova-theme.json"; ThemeName = "Nova"; Accent = "" },
        @{ File = "dark-theme.json"; ThemeName = "Dark"; Accent = "" }
    )
    $themeFixtureFailures = @()
    foreach ($spec in $themeFixtureContracts) {
        $fixturePath = Join-Path $FixtureDir $spec.File
        if (-not (Test-Path $fixturePath)) {
            $themeFixtureFailures += "missing $($spec.File)"
            continue
        }

        try {
            $fixtureJson = Get-Content -Path $fixturePath -Raw | ConvertFrom-Json
            if ($fixtureJson.settings.theme_name -ne $spec.ThemeName) {
                $themeFixtureFailures += "$($spec.File) theme_name=$($fixtureJson.settings.theme_name)"
            }
            if ($fixtureJson.settings.theme_custom_accent -ne $spec.Accent) {
                $themeFixtureFailures += "$($spec.File) accent=$($fixtureJson.settings.theme_custom_accent)"
            }
        } catch {
            $themeFixtureFailures += "$($spec.File) parse failed: $($_.Exception.Message)"
        }
    }
    if ($themeFixtureFailures.Count -eq 0) {
        $checks += "  [OK] Deterministic Nova/Dark fixtures pin theme_name + default accent"
    } else {
        $checks += "  [!!] Theme fixture contract failed: $($themeFixtureFailures -join '; ')"
    }

    # Check Interface Editor contract is wired into headers + UI
    $interfaceEditorH = Join-Path $ProjectRoot "include\InterfaceEditor.h"
    if (Test-Path $interfaceEditorH) {
        $content = Get-Content -Path $interfaceEditorH -Raw
        $members = @("PRESET_SLOT_COUNT", "show_radar", "hp_color_r", "show_hp_number", "hp_number_pos_x", "RenderVitalStatsNumbers", "SetHpNumberMoveMode", "SetArmorNumberMoveMode", "Initialize", "Shutdown")
        $found = @()
        foreach ($member in $members) {
            if ($content -match $member) {
                $found += $member
            }
        }
        if ($found.Count -eq $members.Count) {
            $checks += "  [OK] InterfaceEditor contract: preset persistence + live utility methods present"
        } else {
            $missing = $members | Where-Object { $_ -notin $found }
            $checks += "  [!!] InterfaceEditor contract missing: $($missing -join ', ')"
        }
    } else {
        $checks += "  [!!] InterfaceEditor.h not found"
    }

    $interfaceEditorUiH = Join-Path $ProjectRoot "include\InterfaceEditorUI.h"
    if (Test-Path $interfaceEditorUiH) {
        $content = Get-Content -Path $interfaceEditorUiH -Raw
        $members = @("Render", "SetSettingsManager", "SetInterfaceEditor", "OnSettingsChanged")
        $found = @()
        foreach ($member in $members) {
            if ($content -match $member) {
                $found += $member
            }
        }
        if ($found.Count -eq $members.Count) {
            $checks += "  [OK] InterfaceEditorUI contract: settings/editor wiring present"
        } else {
            $missing = $members | Where-Object { $_ -notin $found }
            $checks += "  [!!] InterfaceEditorUI contract missing: $($missing -join ', ')"
        }
    } else {
        $checks += "  [!!] InterfaceEditorUI.h not found"
    }

    # Check ImGuiOverlay has Show/Hide/Toggle
    $overlayH = Join-Path $ProjectRoot "include\ImGuiOverlay.h"
    if (Test-Path $overlayH) {
        $content = Get-Content -Path $overlayH -Raw
        $methods = @("Show\(\)", "Hide\(\)", "Toggle\(\)", "OnDeviceLost\(\)", "OnDeviceReset\(\)")
        $found = @()
        foreach ($m in $methods) {
            if ($content -match $m) {
                $found += $m
            }
        }
        if ($found.Count -eq $methods.Count) {
            $checks += "  [OK] ImGuiOverlay: all $($methods.Count) lifecycle methods present"
        } else {
            $missing = $methods | Where-Object { $_ -notin $found }
            $checks += "  [!!] ImGuiOverlay missing: $($missing -join ', ')"
        }
    } else {
        $checks += "  [!!] ImGuiOverlay.h not found"
    }

    # Check first-party source files exist
    $requiredSources = @(
        "src\Main.cpp", "src\MainWindowUI.cpp", "src\ImGuiOverlay.cpp",
        "src\KeybindManager.cpp", "src\SettingsManager.cpp",
        "src\KeybindEditorUI.cpp", "src\KeybindListUI.cpp",
        "src\ActionExecutor.cpp", "src\InputManager.cpp",
        "src\ThemeManager.cpp", "src\UIComponents.cpp"
    )
    $missingSources = @()
    foreach ($s in $requiredSources) {
        $path = Join-Path $ProjectRoot $s
        if (-not (Test-Path $path)) {
            $missingSources += $s
        }
    }
    if ($missingSources.Count -eq 0) {
        $checks += "  [OK] All $($requiredSources.Count) required source files present"
    } else {
        $checks += "  [!!] Missing sources: $($missingSources -join ', ')"
    }

    $uiComponentSurface = @(
        @{ Path = "include\UIComponents.h"; ShouldExist = $true },
        @{ Path = "src\UIComponents.cpp"; ShouldExist = $true },
        @{ Path = "include\PremiumUIComponents.h"; ShouldExist = $false },
        @{ Path = "src\PremiumUIComponents.cpp"; ShouldExist = $false }
    )
    $uiSurfaceFailures = @()
    foreach ($entry in $uiComponentSurface) {
        $path = Join-Path $ProjectRoot $entry.Path
        $exists = Test-Path $path
        if ($entry.ShouldExist -and -not $exists) {
            $uiSurfaceFailures += "missing $($entry.Path)"
        }
        if (-not $entry.ShouldExist -and $exists) {
            $uiSurfaceFailures += "legacy file still exists $($entry.Path)"
        }
    }
    $projectContent = if (Test-Path $vcxproj) { Get-Content -Path $vcxproj -Raw } else { "" }
    $runTestsBat = Join-Path $ProjectRoot "tools\run_tests.bat"
    $runTestsContent = if (Test-Path $runTestsBat) { Get-Content -Path $runTestsBat -Raw } else { "" }
    if ($projectContent -notmatch "UIComponents\.cpp" -or $runTestsContent -notmatch "UIComponentsTests\.cpp") {
        $uiSurfaceFailures += "UIComponents not wired through project/test harness"
    }
    if ($uiSurfaceFailures.Count -eq 0) {
        $checks += "  [OK] UIComponents rename contract: new files present, old files absent, build/test wiring present"
    } else {
        $checks += "  [!!] UIComponents rename contract failed: $($uiSurfaceFailures -join '; ')"
    }

    $interfaceEditorSurfaceFiles = @(
        "src\InterfaceEditor.cpp", "src\InterfaceEditorUI.cpp",
        "include\InterfaceEditor.h", "include\InterfaceEditorUI.h",
        "tests\InterfaceEditorTests.cpp"
    )
    $missingInterfaceEditorFiles = @()
    foreach ($f in $interfaceEditorSurfaceFiles) {
        $path = Join-Path $ProjectRoot $f
        if (-not (Test-Path $path)) {
            $missingInterfaceEditorFiles += $f
        }
    }
    if ($missingInterfaceEditorFiles.Count -eq 0) {
        $checks += "  [OK] Interface Editor source surface present (headers, runtime, tests)"
    } else {
        $checks += "  [!!] Interface Editor files missing: $($missingInterfaceEditorFiles -join ', ')"
    }

    $mainWindowCpp = Join-Path $ProjectRoot "src\MainWindowUI.cpp"
    if (Test-Path $mainWindowCpp) {
        $mwContent = Get-Content -Path $mainWindowCpp -Raw
        if (($mwContent -match 'SubTabItem\("Interface"' -or $mwContent -match 'SubTabItem\("Interface Editor"') -and $mwContent -match 'RenderModsView') {
            $checks += "  [OK] MainWindowUI routes Mods > Interface Editor tab"
        } else {
            $checks += "  [!!] MainWindowUI missing Interface Editor tab wiring"
        }
    }

    if ((Test-Path $mainWindowCpp) -and (Test-Path $mainWindowH)) {
        $mwHeaderContent = Get-Content -Path $mainWindowH -Raw
        $mwContent = Get-Content -Path $mainWindowCpp -Raw
        $hasAppearanceEnum = $mwHeaderContent -match "Appearance\s*=\s*1" -and $mwHeaderContent -match "RenderSettingsAppearanceTab"
        $hasAppearanceSubtab = $mwContent -match 'SubTabItem\("Appearance"' -and $mwContent -match "SettingsSubtab::Appearance"
        $hasThemeSelector = $mwContent -match 'UIComponents::Combo\("Theme"' -and $mwContent -match "settings\.theme_name\s*=\s*kThemeNames\[selectedThemeIndex\]"
        $usesCanonicalApply = $mwContent -match "ApplyCurrentTheme\(\)" -and $mwContent -match "ApplyConfiguredTheme\(settingsManager_, GetLayoutScale\(\)\)"
        if ($hasAppearanceEnum -and $hasAppearanceSubtab -and $hasThemeSelector -and $usesCanonicalApply) {
            $checks += "  [OK] Settings > Appearance selector contract: subtab, theme combo, persistence, canonical apply path present"
        } else {
            $checks += "  [!!] Settings > Appearance selector contract failed: enum=$hasAppearanceEnum subtab=$hasAppearanceSubtab selector=$hasThemeSelector canonical=$usesCanonicalApply"
        }
    }

    # Check purple accent token contract (#7B2CBF present in ThemeManager source)
    $themeManagerCpp = Join-Path $ProjectRoot "src\ThemeManager.cpp"
    if (Test-Path $themeManagerCpp) {
        $themeContent = Get-Content -Path $themeManagerCpp -Raw
        if ($themeContent -match "123, 44, 191" -and $themeContent -match "7B2CBF") {
            $checks += "  [OK] Purple token contract: #7B2CBF (123,44,191) present in ThemeManager.cpp"
        } else {
            $checks += "  [!!] Purple token contract: #7B2CBF not found in ThemeManager.cpp"
        }

        $themeManagerH = Join-Path $ProjectRoot "include\ThemeManager.h"
        $themeHeaderContent = if (Test-Path $themeManagerH) { Get-Content -Path $themeManagerH -Raw } else { "" }
        $mainCppContent = Get-Content -Path (Join-Path $ProjectRoot "src\Main.cpp") -Raw
        $mainWindowContent = Get-Content -Path (Join-Path $ProjectRoot "src\MainWindowUI.cpp") -Raw
        $overlayContent = Get-Content -Path (Join-Path $ProjectRoot "src\ImGuiOverlay.cpp") -Raw
        $combinedThemeCallers = @($mainCppContent, $mainWindowContent, $overlayContent) -join "`n"

        $hasCanonicalHelper = $themeHeaderContent -match "ApplyConfiguredTheme" -and $themeContent -match "ApplyConfiguredTheme"
        $canonicalPipeline = $themeContent -match "settings\.theme_name" -and
            $themeContent -match "SetTheme\(" -and
            $themeContent -match "ApplyCustomAccent\(" -and
            $themeContent -match "ApplyImGuiStyle\(scale\)"
        $callerUsesCanonical = $overlayContent -match "ApplyConfiguredTheme\(settings_manager_, current_scale_\)" -and
            $mainWindowContent -match "ApplyConfiguredTheme\(settingsManager_, GetLayoutScale\(\)\)" -and
            $mainCppContent -notmatch "ApplyConfiguredAppearanceStyle" -and
            $mainCppContent -notmatch "ApplyCustomAccent\(" -and
            $mainCppContent -notmatch "ApplyImGuiStyle\("
        $duplicateAccentCalls = [regex]::Matches($combinedThemeCallers, "ApplyCustomAccent\(").Count
        $duplicateStyleCalls = [regex]::Matches($combinedThemeCallers, "ApplyImGuiStyle\(").Count

        if ($hasCanonicalHelper -and $canonicalPipeline -and $callerUsesCanonical -and $duplicateAccentCalls -eq 0 -and $duplicateStyleCalls -eq 0) {
            $checks += "  [OK] Theme application contract: single ThemeManager::ApplyConfiguredTheme pipeline used by overlay/MainWindow/Main"
        } else {
            $checks += "  [!!] Theme application contract failed: canonical=$hasCanonicalHelper pipeline=$canonicalPipeline callers=$callerUsesCanonical duplicateAccentCalls=$duplicateAccentCalls duplicateStyleCalls=$duplicateStyleCalls"
        }
    } else {
        $checks += "  [!!] ThemeManager.cpp not found for purple token check"
    }

    # Check no raw Nova hex values outside ThemeManager.cpp in first-party UI source
    $allowedFiles = @("ThemeManager.cpp", "ThemeManagerTests.cpp")
    $novaHexPatterns = @("0x7C3AED", "0x090A12", "0x161724", "0x1E1E2E", "0x26263A")
    $novaDecimalPatterns = @("124, 58, 237", "9, 10, 18", "22, 23, 36", "30, 30, 46", "38, 38, 58")
    $srcDir = Join-Path $ProjectRoot "src"
    $rawHexViolations = @()
    if (Test-Path $srcDir) {
        $uiSrcFiles = Get-ChildItem -Path $srcDir -Filter "*.cpp" -File | Where-Object {
            $isAllowed = $false
            foreach ($af in $allowedFiles) {
                if ($_.Name -eq $af) { $isAllowed = $true; break }
            }
            -not $isAllowed
        }
        foreach ($sf in $uiSrcFiles) {
            $content = Get-Content -Path $sf.FullName -Raw
            # Check for ImGui-style ImVec4() with Nova-palette RGB floats (distinctive near-zero values)
            # Pattern: ImVec4(0.0Xf, 0.0Xf, 0.0Xf ...) that matches Nova dark surfaces
            if ($content -match "ImVec4\(0\.03f,\s*0\.04f,\s*0\.06f" -or
                $content -match "ImVec4\(0\.05f,\s*0\.07f,\s*0\.09f" -or
                $content -match "ImVec4\(0\.01f,\s*0\.02f,\s*0\.025f" -or
                $content -match "ImVec4\(0\.08f,\s*0\.08f,\s*0\.12f" -or
                $content -match "ImVec4\(0\.06f,\s*0\.06f,\s*0\.10f" -or
                $content -match "ImVec4\(0\.92f,\s*0\.20f,\s*0\.18f" -or
                $content -match "ImVec4\(0\.72f,\s*0\.93f,\s*0\.17f" -or
                $content -match "ImVec4\(0\.95f,\s*0\.73f,\s*0\.20f" -or
                $content -match "ImVec4\(0\.24f,\s*0\.78f,\s*0\.39f" -or
                $content -match "ImVec4\(0\.48f,\s*0\.52f,\s*0\.56f") {
                $rawHexViolations += $sf.Name
            }
        }
    }
    if ($rawHexViolations.Count -eq 0) {
        $checks += "  [OK] No raw Nova palette hex values in first-party UI source (outside ThemeManager.cpp)"
    } else {
        $checks += "  [!!] Raw Nova palette hex values found in: $($rawHexViolations -join ', ')"
    }

    # Check no D3DX dependencies in first-party source
    $srcDir = Join-Path $ProjectRoot "src"
    $d3dxHits = @()
    if (Test-Path $srcDir) {
        $srcFiles = Get-ChildItem -Path $srcDir -Filter "*.cpp" -File
        foreach ($sf in $srcFiles) {
            $content = Get-Content -Path $sf.FullName -Raw
            if ($content -match "D3DX") {
                $d3dxHits += $sf.Name
            }
        }
    }
    if ($d3dxHits.Count -eq 0) {
        $checks += "  [OK] No D3DX dependencies in first-party source"
    } else {
        $checks += "  [!!] D3DX references found in: $($d3dxHits -join ', ')"
    }

    # Check no old close-button literals (0.32f, 0.10f, 0.16f) in MainWindowUI
    $mainWindowCpp = Join-Path $ProjectRoot "src\MainWindowUI.cpp"
    if (Test-Path $mainWindowCpp) {
        $mwContent = Get-Content -Path $mainWindowCpp -Raw
        $oldLiterals = @("0\.32f", "0\.10f", "0\.16f")
        $literalHits = @()
        foreach ($lit in $oldLiterals) {
            if ($mwContent -match $lit) {
                $literalHits += $lit
            }
        }
        if ($literalHits.Count -eq 0) {
            $checks += "  [OK] No old close-button literals (0.32f, 0.10f, 0.16f) in MainWindowUI.cpp"
        } else {
            $checks += "  [!!] Old close-button literals found: $($literalHits -join ', ')"
        }
    } else {
        $checks += "  [!!] MainWindowUI.cpp not found for close-button literal check"
    }

    # Check required test files present
    $requiredTests = @(
        "tests\KeybindTests.cpp", "tests\ActionExecutorTests.cpp",
        "tests\InputManagerTests.cpp", "tests\RegressionTests.cpp",
        "tests\SAMPBridgeTests.cpp", "tests\ImGuiOverlayTests.cpp",
        "tests\HitmarkerTests.cpp", "tests\ThemeManagerTests.cpp",
        "tests\UIComponentsTests.cpp", "tests\SourceContractTests.cpp",
        "tests\TestStubs.cpp"
    )
    $missingTests = @()
    foreach ($t in $requiredTests) {
        $path = Join-Path $ProjectRoot $t
        if (-not (Test-Path $path)) {
            $missingTests += $t
        }
    }
    if ($missingTests.Count -eq 0) {
        $checks += "  [OK] All $($requiredTests.Count) required test files present"
    } else {
        $checks += "  [!!] Missing test files: $($missingTests -join ', ')"
    }

    # Determine overall result
    $failures = ($checks | Where-Object { $_ -match "\[!!\]" }).Count
    if ($failures -eq 0) {
        $result.Status = "PASS"
        $result.Detail = "All source contracts verified"
    } else {
        $result.Detail = "$failures contract check(s) failed"
    }

    $content = @(
        "Source Contracts: $($result.Status)",
        "",
        "--- Checks ---",
        ($checks -join "`n")
    ) -join "`n"
    Write-Evidence -Slug "source-contracts" -Content $content

    return $result
}

# ── Scenario: unit-tests ─────────────────────────────────────────

function Invoke-UnitTests {
    Write-Host "`n[scenario] unit-tests"
    $result = @{ Name = "unit-tests"; Status = "FAIL"; Detail = "" }

    # Try running existing test infrastructure
    $testBat = Join-Path $ProjectRoot "tools\run_tests.bat"
    if (-not (Test-Path $testBat)) {
        $result.Status = "SKIP"
        $result.Detail = "run_tests.bat not found"
        Write-Evidence -Slug "unit-tests" -Content "SKIP: $($result.Detail)"
        return $result
    }

    try {
        Write-Host "[*] Running unit tests..."
        $testOutput = & cmd.exe /c "`"$testBat`"" 2>&1 | Out-String

        if ($LASTEXITCODE -eq 0) {
            $requiredSuiteMarkers = @(
                "Running ThemeManagerTests",
                "Running UIComponentsTests",
                "Running SourceContractTests",
                "TestNovaDefaultTokensAreStable",
                "TestDarkDefaultTokensAreStable",
                "TestAllBuiltInThemesMeetContrastContracts",
                "TestNovaTextHierarchyOrdering",
                "TestSettingsNestedThemeNameExtractionThroughKeybindManager",
                "TestSettingsSubtabEnumContract",
                "TestSubTabMigrationContract",
                "TestDarkTokenContractInThemeManager",
                "TestDeterministicFixturePresence",
                "TestNovaFixtureContract",
                "TestDarkFixtureContract"
            )
            $missingMarkers = @()
            foreach ($marker in $requiredSuiteMarkers) {
                if ($testOutput -notmatch [regex]::Escape($marker)) {
                    $missingMarkers += $marker
                }
            }

            if ($missingMarkers.Count -eq 0) {
                $result.Status = "PASS"
                $result.Detail = "Tests passed and theme suites were invoked (exit code 0)"
            } else {
                $result.Detail = "Tests passed, but missing expected theme suite marker(s): $($missingMarkers -join ', ')"
            }
        } else {
            $result.Detail = "Tests failed (exit code $LASTEXITCODE)"
        }

        Write-Evidence -Slug "unit-tests" -Content (@(
            "Unit Tests: $($result.Status)",
            "Exit Code: $LASTEXITCODE",
            "",
            "--- Output ---",
            $testOutput
        ) -join "`n")
    }
    catch {
        $result.Detail = "Exception: $($_.Exception.Message)"
        Write-Evidence -Slug "unit-tests" -Content "FAIL: $($result.Detail)"
    }

    return $result
}

# ── Runtime scenarios ─────────────────────────────────────────────

function Invoke-RuntimeScenario {
    <#
    .SYNOPSIS
        Execute a runtime QA scenario via the SA-MP launcher + config fixture.
        Uses keybinder.json fixture to set known state, then launches SA-MP.
        The plugin's own logging and the launch script capture runtime evidence.
    #>
    param(
        [Parameter(Mandatory)] [string]$ScenarioName,
        [string]$Description = "",
        [string]$FixtureName = "keybinder_qa.json"
    )

    Write-Host "`n[scenario] $ScenarioName (runtime)"
    $result = @{ Name = $ScenarioName; Status = "FAIL"; Detail = "" }

    # Check runtime env
    $missing = Test-RuntimeEnv
    if ($missing.Count -gt 0) {
        $result.Status = "SKIP"
        $result.Detail = "Missing env: $($missing -join ', ')"
        Write-Evidence -Slug $ScenarioName -Content (@(
            "SKIP: Runtime environment not configured",
            "Missing: $($missing -join ', ')",
            "",
            "To enable runtime QA, set:",
            '  $env:GTA_SA_DIR = "C:\Games\GTA San Andreas"',
            '  $env:SAMP_03DL_DIR = "C:\Games\SA-MP 0.3.DL"',
            '  $env:SAMP_QA_HOST = "127.0.0.1"',
            '  $env:SAMP_QA_PORT = "7777"',
            '  $env:SAMP_QA_NICK = "QA_Tester"'
        ) -join "`n")
        return $result
    }

    if (-not $Runtime) {
        $result.Status = "SKIP"
        $result.Detail = "Runtime mode not enabled (-Runtime flag required)"
        Write-Evidence -Slug $ScenarioName -Content "SKIP: Runtime mode not enabled. Use -Runtime flag."
        return $result
    }

    # Deploy QA config fixture to the live NLBinder config location
    $qaFixture = Join-Path $FixtureDir $FixtureName
    $configuredThemeName = ""
    if (Test-Path $qaFixture) {
        try {
            $fixtureJson = Get-Content -Path $qaFixture -Raw | ConvertFrom-Json
            $configuredThemeName = [string]$fixtureJson.settings.theme_name
            if ($configuredThemeName -notin @("Dark", "Nova", "Ember", "Ocean", "Forest")) {
                $result.Detail = "Runtime fixture $FixtureName has invalid or missing theme_name"
                Write-Evidence -Slug $ScenarioName -Content (@(
                    "FAIL: Runtime fixture theme_name invalid",
                    "Fixture: $qaFixture",
                    "theme_name: $configuredThemeName"
                ) -join "`n")
                return $result
            }
        } catch {
            $result.Detail = "Runtime fixture parse failed: $($_.Exception.Message)"
            Write-Evidence -Slug $ScenarioName -Content "FAIL: $($result.Detail)"
            return $result
        }

        if (-not (Test-Path $LiveConfigDir)) {
            New-Item -ItemType Directory -Path $LiveConfigDir -Force | Out-Null
        }
        Copy-Item -Path $qaFixture -Destination $LiveConfigFile -Force
        Write-Host "[+] Deployed QA fixture to $LiveConfigFile (theme_name=$configuredThemeName)"
    } else {
        $result.Detail = "Missing runtime fixture: $qaFixture"
        Write-Evidence -Slug $ScenarioName -Content "FAIL: $($result.Detail)"
        return $result
    }

    # Launch via existing launcher
    $launchScript = Join-Path $ProjectRoot "tools\launch_samp_03dl.ps1"
    try {
        $launchArgs = @("-Scenario", $ScenarioName)
        $launchArgs += @("-TaskNumber", (Get-LauncherTaskNumber -Value $TaskId), "-DeterministicEvidence")
        if ($NoBuild) { $launchArgs += "-NoBuild" }

        $launchOutput = & $launchScript @launchArgs 2>&1 | Out-String

        if ($LASTEXITCODE -eq 0) {
            $result.Status = "PASS"
            $result.Detail = "$Description - launch successful"
        } else {
            $result.Detail = "$Description - launch failed (exit $LASTEXITCODE)"
        }

        Write-Evidence -Slug $ScenarioName -Content (@(
            "Runtime Scenario: $ScenarioName",
            "Status: $($result.Status)",
            "Description: $Description",
            "Fixture: $FixtureName",
            "Configured theme_name: $configuredThemeName",
            "NLBinder Root: $NlbinderRoot",
            "Live Config Dir: $LiveConfigDir",
            "Live Config File: $LiveConfigFile",
            "Live Config Exists: $(Test-Path $LiveConfigFile)",
            "",
            "--- Launch Output ---",
            $launchOutput
        ) -join "`n")
    }
    catch {
        $result.Detail = "Launch exception: $($_.Exception.Message)"
        Write-Evidence -Slug $ScenarioName -Content "FAIL: $($result.Detail)"
    }

    return $result
}

# ── Runtime scenario wrappers ────────────────────────────────────

function Invoke-OverlayToggle {
    return Invoke-RuntimeScenario -ScenarioName "overlay-toggle" `
        -Description "Toggle overlay via F2 key, verify visible state transitions"
}

function Invoke-ScreenNavigate {
    return Invoke-RuntimeScenario -ScenarioName "screen-navigate" `
        -Description "Navigate sidebar categories and verify Settings > Appearance theme selector is reachable"
}

function Invoke-AccentCustomize {
    return Invoke-RuntimeScenario -ScenarioName "accent-customize" `
        -Description "Customize accent color via Settings > Appearance using Nova fixture" `
        -FixtureName "nova-theme.json"
}

function Invoke-InterfaceEditorToggle {
    return Invoke-RuntimeScenario -ScenarioName "interface-editor-toggle" `
        -Description "Apply persisted Interface Editor radar preset on launch and exercise live scoreboard utility" `
        -FixtureName "interface_editor_qa.json"
}

function Invoke-DeviceReset {
    return Invoke-RuntimeScenario -ScenarioName "device-reset" `
        -Description "Simulate D3D9 device lost/reset with configured theme_name, verify overlay recovery"
}

function Invoke-InputLabelLayout {
    return Invoke-RuntimeScenario -ScenarioName "input-label-layout" `
        -Description "Verify input-label alignment across six regression surfaces: Hitmarker Eigenschaften, Hitmarker Preset-Name, Keybind Editor Name/Slash Command, Keybind List Search, and Settings GUI open inputs"
}

function Invoke-QuickMapToggle {
    return Invoke-RuntimeScenario -ScenarioName "quickmap-toggle" `
        -Description "Toggle QuickMap via hotkey (default M), verify map visibility and mutual exclusivity with main overlay"
}

function Invoke-QuickMapHold {
    return Invoke-RuntimeScenario -ScenarioName "quickmap-hold" `
        -Description "Hold QuickMap hotkey to show map, release to hide, verify hold mode behavior"
}

function Invoke-QuickMapMarkers {
    return Invoke-RuntimeScenario -ScenarioName "quickmap-markers" `
        -Description "Verify player marker, checkpoints, map icons, and gang zones render correctly on QuickMap"
}

function Invoke-QuickMapDeviceReset {
    return Invoke-RuntimeScenario -ScenarioName "quickmap-device-reset" `
        -Description "Alt-tab while QuickMap visible, verify map survives device lost/reset cycle"
}

# ── Scenario dispatch ────────────────────────────────────────────

$allResults = @()

# Static scenarios (always runnable)
$staticScenarios = @("build-verify", "config-roundtrip", "source-contracts", "unit-tests")

# Extended static scenarios (source validation)
$extendedStatic = @("source-contracts", "unit-tests")

# Runtime scenarios
$runtimeScenarios = @("overlay-toggle", "screen-navigate", "accent-customize", "interface-editor-toggle", "device-reset", "input-label-layout", "quickmap-toggle", "quickmap-hold", "quickmap-markers", "quickmap-device-reset")

switch ($Scenario) {
    "all" {
        # Run all static scenarios
        $allResults += Invoke-BuildVerify
        $allResults += Invoke-ConfigRoundtrip
        $allResults += Invoke-SourceContracts
        $allResults += Invoke-UnitTests

        # Run runtime scenarios (will skip if no env)
        foreach ($rs in $runtimeScenarios) {
            switch ($rs) {
                "overlay-toggle"   { $allResults += Invoke-OverlayToggle }
                "screen-navigate"  { $allResults += Invoke-ScreenNavigate }
                "accent-customize" { $allResults += Invoke-AccentCustomize }
                "interface-editor-toggle" { $allResults += Invoke-InterfaceEditorToggle }
                "device-reset"     { $allResults += Invoke-DeviceReset }
                "input-label-layout" { $allResults += Invoke-InputLabelLayout }
                "quickmap-toggle"  { $allResults += Invoke-QuickMapToggle }
                "quickmap-hold"    { $allResults += Invoke-QuickMapHold }
                "quickmap-markers" { $allResults += Invoke-QuickMapMarkers }
                "quickmap-device-reset" { $allResults += Invoke-QuickMapDeviceReset }
            }
        }
    }
    "build-verify" {
        $allResults += Invoke-BuildVerify
    }
    "config-roundtrip" {
        $allResults += Invoke-ConfigRoundtrip
    }
    "source-contracts" {
        $allResults += Invoke-SourceContracts
    }
    "unit-tests" {
        $allResults += Invoke-UnitTests
    }
    "overlay-toggle" {
        $allResults += Invoke-OverlayToggle
    }
    "screen-navigate" {
        $allResults += Invoke-ScreenNavigate
    }
    "accent-customize" {
        $allResults += Invoke-AccentCustomize
    }
    "interface-editor-toggle" {
        $allResults += Invoke-InterfaceEditorToggle
    }
    "device-reset" {
        $allResults += Invoke-DeviceReset
    }
    "input-label-layout" {
        $allResults += Invoke-InputLabelLayout
    }
    "quickmap-toggle" {
        $allResults += Invoke-QuickMapToggle
    }
    "quickmap-hold" {
        $allResults += Invoke-QuickMapHold
    }
    "quickmap-markers" {
        $allResults += Invoke-QuickMapMarkers
    }
    "quickmap-device-reset" {
        $allResults += Invoke-QuickMapDeviceReset
    }
    "full-runtime" {
        $allResults += Invoke-BuildVerify
        foreach ($rs in $runtimeScenarios) {
            switch ($rs) {
                "overlay-toggle"   { $allResults += Invoke-OverlayToggle }
                "screen-navigate"  { $allResults += Invoke-ScreenNavigate }
                "accent-customize" { $allResults += Invoke-AccentCustomize }
                "interface-editor-toggle" { $allResults += Invoke-InterfaceEditorToggle }
                "device-reset"     { $allResults += Invoke-DeviceReset }
                "input-label-layout" { $allResults += Invoke-InputLabelLayout }
                "quickmap-toggle"  { $allResults += Invoke-QuickMapToggle }
                "quickmap-hold"    { $allResults += Invoke-QuickMapHold }
                "quickmap-markers" { $allResults += Invoke-QuickMapMarkers }
                "quickmap-device-reset" { $allResults += Invoke-QuickMapDeviceReset }
            }
        }
    }
}

# ── Summary ──────────────────────────────────────────────────────
$success = Write-EvidenceSummary -Results $allResults

if ($success) {
    Write-Host "`n[+] All QA scenarios passed."
    exit 0
} else {
    Write-Host "`n[!] Some QA scenarios failed. Review evidence above."
    exit 1
}
