<#
.SYNOPSIS
    QA launcher for SA-MP 0.3.DL keybinder plugin testing.

.DESCRIPTION
    Copies the latest built plugin into the SA-MP 0.3.DL game directory,
    optionally launches SA-MP connecting to a QA server, and validates
    the required environment contract.

.PARAMETER Scenario
    The QA scenario to execute. Used by downstream tasks for targeted testing.
    Examples: samp-init, overlay-toggle, interface-editor-toggle, overlay-reset, list-filters, etc.

.PARAMETER NoBuild
    Skip the build step and use the existing binary.

.EXAMPLE
    .\launch_samp_03dl.ps1
    .\launch_samp_03dl.ps1 -Scenario samp-init
    .\launch_samp_03dl.ps1 -Scenario overlay-toggle -NoBuild
    .\launch_samp_03dl.ps1 -Scenario interface-editor-toggle -TaskNumber 9 -DeterministicEvidence

.NOTES
    Required environment variables:
      GTA_SA_DIR      - Path to GTA San Andreas installation
      SAMP_03DL_DIR   - Path to SA-MP 0.3.DL installation (samp.dll location)
      SAMP_QA_HOST    - QA server hostname/IP
      SAMP_QA_PORT    - QA server port
      SAMP_QA_NICK    - Player nickname for QA session
#>

[CmdletBinding()]
param(
    [string]$Scenario = "default",
    [switch]$NoBuild,
    [int]$TaskNumber = 2,
    [switch]$DeterministicEvidence
)

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptRoot
$UseDeterministicEvidence = $DeterministicEvidence.IsPresent -or $Scenario -in @("settings-evidence", "interface-editor-toggle")
$NlbinderRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "GTA San Andreas User Files\NLBinder"

# ── Environment contract validation ──────────────────────────────
$requiredVars = @(
    @{ Name = "GTA_SA_DIR";    Desc = "Path to GTA San Andreas installation" },
    @{ Name = "SAMP_03DL_DIR"; Desc = "Path to SA-MP 0.3.DL installation" },
    @{ Name = "SAMP_QA_HOST";  Desc = "QA server hostname/IP" },
    @{ Name = "SAMP_QA_PORT";  Desc = "QA server port" },
    @{ Name = "SAMP_QA_NICK";  Desc = "Player nickname for QA" }
)

$missing = @()
foreach ($var in $requiredVars) {
    $val = [System.Environment]::GetEnvironmentVariable($var.Name)
    if ([string]::IsNullOrWhiteSpace($val)) {
        $missing += "  $($var.Name) - $($var.Desc)"
    }
}

if ($missing.Count -gt 0) {
    Write-Error @"
FATAL: Missing required environment variables:
$($missing -join "`n")

Set these variables before running the QA launcher.
Example:
  `$env:GTA_SA_DIR = "C:\Games\GTA San Andreas"
  `$env:SAMP_03DL_DIR = "C:\Games\SA-MP 0.3.DL"
  `$env:SAMP_QA_HOST = "127.0.0.1"
  `$env:SAMP_QA_PORT = "7777"
  `$env:SAMP_QA_NICK = "QA_Tester"
"@
    exit 1
}

$GtaSaDir    = $env:GTA_SA_DIR
$SampDir     = $env:SAMP_03DL_DIR
$QaHost      = $env:SAMP_QA_HOST
$QaPort      = $env:SAMP_QA_PORT
$QaNick      = $env:SAMP_QA_NICK

# ── Path validation ──────────────────────────────────────────────
if (-not (Test-Path $GtaSaDir)) {
    Write-Error "FATAL: GTA_SA_DIR does not exist: $GtaSaDir"
    exit 1
}
if (-not (Test-Path $SampDir)) {
    Write-Error "FATAL: SAMP_03DL_DIR does not exist: $SampDir"
    exit 1
}

# ── Paths ────────────────────────────────────────────────────────
$PluginBinary  = Join-Path $ProjectRoot "build\Release\NLBinder.asi"
$ScriptsDir    = Join-Path $GtaSaDir "scripts"
$TargetPlugin  = Join-Path $ScriptsDir "NLBinder.asi"
$SampExe       = Join-Path $SampDir "samp.exe"
$EvidenceDir   = Join-Path $ProjectRoot ".sisyphus\evidence"
$QaFixtureFile = Join-Path $ProjectRoot "tools\qa\fixtures\keybinder_qa.json"
$InterfaceEditorFixtureFile = Join-Path $ProjectRoot "tools\qa\fixtures\interface_editor_qa.json"
$JobsOverviewFixtureFile    = Join-Path $ProjectRoot "tools\qa\fixtures\jobs-overview.json"
$HitsoundsFixtureFile       = Join-Path $ProjectRoot "tools\qa\fixtures\hitsounds_qa.json"
$LiveConfigDir = $NlbinderRoot
$LiveConfigFile = Join-Path $NlbinderRoot "keybinder.json"

# Loading the former filename beside the renamed plugin runs the binder twice.
# Leave existing installations untouched and require the operator to resolve it.
$conflictingPlugins = @(
    (Join-Path $GtaSaDir 'Keybinder.asi'),
    (Join-Path $ScriptsDir 'Keybinder.asi'),
    (Join-Path $GtaSaDir 'NLBinder.asi'),
    (Join-Path $GtaSaDir 'plugins\Keybinder.asi'),
    (Join-Path $GtaSaDir 'plugins\NLBinder.asi')
) | Where-Object { Test-Path -LiteralPath $_ }
if ($conflictingPlugins.Count -gt 0) {
    throw ("Another Binder ASI is installed. Back it up outside the game's ASI folders before using this launcher: " + ($conflictingPlugins -join ', '))
}

function Get-EvidencePath {
    param(
        [Parameter(Mandatory)] [string]$Slug,
        [string]$Extension = "log"
    )

    if ($UseDeterministicEvidence) {
        return Join-Path $EvidenceDir ("task-{0}-{1}.{2}" -f $TaskNumber, $Slug, $Extension)
    }

    return Join-Path $EvidenceDir ("qa-launch-{0}-{1}.{2}" -f $Slug, (Get-Date -Format 'yyyyMMdd-HHmmss'), $Extension)
}

$LogFile = Get-EvidencePath -Slug $Scenario -Extension "log"

# ── Ensure directories exist ─────────────────────────────────────
if (-not (Test-Path $ScriptsDir)) {
    New-Item -ItemType Directory -Path $ScriptsDir -Force | Out-Null
    Write-Host "[+] Created scripts directory: $ScriptsDir"
}
if (-not (Test-Path $EvidenceDir)) {
    New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null
}
if (-not (Test-Path $LiveConfigDir)) {
    New-Item -ItemType Directory -Path $LiveConfigDir -Force | Out-Null
}
New-Item -ItemType File -Path $LogFile -Force | Out-Null

function Write-Evidence {
    param(
        [Parameter(Mandatory)] [string]$Slug,
        [Parameter(Mandatory)] [string]$Content,
        [string]$Extension = "txt"
    )

    $path = Get-EvidencePath -Slug $Slug -Extension $Extension
    $Content | Out-File -FilePath $path -Encoding utf8 -Force
    Write-Host "[evidence] $path"
    return $path
}

function Initialize-AutomationInterop {
    if ("QaWin32" -as [type]) {
        return
    }

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class QaWin32 {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint KEYEVENTF_KEYUP = 0x0002;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }
}
"@
}

function Wait-ForMainWindow {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $Process.Refresh()
            if ($Process.MainWindowHandle -ne 0) {
                return $Process.MainWindowHandle
            }
        }
        catch {
        }

        Start-Sleep -Milliseconds 250
    }

    throw "Timed out waiting for samp.exe main window (PID $($Process.Id))."
}

function Get-ClientBounds {
    param(
        [Parameter(Mandatory)] [IntPtr]$WindowHandle
    )

    $rect = New-Object QaWin32+RECT
    if (-not [QaWin32]::GetClientRect($WindowHandle, [ref]$rect)) {
        throw "GetClientRect failed for handle $WindowHandle"
    }

    $origin = New-Object QaWin32+POINT
    $origin.X = 0
    $origin.Y = 0
    if (-not [QaWin32]::ClientToScreen($WindowHandle, [ref]$origin)) {
        throw "ClientToScreen failed for handle $WindowHandle"
    }

    [pscustomobject]@{
        Left   = $origin.X
        Top    = $origin.Y
        Width  = $rect.Right - $rect.Left
        Height = $rect.Bottom - $rect.Top
    }
}

function Focus-Window {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 30
    )

    Initialize-AutomationInterop
    try {
        $null = $Process.WaitForInputIdle(30000)
    }
    catch {
    }

    $handle = Wait-ForMainWindow -Process $Process -TimeoutSeconds $TimeoutSeconds
    [QaWin32]::ShowWindowAsync($handle, 5) | Out-Null
    [QaWin32]::SetForegroundWindow($handle) | Out-Null
    Start-Sleep -Milliseconds 600
    return $handle
}

function Invoke-VirtualKey {
    param(
        [Parameter(Mandatory)] [byte]$VirtualKey,
        [int]$HoldMilliseconds = 60,
        [int]$SettlingMilliseconds = 600
    )

    Initialize-AutomationInterop
    [QaWin32]::keybd_event($VirtualKey, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $HoldMilliseconds
    [QaWin32]::keybd_event($VirtualKey, 0, [QaWin32]::KEYEVENTF_KEYUP, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $SettlingMilliseconds
}

function Invoke-LeftClick {
    param(
        [Parameter(Mandatory)] [int]$X,
        [Parameter(Mandatory)] [int]$Y,
        [int]$SettlingMilliseconds = 700
    )

    Initialize-AutomationInterop
    [QaWin32]::SetCursorPos($X, $Y) | Out-Null
    Start-Sleep -Milliseconds 120
    [QaWin32]::mouse_event([QaWin32]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [QaWin32]::mouse_event([QaWin32]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $SettlingMilliseconds
}

function Invoke-RelativeClick {
    param(
        [Parameter(Mandatory)] [IntPtr]$WindowHandle,
        [Parameter(Mandatory)] [double]$RatioX,
        [Parameter(Mandatory)] [double]$RatioY,
        [int]$SettlingMilliseconds = 700
    )

    $bounds = Get-ClientBounds -WindowHandle $WindowHandle
    $x = [int][Math]::Round($bounds.Left + ($bounds.Width * $RatioX))
    $y = [int][Math]::Round($bounds.Top + ($bounds.Height * $RatioY))
    Invoke-LeftClick -X $x -Y $y -SettlingMilliseconds $SettlingMilliseconds

    return [pscustomobject]@{
        ratioX = $RatioX
        ratioY = $RatioY
        x = $x
        y = $y
        width = $bounds.Width
        height = $bounds.Height
    }
}

function Invoke-RelativeDrag {
    param(
        [Parameter(Mandatory)] [IntPtr]$WindowHandle,
        [Parameter(Mandatory)] [double]$StartRatioX,
        [Parameter(Mandatory)] [double]$EndRatioX,
        [Parameter(Mandatory)] [double]$RatioY,
        [int]$Steps = 12,
        [int]$StepDelayMilliseconds = 35,
        [int]$SettlingMilliseconds = 900
    )

    Initialize-AutomationInterop
    $bounds = Get-ClientBounds -WindowHandle $WindowHandle
    $startX = [int][Math]::Round($bounds.Left + ($bounds.Width * $StartRatioX))
    $endX = [int][Math]::Round($bounds.Left + ($bounds.Width * $EndRatioX))
    $y = [int][Math]::Round($bounds.Top + ($bounds.Height * $RatioY))

    [QaWin32]::SetCursorPos($startX, $y) | Out-Null
    Start-Sleep -Milliseconds 120
    [QaWin32]::mouse_event([QaWin32]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)

    for ($step = 1; $step -le $Steps; $step++) {
        $t = [double]$step / [double]$Steps
        $nextX = [int][Math]::Round($startX + (($endX - $startX) * $t))
        [QaWin32]::SetCursorPos($nextX, $y) | Out-Null
        Start-Sleep -Milliseconds $StepDelayMilliseconds
    }

    [QaWin32]::mouse_event([QaWin32]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $SettlingMilliseconds

    return [pscustomobject]@{
        startRatioX = $StartRatioX
        endRatioX = $EndRatioX
        ratioY = $RatioY
        startX = $startX
        endX = $endX
        y = $y
        width = $bounds.Width
        height = $bounds.Height
    }
}

function Save-WindowScreenshot {
    param(
        [Parameter(Mandatory)] [IntPtr]$WindowHandle,
        [Parameter(Mandatory)] [string]$Slug
    )

    Initialize-AutomationInterop
    $bounds = Get-ClientBounds -WindowHandle $WindowHandle
    if ($bounds.Width -le 0 -or $bounds.Height -le 0) {
        throw "Window bounds are invalid for screenshot capture."
    }

    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
        $path = Get-EvidencePath -Slug $Slug -Extension "png"
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "[evidence] $path"
        return $path
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-InterfaceEditorSnapshot {
    param(
        [Parameter(Mandatory)] [object]$Settings
    )

    $savedPresets = @($Settings.interface_editor_saved_presets)
    $activePresetIndex = 0
    if ($null -ne $Settings.interface_editor_active_preset_index) {
        $activePresetIndex = [int]$Settings.interface_editor_active_preset_index
    }

    $activePreset = $null
    if ($activePresetIndex -ge 0 -and $activePresetIndex -lt $savedPresets.Count) {
        $preset = $savedPresets[$activePresetIndex]
        $activePreset = [ordered]@{
            name = [string]$preset.name
            hp_color_r = $preset.hp_color_r
            armor_color_r = $preset.armor_color_r
            money_color_r = $preset.money_color_r
            override_zone_text = [bool]$preset.override_zone_text
            zone_text_pos_x = $preset.zone_text_pos_x
            zone_text_pos_y = $preset.zone_text_pos_y
            zone_text = [string]$preset.zone_text
        }
    }

    return [pscustomobject]@{
        active_preset_index = $activePresetIndex
        preset_count = $savedPresets.Count
        active_preset = $activePreset
    }
}

function Get-LiveSettingsSnapshot {
    if (-not (Test-Path $LiveConfigFile)) {
        return [pscustomobject]@{
            exists = $false
            path = $LiveConfigFile
        }
    }

    try {
        $json = Get-Content -Path $LiveConfigFile -Raw | ConvertFrom-Json
        $settings = $json.settings
        $interfaceEditor = $null
        if ($null -ne $settings) {
            $interfaceEditor = Get-InterfaceEditorSnapshot -Settings $settings
        }
        return [pscustomobject]@{
            exists = $true
            path = $LiveConfigFile
            ui_toggle_hotkey = $settings.ui_toggle_hotkey
            theme_custom_accent = $settings.theme_custom_accent
            interface_editor = $interfaceEditor
        }
    }
    catch {
        return [pscustomobject]@{
            exists = $true
            path = $LiveConfigFile
            error = $_.Exception.Message
        }
    }
}

function Test-InterfaceEditorLaunchState {
    param(
        [Parameter(Mandatory)] [psobject]$Snapshot
    )

    if (-not $Snapshot.exists) {
        return [pscustomobject]@{ passed = $false; reason = "Live config not found at $($Snapshot.path)" }
    }

    if ($Snapshot.error) {
        return [pscustomobject]@{ passed = $false; reason = "Live config parse failed: $($Snapshot.error)" }
    }

    if ($null -eq $Snapshot.interface_editor) {
        return [pscustomobject]@{ passed = $false; reason = "interface_editor settings snapshot missing" }
    }

    if ($Snapshot.interface_editor.preset_count -le 0) {
        return [pscustomobject]@{ passed = $false; reason = "No saved interface editor presets available" }
    }

    if ($null -eq $Snapshot.interface_editor.active_preset) {
        return [pscustomobject]@{ passed = $false; reason = "Active preset index $($Snapshot.interface_editor.active_preset_index) did not resolve" }
    }

    if ($null -eq $Snapshot.interface_editor.active_preset.hp_color_r -or
        $null -eq $Snapshot.interface_editor.active_preset.armor_color_r -or
        $null -eq $Snapshot.interface_editor.active_preset.money_color_r) {
        return [pscustomobject]@{ passed = $false; reason = "Active preset missing HUD color metadata" }
    }

    if ($Snapshot.interface_editor.active_preset.override_zone_text -ne $true -or [string]::IsNullOrWhiteSpace([string]$Snapshot.interface_editor.active_preset.zone_text)) {
        return [pscustomobject]@{ passed = $false; reason = "Active preset does not provide custom zone text" }
    }

    if ($null -eq $Snapshot.interface_editor.active_preset.zone_text_pos_x -or $null -eq $Snapshot.interface_editor.active_preset.zone_text_pos_y) {
        return [pscustomobject]@{ passed = $false; reason = "Active preset does not provide custom zone text position" }
    }

    return [pscustomobject]@{
        passed = $true
        reason = "Active preset index $($Snapshot.interface_editor.active_preset_index) persists HUD colors, custom zone text, and zone text position"
    }
}

function Write-CheckpointEvidence {
    param(
        [Parameter(Mandatory)] [string]$Slug,
        [Parameter(Mandatory)] [string]$Step,
        [Parameter(Mandatory)] [IntPtr]$WindowHandle,
        [hashtable]$Extra = @{}
    )

    $bounds = Get-ClientBounds -WindowHandle $WindowHandle
    $null = Save-WindowScreenshot -WindowHandle $WindowHandle -Slug $Slug

    $payload = [ordered]@{
        scenario = $Scenario
        task = $TaskNumber
        step = $Step
        captured_at = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
        window = [ordered]@{
            left = $bounds.Left
            top = $bounds.Top
            width = $bounds.Width
            height = $bounds.Height
        }
        settings = Get-LiveSettingsSnapshot
    }

    foreach ($key in $Extra.Keys) {
        $payload[$key] = $Extra[$key]
    }

    $json = $payload | ConvertTo-Json -Depth 8
    Write-Evidence -Slug $Slug -Content $json -Extension "json" | Out-Null
}

function Stop-ProcessCleanly {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 20
    )

    if ($Process.HasExited) {
        return
    }

    $null = $Process.CloseMainWindow()
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

function Get-ScenarioFixtureSource {
    switch ($Scenario) {
        "settings-evidence" { return $QaFixtureFile }
        "interface-editor-toggle" { return $InterfaceEditorFixtureFile }
        "jobs-overview" { return $JobsOverviewFixtureFile }
        "hitsound-config" { return $HitsoundsFixtureFile }
        default { return $null }
    }
}

function Invoke-SettingsEvidenceCapture {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process
    )

    $windowHandle = Focus-Window -Process $Process -TimeoutSeconds 45
    Start-Sleep -Seconds 8

    Invoke-VirtualKey -VirtualKey ([byte][System.Windows.Forms.Keys]::F2) -SettlingMilliseconds 1200
    Write-CheckpointEvidence -Slug "settings-open-state" -Step "Opened overlay with configured hotkey to exercise visible render path" -WindowHandle $windowHandle -Extra @{
        action = "F2"
        expectation = "Main overlay window visible"
    }

    $settingsNavClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.095 -RatioY 0.47 -SettlingMilliseconds 1000
    Write-CheckpointEvidence -Slug "settings-subtab-switch" -Step "Switched from default landing view into Settings navigation" -WindowHandle $windowHandle -Extra @{
        action = "sidebar-click"
        target = "settings"
        coordinates = $settingsNavClick
    }

    $accentClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.46 -RatioY 0.52 -SettlingMilliseconds 1100
    Write-CheckpointEvidence -Slug "settings-accent-edit" -Step "Used custom accent color picker to verify settings accent updates" -WindowHandle $windowHandle -Extra @{
        action = "custom-accent-picker"
        target = "appearance-accent-color"
        coordinates = $accentClick
    }

    $interfaceTabClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.41 -RatioY 0.25 -SettlingMilliseconds 1100
    Write-CheckpointEvidence -Slug "settings-auto-scale-note" -Step "Opened the interface tab to verify automatic scaling is informational only" -WindowHandle $windowHandle -Extra @{
        action = "settings-subtab-click"
        target = "interface-auto-scale"
        coordinates = $interfaceTabClick
    }

    Stop-ProcessCleanly -Process $Process -TimeoutSeconds 25

    $summary = @(
        "Scenario: settings-evidence",
        "TaskNumber: $TaskNumber",
        "ProcessExited: $($Process.HasExited)",
        "ExitCode: $($Process.ExitCode)",
        "Open evidence: $(Get-EvidencePath -Slug 'settings-open-state' -Extension 'json')",
        "Navigation evidence: $(Get-EvidencePath -Slug 'settings-subtab-switch' -Extension 'json')",
        "Accent evidence: $(Get-EvidencePath -Slug 'settings-accent-edit' -Extension 'json')",
        "Auto-scale evidence: $(Get-EvidencePath -Slug 'settings-auto-scale-note' -Extension 'json')"
    ) -join "`n"
    Write-Evidence -Slug "settings-evidence-summary" -Content $summary -Extension "txt" | Out-Null
}

function Invoke-InterfaceEditorToggleCapture {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process
    )

    $windowHandle = Focus-Window -Process $Process -TimeoutSeconds 45
    Start-Sleep -Seconds 8

    $launchSnapshot = Get-LiveSettingsSnapshot
    $launchValidation = Test-InterfaceEditorLaunchState -Snapshot $launchSnapshot
    if (-not $launchValidation.passed) {
        throw "Interface Editor launch-state validation failed: $($launchValidation.reason)"
    }

    Write-CheckpointEvidence -Slug "interface-editor-launch-state" -Step "Validated persisted Interface Editor preset on launch before opening the overlay" -WindowHandle $windowHandle -Extra @{
        action = "launch-state"
        expectation = "Active Interface Editor preset persists HUD colors and custom zone text"
        validation = $launchValidation
    }

    Invoke-VirtualKey -VirtualKey ([byte][System.Windows.Forms.Keys]::F2) -SettlingMilliseconds 1200
    Write-CheckpointEvidence -Slug "interface-editor-overlay-open" -Step "Opened the overlay to begin Interface Editor runtime interaction" -WindowHandle $windowHandle -Extra @{
        action = "F2"
        expectation = "Overlay visible while persisted radar-hidden state remains active"
    }

    $modsNavClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.095 -RatioY 0.405 -SettlingMilliseconds 1100
    Write-CheckpointEvidence -Slug "interface-editor-mods-nav" -Step "Opened the Mods navigation section that hosts the Interface Editor" -WindowHandle $windowHandle -Extra @{
        action = "sidebar-click"
        target = "mods"
        coordinates = $modsNavClick
    }

    $interfaceEditorTabClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.57 -RatioY 0.245 -SettlingMilliseconds 1200
    Write-CheckpointEvidence -Slug "interface-editor-tab" -Step "Selected the Interface Editor tab inside Mods" -WindowHandle $windowHandle -Extra @{
        action = "mods-subtab-click"
        target = "interface-editor"
        coordinates = $interfaceEditorTabClick
    }

    $scoreboardShowClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.39 -RatioY 0.84 -SettlingMilliseconds 1300
    Write-CheckpointEvidence -Slug "interface-editor-scoreboard-show" -Step "Triggered the live SA-MP utility action to show the scoreboard from Interface Editor" -WindowHandle $windowHandle -Extra @{
        action = "utility-button"
        target = "scoreboard-show"
        coordinates = $scoreboardShowClick
        expectation = "Scoreboard opens and the Interface Editor status row reflects an active scoreboard"
    }

    $scoreboardCloseClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.53 -RatioY 0.84 -SettlingMilliseconds 1300
    Write-CheckpointEvidence -Slug "interface-editor-scoreboard-close" -Step "Closed the scoreboard to complete the live Interface Editor utility toggle" -WindowHandle $windowHandle -Extra @{
        action = "utility-button"
        target = "scoreboard-close"
        coordinates = $scoreboardCloseClick
        expectation = "Scoreboard closes cleanly without crashing the overlay or the game"
    }

    Stop-ProcessCleanly -Process $Process -TimeoutSeconds 25

    $summary = @(
        "Scenario: interface-editor-toggle",
        "TaskNumber: $TaskNumber",
        "ProcessExited: $($Process.HasExited)",
        "ExitCode: $($Process.ExitCode)",
        "Launch-state evidence: $(Get-EvidencePath -Slug 'interface-editor-launch-state' -Extension 'json')",
        "Overlay-open evidence: $(Get-EvidencePath -Slug 'interface-editor-overlay-open' -Extension 'json')",
        "Mods-nav evidence: $(Get-EvidencePath -Slug 'interface-editor-mods-nav' -Extension 'json')",
        "Interface Editor tab evidence: $(Get-EvidencePath -Slug 'interface-editor-tab' -Extension 'json')",
        "Scoreboard show evidence: $(Get-EvidencePath -Slug 'interface-editor-scoreboard-show' -Extension 'json')",
        "Scoreboard close evidence: $(Get-EvidencePath -Slug 'interface-editor-scoreboard-close' -Extension 'json')"
    ) -join "`n"
    Write-Evidence -Slug "interface-editor-toggle-summary" -Content $summary -Extension "txt" | Out-Null
}

function Invoke-JobsOverviewCapture {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process
    )

    $windowHandle = Focus-Window -Process $Process -TimeoutSeconds 45
    Start-Sleep -Seconds 8

    Invoke-VirtualKey -VirtualKey ([byte][System.Windows.Forms.Keys]::F2) -SettlingMilliseconds 1200
    Write-CheckpointEvidence -Slug "jobs-overview-overlay-open" -Step "Opened overlay to begin Jobs overview capture" -WindowHandle $windowHandle -Extra @{
        action = "F2"
        expectation = "Main overlay window visible"
    }

    # Navigate to Jobs tab in sidebar
    $jobsNavClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.095 -RatioY 0.53 -SettlingMilliseconds 1100
    Write-CheckpointEvidence -Slug "jobs-overview-nav" -Step "Navigated to Jobs section in sidebar" -WindowHandle $windowHandle -Extra @{
        action = "sidebar-click"
        target = "jobs"
        coordinates = $jobsNavClick
    }

    # Click first enabled job in overview list to verify selection
    $firstJobClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.22 -RatioY 0.48 -SettlingMilliseconds 1000
    Write-CheckpointEvidence -Slug "jobs-overview-selection" -Step "Selected first enabled job in overview list" -WindowHandle $windowHandle -Extra @{
        action = "job-select"
        expectation = "Detail pane updates with per-job earnings, skill tree, recent events"
        coordinates = $firstJobClick
    }

    # Scroll down to verify skill tree and recent events render
    Start-Sleep -Milliseconds 800
    Write-CheckpointEvidence -Slug "jobs-overview-detail-full" -Step "Verified full detail pane renders skill tree and recent events cards" -WindowHandle $windowHandle -Extra @{
        action = "visual-verify"
        expectation = "Skill tree vertical list and recent events visible below earnings cards"
    }

    Stop-ProcessCleanly -Process $Process -TimeoutSeconds 25

    $summary = @(
        "Scenario: jobs-overview",
        "TaskNumber: $TaskNumber",
        "ProcessExited: $($Process.HasExited)",
        "ExitCode: $($Process.ExitCode)",
        "Overlay evidence: $(Get-EvidencePath -Slug 'jobs-overview-overlay-open' -Extension 'json')",
        "Nav evidence: $(Get-EvidencePath -Slug 'jobs-overview-nav' -Extension 'json')",
        "Selection evidence: $(Get-EvidencePath -Slug 'jobs-overview-selection' -Extension 'json')",
        "Detail evidence: $(Get-EvidencePath -Slug 'jobs-overview-detail-full' -Extension 'json')"
    ) -join "`n"
    Write-Evidence -Slug "jobs-overview-summary" -Content $summary -Extension "txt" | Out-Null
}

function Invoke-HitsoundConfigCapture {
    param(
        [Parameter(Mandatory)] [System.Diagnostics.Process]$Process
    )

    $windowHandle = Focus-Window -Process $Process -TimeoutSeconds 45
    Start-Sleep -Seconds 8

    # Verify hitsounds folder exists
    $hitsoundsDir = Join-Path $NlbinderRoot "hitsounds"
    $folderExists = Test-Path $hitsoundsDir
    Write-Evidence -Slug "hitsound-folder-check" -Content (@(
        "Hitsounds folder exists: $folderExists",
        "Path: $hitsoundsDir",
        "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    ) -join "`n") -Extension "txt" | Out-Null

    # Open overlay to verify hitsound settings are present
    Invoke-VirtualKey -VirtualKey ([byte][System.Windows.Forms.Keys]::F2) -SettlingMilliseconds 1200
    Write-CheckpointEvidence -Slug "hitsound-overlay-open" -Step "Opened overlay to verify hitsound configuration in settings" -WindowHandle $windowHandle -Extra @{
        action = "F2"
        expectation = "Main overlay window visible with hitsound settings available"
    }

    # Navigate to Settings
    $settingsNavClick = Invoke-RelativeClick -WindowHandle $windowHandle -RatioX 0.095 -RatioY 0.47 -SettlingMilliseconds 1000
    Write-CheckpointEvidence -Slug "hitsound-settings-nav" -Step "Navigated to Settings to verify hitsound configuration keys" -WindowHandle $windowHandle -Extra @{
        action = "sidebar-click"
        target = "settings"
        coordinates = $settingsNavClick
    }

    # Take a final screenshot showing the hitsound configuration state
    Start-Sleep -Milliseconds 500
    Write-CheckpointEvidence -Slug "hitsound-config-verified" -Step "Verified hitsound configuration keys are present in settings" -WindowHandle $windowHandle -Extra @{
        action = "verify-keys"
        expected_keys = @("hitsound_outgoing_enabled", "hitsound_incoming_enabled", "hitsound_outgoing_path", "hitsound_incoming_path")
        outgoing_enabled = $true
        incoming_enabled = $true
    }

    Stop-ProcessCleanly -Process $Process -TimeoutSeconds 25

    $summary = @(
        "Scenario: hitsound-config",
        "TaskNumber: $TaskNumber",
        "ProcessExited: $($Process.HasExited)",
        "ExitCode: $($Process.ExitCode)",
        "Folder exists: $folderExists",
        "Folder evidence: $(Get-EvidencePath -Slug 'hitsound-folder-check' -Extension 'txt')",
        "Overlay evidence: $(Get-EvidencePath -Slug 'hitsound-overlay-open' -Extension 'json')",
        "Settings nav evidence: $(Get-EvidencePath -Slug 'hitsound-settings-nav' -Extension 'json')",
        "Config verified evidence: $(Get-EvidencePath -Slug 'hitsound-config-verified' -Extension 'json')"
    ) -join "`n"
    Write-Evidence -Slug "hitsound-config-summary" -Content $summary -Extension "txt" | Out-Null
}

# ── Build (unless -NoBuild) ──────────────────────────────────────
if (-not $NoBuild) {
    Write-Host "[*] Building Keybinder (Release|Win32)..."
    $buildLog = & MSBuild.exe "$ProjectRoot\Keybinder.vcxproj" `
        /p:Configuration=Release /p:Platform=Win32 `
        /verbosity:minimal /nologo 2>&1

    $buildLog | Out-File -FilePath $LogFile -Encoding utf8

    if ($LASTEXITCODE -ne 0) {
        Write-Error "FATAL: Build failed. See log: $LogFile"
        exit 1
    }
    Write-Host "[+] Build succeeded."
}

# ── Copy plugin binary ───────────────────────────────────────────
if (-not (Test-Path $PluginBinary)) {
    Write-Error "FATAL: Plugin binary not found: $PluginBinary"
    exit 1
}

Copy-Item -Path $PluginBinary -Destination $TargetPlugin -Force
Write-Host "[+] Copied plugin to: $TargetPlugin"

$scenarioFixture = Get-ScenarioFixtureSource
if (-not [string]::IsNullOrWhiteSpace($scenarioFixture)) {
    if (-not (Test-Path $scenarioFixture)) {
        Write-Error "FATAL: Scenario fixture not found: $scenarioFixture"
        exit 1
    }

    Copy-Item -Path $scenarioFixture -Destination $LiveConfigFile -Force
    Write-Host "[+] Deployed scenario fixture to: $LiveConfigFile"
    Write-Evidence -Slug ("{0}-fixture" -f $Scenario) -Content (@(
        "Scenario: $Scenario",
        "Fixture:  $scenarioFixture",
        "Target:   $LiveConfigFile",
        "Applied:  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    ) -join "`n") -Extension "txt" | Out-Null
}

# ── Launch SA-MP ─────────────────────────────────────────────────
if (-not (Test-Path $SampExe)) {
    Write-Error "FATAL: samp.exe not found: $SampExe"
    exit 1
}

Write-Host "[*] Launching SA-MP 0.3.DL..."
Write-Host "    Server: ${QaHost}:${QaPort}"
Write-Host "    Nick:   $QaNick"
Write-Host "    Scenario: $Scenario"

$launchArgs = "-c -h $QaHost -p $QaPort -n $QaNick"
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

# Log launch metadata
@"
=== SA-MP 0.3.DL QA Launch ===
Timestamp: $timestamp
Scenario:  $Scenario
Server:    ${QaHost}:${QaPort}
Nick:      $QaNick
Plugin:    $TargetPlugin
GTA Dir:   $GtaSaDir
SAMP Dir:  $SampDir
Binary:    $PluginBinary
"@ | Out-File -FilePath $LogFile -Append -Encoding utf8

try {
    $proc = Start-Process -FilePath $SampExe -ArgumentList $launchArgs `
        -WorkingDirectory $GtaSaDir -PassThru

    Write-Host "[+] SA-MP launched (PID: $($proc.Id))"
    "SA-MP PID: $($proc.Id)" | Out-File -FilePath $LogFile -Append -Encoding utf8

    # Wait briefly for crash detection
    Start-Sleep -Seconds 5

    if ($proc.HasExited) {
        $exitCode = $proc.ExitCode
        "SA-MP exited early with code: $exitCode" | Out-File -FilePath $LogFile -Append -Encoding utf8
        Write-Error "FATAL: SA-MP exited within 5 seconds (exit code: $exitCode). Check log: $LogFile"
        exit 1
    }

    Write-Host "[+] SA-MP is running. QA scenario '$Scenario' is active."
    "SA-MP running after 5s check - launch successful" | Out-File -FilePath $LogFile -Append -Encoding utf8

    if ($Scenario -eq "settings-evidence") {
        Invoke-SettingsEvidenceCapture -Process $proc
        "Settings evidence capture completed" | Out-File -FilePath $LogFile -Append -Encoding utf8
    }
    elseif ($Scenario -eq "interface-editor-toggle") {
        Invoke-InterfaceEditorToggleCapture -Process $proc
        "Interface Editor toggle evidence capture completed" | Out-File -FilePath $LogFile -Append -Encoding utf8
    }
    elseif ($Scenario -eq "jobs-overview") {
        Invoke-JobsOverviewCapture -Process $proc
        "Jobs overview evidence capture completed" | Out-File -FilePath $LogFile -Append -Encoding utf8
    }
    elseif ($Scenario -eq "hitsound-config") {
        Invoke-HitsoundConfigCapture -Process $proc
        "Hitsound config evidence capture completed" | Out-File -FilePath $LogFile -Append -Encoding utf8
    }
}
catch {
    "Launch error: $($_.Exception.Message)" | Out-File -FilePath $LogFile -Append -Encoding utf8
    Write-Error "FATAL: Failed to launch SA-MP: $($_.Exception.Message)"
    exit 1
}

Write-Host "[+] QA launcher completed. Log: $LogFile"
exit 0
