# Manual test plan (Credential Provider)

Everything WaffleCore can verify automatically already has automated
tests (`tests/*.cpp`, GoogleTest). This checklist is for the parts that
can't be — the Credential Provider actually running inside `LogonUI.exe`,
on real Windows, with real accounts.

**None of the rows below have been executed.** This project was built
without access to a Windows machine or VM (see the README's own
[Verification status](../README.md#verification-status) table). Run this
on a disposable VM, **with a snapshot taken before `install.ps1`**, before
trusting any of it on a machine you care about. Check off rows as you
verify them, and note the Windows build/VM software you used.

## Setup

- [ ] VM created (Hyper-V / VMware / VirtualBox), Windows 10 1809+ or
      Windows 11, x64.
- [ ] Snapshot taken **before** running `install.ps1`.
- [ ] Solution built in Release, x64 (`cmake --build build --config Release`).
- [ ] `installer\install.ps1` run as Administrator; confirmation prompt
      answered `YES`.
- [ ] `WaffleJackpotControl.exe status` reports installed + enabled.

## Core login scenarios

- [ ] **Local account**, correct password on the first jackpot →
      logs in.
- [ ] **Microsoft account (MSA)** with a password set, correct password
      on jackpot → logs in.
- [ ] **Wrong password** after winning the jackpot → the tile shows an
      error, password field clears, and (with default config) re-hides
      behind a fresh "THE HOUSE KEEPS THE WAFFLES" gate
      (`resetJackpotOnFailedLogon: true`). Set it to `false` and confirm
      the field instead stays open for another attempt without
      re-spinning.
- [ ] **Lock screen** (Win+L) shows the same tile/flow as logon
      (`CPUS_UNLOCK_WORKSTATION`).
- [ ] **Switch user** (multiple accounts logged in) — the tile appears
      correctly per-user, and one user's jackpot state doesn't leak into
      another's tile.
- [ ] **Remote Desktop (RDP)** session: with default config
      (`showInRemoteSessions: false`), the Waffle tile does **not**
      appear — the standard provider handles the RDP logon. Flip the
      setting and confirm it does appear.

## Coward Mode / system providers

- [ ] "Sign-in options" on the logon screen still shows the standard
      password/PIN tile, and it works normally, with Waffle Jackpot
      installed.
- [ ] If the account only has a PIN (no password) — confirm it signs in
      through the standard tile, not this one.
- [ ] If "Require Windows Hello sign-in for Microsoft accounts" is
      enabled — confirm password-based tiles (including this one)
      disappear, as expected, and that this doesn't crash LogonUI.

## Display / rendering

- [ ] **150% DPI** and **200% DPI**: cabinet, symbols, and text scale
      correctly, nothing clips or blurs oddly.
- [ ] **Two monitors**: the tile/modal automaton appears on the correct
      monitor and doesn't misbehave when moved or when the active
      monitor changes.
- [ ] **"Show animations" disabled** (Settings → Accessibility → Visual
      effects, or the older `SPI_GETCLIENTAREAANIMATION` toggle): spins
      and the jackpot sequence collapse to short transitions, no
      confetti burst.
- [ ] **High-contrast mode**: text stays readable, the PULL button
      stays visible.

## Audio

- [ ] Default config (`logonSoundEnabled: false`): the modal automaton
      is silent on the actual logon screen.
- [ ] Set `logonSoundEnabled: true`: lever/reel-tick/loss/jackpot sounds
      all play, jackpot fanfare is normalized (no clipping/distortion).
- [ ] Demo Mode's own `soundEnabled`/volume controls work independently
      of the logon-specific setting.

## Robustness

- [ ] **Corrupt `config.json`** (`%ProgramData%\WaffleJackpot\config.json`
      — replace with `{not valid json`): the provider falls back to
      defaults and still shows a working tile, no crash. (There's no
      loose asset file to "go missing" — symbols/tile image/sounds are
      all generated in code — so this replaces the spec's original
      "missing asset" case for this project.)
  - [ ] Also test: empty file, valid-but-empty `{}`, and a config with an
        out-of-range value (e.g. `"volume": 42`) — each should clamp or
        fall back per `tests/ConfigTests.cpp`'s already-verified rules,
        now confirmed to actually take effect through the real DLL.
- [ ] **Kill switch**: force 3 crashes in a row (e.g. temporarily corrupt
      the DLL's dependencies, or induce a failure some other way) and
      confirm the tile stops appearing afterward, *without* a config
      change. Confirm `WaffleJackpotControl.exe status` reports it as
      active, and that `WaffleJackpotControl.exe enable` clears it and
      brings the tile back.
  - [ ] Confirm a **successful login** resets the counter to zero even
        after 1–2 prior failures (short of the threshold).

## Install / enable / disable / uninstall lifecycle

- [ ] `install.ps1` run twice in a row → second run succeeds cleanly
      (idempotent), no duplicate registrations or errors.
- [ ] `disable.ps1` → tile disappears on next logon screen; DLL, COM
      registration, and config are untouched (`status` still shows
      installed).
- [ ] `enable.ps1` → tile reappears, no reinstall needed.
- [ ] Same enable/disable roundtrip via `WaffleJackpotControl.exe` GUI
      buttons and via its CLI (`enable`/`disable` args), from both an
      elevated and a non-elevated prompt (confirm the non-elevated path
      triggers a UAC prompt rather than silently failing).
- [ ] `uninstall.ps1` → DLL, COM registration, provider registration,
      config directory, and kill-switch registry key are all gone
      afterward; report printed lists each as removed. Re-run it
      immediately after → reports "already absent" for everything,
      still exits cleanly.
- [ ] After uninstall, the logon screen shows only the standard
      Windows tiles — no trace of Waffle Jackpot.

## Recovery

- [ ] Follow [`recovery/RECOVERY.md`](../recovery/RECOVERY.md) step 1
      (Coward Mode) end-to-end.
- [ ] Follow step 2 (Safe Mode + `disable.ps1`/`WaffleJackpotControl.exe
      disable`) end-to-end.
- [ ] Follow step 3 (WinRE + `recovery/offline-disable.cmd`) end-to-end,
      including getting the drive letter right via `diskpart` — this is
      the step most likely to have something wrong in it since it's the
      least like normal Windows usage.
- [ ] Confirm the BitLocker recovery-key reminder in `RECOVERY.md` step 0
      is actually necessary/sufficient if the test VM has BitLocker
      enabled on the system drive.

---

Once every box above is checked on at least one real VM, update the
verification-status tables in `README.md` and `RECOVERY.md` to reflect
it — including which Windows build and VM software you used.
