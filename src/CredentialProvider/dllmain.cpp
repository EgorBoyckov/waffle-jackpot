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
    return ClassFactory_CreateInstance(rclsid, riid, ppv);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD dwReason, void* /*reserved*/)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDll);
            g_hinst = hinstDll;
            break;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
