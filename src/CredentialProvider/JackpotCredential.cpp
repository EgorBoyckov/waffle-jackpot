// Must precede any header pulling in windows.h/winnt.h (which has its own
// smaller built-in set of NTSTATUS codes), or STATUS_SUCCESS and friends
// get defined twice. Same guard the Microsoft sample's
// CSampleCredential.cpp uses -- and it has to come before this file's own
// first include, since JackpotCredential.h -> FieldDescriptors.h ->
// helpers.h already drags in windows.h.
#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif

#include "JackpotCredential.h"

#include <shlwapi.h>
#include <wincred.h>

#include "guids.h"
#include "helpers.h"

JackpotCredential::JackpotCredential()
    : _cRef(1),
      _cpus(CPUS_INVALID),
      _pCredProvCredentialEvents(nullptr),
      _pwzSid(nullptr),
      _rgFieldStrings{}
{
}

JackpotCredential::~JackpotCredential()
{
    ClearPasswordField();

    for (PWSTR& field : _rgFieldStrings)
    {
        CoTaskMemFree(field);
        field = nullptr;
    }

    CoTaskMemFree(_pwzSid);
    _pwzSid = nullptr;

    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
        _pCredProvCredentialEvents = nullptr;
    }
}

IFACEMETHODIMP JackpotCredential::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(JackpotCredential, ICredentialProviderCredential),
        QITABENT(JackpotCredential, ICredentialProviderCredential2),
        {nullptr, 0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) JackpotCredential::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) JackpotCredential::Release()
{
    LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef)
    {
        delete this;
    }
    return cRef;
}

HRESULT JackpotCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, PCWSTR pwzQualifiedUserName,
                                       PCWSTR pwzSid)
{
    if (!pwzQualifiedUserName || !pwzSid)
    {
        return E_INVALIDARG;
    }

    _cpus = cpus;
    _qualifiedUserName = pwzQualifiedUserName;

    HRESULT hr = SHStrDupW(pwzSid, &_pwzSid);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = SHStrDupW(L"WAFFLE JACKPOT", &_rgFieldStrings[JFI_LARGE_TEXT]);
    if (SUCCEEDED(hr))
    {
        // No slot machine yet in Phase 4 -- this tile is a plain password
        // login. Phase 5 replaces this string with the jackpot-gated
        // status text from spec §6.1.
        hr = SHStrDupW(L"Enter your Windows credentials", &_rgFieldStrings[JFI_SMALL_TEXT]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"", &_rgFieldStrings[JFI_PASSWORD]);
    }

    return hr;
}

IFACEMETHODIMP JackpotCredential::Advise(ICredentialProviderCredentialEvents* pcpce)
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
    }
    pcpce->AddRef();
    _pCredProvCredentialEvents = pcpce;
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::UnAdvise()
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
        _pCredProvCredentialEvents = nullptr;
    }
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::SetSelected(BOOL* pbAutoLogon)
{
    *pbAutoLogon = FALSE;
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::SetDeselected()
{
    // Don't leave a typed password sitting in memory (or on screen) once
    // the tile isn't the active one -- spec §6.5.
    ClearPasswordField();
    HRESULT hr = SHStrDupW(L"", &_rgFieldStrings[JFI_PASSWORD]);
    if (SUCCEEDED(hr) && _pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->SetFieldString(this, JFI_PASSWORD, L"");
    }
    return hr;
}

IFACEMETHODIMP JackpotCredential::GetFieldState(DWORD dwFieldID, CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                                                 CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis)
{
    if (dwFieldID >= JFI_NUM_FIELDS || !pcpfs || !pcpfis)
    {
        return E_INVALIDARG;
    }
    *pcpfs = s_rgFieldStatePairs[dwFieldID].cpfs;
    *pcpfis = s_rgFieldStatePairs[dwFieldID].cpfis;
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::GetStringValue(DWORD dwFieldID, PWSTR* ppwsz)
{
    if (!ppwsz)
    {
        return E_INVALIDARG;
    }

    switch (dwFieldID)
    {
        case JFI_LARGE_TEXT:
        case JFI_SMALL_TEXT:
        case JFI_PASSWORD:
            return SHStrDupW(_rgFieldStrings[dwFieldID] ? _rgFieldStrings[dwFieldID] : L"", ppwsz);
        default:
            return E_INVALIDARG;
    }
}

IFACEMETHODIMP JackpotCredential::GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp)
{
    if (dwFieldID != JFI_TILEIMAGE || !phbmp)
    {
        return E_INVALIDARG;
    }

    HBITMAP hbmp = CreateTileBitmap();
    if (!hbmp)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    *phbmp = hbmp;
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::GetCheckboxValue(DWORD /*dwFieldID*/, BOOL* /*pbChecked*/, PWSTR* /*ppwszLabel*/)
{
    // No checkbox fields in this tile.
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo)
{
    if (dwFieldID != JFI_SUBMIT_BUTTON || !pdwAdjacentTo)
    {
        return E_INVALIDARG;
    }
    *pdwAdjacentTo = JFI_PASSWORD;
    return S_OK;
}

IFACEMETHODIMP JackpotCredential::GetComboBoxValueCount(DWORD /*dwFieldID*/, DWORD* /*pcItems*/,
                                                         DWORD* /*pdwSelectedItem*/)
{
    // No combo box fields in this tile.
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::GetComboBoxValueAt(DWORD /*dwFieldID*/, DWORD /*dwItem*/, PWSTR* /*ppwszItem*/)
{
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::SetStringValue(DWORD dwFieldID, PCWSTR pwz)
{
    if (dwFieldID != JFI_PASSWORD || !pwz)
    {
        return E_INVALIDARG;
    }

    ClearPasswordField();
    return SHStrDupW(pwz, &_rgFieldStrings[JFI_PASSWORD]);
}

IFACEMETHODIMP JackpotCredential::SetCheckboxValue(DWORD /*dwFieldID*/, BOOL /*bChecked*/)
{
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::SetComboBoxSelectedValue(DWORD /*dwFieldID*/, DWORD /*dwSelectedItem*/)
{
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::CommandLinkClicked(DWORD /*dwFieldID*/)
{
    // No command-link fields until Phase 5's PULL! button.
    return E_INVALIDARG;
}

IFACEMETHODIMP JackpotCredential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
                                                     CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
                                                     PWSTR* ppwszOptionalStatusText,
                                                     CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    if (ppwszOptionalStatusText)
    {
        *ppwszOptionalStatusText = nullptr;
    }
    if (pcpsiOptionalStatusIcon)
    {
        *pcpsiOptionalStatusIcon = CPSI_NONE;
    }

    PWSTR pwzProtectedPassword = nullptr;
    HRESULT hr = ProtectIfNecessaryAndCopyPassword(
        _rgFieldStrings[JFI_PASSWORD] ? _rgFieldStrings[JFI_PASSWORD] : L"", _cpus, &pwzProtectedPassword);
    if (FAILED(hr))
    {
        return hr;
    }

    KERB_INTERACTIVE_UNLOCK_LOGON kiul;
    // V2 providers get a single pre-qualified "DOMAIN\User" (or
    // "AzureAD\user@example.com", etc.) string per user from
    // ICredentialProviderUser rather than separate domain/username
    // fields, so the domain argument here is deliberately empty -- the
    // qualification already lives inside _qualifiedUserName. I wasn't
    // able to locate an actual Microsoft V2 sample to confirm this
    // against (see the Phase 4 commit message); this follows the
    // documented shape of KERB_INTERACTIVE_UNLOCK_LOGON /
    // ICredentialProviderUser on Microsoft Learn, not a copied sample.
    hr = KerbInteractiveUnlockLogonInit(const_cast<PWSTR>(L""), const_cast<PWSTR>(_qualifiedUserName.c_str()),
                                         pwzProtectedPassword, _cpus, &kiul);
    if (SUCCEEDED(hr))
    {
        hr = KerbInteractiveUnlockLogonPack(kiul, &pcpcs->rgbSerialization, &pcpcs->cbSerialization);
        if (SUCCEEDED(hr))
        {
            ULONG ulAuthPackage;
            hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
            if (SUCCEEDED(hr))
            {
                pcpcs->ulAuthenticationPackage = ulAuthPackage;
                pcpcs->clsidCredentialProvider = CLSID_JackpotProvider;
                *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
            }
        }
    }

    // The protected/plaintext copy has done its job; never let it outlive
    // this call (spec §6.5).
    if (pwzProtectedPassword)
    {
        SecureZeroMemory(pwzProtectedPassword, wcslen(pwzProtectedPassword) * sizeof(WCHAR));
        CoTaskMemFree(pwzProtectedPassword);
    }

    return hr;
}

namespace {
struct ReportResultStatusInfo
{
    NTSTATUS ntsStatus;
    NTSTATUS ntsSubstatus;
    PCWSTR pwzMessage;
    CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

// Same customization spec §6.1's flow will eventually route the "THE
// HOUSE KEEPS THE WAFFLES" reset through; for Phase 4's plain password
// tile, plain, honest status text.
const ReportResultStatusInfo kLogonStatusInfo[] = {
    {STATUS_LOGON_FAILURE, STATUS_SUCCESS, L"Incorrect password or username.", CPSI_ERROR},
    {STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, L"This account is disabled.", CPSI_WARNING},
};
}  // namespace

IFACEMETHODIMP JackpotCredential::ReportResult(NTSTATUS ntsStatus, NTSTATUS ntsSubstatus,
                                                PWSTR* ppwszOptionalStatusText,
                                                CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    for (const auto& info : kLogonStatusInfo)
    {
        if (info.ntsStatus == ntsStatus && info.ntsSubstatus == ntsSubstatus)
        {
            if (SUCCEEDED(SHStrDupW(info.pwzMessage, ppwszOptionalStatusText)))
            {
                *pcpsiOptionalStatusIcon = info.cpsi;
            }
            break;
        }
    }

    // Whether or not we customized the message, always clear the typed
    // password before LogonUI redisplays this tile after a failure.
    ClearPasswordField();
    SHStrDupW(L"", &_rgFieldStrings[JFI_PASSWORD]);

    return S_OK;
}

IFACEMETHODIMP JackpotCredential::GetUserSid(PWSTR* ppwszSid)
{
    if (!ppwszSid)
    {
        return E_INVALIDARG;
    }
    return SHStrDupW(_pwzSid ? _pwzSid : L"", ppwszSid);
}

void JackpotCredential::ClearPasswordField()
{
    PWSTR& password = _rgFieldStrings[JFI_PASSWORD];
    if (password)
    {
        SecureZeroMemory(password, wcslen(password) * sizeof(WCHAR));
        CoTaskMemFree(password);
        password = nullptr;
    }
}

HBITMAP JackpotCredential::CreateTileBitmap()
{
    // 128x128 is the commonly-cited CPFT_TILE_IMAGE size in community
    // documentation of this API; I could not confirm the exact expected
    // size against Microsoft Learn or a real LogonUI, so this should be
    // checked on the VM this gets tested on (Phase 4's own criterion) and
    // adjusted if the tile looks off.
    constexpr int kSize = 128;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen)
    {
        return nullptr;
    }
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmp = hdcMem ? CreateCompatibleBitmap(hdcScreen, kSize, kSize) : nullptr;
    ReleaseDC(nullptr, hdcScreen);

    if (!hdcMem || !hbmp)
    {
        if (hbmp)
        {
            DeleteObject(hbmp);
        }
        if (hdcMem)
        {
            DeleteDC(hdcMem);
        }
        return nullptr;
    }

    HGDIOBJ hbmpOld = SelectObject(hdcMem, hbmp);

    RECT rc{0, 0, kSize, kSize};
    HBRUSH bgBrush = CreateSolidBrush(RGB(20, 15, 25));
    FillRect(hdcMem, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Same golden-waffle-with-a-grid motif WaffleRender's SymbolPainter
    // draws in Direct2D (spec §3) -- GDI's equivalent for this one
    // synchronous call.
    const int margin = kSize / 6;
    const int left = margin, top = margin, right = kSize - margin, bottom = kSize - margin;

    HBRUSH goldBrush = CreateSolidBrush(RGB(217, 166, 33));
    HPEN brownPen = CreatePen(PS_SOLID, 2, RGB(115, 71, 20));
    HGDIOBJ oldBrush = SelectObject(hdcMem, goldBrush);
    HGDIOBJ oldPen = SelectObject(hdcMem, brownPen);

    RoundRect(hdcMem, left, top, right, bottom, 12, 12);

    const int w = right - left;
    const int h = bottom - top;
    for (int i = 1; i < 4; ++i)
    {
        const int x = left + w * i / 4;
        MoveToEx(hdcMem, x, top + 2, nullptr);
        LineTo(hdcMem, x, bottom - 2);

        const int y = top + h * i / 4;
        MoveToEx(hdcMem, left + 2, y, nullptr);
        LineTo(hdcMem, right - 2, y);
    }

    SelectObject(hdcMem, oldBrush);
    SelectObject(hdcMem, oldPen);
    DeleteObject(goldBrush);
    DeleteObject(brownPen);

    SelectObject(hdcMem, hbmpOld);
    DeleteDC(hdcMem);

    return hbmp;
}
