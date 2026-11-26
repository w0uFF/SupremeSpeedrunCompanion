# Supreme Speedrun Companion

An optional, community-driven speedrunning companion for Supreme Snowboarding (Boarder Zone).

**Status: P0 discovery — not an installable mod.** Current revision v0.0.6-p0 adds native registry/result ownership research and paired baseline file diagnostics to the verified v0.0.5 baseline. Native loader compatibility (G1), race timing, recording and menu integration have not been validated. Do not copy development files into your game directory.

## Project goals

- Native Speedrun menu with nine Time Attack categories, All Tracks, and Championship.
- Remembered character and snowboard selection, weather choice, automatic race sequencing, and training mode.
- Automatic/manual recording, independent real-time and native in-game timing, input overlays, replays, and run management.
- Local submission packages and optional, player-confirmed Speedrun.com API submission.
- Preserve original gameplay, Display Config compatibility, and the keyboard persistence patch.

## Build and test

C++17, CMake 3.16+, C99 and Python 3 (required for full tests). Windows Visual Studio Build Tools:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

Portable Linux: configure without -A Win32, then build and run CTest. Nine baseline CTest targets are expected. The offline supreme_diagnostic program reports unavailable engine data by design. Run outside the game directory.

Read-only reference classifier: `python tools/preflight.py <copied-game-root>`. Exact on-disk matches are not runtime support or competition eligibility. For local research, `python tools/native_xrefs.py <original.exe> --compare <patched.exe>` requires GNU objdump; `python tools/replay_samples.py <replay.dat>` reports candidate coordinates and header timing without an official verdict. Use only trusted local reference copies. See docs/engine/ENGINE_MAP.md.

## Development and distribution

GitHub-hosted Windows x86 CI builds and tests Debug and Release. No Actions artifact storage. Automated development pre-releases are gated by a maintainer-approved LICENSE and SSC_RELEASE_APPROVED repository variable. Release assets are offline diagnostics, not installable game mods. Native integration remains blocked by G1. See PROJECT_STATUS.md and docs/release/CI.md.

The original game, launcher binaries, private recordings, credentials, and raw reference research are not redistributed. The project license is pending maintainer selection; no redistribution or upstream license permissions are implied. Launcher auto-update integration is proposed, not agreed with upstream.

Optional paired diagnostic for a closed disposable game copy: `python tools/baseline_snapshot.py snapshot <game-root>` and `python tools/baseline_snapshot.py compare <before.json> <after.json>`. See docs/testing/WINDOWS_BASELINE.md. This never passes the loader gate.
