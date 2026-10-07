// Adapted from Microsoft's official Windows-classic-samples repository:
// Samples/Win7Samples/security/credentialproviders/helpers/Dll.cpp
// https://github.com/microsoft/Windows-classic-samples (MIT license;
// Copyright (c) Microsoft Corporation; see
// https://github.com/microsoft/Windows-classic-samples/blob/main/LICENSE).
// This is the DLL's standard in-proc-server surface (DllMain,
// DllGetClassObject, DllCanUnloadNow) -- the same shape for any COM
// server, so reused per project spec §6.2 rather than rewritten from
// scratch. Self-registration (DllRegisterServer/DllUnregisterServer) is
// intentionally not implemented here: registration is install.ps1's job
// (Phase 6), writing the exact registry keys spec §10.2 lists, not a
// regsvr32-driven default.

#include "dllmain.h"

#include "ClassFactory.h"
#include "KillSwitch.h"
#include "Log.h"

namespace {
LONG g_cRef = 0;
}  // namespace

HINSTANCE g_hinst = nullptr;

void DllAddRef()
{
    InterlockedIncrement(&g_cRef);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRef);
}

STDAPI DllCanUnloadNow()
{
    return (g_cRef > 0) ? S_FALSE : S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    const HRESULT hr = ClassFactory_CreateInstance(rclsid, riid, ppv);
    waffle::cp::Log(L"DllGetClassObject -> 0x%08lX", static_cast<unsigned long>(hr));
    return hr;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD dwReason, void* reserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDll);
            g_hinst = hinstDll;
            waffle::cp::Log(L"DLL_PROCESS_ATTACH (provider DLL loaded)");
            break;
        case DLL_PROCESS_DETACH:
            // LogonUI is usually torn down without releasing the provider,
            // which would leave the kill-switch counter incremented and
            // eventually disable the tile after a few reboots. A real
            // crash never reaches here, so the counter still works.
            waffle::cp::RecordCleanShutdown();
            waffle::cp::Log(L"DLL_PROCESS_DETACH (reserved=%p)", reserved);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
