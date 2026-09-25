#include "JackpotProvider.h"

#include <new>
#include <string>

#include <propkey.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "KillSwitch.h"

namespace {

std::wstring ResolveConfigPath()
{
    PWSTR pwzProgramData = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &pwzProgramData)) && pwzProgramData)
    {
        path = pwzProgramData;
        path += L"\\WaffleJackpot\\config.json";
    }
    CoTaskMemFree(pwzProgramData);
    return path;
}

}  // namespace

JackpotProvider::JackpotProvider()
    : _cRef(1),
      _cpus(CPUS_INVALID),
      _participate(false),
      _pUserArray(nullptr),
      _pCredProvEvents(nullptr),
      _upAdviseContext(0)
{
    // Owner/ACL validation from spec §7 ("CP проверяет владельца файла и
    // при несоответствии игнорирует конфиг") still isn't implemented.
    // install.ps1 (Phase 6) now actually sets the ACL this check would
    // validate against, so the excuse that there was nothing real to
    // check has expired -- this is a genuine gap, not deferred to a
    // later phase by the plan itself (§13's phase table doesn't list ACL
    // validation under Phase 7 either). Flagging it here rather than
    // quietly carrying it forward again. ConfigLoader's existing
    // malformed/missing-file fallback to defaults still covers "no
    // config file present" safely (never throws, spec §9.1) regardless.
    const std::wstring configPath = ResolveConfigPath();
    _config = configPath.empty() ? waffle::Config::Default() : waffle::ConfigLoader::LoadFromFile(configPath);

    // spec §9.2 kill switch: count this initialization; RecordCleanShutdown
    // (destructor) balances it out unless we crash before getting there.
    waffle::cp::RecordInitializationStart();
}

JackpotProvider::~JackpotProvider()
{
    if (_pUserArray)
    {
        _pUserArray->Release();
        _pUserArray = nullptr;
    }
    if (_pCredProvEvents)
    {
        _pCredProvEvents->Release();
        _pCredProvEvents = nullptr;
    }

    // Reaching here means this session didn't crash -- see KillSwitch.h.
    waffle::cp::RecordCleanShutdown();
}

IFACEMETHODIMP JackpotProvider::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(JackpotProvider, ICredentialProvider),
        QITABENT(JackpotProvider, ICredentialProviderSetUserArray),
        {nullptr, 0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) JackpotProvider::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) JackpotProvider::Release()
{
    LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef)
    {
        delete this;
    }
    return cRef;
}

bool JackpotProvider::ShouldParticipate() const
{
    if (!_config.enabled)
    {
        return false;
    }
    // spec §6.3: RDP sessions get the normal Windows logon by default.
    if (GetSystemMetrics(SM_REMOTESESSION) && !_config.showInRemoteSessions)
    {
        return false;
    }
    // spec §9.2: three crashes/failed inits in a row and we stop showing
    // a tile until a human runs `WaffleJackpotControl.exe enable`. Not a
    // hard failure of our own (spec §9.1) -- just zero tiles, same as a
    // disabled config.
    if (waffle::cp::IsKillSwitchActive())
    {
        return false;
    }
    return true;
}

IFACEMETHODIMP JackpotProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD /*dwFlags*/)
{
    _cpus = cpus;

    switch (cpus)
    {
        case CPUS_LOGON:
        case CPUS_UNLOCK_WORKSTATION:
            break;
        case CPUS_CREDUI:
        case CPUS_CHANGE_PASSWORD:
            // spec §6.3: no waffles in a "Run as administrator" consent
            // dialog or the change-password flow.
            return E_NOTIMPL;
        default:
            return E_INVALIDARG;
    }

    _participate = ShouldParticipate();
    return S_OK;
}

IFACEMETHODIMP JackpotProvider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* /*pcpcs*/)
{
    // We don't support being handed a pre-existing serialized credential
    // (e.g. a Remote Desktop client passing creds through) -- only
    // interactive password entry via our own tile.
    return E_NOTIMPL;
}

IFACEMETHODIMP JackpotProvider::Advise(ICredentialProviderEvents* pcpe, UINT_PTR upAdviseContext)
{
    if (_pCredProvEvents)
    {
        _pCredProvEvents->Release();
    }
    pcpe->AddRef();
    _pCredProvEvents = pcpe;
    _upAdviseContext = upAdviseContext;
    return S_OK;
}

IFACEMETHODIMP JackpotProvider::UnAdvise()
{
    if (_pCredProvEvents)
    {
        _pCredProvEvents->Release();
        _pCredProvEvents = nullptr;
    }
    return S_OK;
}

IFACEMETHODIMP JackpotProvider::GetFieldDescriptorCount(DWORD* pdwCount)
{
    if (!pdwCount)
    {
        return E_INVALIDARG;
    }
    *pdwCount = JFI_NUM_FIELDS;
    return S_OK;
}

IFACEMETHODIMP JackpotProvider::GetFieldDescriptorAt(DWORD dwIndex, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd)
{
    if (dwIndex >= JFI_NUM_FIELDS || !ppcpfd)
    {
        return E_INVALIDARG;
    }
    return FieldDescriptorCoAllocCopy(s_rgCredProvFieldDescriptors[dwIndex], ppcpfd);
}

IFACEMETHODIMP JackpotProvider::GetCredentialCount(DWORD* pdwCount, DWORD* pdwDefault, BOOL* pbAutoLogonWithDefault)
{
    if (!pdwCount || !pdwDefault || !pbAutoLogonWithDefault)
    {
        return E_INVALIDARG;
    }

    *pdwDefault = 0;
    *pbAutoLogonWithDefault = FALSE;

    if (!_participate || !_pUserArray)
    {
        // spec §9.1: a provider that has decided not to participate (or
        // hit an initialization problem) just shows zero tiles -- never a
        // hard failure that could take LogonUI down with it.
        *pdwCount = 0;
        return S_OK;
    }

    return _pUserArray->GetCount(pdwCount);
}

IFACEMETHODIMP JackpotProvider::GetCredentialAt(DWORD dwIndex, ICredentialProviderCredential** ppcpc)
{
    if (!ppcpc)
    {
        return E_INVALIDARG;
    }
    *ppcpc = nullptr;

    if (!_participate || !_pUserArray)
    {
        return E_INVALIDARG;
    }

    ICredentialProviderUser* pUser = nullptr;
    HRESULT hr = _pUserArray->GetAt(dwIndex, &pUser);
    if (FAILED(hr))
    {
        return hr;
    }

    PWSTR pwzQualifiedUserName = nullptr;
    hr = pUser->GetStringValue(PKEY_Identity_QualifiedUserName, &pwzQualifiedUserName);
    if (SUCCEEDED(hr))
    {
        PWSTR pwzSid = nullptr;
        hr = pUser->GetSid(&pwzSid);
        if (SUCCEEDED(hr))
        {
            auto* credential = new (std::nothrow) JackpotCredential();
            if (credential)
            {
                hr = credential->Initialize(_cpus, pwzQualifiedUserName, pwzSid, _config);
                if (SUCCEEDED(hr))
                {
                    hr = credential->QueryInterface(IID_PPV_ARGS(ppcpc));
                }
                credential->Release();
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }
            CoTaskMemFree(pwzSid);
        }
        CoTaskMemFree(pwzQualifiedUserName);
    }

    pUser->Release();
    return hr;
}

IFACEMETHODIMP JackpotProvider::SetUserArray(ICredentialProviderUserArray* users)
{
    if (!users)
    {
        return E_INVALIDARG;
    }
    if (_pUserArray)
    {
        _pUserArray->Release();
    }
    users->AddRef();
    _pUserArray = users;
    return S_OK;
}

HRESULT JackpotProvider_CreateInstance(REFIID riid, void** ppv)
{
    *ppv = nullptr;

    auto* provider = new (std::nothrow) JackpotProvider();
    HRESULT hr = provider ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
    {
        hr = provider->QueryInterface(riid, ppv);
        provider->Release();
    }
    return hr;
}
