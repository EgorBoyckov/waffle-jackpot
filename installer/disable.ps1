#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Disables Waffle Jackpot Login without uninstalling it (spec §10.3).

.DESCRIPTION
    Removes the "Credential Providers\{CLSID}" registration key -- a
    provider's presence under that key is what makes LogonUI enumerate it
    at all, so removing it is a complete, immediate opt-out. The COM
    registration (CLSID\...\InprocServer32) and the installed DLL/config
    are left untouched, so enable.ps1 can turn it back on instantly.

    Spec §10.3 also mentions the alternative of adding the CLSID to the
    "ExcludedCredentialProviders" group policy list. I chose this
    registration-key approach instead because I could verify its
    correctness with confidence (this key's role is unambiguous: it's the
    thing install.ps1 creates to register the provider in the first
    place), whereas I could not independently confirm the exact registry
    value name/shape the policy path expects. Flagging that per spec §13
    ("если не уверен в поведении API — скажи об этом явно, а не
    выдумывай") rather than guessing at the policy key.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$ClsidString = '{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}'
$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$ClsidString"

if (Test-Path $providerKey) {
    Remove-Item -Path $providerKey -Force
    Write-Host 'Waffle Jackpot Login disabled. It will not appear on the next logon screen.' -ForegroundColor Green
} else {
    Write-Host 'Waffle Jackpot Login is already disabled (or not installed).' -ForegroundColor Cyan
}
