#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Re-enables Waffle Jackpot Login after disable.ps1 (spec §10.3).

.DESCRIPTION
    Recreates the "Credential Providers\{CLSID}" registration key
    disable.ps1 removed. Requires install.ps1 to have been run at least
    once (the COM registration must already exist) -- this script only
    restores participation, it doesn't reinstall the DLL or config.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$ClsidString = '{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}'
$clsidKey = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidString"
$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$ClsidString"

if (-not (Test-Path $clsidKey)) {
    throw "Waffle Jackpot Login isn't installed (no COM registration found under $clsidKey). Run install.ps1 first."
}

New-Item -Path $providerKey -Force | Out-Null
Set-ItemProperty -Path $providerKey -Name '(Default)' -Value 'Waffle Jackpot Login'
Write-Host 'Waffle Jackpot Login enabled. It will appear on the next logon screen.' -ForegroundColor Green
