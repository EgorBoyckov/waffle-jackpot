#pragma once

#include <windows.h>

namespace waffle::cp {

// Appends a timestamped line to %ProgramData%\WaffleJackpot\logs\provider.log
// (falls back to C:\ProgramData if the known-folder lookup fails). Never
// throws, never logs passwords. Safe to call from DllMain and from LogonUI's
// SYSTEM context. The file is truncated when it exceeds ~512 KB.
void Log(const wchar_t* format, ...);

// Logs an HRESULT result for a named step; returns hr unchanged so it can
// wrap return statements.
HRESULT LogHr(const wchar_t* step, HRESULT hr);

}  // namespace waffle::cp
