$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$app = Join-Path $root 'dist\AsterUI'
$smoke = Join-Path $build 'AsterUI.HostSmoke.exe'
$pipeSmoke = Join-Path $build 'AsterUI.PipeCancelSmoke.exe'
$policySmoke = Join-Path $build 'AsterUI.RequestPolicySmoke.exe'

if (-not (Test-Path -LiteralPath $smoke)) {
    throw 'Build first: AsterUI.HostSmoke.exe not found'
}
if (-not (Test-Path -LiteralPath (Join-Path $app 'AsterUI.Host.exe'))) {
    throw 'Run package.ps1 first'
}

$tests = @(
    @{ Page = 'ui/index.html'; Width = 640; Height = 360 },
    @{ Page = 'ui/index.html'; Width = 2560; Height = 1440 },
    @{ Page = 'ui/input-smoke/index.html'; Width = 640; Height = 360 },
    @{ Page = 'examples/demo-ui/index.html'; Width = 1280; Height = 720 },
    @{ Page = 'examples/hello-overlay/index.html'; Width = 1280; Height = 720 },
    @{ Page = 'examples/interactive-menu/index.html'; Width = 1920; Height = 1080 }
)

foreach ($test in $tests) {
    & $smoke (Join-Path $app 'AsterUI.Host.exe') $app $test.Page $test.Width $test.Height
    if ($LASTEXITCODE -ne 0) {
        throw "Host smoke test failed: $($test.Page)"
    }
}

if (Test-Path -LiteralPath $pipeSmoke) {
    & $pipeSmoke
    if ($LASTEXITCODE -ne 0) {
        throw 'Pipe cancellation test failed'
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $app '..\AsterUI.Demo.asi'))) {
    throw 'AsterUI.Demo.asi is missing from the package'
}

& $policySmoke
if ($LASTEXITCODE -ne 0) {
    throw 'Request policy test failed'
}

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

Write-Output 'AsterUI smoke tests passed'
