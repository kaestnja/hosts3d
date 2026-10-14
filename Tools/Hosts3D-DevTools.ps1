param(
    [ValidateSet(
        "Help",
        "CheckTools",
        "CheckBuild",
        "CheckDebug",
        "CheckArtifacts",
        "CheckAll",
        "BuildHosts3D",
        "BuildHsen",
        "BuildAllWindows",
        "PackageWindows",
        "StageRuntime",
        "RunClangTidySarif",
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

    [string]$ToolchainRoot,

    [switch]$WithNpcap,

    [switch]$NoPackage,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$script:ToolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:RepoRoot = Split-Path -Parent $script:ToolRoot
$script:ArchWasProvided = $PSBoundParameters.ContainsKey("Arch")
$script:MsysRootWasProvided = $PSBoundParameters.ContainsKey("Msys2Root")
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

function Invoke-Hosts3DToolProbe {
    param([string]$FilePath, [string]$Arguments, [int]$TimeoutMilliseconds = 5000)
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $FilePath
    $start.Arguments = $Arguments
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw "Could not start tool: $FilePath" }
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $process.Kill()
            [void]$process.WaitForExit(1000)
            throw "Tool probe timed out: $FilePath"
        }
        [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = $output.GetAwaiter().GetResult().Trim()
            Error = $errors.GetAwaiter().GetResult().Trim()
        }
    } finally { $process.Dispose() }
}

function Get-Hosts3DToolchain {
    param(
        [ValidateSet('x64', 'x86', 'arm64')][string]$Architecture = $Arch,
        [string]$Prefix = $ToolchainRoot,
        [string]$MsysRoot = $Msys2Root
    )
    if (-not $Prefix) { $Prefix = $env:HOSTS3D_TOOLCHAIN_ROOT }
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    $explicit = -not [string]::IsNullOrWhiteSpace($Prefix)
    if ($explicit) {
        $candidates.Add((Join-Path $Prefix 'bin\g++.exe'))
    } else {
        foreach ($command in @(Get-Command g++.exe -All -ErrorAction SilentlyContinue)) {
            if ($command.Source) { $candidates.Add($command.Source) }
        }
        $folders = switch ($Architecture) {
            'x86' { @('mingw32') }
            'arm64' { @('clangarm64') }
            default { @('mingw64', 'ucrt64', 'clang64') }
        }
        if ($env:HOSTS3D_MSYS2_ROOT -and -not $PSBoundParameters.ContainsKey('MsysRoot') -and
            -not $script:MsysRootWasProvided) { $MsysRoot = $env:HOSTS3D_MSYS2_ROOT }
        foreach ($folder in $folders) {
            $candidates.Add((Join-Path $MsysRoot "$folder\bin\g++.exe"))
        }
        if ($env:LOCALAPPDATA) {
            $packages = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
            if (Test-Path -LiteralPath $packages -PathType Container) {
                foreach ($package in @(Get-ChildItem -LiteralPath $packages -Directory -Filter 'BrechtSanders.WinLibs*' -ErrorAction SilentlyContinue | Sort-Object Name)) {
                    foreach ($folder in $folders) {
                        $candidates.Add((Join-Path $package.FullName "$folder\bin\g++.exe"))
                    }
                }
            }
        }
    }
    $machinePattern = switch ($Architecture) {
        'x86' { '^i[3-6]86-.*mingw32$' }
        'arm64' { '^aarch64-.*mingw32$' }
        default { '^x86_64-.*mingw32$' }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            if ($explicit) { throw "ToolchainRoot has no readable bin\g++.exe: $Prefix" }
            continue
        }
        try { $probe = Invoke-Hosts3DToolProbe -FilePath $candidate -Arguments '-dumpmachine' }
        catch { if ($explicit) { throw }; continue }
        if ($probe.ExitCode -ne 0 -or $probe.Output -notmatch $machinePattern) {
            if ($explicit) { throw "ToolchainRoot compiler is incompatible with $Architecture`: $($probe.Output)" }
            continue
        }
        $compiler = [IO.Path]::GetFullPath($candidate)
        $bin = Split-Path -Parent $compiler
        $prefixRoot = Split-Path -Parent $bin
        $gdb = Join-Path $bin 'gdb.exe'
        if (-not (Test-Path -LiteralPath $gdb -PathType Leaf)) {
            $gdb = ''
            $onPath = Get-Command gdb.exe -ErrorAction SilentlyContinue
            if ($onPath) { $gdb = $onPath.Source }
        }
        $make = Join-Path $bin 'mingw32-make.exe'
        if (-not (Test-Path -LiteralPath $make -PathType Leaf)) {
            $make = ''
            $onPath = Get-Command make.exe -ErrorAction SilentlyContinue
            if ($onPath) { $make = $onPath.Source }
        }
        return [pscustomobject]@{
            Architecture = $Architecture
            Compiler = $compiler
            Machine = $probe.Output
            Root = $prefixRoot
            BinDirectory = $bin
            Gdb = $gdb
            Make = $make
        }
    }
    throw "No compatible $Architecture MinGW compiler found. Use existing tools on PATH or -ToolchainRoot; MSYS2 is the recommended setup when no suitable environment exists."
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

    $oldPrefix = $env:HOSTS3D_TOOLCHAIN_ROOT
    $oldMsysRoot = $env:HOSTS3D_MSYS2_ROOT
    try {
        if ($ToolchainRoot) { $env:HOSTS3D_TOOLCHAIN_ROOT = $ToolchainRoot }
        if ($script:MsysRootWasProvided) { $env:HOSTS3D_MSYS2_ROOT = $Msys2Root }
        & $env:ComSpec /d /c $cmdLine
        if ($LASTEXITCODE -ne 0) {
            throw "Batch command failed with exit code $LASTEXITCODE"
        }
    } finally {
        $env:HOSTS3D_TOOLCHAIN_ROOT = $oldPrefix
        $env:HOSTS3D_MSYS2_ROOT = $oldMsysRoot
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
        [string]$CommandName,
        [bool]$Required = $true
    )

    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $cmd) {
        return New-CheckResult -Area $Area -Name $Name -Found $false -Required $Required
    }
    return New-CheckResult -Area $Area -Name $Name -Found $true -Path $cmd.Source -Required $Required
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

function Find-ClangTidy {
    $cmd = Get-Command "clang-tidy" -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    foreach ($candidate in @(
        "C:\msys64\mingw64\bin\clang-tidy.exe",
        "C:\msys64\ucrt64\bin\clang-tidy.exe",
        "C:\msys64\clang64\bin\clang-tidy.exe",
        "C:\Program Files\LLVM\bin\clang-tidy.exe"
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    return ""
}

function Test-Hosts3DBuild {
    param($ResolvedTools)
    $results = @()
    try { $tools = if ($ResolvedTools) { $ResolvedTools } else { Get-Hosts3DToolchain } }
    catch { return New-CheckResult -Area 'build' -Name "MinGW compiler $Arch" -Found $false -Note $_.Exception.Message }
    $results += New-CheckResult -Area 'build' -Name "MinGW compiler $Arch" -Found $true -Path $tools.Compiler -Version $tools.Machine
    $resourceCompiler = Join-Path $tools.BinDirectory 'windres.exe'
    if (-not (Test-Path -LiteralPath $resourceCompiler -PathType Leaf)) {
        $onPath = Get-Command windres.exe -ErrorAction SilentlyContinue
        if ($onPath) { $resourceCompiler = $onPath.Source }
    }
    $results += Get-FileCheck -Area 'build' -Name 'Windows resource compiler' -Path $resourceCompiler
    $results += Get-FileCheck -Area 'build' -Name 'GLFW headers' -Path (Join-Path $tools.Root 'include\GLFW\glfw3.h')
    $staticLibrary = Join-Path $tools.Root 'lib\libglfw3.a'
    $importLibrary = Join-Path $tools.Root 'lib\libglfw3dll.a'
    $usesStaticLibrary = Test-Path -LiteralPath $staticLibrary -PathType Leaf
    $library = if ($usesStaticLibrary) { $staticLibrary } else { $importLibrary }
    $results += Get-FileCheck -Area 'build' -Name 'GLFW library' -Path $library
    if (-not $usesStaticLibrary) {
        $results += Get-FileCheck -Area 'build' -Name 'GLFW runtime' -Path (Join-Path $tools.BinDirectory 'glfw3.dll')
    }
    return $results
}

function Get-Hosts3DDebugger {
    $prefix = if ($ToolchainRoot) { $ToolchainRoot } else { $env:HOSTS3D_TOOLCHAIN_ROOT }
    if ($prefix) {
        $candidate = Join-Path $prefix 'bin\gdb.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        throw "ToolchainRoot has no readable bin\gdb.exe: $prefix"
    }
    try {
        $tools = Get-Hosts3DToolchain
        if ($tools.Gdb) { return $tools.Gdb }
    } catch { }
    $onPath = Get-Command gdb.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $root = if ($env:HOSTS3D_MSYS2_ROOT -and -not $script:MsysRootWasProvided) { $env:HOSTS3D_MSYS2_ROOT } else { $Msys2Root }
    $folder = switch ($Arch) { 'x86' { 'mingw32' }; 'arm64' { 'clangarm64' }; default { 'mingw64' } }
    $candidate = Join-Path $root "$folder\bin\gdb.exe"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    throw "No GDB found for $Arch. Configure existing debugger tools on PATH or use -ToolchainRoot."
}

function Test-Hosts3DTools {
    try { $tools = Get-Hosts3DToolchain; $results = @(Test-Hosts3DBuild -ResolvedTools $tools) }
    catch {
        $tools = $null
        $results = @(New-CheckResult -Area 'build' -Name "MinGW compiler $Arch" -Found $false -Note $_.Exception.Message)
    }
    $results += Get-CommandCheck -Area 'shell' -Name 'PowerShell 7' -CommandName 'pwsh' -Required $false
    $results += Get-CommandCheck -Area 'shell' -Name 'cmd.exe' -CommandName 'cmd'
    foreach ($tool in @('powershell', 'git', 'gh', 'python', 'py', 'code')) {
        $results += Get-CommandCheck -Area 'workflow' -Name $tool -CommandName $tool -Required $false
    }
    foreach ($name in @('bash', 'pacman')) {
        $results += Get-FileCheck -Area 'msys2' -Name $name -Path (Join-Path $Msys2Root "usr\bin\$name.exe") -Required $false -Note 'needed only for MSYS2-native/package operations'
    }
    if ($tools) { $gdb = $tools.Gdb } else { try { $gdb = Get-Hosts3DDebugger } catch { $gdb = '' } }
    $results += New-CheckResult -Area 'debug' -Name "GDB $Arch" -Found ([bool]$gdb) -Path $gdb -Required $false -Note 'needed for debugging, not compilation'
    $make = if ($tools) { $tools.Make } else { '' }
    $results += New-CheckResult -Area 'make' -Name 'make' -Found ([bool]$make) -Path $make -Required $false -Note 'native batch builds invoke the compiler directly'
    return $results
}

function Test-Hosts3DDebug {
    $results = @()
    try {
        $gdb = Get-Hosts3DDebugger
        $results += New-CheckResult -Area 'debug' -Name "GDB $Arch" -Found $true -Path $gdb
    } catch { $results += New-CheckResult -Area 'debug' -Name "GDB $Arch" -Found $false -Note $_.Exception.Message }
    $debugProgram = if ($Program) { $Program } else { "Debug\windows\$Arch\Hosts3D.exe" }
    $results += Get-FileCheck -Area 'debug' -Name "Debug program $Arch" -Path $debugProgram
    foreach ($file in @('.vscode\launch.json', '.vscode\tasks.json', '.vscode\c_cpp_properties.json')) {
        $results += Get-FileCheck -Area 'editor' -Name $file -Path $file -Required $false -Note 'optional; CLI debugging does not require VS Code'
        $fullPath = Resolve-RepoPath $file
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            try {
                Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json | Out-Null
                $results += New-CheckResult -Area 'editor-json' -Name $file -Found $true -Path $fullPath -Required $false
            } catch { $results += New-CheckResult -Area 'editor-json' -Name $file -Found $false -Path $fullPath -Note $_.Exception.Message -Required $false }
        }
    }
    return $results
}

function Show-Hosts3DChecks {
    param([object[]]$Results)
    $Results | Format-Table -AutoSize
    if (@($Results | Where-Object { $_.Status -eq 'missing' }).Count -gt 0) {
        throw 'Required tools for the selected operation/architecture are missing; optional tools do not require installation.'
    }
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

function Invoke-Hosts3DClangTidySarif {
    $scriptPath = Join-RepoPath "Tools\run-clang-tidy-sarif.ps1"
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        throw "clang-tidy SARIF helper not found: $scriptPath"
    }

    & $scriptPath @Args
    if (-not $?) {
        throw "run-clang-tidy-sarif.ps1 failed"
    }
}

function Invoke-Hosts3DGdbFile {
    if (-not $Program) {
        $Program = "Debug\windows\$Arch\Hosts3D.exe"
    }

    $gdb = Get-Hosts3DDebugger

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
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task CheckBuild -Arch x64
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task CheckAll -Arch x64
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildHosts3D -Config Debug -Arch x64
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildAllWindows -WithNpcap
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task PackageWindows -Arch x64 -WithNpcap
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task RunClangTidySarif
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task PacmanInstall -Packages mingw-w64-x86_64-gdb,mingw-w64-i686-gdb
  pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task GdbFile -Arch x64 -Program Debug/windows/x64/Hosts3D.exe

Compatible existing MinGW/WinLibs tools are discovered automatically.
Optional overrides: -ToolchainRoot <prefix with bin/g++.exe>, -Msys2Root <MSYS prefix>.
x64 does not require x86 tools. GDB, make and MSYS2 are operation-specific.

Dot-source for functions:
  . Tools/Hosts3D-DevTools.ps1
  Get-Hosts3DToolchain -Architecture x64
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
        Show-Hosts3DChecks -Results @(Test-Hosts3DTools)
    }
    "CheckBuild" {
        Show-Hosts3DChecks -Results @(Test-Hosts3DBuild)
    }
    "CheckDebug" {
        Show-Hosts3DChecks -Results @(Test-Hosts3DDebug)
    }
    "CheckArtifacts" {
        Invoke-Hosts3DArtifactChecks
    }
    "CheckAll" {
        Show-Hosts3DChecks -Results @(Test-Hosts3DTools)
        Show-Hosts3DChecks -Results @(Test-Hosts3DDebug)
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
    "RunClangTidySarif" {
        Invoke-Hosts3DClangTidySarif
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
