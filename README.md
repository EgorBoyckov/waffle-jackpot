# Waffle Jackpot Login 🎰🧇

⚠️ **THIS IS A JOKE PROJECT. WAFFLES ARE NOT A SECURITY BOUNDARY.**

To log into Windows, first win three waffles on a slot machine bolted to
the logon screen. Real authentication is still done entirely by Windows
(LSA + Kerberos/MSV1_0) — this project never replaces, weakens, or
bypasses it. It only decides *when the password field is allowed to
appear.*

> «Почему, чтобы войти в Windows, я должен выиграть три вафли?»

No money, no bets, no purchases, no prizes, no ads. Only waffles.

**Screenshots / GIF of Demo Mode:** none yet. This project was built in an
environment with no Windows machine or VM available to run and capture
Demo Mode — see [Verification status](#verification-status) below for
exactly what has and hasn't been run. If you build this and it works,
please add a screenshot here.

---

## Why three waffles?

Sometime in 2024, a systems engineer was staring at a Belgian waffle iron
at 2 AM, deeply unhappy with `sudo` prompts, and had a realization: every
security control in existence is basically a slot machine you're forced
to win before it lets you do your job. MFA, CAPTCHAs, password
complexity rules — all of it. So why not be honest about it? Windows
already has perfectly good authentication. What it's missing is
*ceremony*. Three waffles is the minimum viable ceremony: fewer than
three and it's not a jackpot, more than three and nobody would ever get
into their own computer.

The math backs this up (barely) — see below.

---

## The one rule

```cpp
constexpr bool IsJackpot(Symbol a, Symbol b, Symbol c) noexcept
{
    return a == Symbol::Waffle && b == Symbol::Waffle && c == Symbol::Waffle;
}
```

🧇🧇🧇 → JACKPOT. Everything else — including 7️⃣7️⃣7️⃣, 🍒🍒🍒, or two
waffles and a cherry — is a loss. No easter eggs, no secret key
combinations, no "master waffle." The only other way into Windows is the
completely honest "Sign-in options → Coward Mode" link that's on the tile
at all times (see [Coward Mode](#coward-mode-and-what-this-does-not-hide)).

## The math (so you can actually log in)

At 8 equally-weighted symbols, the naive odds of 🧇🧇🧇 are 1/512 — about
512 pulls per login, which is funny once and then unbearable. So:

1. **The outcome is decided before the animation.** The spin result is
   generated first; the reels then animate to land on it — the same way
   real slot machines work. Animation never influences the result.
2. **Symbol weights are configurable** (`config.json`'s `reelWeights`),
   independently of `IsJackpot()`, which never changes.
3. **A pity timer** (`guaranteedJackpotAfterAttempts`) guarantees a
   jackpot on the Nth attempt. It's mandatory and on by default in the
   logon scenario — this is the difference between a joke and being
   locked out of your own computer.
4. **Near-misses** (two waffles, third symbol different) are deliberately
   boosted above their natural frequency, so the "SO CLOSE" beat actually
   lands often enough to be funny.
5. Randomness comes from a CSPRNG-seeded `std::mt19937_64`
   (`BCryptGenRandom` on Windows). Cryptographic strength isn't needed —
   this is a joke, not a security control — but the generator is
   injectable, so tests use a deterministic source instead.

Default weights (`config/config.default.json`): WAFFLE 5, CHERRY/LEMON/BELL
2 each, DIAMOND/STAR/SEVEN/LUCKY 1 each — a **1/27 chance per pull**
(`p³` where `p = 5/15`), with a guaranteed jackpot by the 8th attempt.

## Quick start: Demo Mode

`WaffleJackpotDemo.exe` is a normal window — it never touches your logon
screen. Build it (see [Building](#building) below), then:

- **PULL!** with the mouse, the lever, or **Enter/Space**.
- **F12** toggles a debug overlay (state, reels, attempts, live jackpot
  probability, FPS).
- **Force Jackpot** / **Force Near-Miss** buttons appear only when
  `config.json`'s `debugMode` is `true`.
- **Simulate Logon** resizes the window to roughly the size the
  Credential Provider's modal automaton uses.

## Building

Requirements: **Visual Studio 2022**, a recent **Windows SDK**, **x64**
(ARM64 is untested but the CMake build doesn't special-case it away).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

This builds, under `if(WIN32)`:

- `WaffleCore` — the game logic (`src/Core/`), no Win32 dependency.
- `WaffleRender` — Direct2D/DirectWrite rendering + XAudio2 audio
  (`src/Render/`).
- `WaffleJackpotDemo.exe` (`src/Demo/`).
- `WaffleJackpotProvider.dll` — the Credential Provider
  (`src/CredentialProvider/`).
- `WaffleJackpotControl.exe` — install-time management GUI/CLI
  (`src/Control/`).

`WaffleCore` and its unit tests (`WaffleCoreTests`, GoogleTest, fetched via
CMake `FetchContent`) have **no** Win32 dependency and build/run on Linux
too — that's what this repository's own CI-equivalent checks (see
[Verification status](#verification-status)):

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build
```

## Installing, disabling, uninstalling

**Read [Testing on a VM first](#testing-on-a-vm-first-please) below before
running any of this on a machine you actually use.**

```powershell
# As Administrator, after building:
installer\install.ps1              # prompts for confirmation; -Force to skip
installer\disable.ps1              # turn it off without uninstalling
installer\enable.ps1               # turn it back on
installer\uninstall.ps1            # remove everything

# Or the equivalent from WaffleJackpotControl.exe (GUI if run with no
# arguments, CLI otherwise — elevates itself via a UAC prompt only for
# the commands that actually change something):
WaffleJackpotControl.exe status
WaffleJackpotControl.exe enable
WaffleJackpotControl.exe disable
WaffleJackpotControl.exe uninstall
WaffleJackpotControl.exe demo
```

`install.ps1` copies the provider DLL to `%ProgramFiles%\WaffleJackpot\`
(ACL-locked to Administrators/SYSTEM — it loads into LogonUI as SYSTEM),
registers it as a COM server and as a Credential Provider, and deploys
`config.json` to `%ProgramData%\WaffleJackpot\`. It never touches Windows'
own built-in credential providers.

**If you get locked out:** see [`recovery/RECOVERY.md`](recovery/RECOVERY.md).
There's also an automatic kill switch — three failed
initializations/crashes in a row and the tile stops appearing on its own
until someone runs `WaffleJackpotControl.exe enable`.

## Testing on a VM first, please

This installs a Credential Provider into your **logon screen**. A bug
here can mean a Windows install that's hard to log into. Before running
`install.ps1` anywhere else:

1. Create a VM (Hyper-V, VMware, or VirtualBox all work) running Windows
   10 1809+ or Windows 11.
2. **Take a snapshot before installing anything.**
3. Test the full flow from [`tests/MANUAL_TEST_PLAN.md`](tests/MANUAL_TEST_PLAN.md)
   on that snapshot.
4. Only then consider a real machine — and even then, keep the recovery
   options in mind (Safe Mode, WinRE, your BitLocker recovery key if this
   machine uses one).

## What this does and doesn't do with your password

- The password lives only in the tile's password-field buffer. It's
  copied exactly once, into the serialization buffer Windows itself
  consumes (`GetSerialization`), packed the same way Microsoft's own
  sample credential providers do it.
- It is `SecureZeroMemory`'d and freed the moment it's no longer needed —
  on tile deselection, after a failed logon, and in the credential
  object's destructor.
- It never appears in logs, the debug overlay, telemetry, crash dumps, or
  `OutputDebugString`.
- There is **no** custom password check, **no** local user database, and
  **no** network access anywhere in this codebase (checking the import
  table for `winhttp`/`wininet`/`ws2_32` is a good way to verify that
  claim yourself).
- Windows — specifically LSA — is the only thing that ever actually
  verifies a password here.

## Limitations

- **PIN, Windows Hello (face/fingerprint), and FIDO keys** are handled by
  system credential providers and can't be proxied through this tile.
  That's not a bug, it's how Credential Provider works: if your account
  only has a PIN set up, you'll sign in through the standard tile
  instead.
- If **"Require Windows Hello sign-in for Microsoft accounts"** is
  enabled, Windows hides password-based providers (including this one)
  entirely.
- Both **local accounts** and **Microsoft accounts with a password**
  should work; passwordless MSA configurations fall under the Windows
  Hello point above.
- **Remote Desktop** sessions get the normal Windows logon by default —
  this tile opts out of RDP unless `config.json`'s `showInRemoteSessions`
  is explicitly set.

### Coward Mode, and what this does not hide

The standard credential providers stay enabled — always. That means the
waffles are technically skippable: click **"Sign-in options"** on the
logon screen and use your normal password/PIN tile. This isn't a bug
we're hiding; it's the point. The tile itself says so, in small print:
*"Not feeling lucky? Sign-in options → Coward Mode."* There is no
`ICredentialProviderFilter` hiding system providers in the default build,
and there won't be one added to it.

## Risks of an experimental Credential Provider

Credential Provider DLLs load directly into `LogonUI.exe`, which runs as
`SYSTEM`, before anyone has authenticated. A bug here is not like a bug in
a normal desktop app:

- Every public COM method here is wrapped so no C++ exception can cross
  the COM boundary — every path returns an `HRESULT`.
- A failed asset/sound/config load degrades silently (no sound, a
  procedural fallback bitmap) rather than crashing.
- A fatal initialization error makes `GetCredentialCount` return 0 tiles —
  the provider quietly declines to participate rather than taking LogonUI
  down with it.
- The kill switch (above) exists specifically for the failure mode where
  the silent-degradation paths above didn't catch something.

None of this makes it *safe* to skip the VM-testing step. Treat this
provider the way you'd treat any code that runs as SYSTEM before you can
log in: with a healthy amount of suspicion, and a snapshot to roll back
to.

## Attribution

- **Credential Provider skeleton** (`src/CredentialProvider/helpers.{h,cpp}`,
  `ClassFactory.{h,cpp}`, `dllmain.{h,cpp}`): adapted from Microsoft's
  official [Windows-classic-samples](https://github.com/microsoft/Windows-classic-samples)
  repository (`Samples/Win7Samples/security/credentialproviders/helpers/`),
  MIT-licensed, Copyright (c) Microsoft Corporation. `FieldDescriptors.h`
  is modeled on the same repo's `samplecredentialprovider/common.h` field
  table shape; the values in it are this project's own. Every adapted
  file keeps the original license/attribution note in its header. I was
  not able to find a published Microsoft sample using the V2
  `ICredentialProviderSetUserArray` model this project's provider actually
  uses (everything in that repository is the older, pre-Windows-8 V1
  shape) — `JackpotProvider`/`JackpotCredential`'s V2-specific parts are
  written from the documented interface contracts on Microsoft Learn, not
  copied from a sample. See the Phase 4 commit for what was checked.
- **JSON parsing**: [nlohmann/json](https://github.com/nlohmann/json)
  (MIT license), vendored as a single header in `third_party/nlohmann/`.
- **Unit tests**: [GoogleTest](https://github.com/google/googletest)
  (BSD-3-Clause), fetched at configure time via CMake `FetchContent` — not
  vendored, since it's dev-only and never ships in a built binary.
- **Every visual and audio asset** (the waffle/cherry/lemon/etc. symbols,
  the tile image, every sound effect) is generated **in code**, at
  runtime — Direct2D primitives for the reel symbols and Credential
  Provider tile image, synthesized PCM waveforms (sine/sweep/noise with
  envelopes) for every sound. Nothing is downloaded, nothing is loaded
  from a file, nothing is embedded as a binary resource. There is
  therefore no third-party asset license to track here beyond the code
  itself.

## FAQ

**Can I win with sevens?** No.

**Is this a real security feature?** No. Waffles are not a security
boundary. Windows' own authentication is what actually protects your
account; this only gates *when the password field shows up*.

**Can I disable it if I get tired of it?** Yes —
`WaffleJackpotControl.exe disable`, or the "Coward Mode" link on the tile
itself for that one login.

**What if it breaks my logon screen?** See
[`recovery/RECOVERY.md`](recovery/RECOVERY.md). The kill switch should
mean this doesn't happen twice.

## Verification status

This project was built end-to-end without access to a Windows machine or
a Windows VM. Being honest about exactly what that means:

| Piece | Verified how |
|---|---|
| `WaffleCore` (game logic, `src/Core/`) | ✅ Builds and its 44 GoogleTest cases pass on Linux — genuinely verified, repeatedly, throughout development. |
| `WaffleRender`, Demo, Credential Provider, Control, kill switch | ❌ Win32/Direct2D/XAudio2/COM code. Written against documented APIs, cross-checked against Microsoft's own sample where one exists (see Attribution), but never compiled or run — this repository's build environment has no Windows SDK. |
| `installer/*.ps1` | ❌ Written for PowerShell 5.1+; never run (no `pwsh` available in the build environment either). |
| `recovery/RECOVERY.md`, `recovery/offline-disable.cmd` | ❌ Neither the broken-logon-screen scenario, Safe Mode, nor WinRE steps have been tested — see that document's own verification-status note. |
| `tests/MANUAL_TEST_PLAN.md` | ❌ A checklist to run on a VM; none of its rows have been executed. |

If you're reading this after building and testing on a real VM: please
update this table (and the one in `RECOVERY.md`) with what you actually
verified, and open a PR. That's the whole point of writing it down.
