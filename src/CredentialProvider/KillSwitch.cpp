#include "KillSwitch.h"

#include <windows.h>

namespace waffle::cp {
namespace {

constexpr wchar_t kKeyPath[] = L"SOFTWARE\\WaffleJackpot";
constexpr wchar_t kValueName[] = L"CrashCount";

DWORD ReadCount()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kKeyPath, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
    {
        return 0;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG status = RegQueryValueExW(key, kValueName, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return (status == ERROR_SUCCESS && type == REG_DWORD) ? value : 0;
}

void WriteCount(DWORD value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kKeyPath, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key,
                         nullptr) != ERROR_SUCCESS)
    {
        // spec §9.1: never let a registry write failure become a hard
        // failure. Worst case, the kill switch just doesn't track this
        // one event.
        return;
    }
    RegSetValueExW(key, kValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

}  // namespace

void RecordInitializationStart()
{
    WriteCount(ReadCount() + 1);
}

void RecordCleanShutdown()
{
    const DWORD current = ReadCount();
    if (current > 0)
    {
        WriteCount(current - 1);
    }
}

void RecordSuccessfulLogon()
{
    WriteCount(0);
}

bool IsKillSwitchActive()
{
    return ReadCount() >= static_cast<DWORD>(kKillSwitchThreshold);
}

}  // namespace waffle::cp
