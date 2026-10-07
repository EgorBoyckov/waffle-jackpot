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
    # Packaged layout (bin\) first, then the CMake Release build tree.
    $candidates = @(
        (Join-Path $RepoRoot 'bin\WaffleJackpotProvider.dll'),
        (Join-Path $RepoRoot 'build\src\CredentialProvider\Release\WaffleJackpotProvider.dll')
    )
    $DllPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not (Test-Path $DllPath)) {
    throw "Built DLL not found at '$DllPath'. Build the solution (CMake + VS 2022, x64 or ARM64) first, or pass -DllPath explicitly."
}

# --- Copy DLL, lock down its ACL --------------------------------------------
# Installed under %ProgramFiles%, not System32 (spec §10.2 step 3): this
# isn't a system component, and %ProgramFiles% already denies write access
# to non-administrators by default -- we tighten it further below since
# this specific DLL loads into LogonUI as SYSTEM.
#
# ACLs are set with well-known SIDs, NOT group names: 'Administrators' and
# 'BUILTIN\Users' don't resolve on localized Windows (e.g. "Администраторы"),
# icacls then fails as a whole, and after /inheritance:r that leaves the file
# with NO access for anyone -- including SYSTEM, so LogonUI can't load it.
$SidAdmins = '*S-1-5-32-544'
$SidSystem = '*S-1-5-18'
$SidUsers  = '*S-1-5-32-545'

function Invoke-Icacls {
    param([string[]]$IcaclsArgs)
    $output = & icacls.exe @IcaclsArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "icacls $($IcaclsArgs -join ' ') failed ($LASTEXITCODE): $output"
    }
}

# Recover from a previous (broken) install whose ACL left nobody with access.
function Reset-Acl {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        & takeown.exe /f $Path /r /d y 2>&1 | Out-Null
        & icacls.exe $Path /reset /t /c 2>&1 | Out-Null
    }
}

Reset-Acl $InstallDir
Reset-Acl $ConfigDir

New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
Copy-Item -Path $DllPath -Destination $DestDllPath -Force

Invoke-Icacls @($DestDllPath, '/inheritance:r')
Invoke-Icacls @($DestDllPath, '/grant:r', "${SidAdmins}:(RX)", "${SidSystem}:(RX)", "${SidUsers}:(RX)")

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

# --- Make our tile the pre-selected one -------------------------------------
# Otherwise LogonUI pre-selects the Password/PIN tile of whichever provider
# last logged the user in, and ours hides under "Sign-in options". The
# provider also re-asserts this on every logon screen (JackpotProvider.cpp).
$logonUiKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\LogonUI'
$userTileKey = "$logonUiKey\UserTile"
New-Item -Path $userTileKey -Force | Out-Null
Set-ItemProperty -Path $logonUiKey -Name 'LastLoggedOnProvider' -Value $ClsidString
Get-CimInstance Win32_UserAccount -Filter 'LocalAccount=True AND Disabled=False' | ForEach-Object {
    Set-ItemProperty -Path $userTileKey -Name $_.SID -Value $ClsidString
}

# --- Deploy config, safe ACL -------------------------------------------------
# Write: Administrators + SYSTEM only (spec §7 -- the DLL runs as SYSTEM in
# LogonUI and must not trust a config an ordinary user could have edited).
# Read: everyone, because LogonUI reads it before any user is authenticated.
New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
if (-not (Test-Path $ConfigPath)) {
    $defaultConfig = Join-Path $RepoRoot 'config\config.default.json'
    Copy-Item -Path $defaultConfig -Destination $ConfigPath
}
Invoke-Icacls @($ConfigDir, '/inheritance:r')
Invoke-Icacls @($ConfigDir, '/grant:r', "${SidAdmins}:(OI)(CI)(F)", "${SidSystem}:(OI)(CI)(F)", "${SidUsers}:(OI)(CI)(RX)")

Write-Host ''
Write-Host 'Installed. Waffle Jackpot Login will appear on the next logon screen.' -ForegroundColor Green
Write-Host 'To disable it without uninstalling:  disable.ps1' -ForegroundColor Green
Write-Host 'To remove it completely:             uninstall.ps1' -ForegroundColor Green
Write-Host 'If you get locked out, see:           recovery\RECOVERY.md' -ForegroundColor Green
