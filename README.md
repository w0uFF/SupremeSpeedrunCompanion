# Supreme Speedrun Companion

**Spend more time racing, less time setting up speedruns.**

Supreme Speedrun Companion (SSC) is a free-to-use, optional Windows mod in development for Supreme Snowboarding (Boarder Zone). It is designed to make practicing, recording, reviewing and submitting speedruns easier, without changing the original game's racing mechanics. It is not a replacement for Online Time Attack or a mandatory anti-cheat.

> **Project status — 28 November 2026:** development prototype v0.0.40-p0 has passed its corrected isolated Windows tests (60/60 Debug and 60/60 Release). Final documentation validation and promotion are pending; verified main is v0.0.39-p0. **Not yet playable or installable.** Continue using your normal game and recording tools.

## Where are we now?

The technical foundations are becoming reliable. The next major challenge is connecting them safely to the actual game. A feature passing an isolated test is not the same as a feature you can use while racing.

| Feature | Progress today | What remains before you can use it |
|---|---|---|
| Speedrun menu and quick retries | Original menu and race transitions are being mapped | Add and test native controls without disrupting normal play |
| Automatic and manual recording | Recorder logic and isolated Windows video capture tested; incomplete recordings preserved in separate tests | Connect actual game window, start/finish events, codec and audio; verify long sessions |
| Original game time (IGT) and independent real time (RTA) | Original timing and result text traced through render submission; independent timing primitives tested offline | Confirm exact displayed time, player identity, restart/reset and result-frame capture in game |
| Input and race information overlay | Offline composition and typed track/difficulty presentation tested | Feed verified live input and actual loaded track/difficulty to recordings |
| Run history and personal records | Database and offline indexing foundations tested; history, notes and comparison candidates awaiting integration | Connect real completed attempts, video/replay playback and in-game library controls |
| Local submission packages and moderator review | Experimental archive creation and checks tested; exchange and validator candidates awaiting integration | Bind real evidence to attempts, test import/export and representative moderator examples |
| Compatibility and settings | Profile editor and isolated Community Launcher loader fixture tested; supplied Display Config and keyboard-patch fingerprints confirmed | Verify actual installed loader, reversible disabled/install/rollback behavior and game performance |

### Latest milestone: recordings that survive interruptions

The v0.0.40-p0 candidate adds private recording output reservations, explicit completion markers and read-only recovery inspection. The goal is to avoid losing or overwriting an attempt when a recording fails or is cancelled. The isolated filesystem tests pass in both Windows build configurations. Actual gameplay recording is still not connected.

We also verified repeated HD test-window recordings of 600, 1,200 and 600 frames, cancellation and simulated failures, and recovery after destroying a capture target. Synthetic audio encoding passes, but real game-audio capture is not yet verified. The test runner lacks a usable playback endpoint for one audio sample test.

## What you will be able to do

- Open a Speedrun menu in the game, choose a track, difficulty, rider, board and weather, then reuse saved setups.
- Race, retry quickly and record automatically, or manually record a complete session including menu selections.
- See the original game time, independent elapsed time and optional input overlay; review attempts and personal records.
- Rewatch videos, compare runs and import/export supported evidence or native replays when safely available.
- Prepare a local submission package and choose when to share it. No automatic upload, no credentials in evidence.

All Tracks and Championship progression, splits, retention settings, configurable recording quality and a standalone moderator validator are planned. The normal game and traditional recording methods remain supported.

## Who is working on what?

The project uses two ChatGPT-assisted development streams, with human oversight and documented source review. Their work is integrated incrementally rather than merging untested changes.

| Workstream | Current work | Remaining milestones |
|---|---|---|
| GPT A — Game integration and validation | Integrate and validate persistent output, maintain compatibility, trace original timing, player identity and result rendering | Actual loader and rollback, safe native menu/track/controller observations, live IGT/RTA and capture binding, gameplay performance, final integration and release checks |
| GPT B — Recording and offline tools | Recorder, overlays, run library and archive features; bounded terminal record format now authorized | Finish and test terminal record encoding; separate review of durable storage, identity allocation and recovery; integrate history, annotations, exchange, validator, playback and comparison features with GPT A |

Both streams share verified v0.0.39-p0 as their baseline. GPT B's additional feature candidates are not all in main yet. The integration queue and exact evidence are maintained in [project status](PROJECT_STATUS.md), [roadmap](docs/product/ROADMAP.md) and the [test matrix](docs/testing/TEST_MATRIX.md).

## What needs to happen before a public build?

1. **Safe game integration:** prove compatibility with the actual installed loader and preserve the player's working game and settings.
2. **Real speedrun demonstration:** record a complete original-game attempt with correct track/difficulty, time and visible finish result, including retries and recording interruptions.
3. **Run management and submission:** connect recordings to the library and evidence packages; verify recovery, import/export, playback and moderator inspection.
4. **Community review and release:** validate performance and rollback on Windows, review representative evidence with moderators, settle licensing and obtain owner approval before distributing an installable build.

There is no announced release date. We will not label a development build as ready to install until the relevant checks pass.

## Optional by design — competition rules

SSC is intended to work alongside the Community Launcher and compatible mods, not replace Xinfe's Online Time Attack. The current reported Speedrun.com rules require video showing track and difficulty selection from the main menu and use the game's displayed finish time. The proposed alternative is to show actual loaded track/difficulty directly in recorded gameplay, if moderators approve the generated evidence.

The approach has received conditional moderator support; a representative demonstration is still required. **No rule change has been approved.** Traditional recording remains valid under the current rules. Client-side checks cannot prove the absence of cheating. Missing or inconsistent evidence is not automatically a violation. See [community acceptance](docs/product/COMMUNITY_ACCEPTANCE.md).

## Community and contribution

SSC is being developed with ChatGPT assistance under a modular architecture, with independently testable components and source-backed documentation. Contributions and review are welcome. The aim is a free, community-maintained project; the actual project license remains pending owner selection. Original game files, private recordings and proprietary third-party binaries are not redistributed.

The [PR66 loader contract](docs/engine/COMMUNITY_LOADER.md) is a promising integration boundary. SSC does not provide a replacement winmm.dll. A tested offline profile editor and native read-only settings preview already exist for development, but actual launcher and in-game controls are not connected.

## Developers — build and evidence

C++17, CMake 3.16+, C99 and Python 3.10+ (for full tests). Visual Studio Build Tools with x86 support:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

The corrected T49 candidate has 60 core test targets per Windows configuration, 6 media tests, 1 storage and 1 entry test, 22 passing capture tests and 1 known audio sample skip, plus the isolated PR66 fixture. Portable Linux excludes Windows-only targets. Run development tools outside the game directory; the offline supreme_diagnostic intentionally reports unavailable engine data.

- [Project status](PROJECT_STATUS.md), [changelog](CHANGELOG.md), [test matrix](docs/testing/TEST_MATRIX.md) — precise revisions and executed evidence.
- [Architecture](docs/architecture/MODULES.md), [recording feasibility](docs/architecture/RECORDING_FEASIBILITY.md), [engine map](docs/engine/ENGINE_MAP.md) — technical design and research.
- [Configuration](docs/contracts/SSC_CONFIGURATION.md), [loader validation](docs/testing/COMMUNITY_LOADER_VALIDATION.md), [Windows baseline](docs/testing/WINDOWS_BASELINE.md) — safe compatibility checks.
- [Run library](src/modules/run_library/README.md), [archive indexing](docs/contracts/ARCHIVE_INDEX.md), [attempt-settings evidence](docs/contracts/ATTEMPT_SETTINGS_EVIDENCE.md) — offline foundations.
- [Roadmap](docs/product/ROADMAP.md), [known issues](docs/testing/KNOWN_ISSUES.md), [release policy](docs/release/CI.md) — future work and gates.

Read-only reference classifier: `python tools/preflight.py <copied-game-root>`. Offline research: `python tools/native_xrefs.py <original.exe> --compare <patched.exe>` (GNU objdump), `python tools/replay_samples.py <replay.dat>`. Optional baseline: `python tools/baseline_snapshot.py snapshot <game-root>` and `python tools/baseline_snapshot.py compare <before.json> <after.json>`. These do not prove runtime compatibility or eligibility.

Public builds remain disabled until licensing and maintainer approval. Development source and binaries are not installable game mods. The public status, player-facing feature table and A/B workstream descriptions are maintained for most substantive changes; minor fixes and raw technical evidence belong in the linked development documents.
