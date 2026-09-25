#pragma once

#include "Core/Config.h"
#include "JackpotCredential.h"

// The provider LogonUI talks to first: it advertises one tile per
// existing Windows user and hands out a JackpotCredential for each on
// request.
//
// This implements ICredentialProviderSetUserArray (spec §6.2) rather than
// enumerating its own accounts the way older (pre-Windows 8)
// V1-style samples do -- LogonUI hands us the real user list via
// SetUserArray, one ICredentialProviderUser per tile, and we read each
// user's SID and qualified name from that interface rather than
// inventing our own. I could not locate an actual Microsoft-published V2
// sample using this interface to base this file on (see the Phase 4
// commit message for what I checked); this class follows the documented
// ICredentialProvider / ICredentialProviderSetUserArray / ICredentialProviderUser
// contracts from Microsoft Learn, not a copied sample.
class JackpotProvider : public ICredentialProvider, public ICredentialProviderSetUserArray
{
public:
    JackpotProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // ICredentialProvider
    IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD dwFlags) override;
    IFACEMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) override;
    IFACEMETHODIMP Advise(ICredentialProviderEvents* pcpe, UINT_PTR upAdviseContext) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP GetFieldDescriptorCount(DWORD* pdwCount) override;
    IFACEMETHODIMP GetFieldDescriptorAt(DWORD dwIndex, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) override;
    IFACEMETHODIMP GetCredentialCount(DWORD* pdwCount, DWORD* pdwDefault, BOOL* pbAutoLogonWithDefault) override;
    IFACEMETHODIMP GetCredentialAt(DWORD dwIndex, ICredentialProviderCredential** ppcpc) override;

    // ICredentialProviderSetUserArray
    IFACEMETHODIMP SetUserArray(ICredentialProviderUserArray* users) override;

    friend HRESULT JackpotProvider_CreateInstance(REFIID riid, void** ppv);

private:
    ~JackpotProvider();

    // False for CPUS_CREDUI/CPUS_CHANGE_PASSWORD (spec §6.3, always
    // E_NOTIMPL from SetUsageScenario itself so this never applies), for
    // a disabled config (spec §7 "enabled"), and for a remote session
    // unless config.showInRemoteSessions is set (spec §6.3). Backs the
    // "fail silent, GetCredentialCount returns 0" contract from spec
    // §9.1 -- never a hard failure just because we've decided not to
    // show a tile.
    bool ShouldParticipate() const;

    LONG _cRef;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO _cpus;
    bool _participate;
    waffle::Config _config;

    ICredentialProviderUserArray* _pUserArray;

    ICredentialProviderEvents* _pCredProvEvents;
    UINT_PTR _upAdviseContext;
};

// Called by ClassFactory::CreateInstance for CLSID_JackpotProvider.
HRESULT JackpotProvider_CreateInstance(REFIID riid, void** ppv);
