# Supreme Speedrun Companion

An optional, community-driven speedrunning companion for Supreme Snowboarding (Boarder Zone).

**Status: P0 discovery — not an installable mod.** The current offline development baseline is `v0.0.4-p0`; the source import is pending. Native loader compatibility (G1), race timing, recording, and in-game menu integration have not been validated on Windows. Do not copy development files into your game directory.

## Project goals

- Native Speedrun menu with nine Time Attack categories, All Tracks, and Championship.
- Remembered character and snowboard selection, weather choice, automatic race sequencing, and training mode.
- Automatic/manual recording, independent real-time and native in-game timing, input overlays, replays, and run management.
- Local submission packages and optional, player-confirmed Speedrun.com API submission.
- Preserve the original gameplay, Display Config compatibility, and existing keyboard persistence patch.

## Development and distribution

C++17, CMake, Windows x86 (Win32), portable offline tests, and modular MVC/ports-and-adapters architecture. GitHub-hosted Windows runners are planned for compilation and regression tests. Release packages will be development pre-releases until human validation and release approval. No Actions artifact storage is required.

The original game, launcher binaries, private recordings, credentials, and unreviewed reference material are not redistributed here. The project license is pending maintainer selection; no redistribution or upstream license permissions are implied. Launcher auto-update integration is proposed, not agreed with upstream.
