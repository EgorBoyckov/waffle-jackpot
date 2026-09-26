// Adapted from Microsoft's official Windows-classic-samples repository:
// Samples/Win7Samples/security/credentialproviders/helpers/helpers.h
// https://github.com/microsoft/Windows-classic-samples
//
// That repository is MIT-licensed (Copyright (c) Microsoft Corporation);
// see the full license text at
// https://github.com/microsoft/Windows-classic-samples/blob/main/LICENSE.
// These serialization/packing helpers are version-independent -- the same
// functions are the right building blocks for a V1 or a V2 credential
// provider's GetSerialization, which is why project spec §6.2 calls for
// reusing them rather than reinventing this COM/LSA plumbing. Content
// below is functionally unchanged from the original; only the include
// guard and this attribution header are new.
//
// Original file's header:
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Helper functions for copying parameters and packaging the buffer
// for GetSerialization.

#pragma once

#include <credentialprovider.h>
#include <ntsecapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <intsafe.h>

#include <windows.h>
#include <strsafe.h>

#pragma warning(push)
#pragma warning(disable : 4995)
#include <shlwapi.h>
#pragma warning(pop)

// makes a copy of a field descriptor using CoTaskMemAlloc
HRESULT FieldDescriptorCoAllocCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd
    );

// makes a copy of a field descriptor on the normal heap
HRESULT FieldDescriptorCopy(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& rcpfd,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* pcpfd
    );

// creates a UNICODE_STRING from a NULL-terminated string
HRESULT UnicodeStringInitWithString(
    PWSTR pwz,
    UNICODE_STRING* pus
    );

// initializes a KERB_INTERACTIVE_UNLOCK_LOGON with weak references to the provided credentials
HRESULT KerbInteractiveUnlockLogonInit(
    PWSTR pwzDomain,
    PWSTR pwzUsername,
    PWSTR pwzPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    KERB_INTERACTIVE_UNLOCK_LOGON* pkiul
    );

// packages the credentials into the buffer that the system expects
HRESULT KerbInteractiveUnlockLogonPack(
    const KERB_INTERACTIVE_UNLOCK_LOGON& rkiulIn,
    BYTE** prgb,
    DWORD* pcb
    );

// get the authentication package that will be used for our logon attempt
HRESULT RetrieveNegotiateAuthPackage(
    ULONG* pulAuthPackage
    );

// encrypt a password (if necessary) and copy it; if not, just copy it
HRESULT ProtectIfNecessaryAndCopyPassword(
    PCWSTR pwzPassword,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    PWSTR* ppwzProtectedPassword
    );

// Concatenates pwszDomain and pwszUsername into "DOMAIN\Username".
HRESULT DomainUsernameStringAlloc(
    PCWSTR pwszDomain,
    PCWSTR pwszUsername,
    PWSTR* ppwszDomainUsername
    );
