# Supreme Speedrun Companion

**Spend more time racing, less time setting up speedruns.**

Supreme Speedrun Companion (SSC) is an optional Windows mod in development for Supreme Snowboarding (Boarder Zone). Its goal is to make practicing, recording, analyzing and submitting speedruns as convenient as possible, without changing the original game's racing mechanics.

SSC is **not a new version of Time Attack, a replacement for Online Time Attack, or a compulsory anti-cheat**. It is a companion for the existing game and its speedrunning community, intended to bring recording, timing, quick retries, run management and submission preparation into one place.

> **Development status: v0.0.26-p0 — not yet playable or installable.** Native configuration, the reviewed PR66 loader in an isolated synthetic host, native run-library storage and verified offline archive indexing and read-only recovery inspection, synthetic A/V encoding, concurrent timestamped isolated test-window video capture (up to 1,200 selected frames per clip; same-process success/cancellation fixtures), synthetic-only admission model and offline evidence components have automated test coverage. Actual launcher UI and game integration, native menus, race timing and gameplay recording remain unverified. Do not install development files into your game.

## Why build it?

Preparing a Speedrun.com attempt currently involves more than racing: configuring capture software such as OBS, setting up an input overlay or external timer, recording the required menu selections, locating footage after a run, and preparing a submission. Retrying or chaining tracks can interrupt the flow, and forgetting to start recording can waste an excellent attempt.

SSC aims to remove that friction and make participation accessible to more players, from someone trying their first time to someone grinding personal bests.

## What the intended player experience looks like

1. **Choose a setup.** Open a Speedrun entry in the existing game menu, choose a track/difficulty, rider, snowboard and weather, or select All Tracks or Championship. Reuse saved loadouts and a personal settings profile.
2. **Race and retry.** Automatic recording starts early enough to capture the required evidence for each attempt. A configurable quick-restart shortcut should let you retry without returning to the main menu or reselecting everything. Reset-to-track remains distinct from restarting.
3. **See your performance.** Optional input overlay, the game's own time (IGT), a separate real-time timer (RTA), and planned splits and comparison tools. The existing leaderboard's official time is not silently replaced by RTA.
4. **Keep and review your runs.** The companion should finish recordings and prepare local evidence asynchronously, with configurable retention. Browse personal records and run history, rename or favorite runs, rewatch video, and inspect or compare native replays where safely supported.
5. **Share when you choose.** Generate a local evidence package with video, timing, race identity, configuration and available replay/diagnostics. Prepare a Speedrun.com submission through its accepted video/link workflow, with explicit player confirmation; no automatic publication or credentials in evidence.

### Planned features

| Feature | What it means for players |
|---|---|
| Automatic and manual recording | Record attempts automatically or manually capture a full session, including menu proof where required. |
| Quick restart and saved selections | Keep practicing without repeatedly navigating menus or choosing your setup. |
| Dual timers and input overlay | See original game time and independently measured elapsed time; enable or disable individual displays. |
| All Tracks and Championship | Planned automated progression and splits using the agreed competition order and retry rules. |
| Player profiles | Save multiple configurations such as Speedrun Tryhard, Training, All Tracks or Low Performance; change settings in the launcher or in-game when available. |
| Recording settings | Choose output folder, quality, resolution, FPS, bitrate, audio and encoder preferences; set retention and disk-space policy. Supported recording modes must be measured. |
| Run library and analysis | Review, import, export and compare attempts, videos and supported replays. |
| Evidence and moderator tools | A separate validator should help examine package consistency, supported modifications and anomalies, without pretending client-side checks can prove the absence of cheating. |

These are product goals, not a list of features available today. See [development status](PROJECT_STATUS.md) and the [roadmap](docs/product/ROADMAP.md) for the tested scope.

## Optional by design — competition rules

Players must remain free to use the original game, community mods and existing recording methods. SSC is intended to work alongside the Community Launcher and other compatible mods, not to replace Xinfe's Online Time Attack, which has its own replay-upload workflow.

Current reported Speedrun.com rules require video showing track and difficulty selection from the main menu, and use the game's displayed finish time. The proposed alternative is to allow approved SSC-generated evidence to establish those selections without recording the main menu every time. **That alternative requires moderator approval and is not in force.** Until then, automatic-only recording is not established to meet menu-proof requirements; the planned manual recording mode preserves the existing workflow. No approval or endorsement is implied. No physics, difficulty or competition rules should be changed silently.

## Community integration and development

SSC is intended to be an independently configurable mod in the existing Community Launcher. The [PR66 native mod loading contract](docs/engine/COMMUNITY_LOADER.md) provides a promising integration boundary; SSC does not supply a replacement winmm.dll. A separate [offline profile editor](docs/contracts/SSC_CONFIGURATION.md) with portable profile import/export and a native read-only settings preview already exist for development tests. The reviewed upstream source parses the SSC manifest and loads its inert DLL successfully in a synthetic host. The actual launcher page and in-game settings are not yet connected.

The project is being developed with ChatGPT assistance under a documented, modular architecture and strict validation protocol, aiming for readable, maintainable source that community contributors can extend. Contributions and review are welcome; the project license is pending owner selection. Original game files, private recordings and third-party binaries are not redistributed.

## Build and test (developers only)

C++17 core, CMake 3.16+, C99 and Python 3.10+ (required for full tests). Use Visual Studio Build Tools with x86 support:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

Windows core: 50 test targets including native profile persistence, launcher-state projection, native run library, attempt-settings evidence binding, archive indexing and timestamped capture handoff. Portable Linux: configure without -A Win32 (Windows-only targets excluded). Run all development tools outside the game directory. The offline supreme_diagnostic reports unavailable engine data by design.

- [Project status](PROJECT_STATUS.md) — latest verified commit, exact CI evidence and blockers.
- [Architecture](docs/architecture/MODULES.md) — independently testable module boundaries.
- [Configuration contract](docs/contracts/SSC_CONFIGURATION.md) — profile settings, precedence, serialization and prototype editor.
- [Community loader validation](docs/testing/COMMUNITY_LOADER_VALIDATION.md) — source fixture and G1 prerequisites.
- [Windows baseline](docs/testing/WINDOWS_BASELINE.md) — safe disposable-copy observations, no installation.
- [Recording feasibility](docs/architecture/RECORDING_FEASIBILITY.md) — synthetic and isolated capture test scope.
- [Engine map](docs/engine/ENGINE_MAP.md) — version-specific static research.
- [Run-library contract](src/modules/run_library/README.md), [archive indexing](docs/contracts/ARCHIVE_INDEX.md) and [attempt-settings evidence contract](docs/contracts/ATTEMPT_SETTINGS_EVIDENCE.md) — independently tested foundations, not in-game features.
- [Test matrix](docs/testing/TEST_MATRIX.md) and [changelog](CHANGELOG.md) — development history.

Read-only reference classifier: `python tools/preflight.py <copied-game-root>`. Offline research: `python tools/native_xrefs.py <original.exe> --compare <patched.exe>` (GNU objdump), `python tools/replay_samples.py <replay.dat>`. Optional baseline snapshots: `python tools/baseline_snapshot.py snapshot <game-root>` and `python tools/baseline_snapshot.py compare <before.json> <after.json>`. None establish runtime compatibility or eligibility.

GitHub-hosted Windows x86 CI tests Debug and Release; see [CI policy](docs/release/CI.md). Public releases remain disabled pending license selection and maintainer approval. No development source or binary is an installable game mod until the relevant gates pass.

T34 verified: controlled HD abort/restart scopes and process/GUI resource observations; actual game remains unverified.

T35 verified in isolated automated fixtures: injected recorder write failure, staged resource measurements and torn-journal refusal. G1 remains blocked; no gameplay recording claim.

T36 verified in isolated Windows fixtures: compound WGC setup reproduces recurring +2 settled handles; deterministic finalization failure/restart and fresh-child torn-journal refusal passed. No leak attribution, gameplay or installation claim.

T37 verified in isolated Windows fixtures: recurring +1 settled handles in WGC support-only, +2 in item setup, unchanged increment with pool/session. Finalization-state and repeated recovery refusal tests passed. No ownership, leak, gameplay or installation claim.

T38 verified in isolated Windows fixtures: one-time support and factory-only controls flat; item setup retains recurring +1 settled handles with either recreated or reused window. Startup/recovery regressions PASS. No ownership, leak, gameplay or installation claim.

T39 verified in isolated Windows fixtures: repeated capture-item creation +1 settled handles per scope; reused-item Size and session setup flat across six scopes. Track-claim workflow and PR66 PASS. Conditional moderator support; demonstration pending, no rule change. See [community acceptance record](docs/product/COMMUNITY_ACCEPTANCE.md).

T40 verified in isolated Windows fixtures: same-item actual acquisition600/1200/600 independently decoded, settled handles360/360/362. Bound-track workflow and PR66 PASS. No leak-free or production-cache claim. No gameplay/install/rule change.

T41 candidate (branch only): separate destroyed-target refusal and replacement acquisition fixture, bound interval claims, portable parser6/6 and standalone C++108 assertions PASS; Windows/PR66 pending. No game integration, install or rule change.
