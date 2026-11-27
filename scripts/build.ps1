$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'configure.ps1')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$devCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$command = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cd /d "{1}" && cmake --build --preset windows-x64' -f `
    $devCommand, $root
& cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Get-Item -LiteralPath `
    (Join-Path $root 'build\AsterUI.asi'), `
    (Join-Path $root 'build\AsterUI.Host.exe'), `
    (Join-Path $root 'build\AsterUI.HostSmoke.exe'), `
    (Join-Path $root 'build\AsterUI.PipeCancelSmoke.exe') |
    Select-Object Name, Length, LastWriteTime |
    Format-Table -AutoSize
