param(
    [ValidateSet(
        "Help",
        "CheckTools",
        "CheckDebug",
        "CheckArtifacts",
        "CheckAll",
        "BuildHosts3D",
        "BuildHsen",
        "BuildAllWindows",
        "PackageWindows",
        "StageRuntime",
        "RunMsys2",
        "PacmanInstall",
        "GdbFile",
        "InstallVSCodeDebugExtensions"
    )]
    [string]$Task = "Help",

    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [ValidateSet("x64", "x86", "arm64")]
    [string]$Arch = "x64",

    [string[]]$Args = @(),

    [string[]]$Packages = @(),

    [string]$Command,

    [string]$Program,

    [string]$Msys2Root = "C:\msys64",

    [switch]$WithNpcap,

    [switch]$NoPackage,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$script:ToolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:RepoRoot = Split-Path -Parent $script:ToolRoot
$script:ArchWasProvided = $PSBoundParameters.ContainsKey("Arch")
$script:IsDotSourced = $MyInvocation.InvocationName -eq "."

function Get-Hosts3DRepoRoot {
    return $script:RepoRoot
}

function Join-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return Join-Path $script:RepoRoot $Path
}

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $script:RepoRoot $Path))
}

function ConvertTo-CmdArgument {
    param([AllowEmptyString()][string]$Value)

    if ($null -eq $Value) {
        return '""'
    }
    if ($Value -eq "") {
        return '""'
    }

    $escaped = $Value -replace '(["^&|<>()])', '^$1'
    if ($escaped -match '[\s"&|<>()^]') {
        return '"' + $escaped + '"'
    }
    return $escaped
}

function ConvertTo-BashSingleQuoted {
    param([AllowEmptyString()][string]$Value)

    if ($null -eq $Value) {
        return "''"
    }
    return "'" + ($Value -replace "'", "'\''") + "'"
}

function Invoke-Hosts3DBatch {
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [string[]]$BatchArgs = @()
    )

    $scriptPath = Resolve-RepoPath $Script
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        throw "Batch script not found: $scriptPath"
    }

    $parts = @("call", (ConvertTo-CmdArgument $scriptPath))
    foreach ($arg in $BatchArgs) {
        $parts += ConvertTo-CmdArgument $arg
    }
    $cmdLine = $parts -join " "

    Write-Host "cmd.exe /d /c $cmdLine"
    if ($DryRun) {
        return
    }

    & $env:ComSpec /d /c $cmdLine
    if ($LASTEXITCODE -ne 0) {
        throw "Batch command failed with exit code $LASTEXITCODE"
    }
}

function Invoke-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][string]$MsysCommand,
        [string]$Root = $Msys2Root
    )

    $bash = Join-Path $Root "usr\bin\bash.exe"
    if (-not (Test-Path -LiteralPath $bash -PathType Leaf)) {
        throw "MSYS2 bash not found: $bash"
    }

    Write-Host "$bash -lc $MsysCommand"
    if ($DryRun) {
        return
    }

    & $bash -lc $MsysCommand
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed with exit code $LASTEXITCODE"
    }
}

function Install-Msys2Packages {
    param(
        [Parameter(Mandatory = $true)][string[]]$PackageNames,
        [string]$Root = $Msys2Root
    )

    $quoted = $PackageNames | ForEach-Object { ConvertTo-BashSingleQuoted $_ }
    Invoke-Msys2Command -Root $Root -MsysCommand ("pacman -S --needed --noconfirm " + ($quoted -join " "))
}

function New-CheckResult {
    param(
        [string]$Area,
        [string]$Name,
        [bool]$Found,
        [string]$Path = "",
        [string]$Version = "",
        [string]$Note = "",
        [bool]$Required = $true
    )

    [pscustomobject]@{
        Area = $Area
        Name = $Name
        Status = if ($Found) { "ok" } elseif ($Required) { "missing" } else { "optional-missing" }
        Path = $Path
        Version = $Version
        Note = $Note
    }
}

function Get-CommandCheck {
    param(
        [string]$Area,
        [string]$Name,
        [string]$CommandName
    )

    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $cmd) {
        return New-CheckResult -Area $Area -Name $Name -Found $false
    }
    return New-CheckResult -Area $Area -Name $Name -Found $true -Path $cmd.Source
}

function Get-FileCheck {
    param(
        [string]$Area,
        [string]$Name,
        [string]$Path,
        [string]$Note = "",
        [bool]$Required = $true
    )

    $fullPath = Resolve-RepoPath $Path
    if ([System.IO.Path]::IsPathRooted($Path)) {
        $fullPath = [System.IO.Path]::GetFullPath($Path)
    }
    return New-CheckResult -Area $Area -Name $Name -Found (Test-Path -LiteralPath $fullPath -PathType Leaf) -Path $fullPath -Note $Note -Required $Required
}

function Get-VSCodeExtensions {
    $code = Get-Command code -ErrorAction SilentlyContinue
    if (-not $code) {
        $fallbacks = @(
            "C:\Program Files\Microsoft VS Code\bin\code.cmd",
            (Join-Path $env:LOCALAPPDATA "Programs\Microsoft VS Code\bin\code.cmd")
        )
        foreach ($candidate in $fallbacks) {
            if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                return @{ Command = $candidate; Extensions = @(& $candidate --list-extensions) }
            }
        }
        return @{ Command = ""; Extensions = @() }
    }
    return @{ Command = $code.Source; Extensions = @(& $code.Source --list-extensions) }
}

function Test-Hosts3DTools {
    $results = @()
    $results += Get-CommandCheck -Area "shell" -Name "PowerShell 7" -CommandName "pwsh"
    $results += Get-CommandCheck -Area "shell" -Name "Windows PowerShell" -CommandName "powershell"
    $results += Get-CommandCheck -Area "shell" -Name "cmd.exe" -CommandName "cmd"
    $results += Get-CommandCheck -Area "source" -Name "git" -CommandName "git"
    $results += Get-CommandCheck -Area "source" -Name "GitHub CLI" -CommandName "gh"
    $results += Get-CommandCheck -Area "script" -Name "python" -CommandName "python"
    $results += Get-CommandCheck -Area "script" -Name "py launcher" -CommandName "py"
    $results += Get-CommandCheck -Area "editor" -Name "VS Code CLI" -CommandName "code"

    $results += Get-FileCheck -Area "msys2" -Name "bash" -Path (Join-Path $Msys2Root "usr\bin\bash.exe")
    $results += Get-FileCheck -Area "msys2" -Name "pacman" -Path (Join-Path $Msys2Root "usr\bin\pacman.exe")
    $results += Get-FileCheck -Area "mingw64" -Name "g++ x64" -Path (Join-Path $Msys2Root "mingw64\bin\g++.exe")
    $results += Get-FileCheck -Area "mingw32" -Name "g++ x86" -Path (Join-Path $Msys2Root "mingw32\bin\g++.exe")
    $results += Get-FileCheck -Area "mingw64" -Name "gdb x64" -Path (Join-Path $Msys2Root "mingw64\bin\gdb.exe")
    $results += Get-FileCheck -Area "mingw32" -Name "gdb x86" -Path (Join-Path $Msys2Root "mingw32\bin\gdb.exe")
    $results += Get-FileCheck -Area "msys2" -Name "make" -Path (Join-Path $Msys2Root "usr\bin\make.exe")
    $results += Get-FileCheck -Area "mingw64" -Name "mingw32-make x64" -Path (Join-Path $Msys2Root "mingw64\bin\mingw32-make.exe") -Required $false -Note "optional fallback; repo scripts currently use g++ directly"
    $results += Get-FileCheck -Area "mingw32" -Name "mingw32-make x86" -Path (Join-Path $Msys2Root "mingw32\bin\mingw32-make.exe") -Required $false -Note "optional fallback; repo scripts currently use g++ directly"

    $vs = Get-VSCodeExtensions
    $requiredExtensions = @("ms-vscode.cpptools", "ms-vscode.powershell", "ms-python.python", "ms-python.debugpy")
    foreach ($extension in $requiredExtensions) {
        $results += New-CheckResult -Area "vscode" -Name $extension -Found ($vs.Extensions -contains $extension) -Path $vs.Command
    }

    return $results
}

function Test-Hosts3DDebug {
    $results = @()
    $results += Get-FileCheck -Area "debug" -Name "launch.json" -Path ".vscode\launch.json"
    $results += Get-FileCheck -Area "debug" -Name "tasks.json" -Path ".vscode\tasks.json"
    $results += Get-FileCheck -Area "debug" -Name "c_cpp_properties.json" -Path ".vscode\c_cpp_properties.json"
    $results += Get-FileCheck -Area "debug" -Name "gdb x64" -Path (Join-Path $Msys2Root "mingw64\bin\gdb.exe")
    $results += Get-FileCheck -Area "debug" -Name "gdb x86" -Path (Join-Path $Msys2Root "mingw32\bin\gdb.exe")
    $results += Get-FileCheck -Area "debug" -Name "Debug Hosts3D x64 exe" -Path "Debug\windows\x64\Hosts3D.exe"
    $results += Get-FileCheck -Area "debug" -Name "Debug Hosts3D x86 exe" -Path "Debug\windows\x86\Hosts3D.exe"

    foreach ($file in @(".vscode\launch.json", ".vscode\tasks.json", ".vscode\c_cpp_properties.json", ".vscode\settings.json")) {
        $fullPath = Resolve-RepoPath $file
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            try {
                Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json | Out-Null
                $results += New-CheckResult -Area "debug-json" -Name $file -Found $true -Path $fullPath
            } catch {
                $results += New-CheckResult -Area "debug-json" -Name $file -Found $false -Path $fullPath -Note $_.Exception.Message
            }
        }
    }

    return $results
}

function Invoke-Hosts3DArtifactChecks {
    Push-Location $script:RepoRoot
    try {
        & git diff --check
        if ($LASTEXITCODE -ne 0) {
            throw "git diff --check failed with exit code $LASTEXITCODE"
        }

        & (Join-RepoPath "Tools\Find-InvalidTextBytes.ps1") -TrackedOnly
        if ($LASTEXITCODE -ne 0) {
            throw "Find-InvalidTextBytes.ps1 failed with exit code $LASTEXITCODE"
        }

        Test-Hosts3DDebug | Where-Object { $_.Area -eq "debug-json" } | Format-Table -AutoSize
    } finally {
        Pop-Location
    }
}

function Invoke-Hosts3DStageRuntime {
    $stageArgs = @{
        Config = $Config
        Arch = $Arch
    }
    if ($WithNpcap) {
        $stageArgs.WithNpcap = $true
    }
    & (Join-RepoPath "Tools\Stage-RuntimePayload.ps1") @stageArgs
    if (-not $?) {
        throw "Stage-RuntimePayload.ps1 failed"
    }
}

function Invoke-Hosts3DGdbFile {
    if (-not $Program) {
        $Program = "Debug\windows\$Arch\Hosts3D.exe"
    }

    $gdb = if ($Arch -eq "x86") {
        Join-Path $Msys2Root "mingw32\bin\gdb.exe"
    } else {
        Join-Path $Msys2Root "mingw64\bin\gdb.exe"
    }
    if (-not (Test-Path -LiteralPath $gdb -PathType Leaf)) {
        throw "GDB not found: $gdb"
    }

    $programPath = Resolve-RepoPath $Program
    if (-not (Test-Path -LiteralPath $programPath -PathType Leaf)) {
        throw "Program not found: $programPath"
    }

    $gdbProgramPath = $programPath -replace "\\", "/"
    & $gdb --batch -ex "file $gdbProgramPath" -ex "info sources"
    if ($LASTEXITCODE -ne 0) {
        throw "GDB failed with exit code $LASTEXITCODE"
    }
}

function Install-VSCodeDebugExtensions {
    $vs = Get-VSCodeExtensions
    if (-not $vs.Command) {
        throw "VS Code CLI not found. Install VS Code CLI or run extension setup manually."
    }

    $extensions = @("ms-vscode.cpptools", "ms-vscode.powershell", "ms-python.python", "ms-python.debugpy")
    foreach ($extension in $extensions) {
        if ($vs.Extensions -contains $extension) {
            Write-Host "Already installed: $extension"
            continue
        }
        Write-Host "Installing VS Code extension: $extension"
        if (-not $DryRun) {
            & $vs.Command --install-extension $extension
            if ($LASTEXITCODE -ne 0) {
                throw "VS Code extension install failed: $extension"
            }
        }
    }
}

function Show-Hosts3DDevToolsHelp {
    @"
Hosts3D DevTools helper

Examples:
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task CheckAll
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildHosts3D -Config Debug -Arch x64
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildAllWindows -WithNpcap
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task PackageWindows -Arch x64 -WithNpcap
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task PacmanInstall -Packages mingw-w64-x86_64-gdb,mingw-w64-i686-gdb
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task GdbFile -Arch x64 -Program Debug/windows/x64/Hosts3D.exe

Dot-source for functions:
  . Tools/Hosts3D-DevTools.ps1
  Invoke-Hosts3DBatch -Script compile-hosts3d.bat -BatchArgs Debug,x64,--no-pause
  Invoke-Msys2Command -MsysCommand 'pacman -Syu --noconfirm'
"@
}

if ($script:IsDotSourced) {
    return
}

switch ($Task) {
    "Help" {
        Show-Hosts3DDevToolsHelp
    }
    "CheckTools" {
        Test-Hosts3DTools | Format-Table -AutoSize
    }
    "CheckDebug" {
        Test-Hosts3DDebug | Format-Table -AutoSize
    }
    "CheckArtifacts" {
        Invoke-Hosts3DArtifactChecks
    }
    "CheckAll" {
        Test-Hosts3DTools | Format-Table -AutoSize
        Test-Hosts3DDebug | Format-Table -AutoSize
        Invoke-Hosts3DArtifactChecks
    }
    "BuildHosts3D" {
        Invoke-Hosts3DBatch -Script "compile-hosts3d.bat" -BatchArgs @($Config, $Arch, "--no-pause")
    }
    "BuildHsen" {
        Invoke-Hosts3DBatch -Script "compile-hsen.bat" -BatchArgs @($Config, $Arch, "--no-pause")
    }
    "BuildAllWindows" {
        $batchArgs = @()
        if ($Config -eq "Debug") {
            $batchArgs += "Debug"
        }
        if ($script:ArchWasProvided) {
            $batchArgs += $Arch
        }
        if ($WithNpcap) {
            $batchArgs += "with-npcap"
        }
        if ($NoPackage) {
            $batchArgs += "no-package"
        }
        $batchArgs += $Args
        Invoke-Hosts3DBatch -Script "compile-all-windows.bat" -BatchArgs $batchArgs
    }
    "PackageWindows" {
        $batchArgs = @()
        if ($Config -eq "Debug") {
            $batchArgs += "Debug"
        }
        if ($script:ArchWasProvided) {
            $batchArgs += $Arch
        }
        if ($WithNpcap) {
            $batchArgs += "with-npcap"
        }
        $batchArgs += $Args
        Invoke-Hosts3DBatch -Script "package-all-windows.bat" -BatchArgs $batchArgs
    }
    "StageRuntime" {
        Invoke-Hosts3DStageRuntime
    }
    "RunMsys2" {
        if (-not $Command) {
            throw "-Command is required for -Task RunMsys2"
        }
        Invoke-Msys2Command -MsysCommand $Command
    }
    "PacmanInstall" {
        if (-not $Packages -or $Packages.Count -eq 0) {
            throw "-Packages is required for -Task PacmanInstall"
        }
        Install-Msys2Packages -PackageNames $Packages
    }
    "GdbFile" {
        Invoke-Hosts3DGdbFile
    }
    "InstallVSCodeDebugExtensions" {
        Install-VSCodeDebugExtensions
    }
}
