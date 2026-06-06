param(
  [string]$OutFile = "build/sarif/clang-tidy.sarif",
  [string[]]$Files = @(
    "src/llist.cpp",
    "src/misc.cpp",
    "src/proto.cpp",
    "src/glwin.cpp",
    "src/objects.cpp",
    "src/hsen.cpp",
    "src/hosts3d.cpp"
  ),
  [string]$Checks = "-*,clang-analyzer-*,bugprone-assignment-in-if-condition,bugprone-narrowing-conversions,bugprone-sizeof-expression,bugprone-suspicious-missing-comma,bugprone-string-constructor,bugprone-use-after-move,performance-*"
)

$ErrorActionPreference = "Stop"

$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy) {
  throw "clang-tidy was not found in PATH. Install LLVM or add clang-tidy.exe to PATH."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$mingwRoot = "C:/msys64/mingw32"
$mingwRootFs = "C:\msys64\mingw32"
$cppInclude = Get-ChildItem (Join-Path $mingwRootFs "include\c++") -Directory -ErrorAction SilentlyContinue |
  Sort-Object Name -Descending |
  Select-Object -First 1
$gccInclude = Get-ChildItem (Join-Path $mingwRootFs "lib\gcc\i686-w64-mingw32") -Directory -ErrorAction SilentlyContinue |
  Sort-Object Name -Descending |
  Select-Object -First 1

$outPath = if ([System.IO.Path]::IsPathRooted($OutFile)) {
  $OutFile
} else {
  Join-Path $repoRoot $OutFile
}
$outDir = Split-Path -Parent $outPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$commonArgs = @(
  "-checks=$Checks",
  "--extra-arg=--target=i686-w64-windows-gnu",
  "--extra-arg=-D__MINGW32__",
  "--extra-arg=-DWIN32",
  "--extra-arg=-D_WIN32",
  "--extra-arg=-Isrc",
  "--extra-arg=-I$mingwRoot/include",
  "--extra-arg=-Ithird_party/wpcap/include",
  "--extra-arg=-I$mingwRoot/include/GLFW",
  "--"
)
if ($cppInclude) {
  $cppIncludePath = ($cppInclude.FullName -replace "\\", "/")
  $commonArgs = $commonArgs[0..7] + @(
    "--extra-arg=-I$cppIncludePath",
    "--extra-arg=-I$cppIncludePath/i686-w64-mingw32"
  ) + $commonArgs[8..($commonArgs.Count - 1)]
}
if ($gccInclude) {
  $gccIncludePath = (($gccInclude.FullName -replace "\\", "/") + "/include")
  $commonArgs = $commonArgs[0..7] + @("--extra-arg=-I$gccIncludePath") + $commonArgs[8..($commonArgs.Count - 1)]
}
$compileArgs = @("-std=gnu++11")

$allOutput = New-Object System.Collections.Generic.List[string]
foreach ($file in $Files) {
  $path = Join-Path $repoRoot $file
  if (-not (Test-Path $path)) {
    Write-Warning "Skipping missing file: $file"
    continue
  }

  Write-Host "Scanning $file"
  $clangArgs = @($file) + $commonArgs + $compileArgs
  Push-Location $repoRoot
  try {
    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $clangTidy.Source @clangArgs 2>&1 | ForEach-Object { $_.ToString() }
    $ErrorActionPreference = $oldErrorActionPreference
    foreach ($line in $output) {
      $allOutput.Add($line)
    }
  } finally {
    $ErrorActionPreference = "Stop"
    Pop-Location
  }
}

$rulesById = @{}
$results = New-Object System.Collections.Generic.List[object]
$diagnosticPattern = "^(.+):(\d+):(\d+):\s+(warning|error):\s+(.+?)(?:\s+\[([^\]]+)\])?$"

foreach ($line in $allOutput) {
  $match = [regex]::Match($line, $diagnosticPattern)
  if (-not $match.Success) {
    continue
  }

  $path = $match.Groups[1].Value
  $levelText = $match.Groups[4].Value
  $message = $match.Groups[5].Value
  $ruleId = $match.Groups[6].Value
  if ([string]::IsNullOrWhiteSpace($ruleId)) {
    $ruleId = if ($levelText -eq "error") { "clang-diagnostic-error" } else { "clang-diagnostic-warning" }
  }

  if (-not $rulesById.ContainsKey($ruleId)) {
    $rulesById[$ruleId] = [ordered]@{
      id = $ruleId
      shortDescription = [ordered]@{
        text = $ruleId
      }
    }
  }

  $fullPath = if ([System.IO.Path]::IsPathRooted($path)) {
    $path
  } else {
    Join-Path $repoRoot $path
  }
  $resolvedFullPath = [System.IO.Path]::GetFullPath($fullPath)
  if (-not $resolvedFullPath.StartsWith($repoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    continue
  }
  $uri = "file:///" + (($resolvedFullPath -replace "\\", "/") -replace "^([A-Za-z]):", '$1:')

  $results.Add([ordered]@{
    ruleId = $ruleId
    level = if ($levelText -eq "error") { "error" } else { "warning" }
    message = [ordered]@{
      text = $message
    }
    locations = @(
      [ordered]@{
        physicalLocation = [ordered]@{
          artifactLocation = [ordered]@{
            uri = $uri
          }
          region = [ordered]@{
            startLine = [int]$match.Groups[2].Value
            startColumn = [int]$match.Groups[3].Value
          }
        }
      }
    )
  })
}

$sarif = [ordered]@{
  version = "2.1.0"
  '$schema' = "https://json.schemastore.org/sarif-2.1.0.json"
  runs = @(
    [ordered]@{
      tool = [ordered]@{
        driver = [ordered]@{
          name = "clang-tidy"
          informationUri = "https://clang.llvm.org/extra/clang-tidy/"
          rules = @($rulesById.Values | ForEach-Object { $_ })
        }
      }
      results = @($results | ForEach-Object { $_ })
    }
  )
}

$sarif | ConvertTo-Json -Depth 20 | Set-Content -Encoding UTF8 -Path $outPath
Write-Host "Wrote $outPath with $($results.Count) result(s)."
