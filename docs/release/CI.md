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
