# DeadSpace2008Fixes feature comparison

This is a code-level comparison against the `main` branch snapshot reviewed on
2026-08-24. Ishimura Stability Patch is an original implementation: it does not
distribute DeadSpace2008Fixes or any of its binaries. The reference project and
other research sources are credited in `THIRD_PARTY_NOTICES.md`.

Status words used below:

- **Implemented:** included and covered by an automated or installed-game test.

- **Safer alternative:** the goal is addressed with a narrower mechanism.
- **Not included:** deliberately excluded from the bug-fix core.
- **Blocked:** a candidate was unsafe and must be redesigned before release.

## Every advertised feature

| DeadSpace2008Fixes goal | How its implementation works | Ishimura Stability Patch | Why this differs |
|---|---|---|---|
| Borderless Windowed | Hooks general Win32 window creation/rectangle functions and forces primary-screen coordinates and dimensions. It is essentially a startup rewrite. | **Implemented.** On the supported EA build it first synchronizes the engine's render globals, then creates a monitor-native D3D9 backbuffer. It strips the confirmed game window's frame, uses that window's nearest monitor (including negative monitor coordinates), verifies outer/client rectangles equal the monitor, and repairs later rewrites every 250 ms while allowing minimize. If engine-dimension validation is unavailable, it refuses to force an unsafe backbuffer. | Avoids affecting unrelated windows, avoids forcing the primary monitor, does not pretend exclusive fullscreen is borderless, and checks the engine size, backbuffer, and window rather than only changing the frame. |
| Reduced Issues At High FPS | Enables the engine's hidden QueryPerformanceCounter timing branch globally and recommends a separate 120-180 FPS cap. | **Safer alternative.** Does not flip the global timer branch. Default frame pacing limits the game to 60 FPS, where its physics, animation, script, and queued-audio assumptions are safest. Higher FPS requires an explicit unsafe opt-in. | A more precise clock is not a fixed timestep. It cannot by itself guarantee stable Havok bodies, scripts, or audio at high frame rates. The conservative cap directly avoids the known bad range and targets the reported floating/sliding corpse and missing-sound regressions. |
| Anisotropic Filtering | Intercepts sampler state and forces anisotropic filtering/max 16 broadly, including mip filter calls. | **Implemented.** Upgrades only compatible linear min/mag filtering, improves point mip filtering to linear, clamps to hardware capability and the configured maximum, and leaves deliberately point-sampled UI/depth paths alone. | Reduces the chance of corrupting special-purpose samplers or submitting unsupported combinations while still improving world texture clarity. |
| Fixed crashes on 10+ core CPUs | If the system reports more than eight processors, sets the process affinity to hard-coded mask `0xFF`. | **Implemented.** Runs before the game's unsafe enumeration, selects up to eight logical processors from the process's already-allowed affinity mask, and applies only to the exact `Dead Space.exe` process. | Works when CPUs 0-7 are not all available due to launcher, VM, job, or user affinity policy; does not broaden an existing restriction. |
| Significantly Faster Startup Times | Hooks DirectInput8 enumeration so the game sees only pointer/keyboard devices and patches its decision to use DirectInput. | **Not included.** The game retains its native device enumeration. | A few seconds of boot time is not worth silently dropping legacy HID devices or risking controller/input regressions in the stability core. This mod also avoids the old mouse-fix/input-patching path. |
| High Resolution Subtitle Fix | Scales subtitle position, bounding box, and font settings from primary-screen dimensions, mutating the stored subtitle data. Aspect correction can enlarge the box beyond the normalized viewport. | **Implemented.** Reads the game's actual render height at each subtitle pre-layout call, calculates a fresh absolute scale, and uses that same value for wrapping/layout, line spacing, and final glyph draw. It never rewrites stored x/y position or bounding-box fields. Automated tests cover 480 through 10,000 pixels, repeated 720p/4K changes, and the 8x safety clamp. | Specifically prevents cumulative growth and the off-screen/runaway box introduced by mutating persistent subtitle settings. Using render height instead of the primary desktop also handles windowed and non-primary-monitor rendering correctly. A live longest-line screenshot matrix remains a release gate. |
| Safer Save String Handling | Replaces a game routine with a custom wide-string copy/clear hook. | **Blocked; not shipped.** A candidate based on the published signature produced a reproducible access violation in the installed EA build and was removed. | Shipping a crash-prone overwrite would be worse than leaving the vanilla bug. This requires independent calling-convention, buffer-size, and per-build reverse engineering before it can be claimed fixed. |
| Removed Telemetry | Broadly hooks `WSAStartup` and NetBIOS to make networking unavailable. | **Not included.** | Global network API failure can affect launchers, overlays, wrappers, and unrelated in-process components. Telemetry/privacy filtering should be a separately testable, destination-specific module rather than part of physics/audio stability. |
| Native PS4/5 and Switch Controller Support | Loads SDL3 and translates supported controllers through the XInput proxy. | **Not included.** The proxy forwards all public XInput 1.3 exports plus private ordinals 100-103 to the real Windows DLL and does not translate input. | Keeps Xbox/controller-wrapper behavior native and avoids an extra input stack. Native PlayStation/Switch support is a separate feature, not a prerequisite for the bug fixes. |
| Fixed VSync | Changes the engine's half-rate interval selections, described as removing the 30 FPS cap while retaining a half-refresh caveat. | **Implemented.** Intercepts the actual D3D9 presentation parameters and converts only intervals TWO/THREE/FOUR to interval ONE. It respects IMMEDIATE when in-game VSync is off. The separate default 60 FPS safety cap remains active. | Produces full-refresh VSync without forcing VSync on and keeps physics/audio safety independent of monitor refresh. |
| Optional 60 FPS Cap | Hooks D3D EndScene when enabled; the source default is off even though the README example says on. | **Implemented, default on.** A 60 FPS maximum is part of the default safe profile. Disabling safety above 60 requires `AllowUnsafeHighFPS=1`. | Stable physics and sound are the main purpose of this mod, so the protective setting should not silently default off. |
| Optional Skip Ishimura Landing Cutscene | Patches story flow for speedrunning. | **Not included.** | This is a gameplay/speedrun convenience, not a bug fix. Keeping it out reduces script-flow test scope. |
| Optional Skip Intro To Main Menu | Patches boot flow to jump directly to the menu. | **Not included.** | This is a convenience feature and can hide startup/streaming behavior that release testing needs to observe. |
| Optional Skip Loading Screen Delay | Changes the post-load tip-cycle hold. | **Not included.** | This changes presentation timing rather than correcting a stability defect and could mask streaming readiness problems. |

## Incidental behaviors in the reference project

DeadSpace2008Fixes also replaces the menu version string with an installation
message and generates/migrates its INI. Those are packaging/diagnostic features,
not game fixes. Ishimura Stability Patch leaves the menu UI untouched, writes a
plain local diagnostic log, includes a normal Windows version resource, and
ships an explicit commented INI.

## Historical verification notes (v0.1.1 Beta)

- The 32-bit proxy builds with a static runtime and forwards its public and
  private XInput exports to the system DLL.
- Automated tests verify absolute subtitle scales at common/high resolutions,
  non-compounding resolution changes, exact monitor-sized borderless geometry,
  and repair after a simulated game style/size rewrite.
- The installed EA executable reaches the 3840x2160 menu, the CPU restriction
  applies, the subtitle pattern installs, the engine render size and D3D9
  backbuffer both synchronize to 3840x2160, the borderless client/monitor match,
  and frame pacing measures 60.00 FPS with the user's ReShade chain active.
- Real dialogue containment at every resolution, a full corpse/prop matrix,
  streamed audio across a playthrough, multiple storefronts/controllers, and
  long-duration borderless behavior remain Beta release gates in `TESTING.md`.

The final distinction matters: a passing synthetic test proves the code's
invariants, but it does not replace a complete game playthrough.<br><br><em>Retained for history — this section describes what was verified during the old 0.1.1 beta. For the current release, see "What is verified in v1.0.3" below.</em><br><br><strong>What is verified in v1.0.3</strong><br><br><ul><li>The 32-bit proxy builds with a static runtime and forwards its public and private XInput exports to the system DLL.</li><li>Automated tests cover the proxy, subtitle scaling, borderless geometry, and native Direct3D9 device recreation.</li><li>The entire game was played through with v1.0.3 installed. Previous versions were also played through in full, and every release has been manually tested.</li><li>The new startup display fix passed two normal EA App launch/exit cycles at 3840x2160 with 175% Windows scaling.</li><li>Version 1.0.2 previously launched through Steam and EA App with the 4GB mod present, including a short Steam gameplay check. The 1.0.3 changes have also been tested working on the Steam release (no full Steam playthrough); every hardware configuration has not been covered.</li></ul>
