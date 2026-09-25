// See ClassFactory.h for attribution: adapted from Microsoft's
// Windows-classic-samples helpers/Dll.cpp (MIT license).
#include "ClassFactory.h"

#include <new>

#include <shlwapi.h>

#include "dllmain.h"
#include "guids.h"

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(ClassFactory, IClassFactory),
        {nullptr, 0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release()
{
    LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef)
    {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    HRESULT hr;
    if (!pUnkOuter)
    {
        hr = JackpotProvider_CreateInstance(riid, ppv);
    }
    else
    {
        *ppv = nullptr;
        hr = CLASS_E_NOAGGREGATION;
    }
    return hr;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL bLock)
{
    if (bLock)
    {
        DllAddRef();
    }
    else
    {
        DllRelease();
    }
    return S_OK;
}

HRESULT ClassFactory_CreateInstance(REFCLSID rclsid, REFIID riid, void** ppv)
{
    *ppv = nullptr;

    HRESULT hr;
    if (CLSID_JackpotProvider == rclsid)
    {
        auto* factory = new (std::nothrow) ClassFactory();
        if (factory)
        {
            hr = factory->QueryInterface(riid, ppv);
            factory->Release();
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    else
    {
        hr = CLASS_E_CLASSNOTAVAILABLE;
    }
    return hr;
}
