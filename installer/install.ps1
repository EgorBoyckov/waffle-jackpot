#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Installs the Waffle Jackpot Credential Provider (project spec §10.2).

.DESCRIPTION
    THIS IS A JOKE PROJECT. WAFFLES ARE NOT A SECURITY BOUNDARY.
    This installs a Credential Provider into your Windows logon screen.
    TEST ON A VIRTUAL MACHINE FIRST, with a snapshot taken before install --
    see README.md.

    Copies the built provider DLL into %ProgramFiles%\WaffleJackpot\, locks
    it down to Administrators/SYSTEM (it loads into LogonUI running as
    SYSTEM), registers it as a COM in-proc server, registers it as a
    Credential Provider, and deploys config.json with a safe ACL. Never
    touches the system's built-in credential providers.

.PARAMETER Force
    Skip the interactive confirmation prompt (for scripted/CI installs).

.PARAMETER DllPath
    Path to the built WaffleJackpotProvider.dll. Defaults to the usual
    CMake build output location relative to this script.
#>
[CmdletBinding()]
param(
    [switch]$Force,
    [string]$DllPath
)

$ErrorActionPreference = 'Stop'

$ClsidString = '{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$InstallDir = Join-Path $env:ProgramFiles 'WaffleJackpot'
$DestDllPath = Join-Path $InstallDir 'WaffleJackpotProvider.dll'
$ConfigDir = Join-Path $env:ProgramData 'WaffleJackpot'
$ConfigPath = Join-Path $ConfigDir 'config.json'

function Test-SupportedOs {
    $os = Get-CimInstance Win32_OperatingSystem
    $buildNumber = [int]$os.BuildNumber
    # Windows 10 1809 = build 17763; Windows 11 = build 22000+. Anything
    # older doesn't have the V2 Credential Provider surface this relies on.
    if ($buildNumber -lt 17763) {
        throw "Unsupported Windows build $buildNumber. Waffle Jackpot Login requires Windows 10 1809 (build 17763) or later."
    }
}

function Test-SupportedArch {
    $arch = $env:PROCESSOR_ARCHITECTURE
    if ($arch -notin @('AMD64', 'ARM64')) {
        throw "Unsupported architecture '$arch'. Waffle Jackpot Login requires x64 or ARM64."
    }
}

Write-Host '=======================================================' -ForegroundColor Yellow
Write-Host ' WAFFLE JACKPOT LOGIN -- INSTALLER' -ForegroundColor Yellow
Write-Host ' THIS IS A JOKE PROJECT. WAFFLES ARE NOT A SECURITY BOUNDARY.' -ForegroundColor Yellow
Write-Host ' This installs a Credential Provider into your Windows logon' -ForegroundColor Yellow
Write-Host ' screen. TEST ON A VIRTUAL MACHINE FIRST, with a snapshot' -ForegroundColor Yellow
Write-Host ' taken before install -- see README.md.' -ForegroundColor Yellow
Write-Host '=======================================================' -ForegroundColor Yellow

if (-not $Force) {
    $confirmation = Read-Host "Type YES to install to THIS machine's logon screen"
    if ($confirmation -ne 'YES') {
        Write-Host 'Aborted -- nothing was changed.' -ForegroundColor Cyan
        exit 1
    }
}

Test-SupportedOs
Test-SupportedArch

if (-not $DllPath) {
    $DllPath = Join-Path $RepoRoot 'build\src\CredentialProvider\WaffleJackpotProvider.dll'
}
if (-not (Test-Path $DllPath)) {
    throw "Built DLL not found at '$DllPath'. Build the solution (CMake + VS 2022, x64 or ARM64) first, or pass -DllPath explicitly."
}

# --- Copy DLL, lock down its ACL --------------------------------------------
# Installed under %ProgramFiles%, not System32 (spec §10.2 step 3): this
# isn't a system component, and %ProgramFiles% already denies write access
# to non-administrators by default -- we tighten it further below since
# this specific DLL loads into LogonUI as SYSTEM.
New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
Copy-Item -Path $DllPath -Destination $DestDllPath -Force

icacls $DestDllPath /inheritance:r | Out-Null
icacls $DestDllPath /grant:r 'Administrators:(RX)' 'SYSTEM:(RX)' 'BUILTIN\Users:(RX)' | Out-Null

# --- Register COM in-proc server --------------------------------------------
$clsidKey = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidString"
$inprocKey = "$clsidKey\InprocServer32"
New-Item -Path $inprocKey -Force | Out-Null
Set-ItemProperty -Path $inprocKey -Name '(Default)' -Value $DestDllPath
Set-ItemProperty -Path $inprocKey -Name 'ThreadingModel' -Value 'Apartment'

# --- Register as a Credential Provider --------------------------------------
# This key's mere presence is what makes LogonUI enumerate the provider;
# disable.ps1 removes just this key (see its own comment for why), leaving
# the COM registration and DLL alone so re-enabling is instant.
$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$ClsidString"
New-Item -Path $providerKey -Force | Out-Null
Set-ItemProperty -Path $providerKey -Name '(Default)' -Value 'Waffle Jackpot Login'

# --- Deploy config, safe ACL -------------------------------------------------
# Write: Administrators + SYSTEM only (spec §7 -- the DLL runs as SYSTEM in
# LogonUI and must not trust a config an ordinary user could have edited).
# Read: everyone, because LogonUI reads it before any user is authenticated.
New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
if (-not (Test-Path $ConfigPath)) {
    $defaultConfig = Join-Path $RepoRoot 'config\config.default.json'
    Copy-Item -Path $defaultConfig -Destination $ConfigPath
}
icacls $ConfigDir /inheritance:r | Out-Null
icacls $ConfigDir /grant:r 'Administrators:(OI)(CI)(F)' 'SYSTEM:(OI)(CI)(F)' 'BUILTIN\Users:(OI)(CI)(RX)' | Out-Null

Write-Host ''
Write-Host 'Installed. Waffle Jackpot Login will appear on the next logon screen.' -ForegroundColor Green
Write-Host 'To disable it without uninstalling:  disable.ps1' -ForegroundColor Green
Write-Host 'To remove it completely:             uninstall.ps1' -ForegroundColor Green
Write-Host 'If you get locked out, see:           recovery\RECOVERY.md' -ForegroundColor Green
