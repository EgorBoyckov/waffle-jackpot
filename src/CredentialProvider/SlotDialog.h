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

// The modal slot machine window opened when the user clicks PULL! on the
// tile (spec §6.1 step 2). Owns its own SlotMachine / JackpotStateMachine
// / SlotRenderer / AudioManager -- the same Core and Render objects Demo
// uses (spec §4.1's "one logic, one render") -- and a small Win32 message
// loop of its own, run modally against the owner HWND that
// ICredentialProviderCredentialEvents::OnCreatingWindow hands back.
//
// Deliberately not shared code with waffle::demo::DemoWindow: this window
// has no debug overlay, no Force Jackpot/Force Near-Miss/Simulate Logon
// controls (Demo-only per spec §5.1), and a different exit contract (it
// reports win/lose back to its caller instead of looping). The two share
// the same lower-level building blocks (SlotMachine, SlotRenderer,
// AudioManager) as spec §4.1 requires; the Win32 glue around them is
// intentionally not factored into a shared controller in this phase --
// noted in the Phase 5 commit message as a reasonable follow-up, not a
// spec requirement.
class SlotDialog
{
public:
    explicit SlotDialog(const waffle::Config& config);
    ~SlotDialog();

    SlotDialog(const SlotDialog&) = delete;
    SlotDialog& operator=(const SlotDialog&) = delete;

    // Creates the window as a modal popup owned by hwndOwner and pumps a
    // nested message loop until the user either wins (the jackpot
    // sequence finishes) or cancels (Escape / closes the window). This is
    // a standard modal-dialog technique -- it pumps messages the entire
    // time -- not the kind of blocking wait spec §9.3 rules out (that's
    // about un-pumped waits: blocking I/O, Sleep, etc). Returns true iff
    // the session ended on a jackpot.
    bool RunModal(HWND hwndOwner);

private:
    static constexpr UINT_PTR kAnimationTimerId = 1;
    static constexpr UINT kAnimationTimerIntervalMs = 16;
    static constexpr std::uint32_t kLossCooldownMs = 700;
    static constexpr int kWindowWidthPx = 420;
    static constexpr int kWindowHeightPx = 560;

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hwnd);
    void OnPaint();
    void OnTimer();
    void OnKeyDown(WPARAM key);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void RequestClose();

    void PullLever();
    void AdvanceFrame(std::uint32_t dtMs);
    void UpdateAnimationTimer();
    void RebuildLayout(UINT widthPx, UINT heightPx);
    void RefreshStatusLine();
    bool ReducedMotionRequested() const;

    waffle::Config _config;
    HWND _hwnd;

    std::unique_ptr<waffle::SlotMachine> _slotMachine;
    waffle::JackpotStateMachine _stateMachine;
    std::unique_ptr<render::SlotRenderer> _renderer;
    std::unique_ptr<render::AudioManager> _audio;

    waffle::SpinResult _pendingResult;
    bool _wonJackpot;
    bool _closeRequested;

    bool _pullButtonPressed;
    bool _pullButtonCaptured;
    RECT _pullHitRectPx{};

    std::uint32_t _lossCooldownRemainingMs;
    bool _timerRunning;

    std::wstring _statusLine;
    int _lastLossMessageIndex;

    LARGE_INTEGER _perfFrequency{};
    LARGE_INTEGER _lastTickTime{};
};
