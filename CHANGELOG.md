# Changelog

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
