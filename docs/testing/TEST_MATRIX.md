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
