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

### T17 follow-up: media retry and archive validation hardening

- TEST-T17-MEDIA-002 PASS (actual Windows x86 synthetic encode/decode): run 45759324352, job 139705335489, commit f3dded0e8accd0c9e6dc1a841648478dc6a950a3. MSVC 19.52.37029.0, 1/1 CTest, 1.54 s. Actual log: Decoded 12 H264 frames, 160x96, monotonic timestamps and luma pattern verified. No actual game, audio or capture test.
- TEST-T17-WIN-002 PASS: same commit, job 139705335614, core 16/16 Debug and 16/16 Release from actual logs.
- TEST-T17-ARCHIVE-002 FAIL reproduced, then PASS: crafted ZIP64 EOCD/locator fixture accepted by original verifier despite classic-only contract. Added explicit ZIP64 locator rejection, new seventh archive unittest passes. Re-executed all portable CTest targets on existing Debug/Release/UBSan builds with updated Python source: 16/16 each, no sanitizer report. New commit Windows CI pending.
- https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45759324352

## P0-T18 — v0.0.9-p0 independent core

- TEST-T17-FINAL-WIN PASS: run 45759479502, commit 5c56048cef89aa868abebdc2325a11d3a2c158b9, actual jobs 139705835560 and 139705835199. MSVC 19.52.37029.0 x86; 16/16 Debug and Release, 1/1 synthetic video (12 decoded frames), release skipped.
- TEST-T18-WORKER PASS Linux: GCC 14.2.0, g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread -I. tests/test_recorder_worker.cpp; run exit 0. Optimized -O2 same flags, 50 executions with 5-second timeout each passed; Debug UBSan -fsanitize=undefined -fno-sanitize-recover=all exit 0. Deterministic gated fake sink, no media execution.
- TEST-T18-REPO PASS Linux: g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. tests/test_run_repository.cpp, exit 0. Pure comparison only, not persistence.
- TEST-T18-MANIFEST PASS Linux: python tests/test_run_manifest.py -v, six cases, exit 0. Reconstructed exact baseline evidence_archive.py, evidence_inventory.py and baseline_snapshot.py from GitHub source; Git blob hashes checked before modifications. New and modified source/test blobs match locally executed content.
- TEST-T18-INPUTS PASS: SHA-256 of all ten supplied inputs matches docs/engine/INPUT_INVENTORY.md. Original Supreme.exe separately extracted read-only; hash matches.
- TEST-T18-CI NOT_RUN at commit preparation: 19 portable CTest targets configured. Local container cannot clone GitHub (DNS unavailable); only isolated new tests were run locally, not full v0.0.9 regression. Hosted CI will establish exact commit regression.
- G1/G2/G3/G4 BLOCKED or NOT_RUN; no gameplay, live capture, archive import or official time validation.

- TEST-T18-AV NOT_RUN at preparation: actual Media Foundation synthetic AAC/H.264 mux and decode candidate, separate opt-in Windows media job; no Windows SDK or runtime locally.
- TEST-T18-STATIC OBSERVED_STATIC: original hash verified, objdump RVA 0x23e15..0x23e67 and PE IAT mapping 0x46d47c to Menu::Discard_Page; no execution.

- TEST-T18-CORE-WIN PASS: run 45776710377, commit 7994b6caf6c87f24927d939c40ec88a115bdd47c, job 139761991897: 19/19 Debug and 19/19 Release; existing video probe 1/1; release skipped.
- TEST-T18-AV PASS: run 45776852153, job 139762464634; actual logs: 2/2 media tests, H264 12 frames, AAC PCM 23552 frames, audio first 0, audio end 4906666, video end 4800000 (100 ns ticks).
- TEST-T18-LEGACY-ARCHIVE PASS Linux: reconstructed exact prior test_evidence_archive.py from GitHub source (blob 6a118015d5400d9d9078e0dfc484ec30cc4a5fbe), seven cases passed against modified reader, Python 3.13.5.
- TEST-T18-ASYNC-AV NOT_RUN at preparation: real synthetic MF worker adapter; separate third media CTest in hosted CI.

- TEST-T18-AV-CORE PASS: run 45776852153, job 139762464975, 19/19 Debug and Release.
- TEST-T18-ASYNC-AV PASS: run 45777095746, job 139763270864, commit 9379028ebdc151d526097334e8e9af109fd86eaa. Actual logs: 3/3 tests passed; worker finalized 24 packets without observed queue drops; decoded 12 H264 frames, 23552 PCM frames, first_100ns=0, audio_end_100ns=4906666, video_end_100ns=4800000. Core job 139763271203 19/19 Debug and Release, release skipped.
- TEST-T18-MEDIA-ARCHIVE NOT_RUN at preparation: actual synthetic async MP4 passed to schema 2 publisher and re-read/compared; final Windows run required. No game capture, actual run evidence or submission.

- TEST-T18-FINAL-WIN PASS: exact code/test commit ab0bcf82ededc13ac2271334b072e4ca730aa01a, run 45777278765, build job 139763888889: actual logs 19/19 Debug and 19/19 Release; release skipped.
- TEST-T18-MEDIA-ARCHIVE PASS: same run, job 139763888598: 3/3 media tests. Async worker finalized 24 packets with no observed queue drops, decoded 12 H.264 frames and 23552 PCM sample frames, audio end 4906666/video end 4800000 100 ns units; actual log "Synthetic async MP4 archived, published without overwrite, and independently verified; eligibility unverified". Temporary synthetic files only, no upload.
- TEST-T18-WRAPPER-LOCAL PASS: Python 3.13.5 executed exact wrapper (blob 796dca5c1332e6c01217f3a56556df034f9af291) with a local ffmpeg-generated 160x96 H.264/AAC synthetic fixture as subprocess, archive path verification passed; not a Windows or worker test. No new dependency in repository.
- Final input hashes recomputed, all ten match the recorded immutable inventory. Full local checkout remained unavailable; no claim of full Linux CTest. This follow-up commit changes documentation only; last code/test revision verified above. https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45777278765

## P0-T19
- TEST-T18-DOC-WIN PASS: exact commit c9ef54e14973cdfc43ee2961f6232a8f967736d3, run 45777481519, actual jobs 139764562866 (19/19 each), 139764563258 (3/3 media incl archive); release skipped.
- TEST-T19-QUALITY PASS local: Debian GCC 14.2.0, g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. tests/test_recording_quality.cpp -o quality && ./quality; exit 0. Repeat -fsanitize=undefined -fno-sanitize-recover=all; exit 0. Git blob hashes match uploaded new header and test. Integrated worker CI pending.
- TEST-T19-INPUT PASS: sha256sum of all ten supplied input ZIP/MD references matches prior inventory. Local clone failed DNS; no full Linux CTest executed.
- TEST-T19-STATIC OBSERVED_STATIC: original exe and Supreme_Game.dll hashes confirmed, objdump -d -Mintel and objdump -p as documented in ENGINE_MAP.md.

- TEST-T19-CORE-WIN PASS: run 45781735952 job 139778579562, exact commit 65d1a7241bf572e3a339b4f011bb604a9ed4090c, 20/20 Debug and Release; existing media 3/3, release skipped.
- TEST-T19-SUSTAINED initial FAIL: run 45781810092, media job 139778827294, wrapper syntax error at line 35 caused by JavaScript replacement-string dollar/quote interpolation; no media executed. Core 20/20 each. Replaced entire wrapper with explicit source; local python -m py_compile exit 0, Git blob checked. Hosted rerun pending.

- TEST-T19-CAPTURE initial FAIL (compile): run 45781989602, capture job 139779424623, MSVC STL1011 in <experimental/coroutine> with C++17. No capture execution. Changed only standalone WGC target to C++20; core remains C++17, /W4 /WX retained. Hosted retry pending.

- TEST-T19-MEDIA PASS: run 45782148999 job 139779954160 (wrapper correction), 4/4 media, core 20/20 both; capture still had prior C++17 compiler error. Final exact code/test revision 8f005bda4557498bc1d3ab0e7c77e0963b783fb9 run 45782226376 job 139780214447: 4/4 media. Actual sustained log: video 250/250 packets, audio 250/250 packets, 100000000 100ns ticks of written duration each, 0 observed internal gaps/drops/rejections/discards, QueueHighWater 2/32, 500 packets encoded, 250 H264 frames decoded, AAC PCM 480256 frames, audio_end_100ns=100053332, video_end_100ns=100000000. MP4 and hashed sidecar archived and verified. Synthetic 160x96 25 FPS, 10 seconds, no real capture performance claim.
- TEST-T19-CAPTURE PASS (feasibility only): same run, job 139780214356, 2/2 tests. WGC own-window frame metadata: 3 frames, 306x233, increasing QPC-correlated timestamps (coarse one-second sanity bound). No pixel validation. Own-process loopback activated and capture service obtained; 48 kHz stereo PCM; no audio packets read. No game or external target.
- TEST-T19-FINAL-CORE PASS: same run, job 139780213928, MSVC 19.52.37029.0 x86; 20/20 Debug and Release; development release skipped. https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45782226376
- TEST-T19-SQLITE-LOCAL PASS: exact standalone source tools/storage/sqlite_probe.cpp compiled Debian GCC 14.2.0 with -std=c++17 -Wall -Wextra -Wpedantic -Werror and -lsqlite3, system SQLite 3.46.1; fresh private temp db exit 0, second invocation same path exit 2. Blob hash dde158f02ce9e790763af69379196d3319f30b77. No default build target or production DB adapter, not Windows-tested.
- G1/G2/G3/G4 BLOCKED or NOT_RUN; no actual game capture, authoritative native race timing or full evidence import.

## P0-T20
- Baseline PASS run 45782546029, exact 064b2cbe52b74555bf2c2583fc297132c1877be8, jobs 139781254496 (20/20 each), 139781254967 (4/4), 139781255032 (2/2), release skipped.
- Capture primitives PASS local: Debian GCC 14.2.0, g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. tests/test_capture_primitives.cpp -o primitives && ./primitives, exit 0. Repeat with -fsanitize=undefined -fno-sanitize-recover=all, exit 0. Exact source/test Git blob hashes checked. Full local checkout failed DNS; no full Linux regression claim. 10/10 supplied reference SHA-256 values match inventory. Windows CI pending.
