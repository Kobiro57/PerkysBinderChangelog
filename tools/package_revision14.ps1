$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $projectRoot 'build\Release\NLBinder.asi'
$reportPath = Join-Path $projectRoot 'build\revision14-artifact.json'
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$binaryHash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
if ($report.revision -ne 'v0.1.1-r14' -or $binaryHash -ne $report.sha256 -or -not $report.embedded_helper_matches) {
    throw 'Die ASI stimmt nicht mit dem finalen r14-Prüfbericht überein.'
}
$helperPath = Join-Path $projectRoot 'build\ReconnectHelper.exe'
$helperHash = (Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($helperHash -ne $report.helper_sha256) { throw 'Der Reconnect-Helfer hat sich seit der Artefaktprüfung geändert.' }
$workerPath = Join-Path $projectRoot 'build\reconnect-worker-tests\result.json'
$workerResult = Get-Content -LiteralPath $workerPath -Raw | ConvertFrom-Json
$validationPath = Join-Path $projectRoot 'docs\review\r14-validation.json'
$finalValidation = Get-Content -LiteralPath $validationPath -Raw | ConvertFrom-Json
$expectedWorkerChecks = [int]$finalValidation.focused_checks.native_reconnect_helper
if ($finalValidation.revision -ne 'v0.1.1-r14' -or $finalValidation.asi_sha256 -ne $binaryHash -or $expectedWorkerChecks -lt 18) {
    throw 'Finaler r14-Prüfnachweis fehlt oder stimmt nicht mit dem Build überein.'
}
if ($workerResult.failures -ne 0 -or $workerResult.checks -ne $expectedWorkerChecks) { throw 'Reconnect-Helfertest fehlgeschlagen oder stimmt nicht mit dem finalen Prüfnachweis überein.' }
if ($workerResult.nova_cases -ne 6 -or $workerResult.nova_start_requires_spawn_confirmation -ne $true -or $workerResult.cancellation_after_readiness_checked -ne $true) {
    throw 'Die neuen Nova-Start-, Beitritts- und Abbruchfälle sind im Helfertest nicht bestätigt.'
}
if ((Get-Item -LiteralPath $workerPath).LastWriteTimeUtc -lt (Get-Item -LiteralPath $helperPath).LastWriteTimeUtc) {
    throw 'Den nativen Helfertest nach dem letzten Helfer-Build erneut ausführen.'
}
$testLog = Get-Content -LiteralPath (Join-Path $projectRoot 'build\revision14-tests.log') -Raw
if ($testLog -notmatch '(?m)^Test exit code: 0\s*$' -or $testLog -match '\[FAIL\]') { throw 'Gesamttests nicht erfolgreich.' }
$buildLog = Get-Content -LiteralPath (Join-Path $projectRoot 'build\revision14-build.log') -Raw
if ($buildLog -notmatch '(?m)^RELEASE_BUILD_EXIT_CODE=0\s*$' -or $buildLog -notmatch 'Keybinder\.vcxproj -> .*NLBinder\.asi' -or $buildLog -match ': (fatal )?error [A-Z]+\d+:') {
    throw 'Erfolgreicher finaler Release-Build nicht bestätigt.'
}

$packageDirectory = Join-Path $projectRoot 'dist\NLBinder-0.1.1-r14'
$archive = Join-Path $projectRoot 'dist\NLBinder-v0.1.1-KOBiRO-r14.zip'
if ((Test-Path -LiteralPath $packageDirectory) -or (Test-Path -LiteralPath $archive)) {
    throw 'r14-Paket existiert bereits; nicht automatisch überschreiben.'
}
New-Item -ItemType Directory -Path $packageDirectory | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $packageDirectory 'NLBinder.asi')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\NLBinder-0.1.1-r14-DE.md') -Destination (Join-Path $packageDirectory 'LIESMICH.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\jobs-support.md') -Destination (Join-Path $packageDirectory 'JOB-HINWEISE.md')
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $packageDirectory 'PRUEFBERICHT.json')
Copy-Item -LiteralPath $workerPath -Destination (Join-Path $packageDirectory 'RECONNECT-TEST.json')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\CHANGELOG-GESAMT.txt') -Destination (Join-Path $packageDirectory 'CHANGELOG-GESAMT.txt')

# Ship only empty weapon folders and instructions, never personal PNG files.
$catalogSource = Get-Content -LiteralPath (Join-Path $projectRoot 'include\WeaponIconCatalog.h') -Raw
$weaponFolderNames = @([regex]::Matches($catalogSource, '\{\s*\d+,\s*"[^"]+",\s*"([^"]+)"\s*\}') | ForEach-Object { $_.Groups[1].Value })
if ($weaponFolderNames.Count -ne 43 -or @($weaponFolderNames | Sort-Object -Unique).Count -ne 43) { throw 'Der Waffenordner-Katalog ist unvollständig oder doppelt.' }
$templateRoot = Join-Path $packageDirectory 'NLBinder\Waffenicons'
New-Item -ItemType Directory -Path $templateRoot | Out-Null
foreach ($folderName in $weaponFolderNames) {
    if ($folderName -notmatch '^[A-Za-z0-9 -]+$') { throw 'Ungültiger Waffenordner im Katalog.' }
    New-Item -ItemType Directory -Path (Join-Path $templateRoot $folderName) | Out-Null
}
@'
# Eigene Waffenicons

Den Vorlagenordner NLBinder nach Dokumente/GTA San Andreas User Files kopieren
und dort mit dem bestehenden NLBinder-Ordner zusammenführen. Vorhandene eigene
Bilder beibehalten. Die Waffenbilder liegen unter:
Dokumente/GTA San Andreas User Files/NLBinder/Waffenicons/<Waffe>/
PNG-Dateien in den passenden Unterordner legen, z.B. Desert Eagle/mein-icon.png.
Am einfachsten im Binder unter Mods / Waffenicons auf 'Waffenicons-Ordner öffnen'
klicken: So wird auch ein verschobener Dokumente-Ordner, etwa unter OneDrive,
korrekt geöffnet. Danach Waffenicons aktivieren und bei Bedarf neu einlesen.
Mehrere PNGs pro Waffe sind möglich; ohne PNG bleibt das originale Waffenicon.
Die Fist wird weiterhin unter Mods / Hud eingerichtet.
'@ | Set-Content -LiteralPath (Join-Path $templateRoot 'LIESMICH.md') -Encoding utf8

$buildInfo = @"
NLBinder v.0.1.1 by KOBiRO – Überarbeitung 14
Erstellt: $([System.TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'W. Europe Standard Time').ToString('dd.MM.yyyy HH:mm:ss')) (Europe/Berlin)
Datei: NLBinder.asi
Format: Win32 Release, PE32/x86 DLL
Größe: $($report.bytes) Bytes
SHA256: $binaryHash
Eingebetteter Reconnect-Helfer: Ressource 300, $($report.embedded_helper_bytes) Bytes
Helfer-SHA256: $($report.helper_sha256)

Verifikation:
- Vollständige automatisierte Tests: Exitcode 0
- Release-Build: Exitcode 0
- ASI-Format, Windows-Imports und eingebetteter Helfer geprüft
- Eingebetteter Reconnect-Helfer stimmt mit dem geprüften Helfer überein
- Nativer Reconnect-Helfer: $($workerResult.checks) Prüfungen, 0 Fehler
- Dafür ausschließlich harmlose Testprozesse verwendet
- Kein echtes Spiel und kein installierter Launcher gestartet oder beendet
- Kein Ingame-Test durchgeführt

Neue Änderungen:
- Friedhofs-Chat löst keine eigene Todeszählung mehr aus
- Ein echter eigener Alive-zu-Dead-Übergang zählt einmal
- Login/Relog übernimmt eine Basis ohne rückwirkenden Zusatz-Tod
- Damagelog aus r13 mit TakeDamage-/Sync-Fallback bleibt enthalten
- Damagelog-Präfix bleibt {33CCFF}; vorhandene Hitsound-Filter bleiben getrennt
- Manuelle Stats-/Jobskills-Synchronisation und sämtliche bisherigen Anpassungen bleiben enthalten
- Kompilierte Datei heißt weiterhin NLBinder.asi
- Gesamter Changelog liegt als CHANGELOG-GESAMT.txt bei

Die früheren Job-, Skilltree-, Fraktions-, Radar-, Tacho- und Reconnect-
Anpassungen sind enthalten. Die ursprüngliche Oberfläche bleibt Grundlage.
Der Reconnect-Helfer ist in der ASI enthalten.

Grenzen: Die Synchronisation wartet auf eine echte Serverantwort. Bei ausschließlich nativer
Messung können mehrere Verluste zwischen Messungen als Gesamtverlust erscheinen.
Ein Live-Test mit dem konkreten Server und Launcher steht weiterhin aus.
Installation und weitere Hinweise stehen in LIESMICH.md.
"@
$buildInfo | Set-Content -LiteralPath (Join-Path $packageDirectory 'BUILD-INFO.txt') -Encoding utf8
"$binaryHash  NLBinder.asi" | Set-Content -LiteralPath (Join-Path $packageDirectory 'SHA256SUMS.txt') -Encoding ascii
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archive -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$templateZip = [System.IO.Compression.ZipFile]::Open($archive, [System.IO.Compression.ZipArchiveMode]::Update)
try {
    foreach ($folderName in $weaponFolderNames) {
        $entryName = 'NLBinder-0.1.1-r14/NLBinder/Waffenicons/' + $folderName + '/'
        if (-not $templateZip.GetEntry($entryName)) { $null = $templateZip.CreateEntry($entryName) }
    }
} finally { $templateZip.Dispose() }
$zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entry = $zip.Entries | Where-Object { $_.FullName -match '/NLBinder\.asi$' }
    if (@($entry).Count -ne 1) { throw 'ASI fehlt im Archiv oder ist mehrfach enthalten.' }
    $stream = $entry.Open()
    $digest = [System.Security.Cryptography.SHA256]::Create()
    try {
        $zippedHash = [BitConverter]::ToString($digest.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
        if ($zippedHash -ne $binaryHash) { throw 'ASI-Prüfsumme im Archiv stimmt nicht.' }
    } finally { $stream.Dispose(); $digest.Dispose() }
} finally { $zip.Dispose() }
$stagedHash = (Get-FileHash -LiteralPath (Join-Path $packageDirectory 'NLBinder.asi') -Algorithm SHA256).Hash.ToLowerInvariant()
if ($stagedHash -ne $binaryHash) { throw 'Kopierte ASI stimmt nicht überein.' }
Get-Item -LiteralPath (Join-Path $packageDirectory 'NLBinder.asi'), $archive | Select-Object FullName,Length
Write-Output "SHA256: $binaryHash"
