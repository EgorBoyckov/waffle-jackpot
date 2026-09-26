#include "SlotRenderer.h"

#include <cmath>
#include <cwchar>

namespace waffle::render {
namespace {
constexpr std::uint32_t kReelStaggerMs = 220;
constexpr std::uint32_t kReducedMotionSpinMs = 60;
constexpr std::uint32_t kReducedMotionFanfareMs = 500;
}  // namespace

SlotRenderer::SlotRenderer(HWND hwnd) : hwnd_(hwnd) {
    CreateDeviceIndependentResources();
    CreateDeviceResources();
}

SlotRenderer::~SlotRenderer() = default;

void SlotRenderer::CreateDeviceIndependentResources() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                         reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf()));
    if (!writeFactory_) {
        return;
    }

    writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 30.0f, L"en-us", &largeFormat_);
    if (largeFormat_) {
        largeFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        largeFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &smallFormat_);
    if (smallFormat_) {
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 40.0f, L"en-us", &jackpotFormat_);
    if (jackpotFormat_) {
        jackpotFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        jackpotFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    writeFactory_->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &debugFormat_);
    if (debugFormat_) {
        debugFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        debugFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
}

void SlotRenderer::CreateDeviceResources() {
    if (renderTarget_ || !d2dFactory_) {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const D2D1_SIZE_U size =
        D2D1::SizeU(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));

    const HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd_, size), &renderTarget_);
    if (FAILED(hr)) {
        // spec §9.1: never crash LogonUI over a render target we couldn't
        // get; IsValid() stays false and Render() becomes a no-op.
        return;
    }

    renderTarget_->SetDpi(dpi_, dpi_);
    renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &sceneBrush_);
    renderTarget_->CreateLayer(nullptr, &blurLayer_);

    if (symbolPainter_) {
        symbolPainter_->OnRenderTargetChanged(renderTarget_.Get());
    } else {
        symbolPainter_ = std::make_unique<SymbolPainter>(renderTarget_.Get());
    }
    if (confetti_) {
        confetti_->OnRenderTargetChanged(renderTarget_.Get());
    } else {
        confetti_ = std::make_unique<ConfettiSystem>(renderTarget_.Get());
    }
}

void SlotRenderer::DiscardDeviceResources() {
    if (symbolPainter_) {
        symbolPainter_->OnRenderTargetChanged(nullptr);
    }
    if (confetti_) {
        confetti_->OnRenderTargetChanged(nullptr);
    }
    blurLayer_.Reset();
    sceneBrush_.Reset();
    renderTarget_.Reset();
}

void SlotRenderer::Resize(UINT widthPx, UINT heightPx) {
    if (renderTarget_) {
        renderTarget_->Resize(D2D1::SizeU(widthPx, heightPx));
    }
}

void SlotRenderer::SetDpi(float dpi) {
    dpi_ = dpi > 0.0f ? dpi : 96.0f;
    if (renderTarget_) {
        renderTarget_->SetDpi(dpi_, dpi_);
    }
}

ID2D1SolidColorBrush* SlotRenderer::SceneBrush(const D2D1_COLOR_F& color) {
    sceneBrush_->SetColor(color);
    return sceneBrush_.Get();
}

void SlotRenderer::BeginSpin(const waffle::SpinResult& result, std::uint32_t animationDurationMs,
                              bool reducedMotion) {
    for (std::size_t i = 0; i < waffle::kReelCount; ++i) {
        const std::uint32_t spinDuration =
            reducedMotion ? kReducedMotionSpinMs
                           : animationDurationMs + static_cast<std::uint32_t>(i) * kReelStaggerMs;
        reelAnimators_[i].Start(result.symbols[i], 0, spinDuration);
    }
}

bool SlotRenderer::IsSpinAnimating() const {
    for (const auto& animator : reelAnimators_) {
        if (animator.IsSpinning()) {
            return true;
        }
    }
    return false;
}

void SlotRenderer::BeginJackpotSequence(std::uint32_t jackpotSequenceMs, bool reducedMotion) {
    jackpotReducedMotion_ = reducedMotion;
    jackpotSequencer_ =
        std::make_unique<JackpotSequencer>(reducedMotion ? kReducedMotionFanfareMs : jackpotSequenceMs);
    jackpotSequencer_->Start();
}

bool SlotRenderer::IsJackpotSequenceActive() const {
    return jackpotSequencer_ && !jackpotSequencer_->IsDone();
}

bool SlotRenderer::IsJackpotSequenceDone() const {
    return !jackpotSequencer_ || jackpotSequencer_->IsDone();
}

void SlotRenderer::SkipCurrentAnimation() {
    bool anySpinning = false;
    for (auto& animator : reelAnimators_) {
        if (animator.IsSpinning()) {
            animator.SkipToEnd();
            anySpinning = true;
        }
    }
    if (!anySpinning && jackpotSequencer_ && !jackpotSequencer_->IsDone()) {
        jackpotSequencer_->Skip();
    }
}

FrameEvents SlotRenderer::Advance(std::uint32_t dtMs) {
    FrameEvents events;
    lampElapsedMs_ += dtMs;

    for (std::size_t i = 0; i < waffle::kReelCount; ++i) {
        events.reelJustLanded[i] = reelAnimators_[i].Advance(dtMs);
    }

    if (jackpotSequencer_) {
        const JackpotPhase before = jackpotSequencer_->CurrentPhase();
        jackpotSequencer_->Advance(dtMs);
        const JackpotPhase after = jackpotSequencer_->CurrentPhase();

        if (before != JackpotPhase::Fanfare && after == JackpotPhase::Fanfare) {
            events.jackpotFanfareStarted = true;
            if (!jackpotReducedMotion_ && confetti_ && renderTarget_) {
                const D2D1_SIZE_F size = renderTarget_->GetSize();
                confetti_->Burst(size.width, size.height);
            }
        }
        if (before != JackpotPhase::Done && after == JackpotPhase::Done) {
            events.jackpotSequenceJustFinished = true;
        }
    }

    if (confetti_) {
        confetti_->Update(dtMs);
    }

    return events;
}

void SlotRenderer::DrawCabinet(const D2D1_SIZE_F& size) {
    renderTarget_->Clear(palette::kCabinetDark);
    const D2D1_RECT_F panel =
        D2D1::RectF(size.width * 0.03f, size.height * 0.03f, size.width * 0.97f, size.height * 0.97f);
    renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(panel, 18.0f, 18.0f), SceneBrush(palette::kCabinetPanel));
    renderTarget_->DrawRoundedRectangle(D2D1::RoundedRect(panel, 18.0f, 18.0f), SceneBrush(palette::kGold), 4.0f);
}

void SlotRenderer::DrawLamps(const D2D1_SIZE_F& size) {
    const bool silent = jackpotSequencer_ && jackpotSequencer_->CurrentPhase() == JackpotPhase::Silence;

    // Full on/off cycle every 400ms (~2.5 Hz) -- under the "not more than
    // 3 times a second" photosensitive-safety cap from spec §5.2.
    constexpr int kLampCount = 20;
    constexpr float kPeriodMs = 400.0f;
    const float phase = std::fmod(static_cast<float>(lampElapsedMs_), kPeriodMs) / kPeriodMs;

    const D2D1_RECT_F panel =
        D2D1::RectF(size.width * 0.03f, size.height * 0.03f, size.width * 0.97f, size.height * 0.97f);
    const float w = panel.right - panel.left;
    const float h = panel.bottom - panel.top;
    const float perim = 2.0f * (w + h);
    const float radius = std::min(w, h) * 0.012f;

    for (int i = 0; i < kLampCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kLampCount);
        const float dist = t * perim;
        float x, y;
        if (dist < w) {
            x = panel.left + dist;
            y = panel.top;
        } else if (dist < w + h) {
            x = panel.right;
            y = panel.top + (dist - w);
        } else if (dist < 2.0f * w + h) {
            x = panel.right - (dist - w - h);
            y = panel.bottom;
        } else {
            x = panel.left;
            y = panel.bottom - (dist - 2.0f * w - h);
        }

        float lampPhase = phase + t;
        lampPhase -= std::floor(lampPhase);
        const bool on = lampPhase < 0.5f;

        D2D1_COLOR_F color = palette::kLampOff;
        if (!silent && on) {
            color = (i % 2 == 0) ? palette::kLampRed : palette::kLampYellow;
        }
        renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), SceneBrush(color));
    }
}

void SlotRenderer::DrawReels(const D2D1_SIZE_F& size) {
    const float margin = size.width * 0.08f;
    const float reelAreaWidth = size.width - margin * 2.0f;
    const float reelWidth = reelAreaWidth / static_cast<float>(waffle::kReelCount);
    const float reelTop = size.height * 0.20f;
    const float reelHeight = size.height * 0.42f;
    const float gap = reelWidth * 0.06f;

    const JackpotPhase phase = jackpotSequencer_ ? jackpotSequencer_->CurrentPhase() : JackpotPhase::Done;
    const bool glowing =
        phase == JackpotPhase::Glow || phase == JackpotPhase::ZoomIn || phase == JackpotPhase::Fanfare;

    for (std::size_t i = 0; i < waffle::kReelCount; ++i) {
        const D2D1_RECT_F cell =
            D2D1::RectF(margin + reelWidth * static_cast<float>(i) + gap * 0.5f, reelTop,
                        margin + reelWidth * static_cast<float>(i + 1) - gap * 0.5f, reelTop + reelHeight);

        renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(cell, 8.0f, 8.0f),
                                             SceneBrush(D2D1::ColorF(0.03f, 0.02f, 0.04f, 1.0f)));

        const float blur = reelAnimators_[i].MotionBlur();
        D2D1_MATRIX_3X2_F saved;
        renderTarget_->GetTransform(&saved);

        if (blur > 0.0f) {
            const D2D1_POINT_2F center = D2D1::Point2F((cell.left + cell.right) * 0.5f, (cell.top + cell.bottom) * 0.5f);
            const auto stretch = D2D1::Matrix3x2F::Scale(1.0f, 1.0f + blur * 0.5f, center);
            renderTarget_->SetTransform(stretch * saved);

            D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters();
            layerParams.opacity = 1.0f - blur * 0.4f;
            renderTarget_->PushLayer(layerParams, blurLayer_.Get());
            symbolPainter_->DrawSymbol(reelAnimators_[i].CurrentSymbol(), cell, glowing ? 1.0f : 0.0f);
            renderTarget_->PopLayer();
        } else {
            symbolPainter_->DrawSymbol(reelAnimators_[i].CurrentSymbol(), cell, glowing ? 1.0f : 0.0f);
        }

        renderTarget_->SetTransform(saved);
        renderTarget_->DrawRoundedRectangle(D2D1::RoundedRect(cell, 8.0f, 8.0f), SceneBrush(palette::kGold), 2.0f);
    }
}

void SlotRenderer::DrawPullButton(const D2D1_SIZE_F& size, bool pressed) {
    const float w = size.width * 0.22f;
    const float h = size.height * 0.10f;
    const float cx = size.width * 0.5f;
    const float cy = size.height * 0.78f + (pressed ? h * 0.08f : 0.0f);
    const D2D1_RECT_F rect = D2D1::RectF(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f);

    const D2D1_COLOR_F top = pressed ? palette::kGold : palette::kGoldBright;
    const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, h * 0.4f, h * 0.4f);
    renderTarget_->FillRoundedRectangle(rr, SceneBrush(top));
    renderTarget_->DrawRoundedRectangle(rr, SceneBrush(D2D1::ColorF(0.35f, 0.24f, 0.03f, 1.0f)),
                                         pressed ? 2.0f : 3.0f);

    if (smallFormat_) {
        renderTarget_->DrawText(L"PULL!", 5, smallFormat_.Get(), rect, SceneBrush(palette::kCabinetDark));
    }
}

void SlotRenderer::DrawTexts(const D2D1_SIZE_F& size, const std::wstring& statusLine) {
    if (largeFormat_) {
        const D2D1_RECT_F titleRect = D2D1::RectF(0.0f, size.height * 0.02f, size.width, size.height * 0.14f);
        static constexpr wchar_t kTitle[] = L"WAFFLE JACKPOT";
        renderTarget_->DrawText(kTitle, static_cast<UINT32>(std::wcslen(kTitle)), largeFormat_.Get(), titleRect,
                                 SceneBrush(palette::kTextLight));
    }

    if (smallFormat_ && !statusLine.empty()) {
        const D2D1_RECT_F statusRect = D2D1::RectF(size.width * 0.05f, size.height * 0.64f, size.width * 0.95f,
                                                     size.height * 0.72f);
        renderTarget_->DrawText(statusLine.c_str(), static_cast<UINT32>(statusLine.size()), smallFormat_.Get(),
                                 statusRect, SceneBrush(palette::kTextLight));
    }

    const JackpotPhase phase = jackpotSequencer_ ? jackpotSequencer_->CurrentPhase() : JackpotPhase::Done;
    if (phase == JackpotPhase::Fanfare && jackpotFormat_) {
        const D2D1_RECT_F bannerRect =
            D2D1::RectF(size.width * 0.05f, size.height * 0.28f, size.width * 0.95f, size.height * 0.62f);
        static constexpr wchar_t kBanner[] = L"WAFFLE JACKPOT!\nYOU MAY ENTER WINDOWS";
        renderTarget_->DrawText(kBanner, static_cast<UINT32>(std::wcslen(kBanner)), jackpotFormat_.Get(),
                                 bannerRect, SceneBrush(palette::kGoldBright));
    }
}

void SlotRenderer::DrawDebugOverlay(const DebugOverlayInfo& info) {
    if (!debugFormat_) {
        return;
    }

    const D2D1_RECT_F panelRect = D2D1::RectF(8.0f, 8.0f, 280.0f, 148.0f);
    renderTarget_->FillRectangle(panelRect, SceneBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f)));

    const wchar_t* stateName = L"Idle";
    switch (info.state) {
        case waffle::JackpotState::Idle:            stateName = L"Idle"; break;
        case waffle::JackpotState::Spinning:        stateName = L"Spinning"; break;
        case waffle::JackpotState::Loss:            stateName = L"Loss"; break;
        case waffle::JackpotState::JackpotSequence: stateName = L"JackpotSequence"; break;
        case waffle::JackpotState::Unlocked:        stateName = L"Unlocked"; break;
    }

    const double oddsOneIn = info.naturalJackpotProbability > 0.0 ? 1.0 / info.naturalJackpotProbability : 0.0;

    wchar_t buf[512];
    swprintf_s(buf, L"state: %ls\nreels: %hs  %hs  %hs\nattempts since jackpot: %d\n"
                     L"P(jackpot): %.4f (~1/%.0f)\nfps: %.1f",
               stateName, waffle::ToString(info.reels[0]), waffle::ToString(info.reels[1]),
               waffle::ToString(info.reels[2]), info.attemptsSinceLastJackpot, info.naturalJackpotProbability,
               oddsOneIn, info.fps);

    const D2D1_RECT_F textRect = D2D1::RectF(16.0f, 16.0f, 272.0f, 140.0f);
    renderTarget_->DrawText(buf, static_cast<UINT32>(std::wcslen(buf)), debugFormat_.Get(), textRect,
                             SceneBrush(palette::kTextLight));
}

void SlotRenderer::Render(const DebugOverlayInfo* overlay, const std::wstring& statusLine, bool pullButtonPressed) {
    if (!renderTarget_) {
        CreateDeviceResources();
        if (!renderTarget_) {
            return;
        }
    }

    renderTarget_->BeginDraw();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());

    const D2D1_SIZE_F size = renderTarget_->GetSize();
    DrawCabinet(size);
    DrawLamps(size);
    DrawReels(size);
    DrawPullButton(size, pullButtonPressed);
    if (confetti_) {
        confetti_->Draw();
    }
    DrawTexts(size, statusLine);
    if (overlay) {
        DrawDebugOverlay(*overlay);
    }

    const HRESULT hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

}  // namespace waffle::render
