$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$dev='C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat'
$build=Join-Path $root 'build\tests'
$exe=Join-Path $build 'DeviceDispatch.exe'
Push-Location $build
try {
    & $env:ComSpec /d /s /c ('call "'+$dev+'" -no_logo -arch=x86 -host_arch=x64 >nul && cl.exe /nologo /EHsc /std:c++17 /O2 /MT /W4 /DUNICODE /D_UNICODE /DNOMINMAX "'+(Join-Path $root 'tests\device_dispatch.cpp')+'" "'+(Join-Path $root 'src\window_fix.cpp')+'" /Fe:"'+$exe+'" /link user32.lib d3d9.lib winmm.lib')
    if($LASTEXITCODE){throw 'Device regression build failed'}
    $result=Join-Path $build 'device-dispatch-result.txt'
    if(Test-Path -LiteralPath $result){Remove-Item -LiteralPath $result -Force}
    $process=Start-Process -FilePath $exe -WorkingDirectory $build -PassThru -WindowStyle Hidden
    if(-not $process.WaitForExit(20000)){Stop-Process -Id $process.Id -Force; throw 'Device regression timed out'}
    if($process.ExitCode -ne 0){throw "Device regression failed: $($process.ExitCode)"}
    $lines=Get-Content -LiteralPath $result
    $lines
    if($lines[-1] -ne 'PASS'){throw 'Device regression did not reach its success marker'}
} finally { Pop-Location }
