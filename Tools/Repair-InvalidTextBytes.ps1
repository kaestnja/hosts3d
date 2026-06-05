<#
.SYNOPSIS
    Applies safe byte-level replacements to normalize text content.

.USE
    Typical usage:
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Repair-InvalidTextBytes.ps1" -Root "." -DryRun
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Repair-InvalidTextBytes.ps1" -Root "." -TrackedOnly -DryRun
        pwsh -NoProfile -ExecutionPolicy Bypass -File ".\Repair-InvalidTextBytes.ps1" -Root "."

.DESCRIPTION
    Scans text candidates and replaces a curated set of unsafe typographic bytes with
    ASCII-safe alternatives. The replacement table is explicit and validated at startup.

    Run with -DryRun first to preview all changes. Without -DryRun, files are rewritten
    in place and a detailed before/after report is printed.

.METADATA
    ScriptName: Repair-InvalidTextBytes.ps1
    PurposeShort: Repair known problematic text bytes by applying a controlled safe replacement table.
    PurposeLong: This script rewrites selected files by replacing known problematic byte sequences with ASCII-safe text. It is designed for reproducible cleanup and gives a detailed change log per file and per line. DryRun mode allows a safe preview before any write operation, and the conservative replacement rules are intended to keep technical text, command options, and code snippets predictable.
    Inputs: Root, TrackedOnly, Json, DryRun
    Outputs: Console report or JSON report
    ExitCodes: 0 success/no changes; throws on fatal errors
    SafetyLevel: ModifiesFiles
    RequiresAdmin: false
    RebootRequired: false
    PublicCategory: encoding
    Version: 1.4
    LastUpdated: 2026-06-03
    Owner: Jan Kaestner / CYS

.REQUIREMENTS
    - PowerShell 5.1+ or PowerShell 7+.
    - Write access to files below the selected root directory.
    - For -TrackedOnly mode: git must be installed and the root must be a git working tree.
    - Use -DryRun first to validate changes before writing files.

.PARAMETER TrackedOnly
    Limit the repair scan to files returned by `git ls-files` below -Root. This
    is the normal commit-cleanup preview mode for this repository when combined
    with -DryRun. It skips untracked, ignored, and generated runtime files, so
    run without -TrackedOnly when you intentionally want to inspect local
    untracked text files too.

.NOTES
    Performance and safety note: the repair scan keeps common ASCII bytes on a
    fast path and only builds replacement output after the first real change in
    a file. Unknown valid UTF-8 sequences are preserved before single-byte
    CP1252 repairs are considered, so multi-byte text is not damaged.
#>

[CmdletBinding()]
param(
    [string]$Root = ".",
    [switch]$TrackedOnly,
    [switch]$Json,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$ScriptDisplayName = 'Repair-InvalidTextBytes.ps1'
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
    $modeParts = @('repair')
    if ($DryRun) { $modeParts += 'dry-run' }
    if ($TrackedOnly) { $modeParts += 'tracked-only' }
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
    ".git", ".mypy_cache", ".pytest_cache", ".rollback", ".ruff_cache", ".venv", "__pycache__",
    "node_modules"
) | ForEach-Object { [void]$skipDirs.Add($_) }

# Safe automatic replacement table.
# Keep this table ASCII-only so this repair script does not become its own finding.
# Source/Replacement name the character, SourceBytes/ReplacementBytes are the actual byte sequences.
# U+2014 EM DASH (E2 80 94) is intentionally not active yet because it can hide an original "--" option.
$safeReplacementSpecs = @(
    [pscustomobject]@{ Source = "U+00A0 NO-BREAK SPACE"; SourceBytes = "C2 A0"; Replacement = " "; ReplacementBytes = "20" }
    [pscustomobject]@{ Source = "U+00AD SOFT HYPHEN"; SourceBytes = "C2 AD"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+FEFF ZERO WIDTH NO-BREAK SPACE"; SourceBytes = "EF BB BF"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+200B ZERO WIDTH SPACE"; SourceBytes = "E2 80 8B"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+200C ZERO WIDTH NON-JOINER"; SourceBytes = "E2 80 8C"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+200D ZERO WIDTH JOINER"; SourceBytes = "E2 80 8D"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+200E LEFT-TO-RIGHT MARK"; SourceBytes = "E2 80 8E"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+200F RIGHT-TO-LEFT MARK"; SourceBytes = "E2 80 8F"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+202A LEFT-TO-RIGHT EMBEDDING"; SourceBytes = "E2 80 AA"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+202B RIGHT-TO-LEFT EMBEDDING"; SourceBytes = "E2 80 AB"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+202C POP DIRECTIONAL FORMATTING"; SourceBytes = "E2 80 AC"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+202D LEFT-TO-RIGHT OVERRIDE"; SourceBytes = "E2 80 AD"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+202E RIGHT-TO-LEFT OVERRIDE"; SourceBytes = "E2 80 AE"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+2060 WORD JOINER"; SourceBytes = "E2 81 A0"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+2066 LEFT-TO-RIGHT ISOLATE"; SourceBytes = "E2 81 A6"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+2067 RIGHT-TO-LEFT ISOLATE"; SourceBytes = "E2 81 A7"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+2068 FIRST STRONG ISOLATE"; SourceBytes = "E2 81 A8"; Replacement = ""; ReplacementBytes = "" }
    [pscustomobject]@{ Source = "U+2069 POP DIRECTIONAL ISOLATE"; SourceBytes = "E2 81 A9"; Replacement = ""; ReplacementBytes = "" }

    [pscustomobject]@{ Source = "U+2192 RIGHT ARROW"; SourceBytes = "E2 86 92"; Replacement = "->"; ReplacementBytes = "2D 3E" }
    [pscustomobject]@{ Source = "U+2190 LEFT ARROW"; SourceBytes = "E2 86 90"; Replacement = "<-"; ReplacementBytes = "3C 2D" }

    [pscustomobject]@{ Source = "U+201C LEFT DOUBLE QUOTE"; SourceBytes = "E2 80 9C"; Replacement = '"'; ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "U+201D RIGHT DOUBLE QUOTE"; SourceBytes = "E2 80 9D"; Replacement = '"'; ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "U+201E LOW DOUBLE QUOTE"; SourceBytes = "E2 80 9E"; Replacement = '"'; ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "CP1252 LEFT DOUBLE QUOTE"; SourceBytes = "93"; Replacement = '"'; ReplacementBytes = "22" }
    [pscustomobject]@{ Source = "CP1252 RIGHT DOUBLE QUOTE"; SourceBytes = "94"; Replacement = '"'; ReplacementBytes = "22" }

    [pscustomobject]@{ Source = "U+2018 LEFT SINGLE QUOTE"; SourceBytes = "E2 80 98"; Replacement = "'"; ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "U+2019 RIGHT SINGLE QUOTE"; SourceBytes = "E2 80 99"; Replacement = "'"; ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "CP1252 LEFT SINGLE QUOTE"; SourceBytes = "91"; Replacement = "'"; ReplacementBytes = "27" }
    [pscustomobject]@{ Source = "CP1252 RIGHT SINGLE QUOTE"; SourceBytes = "92"; Replacement = "'"; ReplacementBytes = "27" }

    [pscustomobject]@{ Source = "U+2010 HYPHEN"; SourceBytes = "E2 80 90"; Replacement = "-"; ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "U+2011 NON-BREAKING HYPHEN"; SourceBytes = "E2 80 91"; Replacement = "-"; ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "U+2013 EN DASH"; SourceBytes = "E2 80 93"; Replacement = "-"; ReplacementBytes = "2D" }
    [pscustomobject]@{ Source = "CP1252 EN DASH"; SourceBytes = "96"; Replacement = "-"; ReplacementBytes = "2D" }
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
        return , [byte[]]::new(0)
    }

    $parts = $Hex.Trim() -split "\s+"
    $bytes = New-Object System.Collections.Generic.List[byte]
    foreach ($part in $parts) {
        if ($part -notmatch "^[0-9A-Fa-f]{2}$") {
            throw "Invalid byte token '$part' in hex byte list '$Hex'"
        }
        [void]$bytes.Add([Convert]::ToByte($part, 16))
    }

    return , [byte[]]$bytes.ToArray()
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
            Source               = [string]$spec.Source
            SourceBytes          = $sourceBytes
            SourceBytesText      = Format-ByteList $sourceBytes
            Replacement          = [string]$spec.Replacement
            ReplacementBytes     = $replacementBytes
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
        return , [byte[]]::new(0)
    }

    $lineBytes = [byte[]]::new($length)
    [Array]::Copy($Bytes, $LineStart, $lineBytes, 0, $length)
    return , [byte[]]$lineBytes
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

function Get-ValidUtf8SequenceLengthAt {
    param(
        [byte[]]$Bytes,
        [int]$Index
    )

    $first = $Bytes[$Index]
    $length = 0
    if ($first -ge 0xC2 -and $first -le 0xDF) {
        $length = 2
    }
    elseif ($first -ge 0xE0 -and $first -le 0xEF) {
        $length = 3
    }
    elseif ($first -ge 0xF0 -and $first -le 0xF4) {
        $length = 4
    }

    if ($length -eq 0 -or $Index + $length -gt $Bytes.Length) {
        return 0
    }

    for ($j = 1; $j -lt $length; $j++) {
        $continuation = $Bytes[$Index + $j]
        if ($continuation -lt 0x80 -or $continuation -gt 0xBF) {
            return 0
        }
    }

    return $length
}

function Convert-BytesWithReplacementRules {
    param(
        [byte[]]$Bytes,
        [hashtable]$RuleIndex
    )

    $result = New-Object System.Collections.Generic.List[byte]
    $i = 0
    while ($i -lt $Bytes.Length) {
        if (
            $i -eq 0 -and
            $Bytes.Length -ge 3 -and
            $Bytes[0] -eq 0xEF -and
            $Bytes[1] -eq 0xBB -and
            $Bytes[2] -eq 0xBF
        ) {
            [void]$result.Add($Bytes[0])
            [void]$result.Add($Bytes[1])
            [void]$result.Add($Bytes[2])
            $i += 3
            continue
        }

        $b = $Bytes[$i]
        if ($b -eq 9 -or $b -eq 10 -or $b -eq 13 -or ($b -ge 32 -and $b -le 126)) {
            [void]$result.Add($b)
            $i++
            continue
        }

        $rule = $null
        if ($RuleIndex.ContainsKey([int]$b)) {
            $rule = Get-MatchingReplacementRule $Bytes $i $RuleIndex
        }
        if ($null -ne $rule) {
            foreach ($byte in $rule.ReplacementBytes) {
                [void]$result.Add($byte)
            }
            $i += $rule.SourceBytes.Length
            continue
        }

        $validUtf8Length = Get-ValidUtf8SequenceLengthAt $Bytes $i
        if ($validUtf8Length -gt 0) {
            for ($j = 0; $j -lt $validUtf8Length; $j++) {
                [void]$result.Add($Bytes[$i + $j])
            }
            $i += $validUtf8Length
            continue
        }

        [void]$result.Add($Bytes[$i])
        $i++
    }

    return , [byte[]]$result.ToArray()
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

    $result = $null
    $changes = New-Object System.Collections.Generic.List[object]
    $line = 1
    $column = 0
    $lineStart = 0
    $i = 0

    while ($i -lt $Bytes.Length) {
        if (
            $i -eq 0 -and
            $Bytes.Length -ge 3 -and
            $Bytes[0] -eq 0xEF -and
            $Bytes[1] -eq 0xBB -and
            $Bytes[2] -eq 0xBF
        ) {
            if ($null -ne $result) {
                [void]$result.Add($Bytes[0])
                [void]$result.Add($Bytes[1])
                [void]$result.Add($Bytes[2])
            }
            $i += 3
            continue
        }

        $b = $Bytes[$i]
        if ($b -eq 10) {
            if ($null -ne $result) {
                [void]$result.Add($b)
            }
            $line++
            $column = 0
            $lineStart = $i + 1
            $i++
            continue
        }

        $column++
        if ($b -eq 9 -or $b -eq 13 -or ($b -ge 32 -and $b -le 126)) {
            if ($null -ne $result) {
                [void]$result.Add($b)
            }
            $i++
            continue
        }

        $rule = $null
        if ($RuleIndex.ContainsKey([int]$b)) {
            $rule = Get-MatchingReplacementRule $Bytes $i $RuleIndex
        }
        if ($null -ne $rule) {
            if ($null -eq $result) {
                $result = New-Object System.Collections.Generic.List[byte]
                for ($copyIndex = 0; $copyIndex -lt $i; $copyIndex++) {
                    [void]$result.Add($Bytes[$copyIndex])
                }
            }
            foreach ($byte in $rule.ReplacementBytes) {
                [void]$result.Add($byte)
            }

            $changes.Add([pscustomobject]@{
                    Offset           = $i
                    Line             = $line
                    Column           = $column
                    Source           = $rule.Source
                    SourceBytes      = $rule.SourceBytesText
                    Replacement      = $rule.Replacement
                    ReplacementBytes = $rule.ReplacementBytesText
                    BeforeLine       = (Get-LineTextAt $Bytes $lineStart $i)
                    AfterLine        = (Get-RepairedLineTextAt $Bytes $lineStart $i $RuleIndex)
                })

            $i += $rule.SourceBytes.Length
            $column += ($rule.SourceBytes.Length - 1)
            continue
        }

        $validUtf8Length = Get-ValidUtf8SequenceLengthAt $Bytes $i
        if ($validUtf8Length -gt 0) {
            if ($null -ne $result) {
                for ($j = 0; $j -lt $validUtf8Length; $j++) {
                    [void]$result.Add($Bytes[$i + $j])
                }
            }
            $i += $validUtf8Length
            $column += ($validUtf8Length - 1)
            continue
        }

        if ($null -ne $result) {
            [void]$result.Add($b)
        }
        $i++
    }

    if ($null -eq $result) {
        $repairedBytes = $Bytes
    }
    else {
        $repairedBytes = [byte[]]$result.ToArray()
    }
    $changeArray = [object[]]$changes.ToArray()

    [pscustomobject]@{
        Bytes   = [object]$repairedBytes
        Changes = $changeArray
    }
}

$safeReplacementRules = Get-SafeReplacementRules $safeReplacementSpecs
$safeReplacementRuleIndex = Get-ReplacementRuleIndex $safeReplacementRules
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
}

$textFiles = @($scannedFiles | Where-Object { Test-IsTextCandidate $_ })

if (-not $Json) {
    Write-ConsoleLine ("Text candidates: {0}" -f $textFiles.Count) -Level Muted
    Write-ConsoleLine "Scanning text contents..." -Level Muted
}

$fileIndex = 0
$fileTotal = $textFiles.Count
$results = foreach ($file in $textFiles) {
    $relativePath = Get-RelativePathCompat $rootPath $file.FullName
    $fileIndex++
    if (-not $Json -and $fileTotal -gt 0) {
        Write-Progress `
            -Activity "Scanning text contents" `
            -Status ("{0}/{1}: {2}" -f $fileIndex, $fileTotal, $relativePath) `
            -PercentComplete ([int](($fileIndex / $fileTotal) * 100))
    }
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $repair = Repair-FileBytes $bytes $safeReplacementRuleIndex

    if ($repair.Changes.Count -gt 0) {
        if (-not $DryRun) {
            [System.IO.File]::WriteAllBytes($file.FullName, $repair.Bytes)
        }

        [pscustomobject]@{
            Path             = $file.FullName
            RelativePath     = $relativePath
            LengthBefore     = $bytes.Length
            LengthAfter      = $repair.Bytes.Length
            ReplacementCount = $repair.Changes.Count
            Applied          = (-not $DryRun)
            Changes          = @($repair.Changes)
        }
    }
}
if (-not $Json -and $fileTotal -gt 0) {
    Write-Progress -Activity "Scanning text contents" -Completed
}

if ($Json) {
    [pscustomobject]@{
        DryRun               = [bool]$DryRun
        ProcessGermanUmlauts = [bool]$ProcessGermanUmlauts
        ReplacementRules     = @($safeReplacementRules | Select-Object Source, SourceBytesText, Replacement, ReplacementBytesText)
        RepairedFiles        = @($results)
    } | ConvertTo-Json -Depth 6
    exit 0
}

if (-not $results) {
    Write-ConsoleLine "OK: no safe replacements found." -Level Success
    Write-ConsoleLine "Summary: 0 file(s), 0 replacement(s)." -Level Muted
    # Keep the user's next interactive command on its own fresh prompt line.
    Write-ConsoleLine "" -Level Info
    exit 0
}

$modeText = if ($DryRun) { "would apply" } else { "applied" }
Write-ConsoleLine ("Safe replacements {0}:" -f $modeText) -Level Warning
foreach ($result in $results) {
    Write-ConsoleLine ("{0} ({1} -> {2} bytes): {3} replacement(s) {4}" -f $result.Path, $result.LengthBefore, $result.LengthAfter, $result.ReplacementCount, $modeText) -Level Info
    $lastLineKey = $null
    foreach ($change in $result.Changes) {
        Write-ConsoleLine ("  line {0}, col {1}, offset {2}: {3} [{4}] -> {5} [{6}]" -f $change.Line, $change.Column, $change.Offset, $change.Source, $change.SourceBytes, $change.Replacement, $change.ReplacementBytes) -Level Muted
        $lineKey = "{0}:{1}" -f $change.Line, $change.BeforeLine
        if ($lineKey -ne $lastLineKey) {
            Write-ConsoleLine ("    before: {0}" -f $change.BeforeLine) -Level Muted
            Write-ConsoleLine ("    after : {0}" -f $change.AfterLine) -Level Muted
            $lastLineKey = $lineKey
        }
    }
}

$fileCount = @($results).Count
$replacementCount = (@($results) | Measure-Object -Property ReplacementCount -Sum).Sum
if ($null -eq $replacementCount) { $replacementCount = 0 }

Write-ConsoleLine "" -Level Info
Write-ConsoleLine ("Summary: {0} file(s), {1} replacement(s) {2}." -f $fileCount, $replacementCount, $modeText) -Level Warning
if ($DryRun) {
    Write-ConsoleLine "No files were modified because DryRun is active." -Level Muted
}
else {
    Write-ConsoleLine "File changes were written to disk." -Level Success
}

# Keep the user's next interactive command on its own fresh prompt line.
Write-ConsoleLine "" -Level Info
exit 0
