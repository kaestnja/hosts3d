<# Tests real resolver/feature rules with isolated tool-boundary fixtures. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
$fixture = Join-Path $tempRoot ('hosts3d-toolchain-' + [guid]::NewGuid().ToString('N'))
$oldPrefix = $env:HOSTS3D_TOOLCHAIN_ROOT
$oldMsys = $env:HOSTS3D_MSYS2_ROOT
$oldLocal = $env:LOCALAPPDATA
$created = $false

function Assert-Rule {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function New-FixtureTool {
    param([string]$Directory, [string]$Relative)
    $file = Join-Path $Directory $Relative
    $null = New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force
    [IO.File]::WriteAllText($file, '')
    return $file
}

try {
    $null = New-Item -ItemType Directory -Path $fixture
    $created = $true
    $env:HOSTS3D_TOOLCHAIN_ROOT = $null
    $env:HOSTS3D_MSYS2_ROOT = $null
    $env:LOCALAPPDATA = Join-Path $fixture 'profile'
    . (Join-Path $root 'Tools\Hosts3D-DevTools.ps1') -Arch x64 -Msys2Root (Join-Path $fixture 'absent-msys')

    # Substitute only external process/command lookup; resolver logic is actual.
    $script:pathCompiler = ''
    $script:pathResourceCompiler = ''
    function Get-Command {
        param([string]$Name, [switch]$All, $ErrorAction)
        if ($Name -eq 'g++.exe' -and $script:pathCompiler) {
            return [pscustomobject]@{ Source = $script:pathCompiler }
        }
        if ($Name -eq 'windres.exe' -and $script:pathResourceCompiler) {
            return [pscustomobject]@{ Source = $script:pathResourceCompiler }
        }
    }
    function Invoke-Hosts3DToolProbe {
        param([string]$FilePath, [string]$Arguments)
        $machine = if ($FilePath -like '*wrong-arch*') { 'i686-w64-mingw32' } else { 'x86_64-w64-mingw32' }
        [pscustomobject]@{ ExitCode = 0; Output = $machine; Error = '' }
    }

    $prefix = Join-Path $fixture 'existing-native'
    $compiler = New-FixtureTool $prefix 'bin\g++.exe'
    foreach ($file in @('bin\windres.exe', 'bin\gdb.exe', 'include\GLFW\glfw3.h', 'lib\libglfw3dll.a', 'bin\glfw3.dll')) {
        $null = New-FixtureTool $prefix $file
    }
    $tools = Get-Hosts3DToolchain -Architecture x64 -Prefix $prefix
    Assert-Rule ($tools.Compiler -eq $compiler) 'An explicit compatible native toolchain was not selected.'
    $build = @(Test-Hosts3DBuild -ResolvedTools $tools)
    Assert-Rule (@($build | Where-Object Status -eq 'missing').Count -eq 0) 'Native x64 build falsely requires MSYS2, make or x86 tools.'
    Remove-Item -LiteralPath (Join-Path $prefix 'bin\windres.exe')
    $script:pathResourceCompiler = New-FixtureTool (Join-Path $fixture 'path-tools') 'windres.exe'
    $splitBuild = @(Test-Hosts3DBuild -ResolvedTools $tools)
    Assert-Rule (@($splitBuild | Where-Object { $_.Name -eq 'Windows resource compiler' -and $_.Status -eq 'ok' -and $_.Path -eq $script:pathResourceCompiler }).Count -eq 1) 'Resource compiler on PATH was rejected despite direct build support.'
    $env:HOSTS3D_TOOLCHAIN_ROOT = $prefix
    $checks = @(Test-Hosts3DTools)
    Assert-Rule (@($checks | Where-Object { $_.Name -eq 'PowerShell 7' -and $_.Status -eq 'optional-missing' }).Count -eq 1) 'A Windows PowerShell build must not require a separate PowerShell 7 installation.'
    Assert-Rule (@($checks | Where-Object { $_.Name -eq 'make' -and $_.Status -eq 'optional-missing' }).Count -eq 1) 'make must be optional for direct compiler builds.'
    Assert-Rule (@($checks | Where-Object { $_.Name -eq 'GDB x64' -and $_.Status -eq 'ok' }).Count -eq 1) 'Native GDB was not found beside the selected compiler.'
    $env:HOSTS3D_TOOLCHAIN_ROOT = $null

    $wrong = Join-Path $fixture 'wrong-arch'
    $null = New-FixtureTool $wrong 'bin\g++.exe'
    $script:pathCompiler = $compiler
    $rejected = $false
    try { $null = Get-Hosts3DToolchain -Architecture x64 -Prefix $wrong } catch { $rejected = $true }
    Assert-Rule $rejected 'An incompatible explicit override silently fell back to another compiler.'
    $fromPath = Get-Hosts3DToolchain -Architecture x64
    Assert-Rule ($fromPath.Compiler -eq $compiler) 'Compatible PATH compiler was not selected before optional providers.'

    $script:pathCompiler = ''
    $winlibs = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_fixture\mingw64'
    $winlibsCompiler = New-FixtureTool $winlibs 'bin\g++.exe'
    $discovered = Get-Hosts3DToolchain -Architecture x64
    Assert-Rule ($discovered.Compiler -eq $winlibsCompiler) 'Installed WinLibs was not discovered without MSYS2 or PATH compiler.'
    $debugPrefix = Join-Path $fixture 'debugger-only'
    $null = New-FixtureTool $debugPrefix 'bin\gdb.exe'
    $env:HOSTS3D_TOOLCHAIN_ROOT = $debugPrefix
    $Program = Join-Path $fixture 'built-program.exe'
    [IO.File]::WriteAllText($Program, '')
    $debug = @(Test-Hosts3DDebug)
    Assert-Rule (@($debug | Where-Object Status -eq 'missing').Count -eq 0) 'Debugging x64 falsely requires another architecture or compiler installation.'
    Write-Output 'PASS: native/PATH/WinLibs discovery, explicit rejection and operation-specific dependencies'
} finally {
    $env:HOSTS3D_TOOLCHAIN_ROOT = $oldPrefix
    $env:HOSTS3D_MSYS2_ROOT = $oldMsys
    $env:LOCALAPPDATA = $oldLocal
    if ($created -and (Test-Path -LiteralPath $fixture)) {
        $resolved = [IO.Path]::GetFullPath($fixture)
        if (-not $resolved.StartsWith($tempRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected fixture cleanup root.' }
        $entries = @(Get-ChildItem -LiteralPath $fixture -Recurse -Force)
        if (@($entries | Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 }).Count -gt 0) { throw 'Unexpected fixture reparse point.' }
        Remove-Item -LiteralPath $fixture -Recurse -Force
        if (Test-Path -LiteralPath $fixture) { throw 'Toolchain fixture remained after cleanup.' }
    }
}
