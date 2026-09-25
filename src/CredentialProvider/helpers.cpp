// Adapted from Microsoft's official Windows-classic-samples repository:
// Samples/Win7Samples/security/credentialproviders/helpers/helpers.cpp
// https://github.com/microsoft/Windows-classic-samples (MIT license;
// Copyright (c) Microsoft Corporation). See helpers.h for the full
// attribution note. Function bodies below are unchanged from the
// original except for dropping the two helpers Phase 4 doesn't use yet
// (KerbInteractiveUnlockLogonRepackNative, KerbInteractiveUnlockLogonUnpackInPlace
// -- both only relevant to SetSerialization/CredUI scenarios, which this
// provider returns E_NOTIMPL for per spec §6.3).
//
// Original file's header:
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "helpers.h"

#include <intsafe.h>
#include <wincred.h>

//
// Copies the field descriptor pointed to by rcpfd into a buffer allocated
// using CoTaskMemAlloc. Returns that buffer in ppcpfd.
//
HRESULT FieldDescriptorCoAllocCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd
    )
{
    HRESULT hr;
    DWORD cbStruct = sizeof(**ppcpfd);

    auto* pcpfd = static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(CoTaskMemAlloc(cbStruct));
    if (pcpfd)
    {
        pcpfd->dwFieldID = rcpfd.dwFieldID;
        pcpfd->cpft = rcpfd.cpft;

        if (rcpfd.pszLabel)
        {
            hr = SHStrDupW(rcpfd.pszLabel, &pcpfd->pszLabel);
        }
        else
        {
            pcpfd->pszLabel = nullptr;
            hr = S_OK;
        }
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        *ppcpfd = pcpfd;
    }
    else
    {
        CoTaskMemFree(pcpfd);
        *ppcpfd = nullptr;
    }

    return hr;
}

//
// Copies rcpfd into the buffer pointed to by pcpfd. The caller is responsible for
// allocating pcpfd. This function uses CoTaskMemAlloc to allocate memory for
// pcpfd->pszLabel.
//
HRESULT FieldDescriptorCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* pcpfd
    )
{
    HRESULT hr;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR cpfd;

    cpfd.dwFieldID = rcpfd.dwFieldID;
    cpfd.cpft = rcpfd.cpft;

    if (rcpfd.pszLabel)
    {
        hr = SHStrDupW(rcpfd.pszLabel, &cpfd.pszLabel);
    }
    else
    {
        cpfd.pszLabel = nullptr;
        hr = S_OK;
    }

    if (SUCCEEDED(hr))
    {
        *pcpfd = cpfd;
    }

    return hr;
}

//
// Copies the length of pwz and the pointer pwz into the UNICODE_STRING structure.
// This function is intended for serializing a credential in GetSerialization only.
// Note that this function just makes a copy of the string pointer. It DOES NOT
// ALLOCATE storage! Be very, very sure that's what you want -- it probably isn't
// outside of the exact GetSerialization call where this is used.
//
HRESULT UnicodeStringInitWithString(
    PWSTR pwz,
    UNICODE_STRING* pus
    )
{
    HRESULT hr;
    if (pwz)
    {
        size_t lenString = lstrlenW(pwz);
        USHORT usCharCount;
        hr = SizeTToUShort(lenString, &usCharCount);
        if (SUCCEEDED(hr))
        {
            USHORT usSize;
            hr = SizeTToUShort(sizeof(WCHAR), &usSize);
            if (SUCCEEDED(hr))
            {
                hr = UShortMult(usCharCount, usSize, &(pus->Length)); // Explicitly NOT including NULL terminator
                if (SUCCEEDED(hr))
                {
                    pus->MaximumLength = pus->Length;
                    pus->Buffer = pwz;
                    hr = S_OK;
                }
                else
                {
                    hr = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
                }
            }
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

//
// The following function is intended to be used ONLY with the Kerb*Pack functions.
// It does no bounds-checking because its callers have precise requirements and are
// written to respect its limitations.
//
static void UnicodeStringPackedUnicodeStringCopy(
    const UNICODE_STRING& rus,
    PWSTR pwzBuffer,
    UNICODE_STRING* pus
    )
{
    pus->Length = rus.Length;
    pus->MaximumLength = rus.Length;
    pus->Buffer = pwzBuffer;

    CopyMemory(pus->Buffer, rus.Buffer, pus->Length);
}

//
// Initialize the members of a KERB_INTERACTIVE_UNLOCK_LOGON with weak references to the
// passed-in strings. Useful if you will later use KerbInteractiveUnlockLogonPack to
// serialize the structure.
//
// The password is stored in encrypted form for CPUS_LOGON and CPUS_UNLOCK_WORKSTATION
// because the system can accept encrypted credentials. It is not encrypted in
// CPUS_CREDUI because we cannot know whether our caller can accept encrypted
// credentials -- moot for this provider, which returns E_NOTIMPL for CPUS_CREDUI
// (spec §6.3), but the branch is kept for fidelity to the original helper's contract.
//
HRESULT KerbInteractiveUnlockLogonInit(
    PWSTR pwzDomain,
    PWSTR pwzUsername,
    PWSTR pwzPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    KERB_INTERACTIVE_UNLOCK_LOGON* pkiul
    )
{
    KERB_INTERACTIVE_UNLOCK_LOGON kiul;
    ZeroMemory(&kiul, sizeof(kiul));

    KERB_INTERACTIVE_LOGON* pkil = &kiul.Logon;

    HRESULT hr = UnicodeStringInitWithString(pwzDomain, &pkil->LogonDomainName);
    if (SUCCEEDED(hr))
    {
        hr = UnicodeStringInitWithString(pwzUsername, &pkil->UserName);
        if (SUCCEEDED(hr))
        {
            hr = UnicodeStringInitWithString(pwzPassword, &pkil->Password);
            if (SUCCEEDED(hr))
            {
                switch (cpus)
                {
                case CPUS_UNLOCK_WORKSTATION:
                    pkil->MessageType = KerbWorkstationUnlockLogon;
                    hr = S_OK;
                    break;

                case CPUS_LOGON:
                    pkil->MessageType = KerbInteractiveLogon;
                    hr = S_OK;
                    break;

                case CPUS_CREDUI:
                    pkil->MessageType = static_cast<KERB_LOGON_SUBMIT_TYPE>(0); // MessageType does not apply to CredUI
                    hr = S_OK;
                    break;

                default:
                    hr = E_FAIL;
                    break;
                }

                if (SUCCEEDED(hr))
                {
                    // KERB_INTERACTIVE_UNLOCK_LOGON is just a series of structures. A
                    // flat copy will properly initialize the output parameter.
                    CopyMemory(pkiul, &kiul, sizeof(*pkiul));
                }
            }
        }
    }

    return hr;
}

//
// WinLogon and LSA consume "packed" KERB_INTERACTIVE_UNLOCK_LOGONs. In these, the
// PWSTR members of each UNICODE_STRING are not actually pointers but byte offsets
// into the overall buffer represented by the packed KERB_INTERACTIVE_UNLOCK_LOGON.
//
HRESULT KerbInteractiveUnlockLogonPack(
    const KERB_INTERACTIVE_UNLOCK_LOGON& rkiulIn,
    BYTE** prgb,
    DWORD* pcb
    )
{
    HRESULT hr;

    const KERB_INTERACTIVE_LOGON* pkilIn = &rkiulIn.Logon;

    DWORD cb = sizeof(rkiulIn) +
        pkilIn->LogonDomainName.Length +
        pkilIn->UserName.Length +
        pkilIn->Password.Length;

    auto* pkiulOut = static_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(CoTaskMemAlloc(cb));
    if (pkiulOut)
    {
        ZeroMemory(&pkiulOut->LogonId, sizeof(pkiulOut->LogonId));

        BYTE* pbBuffer = reinterpret_cast<BYTE*>(pkiulOut) + sizeof(*pkiulOut);

        KERB_INTERACTIVE_LOGON* pkilOut = &pkiulOut->Logon;
        pkilOut->MessageType = pkilIn->MessageType;

        UnicodeStringPackedUnicodeStringCopy(pkilIn->LogonDomainName, reinterpret_cast<PWSTR>(pbBuffer), &pkilOut->LogonDomainName);
        pkilOut->LogonDomainName.Buffer = reinterpret_cast<PWSTR>(pbBuffer - reinterpret_cast<BYTE*>(pkiulOut));
        pbBuffer += pkilOut->LogonDomainName.Length;

        UnicodeStringPackedUnicodeStringCopy(pkilIn->UserName, reinterpret_cast<PWSTR>(pbBuffer), &pkilOut->UserName);
        pkilOut->UserName.Buffer = reinterpret_cast<PWSTR>(pbBuffer - reinterpret_cast<BYTE*>(pkiulOut));
        pbBuffer += pkilOut->UserName.Length;

        UnicodeStringPackedUnicodeStringCopy(pkilIn->Password, reinterpret_cast<PWSTR>(pbBuffer), &pkilOut->Password);
        pkilOut->Password.Buffer = reinterpret_cast<PWSTR>(pbBuffer - reinterpret_cast<BYTE*>(pkiulOut));

        *prgb = reinterpret_cast<BYTE*>(pkiulOut);
        *pcb = cb;

        hr = S_OK;
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

//
// This function packs the string pszSourceString in pszDestinationString
// for use with LSA functions including LsaLookupAuthenticationPackage.
//
static HRESULT LsaInitString(
    PSTRING pszDestinationString,
    PCSTR pszSourceString
    )
{
    size_t cchLength = lstrlenA(pszSourceString);
    USHORT usLength;
    HRESULT hr = SizeTToUShort(cchLength, &usLength);
    if (SUCCEEDED(hr))
    {
        pszDestinationString->Buffer = const_cast<PCHAR>(pszSourceString);
        pszDestinationString->Length = usLength;
        pszDestinationString->MaximumLength = pszDestinationString->Length + 1;
        hr = S_OK;
    }
    return hr;
}

//
// Retrieves the 'negotiate' AuthPackage from the LSA. In this case, Kerberos.
//
HRESULT RetrieveNegotiateAuthPackage(ULONG* pulAuthPackage)
{
    HRESULT hr;
    HANDLE hLsa;

    NTSTATUS status = LsaConnectUntrusted(&hLsa);
    if (SUCCEEDED(HRESULT_FROM_NT(status)))
    {
        ULONG ulAuthPackage;
        LSA_STRING lsaszKerberosName;
        LsaInitString(&lsaszKerberosName, NEGOSSP_NAME_A);

        status = LsaLookupAuthenticationPackage(hLsa, &lsaszKerberosName, &ulAuthPackage);
        if (SUCCEEDED(HRESULT_FROM_NT(status)))
        {
            *pulAuthPackage = ulAuthPackage;
            hr = S_OK;
        }
        else
        {
            hr = HRESULT_FROM_NT(status);
        }
        LsaDeregisterLogonProcess(hLsa);
    }
    else
    {
        hr = HRESULT_FROM_NT(status);
    }

    return hr;
}

//
// Return a copy of pwzToProtect encrypted with the CredProtect API.
// pwzToProtect must not be NULL or the empty string.
//
static HRESULT ProtectAndCopyString(
    PCWSTR pwzToProtect,
    PWSTR* ppwzProtected
    )
{
    *ppwzProtected = nullptr;

    PWSTR pwzToProtectCopy;
    HRESULT hr = SHStrDupW(pwzToProtect, &pwzToProtectCopy);
    if (SUCCEEDED(hr))
    {
        // The first call to CredProtect determines the length of the encrypted
        // string. Because we pass a NULL output buffer, we expect the call to fail.
        DWORD cchProtected = 0;
        if (!CredProtectW(FALSE, pwzToProtectCopy, static_cast<DWORD>(wcslen(pwzToProtectCopy)) + 1, nullptr, &cchProtected, nullptr))
        {
            DWORD dwErr = GetLastError();

            if ((ERROR_INSUFFICIENT_BUFFER == dwErr) && (0 < cchProtected))
            {
                auto* pwzProtected = static_cast<PWSTR>(CoTaskMemAlloc(cchProtected * sizeof(WCHAR)));
                if (pwzProtected)
                {
                    if (CredProtectW(FALSE, pwzToProtectCopy, static_cast<DWORD>(wcslen(pwzToProtectCopy)) + 1, pwzProtected, &cchProtected, nullptr))
                    {
                        *ppwzProtected = pwzProtected;
                        hr = S_OK;
                    }
                    else
                    {
                        CoTaskMemFree(pwzProtected);
                        dwErr = GetLastError();
                        hr = HRESULT_FROM_WIN32(dwErr);
                    }
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
            }
            else
            {
                hr = HRESULT_FROM_WIN32(dwErr);
            }
        }

        SecureZeroMemory(pwzToProtectCopy, wcslen(pwzToProtectCopy) * sizeof(WCHAR));
        CoTaskMemFree(pwzToProtectCopy);
    }

    return hr;
}

//
// If pwzPassword should be encrypted, return a copy encrypted with CredProtect.
// If not, just return a copy.
//
HRESULT ProtectIfNecessaryAndCopyPassword(
    PCWSTR pwzPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    PWSTR* ppwzProtectedPassword
    )
{
    *ppwzProtectedPassword = nullptr;

    HRESULT hr;

    if (pwzPassword && *pwzPassword)
    {
        PWSTR pwzPasswordCopy;
        hr = SHStrDupW(pwzPassword, &pwzPasswordCopy);
        if (SUCCEEDED(hr))
        {
            bool bCredAlreadyEncrypted = false;
            CRED_PROTECTION_TYPE protectionType;

            // If the password is already encrypted, we should not encrypt it again.
            // An encrypted password may be received through SetSerialization in the
            // CPUS_LOGON scenario during a Remote Desktop connection, for instance.
            if (CredIsProtectedW(pwzPasswordCopy, &protectionType))
            {
                if (CredUnprotected != protectionType)
                {
                    bCredAlreadyEncrypted = true;
                }
            }

            if (CPUS_CREDUI == cpus || bCredAlreadyEncrypted)
            {
                hr = SHStrDupW(pwzPasswordCopy, ppwzProtectedPassword);
            }
            else
            {
                hr = ProtectAndCopyString(pwzPasswordCopy, ppwzProtectedPassword);
            }

            SecureZeroMemory(pwzPasswordCopy, wcslen(pwzPasswordCopy) * sizeof(WCHAR));
            CoTaskMemFree(pwzPasswordCopy);
        }
    }
    else
    {
        hr = SHStrDupW(L"", ppwzProtectedPassword);
    }

    return hr;
}

// Concatenates pwszDomain and pwszUsername and places the result in *ppwszDomainUsername.
HRESULT DomainUsernameStringAlloc(
    PCWSTR pwszDomain,
    PCWSTR pwszUsername,
    PWSTR* ppwszDomainUsername
    )
{
    HRESULT hr;
    size_t cchDomain = lstrlenW(pwszDomain);
    size_t cchUsername = lstrlenW(pwszUsername);
    size_t cbLen = sizeof(WCHAR) * (cchDomain + 1 + cchUsername + 1);
    auto* pwszDest = static_cast<PWSTR>(HeapAlloc(GetProcessHeap(), 0, cbLen));
    if (pwszDest)
    {
        hr = StringCbPrintfW(pwszDest, cbLen, L"%s\\%s", pwszDomain, pwszUsername);
        if (SUCCEEDED(hr))
        {
            *ppwszDomainUsername = pwszDest;
        }
        else
        {
            HeapFree(GetProcessHeap(), 0, pwszDest);
        }
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}
