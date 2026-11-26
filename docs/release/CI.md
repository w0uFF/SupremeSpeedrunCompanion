# Windows CI and development releases (P0-CI01)

Decision: official GitHub-hosted windows-latest runner, Visual Studio default generator, -A Win32, Debug and Release, standard-library Python tests. No self-hosted runner or Actions artifact storage. Checkout pinned to v4.2.2 commit. Record the runner/toolchain versions from actual logs; windows-latest is a moving image, not a fully reproducible compiler environment.

The workflow runs on main pushes and pull requests; read-only build job. It requires nineteen or more CTest targets and fails on configure, build or test failure. A source-less bootstrap commit skips compilation explicitly. Release job only runs on main push after CI passes and SSC_RELEASE_APPROVED equals true, with contents:write scoped to that job. It rebuilds and tests Release and checks the LICENSE file exists and the diagnostic PE machine is x86. Packages only the offline diagnostic and license/readme, not game files.

To enable publication after licensing and owner approval: repository Settings > Secrets and variables > Actions > Variables > New repository variable: SSC_RELEASE_APPROVED=true. The next successful main push publishes a development pre-release tagged dev-<full commit SHA>. The release manifest is schema 1 with channel=development, installable=false, loader_gate=blocked, architecture=x86, archive name, checksum and source commit. GitHub Releases assets, not Actions artifacts. Never mark as latest stable. Re-running the same commit will not replace an existing release; duplicate release creation fails safely. Disable publication by removing or setting the variable false. Existing releases remain unchanged.

This is not a launcher update contract or validated installer. Upstream review, stable channel approval, license compliance, manifest evolution and rollback tests required before allowing a launcher to install anything. Protect main against unreviewed contributions when practical. Never use pull_request_target to execute untrusted code with release credentials.

Baseline run 45751578053 was verified from actual logs (Windows x86 7/7 Debug and Release; release skipped). v0.0.5 raises the expected test floor to eight. No change to release gating or artifact policy.

Run 45752930744 verified from logs (v0.0.5 Windows x86, 8/8 Debug and Release, release skipped). v0.0.6 requires nine CTest targets. Release permission and gating unchanged.

Run 45753602966 verified from actual logs (v0.0.6 Windows x86, 9/9 Debug and Release, release skipped). v0.0.7 requires thirteen CTest targets including isolated QPC/steady-clock, SQLite reference store, evidence inventory and API preview. No change to release gating or artifact policy.

T17: 16 portable targets required. Separate read-only media-probe job configures SSC_MEDIA_PROBE=ON on Win32, builds only p0_media_probe Release, runs named CTest verbose. Its failure is visible but does not publish or alter any release. Offline diagnostic release job remains gated by license/approval and core CI, no media binaries packaged. Prior run 45757144179 actual logs: 13/13 Debug and Release, release skipped.

T18 portable floor 19. Last v0.0.8 final run 45759479502 confirmed 16/16 each and media 1/1. Approval and artifact policy unchanged.

T18 media job runs three named opt-in tests: video-only, synchronous A/V and asynchronous A/V. Core 19/19 each and all three media tests verified on 45777095746. Final wrapper adds synthetic MP4 archive verification. Approval gate and no-artifact policy unchanged.

Final T18 exact code/test commit ab0bcf82ededc13ac2271334b072e4ca730aa01a verified from actual run 45777278765 logs: 19/19 Debug and Release, 3/3 media, release skipped. Subsequent documentation-only commit records evidence without changing build/test inputs.

T19: 20 portable targets, separate media job, unchanged approval and artifact policy.

Final T19 code/test commit 8f005bda4557498bc1d3ab0e7c77e0963b783fb9 verified in run 45782226376 (20/20 per core configuration, 4/4 media, 2/2 capture feasibility, development release skipped). Standalone WGC C++20 target only, SSC_CAPTURE_PROBES=ON. CTest exit 77 is skip/BLOCKED, never validated capture. No Actions artifact or release changes. Storage probe not part of CI/default build.

T20 final exact code/test commit eda75eb2adc1d7ed319526b8a083696658a7bbf0 run 45783962593: core 21 targets per configuration, media five, capture three passed/one skipped (audio sampling), storage one passed. Separate storage-probe job loads system WinSQLite; capture opt-in also builds media for captured pixels. SKIP_RETURN_CODE 77 is BLOCKED, not PASS. Approval and no-artifact policy unchanged. No release published.

T21 core floor 22; separate community-entry job builds only inert SSCNative and independent test host with SSC_NATIVE_CANDIDATE=ON. No real launcher or game loaded. No artifacts/releases of DLL; existing development release gate unchanged.

T21 final code/test revision 478a33a790fdebc24c0f3788f10b1a00fa5dbdeb run 45795828198: 23/23 core each, 5/5 media, 3 capture pass/1 skip, 1/1 storage, 1/1 inert native entry. No actual community loader/game execution. Release skipped, no artifact publication.

T22 Windows core floor 26; actual native FileStore and Python/C++ conformance. Separate reviewed PR66 source fixture CI (read-only repository, exact fingerprint gate, no artifacts, synthetic host). Source download failure/mismatch is BLOCKED not PASS even if workflow succeeds. No public release authorization.

T23 Windows core floor 30; final code revision ca9637c90d3ed72abc6701f59f3699b7d9e996e9, run 45801293414 all core/media/storage/entry pass, capture 3 PASS/1 SKIP; release skipped. Extended PR66 run 45801293718/job 139844872583 actual source fixture PASS including 3 upstream + 2 SSC tests, actual Rust serializer/native preview and runtime 0/0/1 gating. No upstream artifact/source redistribution, no license approval or public release.

T24 floor 33 core Windows targets, includes native WinSQLite repository and independent settings archive binding. Final code 1d3a55591e49f4e7acac55da84d967e01448d763 run 45803303956: core 33/33 each, media 5/5, capture 3 PASS/1 SKIP, storage 1/1, inert DLL 1/1; release skipped. Prior failed DB constraint corrected. PR66 baseline repeat 45802187381 PASS actual 3 upstream/2 SSC tests and runtime; new manifest run 45803303975 pending at preparation. No release gating or artifact storage change.

T25 floor 35 core Windows targets, capture probe regex includes timed_video. Final code 95d197430ade7544ab429bc61c471e59ace96262 run 45816213715 core 35/35 both, media 5/5, capture 4 PASS/1 SKIP, storage/entry 1/1, release skipped. Prior C4456 fixed without relaxing warnings. PR66 v0.0.15 run 45803303975 PASS, v0.0.16 run 45816213574 pending at preparation. No public release or artifact upload.

T25 final fixture follow-up: v0.0.16 reviewed PR66 source run 45816213574/job 139896508310 on code revision 95d197430ade7544ab429bc61c471e59ace96262 completed successfully. Actual logs: upstream 3/3, SSC 2/2 (including native preview from upstream state), startup/runtime fixture PASS, loads 0/0/1 for disabled/requested, enabled/not requested and enabled/requested. This supersedes T25's pending fixture note, not G1 or installed-game verification.

T26 floor36 core, capture seven registered (six PASS/one audio SKIP), media5/5, storage/entry1/1, release skipped. Exact code abfac10ed764e1004978a5c9b9f2b42110fa51dd run 45856451103. v0.0.17 PR66 run 45856450816/job 140035983590 PASS actual upstream 3/3, SSC 2/2 including native preview, startup/runtime, load counts 0/0/1. No actual installed game/launcher validation. No artifact or approval policy change.

T27 candidate floor37 and capture regex includes live_extended_video; release gate unchanged. Documentation head run 45857509889 logs verified (36/36 each, media5/5, capture6 PASS/1 SKIP, storage/entry1/1, release skipped). T27 candidate CI pending.

T27: permit PR66 fixture on P0 validation branch pushes with unchanged path filters and read-only permissions, to verify versioned manifest before main promotion. No public release, artifact or upstream policy changes.

T27 final implementation 83c89e0336cc781bf2fa70cd22754f89c2acf31a; run 45861739545: Windows x86 Debug/Release 37/37 each (140054226996), synthetic media 5/5 (140054227147), capture 7 PASS/1 SKIP (140054226510), storage 1/1 (140054226975), inert entry 1/1 (140054227043), release skipped (140054925770). Actual logs inspected. 600-frame LiveCapture 600 600 0 598; LiveHandoff 600 0 214150000 211243750 2906250 1 0 0; decoded 600, max PTS delta 400*100ns. Resize rejected 386x293; injected removal cancelled; actual device loss/recovery NOT_RUN, audio MMRESULT=2 BLOCKED. PR66 exact revision run 45861731467/job 140054200461: reviewed archive fingerprint gate, upstream 3/3, SSC 2/2 including native preview, startup fixture PASS, load counts 0/0/1. Actual logs inspected. Prior 45861460575 also passed, but its Windows core failed version mismatch; corrected generator and CMake equality guard. Final manifest and generator synchronized; CMake version consistency assertion added. PR66 branch path filters include generator and CMake, unchanged read-only permissions. No release approval or artifact policy change.

T27 exact 83c89e0336cc781bf2fa70cd22754f89c2acf31a passed Windows 45861739545 and PR66 45861731467; merge baseline 5b26d230577d1cef0f9963eac8d275935dcd347d, docs-only tree. T28 candidate floor 39, capture regex includes live_marathon_video; manifest, CMake and generator 0.0.19-p0, existing conformance assertion preserved. No release/artifact permission or dependency change. New CI pending.

T29 candidate raises core floor to41 and includes media hd720_probe in existing regex; capture suite unchanged. Version metadata aligned at0.0.20-p0; PR66 triggered by manifest/CMake/generator changes. No release approval or permissions change. Candidate CI pending.

T30 core floor43, capture regex includes live_hd720_video; existing media6, release permissions/gate unchanged. CMake/manifest/generator synchronized at0.0.21-p0. Candidate CI pending.

T30 corrected capture VERIFIED_AUTOMATED: implementation dee725313bf2accf6959b4075ed8d3bf0a7474a7 run45867815727: core43/43 both (140075092193), media6/6 (140075092491), capture9 PASS/1 audio SKIP (140075091898), storage/entry PASS. Actual HDDisplayOriginal 1024 768 32 64 0; supported 1280x720 dynamic mode test/change return0; exact current and original restore confirmed. HDWindowGeometry 1280 720 1280 720 1280 720 1280 720; LivePerformance 240 9732221 36875000 6889 10067 11457 12039 15314; LiveResources 1280 720 1280 720 115347456 41922560 12881920 39202 11059200; LiveHandoff 240 0 96806250 93900000 2906250 1 0 0; 240 decoded, max PTS delta400*100ns. Repeat implementation 0769a7482fc01d20ef0f1a3ab8736ec9c6fb2b73 run45867955381: core44/44 both (140075586635; display mock contract PASS), media6/6 (140075586570), capture9 PASS/1 SKIP (140075586630); actual mode test/change/restore success, LivePerformance 240 9233235 33437500 6423 7139 7792 8791 15453; LiveResources 1280 720 1280 720 115507200 41881600 13111296 39162 11059200; LiveHandoff 240 0 91493750 88587500 2906250 1 0 0; decoded240, max PTS delta400*100ns. Actual logs inspected. Earlier 1024x768 desktop failures at exact 120 selected now reproducibly resolved by supported larger mode; exact GDI/compositor clipping mechanism not independently isolated. CI-only opt-in, no persistent registry mode, single monitor, bounded candidates and restore in parent finally. Hard termination of parent cannot guarantee restoration; hosted runner disposable, normal user system never opts in automatically. All 720p acceptance preserved, no actual gameplay claim. Portable Python 23/23 (18 previous +5 mock display), C++ latency summary PASS, exact two new file blobs matched. Audio MMRESULT=2 and G1 BLOCKED. Updated CMake PR66 run45867949125 pending; no promotion until reviewed.

T31 candidate expected core46, media6, capture11 with separate extended HD case; CMake/manifest/generator 0.0.22-p0. No release permissions or approval change. Local portable 33/33, strict C++ summary PASS. Native/capture and exact PR66 pending.

T32 expected core47/media6/capture12; repeat case requires two successful full extended HD children. Version CMake/manifest/generator synchronized0.0.23-p0; no permission/release gate changes.

T33 expected core48/media6/capture13. Additional lifecycle timeout440s and capture job15min to allow 3 clips; previous case timeouts/acceptance, release permission and gate unchanged. CMake/manifest/generator0.0.24-p0.

T34 expected core49/media6/capture14; new fault lifecycle CTest timeout480s, capture job20min, version metadata0.0.25-p0; existing gates unchanged.

T35 expected core50/media6/capture15; add write-failure case330s CTest and child210s, capture job20min retained. Version CMake/manifest/generator0.0.26-p0. Release permissions and gates unchanged.
