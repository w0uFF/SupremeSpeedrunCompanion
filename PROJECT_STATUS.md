# Project status

Revision v0.0.14-p0 — native configuration and reviewed community-loader fixture verified; real-game gate remains BLOCKED.

## Verified source/test revision
- SSC commit ca9637c90d3ed72abc6701f59f3699b7d9e996e9, Windows x86 run [45801293414](https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45801293414): 30/30 core targets each Debug and Release (job 139844872143), media 5/5 (139844872035), capture 3 PASS/1 SKIP (139844872157; own-process audio MMRESULT=2, no sample), WinSQLite 1/1 (139844871709), independent inert DLL 1/1 (139844872085); release skipped (139845451478).
- Exact reviewed PR66 source fixture [45801293718](https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45801293718), job 139844872583: 3 upstream DLL tests, 2 SSC Rust-side integration tests including actual ModsState serialization to native configuration preview, actual upstream synthetic-host runtime fixture PASS, SSC proxy load counts 0/0/1. Actual logs inspected. Previous fixture 45798560012 and repeated v0.0.14 manifest fixture 45800954974 also passed.
- All 18 private reference hashes unchanged. No upstream changes, third-party code committed, original game execution, proxy replacement or release.

## Implemented and tested
- Native bounded read-only PR66 JSON projection, actual UTF-8 launcher-state file consumption by standalone Win32 preview, existing typed profiles and whole-group overrides, exact numeric validation, ignored unrelated mod configuration. C++/Python representative conformance cases and malformed/missing input rejection. Source files unchanged. No DLL bootstrap or live game consumption.
- Native immutable owned AttemptSettings, independent from later profile edits; no actual run ID/evidence linkage yet.
- Portable single-profile import/export strips machine-local package path, refuses name collisions, preserves active profile. Python CLI/editor use optimistic stale-editor checks; single writer remains required. Local virtual-display GUI smoke and simulated import/export controller checks passed, not human UX or actual launcher UI validation.
- Static original-game result-row writer and alternate source path located; see COMMUNITY_LOADER.md. IGT semantics, authoritative finish event and game-thread observation still unresolved.

## Gates and next action
G1 synthetic source-loader fixture prerequisite satisfied for exact reviewed PR66 source, not for the installed launcher/proxy binary. Owner disposable-copy Windows baseline, supported loader provenance, installed-disabled discovery/launch/exit/rollback and review still required. Do not install development files yet. See WINDOWS_BASELINE.md and COMMUNITY_LOADER_VALIDATION.md. No live hooks, race control, recording activation, native menu or competition approval. Public releases disabled pending licensing/owner approval; no actual game package distributed.

P0-T24: collect baseline and identify installed loader without changing it; independently continue static lifecycle/IGT research, source-backed observation boundary design, run-library adapter and safe session/evidence binding. Do not advance G1 or G2 without applicable evidence. Product-first README retained; SSC remains optional and current rules remain in effect.
