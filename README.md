# Ishimura Stability Patch

![Version](https://img.shields.io/badge/version-1.0.3-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![Game](https://img.shields.io/badge/game-Dead%20Space%20(2008)-c41e1f)

**An original, from-scratch compatibility and bug-fix mod for Dead Space (2008) on PC.**
It makes the 2008 game run correctly on modern hardware: fixes startup crashes on
high-core-count CPUs, keeps physics and audio stable with a safe 60 FPS default,
adds proper borderless windowed mode, corrects VSync, scales subtitles for high
resolutions, and enables anisotropic filtering where the hardware supports it.

## Is this a copy of DeadSpace2008Fixes?

No. Ishimura Stability Patch is an independent implementation — it contains no
source code, binaries, libraries, or other assets from DeadSpace2008Fixes. That
project was consulted as a research reference and is credited in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Every overlapping feature is
implemented differently:

| Feature | DeadSpace2008Fixes | Ishimura Stability Patch |
|---|---|---|
| CPU crash fix | Hardcodes affinity mask `0xFF` | Reads the process's allowed mask and selects permitted processors, respecting launcher/VM/user restrictions |
| VSync | Byte-patches two engine code sites | Rewrites the presentation interval at the D3D9 API boundary |
| Subtitle scaling | Assembly hook mutating stored subtitle fields from desktop metrics | Computes scale per call from render height; never touches stored fields |
| Borderless window | Intercepts window creation, forces primary monitor | Fixes up the confirmed game window on its nearest monitor, verifies geometry, repairs later changes |
| High-FPS timing | Enables the hidden global hi-res timer flag | Conservative 60 FPS safety cap; higher rates need an explicit unsafe opt-in |
| Anisotropic filtering | Forces AF broadly, including special-purpose samplers | Upgrades only compatible samplers, checks hardware caps first |

The full feature-by-feature breakdown is documented in
[DEADSPACE2008FIXES_COMPARISON.md](DEADSPACE2008FIXES_COMPARISON.md), and the
complete source is in [`src/`](src/).

## What it fixes

- **Modern CPU startup compatibility:** constrains the game to eight allowed
  logical processors before its unsafe legacy CPU enumeration runs. It respects
  the process's existing allowed mask instead of assuming CPUs 0-7 exist.
- **Physics and audio timing safety:** caps presentation to 60 FPS by default.
  This prevents the high-FPS timestep from producing drifting/floating bodies,
  unstable ragdolls, skipped script windows, and dropped queued sound events.
- **VSync:** converts the game's half-refresh D3D9 interval to one refresh,
  while respecting the player's choice to disable VSync.
- **Borderless windowed:** applies borderless geometry only when D3D confirms
  that the game is windowed, uses the monitor containing the game window, and
  synchronizes the engine render size and D3D backbuffer to that monitor before
  device creation. It sets system DPI awareness before the game creates a window
  and resolves the physical monitor pixel size, preventing scaled Windows
  coordinates from squashing the startup image. It periodically verifies/repairs
  the exact client size and borderless styles. It never converts exclusive
  fullscreen behind the player's back.
- **Texture filtering:** upgrades linear min/mag filters to supported
  anisotropic filtering and point mip filtering to trilinear. It leaves
  deliberately point-sampled textures alone.
- **Subtitle scaling:** reads the game's actual render height at subtitle-layout
  time and applies the same absolute scale to layout and final drawing. It does
  not permanently enlarge subtitle positions or bounding boxes, avoiding the
  cumulative/off-screen behavior seen in the reference mod at high resolutions.
- **Controller-safe XInput proxy:** forwards all named XInput 1.3 functions and
  private ordinals 100-103 to Windows. It does not replace controller input,
  start SDL, or patch mouse behavior.

## Deliberately not included

This project does **not** enable the game's hidden global high-resolution timer
flag. That broad clock change is not a complete Havok fix and can change timing
for physics, scripts, streaming, and sound. It also does not globally hook
Winsock, NetBIOS, DirectInput, or Windows window-creation functions.

Native PlayStation/Switch translation, telemetry blocking, legacy-HID removal,
save-string replacement, and cutscene/loading skips are intentionally not in
this core bug-fix release. They are separate features, and adding them to the
timing-critical core would make controller, audio, or compatibility regressions
harder to isolate. A candidate save-string replacement was rejected during
real EA-build testing because it caused a reproducible access violation.

## Test status

Version 1.0.3 has passed the automated proxy, subtitle, borderless, and native
Direct3D9 device-recreation tests. Its new startup display fix passed two
normal EA App launch/exit cycles at 3840x2160 with 175% Windows scaling.
Version 1.0.2 previously launched through Steam and EA App with the 4GB mod
present, including a short Steam gameplay check. The new 1.0.3 change has not
been retested on Steam or every hardware configuration.

## Install

1. Remove or rename the existing `xinput1_3.dll`, `SDL3.dll`, and
   `DeadSpaceFixes.ini` from the game folder. Do not run both mods together.
2. Copy this archive's files beside `Dead Space.exe`.
3. In Dead Space, set display mode to **Windowed** if you want borderless mode.
4. Launch normally through EA App, Steam, GOG, or Vortex.
5. Check `IshimuraStabilityPatch.log` in the game folder.

To uninstall, remove this mod's `xinput1_3.dll`, INI, log, and documentation.
Restore any previous proxy DLL only if you deliberately backed it up.

## Recommended settings

Keep these values for stable physics and sound:

```ini
[Timing]
PhysicsAndAudioSafety=1
MaxFPS=60
AllowUnsafeHighFPS=0
```

Setting `AllowUnsafeHighFPS=1` makes high-FPS physics/audio bugs possible again
and is not considered a supported configuration.

## Compatibility

- Built as a 32-bit DLL for the 32-bit game.
- Targets Windows 7 SP1 or later and uses the static MSVC runtime.
- Designed to coexist with D3D9 wrappers by changing the game device's current
  COM vtable instead of detouring D3D9 machine code.
- Do not combine it with another `xinput1_3.dll` proxy or another subtitle hook.
- ReShade was verified on the installed EA build at a native 3840x2160
  backbuffer and 60 FPS. Other ReShade versions and DXVK remain unverified.

## Display-mode and Auto HDR notes

Dead Space has only an in-game **Full Screen** on/off toggle. For this release,
set it **off** and keep `BorderlessWindowed=1` in the mod INI. The result is a
real windowed D3D9 device whose frame and client area exactly cover the monitor;
the successful EA/ReShade test synchronized the engine, backbuffer, client, and
monitor to 3840x2160 and measured 60 FPS.

A separate three-way Fullscreen / Windowed / Borderless entry is not injected
into the original menu in v1.0.3. That menu is serialized in the game's
frontend asset and shares global On/Off controls with other settings, so a
label-only replacement would display the wrong state and is not a real option.

Native Windows 11 Auto HDR is not claimed. Dead Space and the preserved ReShade
chain are Direct3D 9; Microsoft's current Auto HDR and windowed-game paths
document DirectX 10/11 or DirectX 11/12 eligibility, not native D3D9. Being a
genuine borderless window does not convert the renderer. ReShade color/HDR
shaders and GPU-vendor HDR filters are separate from Windows Auto HDR.

## Source and build

Run `build.ps1 -Package` in a PowerShell prompt with Visual Studio C++ Build
Tools installed. It builds Release Win32, verifies the XInput exports and
forwarding path, and creates a versioned ZIP in the workspace `outputs` folder.

No game file or third-party binary is distributed.

## Credits and licensing

DeadSpace2008Fixes by Seamus McGrath was consulted during research into known
Dead Space (2008) compatibility issues:

https://github.com/seamusduncmcgrath/DeadSpace2008Fixes

Ishimura Stability Patch is a separate implementation and does not contain
source code, binaries, libraries, or other assets from DeadSpace2008Fixes.

MIT licensed — see [LICENSE](LICENSE).
