# Changelog

## v0.0.17-p0 — concurrent own-window video and recovery inspection
- Bounded one-frame lookahead, shared test-only MF sink, direct concurrent WGC video capture, 60/240-frame decode checks, controlled gap and actual resize refusal. No gameplay or actual audio claim.
- Explicit single-run read-only archive status and native SQLite READONLY comparison/presence; no automatic repair or local-origin admission. Proposed private local admission boundary.
- Static scheduler/render separation and direct candidate IGT formatter; no authoritative finish inference.
- Windows core 36/36 each, media 5/5, capture 6 PASS/1 SKIP, storage/entry 1/1. VERIFIED_AUTOMATED: v0.0.17 PR66 rerun 45856450816/job 140035983590: reviewed upstream 3/3, SSC 2/2 including native preview, startup/runtime PASS, loads 0/0/1. Actual logs inspected. Prior v0.0.16 fixture preserved. G1 blocked, release skipped.

## v0.0.16-p0 — verified archive indexing and timestamped own-window capture
- Add recoverable offline schema-2 archive admission, no-replace publication, exact manifest digest linkage and native index bridge. Imports remain nonlocal regardless of claimed origin; synthetic fixture explicit; local producer not enabled.
- Add bounded timestamped WGC sequence handoff and video-only worker encoding/independent decode check, with explicit hold/tail policy and coverage limitations.
- Trace nominal fixed-step scheduler and result conversion statically; no authoritative IGT claim or native hooks.
- Windows core 35/35 each, media 5/5, capture 4 pass/1 skip, storage and inert DLL 1/1 each. Intermediate compile warning fixed; v0.0.16 PR66 rerun pending at preparation. G1 remains blocked, no release.

## v0.0.15-p0 — native run library and immutable settings evidence
- Add independently tested WinSQLite adapter with insert-only record semantics, exact schema identity, separate Unicode labels/favorites, bounded history and comparable local PB queries. Preserve unknown database versions and schemas; no automatic migration.
- Bind a redacted immutable C++ attempt settings snapshot to run/session IDs and optional schema-2 archive diagnostic; independent Python verification and negative fixtures. No eligibility claim or live recording.
- Static research identifies per-player candidate timing accumulator, reset and conditional completion paths; proposed observation boundary documented. G1 remains blocked.
- Correct SQL null final IGT constraint exposed by initial native regression. Final Windows core 33/33 each, media 5/5, capture 3 pass/1 skip, storage 1/1, inert DLL 1/1. Prior PR66 baseline fixture passed; v0.0.15 manifest rerun pending at preparation. No release.

## v0.0.14-p0 — reviewed loader fixture and native launcher configuration
- Actual PR66 source-loader fixture passed, including manifest parsing, initializer lifecycle, gating and Rust-serialized launcher state consumed by native SSC preview; no game or actual launcher UI execution.
- Native bounded read-only launcher JSON adapter, Win32 preview, representative cross-language conformance, immutable owned attempt settings.
- Single-profile import/export with local path stripped, collision refusal and optimistic stale-editor protection; CLI and offline GUI controls.
- Trace original-game candidate row writer and alternate source path; not authoritative IGT or finish evidence.
- Windows core 30/30 Debug and Release, media 5/5, capture 3 pass/1 skip, WinSQLite 1/1, inert DLL 1/1. G1 still blocked on installed loader identity and disposable game baseline/rollback. No release.

## v0.0.13-p0 — native profile persistence and launcher fixture preparation
- Add native canonical schema-1 profile codec, lexical path policy and Win32 private file store, Python/C++ interoperability and persistence tests.
- Correct offline launcher gate to require order and enablement; add offline Tkinter profile editor and fingerprint-gated PR66 source fixture.
- Rewrite README for players, clarify optionality and current competition rules; document launcher UI proposal and G1 rollback procedure.
- Windows core 26/26 Debug and Release, media 5/5, capture 3 passed/1 skipped, storage 1/1, independent inert DLL 1/1. Actual upstream fixture pending at documentation preparation. No game integration or release.

## v0.0.12-p0 — community-loader candidate and configuration prototype
- Verify v0.0.11 final CI; inspect supplied PR66 loader source and private community references.
- Add inert opt-in x86 cdecl initializer and launcher manifest with default-disabled loading; no game effects; independent Windows host test passed.
- Add versioned SSC profile store, offline CLI, settings validation, explicit launcher precedence and 21 declarative controls. Seven local profile tests and initial Windows core 22/22 each passed. Add tested C++ typed settings/controller port with injected storage/path policy and copy snapshots; final Windows regression 23/23 each passed.
- Document native menu/finish leads, G1 procedure and limitations; no actual launcher UI, native configuration consumption or release.

## v0.0.11-p0 — capture primitives and isolated platform experiments
- Verify v0.0.10 Windows baseline. Add bounded BGRA-to-NV12 conversion and source audio discontinuity/position counters, independent C++17 tests.
- Verify actual own-window pixel readback, NV12 conversion and synthetic-timestamp encode/decode; extend synthetic A/V to 30 seconds with archived quality sidecar.
- Add own-process controlled-signal audio sampling probe; hosted test skipped at playback device open (MMRESULT=2), no actual audio claim.
- Add WinSQLite standalone system-library feasibility (3.53.4, Win32 test passed); initial SDK header absence corrected, separate storage job.
- Document static high-time persistence reader/writer candidates. Final Windows core 21/21 per configuration, media 5/5, capture 3 passed/1 skipped, storage 1/1. No native or release approval.

## v0.0.10-p0 — recording quality and clock mapping
- Add separate own-window WGC metadata and own-process WASAPI activation probes; Windows feasibility verified (WGC frame metadata, WASAPI activation only).
- Add opt-in paced 10-second synthetic A/V test and archive quality sidecar; Windows test verified (10 seconds, 500 packets, quality sidecar archive).
- Verify v0.0.9 final Windows CI. Add per-stream worker handoff counters, gap and queue high-water statistics, bounded QPC timestamp mapping and pure tests. Additional static result-presentation evidence; no native timing claim.

## v0.0.9-p0 — offline recorder lifecycle and evidence publication
- Verified final v0.0.8 Windows core and synthetic video CI.
- Added bounded recorder worker, per-stream timestamp checks, drain/cancel/failure lifecycle and separate drop counters; fake sink tests only.
- Added C++ run repository port and comparison helper, no production adapter.
- Added opt-in schema 2 run manifest, canonical digest and verified partial publication without overwrite, retaining schema 1 support.
- Extend opt-in Windows synthetic media probe to test AAC mux/decode and coarse A/V endpoint coverage; synchronous AAC/H.264 mux/decode verified. Added standalone async worker/MF adapter; verified 24 packets finalized and both streams decoded. Synthetic media-to-archive integration verified on Windows. Additional static presentation copy/discard evidence.
- Isolated Linux tests pass; 19 portable CTest targets configured; Final exact code/test revision verified on Windows: 19/19 Debug and Release, 3/3 media. No release or native runtime approval. No runtime or release authorization.

## v0.0.8-p0 — offline archive and media feasibility
- Verify v0.0.7 Windows x86 logs, 13/13 Debug and Release.
- Add bounded stored ZIP archive creation and independent verification; no eligibility verdict or extraction.
- Add injected C++ preference storage port/controller, candidate replay split comparison and additional static result-path evidence.
- Add opt-in standalone Windows Media Foundation synthetic video probe and separate CI job; first compilation fixed for MSVC C4245, hosted synthetic H.264 MP4 encode/decode test passed (12/12 frames).
- Reject ZIP64 footer bypass with a seventh archive test. Linux Debug/Release/UBSan 16/16; Windows core Debug/Release 16/16 before archive follow-up. No game runtime or release gate changes.

## v0.0.7-p0 — independent timing, local data and evidence foundations
- Verify v0.0.6 Windows x86 logs, 9/9 Debug and Release.
- Add injected monotonic interval and QPC/steady-clock platform adapter, isolated from game events.
- Add standalone SQLite preference/history/PB reference store, bounded evidence inventory and non-dispatchable Speedrun.com request preview.
- Add four CTest targets; Linux Debug/Release/Debug+UBSan 13/13. CI floor thirteen.
- Document additional presentation constant evidence, replay pre/post-roll coverage and capture feasibility. No native hooks, capture, network dispatch or release approval.

## v0.0.6-p0 — baseline evidence and native object lifetime research
- Verify v0.0.5 Windows x86 CI logs (8/8 Debug and Release).
- Map menu list registry producer, allocation/destruction boundaries, default selection reads and borrowed result presentation object; document mutating result consumer and remaining unknowns.
- Add bounded read-only fixed-file baseline snapshot and comparison with seven synthetic tests; Linux Debug/Release 9/9 targets.
- Document proposed upstream extension lifecycle and explicit baseline/G1 procedure; CI floor nine. No native activation or release approval.

## v0.0.5-p0 — offline native research
- Verify baseline Windows x86 Debug and Release CI logs (7/7 each).
- Add read-only candidate menu/import xref scanner and comparison; document menu registration, configuration, lifecycle and clock findings.
- Add bounded replay position summary with terminal exclusion and null nonfinite markers; 55/61 reference layouts recognized, no geometry or timing verdict.
- Add seven synthetic tests, 8/8 local CTest targets in Debug and Release; raise CI floor to eight.
- No native integration, runtime capabilities, gameplay, schema or release-approval changes.

## Repository bootstrap (source baseline v0.0.4-p0)
- Import verified baseline build source, tests, tools and compatibility profile without copying game or upstream binaries.
- Add Windows x86 Debug/Release CI, release approval gate and diagnostic-only pre-release manifest.
- Adopt Supreme Speedrun Companion public name; retain existing internal identifiers.
- No native integration changes.

## v0.0.4-p0 — source discovery and offline integration feasibility
- Inspected owner-supplied Display Config source and compared template/release exports; documented lifecycle and source limitations.
- Added independent bounded replay header and reference asset diagnostics, reproducible scan and synthetic fixtures.
- Added isolated C ABI mock with C caller regression; no native hooks or loader changes.
- Recorded reference scan, build/test logs, upstream license constraints and optional Windows baseline extensions. No schema or installation migration.

## v0.0.3-p0
- Record approved player workflow and optional official API submission requirements; further static proxy entry and file-overlay script evidence.
- Add isolated synthetic category, setup, session, capture-boundary and submission-review models with regression tests; no production integration or native capability.
- Preserve existing preflight, observation schema and diagnostic. No game changes.

## v0.0.2-p0
- All source inputs accessible, fingerprints and PE inventories recorded; original keyboard patch reconstruction and import preservation verified.
- Partial Display Config proxy forwarding and engine boundaries documented. No loader replacement or hooks.
- Read-only reference combination preflight and pure fail-closed admission prototype with tests. No observation contract changes, gameplay changes or supported runtime capability.

## v0.0.1-p0
Initial private discovery: archive fingerprints, static engine candidate map, portable observation contract, offline diagnostic MVC and module lifecycle prototype. No changes to gameplay, files, loader or Windows installation.

T19 validation: exact source/test revision Windows 20/20 Debug and Release, 4/4 media and 2/2 capture feasibility. Corrected wrapper syntax and isolated WGC C++20 compile. Added locally tested standalone SQLite feasibility source; no new runtime dependency.

T21 final validation: Windows 23/23 Debug and Release, media 5/5, capture 3 pass/1 skip, storage 1/1, inert entry 1/1. OnlinePlay result-row candidate traced to matching original game build metadata; no authoritative IGT claim.

T25 final fixture follow-up: v0.0.16 reviewed PR66 source run 45816213574/job 139896508310 on code revision 95d197430ade7544ab429bc61c471e59ace96262 completed successfully. Actual logs: upstream 3/3, SSC 2/2 (including native preview from upstream state), startup/runtime fixture PASS, loads 0/0/1 for disabled/requested, enabled/not requested and enabled/requested. This supersedes T25's pending fixture note, not G1 or installed-game verification.

## v0.0.18-p0 — candidate (T27)
- Extend isolated concurrent capture fixture to 600 frames; preserve short and long fixtures and add simulated device-removal cancellation.
- Extend portable sink failure coverage; add synthetic-only admission journal model and recovery tests. No local admission or durable journal.
- Windows validation pending; no game integration or release.

T27 first candidate ef335294a03a1746b8b3d52d1e48c660b09aa899 Windows CI 45860359753 passed: core 37/37 both configurations, media 5/5, capture 7 PASS/1 SKIP, storage/entry 1/1. 600 actual own-window frames decoded with max PTS error 0.04 ms. Follow-up tests and explicit synthetic journal capture completeness/reconciliation pending CI; no game integration or promotion yet.

T27 final validation: T27 final implementation 83c89e0336cc781bf2fa70cd22754f89c2acf31a; run 45861739545: Windows x86 Debug/Release 37/37 each (140054226996), synthetic media 5/5 (140054227147), capture 7 PASS/1 SKIP (140054226510), storage 1/1 (140054226975), inert entry 1/1 (140054227043), release skipped (140054925770). Actual logs inspected. 600-frame LiveCapture 600 600 0 598; LiveHandoff 600 0 214150000 211243750 2906250 1 0 0; decoded 600, max PTS delta 400*100ns. Resize rejected 386x293; injected removal cancelled; actual device loss/recovery NOT_RUN, audio MMRESULT=2 BLOCKED. PR66 exact revision run 45861731467/job 140054200461: reviewed archive fingerprint gate, upstream 3/3, SSC 2/2 including native preview, startup fixture PASS, load counts 0/0/1. Actual logs inspected. Prior 45861460575 also passed, but its Windows core failed version mismatch; corrected generator and CMake equality guard. All follow-up regression tests passed; capture completeness and recovery reconciliation remain synthetic-only. No installable release.

## v0.0.19-p0 — candidate (T28)
- Synthetic-only append-only phase snapshots with bounded strict codec, no-replace publication and explicit retry after interrupted writes. No local authority, production index or automatic repair. Six local tests passed including three subprocess termination points.
- Extend own-window test to 1200 selected frames and bounded latency/CPU/wall/drain reporting; retain previous regressions. Pure summary test passed locally. Windows integration pending.
- Correlate original checkpoint, Finishpoint and On_Screen_Display RTTI/vtables; no runtime hooks or official timing conversion. G1 and actual audio remain blocked.

T28 final: T28 verified implementation abff44ba0bfa8e3b7f7b3a4fdea2f784c37ef72b, Windows run 45862955701; final evidence head 06ba05517db0edcd715c8f56d0240b78e39579c7 run 45863490997 actual logs: Debug/Release 39/39 (140060273173), media 5/5 (140060273236), capture 8 PASS/1 SKIP (140060272857), storage/entry 1/1 (140060273131/140060273279), release skipped. 1200-frame repeat LivePerformance 1200 41239585 16875000 337 476 591 1323 5478; LiveHandoff 1200 0 411962500 409212500 2750000 1 0 0; max PTS delta 400*100ns. PR66 run 45862722286/job 140057622119 upstream3/3, SSC2/2, startup PASS, load counts 0/0/1. Full compare 26bf9105a7446e2645c792ea210eaa43165fa82c -> implementation changed only Markdown, tests/run_live_video.py and tools/media/wgc_probe.cpp; neither capture probe is enabled or built by the PR66 workflow. CMake, workflow, fixture script, generator, Rust manifest tests, mod.json, native entry, config preview blob hashes match exactly; no other dependency changed. Implementation -> evidence head changed only Markdown. Fixture-relevant equivalence accepted. Actual audio MMRESULT=2 and G1 remain BLOCKED. No game install, actual device loss, local origin or public release.

## v0.0.20-p0 — candidate (T29)
- Synthetic journal/archive/index recovery coordinator, read-only inspection and explicit re-inspected journal-only reconciliation; preserve conflicts. New native index/archive interruption fixture; local pure tests passed.
- 720p synthetic NV12 to H264 worker encode/decode fixture with queue payload bounds and process resource/latency measurement; no game/high-res WGC claim. Windows candidate CI pending.
- RTTI confirms AI_Player inherits Player and shares result callback; further checkpoint count and row-value research. G1/audio blockers unchanged; no local authority or public release.

T29 first Windows candidate: T29 exact candidate 45a348df2fb1241a9ced5697aca5fe234d33a285 run 45864597707 actual completed-job logs: core Debug/Release41/41 (140064063817; pure recovery and actual native archive/index interruption tests PASS both), media6/6 (140064063880), storage/entry1/1 (140064063796/140064063779), release skipped. HDPerformance 250 9997117 17187500 585 630 708 778 14095 88203264 16175104 4530176 433404 1 11059200; 250 decoded H264 1280x720 with PTS/luma validation. Units count, wall us, process CPU 100ns, producer median/p95/p99/max us, drain us, peak/current working set bytes, current private commit bytes, MP4 bytes, queue high-water, queue payload cap bytes. Synthetic moving test pattern only; not actual high-res capture or gameplay performance. Capture job140064063406 and PR66 run45864274427 still in progress at evidence preparation. G1 blocked and no promotion.

T29 VERIFIED_AUTOMATED: implementation 45a348df2fb1241a9ced5697aca5fe234d33a285 run45864597707: Debug/Release41/41 (140064063817), media6/6 (140064063880), capture8 PASS/1 SKIP (140064063406), storage/entry1/1 (140064063796/140064063779), release skipped. HDPerformance 250 9997117 17187500 585 630 708 778 14095 88203264 16175104 4530176 433404 1 11059200. Actual 1200-frame LiveHandoff 1200 0 406025000 403118750 2906250 1 0 0; max decoded PTS delta400*100ns. Final documentation head dc9012a1d653ba0704614d17653949e20ad3720c run45864960360: core41/41 both (140065294341), media6/6 (140065294432), capture8 PASS/1 SKIP (140065294396), storage/entry1/1 (140065294260/140065294561), release skipped. Actual logs inspected. PR66 run45864274427/job140062944148 upstream3/3 SSC2/2 startup PASS loads0/0/1; complete comparison bf6f0af6d2b3b135cb3228b9ff2952cfb91782c1 -> implementation -> evidence head Markdown-only, so exact fixture-relevant sources unchanged. Audio MMRESULT=2, G1 BLOCKED; no game, actual device recovery, local authority or release.

## v0.0.21-p0 — candidate (T30)
- Explicit synthetic archive publication/alias cleanup/index retry plans with fresh observation, immutable journal binding and separate reconciliation. Native process-interruption tests registered; local pure plan tests PASS.
- Actual representative-resolution own-window capture fixture with bounded lookahead/queue, 720p encoder and independent decode, resource measurements. Candidate Windows CI pending.
- Replay-related object and replay-duration HUD distinguished from controller identity and raw IGT; no runtime claims.

T30 corrected capture VERIFIED_AUTOMATED: implementation dee725313bf2accf6959b4075ed8d3bf0a7474a7 run45867815727: core43/43 both (140075092193), media6/6 (140075092491), capture9 PASS/1 audio SKIP (140075091898), storage/entry PASS. Actual HDDisplayOriginal 1024 768 32 64 0; supported 1280x720 dynamic mode test/change return0; exact current and original restore confirmed. HDWindowGeometry 1280 720 1280 720 1280 720 1280 720; LivePerformance 240 9732221 36875000 6889 10067 11457 12039 15314; LiveResources 1280 720 1280 720 115347456 41922560 12881920 39202 11059200; LiveHandoff 240 0 96806250 93900000 2906250 1 0 0; 240 decoded, max PTS delta400*100ns. Repeat implementation 0769a7482fc01d20ef0f1a3ab8736ec9c6fb2b73 run45867955381: core44/44 both (140075586635; display mock contract PASS), media6/6 (140075586570), capture9 PASS/1 SKIP (140075586630); actual mode test/change/restore success, LivePerformance 240 9233235 33437500 6423 7139 7792 8791 15453; LiveResources 1280 720 1280 720 115507200 41881600 13111296 39162 11059200; LiveHandoff 240 0 91493750 88587500 2906250 1 0 0; decoded240, max PTS delta400*100ns. Actual logs inspected. Earlier 1024x768 desktop failures at exact 120 selected now reproducibly resolved by supported larger mode; exact GDI/compositor clipping mechanism not independently isolated. CI-only opt-in, no persistent registry mode, single monitor, bounded candidates and restore in parent finally. Hard termination of parent cannot guarantee restoration; hosted runner disposable, normal user system never opts in automatically. All 720p acceptance preserved, no actual gameplay claim. Portable Python 23/23 (18 previous +5 mock display), C++ latency summary PASS, exact two new file blobs matched. Audio MMRESULT=2 and G1 BLOCKED. Updated CMake PR66 run45867949125 pending; no promotion until reviewed.
