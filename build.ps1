[CmdletBinding()]
param(
    [switch]$Package
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$msbuildCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
)
$msbuild = $msbuildCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild with the Visual C++ x86 toolset was not found.'
}

& $msbuild (Join-Path $projectRoot 'IshimuraStabilityPatch.vcxproj') `
    /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed.' }

& $msbuild (Join-Path $projectRoot 'tests\ProxySmoke.vcxproj') `
    /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'Smoke-test build failed.' }

& $msbuild (Join-Path $projectRoot 'tests\RegressionSmoke.vcxproj') `
    /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'Subtitle/borderless regression-test build failed.' }

$dll = Join-Path $projectRoot 'build\Release\xinput1_3.dll'
$smoke = Join-Path $projectRoot 'build\tests\ProxySmoke.exe'
& $smoke $dll
if ($LASTEXITCODE -ne 0) { throw 'Proxy smoke test failed.' }

$regressionSmoke = Join-Path $projectRoot 'build\tests\RegressionSmoke.exe'
& $regressionSmoke
if ($LASTEXITCODE -ne 0) { throw 'Subtitle/borderless regression test failed.' }

if ($Package) {
    $releaseName = 'Ishimura_Stability_Patch_v1.0.1_Rama2120'
    $stagingRoot = Join-Path $projectRoot 'build\package'
    $staging = Join-Path $stagingRoot $releaseName
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    New-Item -ItemType Directory -Path $staging -Force | Out-Null

    $files = @(
        @{ Source = $dll; Name = 'xinput1_3.dll' },
        @{ Source = (Join-Path $projectRoot 'IshimuraStabilityPatch.ini'); Name = 'IshimuraStabilityPatch.ini' },
        @{ Source = (Join-Path $projectRoot 'README.md'); Name = 'README.md' },
        @{ Source = (Join-Path $projectRoot 'LICENSE'); Name = 'LICENSE' }
    )
    foreach ($file in $files) {
        Copy-Item -LiteralPath $file.Source -Destination (Join-Path $staging $file.Name)
    }

    $outputs = Join-Path (Split-Path -Parent $projectRoot) 'outputs'
    New-Item -ItemType Directory -Path $outputs -Force | Out-Null
    $zip = Join-Path $outputs ($releaseName + '.zip')
    if (Test-Path -LiteralPath $zip) {
        Remove-Item -LiteralPath $zip -Force
    }
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip -CompressionLevel Optimal
    Get-FileHash -LiteralPath $zip -Algorithm SHA256
}
