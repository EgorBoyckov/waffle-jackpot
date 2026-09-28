#pragma once

namespace waffle::cp {

// spec §9.2: after 3 consecutive failed initializations/crashes, the
// provider stops showing a tile until a human runs
// `WaffleJackpotControl.exe enable`. Tracked in
// HKLM\SOFTWARE\WaffleJackpot\CrashCount, written by SYSTEM since this
// DLL runs as SYSTEM inside LogonUI.
//
// "Crash" here is inferred, not caught: RecordInitializationStart()
// increments the counter at the top of JackpotProvider's constructor,
// and RecordCleanShutdown() decrements it back in the destructor. A
// normal session -- construct, enumerate tiles, destruct once LogonUI is
// done with us, whether or not the user actually logged in -- nets to no
// change. Only a real crash (LogonUI dying, an access violation taking
// the process down) skips the destructor and leaves the increment stuck,
// so it accumulates across restarts. RecordSuccessfulLogon() goes
// further and zeroes the counter outright, per spec's "счётчик
// сбрасывается при успешном входе".
inline constexpr int kKillSwitchThreshold = 3;

void RecordInitializationStart();
void RecordCleanShutdown();
void RecordSuccessfulLogon();

// True once the counter has reached the threshold. ShouldParticipate()
// treats this the same as a disabled config -- zero tiles, never a crash
// or hard failure of our own (spec §9.1).
bool IsKillSwitchActive();

}  // namespace waffle::cp
