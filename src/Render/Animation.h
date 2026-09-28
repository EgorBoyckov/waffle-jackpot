#pragma once

#include <cstdint>

#include "Core/Symbol.h"

namespace waffle::render {

// Standard ease-out cubic; used for the reel's final settle and any UI
// transition (zoom-in, fade) that wants to start fast and land soft.
float EaseOutCubic(float t);

// Drives one reel's *visual* spin only. The outcome (`target`) was already
// decided by SlotMachine before this animator ever runs — per project spec
// §2.1, the animation never influences the result, only performs it. This
// class doesn't know about RNG or weights, just how to cycle glyphs and
// land on schedule.
class ReelSpinAnimator {
public:
    // startDelayMs staggers this reel's landing after the shared spin
    // start, giving the "поочерёдная остановка барабанов" beat (spec
    // §5.1). spinDurationMs is how long *this* reel spends actually
    // cycling before settling on `target`.
    void Start(waffle::Symbol target, std::uint32_t startDelayMs, std::uint32_t spinDurationMs);

    // Advances by dtMs. Returns true exactly once, on the tick this reel
    // lands — the caller uses that to trigger the per-reel "tick" sound.
    bool Advance(std::uint32_t dtMs);

    bool IsSpinning() const;
    bool HasLanded() const;

    // Symbol to draw right now: cycles while spinning, holds at `target`
    // once landed.
    waffle::Symbol CurrentSymbol() const { return currentSymbol_; }

    // 0 = no blur, 1 = full-speed vertical motion blur. Ramps down during
    // the settle window just before landing.
    float MotionBlur() const { return blur_; }

    // Jumps straight to the landed state — used when the user clicks
    // through a spin, or when SPI_GETCLIENTAREAANIMATION says animations
    // are off (spec §5.2, "короткие переходы без вращения").
    void SkipToEnd();

private:
    enum class Phase { Idle, WaitingToStart, Spinning, Landed };

    waffle::Symbol target_ = waffle::Symbol::Waffle;
    Phase phase_ = Phase::Idle;
    std::uint32_t elapsedMs_ = 0;
    std::uint32_t startDelayMs_ = 0;
    std::uint32_t spinDurationMs_ = 1;
    std::uint32_t cycleAccumulatorMs_ = 0;
    waffle::Symbol currentSymbol_ = waffle::Symbol::Cherry;
    float blur_ = 0.0f;
};

// Beats of the jackpot celebration, spec §5.3 steps 2-7 (step 1, the third
// reel landing, is ReelSpinAnimator's job; step 8's later transition to
// "Continue to Windows authentication..." is the caller's timer, not
// this one's).
enum class JackpotPhase {
    Silence,  // 300-500ms, lamps off
    Glow,     // the three waffles start glowing
    ZoomIn,   // light zoom-in on the cabinet
    Fanfare,  // fanfare + confetti + "WAFFLE JACKPOT!" — the long beat
    Done,
};

// Drives the timed sequence above. The Fanfare beat's length comes from
// Config::jackpotSequenceMs; the earlier beats use short fixed durations
// matching spec §5.3's numbers, since they aren't configurable in the
// schema (spec §7). Skip() jumps straight to Done — spec §5.3: "даже
// самая смешная анимация на десятый раз должна пропускаться".
class JackpotSequencer {
public:
    explicit JackpotSequencer(std::uint32_t fanfareDurationMs);

    void Start();
    void Advance(std::uint32_t dtMs);
    void Skip();

    JackpotPhase CurrentPhase() const { return phase_; }
    bool IsDone() const { return phase_ == JackpotPhase::Done; }

    // 0..1 progress within the current phase, for glow/zoom interpolation.
    float PhaseProgress() const;

private:
    void EnterPhase(JackpotPhase phase);

    JackpotPhase phase_ = JackpotPhase::Done;
    std::uint32_t phaseElapsedMs_ = 0;
    std::uint32_t phaseDurationMs_ = 0;
    std::uint32_t fanfareDurationMs_;
};

}  // namespace waffle::render
