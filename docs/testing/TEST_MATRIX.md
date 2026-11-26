# P0-T14 test matrix

| Test ID | Result | Evidence |
|---|---|---|
| TEST-WIN-001 | PASS (Windows x86 offline, v0.0.4 baseline) | GitHub Actions run 45751578053, job 139680364720, source commit 40c15f35014c603bde152279122bc3874194d12a; actual job logs inspected |
| TEST-T14-STATIC-001 | PASS (reference comparison) | t14/xref-comparison.json, GNU objdump 2.44; no execution |
| TEST-T14-REPLAY-001 | PASS (reference summary) | t14/replay-summary.json: 55/61 layout recognized, 6 unsupported, no official time claim |
| TEST-T14-SYNTHETIC-001 | PASS (7 Python tests) | python tests/test_native_research.py -v; local exit 0 |
| TEST-T14-REGRESSION-001 | PASS (Linux Debug, 8/8 CTest) | GCC 14.2.0, CMake 3.31.6, Python 3.13.5; local exit 0 |
| TEST-T14-REGRESSION-002 | PASS (Linux Release, 8/8 CTest) | Same environment; local exit 0 |
| TEST-T14-WIN-001 | NOT_RUN at commit preparation | Inspect GitHub Actions for exact new commit; record separately after execution |
| TEST-T14-INPUTS-001 | PASS | sha256sum -c against pre-existing immutable reference manifest; 10 inputs unchanged |
| TEST-BASELINE-WIN-001 | NOT_RUN | WINDOWS_BASELINE.md |
| TEST-LOADER-001 | BLOCKED | No reviewed native candidate; G1 not passed |
| TEST-TIME-RUNTIME / MENU / CAPTURE / ONLINE | NOT_RUN | No hooks, game or network submissions exercised |

Baseline Windows job logs showed MSVC 19.52.37029.0, Hostx64/x86 cl.exe, Python 3.12.10, 7/7 Debug and 7/7 Release; release job skipped. https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45751578053

Executed local commands from verified v0.0.4 source copy with the three new research/test files and updated CMakeLists.txt:
- python tests/test_native_research.py -v
- cmake -S . -B <fresh build> -DCMAKE_BUILD_TYPE=Debug; cmake --build <fresh build> --parallel 4; ctest --test-dir <fresh build> --output-on-failure --no-tests=error
- Repeat for Release (both 8/8).
- python tools/native_xrefs.py <original> --compare <patched>
- For every reference .dat with parent directory case-insensitive replays: tools/replay_samples.summarize_file; aggregate only recognized layout results, classify ValueError as unsupported. 54 competitive track files, one practice; six ramp/halfpipe unsupported.
- sha256sum -c <private immutable input manifest>.

All raw reference files and verbose local logs are retained in the private research workspace. New tests are synthetic and require neither objdump nor game files on CI. No native runtime, full parser security, performance or leaderboard acceptance claim.

## P0-T15 — v0.0.6-p0

| Test ID | Result | Evidence |
|---|---|---|
| TEST-T14-WIN-001 | PASS (Windows x86 offline) | Run 45752930744, job 139684744190, commit f1dc014864bfad4a32e34d96b6b69be10c12e7b2; actual logs inspected, 8/8 Debug and 8/8 Release, release skipped |
| TEST-T15-STATIC-001 | PASS (selected byte windows only) | t15/byte-windows.json; 9/9 identical original/patched windows, PE raw section mapping; no execution |
| TEST-T15-SNAPSHOT-001 | PASS (7 Python tests) | python tests/test_baseline_snapshot.py -v, Python 3.13.5 Linux exit 0 |
| TEST-T15-SNAPSHOT-002 | PASS (static reference) | two snapshots on untouched SupremeLight, six core subset includes absent winmm.dll, core_observation=incomplete; 16 unchanged files, two absent |
| TEST-T15-SNAPSHOT-003 | PASS (static staged subset) | same six core files including Display Config 1.8.0 + keyboard-patched executable; two snapshots, core_observation=unchanged; no launch |
| TEST-T15-REGRESSION-001 | PASS | fresh Linux Debug configure/build/CTest 9/9, GCC 14.2.0, CMake 3.31.6, Python 3.13.5; exit 0 |
| TEST-T15-REGRESSION-002 | PASS | fresh Linux Release configure/build/CTest 9/9; same toolchain; exit 0 |
| TEST-T15-INPUTS-001 | PASS | sha256sum -c private pre-existing immutable manifest; all 10 inputs unchanged |
| TEST-T15-WIN-001 | NOT_RUN at commit preparation | Inspect CI logs for new commit |
| TEST-BASELINE-WIN-001 | NOT_RUN | Owner disposable-copy procedure in WINDOWS_BASELINE.md |
| TEST-LOADER-001 | BLOCKED | No reviewed native candidate or installed-disabled test |

Linux commands: cmake -S . -B <fresh build> -DCMAKE_BUILD_TYPE=Debug (repeat Release); cmake --build <fresh build> --parallel 4; ctest --test-dir <fresh build> --output-on-failure --no-tests=error. Local source build inputs were compared with main Git blob hashes before edits; uploaded CMake and new Python code/test blobs match locally tested files. GNU objdump -d -Mintel --start-address=<preferred VA> --stop-address=<end> for static disassembly; map RVAs to raw sections for comparison. No runtime game test, official timing comparison or new sanitizer run performed in T15. Windows symlink test may skip if privilege unavailable.

Actual previous Windows CI: MSVC 19.52.37029.0, x86 compiler, Python 3.12.10, 8/8 Debug and Release. https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45752930744

## P0-T16 — v0.0.7-p0

| Test ID | Result | Evidence |
|---|---|---|
| TEST-T15-WIN-001 | PASS | Actual Windows x86 run 45753602966, job 139686916040, commit 8ae9d86d5c1e0a77473db0e10308724b1aab4058; Debug and Release 9/9; release skipped |
| TEST-T16-TIME-001 | PASS Linux | P0_independent_timing: synthetic clock and real steady_clock; Windows QPC branch not executed locally |
| TEST-T16-STORAGE-001 | PASS | six Python unittest cases including persistence across reopen, comparability and malformed values |
| TEST-T16-EVIDENCE-001 | PASS | four Python unittest cases including golden digest and invalid paths |
| TEST-T16-API-001 | PASS | three Python unittest cases, no network |
| TEST-T16-STATIC-001 | PASS | Recomputed Supreme.exe hash; objdump exact range 0x427151..0x427196; presentation constant observed |
| TEST-T16-REPLAY-001 | PASS | 55 recognized, 6 unsupported; aggregate header timings only |
| TEST-T16-REGRESSION-001 | PASS | Linux Debug 13/13, exit 0 |
| TEST-T16-REGRESSION-002 | PASS | Linux Release 13/13, exit 0 |
| TEST-T16-REGRESSION-003 | PASS | Linux Debug + UBSan 13/13, exit 0, no reported sanitizer error |
| TEST-T16-INPUTS-001 | PASS | sha256sum -c against immutable input manifest: 10/10 unchanged before and after |
| TEST-T16-WIN-001 | NOT_RUN at commit preparation | Inspect new Windows CI logs; actual QPC smoke check in test_timing.cpp |
| TEST-CAPTURE / ONLINE / NATIVE | BLOCKED/NOT_RUN | No recorder, transport, native loader or game launch |

Environment: Linux GCC 14.2.0, CMake 3.31.6, Python 3.13.5, SQLite 3.46.1. From fresh copy of local v0.0.6 build inputs verified against current Git tree blobs: cmake -S . -B <build> -DCMAKE_BUILD_TYPE=Debug (repeat Release), cmake --build <build> --parallel 4, ctest --test-dir <build> --output-on-failure --no-tests=error; UBSan Debug adds -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" and equivalent C flags. All final source/test/CMake blobs compared to locally tested content before commit. No Windows game, storage crash-recovery or actual media backend test. Windows symlink fixture may skip when privilege unavailable. See docs/contracts for scopes and limitations.

Previous Windows logs: MSVC 19.52.37029.0, x86 compiler, Python 3.12.10, 9/9 Debug and Release; https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45753602966

## P0-T17 — v0.0.8-p0

| Test ID | Result | Evidence |
|---|---|---|
| TEST-T16-WIN-001 | PASS | Actual run 45757144179 job 139698327194, commit 34f348c0508f8d7fe5e1c9e0e448712afbae954e; 13/13 Debug and Release; QPC smoke passed; release skipped |
| TEST-T17-ARCHIVE-001 | PASS Linux | python tests/test_evidence_archive.py -v, six cases, exit 0 |
| TEST-T17-PREF-001 | PASS Linux | P0_preferences_port, injected in-memory adapter |
| TEST-T17-REPLAY-001 | PASS Linux | P0_replay_compare, two synthetic cases |
| TEST-T17-REPLAY-002 | PASS static | 135 same-directory pair comparisons; arithmetic reconciliation, not IGT validation |
| TEST-T17-STATIC-001 | OBSERVED_STATIC | Original objdump range 0x427000..0x427100, second presentation path |
| TEST-T17-REGRESSION-001 | PASS Linux | fresh Debug CTest 16/16 |
| TEST-T17-REGRESSION-002 | PASS Linux | fresh Release CTest 16/16 |
| TEST-T17-INPUTS-001 | PASS | sha256sum -c against recorded input manifest and v0.0.4 hash; 10/10 unchanged |
| TEST-T17-WIN-001 | NOT_RUN at commit preparation | Inspect exact new hosted CI logs |
| TEST-T17-MEDIA-001 | NOT_RUN | Windows-only opt-in probe; separate CI job configured |
| TEST-G1/G2/G3/G4 | BLOCKED/NOT_RUN | No native game test or complete evidence import/export workflow |

Local commands: cmake -S . -B <fresh build> -DCMAKE_BUILD_TYPE=Debug (repeat Release); cmake --build <build> --parallel 4; ctest --test-dir <build> --output-on-failure --no-tests=error. Both passed 16/16; new CMake and source/test Git blob hashes checked against uploaded tree. No fresh sanitizer run. Windows CI uses official hosted runner and explicit Win32. Media probe compiled/run only by separate hosted job, not part of portable 16 targets. No release authorization. Prior verified logs: https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45757144179 .

### T17 follow-up: first hosted run and sanitizer

- TEST-T17-WIN-001 PASS: Windows x86 core build job 139704913828, run 45759192220, commit 3b523f130791e88df44bfb74455a671412c00142. Actual logs inspected: MSVC 19.52.37029.0, 16/16 Debug and 16/16 Release; development release skipped.
- TEST-T17-MEDIA-001 FAIL (compile): job 139704913379 on the same commit, MSVC C4245/C2220 at mf_probe.cpp lines 68–69, signed/unsigned SDK stream selectors. No encoding/decoding executed. Explicit DWORD casts added, /W4 /WX retained. Re-run pending.
- TEST-T17-REGRESSION-003 PASS: Linux Debug + UBSan 16/16, GCC 14.2.0, CMake 3.31.6, Python 3.13.5. Fresh build with CMAKE_CXX_FLAGS and CMAKE_C_FLAGS=-fsanitize=undefined -fno-sanitize-recover=all, configure/build/CTest exit 0. No sanitizer errors reported. Same portable source as above; media target disabled.
- Exact hosted logs: https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45759192220 .
