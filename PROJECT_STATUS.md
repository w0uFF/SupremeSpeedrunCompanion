# Project status

Revision v0.0.15-p0 — native run-library adapter and optional attempt-settings evidence member verified in independent Windows fixtures. G1 remains BLOCKED.

## Verified revisions and evidence
- Baseline 1c548dce1e23c9a2a0e09a1fea2002bc1b11ca37, Windows run 45802187280: 30/30 core Debug and Release, media 5/5, capture 3 PASS/1 SKIP, storage 1/1, inert DLL 1/1, release skipped. Repeated exact PR66 source fixture 45802187381/job 139847990756 PASS: 3 upstream + 2 SSC tests, upstream startup fixture, SSC 0/0/1 actual load counts.
- T24 code/test revision 1d3a55591e49f4e7acac55da84d967e01448d763, Windows run [45803303956](https://github.com/w0uFF/SupremeSpeedrunCompanion/actions/runs/45803303956): core 33/33 Debug and Release (139851861775), media 5/5 (139851861738), capture 3 PASS/1 SKIP (139851861560; playback MMRESULT=2), storage 1/1 (139851861051), independent inert DLL 1/1 (139851861420), release skipped (139852482918). Intermediate 45803020180 and 45803197801 failed new SQL CHECK for null final IGT of unfinished run; independently reproduced and fixed, explicit DB-level regression passes final revision. New attempt binding/archive tests passed in intermediate Debug and final both configurations.
- v0.0.15 manifest PR66 rerun 45803303975/job 139851860745 still in progress at documentation preparation; previous exact reviewed source fixture passed. Do not infer result until actual logs. No loader implementation changes.
- All 18 immutable supplied references verified by sha256sum -c before/after static research. Original DLL hash unchanged. No game execution, source redistribution, replacement proxy, upstream changes or release.

## Implemented
- Native WinSQLite run-library adapter, explicit new/private runs.sqlite, version 1 and application ID, exact schema checks; validated insert-only records, duplicate identical/conflict, nullable IGT/RTA, separate Unicode annotations, recent history and comparable local PB. Independent Python SQLite readback, process reopen, unknown schema/version preservation, owner-thread tests. Derived index, caller supplies manifest digest, no archive-to-index admission or game wiring. No migration from separate Python prototype.
- Optional canonical redacted attempt-settings.json diagnostic member in existing schema-2 archive; C++ AttemptBinding owns fixed run/session IDs and immutable settings. Python validator checks member SHA-256, IDs, schema, settings and private path redaction; mismatched rehashed members rejected. Legacy archives without member supported. No eligibility, actual media validity or observed gameplay claim.
- OBSERVED_STATIC: original Player and AI_Player RTTI vtables share result writer and getter; timer subobject allocation, reset, conditional +0.01 accumulator, start/finish candidate flag paths traced; same handler can receive zero argument on incomplete checkpoints, so not a unique finish. See ENGINE_MAP.md and RACE_OBSERVATION_PLAN.md. No native observations or calls enabled.

## Blocking gates and next action
G1 still requires owner disposable Windows baseline, actual supported loader provenance, installed-disabled discovery/launch/exit/rollback after review. No owner baseline supplied this turn. G2/G3/G4 for actual game not run. No installation authorized. Existing rules, optional companion and player-first README retained. Licensing pending; public releases disabled.
Next P0-T25: verify final CI and PR66 rerun, collect baseline if provided, continue independent archive-to-index verified ingestion/provenance and recoverable pending/finalized records, continuous own-window capture timestamp integration and original timer/display correlation research. Do not enable hooks until G1; no official IGT or rule approval claim.

T25 candidate v0.0.16: standalone verified archive admission and native index bridge, imported/synthetic only, recoverable partial/final, no local admission. Windows tests pending.
