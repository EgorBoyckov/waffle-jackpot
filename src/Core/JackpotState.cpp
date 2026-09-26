#include "JackpotState.h"

namespace waffle {

JackpotStateMachine::JackpotStateMachine(bool resetOnFailedLogon)
    : resetOnFailedLogon_(resetOnFailedLogon) {}

bool JackpotStateMachine::Pull() {
    if (state_ != JackpotState::Idle) {
        return false;
    }
    state_ = JackpotState::Spinning;
    return true;
}

bool JackpotStateMachine::ResolveSpin(bool jackpot) {
    if (state_ != JackpotState::Spinning) {
        return false;
    }
    state_ = jackpot ? JackpotState::JackpotSequence : JackpotState::Loss;
    return true;
}

bool JackpotStateMachine::FinishCooldown() {
    if (state_ != JackpotState::Loss) {
        return false;
    }
    state_ = JackpotState::Idle;
    return true;
}

bool JackpotStateMachine::FinishJackpotSequence() {
    if (state_ != JackpotState::JackpotSequence) {
        return false;
    }
    state_ = JackpotState::Unlocked;
    return true;
}

bool JackpotStateMachine::NotifyFailedLogon() {
    if (state_ != JackpotState::Unlocked) {
        return false;
    }
    if (resetOnFailedLogon_) {
        state_ = JackpotState::Idle;
    }
    return true;
}

void JackpotStateMachine::Reset() {
    state_ = JackpotState::Idle;
}

}  // namespace waffle
