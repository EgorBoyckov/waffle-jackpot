#pragma once

namespace waffle {

// States from project spec §4.3:
//   Idle --Pull--> Spinning --> Loss --(cooldown)--> Idle
//                      |
//                      +--> JackpotSequence --> Unlocked --> (password -> Windows)
enum class JackpotState {
    Idle,
    Spinning,
    Loss,
    JackpotSequence,
    Unlocked,
};

// Drives the state machine above. Every transition method is a no-op
// (returns false, state unchanged) if called from the wrong state — spec
// §4.3: "недопустимые переходы... игнорируются, не падают". This class has
// no timers, no Win32, no rendering; callers (Demo, the CP's modal window)
// decide when e.g. a cooldown or jackpot sequence has finished and call the
// matching Finish*() method.
class JackpotStateMachine {
public:
    explicit JackpotStateMachine(bool resetOnFailedLogon = true);

    JackpotState Current() const { return state_; }

    // Idle -> Spinning.
    bool Pull();

    // Spinning -> Loss (jackpot == false) or JackpotSequence (jackpot == true).
    bool ResolveSpin(bool jackpot);

    // Loss -> Idle, once the loss/cooldown beat has played.
    bool FinishCooldown();

    // JackpotSequence -> Unlocked, once the celebration has played.
    bool FinishJackpotSequence();

    // Unlocked, but the password didn't check out. If resetOnFailedLogon is
    // set, drops back to Idle (spec §4.3 default: "THE HOUSE KEEPS THE
    // WAFFLES"); otherwise stays Unlocked so the field stays open for
    // another attempt without re-spinning.
    bool NotifyFailedLogon();

    void SetResetOnFailedLogon(bool value) { resetOnFailedLogon_ = value; }
    bool ResetOnFailedLogon() const { return resetOnFailedLogon_; }

    // Unconditional return to Idle from any state. Spec §4.3: "неизвестное
    // состояние -> безопасный откат в Idle" — this is that fallback, also
    // used whenever a Credential Provider tile needs to start clean.
    void Reset();

private:
    JackpotState state_ = JackpotState::Idle;
    bool resetOnFailedLogon_ = true;
};

}  // namespace waffle
