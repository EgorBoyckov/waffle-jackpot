#include "SlotDialog.h"

#include <algorithm>
#include <iterator>
#include <windowsx.h>

#include "Core/Rng.h"

namespace {

const wchar_t* const kLossMessages[] = {
    L"NOPE! The Windows gods are not impressed.",
    L"ACCESS DENIED. Reason: NOT ENOUGH WAFFLES",
    L"7 7 7? Cute. We only accept waffles.",
    L"Your waffle balance is insufficient.",
};
const wchar_t* const kNearMissMessage = L"SO CLOSE. The third waffle is shy.";

const wchar_t kWindowClassName[] = L"WaffleJackpotSlotDialog";

}  // namespace

SlotDialog::SlotDialog(const waffle::Config& config)
    : _config(config),
      _hwnd(nullptr),
      _wonJackpot(false),
      _closeRequested(false),
      _pullButtonPressed(false),
      _pullButtonCaptured(false),
      _lossCooldownRemainingMs(0),
      _timerRunning(false),
      _lastLossMessageIndex(-1)
{
}

SlotDialog::~SlotDialog()
{
    if (_timerRunning && _hwnd)
    {
        KillTimer(_hwnd, kAnimationTimerId);
    }
}

bool SlotDialog::RunModal(HWND hwndOwner)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &SlotDialog::WndProcThunk;
    wc.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwndOwner, GWLP_HINSTANCE));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    // Registering a second time in the same process fails harmlessly
    // (ERROR_CLASS_ALREADY_EXISTS); either way CreateWindowExW below can
    // proceed.
    RegisterClassExW(&wc);

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT rect{0, 0, kWindowWidthPx, kWindowHeightPx};
    AdjustWindowRectEx(&rect, style, FALSE, WS_EX_DLGMODALFRAME);

    RECT ownerRect{};
    GetWindowRect(hwndOwner, &ownerRect);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - (rect.right - rect.left)) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - (rect.bottom - rect.top)) / 2;

    _hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kWindowClassName, L"Waffle Jackpot", style, x, y,
                             rect.right - rect.left, rect.bottom - rect.top, hwndOwner, nullptr, wc.hInstance,
                             this);
    if (!_hwnd)
    {
        return false;
    }

    EnableWindow(hwndOwner, FALSE);
    ShowWindow(_hwnd, SW_SHOW);
    UpdateWindow(_hwnd);

    MSG msg;
    while (!_closeRequested)
    {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0)
        {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(hwndOwner, TRUE);
    SetForegroundWindow(hwndOwner);
    if (IsWindow(_hwnd))
    {
        DestroyWindow(_hwnd);
    }
    _hwnd = nullptr;

    return _wonJackpot;
}

LRESULT CALLBACK SlotDialog::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SlotDialog* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<SlotDialog*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<SlotDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SlotDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
            OnCreate(hwnd);
            return 0;
        case WM_SIZE:
            RebuildLayout(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_TIMER:
            if (wParam == kAnimationTimerId)
            {
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
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
            // Cancel: leave _wonJackpot false, just stop the modal loop.
            RequestClose();
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void SlotDialog::RequestClose()
{
    _closeRequested = true;
    // Wake GetMessageW if it's blocked waiting -- posting any message to
    // this window is enough; WM_NULL is the conventional no-op choice.
    if (_hwnd)
    {
        PostMessageW(_hwnd, WM_NULL, 0, 0);
    }
}

void SlotDialog::OnCreate(HWND hwnd)
{
    _hwnd = hwnd;

    _slotMachine = std::make_unique<waffle::SlotMachine>(_config, std::make_unique<waffle::SystemRandomSource>());
    _stateMachine.SetResetOnFailedLogon(_config.resetJackpotOnFailedLogon);

    _renderer = std::make_unique<render::SlotRenderer>(_hwnd);
    _renderer->SetDpi(static_cast<float>(GetDpiForWindow(_hwnd)));

    _audio = std::make_unique<render::AudioManager>();
    _audio->SetVolume(static_cast<float>(_config.volume));
    // spec §5.4: sound is off by default on the logon screen specifically
    // (logonSoundEnabled), separate from the general soundEnabled switch
    // Demo uses -- a slot machine blaring fanfares at 2 AM next to
    // someone's bed is not the joke.
    _audio->SetMuted(!_config.logonSoundEnabled);

    QueryPerformanceFrequency(&_perfFrequency);
    QueryPerformanceCounter(&_lastTickTime);

    RECT rc{};
    GetClientRect(_hwnd, &rc);
    RebuildLayout(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));
    RefreshStatusLine();
}

void SlotDialog::RebuildLayout(UINT widthPx, UINT heightPx)
{
    const float w = static_cast<float>(widthPx);
    const float h = static_cast<float>(heightPx);

    // Matches SlotRenderer::DrawPullButton's proportions -- see
    // DemoWindow::RebuildLayout for why no DIP/pixel conversion is
    // needed here.
    const float buttonWidth = w * 0.22f;
    const float buttonHeight = h * 0.10f;
    const float centerX = w * 0.5f;
    const float centerY = h * 0.78f;
    _pullHitRectPx = {static_cast<LONG>(centerX - buttonWidth * 0.5f),
                       static_cast<LONG>(centerY - buttonHeight * 0.5f),
                       static_cast<LONG>(centerX + buttonWidth * 0.5f),
                       static_cast<LONG>(centerY + buttonHeight * 0.5f)};

    if (_renderer)
    {
        _renderer->Resize(widthPx, heightPx);
    }
}

void SlotDialog::OnPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(_hwnd, &ps);
    if (_renderer)
    {
        // No debug overlay in the Credential Provider -- F12 isn't a
        // thing on the logon screen, and spec §5.1 gates it to Demo.
        _renderer->Render(nullptr, _statusLine, _pullButtonPressed);
    }
    EndPaint(_hwnd, &ps);
}

void SlotDialog::OnTimer()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double elapsedSec =
        static_cast<double>(now.QuadPart - _lastTickTime.QuadPart) / static_cast<double>(_perfFrequency.QuadPart);
    _lastTickTime = now;

    std::uint32_t dtMs = static_cast<std::uint32_t>(std::clamp(elapsedSec, 0.0, 0.1) * 1000.0);
    dtMs = std::max<std::uint32_t>(dtMs, 1);

    AdvanceFrame(dtMs);
}

void SlotDialog::OnKeyDown(WPARAM key)
{
    switch (key)
    {
        case VK_RETURN:
        case VK_SPACE:
            if (_renderer && (_renderer->IsSpinAnimating() || _renderer->IsJackpotSequenceActive()))
            {
                _renderer->SkipCurrentAnimation();
            }
            else if (_stateMachine.Current() == waffle::JackpotState::Idle)
            {
                PullLever();
            }
            return;
        case VK_ESCAPE:
            // spec §6.5's "Coward Mode" is the tile-level escape hatch;
            // Escape here just cancels this spin session without
            // resolving a serialization -- the tile goes back to
            // showing PULL!.
            RequestClose();
            return;
        default:
            return;
    }
}

void SlotDialog::OnLButtonDown(int x, int y)
{
    const POINT pt{x, y};
    if (PtInRect(&_pullHitRectPx, pt))
    {
        _pullButtonPressed = true;
        _pullButtonCaptured = true;
        SetCapture(_hwnd);
        InvalidateRect(_hwnd, nullptr, FALSE);
    }
}

void SlotDialog::OnLButtonUp(int x, int y)
{
    if (!_pullButtonCaptured)
    {
        return;
    }
    ReleaseCapture();
    _pullButtonCaptured = false;
    _pullButtonPressed = false;

    const POINT pt{x, y};
    if (PtInRect(&_pullHitRectPx, pt))
    {
        if (_renderer && (_renderer->IsSpinAnimating() || _renderer->IsJackpotSequenceActive()))
        {
            _renderer->SkipCurrentAnimation();
        }
        else if (_stateMachine.Current() == waffle::JackpotState::Idle)
        {
            PullLever();
        }
    }
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void SlotDialog::PullLever()
{
    if (!_stateMachine.Pull())
    {
        return;
    }
    _pendingResult = _slotMachine->Spin();
    if (_renderer)
    {
        _renderer->BeginSpin(_pendingResult, static_cast<std::uint32_t>(_config.animationDurationMs),
                              ReducedMotionRequested());
    }
    if (_audio)
    {
        _audio->Play(render::Clip::Lever);
    }
    RefreshStatusLine();
    UpdateAnimationTimer();
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void SlotDialog::AdvanceFrame(std::uint32_t dtMs)
{
    if (!_renderer)
    {
        return;
    }

    const render::FrameEvents events = _renderer->Advance(dtMs);

    if (_audio)
    {
        for (bool landed : events.reelJustLanded)
        {
            if (landed)
            {
                _audio->Play(render::Clip::ReelTick);
            }
        }
    }

    if (_stateMachine.Current() == waffle::JackpotState::Spinning && !_renderer->IsSpinAnimating())
    {
        _stateMachine.ResolveSpin(_pendingResult.jackpot);
        if (_pendingResult.jackpot)
        {
            _renderer->BeginJackpotSequence(static_cast<std::uint32_t>(_config.jackpotSequenceMs),
                                             ReducedMotionRequested());
        }
        else
        {
            if (_audio)
            {
                _audio->Play(_pendingResult.nearMiss ? render::Clip::NearMiss : render::Clip::Loss);
            }
            _lossCooldownRemainingMs = kLossCooldownMs;
        }
        RefreshStatusLine();
    }

    if (events.jackpotFanfareStarted && _audio)
    {
        _audio->Play(render::Clip::Jackpot);
    }

    if (_stateMachine.Current() == waffle::JackpotState::JackpotSequence && _renderer->IsJackpotSequenceDone())
    {
        _stateMachine.FinishJackpotSequence();
        // spec §6.1 step 3: the automaton's job ends the moment the tile
        // is unlocked -- close and hand control back to the tile, which
        // reveals the password field.
        _wonJackpot = true;
        RequestClose();
        return;
    }

    if (_stateMachine.Current() == waffle::JackpotState::Loss && _lossCooldownRemainingMs > 0)
    {
        _lossCooldownRemainingMs = (_lossCooldownRemainingMs > dtMs) ? _lossCooldownRemainingMs - dtMs : 0;
        if (_lossCooldownRemainingMs == 0)
        {
            _stateMachine.FinishCooldown();
            RefreshStatusLine();
        }
    }

    UpdateAnimationTimer();
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void SlotDialog::UpdateAnimationTimer()
{
    const bool needsTimer =
        (_renderer && (_renderer->IsSpinAnimating() || _renderer->IsJackpotSequenceActive())) ||
        _lossCooldownRemainingMs > 0;

    if (needsTimer && !_timerRunning)
    {
        SetTimer(_hwnd, kAnimationTimerId, kAnimationTimerIntervalMs, nullptr);
        QueryPerformanceCounter(&_lastTickTime);
        _timerRunning = true;
    }
    else if (!needsTimer && _timerRunning)
    {
        KillTimer(_hwnd, kAnimationTimerId);
        _timerRunning = false;
    }
}

bool SlotDialog::ReducedMotionRequested() const
{
    if (!_config.respectReducedMotion)
    {
        return false;
    }
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
    {
        return !enabled;
    }
    return false;
}

void SlotDialog::RefreshStatusLine()
{
    switch (_stateMachine.Current())
    {
        case waffle::JackpotState::Idle:
            _statusLine = L"GOOD LUCK";
            break;
        case waffle::JackpotState::Spinning:
            _statusLine = L"SPINNING...";
            break;
        case waffle::JackpotState::Loss:
            if (_pendingResult.nearMiss)
            {
                _statusLine = kNearMissMessage;
            }
            else
            {
                int index = static_cast<int>(GetTickCount64() % std::size(kLossMessages));
                if (index == _lastLossMessageIndex && std::size(kLossMessages) > 1)
                {
                    index = static_cast<int>((index + 1) % std::size(kLossMessages));
                }
                _lastLossMessageIndex = index;
                _statusLine = kLossMessages[index];
            }
            break;
        case waffle::JackpotState::JackpotSequence:
            _statusLine = L"THREE WAFFLES! JACKPOT!";
            break;
        case waffle::JackpotState::Unlocked:
            _statusLine = L"YOU MAY ENTER WINDOWS";
            break;
    }
}
