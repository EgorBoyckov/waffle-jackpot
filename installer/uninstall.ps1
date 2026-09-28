#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Fully removes Waffle Jackpot Login (spec §10.4).

.DESCRIPTION
    Disables the provider, then removes its COM registration, the
    installed DLL, config.json, and the Phase 7 kill-switch registry key
    (removed here too, harmlessly, if a future run has created it).
    Tolerant of a partial install -- any step whose target is already
    missing is reported as such rather than treated as an error, so this
    is safe to run more than once or after a failed/partial install.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Continue'

$ClsidString = '{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}'
$InstallDir = Join-Path $env:ProgramFiles 'WaffleJackpot'
$ConfigDir = Join-Path $env:ProgramData 'WaffleJackpot'
$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$ClsidString"
$clsidKey = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidString"
$killSwitchKey = 'HKLM:\SOFTWARE\WaffleJackpot'

function Remove-PathIfPresent {
    param([string]$Path, [switch]$Recurse)
    if (-not (Test-Path $Path)) {
        return 'already absent'
    }
    try {
        # Undo install.ps1's ACL lockdown first, or the delete can fail --
        # /reset restores inheritance so Remove-Item can actually touch it.
        icacls $Path /reset /T /C 2>$null | Out-Null
        if ($Recurse) {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
        } else {
            Remove-Item -Path $Path -Force -ErrorAction Stop
        }
        if (Test-Path $Path) { 'FAILED TO REMOVE' } else { 'removed' }
    } catch {
        "FAILED TO REMOVE ($($_.Exception.Message))"
    }
}

$report = [ordered]@{}

# 1. Disable first (same effect as disable.ps1).
$report['Provider registration'] = Remove-PathIfPresent -Path $providerKey

# 2. Remove COM registration.
$report['COM registration'] = Remove-PathIfPresent -Path $clsidKey -Recurse

# 3. Remove the installed DLL / install directory.
$report['Install directory'] = Remove-PathIfPresent -Path $InstallDir -Recurse

# 4. Remove config.
$report['Config directory'] = Remove-PathIfPresent -Path $ConfigDir -Recurse

# 5. Remove the kill-switch key Phase 7 introduces, if present.
$report['Kill-switch registry key'] = Remove-PathIfPresent -Path $killSwitchKey -Recurse

Write-Host ''
Write-Host 'Uninstall report:' -ForegroundColor Yellow
foreach ($key in $report.Keys) {
    $status = $report[$key]
    $color = if ($status -like 'FAILED*') { 'Red' } else { 'Green' }
    Write-Host ('  {0,-26} {1}' -f $key, $status) -ForegroundColor $color
}

$failures = $report.Values | Where-Object { $_ -like 'FAILED*' }
if ($failures) {
    Write-Warning 'Some items could not be removed. Re-run as Administrator, or remove them manually.'
    exit 1
}
Write-Host ''
Write-Host 'Waffle Jackpot Login fully removed.' -ForegroundColor Green
