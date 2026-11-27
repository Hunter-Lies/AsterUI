$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$cef = $env:ASTERUI_CEF_ROOT
if (-not $cef) {
    $savedCef = Join-Path $root '.deps\cef-root.txt'
    if (Test-Path -LiteralPath $savedCef) {
        $cef = (Get-Content -LiteralPath $savedCef -Raw).Trim()
    } else {
        $cef = Join-Path $root '.deps\cef'
    }
}
$dist = Join-Path $root 'dist'
$app = Join-Path $dist 'AsterUI'

if (-not (Test-Path -LiteralPath (Join-Path $build 'AsterUI.asi'))) {
    throw 'Build first: AsterUI.asi not found'
}
if (-not (Test-Path -LiteralPath (Join-Path $cef 'Release\libcef.dll'))) {
    throw "CEF runtime is missing: $cef"
}

if (Test-Path -LiteralPath $dist) {
    $resolved = (Resolve-Path -LiteralPath $dist).Path
    $expected = [System.IO.Path]::GetFullPath($dist)
    if ($resolved -ne $expected) {
        throw "Unexpected dist path: $resolved"
    }
    Remove-Item -LiteralPath $dist -Recurse -Force
}

New-Item -ItemType Directory -Path $app -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'AsterUI.asi') -Destination $dist
Copy-Item -LiteralPath (Join-Path $build 'AsterUI.Demo.asi') -Destination $dist
Copy-Item -LiteralPath (Join-Path $build 'AsterUI.Host.exe') -Destination $app
Copy-Item -Path (Join-Path $cef 'Release\*') -Destination $app -Recurse -Force
Copy-Item -Path (Join-Path $cef 'Resources\*') -Destination $app -Recurse -Force
Copy-Item -Path (Join-Path $root 'assets\ui') -Destination (Join-Path $app 'ui') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'config\AsterUI.json') `
    -Destination (Join-Path $app 'AsterUI.json')
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'CONTRIBUTORS.md') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'RELEASE-NOTES.md') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root 'include') -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root 'sdk') -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root 'examples') -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $root 'sdk') `
    -Destination (Join-Path $app 'sdk') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'examples') `
    -Destination (Join-Path $app 'examples') -Recurse
Copy-Item -LiteralPath (Join-Path $cef 'LICENSE.txt') `
    -Destination (Join-Path $app 'CEF-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $cef 'CREDITS.html') `
    -Destination (Join-Path $app 'CEF-CREDITS.html')

$runtimeArtifacts = @(
    (Join-Path $app 'cache'),
    (Join-Path $app 'AsterUI.log'),
    (Join-Path $app 'AsterUI.Host.log'),
    (Join-Path $app 'cef.log')
)
foreach ($artifact in $runtimeArtifacts) {
    if (Test-Path -LiteralPath $artifact) {
        Remove-Item -LiteralPath $artifact -Recurse -Force
    }
}

$manifest = [System.Collections.Generic.List[string]]::new()
$manifest.Add('AsterUI engine package')
$manifest.Add('Generated: ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))
$manifest.Add('')
Get-ChildItem -LiteralPath $dist -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative = [System.IO.Path]::GetRelativePath($dist, $_.FullName)
        $manifest.Add($relative)
        $manifest.Add('  ' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)
    }
$manifest | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding UTF8

Get-ChildItem -LiteralPath $dist -Recurse -File |
    Measure-Object Length -Sum |
    ForEach-Object { 'Release size: {0:N1} MB' -f ($_.Sum / 1MB) }
