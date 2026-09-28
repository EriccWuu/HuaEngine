function Enter-HuaMsvcEnvironment([string]$VsRoot, [string]$ScratchRoot) {
    $scriptPath = Join-Path $ScratchRoot 'enter-msvc.cmd'
    $developerCommand = Join-Path $VsRoot 'Common7/Tools/VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $developerCommand -PathType Leaf)) { throw "VS developer command is missing: $developerCommand" }
    if ($developerCommand.Contains('"') -or $developerCommand.Contains('%')) { throw 'Unsupported characters in Visual Studio path.' }
    # Load only SDK and compiler setup: the optional vcpkg extension may launch bundled pwsh.
    $compilerCommand = Join-Path $VsRoot 'Common7/Tools/vsdevcmd/ext/vcvars.bat'
    @('@echo off', 'set VSCMD_SKIP_SENDTELEMETRY=1',
      ('call "' + $developerCommand + '" -no_logo -no_ext -arch=amd64 -host_arch=amd64 >nul'),
      'if errorlevel 1 exit /b %errorlevel%',
      'set VSCMD_ARG_HOST_ARCH=x64', 'set VSCMD_ARG_TGT_ARCH=x64', 'set VSCMD_ARG_APP_PLAT=Desktop',
      ('call "' + $compilerCommand + '" >nul'), 'if errorlevel 1 exit /b %errorlevel%',
      'set "INCLUDE=%__VSCMD_VCVARS_INCLUDE%%INCLUDE%"',
      'set "EXTERNAL_INCLUDE=%__VSCMD_VCVARS_INCLUDE%%EXTERNAL_INCLUDE%"', 'set') | Set-Content -LiteralPath $scriptPath -Encoding ASCII
    Push-Location -LiteralPath $ScratchRoot
    try {
        $environment = & $env:ComSpec /d /c '.\enter-msvc.cmd'
        $commandExitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($commandExitCode -ne 0) { throw "VS developer command failed with exit code $commandExitCode" }
    foreach ($line in $environment) {
        if ($line -match '^(PATH|INCLUDE|EXTERNAL_INCLUDE|LIB|LIBPATH|VCINSTALLDIR|VCToolsInstallDir|VCToolsVersion|VSINSTALLDIR|WindowsSdkDir|WindowsSDKVersion|UniversalCRTSdkDir|UCRTVersion)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    if (-not $env:VCINSTALLDIR -or -not $env:INCLUDE -or -not $env:LIB) { throw 'VS developer command did not supply a complete C++ environment.' }
}
