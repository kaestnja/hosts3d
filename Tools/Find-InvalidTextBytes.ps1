
<#
.SYNOPSIS
    Detects non-ASCII and problematic byte sequences in text candidates.

.USE
    Typical usage:
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Find-InvalidTextBytes.ps1" -Root "."
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Find-InvalidTextBytes.ps1" -Root "." -TrackedOnly
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Find-InvalidTextBytes.ps1" -Root "." -ReportBom
        pwsh -NoProfile -ExecutionPolicy Bypass -File "C:\Users\kaestnja\source\repos\github.com\kaestnja\hosts3d\Tools\Find-InvalidTextBytes.ps1" -Root "C:\Users\kaestnja\source\repos\github.com\kaestnja\hosts3d"

.DESCRIPTION
    Scans repository files and reports suspicious bytes in path names and file contents.
    The script is intentionally strict for text hygiene and CI use: it exits with code 1
    when findings exist and 0 when everything is clean.

    It supports plain-text output for interactive review and JSON output for automation.
    Optional tracked-only scanning can reduce noise and run time in larger repositories.

.METADATA
    ScriptName: Find-InvalidTextBytes.ps1
    PurposeShort: Find problematic text bytes in public repository content and show exact positions.
    PurposeLong: This script scans selected text files and reports problematic bytes with line and column details. It helps to detect copy-paste artifacts, mixed encodings, and hidden characters before publication or commit. The report can be read by humans or consumed as JSON in automated checks, and the strict findings model supports stable text hygiene across tools, shells, and pipeline steps.
    Inputs: Root, TrackedOnly, Json, ReportBom
    Outputs: Console report or JSON report
    ExitCodes: 0 clean; 1 findings; throws on fatal errors
    SafetyLevel: ReadOnly
    RequiresAdmin: false
    RebootRequired: false
    PublicCategory: diagnostics
    Version: 1.4
    LastUpdated: 2026-06-03
    Owner: Jan Kaestner / CYS

.REQUIREMENTS
    - PowerShell 5.1+ or PowerShell 7+.
    - Read access to the selected root directory.
    - For -TrackedOnly mode: git must be installed and the root must be a git working tree.

.PARAMETER TrackedOnly
    Limit the scan to files returned by `git ls-files` below -Root. This is the
    normal commit-check mode for this repository. It skips untracked, ignored,
    and generated runtime files, so run without -TrackedOnly when you
    intentionally want to inspect local untracked text files too.

.NOTES
    Performance note: the content scanner keeps the expensive UTF-8 exception
    checks out of the common ASCII path. BOM handling runs only at byte 0, and
    German umlaut checks run only when the current byte can start that sequence. This keeps full
    -TrackedOnly scans fast while preserving the strict findings behavior.
#>

[CmdletBinding()]
param(
    [string]$Root = ".",
    [switch]$TrackedOnly,
    [switch]$Json,
    [switch]$ReportBom
)

$ErrorActionPreference = "Stop"
$ScriptDisplayName = 'Find-InvalidTextBytes.ps1'
$ScriptVersion = '1.4'
$UseColor = -not [Console]::IsOutputRedirected

function Write-ConsoleLine {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Message,
        [ValidateSet('Info', 'Success', 'Warning', 'Error', 'Section', 'Muted')][string]$Level = 'Info'
    )

    if (-not $UseColor) {
        Write-Output $Message
        return
    }

    $color = switch ($Level) {
        'Success' { 'Green' }
        'Warning' { 'Yellow' }
        'Error' { 'Red' }
        'Section' { 'Cyan' }
        'Muted' { 'DarkGray' }
        default { 'Gray' }
    }

    Write-Host $Message -ForegroundColor $color
}

function Write-ScriptBanner {
    Write-ConsoleLine ("=== {0} v{1} ===" -f $ScriptDisplayName, $ScriptVersion) -Level Section
    $modeParts = @('scan')
    if ($TrackedOnly) { $modeParts += 'tracked-only' }
    if ($ReportBom) { $modeParts += 'report-bom' }
    Write-ConsoleLine ("Mode: {0}" -f ($modeParts -join ', ')) -Level Muted
}

function Get-RelativePathCompat {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $baseUri = New-Object System.Uri (($BasePath.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar))
    $pathUri = New-Object System.Uri $Path
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

# Set to $true if German umlauts and eszett should be reported as findings.
$ProcessGermanUmlauts = $false

$textExtensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ino",
    ".bat", ".cmd", ".cfg", ".conf", ".csv", ".css", ".desktop", ".env",
    ".gitignore", ".gitattributes", ".html", ".ini", ".js", ".json", ".md",
    ".ps1", ".py", ".qml", ".sample", ".service", ".sh", ".sln", ".sql",
    ".theme", ".toml", ".txt", ".url", ".xml", ".yaml", ".yml"
) | ForEach-Object { [void]$textExtensions.Add($_) }

$textNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    "AGENTS.md", "CODEX_HANDOVER.md", "CODEX_TASK_PROMPTS.md", "CODING_RULES.md",
    "LICENSE", "README", "requirements.txt"
) | ForEach-Object { [void]$textNames.Add($_) }

$ignoredTextNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    "controls.txt", "settings.ini",
    "Find-InvalidTextBytes.ps1", "Repair-InvalidTextBytes.ps1"
) | ForEach-Object { [void]$ignoredTextNames.Add($_) }

$binaryContentExtensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    ".pdf"
) | ForEach-Object { [void]$binaryContentExtensions.Add($_) }

$skipDirs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    ".git", ".mypy_cache", ".pytest_cache", ".rollback", ".ruff_cache", ".venv", "__pycache__",
    "node_modules"
) | ForEach-Object { [void]$skipDirs.Add($_) }

function Test-IsTextCandidate {
    param([System.IO.FileInfo]$File)

    if ($ignoredTextNames.Contains($File.Name)) {
        return $false
    }
    if ($binaryContentExtensions.Contains($File.Extension)) {
        return $false
    }
    if ($textExtensions.Contains($File.Extension)) {
        return $true
    }
    return $textNames.Contains($File.Name)
}

function Format-Byte {
    param([byte]$Byte)

    if ($Byte -ge 32 -and $Byte -le 126) {
        return [string][char]$Byte
    }
    return ("0x{0:X2}" -f $Byte)
}

function Format-ByteList {
    param([byte[]]$Bytes)

    return (($Bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " ")
}

function Get-InvalidCharacterAt {
    param(
        [byte[]]$Bytes,
        [int]$Index
    )

    $first = $Bytes[$Index]
    $length = 1
    if ($first -ge 0xC2 -and $first -le 0xDF) {
        $length = 2
    }
    elseif ($first -ge 0xE0 -and $first -le 0xEF) {
        $length = 3
    }
    elseif ($first -ge 0xF0 -and $first -le 0xF4) {
        $length = 4
    }

    if ($Index + $length -gt $Bytes.Length) {
        $length = 1
    }

    for ($j = 1; $j -lt $length; $j++) {
        $continuation = $Bytes[$Index + $j]
        if ($continuation -lt 0x80 -or $continuation -gt 0xBF) {
            $length = 1
            break
        }
    }

    $sequence = [byte[]]::new($length)
    [Array]::Copy($Bytes, $Index, $sequence, 0, $length)

    try {
        $decoder = [System.Text.UTF8Encoding]::new($false, $true)
        $text = $decoder.GetString($sequence)
    }
    catch {
        $text = Format-ByteList $sequence
    }

    [pscustomobject]@{
        Text   = $text
        Bytes  = Format-ByteList $sequence
        Length = $length
    }
}

function Format-LineTextForOutput {
    param([string]$Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return ""
    }

    $builder = [System.Text.StringBuilder]::new()
    foreach ($character in $Text.ToCharArray()) {
        $codePoint = [int][char]$character
        if ($codePoint -eq 9) {
            [void]$builder.Append("`t")
        }
        elseif ($codePoint -eq 0xFEFF) {
            [void]$builder.Append("\uFEFF")
        }
        elseif ($codePoint -lt 32 -or ($codePoint -ge 127 -and $codePoint -le 159)) {
            [void]$builder.Append(("\x{0:X2}" -f $codePoint))
        }
        else {
            [void]$builder.Append($character)
        }
    }

    return $builder.ToString()
}

function Decode-LineBytes {
    param([byte[]]$LineBytes)

    if ($LineBytes.Length -eq 0) {
        return ""
    }

    try {
        $utf8 = [System.Text.UTF8Encoding]::new($false, $true)
        return $utf8.GetString($LineBytes)
    }
    catch {
        try {
            $windows1252 = [System.Text.Encoding]::GetEncoding(1252)
            return $windows1252.GetString($LineBytes)
        }
        catch {
            return [System.Text.Encoding]::Latin1.GetString($LineBytes)
        }
    }
}

function Get-LineTextAt {
    param(
        [byte[]]$Bytes,
        [int]$LineStart,
        [int]$Index
    )

    $lineEnd = $Index
    while ($lineEnd -lt $Bytes.Length -and $Bytes[$lineEnd] -ne 10 -and $Bytes[$lineEnd] -ne 13) {
        $lineEnd++
    }

    $length = $lineEnd - $LineStart
    if ($length -le 0) {
        return ""
    }

    $lineBytes = [byte[]]::new($length)
    [Array]::Copy($Bytes, $LineStart, $lineBytes, 0, $length)
    return Format-LineTextForOutput (Decode-LineBytes $lineBytes)
}

function Test-AllowedTextByte {
    param([byte]$Byte)

    if ($Byte -eq 9 -or $Byte -eq 10 -or $Byte -eq 13) {
        return $true
    }
    if ($Byte -ge 32 -and $Byte -le 126) {
        return $true
    }
    return $false
}

function Get-GermanUmlautLengthAt {
    param(
        [byte[]]$Bytes,
        [int]$Index
    )

    if ($ProcessGermanUmlauts) {
        return 0
    }

    # UTF-8: ae, oe, ue, Ae, Oe, Ue, eszett.
    if ($Index + 1 -lt $Bytes.Length) {
        if (
            $Bytes[$Index] -eq 0xC3 -and
            @(
                0xA4, 0xB6, 0xBC,
                0x84, 0x96, 0x9C,
                0x9F
            ) -contains $Bytes[$Index + 1]
        ) {
            return 2
        }
    }

    return 0
}

function Get-IgnoredBomLengthAt {
    param(
        [byte[]]$Bytes,
        [int]$Index
    )

    if ($ReportBom -or $Index -ne 0) {
        return 0
    }

    # UTF-32 BOMs first because UTF-32LE starts with the UTF-16LE BOM bytes.
    if (
        $Bytes.Length -ge 4 -and
        $Bytes[0] -eq 0xFF -and
        $Bytes[1] -eq 0xFE -and
        $Bytes[2] -eq 0x00 -and
        $Bytes[3] -eq 0x00
    ) {
        return 4
    }
    if (
        $Bytes.Length -ge 4 -and
        $Bytes[0] -eq 0x00 -and
        $Bytes[1] -eq 0x00 -and
        $Bytes[2] -eq 0xFE -and
        $Bytes[3] -eq 0xFF
    ) {
        return 4
    }

    if (
        $Bytes.Length -ge 3 -and
        $Bytes[0] -eq 0xEF -and
        $Bytes[1] -eq 0xBB -and
        $Bytes[2] -eq 0xBF
    ) {
        return 3
    }

    if (
        $Bytes.Length -ge 2 -and
        $Bytes[0] -eq 0xFF -and
        $Bytes[1] -eq 0xFE
    ) {
        return 2
    }
    if (
        $Bytes.Length -ge 2 -and
        $Bytes[0] -eq 0xFE -and
        $Bytes[1] -eq 0xFF
    ) {
        return 2
    }

    return 0
}

function Find-InvalidPathBytes {
    param(
        [string]$RelativePath,
        [string]$FullPath
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($FullPath)
    $bad = New-Object System.Collections.Generic.List[object]
    $invalidCharacters = [ordered]@{}

    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $b = $bytes[$i]

        $germanUmlautLength = Get-GermanUmlautLengthAt $bytes $i
        if ($germanUmlautLength -gt 0) {
            $i += ($germanUmlautLength - 1)
            continue
        }

        if (-not (Test-AllowedTextByte $b)) {
            $invalidCharacter = Get-InvalidCharacterAt $bytes $i
            if ($invalidCharacter.Length -gt 1 -or $b -lt 0x80 -or $b -gt 0xBF) {
                $key = "{0} [{1}]" -f $invalidCharacter.Text, $invalidCharacter.Bytes
                if (-not $invalidCharacters.Contains($key)) {
                    $invalidCharacters[$key] = $true
                }
            }
            $bad.Add([pscustomobject]@{
                    Offset         = $i
                    Byte           = ("0x{0:X2}" -f $b)
                    Character      = $invalidCharacter.Text
                    CharacterBytes = $invalidCharacter.Bytes
                })
        }
    }

    if ($bad.Count -gt 0) {
        [pscustomobject]@{
            Path              = $FullPath
            RelativePath      = $RelativePath
            InvalidByteCount  = $bad.Count
            InvalidCharacters = @($invalidCharacters.Keys)
            FirstFindings     = @($bad | Select-Object -First 10)
        }
    }
}

function Test-AllowedTextSequenceAt {
    param(
        [byte[]]$Bytes,
        [int]$Index
    )

    $first = $Bytes[$Index]

    if ($first -eq 0xC3) {
        $germanUmlautLength = Get-GermanUmlautLengthAt $Bytes $Index
        if ($germanUmlautLength -gt 0) {
            return $germanUmlautLength
        }
    }

    return 0
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path

if (-not $Json) {
    Write-ScriptBanner
    Write-ConsoleLine ("Root: {0}" -f $rootPath) -Level Muted
    if ($TrackedOnly) {
        Write-ConsoleLine "Collecting tracked files..." -Level Muted
    }
    else {
        Write-ConsoleLine "Collecting files..." -Level Muted
    }
}

if ($TrackedOnly) {
    $tracked = git -C $rootPath -c core.quotePath=false ls-files -z
    if ($LASTEXITCODE -ne 0) {
        throw "git ls-files failed"
    }
    $files = $tracked -split "`0" |
    Where-Object { $_ } |
    ForEach-Object {
        $path = Join-Path $rootPath $_
        if (Test-Path -LiteralPath $path) {
            Get-Item -LiteralPath $path
        }
        else {
            Write-Warning "Tracked path not found in working tree: $_"
        }
    }
}
else {
    $files = Get-ChildItem -LiteralPath $rootPath -Recurse -Force -File
}

$scannedFiles = @($files | Where-Object {
    $relative = Get-RelativePathCompat $rootPath $_.FullName
    $parts = $relative -split '[\\/]'
    foreach ($part in $parts) {
        if ($skipDirs.Contains($part)) {
            return $false
        }
    }
    return $true
})

if (-not $Json) {
    Write-ConsoleLine ("Files considered: {0}" -f $scannedFiles.Count) -Level Muted
    Write-ConsoleLine "Checking path names..." -Level Muted
}

$pathIndex = 0
$pathTotal = $scannedFiles.Count
$pathFindings = foreach ($file in $scannedFiles) {
    $relative = Get-RelativePathCompat $rootPath $file.FullName
    $pathIndex++
    if (-not $Json -and $pathTotal -gt 0) {
        Write-Progress `
            -Activity "Checking path names" `
            -Status ("{0}/{1}: {2}" -f $pathIndex, $pathTotal, $relative) `
            -PercentComplete ([int](($pathIndex / $pathTotal) * 100))
    }
    Find-InvalidPathBytes $relative $file.FullName
}
if (-not $Json -and $pathTotal -gt 0) {
    Write-Progress -Activity "Checking path names" -Completed
}

$files = @($scannedFiles | Where-Object { Test-IsTextCandidate $_ })

if (-not $Json) {
    Write-ConsoleLine ("Text candidates: {0}" -f $files.Count) -Level Muted
    Write-ConsoleLine "Scanning text contents..." -Level Muted
}

$fileIndex = 0
$fileTotal = $files.Count
$findings = foreach ($file in $files) {
    $relativePath = Get-RelativePathCompat $rootPath $file.FullName
    $fileIndex++
    if (-not $Json -and $fileTotal -gt 0) {
        Write-Progress `
            -Activity "Scanning text contents" `
            -Status ("{0}/{1}: {2}" -f $fileIndex, $fileTotal, $relativePath) `
            -PercentComplete ([int](($fileIndex / $fileTotal) * 100))
    }
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $line = 1
    $column = 0
    $lineStart = 0
    $bad = New-Object System.Collections.Generic.List[object]
    $invalidCharacters = [ordered]@{}

    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $b = $bytes[$i]

        if ($i -eq 0) {
            $ignoredBomLength = Get-IgnoredBomLengthAt $bytes $i
            if ($ignoredBomLength -gt 0) {
                $i += ($ignoredBomLength - 1)
                continue
            }
        }

        if ($b -eq 10) {
            $line++
            $column = 0
            $lineStart = $i + 1
            continue
        }
        $column++

        if ($b -eq 9 -or $b -eq 13 -or ($b -ge 32 -and $b -le 126)) {
            continue
        }

        $allowedSequenceLength = Test-AllowedTextSequenceAt $bytes $i
        if ($allowedSequenceLength -gt 0) {
            $i += ($allowedSequenceLength - 1)
            $column += ($allowedSequenceLength - 1)
            continue
        }

        $invalidCharacter = Get-InvalidCharacterAt $bytes $i
        if ($invalidCharacter.Length -gt 1 -or $b -lt 0x80 -or $b -gt 0xBF) {
            $key = "{0} [{1}]" -f $invalidCharacter.Text, $invalidCharacter.Bytes
            if (-not $invalidCharacters.Contains($key)) {
                $invalidCharacters[$key] = $true
            }
        }
        $bad.Add([pscustomobject]@{
                Offset         = $i
                Line           = $line
                Column         = $column
                Byte           = ("0x{0:X2}" -f $b)
                Shown          = (Format-Byte $b)
                LineText       = (Get-LineTextAt $bytes $lineStart $i)
                Character      = $invalidCharacter.Text
                CharacterBytes = $invalidCharacter.Bytes
            })
    }

    if ($bad.Count -gt 0) {
        [pscustomobject]@{
            Path              = $file.FullName
            RelativePath      = $relativePath
            Length            = $bytes.Length
            InvalidByteCount  = $bad.Count
            InvalidCharacters = @($invalidCharacters.Keys)
            FirstFindings     = @($bad | Select-Object -First 10)
        }
    }
}
if (-not $Json -and $fileTotal -gt 0) {
    Write-Progress -Activity "Scanning text contents" -Completed
}

if ($Json) {
    [pscustomobject]@{
        ReportBom            = [bool]$ReportBom
        ProcessGermanUmlauts = [bool]$ProcessGermanUmlauts
        InvalidPaths         = @($pathFindings)
        InvalidContents      = @($findings)
    } | ConvertTo-Json -Depth 5
    exit 0
}

if (-not $pathFindings -and -not $findings) {
    Write-ConsoleLine "OK: no invalid bytes found in text candidates." -Level Success
    Write-ConsoleLine "Summary: 0 path findings, 0 content findings." -Level Muted
    # Keep the user's next interactive command on its own fresh prompt line.
    Write-ConsoleLine "" -Level Info
    exit 0
}

if ($pathFindings) {
    Write-ConsoleLine "Invalid path bytes:" -Level Warning
    foreach ($finding in $pathFindings) {
        Write-ConsoleLine ("{0}: {1} invalid byte(s)" -f $finding.Path, $finding.InvalidByteCount) -Level Error
        Write-ConsoleLine ("  invalid chars: {0}" -f (($finding.InvalidCharacters | Select-Object -First 20) -join ", ")) -Level Muted
        foreach ($item in $finding.FirstFindings) {
            Write-ConsoleLine ("  offset {0}: {1} ({2})" -f $item.Offset, $item.Byte, $item.CharacterBytes) -Level Muted
        }
    }
    Write-ConsoleLine "" -Level Info
}

if ($findings) {
    Write-ConsoleLine "Invalid content bytes:" -Level Warning
}
foreach ($finding in $findings) {
    Write-ConsoleLine ("{0} ({1} bytes): {2} invalid byte(s)" -f $finding.Path, $finding.Length, $finding.InvalidByteCount) -Level Error
    Write-ConsoleLine ("  invalid chars: {0}" -f (($finding.InvalidCharacters | Select-Object -First 20) -join ", ")) -Level Muted
    $lastPrintedLine = $null
    foreach ($item in $finding.FirstFindings) {
        Write-ConsoleLine ("  line {0}, col {1}, offset {2}: {3} ({4})" -f $item.Line, $item.Column, $item.Offset, $item.Byte, $item.CharacterBytes) -Level Muted
        if ($lastPrintedLine -ne $item.Line) {
            Write-ConsoleLine ("    text: {0}" -f $item.LineText) -Level Muted
            $lastPrintedLine = $item.Line
        }
    }
}

$pathCount = @($pathFindings).Count
$contentCount = @($findings).Count
$invalidPathBytes = (@($pathFindings) | Measure-Object -Property InvalidByteCount -Sum).Sum
$invalidContentBytes = (@($findings) | Measure-Object -Property InvalidByteCount -Sum).Sum
if ($null -eq $invalidPathBytes) { $invalidPathBytes = 0 }
if ($null -eq $invalidContentBytes) { $invalidContentBytes = 0 }

Write-ConsoleLine "" -Level Info
Write-ConsoleLine (
    "Summary: {0} path finding(s), {1} content finding(s), {2} invalid path byte(s), {3} invalid content byte(s)." -f
    $pathCount,
    $contentCount,
    $invalidPathBytes,
    $invalidContentBytes
) -Level Warning

# Keep the user's next interactive command on its own fresh prompt line.
Write-ConsoleLine "" -Level Info
exit 1
