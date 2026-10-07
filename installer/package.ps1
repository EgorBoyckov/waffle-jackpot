#Requires -Version 5.1
<#
.SYNOPSIS
    Builds (Release, x64) and assembles a ready-to-install package in dist\.

.DESCRIPTION
    Produces dist\WaffleJackpot\ and dist\WaffleJackpot.zip containing the
    binaries, default config, installer scripts, recovery docs, and
    INSTALL.cmd (self-elevating wrapper around installer\install.ps1).
#>
[CmdletBinding()]
param([switch]$SkipBuild)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build'
$Dist = Join-Path $Root 'dist'
$Pkg = Join-Path $Dist 'WaffleJackpot'

if (-not $SkipBuild) {
    cmake -S $Root -B $Build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    cmake --build $Build --config Release -- /m
    if ($LASTEXITCODE) { throw 'Build failed' }
    ctest --test-dir $Build -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed' }
}

if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Path "$Pkg\bin", "$Pkg\config", "$Pkg\installer", "$Pkg\recovery" -Force | Out-Null

Copy-Item "$Build\src\CredentialProvider\Release\WaffleJackpotProvider.dll" "$Pkg\bin"
Copy-Item "$Build\src\Control\Release\WaffleJackpotControl.exe" "$Pkg\bin"
Copy-Item "$Build\src\Demo\Release\WaffleJackpotDemo.exe" "$Pkg\bin"
Copy-Item "$Root\config\config.default.json" "$Pkg\config"
Copy-Item "$Root\installer\install.ps1", "$Root\installer\uninstall.ps1", "$Root\installer\enable.ps1", "$Root\installer\disable.ps1", "$Root\installer\diagnose.ps1" "$Pkg\installer"
Copy-Item "$Root\recovery\*" "$Pkg\recovery"
Copy-Item "$Root\README.md" $Pkg

@'
@echo off
rem Self-elevating launcher for installer\install.ps1 (joke project: test on a VM first!)
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1"
pause
'@ | Set-Content "$Pkg\INSTALL.cmd" -Encoding ASCII

@'
@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\uninstall.ps1"
pause
'@ | Set-Content "$Pkg\UNINSTALL.cmd" -Encoding ASCII

Compress-Archive -Path $Pkg -DestinationPath "$Dist\WaffleJackpot.zip" -Force
Write-Host "Package ready: $Pkg  and  $Dist\WaffleJackpot.zip" -ForegroundColor Green
