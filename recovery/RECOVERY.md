# Recovery: what to do if Waffle Jackpot Login gets in your way

> ⚠️ **Verification status: NONE of the steps below have been tested on a
> real VM.** This project has been developed without access to a Windows
> machine or a Windows VM to actually boot and test against — every phase
> so far has been built and (where the toolchain allows) built/tested on
> Linux, with the Windows-only pieces (Direct2D, the Credential Provider,
> this recovery procedure) written against documented APIs and never run.
> **Before you rely on this document on a machine you care about, test
> every step below on a disposable VM with a snapshot taken before you
> install anything.** If a step doesn't work the way this document
> describes, that's expected until someone verifies it on real hardware —
> please open an issue or fix this file once you have.

If you installed Waffle Jackpot Login and now can't get past it, work
through these in order. Each one is less convenient than the last, so try
them top to bottom.

## 0. Before you even install: know your BitLocker recovery key

If this machine uses BitLocker, **locate your BitLocker recovery key
before installing anything**. None of the steps below touch BitLocker, but
if something else goes wrong and you end up needing WinRE (step 3) on a
BitLocker-protected drive, WinRE may ask for that key before it lets you
access the drive at all. Find it now, while you can still log in normally:

- `Settings -> Privacy & security -> Device encryption` (or `Manage
  BitLocker` from Control Panel), or
- your Microsoft account's BitLocker recovery keys page, or
- wherever your organization's IT department stores them, if this is a
  managed machine.

Write it down somewhere that isn't this computer.

## 1. On the logon screen: use the standard tile (Coward Mode)

Waffle Jackpot Login never removes or hides the built-in Windows
credential providers (project spec §6.5 — this is a deliberate design
constraint, not an oversight). On the logon screen:

1. Click **"Sign-in options"** (usually below or near the tile list).
2. Pick your normal password/PIN/Windows Hello tile.
3. Log in as usual — Waffle Jackpot Login is not involved at all.

This works even if the Waffle tile is completely broken. If this gets you
in, you don't need anything below — just run `WaffleJackpotControl.exe
disable` or `installer\disable.ps1` once you're logged in, at your
convenience.

## 2. If "Sign-in options" itself is missing or LogonUI looks broken: Safe Mode

If the Waffle tile has actually broken LogonUI badly enough that you can't
even get to "Sign-in options":

1. Force a reboot 2–3 times during the Windows boot sequence (hold the
   power button) — Windows should detect this and offer **Automatic
   Repair**, which leads to **Advanced options**.
   - Alternatively, if you can reach the login screen at all: hold Shift
     while clicking **Power -> Restart**, which boots straight into the
     recovery environment.
2. Go to **Troubleshoot -> Advanced options -> Startup Settings ->
   Restart**, then press **4** (or **F4**) for **Safe Mode**, or **5**
   (**F5**) for **Safe Mode with Networking**.
3. Safe Mode loads a minimal set of drivers and services; credential
   providers still load, but this is a good place to open an elevated
   Command Prompt or PowerShell and run:

   ```powershell
   WaffleJackpotControl.exe disable
   ```

   or, if you have the repository:

   ```powershell
   installer\disable.ps1
   ```

4. Reboot normally.

## 3. If Safe Mode doesn't help either: WinRE offline registry edit

This is the last resort short of reinstalling Windows, and it's the one
step that doesn't require Windows to boot at all.

1. Boot into **Windows Recovery Environment (WinRE)**:
   - From a Windows installation/repair USB drive, choose **Repair your
     computer**, or
   - from the machine itself, force 2–3 failed boots as in step 2 above,
     then **Troubleshoot -> Advanced options -> Command Prompt**.
2. In the Command Prompt, run
   [`offline-disable.cmd`](offline-disable.cmd) from this `recovery/`
   folder (copy it to a USB drive beforehand if you don't have this repo
   on hand, or type its `reg load` / `reg delete` / `reg unload` commands
   manually — they're short).
3. The script will ask you to find your real Windows drive letter with
   `diskpart`, since **WinRE almost never uses `C:` for your actual
   Windows installation** — it's commonly `D:`, `E:`, or `X:` instead.
   Follow its prompts.
4. It loads the offline `SOFTWARE` hive, removes just the credential
   provider's registration key (the same thing `disable.ps1` does, but
   without Windows running), and unloads the hive again.
5. Reboot normally.

If you'd rather not run the script, the three commands it wraps are:

```cmd
reg load HKLM\WAFFLE_OFFLINE D:\Windows\System32\config\SOFTWARE
reg delete "HKLM\WAFFLE_OFFLINE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}" /f
reg unload HKLM\WAFFLE_OFFLINE
```

(replace `D:` with your actual Windows drive letter).

## 4. The kill switch should mean you never actually need any of this

Waffle Jackpot Login tracks its own failed initializations in
`HKLM\SOFTWARE\WaffleJackpot\CrashCount` (project spec §9.2). After **3**
in a row, it stops showing a tile on its own — no user action required —
until someone runs `WaffleJackpotControl.exe enable`. A single crash or
two shouldn't lock you out; this document is for the cases the kill switch
doesn't catch (a hang rather than a crash, or a first-time crash on your
very next logon before the kill switch has tripped).

## Still stuck?

If none of the above gets you back into Windows, the credential provider
DLL can also simply be deleted while offline (from WinRE's Command
Prompt, or by mounting the drive on another machine):

```
%ProgramFiles%\WaffleJackpot\WaffleJackpotProvider.dll
```

Without the DLL, Windows can't load the provider at all, regardless of
what the registry says — this is a strictly bigger hammer than
`offline-disable.cmd` (it also breaks re-enabling until you reinstall),
but it will get you booting.
