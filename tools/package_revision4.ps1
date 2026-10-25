$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $projectRoot 'build\Release\Keybinder.asi'
$reportPath = Join-Path $projectRoot 'build\revision4-artifact.json'
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$binaryHash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
if ($report.revision -ne 'v0.1.1-r4' -or $binaryHash -ne $report.sha256 -or -not $report.embedded_helper_matches) {
    throw 'Die ASI stimmt nicht mit dem finalen r4-Prüfbericht überein.'
}
$helperPath = Join-Path $projectRoot 'build\ReconnectHelper.exe'
$helperHash = (Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($helperHash -ne $report.helper_sha256) { throw 'Der Reconnect-Helfer hat sich seit der Artefaktprüfung geändert.' }
$workerPath = Join-Path $projectRoot 'build\reconnect-worker-tests\result.json'
$workerResult = Get-Content -LiteralPath $workerPath -Raw | ConvertFrom-Json
if ($workerResult.failures -ne 0 -or $workerResult.checks -lt 18) { throw 'Reconnect-Helfertest fehlgeschlagen oder unvollständig.' }
if ((Get-Item -LiteralPath $workerPath).LastWriteTimeUtc -lt (Get-Item -LiteralPath $helperPath).LastWriteTimeUtc) {
    throw 'Den nativen Helfertest nach dem letzten Helfer-Build erneut ausführen.'
}
$testLog = Get-Content -LiteralPath (Join-Path $projectRoot 'build\revision4-tests.log') -Raw
if ($testLog -notmatch '(?m)^Test exit code: 0\s*$' -or $testLog -match '\[FAIL\]') { throw 'Gesamttests nicht erfolgreich.' }
$buildLog = Get-Content -LiteralPath (Join-Path $projectRoot 'build\revision4-build.log') -Raw
if ($buildLog -notmatch '(?m)^RELEASE_BUILD_EXIT_CODE=0\s*$' -or $buildLog -notmatch 'Keybinder\.vcxproj -> .*Keybinder\.asi' -or $buildLog -match ': (fatal )?error [A-Z]+\d+:') {
    throw 'Erfolgreicher finaler Release-Build nicht bestätigt.'
}

$packageDirectory = Join-Path $projectRoot 'dist\NLBinder-0.1.1-r4'
$archive = Join-Path $projectRoot 'dist\NLBinder-v0.1.1-KOBiRO-r4.zip'
if ((Test-Path -LiteralPath $packageDirectory) -or (Test-Path -LiteralPath $archive)) {
    throw 'r4-Paket existiert bereits; nicht automatisch überschreiben.'
}
New-Item -ItemType Directory -Path $packageDirectory | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $packageDirectory 'Keybinder.asi')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\NLBinder-0.1.1-r4-DE.md') -Destination (Join-Path $packageDirectory 'LIESMICH.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\jobs-support.md') -Destination (Join-Path $packageDirectory 'JOB-HINWEISE.md')
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $packageDirectory 'PRUEFBERICHT.json')
Copy-Item -LiteralPath $workerPath -Destination (Join-Path $packageDirectory 'RECONNECT-TEST.json')

$buildInfo = @"
NLBinder v.0.1.1 by KOBiRO – Überarbeitung 4
Erstellt: $([System.TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'W. Europe Standard Time').ToString('dd.MM.yyyy HH:mm:ss')) (Europe/Berlin)
Datei: Keybinder.asi
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
- Damagelog über direkte HP-/Westenmessung, einschließlich Fallschaden
- Getrennte Variablen {city} und {bezirk}; angebotene Doppelungen entfernt
- Kill simulieren sendet den aktuellen Text mit [Testkill] nach dem Chatbefehl
- Einmaliger automatischer /stats-Abruf nach dem sicheren Loginzustand
- Eigener Statistikblock wird gelesen, lokal gespeichert und verborgen
- Statistik-Gesamtzähler werden ohne künstlichen Kill/Tod übernommen
- Motorstart bewertet vollständig erkannten Fahrersitz nach 1,5 Sekunden
- Vorübergehendes EngineOn und alte Chatbestätigungen verwerfen ihn nicht
- Motorstart-Versuchsphase beginnt erst mit dem tatsächlichen Tastendruck

Die früheren Job-, Skilltree-, Fraktions-, Radar-, Tacho- und Reconnect-
Anpassungen sind enthalten. Die ursprüngliche Oberfläche bleibt Grundlage.
Der Reconnect-Helfer ist in der ASI enthalten.

Grenzen: Die Synchronisation wartet auf eine echte Serverantwort. Mehrere
Schadensereignisse zwischen Messungen erscheinen als gemessener Gesamtverlust.
Ein Live-Test mit dem konkreten Server und Launcher steht weiterhin aus.
Installation und weitere Hinweise stehen in LIESMICH.md.
"@
$buildInfo | Set-Content -LiteralPath (Join-Path $packageDirectory 'BUILD-INFO.txt') -Encoding utf8
"$binaryHash  Keybinder.asi" | Set-Content -LiteralPath (Join-Path $packageDirectory 'SHA256SUMS.txt') -Encoding ascii
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archive -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entry = $zip.Entries | Where-Object { $_.FullName -match '/Keybinder\.asi$' }
    if (@($entry).Count -ne 1) { throw 'ASI fehlt im Archiv oder ist mehrfach enthalten.' }
    $stream = $entry.Open()
    $digest = [System.Security.Cryptography.SHA256]::Create()
    try {
        $zippedHash = [BitConverter]::ToString($digest.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
        if ($zippedHash -ne $binaryHash) { throw 'ASI-Prüfsumme im Archiv stimmt nicht.' }
    } finally { $stream.Dispose(); $digest.Dispose() }
} finally { $zip.Dispose() }
$stagedHash = (Get-FileHash -LiteralPath (Join-Path $packageDirectory 'Keybinder.asi') -Algorithm SHA256).Hash.ToLowerInvariant()
if ($stagedHash -ne $binaryHash) { throw 'Kopierte ASI stimmt nicht überein.' }
Get-Item -LiteralPath (Join-Path $packageDirectory 'Keybinder.asi'), $archive | Select-Object FullName,Length
Write-Output "SHA256: $binaryHash"
