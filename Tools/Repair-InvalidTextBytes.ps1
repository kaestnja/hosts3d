# usage samples:
# pwsh -NoProfile -ExecutionPolicy Bypass -File "C:\Temp\_SecLab\SpezialTests\Repair-InvalidTextBytes.ps1" -Root "C:\Temp\_SecLab" -DryRun
# pwsh -NoProfile -ExecutionPolicy Bypass -File "C:\Temp\_SecLab\SpezialTests\Repair-InvalidTextBytes.ps1" -Root "C:\Temp\_SecLab"

param(
    [string]$Root = ".",
    [switch]$TrackedOnly,
    [switch]$Json,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

# Set to $true if German umlauts and eszett should be replaced with ASCII.
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

# Safe automatic replacement table.
# Keep this table ASCII-only so this repair script does not become its own finding.
# Source/Replacement name the character, SourceBytes/ReplacementBytes are the actual byte sequences.
# U+2014 EM DASH (E2 80 94) is intentionally not active yet because it can hide an original "--" option.
$safeReplacementSpecs = @(
    [pscustomobject]@{ Source = "U+2192 RIGHT ARROW";           SourceBytes = "E2 86 92"; Replacement = "->"; ReplacementBytes = "2D 3E" }
    [pscustomobject]@{ Source = "U+2190 LEFT ARROW";            SourceBytes = "E2 86 90"; Replacement = "<-"; ReplacementBytes = "3C 2D" }

    [pscustomobject]@{ Source = "U+201C LEFT DOUBLE QUOTE";     SourceBytes = "E2 80 9C"; Replacement = '"';  ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "U+201D RIGHT DOUBLE QUOTE";    SourceBytes = "E2 80 9D"; Replacement = '"';  ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "U+201E LOW DOUBLE QUOTE";      SourceBytes = "E2 80 9E"; Replacement = '"';  ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "CP1252 LEFT DOUBLE QUOTE";     SourceBytes = "93";       Replacement = '"';  ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "CP1252 RIGHT DOUBLE QUOTE";    SourceBytes = "94";       Replacement = '"';  ReplacementBytes = "22" }

    [pscustomobject]@{ Source = "U+2018 LEFT SINGLE QUOTE";     SourceBytes = "E2 80 98"; Replacement = "'";  ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "U+2019 RIGHT SINGLE QUOTE";    SourceBytes = "E2 80 99"; Replacement = "'";  ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "CP1252 LEFT SINGLE QUOTE";     SourceBytes = "91";       Replacement = "'";  ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "CP1252 RIGHT SINGLE QUOTE";    SourceBytes = "92";       Replacement = "'";  ReplacementBytes = "27" }

    [pscustomobject]@{ Source = "U+2010 HYPHEN";                SourceBytes = "E2 80 90"; Replacement = "-";  ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "U+2011 NON-BREAKING HYPHEN";   SourceBytes = "E2 80 91"; Replacement = "-";  ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "U+2013 EN DASH";               SourceBytes = "E2 80 93"; Replacement = "-";  ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "CP1252 EN DASH";               SourceBytes = "96";       Replacement = "-";  ReplacementBytes = "2D" }
)

# Optional German umlaut replacement table.
# This table is only active when $ProcessGermanUmlauts is set to $true.
$germanUmlautReplacementSpecs = @(
    [pscustomobject]@{ Source = "U+00E4 LATIN SMALL LETTER A WITH DIAERESIS"; SourceBytes = "C3 A4"; Replacement = "ae"; ReplacementBytes = "61 65" }
    [pscustomobject]@{ Source = "U+00F6 LATIN SMALL LETTER O WITH DIAERESIS"; SourceBytes = "C3 B6"; Replacement = "oe"; ReplacementBytes = "6F 65" }
    [pscustomobject]@{ Source = "U+00FC LATIN SMALL LETTER U WITH DIAERESIS"; SourceBytes = "C3 BC"; Replacement = "ue"; ReplacementBytes = "75 65" }
    [pscustomobject]@{ Source = "U+00C4 LATIN CAPITAL LETTER A WITH DIAERESIS"; SourceBytes = "C3 84"; Replacement = "Ae"; ReplacementBytes = "41 65" }
    [pscustomobject]@{ Source = "U+00D6 LATIN CAPITAL LETTER O WITH DIAERESIS"; SourceBytes = "C3 96"; Replacement = "Oe"; ReplacementBytes = "4F 65" }
    [pscustomobject]@{ Source = "U+00DC LATIN CAPITAL LETTER U WITH DIAERESIS"; SourceBytes = "C3 9C"; Replacement = "Ue"; ReplacementBytes = "55 65" }
    [pscustomobject]@{ Source = "U+00DF LATIN SMALL LETTER SHARP S"; SourceBytes = "C3 9F"; Replacement = "ss"; ReplacementBytes = "73 73" }
)

if ($ProcessGermanUmlauts) {
    $safeReplacementSpecs += $germanUmlautReplacementSpecs
}

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

function Convert-HexByteList {
    param([string]$Hex)

    if ([string]::IsNullOrWhiteSpace($Hex)) {
        return ,[byte[]]::new(0)
    }

    $parts = $Hex.Trim() -split "\s+"
    $bytes = New-Object System.Collections.Generic.List[byte]
    foreach ($part in $parts) {
        if ($part -notmatch "^[0-9A-Fa-f]{2}$") {
            throw "Invalid byte token '$part' in hex byte list '$Hex'"
        }
        [void]$bytes.Add([Convert]::ToByte($part, 16))
    }

    return ,[byte[]]$bytes.ToArray()
}

function Format-ByteList {
    param([byte[]]$Bytes)

    if ($Bytes.Length -eq 0) {
        return ""
    }
    return (($Bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " ")
}

function Test-ByteArrayEqual {
    param(
        [byte[]]$Left,
        [byte[]]$Right
    )

    if ($Left.Length -ne $Right.Length) {
        return $false
    }
    for ($i = 0; $i -lt $Left.Length; $i++) {
        if ($Left[$i] -ne $Right[$i]) {
            return $false
        }
    }
    return $true
}

function Get-SafeReplacementRules {
    param([object[]]$Specs)

    $rules = foreach ($spec in $Specs) {
        $sourceBytes = Convert-HexByteList $spec.SourceBytes
        $replacementBytes = Convert-HexByteList $spec.ReplacementBytes
        $replacementBytesFromText = [System.Text.Encoding]::ASCII.GetBytes([string]$spec.Replacement)

        if (-not (Test-ByteArrayEqual $replacementBytes $replacementBytesFromText)) {
            throw "ReplacementBytes for '$($spec.Source)' do not match Replacement text '$($spec.Replacement)'"
        }

        [pscustomobject]@{
            Source = [string]$spec.Source
            SourceBytes = $sourceBytes
            SourceBytesText = Format-ByteList $sourceBytes
            Replacement = [string]$spec.Replacement
            ReplacementBytes = $replacementBytes
            ReplacementBytesText = Format-ByteList $replacementBytes
        }
    }

    return @($rules | Sort-Object @{ Expression = { $_.SourceBytes.Length }; Descending = $true }, Source)
}

function Get-ReplacementRuleIndex {
    param([object[]]$Rules)

    $index = @{}
    foreach ($rule in $Rules) {
        $firstByte = [int]$rule.SourceBytes[0]
        if (-not $index.ContainsKey($firstByte)) {
            $index[$firstByte] = [System.Collections.Generic.List[object]]::new()
        }
        [void]$index[$firstByte].Add($rule)
    }

    return $index
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
            return [System.Text.Encoding]::GetEncoding(28591).GetString($LineBytes)
        }
    }
}

function Get-LineBytesAt {
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
        return ,[byte[]]::new(0)
    }

    $lineBytes = [byte[]]::new($length)
    [Array]::Copy($Bytes, $LineStart, $lineBytes, 0, $length)
    return ,[byte[]]$lineBytes
}

function Test-ByteSequenceAt {
    param(
        [byte[]]$Bytes,
        [int]$Index,
        [byte[]]$Sequence
    )

    if ($Sequence.Length -eq 0 -or $Index + $Sequence.Length -gt $Bytes.Length) {
        return $false
    }

    for ($j = 0; $j -lt $Sequence.Length; $j++) {
        if ($Bytes[$Index + $j] -ne $Sequence[$j]) {
            return $false
        }
    }
    return $true
}

function Get-MatchingReplacementRule {
    param(
        [byte[]]$Bytes,
        [int]$Index,
        [hashtable]$RuleIndex
    )

    $candidates = $RuleIndex[[int]$Bytes[$Index]]
    if ($null -eq $candidates) {
        return $null
    }

    foreach ($rule in $candidates) {
        if (Test-ByteSequenceAt $Bytes $Index $rule.SourceBytes) {
            return $rule
        }
    }
    return $null
}

function Convert-BytesWithReplacementRules {
    param(
        [byte[]]$Bytes,
        [hashtable]$RuleIndex
    )

    $result = New-Object System.Collections.Generic.List[byte]
    $i = 0
    while ($i -lt $Bytes.Length) {
        $rule = Get-MatchingReplacementRule $Bytes $i $RuleIndex
        if ($null -ne $rule) {
            foreach ($byte in $rule.ReplacementBytes) {
                [void]$result.Add($byte)
            }
            $i += $rule.SourceBytes.Length
            continue
        }

        [void]$result.Add($Bytes[$i])
        $i++
    }

    return ,[byte[]]$result.ToArray()
}

function Get-LineTextAt {
    param(
        [byte[]]$Bytes,
        [int]$LineStart,
        [int]$Index
    )

    $lineBytes = Get-LineBytesAt $Bytes $LineStart $Index
    return Format-LineTextForOutput (Decode-LineBytes $lineBytes)
}

function Get-RepairedLineTextAt {
    param(
        [byte[]]$Bytes,
        [int]$LineStart,
        [int]$Index,
        [hashtable]$RuleIndex
    )

    $lineBytes = Get-LineBytesAt $Bytes $LineStart $Index
    $repairedLineBytes = Convert-BytesWithReplacementRules $lineBytes $RuleIndex
    return Format-LineTextForOutput (Decode-LineBytes $repairedLineBytes)
}

function Repair-FileBytes {
    param(
        [byte[]]$Bytes,
        [hashtable]$RuleIndex
    )

    $result = New-Object System.Collections.Generic.List[byte]
    $changes = New-Object System.Collections.Generic.List[object]
    $line = 1
    $column = 0
    $lineStart = 0
    $i = 0

    while ($i -lt $Bytes.Length) {
        $b = $Bytes[$i]
        if ($b -eq 10) {
            [void]$result.Add($b)
            $line++
            $column = 0
            $lineStart = $i + 1
            $i++
            continue
        }

        $column++
        $rule = Get-MatchingReplacementRule $Bytes $i $RuleIndex
        if ($null -ne $rule) {
            foreach ($byte in $rule.ReplacementBytes) {
                [void]$result.Add($byte)
            }

            $changes.Add([pscustomobject]@{
                Offset = $i
                Line = $line
                Column = $column
                Source = $rule.Source
                SourceBytes = $rule.SourceBytesText
                Replacement = $rule.Replacement
                ReplacementBytes = $rule.ReplacementBytesText
                BeforeLine = (Get-LineTextAt $Bytes $lineStart $i)
                AfterLine = (Get-RepairedLineTextAt $Bytes $lineStart $i $RuleIndex)
            })

            $i += $rule.SourceBytes.Length
            $column += ($rule.SourceBytes.Length - 1)
            continue
        }

        [void]$result.Add($b)
        $i++
    }

    $repairedBytes = [byte[]]$result.ToArray()
    $changeArray = [object[]]$changes.ToArray()

    [pscustomobject]@{
        Bytes = [object]$repairedBytes
        Changes = $changeArray
    }
}

$safeReplacementRules = Get-SafeReplacementRules $safeReplacementSpecs
$safeReplacementRuleIndex = Get-ReplacementRuleIndex $safeReplacementRules
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

$textFiles = $scannedFiles | Where-Object { Test-IsTextCandidate $_ }

$results = foreach ($file in $textFiles) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $repair = Repair-FileBytes $bytes $safeReplacementRuleIndex

    if ($repair.Changes.Count -gt 0) {
        if (-not $DryRun) {
            [System.IO.File]::WriteAllBytes($file.FullName, $repair.Bytes)
        }

        [pscustomobject]@{
            Path = $file.FullName
            RelativePath = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName)
            LengthBefore = $bytes.Length
            LengthAfter = $repair.Bytes.Length
            ReplacementCount = $repair.Changes.Count
            Applied = (-not $DryRun)
            Changes = @($repair.Changes)
        }
    }
}

if ($Json) {
    [pscustomobject]@{
        DryRun = [bool]$DryRun
        ProcessGermanUmlauts = [bool]$ProcessGermanUmlauts
        ReplacementRules = @($safeReplacementRules | Select-Object Source, SourceBytesText, Replacement, ReplacementBytesText)
        RepairedFiles = @($results)
    } | ConvertTo-Json -Depth 6
    exit 0
}

if (-not $results) {
    Write-Output "OK: no safe replacements found."
    exit 0
}

$modeText = if ($DryRun) { "would apply" } else { "applied" }
Write-Output ("Safe replacements {0}:" -f $modeText)
foreach ($result in $results) {
    Write-Output ("{0} ({1} -> {2} bytes): {3} replacement(s) {4}" -f $result.Path, $result.LengthBefore, $result.LengthAfter, $result.ReplacementCount, $modeText)
    $lastLineKey = $null
    foreach ($change in $result.Changes) {
        Write-Output ("  line {0}, col {1}, offset {2}: {3} [{4}] -> {5} [{6}]" -f $change.Line, $change.Column, $change.Offset, $change.Source, $change.SourceBytes, $change.Replacement, $change.ReplacementBytes)
        $lineKey = "{0}:{1}" -f $change.Line, $change.BeforeLine
        if ($lineKey -ne $lastLineKey) {
            Write-Output ("    before: {0}" -f $change.BeforeLine)
            Write-Output ("    after : {0}" -f $change.AfterLine)
            $lastLineKey = $lineKey
        }
    }
}

$fileCount = @($results).Count
$replacementCount = (@($results) | Measure-Object -Property ReplacementCount -Sum).Sum
Write-Output ""
Write-Output ("Summary: {0} file(s), {1} replacement(s) {2}." -f $fileCount, $replacementCount, $modeText)

exit 0
