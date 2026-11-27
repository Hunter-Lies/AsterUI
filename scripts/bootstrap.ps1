param(
    [string]$ScriptHookVSdk = $env:ASTERUI_SCRIPTHOOKV_ROOT
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$lock = Get-Content -LiteralPath (Join-Path $root 'dependencies.lock.json') -Raw |
    ConvertFrom-Json
$dependencies = Join-Path $root '.deps'
$downloads = Join-Path $dependencies 'downloads'
$cefRoot = Join-Path $dependencies 'cef'
$archive = Join-Path $downloads $lock.cef.archive

New-Item -ItemType Directory -Path $downloads -Force | Out-Null

if (-not (Test-Path -LiteralPath $archive)) {
    Write-Host "Downloading CEF $($lock.cef.version)"
    Invoke-WebRequest -Uri $lock.cef.url -OutFile $archive
}

$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
if ($hash -ne $lock.cef.sha256) {
    throw "CEF archive hash mismatch. Expected $($lock.cef.sha256), got $hash"
}

if (-not (Test-Path -LiteralPath (Join-Path $cefRoot 'cmake'))) {
    $extract = Join-Path $dependencies 'cef-extract'
    if (Test-Path -LiteralPath $extract) {
        $resolved = [System.IO.Path]::GetFullPath($extract)
        $expected = [System.IO.Path]::GetFullPath($dependencies)
        if (-not $resolved.StartsWith($expected, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Unexpected extraction path: $resolved"
        }
        Remove-Item -LiteralPath $extract -Recurse -Force
    }
    New-Item -ItemType Directory -Path $extract -Force | Out-Null
    tar -xf $archive -C $extract
    $directory = Get-ChildItem -LiteralPath $extract -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'cmake') } |
        Select-Object -First 1
    if (-not $directory) {
        throw 'The CEF archive did not contain a distribution root'
    }
    Move-Item -LiteralPath $directory.FullName -Destination $cefRoot
    Remove-Item -LiteralPath $extract -Force
}

if (-not $ScriptHookVSdk) {
    $ScriptHookVSdk = Join-Path $dependencies 'scripthookv'
}
if (-not (Test-Path -LiteralPath (Join-Path $ScriptHookVSdk 'inc\main.h')) -or
    -not (Test-Path -LiteralPath (Join-Path $ScriptHookVSdk 'lib\ScriptHookV.lib'))) {
    Write-Warning @"
ScriptHookV SDK was not found.
Download the SDK from http://www.dev-c.com/gtav/scripthookv/
and pass it with:
  .\scripts\bootstrap.ps1 -ScriptHookVSdk C:\path\to\ScriptHookV
"@
} else {
    Set-Content -LiteralPath (Join-Path $dependencies 'scripthookv-root.txt') `
        -Value ([System.IO.Path]::GetFullPath($ScriptHookVSdk)) -Encoding UTF8
}

Write-Host "ASTERUI_CEF_ROOT=$cefRoot"
if ($ScriptHookVSdk) {
    Write-Host "ASTERUI_SCRIPTHOOKV_ROOT=$ScriptHookVSdk"
}
