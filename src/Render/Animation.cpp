#include "Animation.h"

#include <algorithm>

namespace waffle::render {
namespace {
constexpr std::uint32_t kSettleWindowMs = 300;
constexpr std::uint32_t kFastCycleMs = 70;
constexpr std::uint32_t kSlowCycleMs = 140;

constexpr std::uint32_t kSilenceMs = 400;
constexpr std::uint32_t kGlowMs = 400;
constexpr std::uint32_t kZoomMs = 300;
}  // namespace

float EaseOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

void ReelSpinAnimator::Start(waffle::Symbol target, std::uint32_t startDelayMs, std::uint32_t spinDurationMs) {
    target_ = target;
    startDelayMs_ = startDelayMs;
    spinDurationMs_ = std::max<std::uint32_t>(spinDurationMs, 1);
    elapsedMs_ = 0;
    cycleAccumulatorMs_ = 0;
    currentSymbol_ = target_;
    blur_ = 0.0f;
    phase_ = Phase::WaitingToStart;
}

bool ReelSpinAnimator::Advance(std::uint32_t dtMs) {
    if (phase_ == Phase::Idle || phase_ == Phase::Landed) {
        return false;
    }

    elapsedMs_ += dtMs;

    if (elapsedMs_ < startDelayMs_) {
        return false;
    }

    const std::uint32_t spinElapsed = elapsedMs_ - startDelayMs_;
    if (spinElapsed >= spinDurationMs_) {
        currentSymbol_ = target_;
        blur_ = 0.0f;
        phase_ = Phase::Landed;
        return true;
    }

    phase_ = Phase::Spinning;
    const std::uint32_t remaining = spinDurationMs_ - spinElapsed;
    const std::uint32_t cycleIntervalMs = remaining < kSettleWindowMs ? kSlowCycleMs : kFastCycleMs;

    cycleAccumulatorMs_ += dtMs;
    if (cycleAccumulatorMs_ >= cycleIntervalMs) {
        cycleAccumulatorMs_ = 0;
        const int next = (static_cast<int>(currentSymbol_) + 1) % static_cast<int>(waffle::Symbol::Count);
        currentSymbol_ = static_cast<waffle::Symbol>(next);
    }

    blur_ = remaining < kSettleWindowMs
                ? static_cast<float>(remaining) / static_cast<float>(kSettleWindowMs)
                : 1.0f;
    return false;
}

bool ReelSpinAnimator::IsSpinning() const {
    return phase_ == Phase::WaitingToStart || phase_ == Phase::Spinning;
}

bool ReelSpinAnimator::HasLanded() const {
    return phase_ == Phase::Landed;
}

void ReelSpinAnimator::SkipToEnd() {
    currentSymbol_ = target_;
    blur_ = 0.0f;
    phase_ = Phase::Landed;
}

JackpotSequencer::JackpotSequencer(std::uint32_t fanfareDurationMs)
    : fanfareDurationMs_(std::max<std::uint32_t>(fanfareDurationMs, 500)) {}

void JackpotSequencer::EnterPhase(JackpotPhase phase) {
    phase_ = phase;
    phaseElapsedMs_ = 0;
    switch (phase) {
        case JackpotPhase::Silence: phaseDurationMs_ = kSilenceMs; break;
        case JackpotPhase::Glow:    phaseDurationMs_ = kGlowMs; break;
        case JackpotPhase::ZoomIn:  phaseDurationMs_ = kZoomMs; break;
        case JackpotPhase::Fanfare: phaseDurationMs_ = fanfareDurationMs_; break;
        case JackpotPhase::Done:    phaseDurationMs_ = 0; break;
    }
}

void JackpotSequencer::Start() {
    EnterPhase(JackpotPhase::Silence);
}

void JackpotSequencer::Advance(std::uint32_t dtMs) {
    if (phase_ == JackpotPhase::Done) {
        return;
    }
    phaseElapsedMs_ += dtMs;
    if (phaseElapsedMs_ < phaseDurationMs_) {
        return;
    }
    switch (phase_) {
        case JackpotPhase::Silence: EnterPhase(JackpotPhase::Glow); break;
        case JackpotPhase::Glow:    EnterPhase(JackpotPhase::ZoomIn); break;
        case JackpotPhase::ZoomIn:  EnterPhase(JackpotPhase::Fanfare); break;
        case JackpotPhase::Fanfare: EnterPhase(JackpotPhase::Done); break;
        case JackpotPhase::Done:    break;
    }
}

void JackpotSequencer::Skip() {
    EnterPhase(JackpotPhase::Done);
}

float JackpotSequencer::PhaseProgress() const {
    if (phaseDurationMs_ == 0) {
        return 1.0f;
    }
    return std::min(1.0f, static_cast<float>(phaseElapsedMs_) / static_cast<float>(phaseDurationMs_));
}

}  // namespace waffle::render
