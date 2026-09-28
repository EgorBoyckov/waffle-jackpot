#include <windows.h>

#include "DemoWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prevInstance*/, LPWSTR /*cmdLine*/, int /*showCmd*/) {
    // XAudio2Create requires COM initialized on the calling thread;
    // Direct2D doesn't need it but isn't affected by it either.
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comHr);

    // Per-Monitor DPI v2 (spec §5.2). Also declared in
    // WaffleJackpotDemo.manifest, which is the primary mechanism on
    // Windows 10/11; this call is the documented fallback for builds
    // where the manifest didn't get embedded.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    waffle::demo::DemoWindow window;
    int exitCode = 1;
    if (window.Create(instance)) {
        exitCode = window.RunMessageLoop();
    }

    if (comInitialized) {
        CoUninitialize();
    }
    return exitCode;
}
