// WaffleJackpotControl.exe -- the single management entry point for the
// Credential Provider (project spec §10.1): a GUI ([Enable] [Disable]
// [Uninstall] [Run Demo] [Status]) when launched with no arguments, and a
// CLI (status/enable/disable/uninstall/demo) for scripts and recovery
// when launched with one.
//
// Elevation is requested on demand (spec §10.1: "повышение через runas по
// требованию"), not baked into the app manifest -- the manifest here is
// plain "asInvoker" so Status and Run Demo never trigger a UAC prompt;
// only Enable/Disable/Uninstall do, and only when not already elevated.
//
// Deliberately never installs itself (or WaffleJackpotDemo.exe) into the
// ACL-locked %ProgramFiles%\WaffleJackpot\ directory install.ps1 owns --
// Registration::UninstallProvider only ever removes files it knows
// install.ps1 put there (the provider DLL, config.json), so this .exe
// never has to delete itself while running (the classic self-delete
// problem other uninstallers work around with a detached helper process).
// Build it, keep it, run it from wherever -- a shortcut, the build
// output, a USB stick during recovery.

#include <windows.h>

#include <shellapi.h>

#include <sstream>
#include <string>

#include "Registration.h"

namespace {

using waffle::control::DisableProvider;
using waffle::control::EnableProvider;
using waffle::control::IsElevated;
using waffle::control::QueryStatus;
using waffle::control::RelaunchElevated;
using waffle::control::StatusReport;
using waffle::control::UninstallProvider;

constexpr int kIdStatus = 101;
constexpr int kIdEnable = 102;
constexpr int kIdDisable = 103;
constexpr int kIdUninstall = 104;
constexpr int kIdRunDemo = 105;

std::wstring FormatStatus(const StatusReport& status)
{
    std::wstringstream ss;
    ss << L"COM registration:      " << (status.comRegistered ? L"present" : L"absent") << L"\n"
       << L"Provider registration: " << (status.providerRegistered ? L"present (enabled)" : L"absent (disabled)")
       << L"\n"
       << L"DLL:                   " << (status.dllPresent ? status.dllPath : L"not found") << L"\n"
       << L"Config:                " << (status.configPresent ? status.configPath : L"not found (defaults apply)")
       << L"\n"
       << L"Kill switch:           " << status.killSwitchCount << L"/3 recent failures/crashes"
       << (status.killSwitchActive ? L" -- ACTIVE (tile hidden)" : L"") << L"\n\n";

    if (!status.comRegistered && !status.dllPresent)
    {
        ss << L"Not installed. Run install.ps1 as Administrator.";
    }
    else if (status.killSwitchActive)
    {
        ss << L"Installed and enabled, but the kill switch has tripped (spec §9.2): 3 failed "
              L"initializations/crashes in a row. The tile will stay hidden until you click Enable "
              L"again (that also resets this counter).";
    }
    else if (!status.IsEnabled())
    {
        ss << L"Installed, but disabled. Run enable.ps1 or the Enable button.";
    }
    else
    {
        ss << L"Installed and enabled -- it will appear on the next logon screen.";
    }
    return ss.str();
}

// Finds WaffleJackpotDemo.exe next to this executable. Works regardless
// of where the user has put Control.exe, since it never assumes an
// install location for itself.
std::wstring FindDemoExePath()
{
    wchar_t exePath[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath, len);
    const auto pos = path.find_last_of(L"\\/");
    const std::wstring dir = (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
    return dir + L"\\WaffleJackpotDemo.exe";
}

int RunDemo()
{
    const std::wstring demoPath = FindDemoExePath();
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", demoPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxW(nullptr, (L"Could not launch:\n" + demoPath).c_str(), L"Waffle Jackpot Control",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}

// --- CLI ---------------------------------------------------------------

void AttachToParentConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        FILE* unused;
        freopen_s(&unused, "CONOUT$", "w", stdout);
        freopen_s(&unused, "CONOUT$", "w", stderr);
    }
}

void PrintLine(const std::wstring& line)
{
    DWORD written = 0;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    std::wstring withNewline = line + L"\n";
    WriteConsoleW(out, withNewline.c_str(), static_cast<DWORD>(withNewline.size()), &written, nullptr);
}

int RunModifyingCommand(const std::wstring& command, bool (*action)(std::wstring*))
{
    if (!IsElevated())
    {
        // spec §10.1: elevate on demand rather than always requiring it.
        RelaunchElevated(command);
        return 0;
    }
    std::wstring error;
    const bool ok = action(&error);
    PrintLine(ok ? (command + L": OK") : (command + L": FAILED - " + error));
    return ok ? 0 : 1;
}

int RunCli(const std::wstring& command)
{
    AttachToParentConsole();

    if (command == L"status")
    {
        PrintLine(FormatStatus(QueryStatus()));
        return 0;
    }
    if (command == L"enable")
    {
        return RunModifyingCommand(L"enable", &EnableProvider);
    }
    if (command == L"disable")
    {
        return RunModifyingCommand(L"disable", &DisableProvider);
    }
    if (command == L"uninstall")
    {
        return RunModifyingCommand(L"uninstall", &UninstallProvider);
    }
    if (command == L"demo")
    {
        return RunDemo();
    }

    PrintLine(L"Usage: WaffleJackpotControl.exe [status|enable|disable|uninstall|demo]");
    PrintLine(L"Run with no arguments for the GUI.");
    return 1;
}

// --- GUI -----------------------------------------------------------------

void ShowResult(HWND owner, const wchar_t* title, bool ok, const std::wstring& detail)
{
    MessageBoxW(owner, detail.c_str(), title, MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
}

// Runs a modifying action if already elevated; otherwise offers to
// relaunch elevated (back into the GUI, since there's no simple way to
// report a background CLI action's result back into this window) and
// closes this (unelevated) window -- the user re-clicks the same button
// in the elevated copy that opens.
void HandleModifyingButton(HWND hwnd, const wchar_t* title, bool (*action)(std::wstring*))
{
    if (!IsElevated())
    {
        const int choice =
            MessageBoxW(hwnd,
                         L"This action requires administrator rights.\n\nRelaunch Waffle Jackpot Control "
                         L"elevated now?",
                         title, MB_YESNO | MB_ICONWARNING);
        if (choice == IDYES)
        {
            RelaunchElevated(L"");
            DestroyWindow(hwnd);
        }
        return;
    }

    std::wstring error;
    const bool ok = action(&error);
    ShowResult(hwnd, title, ok, ok ? L"Done." : error);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            const int buttonWidth = 220, buttonHeight = 32, gap = 10, left = 20;
            int top = 20;
            auto addButton = [&](const wchar_t* label, int id) {
                CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, left, top, buttonWidth,
                              buttonHeight, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance,
                              nullptr);
                top += buttonHeight + gap;
            };
            addButton(L"Status", kIdStatus);
            addButton(L"Enable", kIdEnable);
            addButton(L"Disable", kIdDisable);
            addButton(L"Uninstall", kIdUninstall);
            addButton(L"Run Demo", kIdRunDemo);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case kIdStatus:
                    MessageBoxW(hwnd, FormatStatus(QueryStatus()).c_str(), L"Waffle Jackpot -- Status",
                                MB_OK | MB_ICONINFORMATION);
                    return 0;
                case kIdEnable:
                    HandleModifyingButton(hwnd, L"Waffle Jackpot -- Enable", &EnableProvider);
                    return 0;
                case kIdDisable:
                    if (MessageBoxW(hwnd, L"Disable Waffle Jackpot Login? It will stop appearing on the logon "
                                           L"screen until re-enabled.",
                                     L"Waffle Jackpot -- Disable", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        HandleModifyingButton(hwnd, L"Waffle Jackpot -- Disable", &DisableProvider);
                    }
                    return 0;
                case kIdUninstall:
                    if (MessageBoxW(hwnd,
                                     L"Completely remove Waffle Jackpot Login? This deletes the provider DLL, "
                                     L"its registration, and config.json.",
                                     L"Waffle Jackpot -- Uninstall", MB_YESNO | MB_ICONWARNING) == IDYES)
                    {
                        HandleModifyingButton(hwnd, L"Waffle Jackpot -- Uninstall", &UninstallProvider);
                    }
                    return 0;
                case kIdRunDemo:
                    RunDemo();
                    return 0;
                default:
                    return 0;
            }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int RunGui(HINSTANCE instance)
{
    const wchar_t kClassName[] = L"WaffleJackpotControlWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    const HWND hwnd = CreateWindowExW(0, kClassName, L"Waffle Jackpot Control", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 280, 260, nullptr, nullptr, instance, nullptr);
    if (!hwnd)
    {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prevInstance*/, LPWSTR /*cmdLine*/, int /*showCmd*/)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    int exitCode;
    if (argv && argc > 1)
    {
        exitCode = RunCli(argv[1]);
    }
    else
    {
        exitCode = RunGui(instance);
    }

    if (argv)
    {
        LocalFree(argv);
    }
    return exitCode;
}
