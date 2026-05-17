
# usage sample:
# pwsh -NoProfile -ExecutionPolicy Bypass -File "C:\Temp\_SecLab\SpezialTests\Find-InvalidTextBytes.ps1" -Root "C:\Temp\_SecLab"
# pwsh -NoProfile -ExecutionPolicy Bypass -File "C:\Temp\_SecLab\SpezialTests\Find-InvalidTextBytes.ps1" -Root "C:\Temp\_SecLab" -ReportBom

param(
    [string]$Root = ".",
    [switch]$TrackedOnly,
    [switch]$Json,
    [switch]$ReportBom
)

$ErrorActionPreference = "Stop"

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
    ".git", ".mypy_cache", ".pytest_cache", ".ruff_cache", ".venv", "__pycache__",
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
    } elseif ($first -ge 0xE0 -and $first -le 0xEF) {
        $length = 3
    } elseif ($first -ge 0xF0 -and $first -le 0xF4) {
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
    } catch {
        $text = Format-ByteList $sequence
    }

    [pscustomobject]@{
        Text = $text
        Bytes = Format-ByteList $sequence
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
        } elseif ($codePoint -eq 0xFEFF) {
            [void]$builder.Append("\uFEFF")
        } elseif ($codePoint -lt 32 -or ($codePoint -ge 127 -and $codePoint -le 159)) {
            [void]$builder.Append(("\x{0:X2}" -f $codePoint))
        } else {
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
    } catch {
        try {
            $windows1252 = [System.Text.Encoding]::GetEncoding(1252)
            return $windows1252.GetString($LineBytes)
        } catch {
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
                Offset = $i
                Byte = ("0x{0:X2}" -f $b)
                Character = $invalidCharacter.Text
                CharacterBytes = $invalidCharacter.Bytes
            })
        }
    }

    if ($bad.Count -gt 0) {
        [pscustomobject]@{
            Path = $FullPath
            RelativePath = $RelativePath
            InvalidByteCount = $bad.Count
            InvalidCharacters = @($invalidCharacters.Keys)
            FirstFindings = @($bad | Select-Object -First 10)
        }
    }
}

function Test-AllowedTextSequenceAt {
    param(
        [byte[]]$Bytes,
        [int]$Index,
        [string]$RelativePath
    )

    $germanUmlautLength = Get-GermanUmlautLengthAt $Bytes $Index
    if ($germanUmlautLength -gt 0) {
        return $germanUmlautLength
    }

    # Temperature unit "deg C" written as UTF-8 degree sign plus ASCII C.
    if (
        $Index + 2 -lt $Bytes.Length -and
        $Bytes[$Index] -eq 0xC2 -and
        $Bytes[$Index + 1] -eq 0xB0 -and
        $Bytes[$Index + 2] -eq 0x43
    ) {
        return 2
    }

    # JKsRadio.py uses one UTF-8 play marker for the current station.
    if (
        $RelativePath -eq "theRadio\JKsRadio.py" -and
        $Index + 2 -lt $Bytes.Length -and
        $Bytes[$Index] -eq 0xE2 -and
        $Bytes[$Index + 1] -eq 0x96 -and
        $Bytes[$Index + 2] -eq 0xB6
    ) {
        return 3
    }

    return 0
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path

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
            } else {
                Write-Warning "Tracked path not found in working tree: $_"
            }
        }
} else {
    $files = Get-ChildItem -LiteralPath $rootPath -Recurse -Force -File
}

$scannedFiles = $files | Where-Object {
    $relative = [System.IO.Path]::GetRelativePath($rootPath, $_.FullName)
    $parts = $relative -split '[\\/]'
    foreach ($part in $parts) {
        if ($skipDirs.Contains($part)) {
            return $false
        }
    }
    return $true
}

$pathFindings = foreach ($file in $scannedFiles) {
    $relative = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName)
    Find-InvalidPathBytes $relative $file.FullName
}

$files = $scannedFiles | Where-Object { Test-IsTextCandidate $_ }

$findings = foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $relativePath = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName)
    $line = 1
    $column = 0
    $lineStart = 0
    $bad = New-Object System.Collections.Generic.List[object]
    $invalidCharacters = [ordered]@{}

    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $b = $bytes[$i]

        $ignoredBomLength = Get-IgnoredBomLengthAt $bytes $i
        if ($ignoredBomLength -gt 0) {
            $i += ($ignoredBomLength - 1)
            continue
        }

        if ($b -eq 10) {
            $line++
            $column = 0
            $lineStart = $i + 1
            continue
        }
        $column++

        $allowedSequenceLength = Test-AllowedTextSequenceAt $bytes $i $relativePath
        if ($allowedSequenceLength -gt 0) {
            $i += ($allowedSequenceLength - 1)
            $column += ($allowedSequenceLength - 1)
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
                Offset = $i
                Line = $line
                Column = $column
                Byte = ("0x{0:X2}" -f $b)
                Shown = (Format-Byte $b)
                LineText = (Get-LineTextAt $bytes $lineStart $i)
                Character = $invalidCharacter.Text
                CharacterBytes = $invalidCharacter.Bytes
            })
        }
    }

    if ($bad.Count -gt 0) {
        [pscustomobject]@{
            Path = $file.FullName
            RelativePath = $relativePath
            Length = $bytes.Length
            InvalidByteCount = $bad.Count
            InvalidCharacters = @($invalidCharacters.Keys)
            FirstFindings = @($bad | Select-Object -First 10)
        }
    }
}

if ($Json) {
    [pscustomobject]@{
        ReportBom = [bool]$ReportBom
        ProcessGermanUmlauts = [bool]$ProcessGermanUmlauts
        InvalidPaths = @($pathFindings)
        InvalidContents = @($findings)
    } | ConvertTo-Json -Depth 5
    exit 0
}

if (-not $pathFindings -and -not $findings) {
    Write-Output "OK: no invalid bytes found in text candidates."
    exit 0
}

if ($pathFindings) {
    Write-Output "Invalid path bytes:"
    foreach ($finding in $pathFindings) {
        Write-Output ("{0}: {1} invalid byte(s)" -f $finding.Path, $finding.InvalidByteCount)
        Write-Output ("  invalid chars: {0}" -f (($finding.InvalidCharacters | Select-Object -First 20) -join ", "))
        foreach ($item in $finding.FirstFindings) {
            Write-Output ("  offset {0}: {1} ({2})" -f $item.Offset, $item.Byte, $item.CharacterBytes)
        }
    }
    Write-Output ""
}

if ($findings) {
    Write-Output "Invalid content bytes:"
}
foreach ($finding in $findings) {
    Write-Output ("{0} ({1} bytes): {2} invalid byte(s)" -f $finding.Path, $finding.Length, $finding.InvalidByteCount)
    Write-Output ("  invalid chars: {0}" -f (($finding.InvalidCharacters | Select-Object -First 20) -join ", "))
    $lastPrintedLine = $null
    foreach ($item in $finding.FirstFindings) {
        Write-Output ("  line {0}, col {1}, offset {2}: {3} ({4})" -f $item.Line, $item.Column, $item.Offset, $item.Byte, $item.CharacterBytes)
        if ($lastPrintedLine -ne $item.Line) {
            Write-Output ("    text: {0}" -f $item.LineText)
            $lastPrintedLine = $item.Line
        }
    }
}

exit 1
