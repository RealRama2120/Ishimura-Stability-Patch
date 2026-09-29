# Beta test checklist

Use a clean game install for the first pass. Keep
`IshimuraStabilityPatch.log` from every failed run.

## Startup and compatibility

- Test EA App, Steam, and GOG separately when available.
- Test a CPU with more than 10 logical processors without Process Lasso,
  msconfig limits, or a special launcher.
- Confirm the main menu loads five consecutive times.
- Test with no controller, an Xbox controller, and Steam Input/DS4Windows.

## Physics acceptance test

Keep the default 60 FPS safety settings.

1. Kill two enemies in a room with loose props.
2. Walk into, stomp, melee, and kinesis-drop each corpse and prop.
3. Leave them untouched for 60 seconds.
4. Confirm there is no continuous one-direction slide, hovering, escalating
   jitter, or launch caused by a light player bump.
5. Repeat in an elevator, zero-G room, and after loading a save.

Record the same scene without the mod at uncapped/high refresh for comparison.

## Audio acceptance test

- Listen for door, weapon, stomp, enemy, locator, kinesis, and UI sounds.
- Play every audio/video log found in two chapters.
- Confirm dialogue is present during the opening, tram calls, and a chapter
  transition.
- Pause/unpause, alt-tab, and change rooms during a playing log.
- Test stereo and the user's real surround layout. A missing center channel due
  to Windows speaker configuration is separate from a timing/streaming bug.

## Display and graphics

- With in-game VSync on, confirm full-refresh presentation and a 60 FPS safety
  limit rather than the vanilla 30 FPS half-refresh cap.
- With in-game VSync off, confirm the mod still limits to 60 FPS.
- Set **Full Screen** off, restart the game, and confirm the log says
  `Synchronized engine render size`, a monitor-native `D3D9 device observed`
  resolution with `windowed=1`, and `Verified borderless window`.
- Confirm there is no title bar, frame, uncovered desktop strip, or one-pixel
  edge on all four sides. The client area must equal the monitor bounds.
- Confirm the log's engine size, D3D9 backbuffer, borderless client, and monitor
  dimensions are identical. A monitor-sized client with a shorter backbuffer
  is a stretched window and fails this test.
- Alt-tab ten times, minimize/restore, open and close the graphics menu, and
  leave the game running for ten minutes. Confirm the border does not return.
- Test a secondary monitor, including one positioned left of or above the
  primary monitor. Confirm the window stays on that monitor and is not forced
  to coordinate 0,0.
- Switch back to in-game Full Screen and restart. Confirm the log says
  `windowed=0` and no borderless activation is logged.
- Inspect UI, holograms, shadows, video playback, and world textures with
  anisotropic filtering on and off.

## Subtitle regression checks

- Enable subtitles before starting the test scene.
- Test 1280x720, 1920x1080, 2560x1440, 3440x1440, 3840x2160,
  5120x1440, and 7680x4320 where the monitor/virtual display supports them.
- Use short, two-line, and the longest available dialogue subtitles. Confirm
  every glyph remains inside the visible screen with comfortable left, right,
  and bottom margins; no line may run past an edge or be cut off.
- Confirm line wrapping and line spacing match the rendered glyph size and no
  line overlaps another.
- Change 720p -> 4K -> 720p in one session and repeat it several times. Text
  must return to the original size; it must never grow cumulatively.
- Repeat after alt-tab and after loading a save. Confirm the log reports the
  current render height and the expected fresh scale (720p=1, 1440p=2,
  2160p=3, 4320p=6 with the default 720 base).
- Capture screenshots with the full frame visible for each release-gate test.

Windows Auto HDR is not a release gate for the native D3D9 build. Do not treat
a ReShade HDR shader or a GPU-driver HDR overlay as proof of Windows Auto HDR.

## Release gate

Do not call the mod final until the physics and audio tests pass on at least
EA App and Steam/GOG, with ReShade or DXVK, an Xbox controller, and a complete
playthrough. Until then, publish it as Beta and describe exactly what was tested.
