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
    Copy-PayloadFile -Source (Join-Path $runtimeDir $name) -DestinationDirectory $destinationPath -DestinationName $name
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
Copy-PayloadFile -Source (Join-Path $repoRoot "Tools\snmp\scalance_xr328_mirror_check.py") -DestinationDirectory $snmpToolsDir -DestinationName "scalance_xr328_mirror_check.py" -Required
Copy-PayloadFile -Source (Join-Path $repoRoot "Tools\snmp\scalance_xr328_snmp_mirroring_abfrage.md") -DestinationDirectory $snmpToolsDir -DestinationName "scalance_xr328_snmp_mirroring_abfrage.md" -Required

$snmpReadme = @'
# Hosts3D SNMP Tools

This folder contains optional SNMP diagnostics and switch-specific helpers.

Current helper:
- scalance_xr328_mirror_check.py
- scalance_xr328_snmp_mirroring_abfrage.md

The switch type is encoded in the file names so future SNMP helpers can live
beside these files without creating one folder per switch. From the repository
root and from a staged package root, the same relative path works.

Quick lab example:

```powershell
.\run-scalance-check.ps1 -SwitchIp 192.168.6.248 -Version 2c -Community public
```

SNMPv3 credentials should be provided through environment variables or a future
local credential profile, not stored in this package.
'@
Write-TextFile -Path (Join-Path $snmpToolsDir "README.md") -Content $snmpReadme

$psWrapper = @'
param(
    [Parameter(Mandatory = $true)]
    [string]$SwitchIp,

    [ValidateSet("1", "2c", "3")]
    [string]$Version = "2c",

    [int]$Port = 161,

    [int]$Timeout = 3,

    [int]$Retries = 1,

    [string]$Community = $env:SNMP_COMMUNITY,

    [string]$User = $env:SNMP_USER,

    [ValidateSet("noAuthNoPriv", "authNoPriv", "authPriv")]
    [string]$Level = "authPriv",

    [string]$AuthProto = $(if ($env:SNMP_AUTH_PROTO) { $env:SNMP_AUTH_PROTO } else { "SHA" }),

    [string]$AuthPass = $env:SNMP_AUTH_PASS,

    [string]$PrivProto = $(if ($env:SNMP_PRIV_PROTO) { $env:SNMP_PRIV_PROTO } else { "AES" }),

    [string]$PrivPass = $env:SNMP_PRIV_PASS,

    [switch]$CheckAccessOnly,

    [bool]$Pretty = $true,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$script = Join-Path $PSScriptRoot "scalance_xr328_mirror_check.py"
$snmpget = Join-Path $root "snmpget.exe"
$snmpwalk = Join-Path $root "snmpwalk.exe"

$args = @($script, $SwitchIp, "--version", $Version, "--port", $Port, "--timeout", $Timeout, "--retries", $Retries)
if ($Version -in @("1", "2c") -and $Community) {
    $args += @("--community", $Community)
}
if ($Version -eq "3") {
    if ($User) {
        $args += @("--user", $User)
    }
    $args += @("--level", $Level)
    if ($Level -in @("authNoPriv", "authPriv")) {
        $args += @("--auth-proto", $AuthProto)
        if ($AuthPass) {
            $args += @("--auth-pass", $AuthPass)
        }
    }
    if ($Level -eq "authPriv") {
        $args += @("--priv-proto", $PrivProto)
        if ($PrivPass) {
            $args += @("--priv-pass", $PrivPass)
        }
    }
}
if (Test-Path -LiteralPath $snmpget) {
    $args += @("--snmpget", $snmpget)
}
if (Test-Path -LiteralPath $snmpwalk) {
    $args += @("--snmpwalk", $snmpwalk)
}
if ($CheckAccessOnly) {
    $args += "--check-access-only"
}
if ($Pretty) {
    $args += "--pretty"
}
if ($ExtraArgs) {
    $args += $ExtraArgs
}

python @args
'@
Write-TextFile -Path (Join-Path $snmpToolsDir "run-scalance-check.ps1") -Content $psWrapper

$cmdWrapper = @'
@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%..\.."
python "%SCRIPT_DIR%scalance_xr328_mirror_check.py" %* --snmpget "%ROOT%\snmpget.exe" --snmpwalk "%ROOT%\snmpwalk.exe"
exit /b %ERRORLEVEL%
'@
Write-TextFile -Path (Join-Path $snmpToolsDir "run-scalance-check.cmd") -Content $cmdWrapper

Write-Host "Staged runtime payload to $destinationPath"
