#include <gtest/gtest.h>

#include "Core/JackpotState.h"

using waffle::JackpotState;
using waffle::JackpotStateMachine;

TEST(JackpotStateMachineTest, StartsIdle) {
    JackpotStateMachine sm;
    EXPECT_EQ(sm.Current(), JackpotState::Idle);
}

TEST(JackpotStateMachineTest, FullLossCycle) {
    JackpotStateMachine sm;
    EXPECT_TRUE(sm.Pull());
    EXPECT_EQ(sm.Current(), JackpotState::Spinning);

    EXPECT_TRUE(sm.ResolveSpin(/*jackpot=*/false));
    EXPECT_EQ(sm.Current(), JackpotState::Loss);

    EXPECT_TRUE(sm.FinishCooldown());
    EXPECT_EQ(sm.Current(), JackpotState::Idle);
}

TEST(JackpotStateMachineTest, FullJackpotCycle) {
    JackpotStateMachine sm;
    ASSERT_TRUE(sm.Pull());
    ASSERT_TRUE(sm.ResolveSpin(/*jackpot=*/true));
    EXPECT_EQ(sm.Current(), JackpotState::JackpotSequence);

    EXPECT_TRUE(sm.FinishJackpotSequence());
    EXPECT_EQ(sm.Current(), JackpotState::Unlocked);
}

TEST(JackpotStateMachineTest, InvalidTransitionsAreNoOpsAndIgnored) {
    JackpotStateMachine sm;

    // Not spinning yet -- ResolveSpin must be rejected.
    EXPECT_FALSE(sm.ResolveSpin(true));
    EXPECT_EQ(sm.Current(), JackpotState::Idle);

    // Not in Loss -- FinishCooldown rejected.
    EXPECT_FALSE(sm.FinishCooldown());
    EXPECT_EQ(sm.Current(), JackpotState::Idle);

    // Not in JackpotSequence -- FinishJackpotSequence rejected.
    EXPECT_FALSE(sm.FinishJackpotSequence());
    EXPECT_EQ(sm.Current(), JackpotState::Idle);

    // Not Unlocked -- NotifyFailedLogon rejected.
    EXPECT_FALSE(sm.NotifyFailedLogon());
    EXPECT_EQ(sm.Current(), JackpotState::Idle);

    ASSERT_TRUE(sm.Pull());
    // Already spinning -- a second Pull is rejected.
    EXPECT_FALSE(sm.Pull());
    EXPECT_EQ(sm.Current(), JackpotState::Spinning);
}

TEST(JackpotStateMachineTest, FailedLogonResetsToIdleByDefault) {
    JackpotStateMachine sm(/*resetOnFailedLogon=*/true);
    ASSERT_TRUE(sm.Pull());
    ASSERT_TRUE(sm.ResolveSpin(true));
    ASSERT_TRUE(sm.FinishJackpotSequence());
    ASSERT_EQ(sm.Current(), JackpotState::Unlocked);

    EXPECT_TRUE(sm.NotifyFailedLogon());
    EXPECT_EQ(sm.Current(), JackpotState::Idle);
}

TEST(JackpotStateMachineTest, FailedLogonStaysUnlockedWhenConfigured) {
    JackpotStateMachine sm(/*resetOnFailedLogon=*/false);
    ASSERT_TRUE(sm.Pull());
    ASSERT_TRUE(sm.ResolveSpin(true));
    ASSERT_TRUE(sm.FinishJackpotSequence());
    ASSERT_EQ(sm.Current(), JackpotState::Unlocked);

    EXPECT_TRUE(sm.NotifyFailedLogon());
    EXPECT_EQ(sm.Current(), JackpotState::Unlocked);
}

TEST(JackpotStateMachineTest, SetResetOnFailedLogonTakesEffectImmediately) {
    JackpotStateMachine sm(/*resetOnFailedLogon=*/true);
    sm.SetResetOnFailedLogon(false);
    EXPECT_FALSE(sm.ResetOnFailedLogon());

    ASSERT_TRUE(sm.Pull());
    ASSERT_TRUE(sm.ResolveSpin(true));
    ASSERT_TRUE(sm.FinishJackpotSequence());
    sm.NotifyFailedLogon();
    EXPECT_EQ(sm.Current(), JackpotState::Unlocked);
}

TEST(JackpotStateMachineTest, ResetForcesIdleFromAnyState) {
    JackpotStateMachine sm;
    ASSERT_TRUE(sm.Pull());
    ASSERT_EQ(sm.Current(), JackpotState::Spinning);

    sm.Reset();
    EXPECT_EQ(sm.Current(), JackpotState::Idle);

    // Reset is safe to call even when already Idle.
    sm.Reset();
    EXPECT_EQ(sm.Current(), JackpotState::Idle);
}
