#Requires -Version 5.1
<#
.SYNOPSIS
    Collects everything needed to debug "the tile doesn't appear" into one
    text file on the Desktop (diagnose-waffle.txt). Run from an elevated
    PowerShell. Never includes passwords (the provider never logs them).
#>
$ErrorActionPreference = 'Continue'
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host 'Run this from an ELEVATED PowerShell (Run as administrator) -- otherwise the install/data dirs are unreadable.' -ForegroundColor Red
    exit 1
}
$out = Join-Path ([Environment]::GetFolderPath('Desktop')) 'diagnose-waffle.txt'
$clsid = '{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}'
$installDir = Join-Path $env:ProgramFiles 'WaffleJackpot'
$dataDir = Join-Path $env:ProgramData 'WaffleJackpot'

function Section($title, [scriptblock]$body) {
    "`r`n=== $title ===" | Out-File $out -Append -Encoding utf8
    try { & $body 2>&1 | Out-String | Out-File $out -Append -Encoding utf8 }
    catch { "ERROR: $_" | Out-File $out -Append -Encoding utf8 }
}

"Waffle Jackpot diagnostics  $(Get-Date -Format s)" | Out-File $out -Encoding utf8

Section 'OS' { Get-CimInstance Win32_OperatingSystem | Select Caption, Version, BuildNumber, OSArchitecture | fl }
Section 'Credential Provider key' { Get-Item "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$clsid" | fl }
Section 'COM registration (64-bit)' { Get-ItemProperty "HKLM:\SOFTWARE\Classes\CLSID\$clsid\InprocServer32" | fl }
Section 'Kill switch (CrashCount >= 3 disables the tile)' { Get-ItemProperty 'HKLM:\SOFTWARE\WaffleJackpot' | fl }
Section 'Install dir' { Get-ChildItem $installDir -Force | Select Name, Length, LastWriteTime | ft -Auto; icacls (Join-Path $installDir 'WaffleJackpotProvider.dll') }
Section 'Data dir' { Get-ChildItem $dataDir -Recurse -Force | Select FullName, Length, LastWriteTime | ft -Auto; icacls $dataDir }
Section 'config.json' { Get-Content (Join-Path $dataDir 'config.json') }
Section 'Provider DLL: authenticode / version / signature' { Get-Item (Join-Path $installDir 'WaffleJackpotProvider.dll') | Select Name, Length, VersionInfo | fl }
Section 'Other credential providers / filters' {
    Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers' | % { "$($_.PSChildName)  $((Get-ItemProperty $_.PSPath).'(default)')" }
    '--- filters'
    Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Provider Filters' | % { "$($_.PSChildName)  $((Get-ItemProperty $_.PSPath).'(default)')" }
}
Section 'Policies that hide password providers' {
    Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' | Select ExcludedCredentialProviders, DontDisplayLastUserName | fl
    Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PasswordLess\Device' | fl
}
Section 'provider.log (last 200 lines)' { Get-Content (Join-Path $dataDir 'logs\provider.log') -Tail 200 }
Section 'Application errors mentioning LogonUI/Waffle (last 10)' {
    Get-WinEvent -FilterHashtable @{LogName = 'Application'; Level = 1, 2, 3} -MaxEvents 300 |
        ? { $_.Message -match 'LogonUI|Waffle' } | select -First 10 | fl TimeCreated, ProviderName, Message
}

Write-Host "Saved: $out" -ForegroundColor Green
Write-Host 'Send that file (and optionally C:\ProgramData\WaffleJackpot\logs\provider.log).'
