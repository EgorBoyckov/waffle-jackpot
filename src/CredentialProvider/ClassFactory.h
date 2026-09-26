// Adapted from Microsoft's official Windows-classic-samples repository:
// Samples/Win7Samples/security/credentialproviders/helpers/Dll.cpp
// https://github.com/microsoft/Windows-classic-samples (MIT license;
// Copyright (c) Microsoft Corporation; see
// https://github.com/microsoft/Windows-classic-samples/blob/main/LICENSE).
// The class factory is boilerplate identical in shape for any COM
// in-proc server, V1 or V2 credential provider alike -- project spec
// §6.2: "не изобретай COM-скелет с нуля". Split out of dllmain.cpp's
// combined Dll.cpp for one class per file; behavior is unchanged from the
// original CClassFactory.
#pragma once

#include <unknwn.h>
#include <windows.h>

// Creates a JackpotProvider instance. Defined in JackpotProvider.cpp.
HRESULT JackpotProvider_CreateInstance(REFIID riid, void** ppv);

class ClassFactory : public IClassFactory
{
public:
    ClassFactory() : _cRef(1) {}

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    IFACEMETHODIMP LockServer(BOOL bLock) override;

private:
    ~ClassFactory() = default;
    LONG _cRef;
};

// Entry point DllGetClassObject forwards to: constructs a ClassFactory for
// rclsid if it's one this DLL serves.
HRESULT ClassFactory_CreateInstance(REFCLSID rclsid, REFIID riid, void** ppv);
