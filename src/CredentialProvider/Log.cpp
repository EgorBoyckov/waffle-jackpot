#include "Log.h"

#include <cstdarg>
#include <cstdio>

namespace waffle::cp {
namespace {

constexpr DWORD kMaxLogBytes = 512 * 1024;

void BuildLogPath(wchar_t* out, size_t cch)
{
    wchar_t base[MAX_PATH] = L"C:\\ProgramData";
    const DWORD n = GetEnvironmentVariableW(L"ProgramData", base, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        wcscpy_s(base, L"C:\\ProgramData");
    }
    wchar_t dir[MAX_PATH];
    swprintf_s(dir, L"%s\\WaffleJackpot", base);
    CreateDirectoryW(dir, nullptr);
    swprintf_s(dir, L"%s\\WaffleJackpot\\logs", base);
    CreateDirectoryW(dir, nullptr);
    swprintf_s(out, cch, L"%s\\provider.log", dir);
}

}  // namespace

void Log(const wchar_t* format, ...)
{
    wchar_t path[MAX_PATH * 2];
    BuildLogPath(path, _countof(path));

    wchar_t msg[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(msg, _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t exe[MAX_PATH] = L"?";
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* exeName = wcsrchr(exe, L'\\');
    exeName = exeName ? exeName + 1 : exe;

    wchar_t line[1400];
    swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u [pid %lu tid %lu %s] %s\r\n", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId(),
               exeName, msg);

    char utf8[4200];
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1)
    {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) && size.QuadPart > kMaxLogBytes)
    {
        // Crude rotation: start over. The log is diagnostic only.
        SetFilePointer(file, 0, nullptr, FILE_BEGIN);
        SetEndOfFile(file);
    }
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, nullptr);
    CloseHandle(file);
}

HRESULT LogHr(const wchar_t* step, HRESULT hr)
{
    Log(L"%s -> 0x%08lX", step, static_cast<unsigned long>(hr));
    return hr;
}

}  // namespace waffle::cp
