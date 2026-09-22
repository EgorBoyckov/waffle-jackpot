#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include "Animation.h"
#include "Assets.h"
#include "Confetti.h"
#include "Core/SlotMachine.h"
#include "Core/JackpotState.h"

namespace waffle::render {

// Snapshot for the debug overlay (F12), spec §5.1: state, reels, attempts,
// probability, FPS.
struct DebugOverlayInfo {
    waffle::JackpotState state = waffle::JackpotState::Idle;
    std::array<waffle::Symbol, waffle::kReelCount> reels{};
    int attemptsSinceLastJackpot = 0;
    double naturalJackpotProbability = 0.0;
    double fps = 0.0;
};

// Notable things that happened during one Advance() call, so the caller
// can react (mainly: play the matching sound). SlotRenderer never touches
// audio itself -- AudioManager is a sibling the caller (DemoWindow today,
// the Credential Provider's modal window in Phase 5) owns and drives.
struct FrameEvents {
    std::array<bool, waffle::kReelCount> reelJustLanded{};
    bool jackpotFanfareStarted = false;
    bool jackpotSequenceJustFinished = false;
};

// Renders the whole slot-machine scene into an HWND-bound Direct2D render
// target and owns the device resources tied to that HWND, including
// device-lost recovery. Holds no SlotMachine/JackpotStateMachine of its
// own and no input handling -- those live in the caller, so this class is
// identically reusable from Demo and from the Credential Provider's modal
// window (spec §4.1's "один и тот же рендер").
class SlotRenderer {
public:
    explicit SlotRenderer(HWND hwnd);
    ~SlotRenderer();

    SlotRenderer(const SlotRenderer&) = delete;
    SlotRenderer& operator=(const SlotRenderer&) = delete;

    bool IsValid() const { return renderTarget_ != nullptr; }

    // Call on WM_SIZE with the new client area in pixels.
    void Resize(UINT widthPx, UINT heightPx);

    // Call on WM_DPICHANGED (and once at startup) with GetDpiForWindow's
    // result, so Direct2D's DIP-to-pixel mapping tracks the monitor the
    // window is actually on (spec §5.2: Per-Monitor DPI v2, 100-250%).
    void SetDpi(float dpi);

    // Starts the visual spin for a SpinResult SlotMachine has already
    // decided (spec §2.1: the animation never influences the outcome).
    // reducedMotion collapses the spin to a short transition per spec
    // §5.2 / SPI_GETCLIENTAREAANIMATION.
    void BeginSpin(const waffle::SpinResult& result, std::uint32_t animationDurationMs, bool reducedMotion);
    bool IsSpinAnimating() const;

    void BeginJackpotSequence(std::uint32_t jackpotSequenceMs, bool reducedMotion);
    bool IsJackpotSequenceActive() const;
    bool IsJackpotSequenceDone() const;

    // Lets an impatient user click/Enter through the current beat (spec
    // §5.3: "есть «skip» по клику/Enter").
    void SkipCurrentAnimation();

    // Advances all animation/particle state by dtMs and reports what
    // happened this tick. Call once per frame only while something is
    // actually animating (spec §5.2: "0% CPU в покое" -- no animation, no
    // ticking).
    FrameEvents Advance(std::uint32_t dtMs);

    // Renders one frame. `overlay` is null to hide the debug overlay.
    void Render(const DebugOverlayInfo* overlay, const std::wstring& statusLine, bool pullButtonPressed);

private:
    void CreateDeviceIndependentResources();
    void CreateDeviceResources();
    void DiscardDeviceResources();

    ID2D1SolidColorBrush* SceneBrush(const D2D1_COLOR_F& color);

    void DrawCabinet(const D2D1_SIZE_F& size);
    void DrawLamps(const D2D1_SIZE_F& size);
    void DrawReels(const D2D1_SIZE_F& size);
    void DrawPullButton(const D2D1_SIZE_F& size, bool pressed);
    void DrawTexts(const D2D1_SIZE_F& size, const std::wstring& statusLine);
    void DrawDebugOverlay(const DebugOverlayInfo& info);

    HWND hwnd_;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> sceneBrush_;
    Microsoft::WRL::ComPtr<ID2D1Layer> blurLayer_;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> largeFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> jackpotFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> debugFormat_;

    std::unique_ptr<SymbolPainter> symbolPainter_;
    std::unique_ptr<ConfettiSystem> confetti_;

    std::array<ReelSpinAnimator, waffle::kReelCount> reelAnimators_;
    std::unique_ptr<JackpotSequencer> jackpotSequencer_;
    bool jackpotReducedMotion_ = false;

    std::uint32_t lampElapsedMs_ = 0;
    float dpi_ = 96.0f;
};

}  // namespace waffle::render
