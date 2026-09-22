#include "DemoWindow.h"

#include <algorithm>
#include <iterator>
#include <windowsx.h>

#include "Core/Rng.h"

namespace waffle::demo {
namespace {

const wchar_t* const kLossMessages[] = {
    L"NOPE! The Windows gods are not impressed.",
    L"ACCESS DENIED. Reason: NOT ENOUGH WAFFLES",
    L"7 7 7? Cute. We only accept waffles.",
    L"Your waffle balance is insufficient.",
};
const wchar_t* const kNearMissMessage = L"SO CLOSE. The third waffle is shy.";

const wchar_t* const kFooterMessages[] = {
    L"100% Official Waffle Authentication™",
    L"Certified by Nobody",
    L"Totally Serious Security System™",
    L"Please insert 3 waffles",
    L"Waffles are not a security boundary",
    L"No syrup was harmed",
};

const wchar_t kWindowClassName[] = L"WaffleJackpotDemoWindow";

// Resolves config/config.default.json next to the running .exe, rather
// than relative to the current working directory -- a Demo launched via
// Explorer or a shortcut can't rely on CWD being its own folder.
std::wstring ResolveConfigPath() {
    wchar_t exePath[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath, len);
    const auto pos = path.find_last_of(L"\\/");
    const std::wstring dir = (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
    return dir + L"\\config\\config.default.json";
}

}  // namespace

DemoWindow::DemoWindow() = default;
DemoWindow::~DemoWindow() {
    if (timerRunning_ && hwnd_) {
        KillTimer(hwnd_, kAnimationTimerId);
    }
}

bool DemoWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &DemoWindow::WndProcThunk;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // Direct2D owns the whole client area
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, kWindowClassName, L"Waffle Jackpot Login — Demo",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 700, nullptr, nullptr,
                             instance, this);
    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    return true;
}

int DemoWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK DemoWindow::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DemoWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<DemoWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DemoWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT DemoWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate(hwnd);
            return 0;
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_DPICHANGED:
            OnDpiChanged(wParam, lParam);
            return 0;
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_TIMER:
            if (wParam == kAnimationTimerId) {
                OnTimer();
            }
            return 0;
        case WM_KEYDOWN:
            OnKeyDown(wParam);
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            return 0;
        case WM_ERASEBKGND:
            // Direct2D repaints the whole client area every frame; avoid
            // the GDI flicker-fill.
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void DemoWindow::OnCreate(HWND hwnd) {
    hwnd_ = hwnd;

    std::vector<std::string> warnings;
    config_ = waffle::ConfigLoader::LoadFromFile(ResolveConfigPath(), &warnings);
    config_.demoMode = true;

    slotMachine_ =
        std::make_unique<waffle::SlotMachine>(config_, std::make_unique<waffle::SystemRandomSource>());
    stateMachine_.SetResetOnFailedLogon(config_.resetJackpotOnFailedLogon);

    renderer_ = std::make_unique<render::SlotRenderer>(hwnd_);
    renderer_->SetDpi(static_cast<float>(GetDpiForWindow(hwnd_)));

    audio_ = std::make_unique<render::AudioManager>();
    audio_->SetVolume(static_cast<float>(config_.volume));
    audio_->SetMuted(!config_.soundEnabled);

    QueryPerformanceFrequency(&perfFrequency_);
    QueryPerformanceCounter(&lastTickTime_);

    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
    forceJackpotButton_ =
        CreateWindowW(L"BUTTON", L"Force Jackpot", WS_CHILD | BS_PUSHBUTTON, 8, 8, 150, 26, hwnd_,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdForceJackpot)), instance, nullptr);
    forceNearMissButton_ =
        CreateWindowW(L"BUTTON", L"Force Near-Miss", WS_CHILD | BS_PUSHBUTTON, 8, 40, 150, 26, hwnd_,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdForceNearMiss)), instance, nullptr);
    simulateLogonButton_ =
        CreateWindowW(L"BUTTON", L"Simulate Logon", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 150, 26,
                      hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSimulateLogon)), instance, nullptr);
    soundToggleButton_ = CreateWindowW(L"BUTTON", config_.soundEnabled ? L"Sound: On" : L"Sound: Off",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 150, 26, hwnd_,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSoundToggle)), instance,
                                        nullptr);

    // Force Jackpot / Force Near-Miss are debug-only tools -- spec §5.1:
    // "только в Demo и только при debugMode".
    if (config_.debugMode) {
        ShowWindow(forceJackpotButton_, SW_SHOW);
        ShowWindow(forceNearMissButton_, SW_SHOW);
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    RebuildLayout(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));
    RefreshStatusLine();
}

void DemoWindow::OnSize(UINT widthPx, UINT heightPx) {
    RebuildLayout(widthPx, heightPx);
}

void DemoWindow::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
    const UINT dpi = LOWORD(wParam);
    if (renderer_) {
        renderer_->SetDpi(static_cast<float>(dpi));
    }

    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    if (suggested) {
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void DemoWindow::RebuildLayout(UINT widthPx, UINT heightPx) {
    const float w = static_cast<float>(widthPx);
    const float h = static_cast<float>(heightPx);

    // Matches SlotRenderer::DrawPullButton's proportions exactly, so the
    // clickable area lines up with what's drawn. Fractions are
    // resolution-independent, so no DIP/pixel conversion is needed here.
    const float buttonWidth = w * 0.22f;
    const float buttonHeight = h * 0.10f;
    const float centerX = w * 0.5f;
    const float centerY = h * 0.78f;
    pullHitRectPx_ = {static_cast<LONG>(centerX - buttonWidth * 0.5f),
                       static_cast<LONG>(centerY - buttonHeight * 0.5f),
                       static_cast<LONG>(centerX + buttonWidth * 0.5f),
                       static_cast<LONG>(centerY + buttonHeight * 0.5f)};

    if (simulateLogonButton_) {
        SetWindowPos(simulateLogonButton_, nullptr, static_cast<int>(w) - 158, 8, 150, 26, SWP_NOZORDER);
    }
    if (soundToggleButton_) {
        SetWindowPos(soundToggleButton_, nullptr, static_cast<int>(w) - 158, 40, 150, 26, SWP_NOZORDER);
    }

    if (renderer_) {
        renderer_->Resize(widthPx, heightPx);
    }
}

void DemoWindow::OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);

    render::DebugOverlayInfo overlay;
    const render::DebugOverlayInfo* overlayPtr = nullptr;
    if (debugOverlayVisible_ && slotMachine_) {
        overlay.state = stateMachine_.Current();
        overlay.reels = pendingResult_.symbols;
        overlay.attemptsSinceLastJackpot = slotMachine_->AttemptsSinceLastJackpot();
        overlay.naturalJackpotProbability = slotMachine_->NaturalJackpotProbability();
        overlay.fps = fps_;
        overlayPtr = &overlay;
    }

    if (renderer_) {
        renderer_->Render(overlayPtr, statusLine_, pullButtonPressed_);
    }

    EndPaint(hwnd_, &ps);
}

void DemoWindow::OnTimer() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double elapsedSec =
        static_cast<double>(now.QuadPart - lastTickTime_.QuadPart) / static_cast<double>(perfFrequency_.QuadPart);
    lastTickTime_ = now;

    // Clamp so a stalled/minimized window doesn't feed a huge dt into the
    // animators once it's back.
    std::uint32_t dtMs = static_cast<std::uint32_t>(std::clamp(elapsedSec, 0.0, 0.1) * 1000.0);
    dtMs = std::max<std::uint32_t>(dtMs, 1);

    fps_ = fps_ * 0.9 + (1000.0 / dtMs) * 0.1;

    AdvanceFrame(dtMs);
}

void DemoWindow::OnKeyDown(WPARAM key) {
    switch (key) {
        case VK_RETURN:
        case VK_SPACE:
            if (renderer_ && (renderer_->IsSpinAnimating() || renderer_->IsJackpotSequenceActive())) {
                renderer_->SkipCurrentAnimation();
            } else if (stateMachine_.Current() == waffle::JackpotState::Idle) {
                PullLever();
            }
            return;
        case VK_F12:
            debugOverlayVisible_ = !debugOverlayVisible_;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        default:
            return;
    }
}

void DemoWindow::OnLButtonDown(int x, int y) {
    const POINT pt{x, y};
    if (PtInRect(&pullHitRectPx_, pt)) {
        pullButtonPressed_ = true;
        pullButtonCaptured_ = true;
        SetCapture(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void DemoWindow::OnLButtonUp(int x, int y) {
    if (!pullButtonCaptured_) {
        return;
    }
    ReleaseCapture();
    pullButtonCaptured_ = false;
    pullButtonPressed_ = false;

    const POINT pt{x, y};
    if (PtInRect(&pullHitRectPx_, pt)) {
        if (renderer_ && (renderer_->IsSpinAnimating() || renderer_->IsJackpotSequenceActive())) {
            renderer_->SkipCurrentAnimation();
        } else if (stateMachine_.Current() == waffle::JackpotState::Idle) {
            PullLever();
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DemoWindow::OnCommand(int controlId) {
    switch (controlId) {
        case kIdForceJackpot: {
            if (!config_.debugMode || !stateMachine_.Pull()) {
                return;
            }
            waffle::SpinResult forced;
            forced.symbols = {waffle::Symbol::Waffle, waffle::Symbol::Waffle, waffle::Symbol::Waffle};
            forced.jackpot = true;
            forced.nearMiss = false;
            forced.attemptNumber = slotMachine_->AttemptsSinceLastJackpot();
            pendingResult_ = forced;
            BeginResult(forced);
            return;
        }
        case kIdForceNearMiss: {
            if (!config_.debugMode || !stateMachine_.Pull()) {
                return;
            }
            waffle::SpinResult forced;
            forced.symbols = {waffle::Symbol::Waffle, waffle::Symbol::Waffle, waffle::Symbol::Cherry};
            forced.jackpot = false;
            forced.nearMiss = true;
            forced.attemptNumber = slotMachine_->AttemptsSinceLastJackpot();
            pendingResult_ = forced;
            BeginResult(forced);
            return;
        }
        case kIdSimulateLogon:
            ToggleSimulateLogon();
            return;
        case kIdSoundToggle:
            config_.soundEnabled = !config_.soundEnabled;
            if (audio_) {
                audio_->SetMuted(!config_.soundEnabled);
            }
            SetWindowTextW(soundToggleButton_, config_.soundEnabled ? L"Sound: On" : L"Sound: Off");
            return;
        default:
            return;
    }
}

void DemoWindow::PullLever() {
    if (!stateMachine_.Pull()) {
        return;
    }
    pendingResult_ = slotMachine_->Spin();
    BeginResult(pendingResult_);
}

void DemoWindow::BeginResult(const waffle::SpinResult& result) {
    if (renderer_) {
        renderer_->BeginSpin(result, static_cast<std::uint32_t>(config_.animationDurationMs),
                              ReducedMotionRequested());
    }
    if (audio_) {
        audio_->Play(render::Clip::Lever);
    }
    RefreshStatusLine();
    UpdateAnimationTimer();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DemoWindow::AdvanceFrame(std::uint32_t dtMs) {
    if (!renderer_) {
        return;
    }

    const render::FrameEvents events = renderer_->Advance(dtMs);

    if (audio_) {
        for (bool landed : events.reelJustLanded) {
            if (landed) {
                audio_->Play(render::Clip::ReelTick);
            }
        }
    }

    if (stateMachine_.Current() == waffle::JackpotState::Spinning && !renderer_->IsSpinAnimating()) {
        stateMachine_.ResolveSpin(pendingResult_.jackpot);
        if (pendingResult_.jackpot) {
            renderer_->BeginJackpotSequence(static_cast<std::uint32_t>(config_.jackpotSequenceMs),
                                             ReducedMotionRequested());
        } else {
            if (audio_) {
                audio_->Play(pendingResult_.nearMiss ? render::Clip::NearMiss : render::Clip::Loss);
            }
            lossCooldownRemainingMs_ = kLossCooldownMs;
        }
        RefreshStatusLine();
    }

    if (events.jackpotFanfareStarted && audio_) {
        audio_->Play(render::Clip::Jackpot);
    }

    if (stateMachine_.Current() == waffle::JackpotState::JackpotSequence && renderer_->IsJackpotSequenceDone()) {
        stateMachine_.FinishJackpotSequence();
        unlockedHoldRemainingMs_ = kUnlockedHoldMs;
        RefreshStatusLine();
    }

    if (stateMachine_.Current() == waffle::JackpotState::Loss && lossCooldownRemainingMs_ > 0) {
        lossCooldownRemainingMs_ = (lossCooldownRemainingMs_ > dtMs) ? lossCooldownRemainingMs_ - dtMs : 0;
        if (lossCooldownRemainingMs_ == 0) {
            stateMachine_.FinishCooldown();
            RefreshStatusLine();
        }
    }

    if (stateMachine_.Current() == waffle::JackpotState::Unlocked && unlockedHoldRemainingMs_ > 0) {
        unlockedHoldRemainingMs_ = (unlockedHoldRemainingMs_ > dtMs) ? unlockedHoldRemainingMs_ - dtMs : 0;
        if (unlockedHoldRemainingMs_ == 0) {
            // Demo doesn't perform real Windows authentication -- that's
            // the Credential Provider's job (Phase 4/5). Loop back to
            // Idle so the machine can be played again.
            stateMachine_.Reset();
            RefreshStatusLine();
        }
    }

    UpdateAnimationTimer();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DemoWindow::UpdateAnimationTimer() {
    const bool needsTimer = (renderer_ && (renderer_->IsSpinAnimating() || renderer_->IsJackpotSequenceActive())) ||
                             lossCooldownRemainingMs_ > 0 || unlockedHoldRemainingMs_ > 0;

    if (needsTimer && !timerRunning_) {
        SetTimer(hwnd_, kAnimationTimerId, kAnimationTimerIntervalMs, nullptr);
        QueryPerformanceCounter(&lastTickTime_);
        timerRunning_ = true;
    } else if (!needsTimer && timerRunning_) {
        // spec §5.2: "0% CPU в покое" -- no animation running means no
        // timer ticking.
        KillTimer(hwnd_, kAnimationTimerId);
        timerRunning_ = false;
    }
}

void DemoWindow::ToggleSimulateLogon() {
    simulateLogon_ = !simulateLogon_;
    SetWindowTextW(simulateLogonButton_, simulateLogon_ ? L"Exit Simulate Logon" : L"Simulate Logon");

    // Approximates the Credential Provider's modal spin-window footprint
    // (spec §5.1: "открывает автомат в размере и стиле, как он будет
    // выглядеть в CP"). The real dimensions are pinned down in Phase 5
    // against an actual LogonUI tile.
    const int width = simulateLogon_ ? 420 : 900;
    const int height = simulateLogon_ ? 620 : 700;

    RECT rect{0, 0, width, height};
    AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE,
                        static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE)));
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

bool DemoWindow::ReducedMotionRequested() const {
    if (!config_.respectReducedMotion) {
        return false;
    }
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        return !enabled;
    }
    return false;
}

void DemoWindow::RefreshStatusLine() {
    std::wstring main;
    switch (stateMachine_.Current()) {
        case waffle::JackpotState::Idle:
            main = L"INSERT WINDOWS USER · GOOD LUCK";
            break;
        case waffle::JackpotState::Spinning:
            main = L"SPINNING...";
            break;
        case waffle::JackpotState::Loss: {
            if (pendingResult_.nearMiss) {
                main = kNearMissMessage;
            } else {
                int index = static_cast<int>(GetTickCount64() % std::size(kLossMessages));
                if (index == lastLossMessageIndex_ && std::size(kLossMessages) > 1) {
                    index = static_cast<int>((index + 1) % std::size(kLossMessages));
                }
                lastLossMessageIndex_ = index;
                main = kLossMessages[index];
            }
            break;
        }
        case waffle::JackpotState::JackpotSequence:
            main = L"THREE WAFFLES! JACKPOT!";
            break;
        case waffle::JackpotState::Unlocked:
            main = L"YOU MAY ENTER WINDOWS · Continue to Windows authentication...";
            break;
    }

    const std::size_t footerIndex = (GetTickCount64() / 4000) % std::size(kFooterMessages);
    statusLine_ = main + L"\n" + kFooterMessages[footerIndex];
}

}  // namespace waffle::demo
