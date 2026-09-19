<#
Stages the shared Windows runtime payload into Release/Debug windows folders or
an explicit package destination. Debug staging also reuses Release Net-SNMP CLI
tools for the same architecture when Debug-local snmp*.exe files are absent,
because compile-net-snmp-windows.bat currently produces those tools under
Release/windows/<arch>.
#>
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [ValidateSet("x64", "x86", "arm64")]
    [string]$Arch = "x64",

    [string]$Destination,

    [switch]$WithNpcap
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$runtimeDir = Join-Path $repoRoot (Join-Path $Config (Join-Path "windows" $Arch))

if (-not $Destination) {
    $destinationPath = $runtimeDir
} elseif ([System.IO.Path]::IsPathRooted($Destination)) {
    $destinationPath = $Destination
} else {
    $destinationPath = Join-Path $repoRoot $Destination
}

function Convert-ToFullPath {
    param([string]$Path)
    [System.IO.Path]::GetFullPath($Path)
}

function Copy-PayloadFile {
    param(
        [string]$Source,
        [string]$DestinationDirectory,
        [string]$DestinationName,
        [switch]$Required
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        if ($Required) {
            throw "Missing required payload file: $Source"
        }
        return
    }

    if (-not (Test-Path -LiteralPath $DestinationDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $DestinationDirectory | Out-Null
    }

    $target = Join-Path $DestinationDirectory $DestinationName
    if ((Convert-ToFullPath $Source) -ieq (Convert-ToFullPath $target)) {
        return
    }
    Copy-Item -LiteralPath $Source -Destination $target -Force
}

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )
    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory | Out-Null
    }
    if ([System.IO.Path]::GetExtension($Path) -in @(".bat", ".cmd", ".ps1")) {
        $Content = $Content -replace "`r?`n", "`r`n"
    }
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

if (-not (Test-Path -LiteralPath $runtimeDir -PathType Container)) {
    throw "Runtime directory does not exist: $runtimeDir"
}

if (-not (Test-Path -LiteralPath $destinationPath -PathType Container)) {
    New-Item -ItemType Directory -Path $destinationPath | Out-Null
}

foreach ($name in @("Hosts3D.exe", "hsen.exe", "glfw3.dll", "libwinpthread-1.dll")) {
    Copy-PayloadFile -Source (Join-Path $runtimeDir $name) -DestinationDirectory $destinationPath -DestinationName $name -Required
}

foreach ($name in @("snmpget.exe", "snmpwalk.exe", "snmpset.exe")) {
    $toolSource = Join-Path $runtimeDir $name
    if (-not (Test-Path -LiteralPath $toolSource -PathType Leaf) -and $Config -ne "Release") {
        $releaseToolSource = Join-Path $repoRoot (Join-Path "Release" (Join-Path "windows" (Join-Path $Arch $name)))
        if (Test-Path -LiteralPath $releaseToolSource -PathType Leaf) {
            $toolSource = $releaseToolSource
        }
    }
    Copy-PayloadFile -Source $toolSource -DestinationDirectory $destinationPath -DestinationName $name
}

if ($WithNpcap) {
    foreach ($name in @("Packet.dll", "wpcap.dll")) {
        Copy-PayloadFile -Source (Join-Path $runtimeDir $name) -DestinationDirectory $destinationPath -DestinationName $name -Required
    }
}

Copy-PayloadFile -Source (Join-Path $repoRoot "COPYING") -DestinationDirectory $destinationPath -DestinationName "COPYING" -Required
Copy-PayloadFile -Source (Join-Path $repoRoot "README-runtime-windows.md") -DestinationDirectory $destinationPath -DestinationName "README-runtime-windows.md" -Required
Copy-PayloadFile -Source (Join-Path $repoRoot "README-runtime-windows.md") -DestinationDirectory $destinationPath -DestinationName "README.md" -Required
Copy-PayloadFile -Source (Join-Path $repoRoot "testing\README.md") -DestinationDirectory $destinationPath -DestinationName "README-testing.md" -Required

foreach ($name in @("sim-hsen.ps1", "sim-hsen.py", "demo-hsen.ps1", "demo-hsen.py")) {
    Copy-PayloadFile -Source (Join-Path $repoRoot (Join-Path "testing" $name)) -DestinationDirectory $destinationPath -DestinationName $name -Required
}

$snmpToolsDir = Join-Path $destinationPath "Tools\snmp"
foreach ($obsoleteSnmpFile in @(
    "scalance_xr328_snmp_mirroring_abfrage.md",
    "scalance_xc208g_snmp_mirroring_abfrage.md",
    "scalance_xr328_mirror_check.py",
    "scalance_xc208g_mirror_check.py",
    "run-scalance-check.ps1",
    "run-scalance-check.cmd",
    "scalance_xr328_mirror_check.md",
    "scalance_xc208g_mirror_check.md"
)) {
    $obsoletePath = Join-Path $snmpToolsDir $obsoleteSnmpFile
    if (Test-Path -LiteralPath $obsoletePath -PathType Leaf) {
        Remove-Item -LiteralPath $obsoletePath -Force
    }
}
Copy-PayloadFile -Source (Join-Path $repoRoot "Tools\snmp\README.md") -DestinationDirectory $snmpToolsDir -DestinationName "README.md" -Required
Copy-PayloadFile -Source (Join-Path $repoRoot "Tools\snmp\scalance_switches_refresh.py") -DestinationDirectory $snmpToolsDir -DestinationName "scalance_switches_refresh.py" -Required

Write-Host "Staged runtime payload to $destinationPath"
