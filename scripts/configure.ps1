param(
    [string]$CEFRoot = $env:ASTERUI_CEF_ROOT,
    [string]$ScriptHookVRoot = $env:ASTERUI_SCRIPTHOOKV_ROOT
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$dependencies = Join-Path $root '.deps'
if (-not $CEFRoot) {
    $saved = Join-Path $dependencies 'cef-root.txt'
    if (Test-Path -LiteralPath $saved) {
        $CEFRoot = (Get-Content -LiteralPath $saved -Raw).Trim()
    } else {
        $CEFRoot = Join-Path $dependencies 'cef'
    }
}
if (-not $ScriptHookVRoot) {
    $saved = Join-Path $dependencies 'scripthookv-root.txt'
    if (Test-Path -LiteralPath $saved) {
        $ScriptHookVRoot = (Get-Content -LiteralPath $saved -Raw).Trim()
    }
}
if (-not $ScriptHookVRoot) {
    throw 'Set -ScriptHookVRoot or ASTERUI_SCRIPTHOOKV_ROOT'
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe not found. Install Visual Studio with Desktop development with C++.'
}
$installation = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installation) {
    throw 'No Visual Studio C++ toolchain was found'
}
$devCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $devCommand)) {
    throw "VsDevCmd.bat not found: $devCommand"
}

$env:ASTERUI_CEF_ROOT = [System.IO.Path]::GetFullPath($CEFRoot)
$env:ASTERUI_SCRIPTHOOKV_ROOT = [System.IO.Path]::GetFullPath($ScriptHookVRoot)
New-Item -ItemType Directory -Path $dependencies -Force | Out-Null
Set-Content -LiteralPath (Join-Path $dependencies 'cef-root.txt') `
    -Value $env:ASTERUI_CEF_ROOT -Encoding UTF8
Set-Content -LiteralPath (Join-Path $dependencies 'scripthookv-root.txt') `
    -Value $env:ASTERUI_SCRIPTHOOKV_ROOT -Encoding UTF8
$command = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cmake --preset windows-x64' -f $devCommand
& cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Configure failed with exit code $LASTEXITCODE"
}
