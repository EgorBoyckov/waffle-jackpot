// Adapted from Microsoft's official Windows-classic-samples repository:
// Samples/Win7Samples/security/credentialproviders/helpers/Dll.h
// https://github.com/microsoft/Windows-classic-samples (MIT license;
// Copyright (c) Microsoft Corporation; see
// https://github.com/microsoft/Windows-classic-samples/blob/main/LICENSE).
#pragma once

#include <windows.h>

// Global DLL instance handle, set in DllMain. Used e.g. for
// LoadBitmap(HINST_THISDLL, ...) if we ever load a resource-based bitmap.
extern HINSTANCE g_hinst;
#define HINST_THISDLL g_hinst

// Lock-count helpers IClassFactory::LockServer and DllCanUnloadNow share.
void DllAddRef();
void DllRelease();
