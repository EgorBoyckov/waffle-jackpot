#include "Registration.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>

namespace waffle::control {
namespace {

std::wstring KnownFolderPath(REFKNOWNFOLDERID id)
{
    PWSTR pwz = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &pwz)) && pwz)
    {
        result = pwz;
    }
    CoTaskMemFree(pwz);
    return result;
}

std::wstring InstallDirPath()
{
    const std::wstring programFiles = KnownFolderPath(FOLDERID_ProgramFiles);
    return programFiles.empty() ? std::wstring() : programFiles + L"\\WaffleJackpot";
}

std::wstring ConfigDirPath()
{
    const std::wstring programData = KnownFolderPath(FOLDERID_ProgramData);
    return programData.empty() ? std::wstring() : programData + L"\\WaffleJackpot";
}

std::wstring ProviderKeyPath()
{
    return std::wstring(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\Credential Providers\\") +
           kClsidString;
}

std::wstring ClsidKeyPath()
{
    return std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidString;
}

std::wstring InprocKeyPath()
{
    return ClsidKeyPath() + L"\\InprocServer32";
}

// Same key src/CredentialProvider/KillSwitch.cpp tracks
// (HKLM\SOFTWARE\WaffleJackpot\CrashCount). Duplicated here for the same
// reason kClsidString is: Control.exe doesn't link against the provider
// DLL.
constexpr wchar_t kKillSwitchKeyPath[] = L"SOFTWARE\\WaffleJackpot";
constexpr wchar_t kKillSwitchValueName[] = L"CrashCount";

DWORD ReadKillSwitchCount()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kKillSwitchKeyPath, 0, KEY_READ | KEY_WOW64_64KEY, &key) !=
        ERROR_SUCCESS)
    {
        return 0;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG status = RegQueryValueExW(key, kKillSwitchValueName, nullptr, &type, reinterpret_cast<BYTE*>(&value),
                                          &size);
    RegCloseKey(key);
    return (status == ERROR_SUCCESS && type == REG_DWORD) ? value : 0;
}

bool RegistryKeyExists(HKEY root, const std::wstring& subKey)
{
    HKEY key = nullptr;
    const LONG status = RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (key)
    {
        RegCloseKey(key);
    }
    return status == ERROR_SUCCESS;
}

bool FileExists(const std::wstring& path)
{
    if (path.empty())
    {
        return false;
    }
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring FormatWin32Error(DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length ? std::wstring(buffer, length) : L"Unknown error";
    if (buffer)
    {
        LocalFree(buffer);
    }
    while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r'))
    {
        message.pop_back();
    }
    return message;
}

}  // namespace

StatusReport QueryStatus()
{
    StatusReport report;
    report.comRegistered = RegistryKeyExists(HKEY_LOCAL_MACHINE, InprocKeyPath());
    report.providerRegistered = RegistryKeyExists(HKEY_LOCAL_MACHINE, ProviderKeyPath());

    const std::wstring installDir = InstallDirPath();
    if (!installDir.empty())
    {
        report.dllPath = installDir + L"\\WaffleJackpotProvider.dll";
        report.dllPresent = FileExists(report.dllPath);
    }

    const std::wstring configDir = ConfigDirPath();
    if (!configDir.empty())
    {
        report.configPath = configDir + L"\\config.json";
        report.configPresent = FileExists(report.configPath);
    }

    // Same threshold as waffle::cp::kKillSwitchThreshold (spec §9.2: 3
    // consecutive failures/crashes).
    constexpr DWORD kKillSwitchThreshold = 3;
    const DWORD killSwitchCount = ReadKillSwitchCount();
    report.killSwitchCount = static_cast<int>(killSwitchCount);
    report.killSwitchActive = killSwitchCount >= kKillSwitchThreshold;

    return report;
}

bool IsElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    bool elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
    {
        elevated = elevation.TokenIsElevated != 0;
    }
    CloseHandle(token);
    return elevated;
}

bool RelaunchElevated(const std::wstring& arguments)
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
    {
        return false;
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = arguments.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei))
    {
        return false;
    }
    if (sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
    return true;
}

bool EnableProvider(std::wstring* outError)
{
    if (!RegistryKeyExists(HKEY_LOCAL_MACHINE, InprocKeyPath()))
    {
        if (outError)
        {
            *outError = L"Waffle Jackpot Login isn't installed (no COM registration found). Run install.ps1 first.";
        }
        return false;
    }

    HKEY key = nullptr;
    LONG status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, ProviderKeyPath().c_str(), 0, nullptr, 0,
                                   KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS)
    {
        if (outError)
        {
            *outError = L"Could not create the provider registration key: " + FormatWin32Error(status);
        }
        return false;
    }

    static constexpr wchar_t kFriendlyName[] = L"Waffle Jackpot Login";
    status = RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(kFriendlyName),
                             static_cast<DWORD>((wcslen(kFriendlyName) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);

    if (status != ERROR_SUCCESS)
    {
        if (outError)
        {
            *outError = L"Could not set the provider registration value: " + FormatWin32Error(status);
        }
        return false;
    }

    // spec §9.2: this is the one place that's supposed to clear the kill
    // switch -- best-effort, not a reason to fail the whole call if it
    // doesn't work (matches KillSwitch.cpp's own "never a hard failure"
    // stance on registry writes).
    HKEY killSwitchKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kKillSwitchKeyPath, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY, nullptr,
                         &killSwitchKey, nullptr) == ERROR_SUCCESS)
    {
        const DWORD zero = 0;
        RegSetValueExW(killSwitchKey, kKillSwitchValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&zero),
                        sizeof(zero));
        RegCloseKey(killSwitchKey);
    }

    return true;
}

bool DisableProvider(std::wstring* outError)
{
    const LONG status = RegDeleteKeyW(HKEY_LOCAL_MACHINE, ProviderKeyPath().c_str());
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
    {
        if (outError)
        {
            *outError = L"Could not remove the provider registration key: " + FormatWin32Error(status);
        }
        return false;
    }
    return true;
}

bool UninstallProvider(std::wstring* outError)
{
    std::wstring error;

    // 1. Disable first -- same effect as disable.ps1.
    DisableProvider(nullptr);

    // 2. Remove COM registration (recursive: CLSID key has an
    // InprocServer32 subkey).
    const LONG comStatus = SHDeleteKeyW(HKEY_LOCAL_MACHINE, ClsidKeyPath().c_str());
    if (comStatus != ERROR_SUCCESS && comStatus != ERROR_FILE_NOT_FOUND)
    {
        error += L"Could not remove COM registration: " + FormatWin32Error(comStatus) + L"\n";
    }

    // 3. Remove the installed DLL. DeleteFileW succeeds even if LogonUI
    // (a different process) still has it memory-mapped -- NTFS just
    // unlinks the directory entry; the mapping stays valid for whoever
    // already has it open until they unmap it.
    const std::wstring installDir = InstallDirPath();
    if (!installDir.empty())
    {
        const std::wstring dllPath = installDir + L"\\WaffleJackpotProvider.dll";
        if (!DeleteFileW(dllPath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            error += L"Could not remove " + dllPath + L": " + FormatWin32Error(GetLastError()) + L"\n";
        }
        // Best-effort: only succeeds if nothing else was left in this
        // directory. Control.exe and WaffleJackpotDemo.exe are never
        // installed here (see ControlMain.cpp), so in the common case
        // this directory is now empty and this cleans it up; if it
        // isn't (e.g. a user dropped something else in there), leaving
        // it behind isn't an uninstall failure.
        RemoveDirectoryW(installDir.c_str());
    }

    // 4. Remove config.json and its directory.
    const std::wstring configDir = ConfigDirPath();
    if (!configDir.empty())
    {
        const std::wstring configPath = configDir + L"\\config.json";
        if (!DeleteFileW(configPath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            error += L"Could not remove " + configPath + L": " + FormatWin32Error(GetLastError()) + L"\n";
        }
        RemoveDirectoryW(configDir.c_str());
    }

    // 5. Remove the kill-switch key too.
    const LONG killSwitchStatus = RegDeleteKeyW(HKEY_LOCAL_MACHINE, kKillSwitchKeyPath);
    if (killSwitchStatus != ERROR_SUCCESS && killSwitchStatus != ERROR_FILE_NOT_FOUND)
    {
        error += L"Could not remove the kill-switch registry key: " + FormatWin32Error(killSwitchStatus) + L"\n";
    }

    if (!error.empty())
    {
        if (outError)
        {
            *outError = error;
        }
        return false;
    }
    return true;
}

}  // namespace waffle::control
