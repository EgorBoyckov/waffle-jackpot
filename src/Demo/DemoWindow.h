#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <windows.h>

#include "Core/Config.h"
#include "Core/JackpotState.h"
#include "Core/SlotMachine.h"
#include "Render/AudioManager.h"
#include "Render/SlotRenderer.h"

namespace waffle::demo {

// Top-level Win32 window for WaffleJackpotDemo.exe (project spec §5). Owns
// the whole demo experience: the game objects (WaffleCore), the renderer
// and audio (WaffleRender), and the Win32 input/DPI/timer glue around
// them. The Credential Provider's modal spin window (Phase 5) will own a
// much smaller equivalent of the glue in this file, but reuse
// SlotRenderer / AudioManager / WaffleCore unchanged -- per spec §4.1,
// this class is the only part of the picture that's Demo-specific.
class DemoWindow {
public:
    DemoWindow();
    ~DemoWindow();

    DemoWindow(const DemoWindow&) = delete;
    DemoWindow& operator=(const DemoWindow&) = delete;

    bool Create(HINSTANCE instance);
    int RunMessageLoop();

private:
    static constexpr UINT_PTR kAnimationTimerId = 1;
    static constexpr UINT kAnimationTimerIntervalMs = 16;  // ~60 FPS while animating
    static constexpr std::uint32_t kLossCooldownMs = 700;
    static constexpr std::uint32_t kUnlockedHoldMs = 2000;

    static constexpr int kIdForceJackpot = 101;
    static constexpr int kIdForceNearMiss = 102;
    static constexpr int kIdSimulateLogon = 103;
    static constexpr int kIdSoundToggle = 104;

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hwnd);
    void OnSize(UINT widthPx, UINT heightPx);
    void OnDpiChanged(WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnTimer();
    void OnKeyDown(WPARAM key);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnCommand(int controlId);

    void PullLever();
    void BeginResult(const waffle::SpinResult& result);
    void AdvanceFrame(std::uint32_t dtMs);
    void UpdateAnimationTimer();
    void RebuildLayout(UINT widthPx, UINT heightPx);
    void ToggleSimulateLogon();
    void RefreshStatusLine();
    bool ReducedMotionRequested() const;

    HWND hwnd_ = nullptr;
    HWND forceJackpotButton_ = nullptr;
    HWND forceNearMissButton_ = nullptr;
    HWND simulateLogonButton_ = nullptr;
    HWND soundToggleButton_ = nullptr;

    waffle::Config config_;
    std::unique_ptr<waffle::SlotMachine> slotMachine_;
    waffle::JackpotStateMachine stateMachine_;
    std::unique_ptr<render::SlotRenderer> renderer_;
    std::unique_ptr<render::AudioManager> audio_;

    waffle::SpinResult pendingResult_;
    bool simulateLogon_ = false;
    bool debugOverlayVisible_ = false;
    bool pullButtonPressed_ = false;
    bool pullButtonCaptured_ = false;

    std::uint32_t lossCooldownRemainingMs_ = 0;
    std::uint32_t unlockedHoldRemainingMs_ = 0;
    bool timerRunning_ = false;

    std::wstring statusLine_;
    int lastLossMessageIndex_ = -1;
    int lastFooterIndex_ = -1;

    RECT pullHitRectPx_{};

    LARGE_INTEGER perfFrequency_{};
    LARGE_INTEGER lastTickTime_{};
    double fps_ = 0.0;
};

}  // namespace waffle::demo
