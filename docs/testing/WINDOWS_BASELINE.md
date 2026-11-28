# Windows baseline evidence — G1 prerequisite

No companion installation. Close game and launcher; make a separate user-writable disposable copy of the working game including Saved_Data. Preserve the primary installation. No administrator privileges or security-policy bypass.

1. In copied game root run PowerShell Get-FileHash .\Supreme.exe, .\Display_Config.exe, .\winmm.dll, .\Main_Menu.dll, .\Supreme_Game.dll, .\HMG_Kernel.dll -Algorithm SHA256. Optional: from source root run python tools/preflight.py "<copied game root>". Unknown identity is not cheating; all native capabilities remain disabled.
2. Launch via Display Config normally. Check main menu, audio, renderer; complete a race, restart and reset-to-track separately, and check result and audio.
3. Change Jump and Cant and another action, verify in race, exit normally. Note whether Saved_Data\Keyboard_Bindings.bin exists and is 36 bytes. Relaunch and verify persistence. If absent, note possible redirection; never move or overwrite data.
4. Optional read-only module listing: Get-Process Supreme | Select-Object -ExpandProperty Modules | Where-Object ModuleName -eq 'winmm.dll' | Select-Object ModuleName,FileName. If denied, mark unavailable, do not elevate. Redact personal paths.
5. Exit and compare hashes before/after. Record Windows version, renderer, mode, refresh rate, FPS cap, actual observations and errors. A baseline passes when both launches/exits, race result/restart/reset, expected audio and changed key persistence work. This does not pass G1; later separate reviewed native candidate needs installed-disabled smoke tests and rollback.
6. Optional: save a replay via the original UI if available, note actual displayed result, run python tools/replay_header.py on a copied replay and compare candidate time. No official timing conclusion without actual evidence.

Do not enable an injector, replace winmm.dll, edit game files, or install this repository into the game.

## P0-T15: paired evidence (optional Python, no companion installation)

Use the same closed, disposable working copy for both snapshots. Keep the report files outside the game. Do not run these commands against the only working installation. No administrator access, injected code, game updates or changes to loader/patch files are needed.

From the extracted repository root, with the game and launcher closed, use PowerShell:

```powershell
$Game = "C:\path\to\disposable-game"
py -3 tools/baseline_snapshot.py snapshot "$Game" | Out-File -Encoding utf8 before.json
```

If Python is unavailable, the existing manual Get-FileHash method above remains sufficient; do not install Python solely for this test.

1. Launch the disposable copy using the normal method; use the working renderer and refresh/FPS settings. Record the displayed selection of region, difficulty, rider, board and weather. Complete a Time Attack race. Note the exact displayed final time (including decimal digits) and whether it is the live race rather than replay. A screenshot or short recording of the selection and result is helpful but optional. Record whether the game's own restart and reset-to-track behave differently.
2. Repeat a race with one changed track/difficulty or weather, and note the actual loaded choice. Exit normally. If the normal built-in replay save is available, preserve a replay only through that UI and note whether playback corresponds to the run; otherwise mark unavailable. A non-PB need not change high-time files.
3. Execute the keyboard binding persistence and audio checks described above. Exit completely. Then, from the repository root:

```powershell
py -3 tools/baseline_snapshot.py snapshot "$Game" | Out-File -Encoding utf8 after.json
py -3 tools/baseline_snapshot.py compare before.json after.json | Out-File -Encoding utf8 comparison.json
Get-Content comparison.json
```

Expected fixed-file observation for the supported Display Config + keyboard patch combination: core_observation=unchanged, with some saved-data files possibly created or changed. A missing, redirected, unreadable or oversized file is a coverage gap, not a cheating or failure verdict. If core files change unexpectedly, stop and retain the evidence; do not restore or edit them automatically. The six-file comparison does not cover every DLL or runtime loaded module. Use preflight separately for full reference identity.

Return comparison.json, before/after fingerprints if comfortable (no raw save contents), Windows version, renderer, window mode, Hz/FPS cap, launch/exit/audio/binding observations, and the actual selection/result observations. Redact personal paths, usernames or other private details. No video upload required. The baseline passes only with actual observations; the tool always reports runtime_verified=false and loader_gate_passed=false. G1 additionally needs a reviewed native candidate installed-disabled, launch/exit, and rollback tests.

## T43 immediate human prerequisite (no SSC installation)
Return a redacted record for the copied game: Windows version; Display Config version; renderer, fullscreen/windowed, refresh/FPS cap; before/after six fixed-file SHA256 results (include HMG_Kernel.dll); launch and normal exit; menu audio and race audio; two distinct track/difficulty choices, exact displayed finish digits for each; reset-to-track versus genuine restart; key changes and post-relaunch persistence; any crash or unexpected file change. Use existing snapshot and compare CLI optionally, never send raw save files, credentials or private paths. A successful baseline permits review of actual loader identity and a separate installed-disabled/rollback test plan, not G1 PASS or installation authorization. If a check is unavailable mark BLOCKED/NOT_RUN with reason. No SSC binaries or replacement winmm.dll needed for this step.

## T46 next human prerequisite (same no-install baseline, not a new test)
The isolated loader/recorder CI has passed, but no actual installed loader identity or disposable baseline was received. Follow the T43 record above; include SHA256 of the actual winmm.dll and its known launcher distribution/version/source provenance (if unknown say unknown), optional loaded-module filename only (redact path). Use two distinct Time Attack tracks/difficulties and exact displayed finish digits, separate reset/restart, key persistence and audio observations. No SSC files, proxy replacement, administrator rights or new software needed. Stop on unexpected core changes/crashes. This evidence supports next G1 review, not G1 PASS. Actual installed-disabled and rollback tests will require a reviewed package and separate authorization.


## A-0024 — Owner configuration (partial baseline)
Six supplied pregame SHA256 independently match ZIP members: Supreme.exe f7bc9074e41244fb5a9a3ab53a4e433ac14564fb47206a5e3c91caf975531e02 (Keyboard Persistence0.1); Display_Config.exe 2f43d99d5f294d81c3b9e4da40bd8a26ccb6c7bafd99a747fae8e83b1363861b and winmm.dll 589fb68d98c4f71258f4f9dc1192d9d041ac7967c0f5d2e26340ee8634803dcb (Display Config1.8.0); Main_Menu.dll aed413babf03cb64596415f8fe9a1af043da3c2f6590e4dffe1dd37ac71098f9, Supreme_Game.dll b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b, HMG_Kernel.dll 814b7c5f32a17553c168cade2474e5ae9caec8b7fff4bd41090312ad097fdf15 (original). Owner launches Supreme.exe directly, believes Display Config starts automatically; prior keyboard saving across relaunch and gameplay successful (user report). No redundant unchanged keyboard test. No actual loaded-module proof/PR66 loader capability, no postgame hashes or new timing/audio observations. G1 unpassed, further gameplay deferred until relevant implementation. No SSC installation requested, no personal paths retained.
