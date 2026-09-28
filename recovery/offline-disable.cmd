@echo off
setlocal enabledelayedexpansion

echo =========================================================
echo  Waffle Jackpot Login -- OFFLINE DISABLE (WinRE)
echo =========================================================
echo.
echo  This disables the Waffle Jackpot credential provider by editing
echo  the SOFTWARE registry hive of an OFFLINE Windows installation.
echo  Run this from a WinRE / Windows Setup "Command Prompt":
echo    Shift+F10 during Windows Setup, or
echo    Troubleshoot -^> Advanced options -^> Command Prompt from the
echo    recovery environment.
echo.
echo  It only removes the provider's registration (same effect as
echo  disable.ps1) -- it does not touch the DLL, COM registration, or
echo  config.json, so re-enabling later just needs enable.ps1 or
echo  WaffleJackpotControl.exe enable once you're back in Windows.
echo.
echo  IMPORTANT: In WinRE, drive letters are almost never the same as in
echo  normal Windows. Your real Windows drive is very often D:, E:, or
echo  X:, NOT C:. Find it first with diskpart:
echo.
echo      diskpart
echo      list volume
echo      exit
echo.
echo  Look for the volume with a large size and a Windows-looking
echo  layout (a "Windows" label, or check with "dir D:\Windows" etc.
echo  after exiting diskpart). Note its drive letter.
echo.

set /p WINDOWS_DRIVE="Enter the drive letter of your Windows installation (e.g. D, no colon): "

if "%WINDOWS_DRIVE%"=="" (
    echo No drive letter entered. Aborting -- nothing was changed.
    goto :eof
)

set HIVE_PATH=%WINDOWS_DRIVE%:\Windows\System32\config\SOFTWARE
if not exist "%HIVE_PATH%" (
    echo.
    echo Could not find %HIVE_PATH%
    echo Double-check the drive letter with diskpart ^(see above^) and run this script again.
    goto :eof
)

echo.
echo Loading %HIVE_PATH% as HKLM\WAFFLE_OFFLINE ...
reg load HKLM\WAFFLE_OFFLINE "%HIVE_PATH%"
if errorlevel 1 (
    echo.
    echo reg load failed. Possible causes:
    echo   - Wrong drive letter ^(re-check with diskpart^)
    echo   - The hive is already loaded as HKLM\WAFFLE_OFFLINE from a
    echo     previous run of this script that didn't finish -- try:
    echo       reg unload HKLM\WAFFLE_OFFLINE
    echo     then run this script again.
    goto :eof
)

set CLSID={81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}

echo.
echo Removing credential provider registration for %CLSID% ...
reg delete "HKLM\WAFFLE_OFFLINE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\%CLSID%" /f
if errorlevel 1 (
    echo   ^(Key was not found -- Waffle Jackpot may already be disabled or was never installed on this drive.^)
)

echo.
echo Unloading hive...
reg unload HKLM\WAFFLE_OFFLINE
if errorlevel 1 (
    echo.
    echo WARNING: reg unload failed. The hive may still be mounted as
    echo HKLM\WAFFLE_OFFLINE, and Windows on %WINDOWS_DRIVE%: may not boot
    echo correctly until it's unloaded. Try running:
    echo     reg unload HKLM\WAFFLE_OFFLINE
    echo one more time before rebooting. If it keeps failing, close any
    echo other Registry Editor / reg.exe windows that might still have
    echo a handle into it, then retry.
) else (
    echo.
    echo Done. The Waffle Jackpot tile should no longer appear on
    echo %WINDOWS_DRIVE%:'s logon screen. You can now reboot normally.
)

pause
