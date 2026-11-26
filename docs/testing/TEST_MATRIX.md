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

- T20 core PASS on 45783418178 job 139784156751 (21/21 each); media 4/4; capture baseline 2/2. Pixel fixture PASS run 45783539220 capture job 139784561057, 3/3 tests, actual two verified NV12 crops and decoded MP4; synthetic timestamps/audio. Core 21/21 and media 4/4 same run.
- T20 storage initial FAIL compile C1083 missing winsqlite3.h on run 45783668360 capture job 139784999468, audio sample target compiled but tests not reached. Switch to documented system ABI and separate job, retry pending. Added 30s media test pending.
- T20 static high-time file reader/writer candidates documented using original hashes, objdump -d/-p; not race timing validation.

- T20 final core PASS: exact eda75eb2adc1d7ed319526b8a083696658a7bbf0 run 45783962593 job 139785959273, MSVC 19.52 x86, Debug 21/21 and Release 21/21; release skipped.
- T20 final media PASS: job 139785958797, 5/5 CTest, 30s 160x96 25 FPS 750 video and 750 audio accepted/written, 0 drops/invalid/discarded/internal gaps, high-water 2/32; decoded 750 H264 frames and 1440768 PCM frames; audio_end_100ns=300159999, video_end_100ns=300000000. Archive and quality sidecar verified. No actual capture timing inference.
- T20 capture PASS LIMITED: job 139785959450, three passed, one skipped. Actual own-window pixel crop/encode, metadata and audio activation passed. Process audio sample test SKIPPED/BLOCKED, waveOutOpen MMRESULT=2, before Start/GetBuffer. CTest summary 100% excludes skip; do not report 4/4. Interactive procedure CAPTURE_PROBE_VALIDATION.md.
- T20 WinSQLite PASS: job 139785959280, 1/1 CTest, system WinSQLite 3.53.4 parameter binding, null/zero, rollback, foreign keys, reopen. Python Unicode temporary path/existing database test passed. No production adapter.
- Exact CI logs: https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45783962593 . This follow-up only records documentation, no build/test changes. G1/G2/G3/G4 remain blocked/not run.

## P0-T21
- Baseline PASS run 45784286622, exact 9c5f532763d0d6a4253180843c7a4be0a1523d08, jobs 139787030455 (21/21 each), 139787030791 (5/5), 139787030977 (3 pass/1 skip audio MMRESULT=2), 139787030840 (1/1); release skipped.
- Profile reference PASS local: python tests/test_ssc_profiles.py -v, seven cases, exit 0; python -m py_compile tools/ssc_profiles.py tools/ssc_profile_cli.py exit 0; CLI init, duplicate, select, resolve exit 0 in private temporary directory. Exact source/manifest/test Git hashes matched remote. No actual launcher UI or native profile consumption.
- Inert PE static feasibility PASS: clang-cl /nologo /c /W4 /WX /GS- /Zl /O2 /clang:--target=i686-pc-windows-msvc entry.cpp; lld-link /dll /noentry /nodefaultlib /machine:x86 /def:exports.def. Initial explicit decorated alias failed (undefined __SSCInitialize), corrected to undecorated export definition. objdump -p confirms PE32 i386, one SSCInitialize export, zero imports/entrypoint; objdump -d shows mov eax,1; ret. Freestanding static build only, not MSVC Windows runtime. Independent Windows host test pending. No upstream binaries executed.
- All 18 immutable reference hashes unchanged after inspection. Full local checkout unavailable DNS; no full Linux regression.
- Source inspection OBSERVED_STATIC, see COMMUNITY_LOADER.md. Upstream fixture and actual game G1 NOT_RUN. 22 core targets plus separate native-entry test configured. Hosted regression pending.

- T21 first hosted core PASS: exact a8d229d4895598c0bd2ce92000e6e341b5741ab0 run 45795565474 job 139825204147, Windows x86 22/22 Debug and Release; inert entry PASS job 139825204250, actual MSVC DLL loaded and exact initializer called twice in independent host, 1/1 CTest; storage 1/1; release skipped. No actual loader/game test.
- T21 C++ configuration port PASS local: g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. tests/test_configuration_port.cpp, exit 0; repeat with -fsanitize=undefined -fno-sanitize-recover=all exit 0. Fake store/path policy; validate precedence, snapshot copy, session edit, persistence error, invalid enum/FPS/path/hotkeys and duplicate profiles. Exact source/test Git blobs checked. Final 23-target Windows regression pending.

- T21 FINAL PASS (limited scope): exact code/test commit 478a33a790fdebc24c0f3788f10b1a00fa5dbdeb, actual run 45795828198. Build job 139826087549, Debug 23/23 and Release 23/23, P0_configuration_port passed both. Community-entry job 139826085384 1/1, exact export loaded and callable twice in independent Win32 host. Media job 139826085313 5/5, 30s 1500 packets, no observed queue drops, 750 H264 frames, 1440768 PCM frames, quality archive verified. Capture job 139826085051 3 pass/1 skip, same MMRESULT=2 (playback endpoint), no actual audio sample collection. Storage job 139826085346 1/1. Release job 139826521975 skipped. https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45795828198
- T21 additional static evidence: original game PE timestamp and SizeOfImage compared with OnlinePlay initializer checks; helper reads candidate result-row float as documented in COMMUNITY_LOADER.md. No engine execution or validation.
- Final source reference SHA-256 recheck 18/18. Documentation-only follow-up records evidence; no build/test change. G1/G2/G3/G4 remain blocked/not run for real game.

## P0-T22
- Baseline PASS run 45796169235, exact cf214cf65fae15e13557c2d2e9b64e49178c56fc, core 23/23 each, media 5/5, capture 3 pass/1 skip, storage 1/1, inert DLL 1/1; release skipped.
- Codec/conformance PASS run 45797732188 job 139832612029, 25/25 Debug and Release. Python/C++ canonical roundtrip including Unicode, six invalid inputs rejected; bounded schema subset only.
- Native persistence PASS run 45797875454 job 139833105075, 26/26 Debug and Release. Win32 profile store exclusive create, replace/reopen, Python interchange, malformed preservation, Unicode path and existing-directory checks. Actual directory/free-space policy not fully exercised; no game consumption.
- Offline GUI PASS limited: python -m py_compile tools/pr66_fixture.py tools/ssc_profiles.py tools/ssc_profile_gui.py; xvfb-run -a python tools/ssc_profile_gui.py --smoke, exit 0, local virtual display. Widget creation and save/duplicate/activate. No user interaction/actual launcher.
- PR66 staging PASS local: exact archive verified, fresh staging + SSC test files, existing destination refused. Rust/Cargo and Windows not present locally. Workflow 45798445269 failed at SSC build due wrong CMake option, fixed; corrected run 45798560012 upstream build/fixture in progress at preparation. No actual fixture pass claim.
- Final Windows core/test revision 48fabde79818ac549310c47bf8781c413ed02855 run 45798560015, build job 139835457442 26/26 each, media job 139835457762 5/5 with 1500 packets and verified quality archive, capture job 139835457956 three pass/one skip MMRESULT=2, storage job 139835457527 1/1, entry job 139835457187 1/1, release skipped. Actual logs checked. No full local Linux regression.
- 18/18 immutable SHA-256 checks passed. Original game additional lifecycle xrefs OBSERVED_STATIC, see COMMUNITY_LOADER.md. G1/G2/G3/G4 not advanced. Actual game/launcher UI NOT_RUN.

## P0-T23 — v0.0.14-p0
- T22 final documentation baseline 2d66e667b006c2bb17460513e9722697232383b1: run 45799199128, 26/26 Debug/Release, media 5/5, capture 3 PASS/1 SKIP, storage 1/1, independent DLL 1/1; release skipped.
- PR66 actual reviewed-source synthetic runtime fixture 45798560012/job 139835457310 PASS: upstream DLL tests 3/3, SSC manifest 1/1, startup timing, once-only load, private dependency, same-thread initializer, loader-lock release, INI replacement, original INI unchanged, disabled mod. SSC load counts 0/0/1. Repeated v0.0.14 manifest fixture 45800954974/job 139843720974 PASS.
- 28-target intermediate run 45800955070/job 139843721735 PASS both configurations; 30-target native preview/profile exchange run 45801105316/job 139844236428 PASS both configurations.
- Final code/test revision ca9637c90d3ed72abc6701f59f3699b7d9e996e9, run 45801293414: core 30/30 each (139844872143), media 5/5 (139844872035), capture 3 PASS/1 SKIP (139844872157; MMRESULT=2 no audio sample), WinSQLite 1/1 (139844871709), inert DLL 1/1 (139844872085), release skipped (139845451478). Logs checked.
- Extended fixture 45801293718/job 139844872583 on final code revision PASS: 3 upstream tests, 2 SSC tests including ssc_native_preview_from_upstream_state, upstream runtime fixture and SSC load counts 0/0/1. Build command and --config-preview argument verified in actual logs; no skip inferred as PASS.
- Native launcher parser/attempt snapshot, Python/C++ representative conformance, read-only actual file preview, malformed/missing state refusal, profile exchange and stale-editor tests included in core. Local Python compile and xvfb GUI smoke PASS; simulated import/export dialogs exercised actual GUI controller, path stripping and active selection preservation; no human GUI test. Profile module blob 288416390451140d4d005f0d835aaeadf6aad387 and GUI blob e43710c9dc6d44aac8a4ae200489ab6db69634f6. Full local Linux regression not run.
- 18/18 reference SHA-256 checks passed. Original game row writer and alternate path OBSERVED_STATIC. G1/G2/G3/G4 not advanced. Actual installed loader, launcher GUI, original game and authoritative timing NOT_RUN.

## P0-T24 — v0.0.15-p0
- Baseline 1c548dce1e23c9a2a0e09a1fea2002bc1b11ca37 run 45802187280: 30/30 Debug and Release (139847991743), media 5/5 (139847991628), capture 3 PASS/1 SKIP (139847991332, MMRESULT=2), WinSQLite 1/1 (139847991346), independent DLL 1/1 (139847991029), release skipped. PR66 run 45802187381/job 139847990756 actual logs: 3 upstream and 2 SSC tests passed, upstream fixture PASS, actual load counts 0/0/1.
- Intermediate native DB run 45803020180/job 139850866774 FAILED Debug 30/31, new P0_native_repository exit 3 WinSQLite result 19; repeated 45803197801/job 139851481385 FAILED Debug 32/33 same new DB failure, both new attempt tests passed. Root cause SQL CHECK(igt_ms>=0 AND status=0) evaluates false for status!=0 and NULL. Local Python sqlite3 in-memory reproduction confirmed four cases; corrected CHECK(igt_ms IS NULL OR (igt_ms>=0 AND status=0)), independent DB-level acceptance/rejection regression added. No deployed native schema to migrate.
- Final exact code/test commit 1d3a55591e49f4e7acac55da84d967e01448d763, run 45803303956: 33/33 core both (139851861775), 5/5 media (139851861738, 1500 packets without observed queue drops), capture 3 PASS/1 SKIP (139851861560, MMRESULT=2), storage 1/1 (139851861051), inert DLL 1/1 (139851861420), release skipped (139852482918). Actual logs inspected. Native DB fixture uses actual system WinSQLite, separate subprocess reopening and Python SQLite independent schema/contents inspection. Unknown schema/version bytes unchanged, no-overwrite create, duplicate/conflict, NULL/0, annotation/FK failure and PB checks. Actual attempt sidecar C++ canonical bytes matched Python expected, wrong identity/schema/source/private path/role rejected and failed output removed, legacy schema2 unaffected without reserved member. Synthetic video bytes are intentionally not playable; no gameplay/media validity inferred from archive.
- v0.0.15 manifest fixture 45803303975/job 139851860745 in progress at preparation, fingerprint acquisition and inert build completed, actual upstream test not yet verified for this revision.
- OBSERVED_STATIC: GNU objdump -d -Mintel on original Supreme_Game.dll SHA256 b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b, PE .rdata RTTI/32-bit pointer cross references and call-site trace, see ENGINE_MAP.md. All 18 references SHA-256 rechecked unchanged (sha256sum -c). No full local Linux regression this phase. G1/G2/G3/G4 actual game NOT_RUN.

## P0-T25 — v0.0.16-p0
- Baseline 3727200a75a8683e820d62893227c2b7e477f6d6 run 45803911664: core 33/33 each (139854002001), media 5/5 (139854002172), capture 3 PASS/1 SKIP (139854001675, MMRESULT=2), storage 1/1 (139854002314), entry 1/1 (139854002334), release skipped. PR66 run 45803303975/job 139851860745 actual 3 upstream+2 SSC tests, startup fixture PASS, loads 0/0/1.
- Archive index 952587ee1743f94256acb1961bbe26aa0d912b41 run 45815855072/job 139895268738 PASS 34/34 both. Real native repository via ssc_index subprocess, independent Python SQLite readback; pending recovery/index outage, finalized retry, duplicate, manifest conflict, provenance conflict, invalid archive/partial and direct local origin refusal. Synthetic bytes not playable.
- Intermediate 2dec8ad07bb20785cea1262e8768330b469a83cf run 45816020141: core 35/35 both (139895849632), media and capture compile FAILED (139895849274/139895849629) MSVC C4456 local captured shadows previous bool, C2220 /WX. Corrected local name, not compiler policy.
- Final code/test 95d197430ade7544ab429bc61c471e59ace96262 run 45816213715: core 35/35 both (139896509798), media 5/5 (139896510049), capture 4 PASS/1 SKIP (139896509862, audio MMRESULT=2 before sample), storage 1/1 (139896509802), independent DLL 1/1 (139896509330), release skipped (139897170318). Actual logs checked. Extended synthetic A/V: 1500 packets no observed queue drops, 750 video and 1440768 PCM frames, existing quality/endpoint validation.
- p0_timed_video_probe PASS actual TimedWGC 12 13 1 3929218750 3932968750; TimedHandoff 12 0 4150000 4150000 0 11; actual TimedPTS expected/decoded in 100ns: 0/0, 312500/312400, 468750/468800, 781250/781200, 937500/937600, 1406250/1406400, 2031250/2031600, 2343750/2344000, 2656250/2656400, 2968750/2968800, 3437500/3437600, 3750000/3750000. Max abs delta 350*100ns=0.035ms. 12 decoded H264 frames 160x96, luma pattern verified. Python independently parses handoff bytes and checks intervals/quality/PTS, fixed 2ms exploratory tolerance. No audio/game/continuous concurrent pipeline, selected frames not full compositor coverage; 40ms final nominal duration. Portable P0_timed_frames roundtrip/truncation/order/cap unit tests passed both configurations.
- v0.0.16 PR66 rerun 45816213574/job 139896508310 still in progress at preparation; no loader implementation change. All 18 reference SHA-256 values computed and compared to recorded inventory before/after; original EXE and DLL hashes unchanged. Static engine evidence in ENGINE_MAP.md. No full local Linux regression, no actual game launch or gate change.

T25 final fixture follow-up: v0.0.16 reviewed PR66 source run 45816213574/job 139896508310 on code revision 95d197430ade7544ab429bc61c471e59ace96262 completed successfully. Actual logs: upstream 3/3, SSC 2/2 (including native preview from upstream state), startup/runtime fixture PASS, loads 0/0/1 for disabled/requested, enabled/not requested and enabled/requested. This supersedes T25's pending fixture note, not G1 or installed-game verification.

## P0-T26 — v0.0.17-p0
- Baseline 31f64a5976736fef7e763625b155811f71535877 run 45816915064: 35/35 each, media5/5, capture4 PASS/1 SKIP, storage/entry1/1, release skipped.
- f608fdc0c6cecb833cc26b68d9104fbb4cb8332a run 45855940721, e1f15e10d9c8939ed3028c1ca5d0165f672699ba run 45856111512: 36/36 each, media5, capture5 PASS/1 SKIP, storage/entry1. Actual concurrent 60-frame handoff, resize refusal, independent decode; expanded archive status regression including pending/finalized/alias/conflict/invalid/orphan/unavailable, DB bytes unchanged.
- Final code abfac10ed764e1004978a5c9b9f2b42110fa51dd run 45856451103: core 36/36 Debug and Release (140035985481), media5/5 (140035984753), capture6 PASS/1 SKIP (140035984295, MMRESULT=2), storage1/1 (140035984568), entry1/1 (140035984613), release skipped (140036790228). Actual 60-frame logs LiveCapture 60 61 1 58; LiveHandoff 60 0 20556250 20556250 0 1 0 0; decoder delta400*100ns, resize386x293. 240-frame logs LiveCapture 240 240 0 238; LiveHandoff 240 0 87275000 84368750 2906250 1 0 0; decoder delta400*100ns. Controlled own-window pause, no game. Synthetic 1500 packets regression passed. 
- v0.0.17 PR66 run 45856450816/job 140035983590 PASS actual upstream 3/3, SSC 2/2 including native preview, startup/runtime, load counts 0/0/1. No actual installed game/launcher validation.
- All 18 input hashes match before/after. Static evidence ENGINE_MAP.md. Full local Linux regression, actual device loss and G1/G2/G3/G4 game NOT_RUN.

T27 candidate: 37 core targets expected; 8 capture targets expected, including 600-frame own-window capture and simulated removal inside the short fixture. New synthetic journal test, expanded sink failure checks. Actual CI pending; no new PASS claimed. Existing audio endpoint blocker and G1 retained.

T27 first candidate ef335294a03a1746b8b3d52d1e48c660b09aa899 run 45860359753: core 37/37 both (140049475158), media 5/5 (140049474769), capture 7 PASS/1 SKIP (140049475038), storage 1/1 (140049474987), entry 1/1 (140049476914), release skipped (140050194436); actual logs inspected. Extended live 600 decoded, max delta 400*100ns; span 20.993125s, gap 275ms, high-water 1/8, zero drops/discards. Injected removal cancelled; actual audio skip MMRESULT=2. Initial Python model 4/4 locally. Follow-up saturation, mixed archive and synthetic completeness/reconciliation tests pending CI. Full Linux regression, game and actual device loss NOT_RUN.

T27 version-only intermediate 7bfabcbd2b44b38967d31741b75b38dae2745f09 run 45861470989/job 140053314875 Debug 36/37 FAIL: P0_ssc_profiles full manifest equality found stale generator version. Release NOT_RUN. Correct generator and add CMake version equality; do not weaken exact manifest test. New CI pending. T27 follow-up pure journal tests 5/5 locally, exact blob hashes recorded in PROJECT_STATUS.md.

## T27 final validation
T27 final implementation 83c89e0336cc781bf2fa70cd22754f89c2acf31a; run 45861739545: Windows x86 Debug/Release 37/37 each (140054226996), synthetic media 5/5 (140054227147), capture 7 PASS/1 SKIP (140054226510), storage 1/1 (140054226975), inert entry 1/1 (140054227043), release skipped (140054925770). Actual logs inspected. 600-frame LiveCapture 600 600 0 598; LiveHandoff 600 0 214150000 211243750 2906250 1 0 0; decoded 600, max PTS delta 400*100ns. Resize rejected 386x293; injected removal cancelled; actual device loss/recovery NOT_RUN, audio MMRESULT=2 BLOCKED. PR66 exact revision run 45861731467/job 140054200461: reviewed archive fingerprint gate, upstream 3/3, SSC 2/2 including native preview, startup fixture PASS, load counts 0/0/1. Actual logs inspected. Prior 45861460575 also passed, but its Windows core failed version mismatch; corrected generator and CMake equality guard.
Runs 45861136816, 45861209386, 45861281042 actual logs: core 37/37 Debug/Release, media5/5, capture7 PASS/1 SKIP, storage/entry1/1. Local exact model 5/5. No native game tests or local admission.

## T28 candidate validation
Local Linux x86_64 Python 3.13.5: exact T27 model 5/5; new private snapshot store 6/6 including real os._exit(91) after partial write, flush/fsync and publication; prior snapshots and conflicting destination preserved, exact retries only, canonical schema and phase chain checked. GCC 14.2.0 -std=c++17 -Wall -Wextra -Wpedantic -Werror pure nearest-rank summary exit 0. Git blob hashes in PROJECT_STATUS.md; independently repeated from clean prototype ZIP. Windows expected 39 core tests and 9 capture tests. New 1200-frame fixture NOT_RUN at preparation; preserve 60/240/600 and synthetic A/V. Existing audio endpoint blocker, actual device removal/recovery and game NOT_RUN. No power-loss or security claim.

T28 exact candidate abff44ba0bfa8e3b7f7b3a4fdea2f784c37ef72b, run 45862955701 actual logs: Debug/Release 39/39 each (140058429341; new store and performance tests PASS in both), media 5/5 (140058429903), capture 8 PASS/1 SKIP (140058429930), storage and entry 1/1 each (140058429832, 140058430113), release skipped (140059599122). Actual 1200-frame own-window decode PASS, max PTS deviation 400*100ns. LiveHandoff 1200 0 431337500 428431250 2906250 1 0 0; LivePerformance 1200 43162045 11875000 225 367 605 971 5069 (wall us, process CPU 100ns, median/p95/p99/max selected latency us, final drain us). No gameplay threshold. Actual audio MMRESULT=2 BLOCKED. Exact candidate version PR66 fixture 45862722286 pending. Main v0.0.18 post-merge run 45862519884 PASS, PR66 45862519847 actual logs upstream3/3, SSC2/2, startup/runtime PASS. G1 remains BLOCKED; full Linux regression, actual gameplay and actual device-loss recovery NOT_RUN.

T28 verified implementation abff44ba0bfa8e3b7f7b3a4fdea2f784c37ef72b, Windows run 45862955701; final evidence head 06ba05517db0edcd715c8f56d0240b78e39579c7 run 45863490997 actual logs: Debug/Release 39/39 (140060273173), media 5/5 (140060273236), capture 8 PASS/1 SKIP (140060272857), storage/entry 1/1 (140060273131/140060273279), release skipped. 1200-frame repeat LivePerformance 1200 41239585 16875000 337 476 591 1323 5478; LiveHandoff 1200 0 411962500 409212500 2750000 1 0 0; max PTS delta 400*100ns. PR66 run 45862722286/job 140057622119 upstream3/3, SSC2/2, startup PASS, load counts 0/0/1. Full compare 26bf9105a7446e2645c792ea210eaa43165fa82c -> implementation changed only Markdown, tests/run_live_video.py and tools/media/wgc_probe.cpp; neither capture probe is enabled or built by the PR66 workflow. CMake, workflow, fixture script, generator, Rust manifest tests, mod.json, native entry, config preview blob hashes match exactly; no other dependency changed. Implementation -> evidence head changed only Markdown. Fixture-relevant equivalence accepted. Actual audio MMRESULT=2 and G1 remain BLOCKED. No game install, actual device loss, local origin or public release.

## T29 candidate validation
Local Python3.13.5 Linux x86_64 admission5/5, store6/6, coordinator4/4 (real process death during published snapshot partial write/after flush/after link and conflict preservation; injected external facts), C++ summary GCC14.2.0 exit0 under strict C++17 warnings. Exact new blobs in PROJECT_STATUS.md. Actual ZIP/native index subprocess death after publication or insertion test registered but NOT_RUN locally. Expected Windows core41/media6/capture9. 720p synthetic moving-pattern encode/decode and resource summary pending; retain all low-res/AV/capture regressions. Actual audio, game and physical device loss NOT_RUN/BLOCKED. No new production authority.

T29 exact candidate 45a348df2fb1241a9ced5697aca5fe234d33a285 run 45864597707 actual completed-job logs: core Debug/Release41/41 (140064063817; pure recovery and actual native archive/index interruption tests PASS both), media6/6 (140064063880), storage/entry1/1 (140064063796/140064063779), release skipped. HDPerformance 250 9997117 17187500 585 630 708 778 14095 88203264 16175104 4530176 433404 1 11059200; 250 decoded H264 1280x720 with PTS/luma validation. Units count, wall us, process CPU 100ns, producer median/p95/p99/max us, drain us, peak/current working set bytes, current private commit bytes, MP4 bytes, queue high-water, queue payload cap bytes. Synthetic moving test pattern only; not actual high-res capture or gameplay performance. Capture job140064063406 and PR66 run45864274427 still in progress at evidence preparation. G1 blocked and no promotion.

T29 VERIFIED_AUTOMATED: implementation 45a348df2fb1241a9ced5697aca5fe234d33a285 run45864597707: Debug/Release41/41 (140064063817), media6/6 (140064063880), capture8 PASS/1 SKIP (140064063406), storage/entry1/1 (140064063796/140064063779), release skipped. HDPerformance 250 9997117 17187500 585 630 708 778 14095 88203264 16175104 4530176 433404 1 11059200. Actual 1200-frame LiveHandoff 1200 0 406025000 403118750 2906250 1 0 0; max decoded PTS delta400*100ns. Final documentation head dc9012a1d653ba0704614d17653949e20ad3720c run45864960360: core41/41 both (140065294341), media6/6 (140065294432), capture8 PASS/1 SKIP (140065294396), storage/entry1/1 (140065294260/140065294561), release skipped. Actual logs inspected. PR66 run45864274427/job140062944148 upstream3/3 SSC2/2 startup PASS loads0/0/1; complete comparison bf6f0af6d2b3b135cb3228b9ff2952cfb91782c1 -> implementation -> evidence head Markdown-only, so exact fixture-relevant sources unchanged. Audio MMRESULT=2, G1 BLOCKED; no game, actual device recovery, local authority or release.

## T30 candidate
Local 18 Python tests (admission5/store6/recovery4/retry3) and strict C++17 summary PASS; new retry-plan tests mock observation, do not exercise native external writes locally. Native retry and 720p capture registered, Windows expected core43/media6/capture10. T30 candidate: 240 selected actual 1280x720 NV12 own-window crops, 40s acquisition deadline, same WGC timestamps, one-frame lookahead (explicit byte count), eight-slot payload bound 11,059,200 bytes, 100ms hold cap, 40ms terminal, controlled pause; independent H264 decode count/dimensions/PTS/first luma sample. Uniform dark/bright GDI pattern validated across entire pre-encode crop, not representative game complexity. Report wall/CPU, selected crop conversion and offer latency, final drain, peak/current working set, current private commit, file size and source/crop geometry. Post-finalization output cap16MiB and wrapper timeouts; not a full process memory/disk quota or actual game benchmark. Existing lower-resolution and synthetic A/V tests retained. Synthetic external retry plan binds action, exact immutable journal and resolved directories; reinspection required, one explicit archive publication/alias cleanup or synthetic index insertion, no journal writes. Caller must bind lookup/insert to same trusted database; plan is not independently authenticated DB identity. Native fixture real process termination after hardlink and after native insert; conflicts preserve evidence. No automatic repair, production local authority, native hook, installation, rule changes or release. Expected core43/media6/capture10; Windows candidate pending.

T30 initial Windows 45866018415/job140068924843 Debug42/43 FAIL: native retry fixture used conflict.sqlite for alternate test DB; NativeRepository correctly refused with 'explicit private runs.sqlite path required'. Release not reached. Correct fixture to separate existing private directory with runs.sqlite; preserve repository path restriction and all conflict/byte-preservation assertions. New exact CI required.

T30 initial capture run45866018415/job140068924859: prior low-res 60/240/600/1200 fixtures PASS, 1200 decoded max delta400*100ns; actual audio skipped MMRESULT=2. New HD fixture FAIL generic WGC E_FAIL (1.59s); cause not established from logs. Fix exploratory fixture geometry to exact 1280x720 borderless own window at origin (no desktop mode change); log client/outer/item/desktop dimensions and actual HRESULT message. Preserve exact resolution/PTS/pixel acceptance; no silent skip or fallback. New CI required.
