#pragma once

#include <string>

namespace waffle::control {

// Same CLSID as src/CredentialProvider/guids.h's CLSID_JackpotProvider
// ({81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}) -- duplicated as a string
// here rather than shared, since Control.exe only ever needs the string
// form for registry paths and doesn't link against the provider DLL.
inline constexpr wchar_t kClsidString[] = L"{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}";

// What Status (CLI) / the GUI's Status button report -- mirrors
// installer/uninstall.ps1's own idea of "what's actually there" so
// Control.exe and the PowerShell scripts never disagree about state.
struct StatusReport
{
    bool comRegistered = false;
    bool providerRegistered = false;
    bool dllPresent = false;
    bool configPresent = false;
    std::wstring dllPath;
    std::wstring configPath;

    bool IsFullyInstalled() const { return comRegistered && dllPresent; }
    bool IsEnabled() const { return providerRegistered; }
};

StatusReport QueryStatus();

// True if this process token is elevated (member of the "High" or
// "System" integrity / has TokenIsElevated set) -- used to decide whether
// a modifying command needs to relaunch itself via RelaunchElevated
// rather than fail outright (spec §10.1: elevation "по требованию", not
// baked into the app manifest, so Status/Run Demo never force a UAC
// prompt).
bool IsElevated();

// Re-launches this same .exe with the given argument string via the
// "runas" verb (triggers the UAC elevation prompt) and waits for it to
// exit. Returns false if the user declined the prompt or the relaunch
// itself failed to start.
bool RelaunchElevated(const std::wstring& arguments);

// Registry-only; never touches the DLL file or config.json. Idempotent:
// safe to call when already in the target state.
bool EnableProvider(std::wstring* outError);
bool DisableProvider(std::wstring* outError);

// Removes the provider's registration (COM + Credential Providers key),
// the installed DLL, and config.json/its directory. Deliberately does
// NOT touch Control.exe or WaffleJackpotDemo.exe -- see ControlMain.cpp's
// comment on why those are never installed under the same ACL-locked
// directory this removes, which is what keeps this simple (no
// self-delete-while-running problem to solve).
bool UninstallProvider(std::wstring* outError);

}  // namespace waffle::control
