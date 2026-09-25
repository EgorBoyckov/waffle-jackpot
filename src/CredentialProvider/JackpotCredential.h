#pragma once

#include <string>

#include "FieldDescriptors.h"

// One tile, for one Windows user, in the LogonUI credential-provider list.
//
// Phase 4 scope: a plain password credential -- no slot machine yet, the
// password field is always visible and enabled. Phase 5 adds the PULL
// command link, the modal spin window, and hides password/submit behind
// a jackpot (spec §6.1). ICredentialProviderCredential2 (not just
// ICredentialProviderCredential) is required for GetUserSid, which V2
// providers use to tell LogonUI which existing Windows user this tile
// belongs to (spec §6.2).
//
// Every interface method here is a COM boundary: no C++ exception may
// cross it, every path returns an HRESULT (spec §9.1).
class JackpotCredential : public ICredentialProviderCredential2
{
public:
    JackpotCredential();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // Not a COM method: called once by JackpotProvider right after
    // construction, before this credential is handed to LogonUI.
    // pwzQualifiedUserName is the "DOMAIN\User"-shaped string from
    // ICredentialProviderUser's PKEY_Identity_QualifiedUserName (spec
    // §6.2); pwzSid is that user's SID string, returned later via
    // GetUserSid.
    HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, PCWSTR pwzQualifiedUserName, PCWSTR pwzSid);

    // ICredentialProviderCredential
    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* pcpce) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP SetSelected(BOOL* pbAutoLogon) override;
    IFACEMETHODIMP SetDeselected() override;
    IFACEMETHODIMP GetFieldState(DWORD dwFieldID, CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                                  CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) override;
    IFACEMETHODIMP GetStringValue(DWORD dwFieldID, PWSTR* ppwsz) override;
    IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp) override;
    IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, BOOL* pbChecked, PWSTR* ppwszLabel) override;
    IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo) override;
    IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem) override;
    IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, PWSTR* ppwszItem) override;
    IFACEMETHODIMP SetStringValue(DWORD dwFieldID, PCWSTR pwz) override;
    IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked) override;
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem) override;
    IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID) override;
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
                                     CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
                                     PWSTR* ppwszOptionalStatusText,
                                     CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;
    IFACEMETHODIMP ReportResult(NTSTATUS ntsStatus, NTSTATUS ntsSubstatus, PWSTR* ppwszOptionalStatusText,
                                 CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) override;

    // ICredentialProviderCredential2
    IFACEMETHODIMP GetUserSid(PWSTR* ppwszSid) override;

private:
    ~JackpotCredential();

    // Overwrites and frees the current password field value. Called
    // before replacing it and from the destructor -- spec §6.5: a
    // password never outlives its use uncleared.
    void ClearPasswordField();

    // Synchronous GDI rendering of the tile image: a golden rounded
    // square with a waffle grid, the same motif WaffleRender's
    // SymbolPainter draws procedurally in Direct2D (spec §3), redone here
    // in GDI because GetBitmapValue must return a ready HBITMAP and can't
    // wait on a Direct2D render target. No file, no embedded resource.
    static HBITMAP CreateTileBitmap();

    LONG _cRef;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO _cpus;
    ICredentialProviderCredentialEvents* _pCredProvCredentialEvents;

    std::wstring _qualifiedUserName;
    PWSTR _pwzSid;

    PWSTR _rgFieldStrings[JFI_NUM_FIELDS];
};
