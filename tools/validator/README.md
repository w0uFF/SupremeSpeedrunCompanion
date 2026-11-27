# B-VAL-01 — Standalone read-only moderator package inspection

Status: implemented, exact existing verifier integration and portable tests PASS on Linux. Windows execution, untrusted-parser isolation, actual media/replay analysis, native gameplay validation and competition policy NOT_RUN / blocked. Not a moderation verdict.

## Feature boundary

Python standard library, tools/validator/report.py (pure report projection over a trusted injected verifier result), tools/validate_package.py (single-file adapter, controller and human/JSON view), tools/validator/__init__.py. Reuse evidence_archive.verify(path, include_manifest=True) exactly once for each command; do not reopen manifest or payload, implement ZIP parser, recompute hashes, extract or execute members, write index, change source or invoke game. Existing run_manifest and attempt_settings_evidence/ssc_profiles checks run inside the unchanged verifier. Use current Python toolchain rather than another native reader; independent standalone feature can be omitted without affecting game, library or recorder. No owned state, configuration, thread, queue, persistent report, background job, network, credential, admission or submission. Run as a separate explicit offline command; no game/UI callback. Long verifier call inherits existing bounds and has no hard cancellation deadline. Caller chooses one closed file in a trusted, quiescent private directory; no hostile path/parent races, arbitrary untrusted filesystem or concurrent writers supported. The adapter lstat refuses nonregular source types; existing verifier retains link/reparse and file-size checks. No claim of path security. A missing file or IO/dependency failure is unavailable, not a clean report.

Developer usage from repository root (same Python interpreter as current verifier):

    python -B tools/validate_package.py path/to/closed-evidence.zip
    python -B tools/validate_package.py path/to/closed-evidence.zip --json

Output to stdout only. JSON report_version=1 is a feature-local read model, not an evidence schema, signature, persistent verdict or new archive format. Reports include verified schema, member count/payload bytes, each bounded member name/role/size/digest, schema-2 manifest digest and explicitly labeled claims: run/session IDs, origin, attempt/capture status, mode, nullable IGT milliseconds/RTA nanoseconds, track/difficulty, member presence, settings consistency. No raw verifier errors, source absolute path, settings values, account identity, full free-text warnings or paths from other metadata. Member names and selected metadata strings are untrusted user data; escaped JSON and human values, review privacy before sharing. No whole-container digest in this report; manifest digest binds inventory, not ZIP container bytes. Exact archive exchange identity remains separate.

## Assessment and error contract

The report projection checks only consumed output shape, strict types, size/count/identity agreement, bounded safe display strings and fixed unverified flags before projecting from the trusted verifier. This is defensive adapter validation, not independent validation of archive contents or authority for arbitrary externally supplied JSON. It never trusts an external claimed verdict. A fabricated report can mimic a trusted result; do not feed user JSON to inspect(). Read actual archives through the adapter.

| Verification | Meaning |
| --- | --- |
| consistent | Existing verifier successfully checked supported structure and hashes against manifest. |
| invalid_package | Known decode or validation rejection; package-level finding, not a rule violation. |
| unsupported_format | Known unsupported source type or ZIP64 locator rejection. |
| invalid_or_unsupported | Existing verifier rejected input but does not provide a typed invalid/unsupported distinction. |
| unavailable | Source, verifier, dependency or unexpected operation failure. |
| malformed_report | Consumed verifier output failed defensive boundary checks. |

Schema 1 can be structurally consistent but has no run metadata; unsupported_run_schema finding, no fabricated identity/digest. Unknown schema currently yields ambiguous rejection; cannot reliably split all existing ValueError messages into invalid/unsupported. Narrow exact known error messages are classified in adapter, no substring guessing and no raw exception text in report. Request typed shared-verifier error taxonomy later with A review; do not change existing errors now. Assessment is independent: insufficient_evidence for structurally consistent packages, review_required for filename heuristic or malformed report, not_assessed for failed verification. No confirmed gameplay violations, no CHECKS_PASSED verdict, no eligibility or local provenance. Exit 0 means only schema-2 structural consistency with no flagged filename, NOT eligible; 2 invalid/ambiguous rejection; 3 unsupported, schema1 inventory-only, review or malformed report; 4 unavailable. Missing replay is not a violation and replay-member presence (even empty) is not playable proof. Nonempty video role does not prove video or menu/start/result coverage. Fixed checks: timing, track/difficulty, capture/telemetry coverage, menu proof unverified; video/replay playback, reference baseline and gameplay integrity not assessed; competition policy, local provenance and payload safety not established. Even caller-supplied origin=local and capture_status=finalized cannot elevate these. Missing final IGT, nonempty video, identity or incomplete attempt/capture claims have explicit findings, not cheating accusations. No verified mapping from profile labels to policy.

Inspected existing submission/public/track_claims.hpp, bound_track_claims.hpp, track_interval_claims.hpp, track_coverage_claims.hpp: pure agreement of caller-supplied typed values, attempt identities, same clock epoch/frequency, intervals and contiguous bounded spans. Schema2 does not carry independently established inputs for those comparators. Do not derive them from display labels, file presence or synthetic times; no second Python comparison algorithm or fabricated positive agreement. Future reader must supply actual bound claims with explicit source and coverage; reuse existing comparators through reviewed binding. No policy or menu-proof rule change.

## Member safety finding

OBSERVED_STATIC in existing evidence_inventory.safe_name/evidence_archive.verify: bounded flat names, allowed roles, stored ZIP, count/size/CRC/hash and known schema checks; no generic content-type validation or executable-file ban. Actual controlled regression: existing exact verifier accepts inert text named payload.EXE.txt and driver.dll as diagnostic; new report returns structural consistency AND review_required, executable_looking_name per member. Review heuristic checks case-insensitive dot-delimited filename components (including double suffixes). This is not malware detection, comprehensive extension policy, nor permission to execute/import/play. Inert signature-marker bytes under opaque.bin do not trigger filename warning; payload_safety remains not_established even with no warning. No file content sniffing, decoding, extraction or execution. Misleading extensions/unknown formats, nested payloads, actual executable content, arbitrary private data and parser vulnerabilities remain unaddressed. Before untrusted import/playback activation, A/B must agree explicit versioned allowed role/name/content policies (including known inert formats, prohibited executable content and safe isolated media/replay parsing), resource/time limits, link/path binding, privacy review and fixtures. Request typed verifier failure reasons and guarded archive admission from B-0010; no edits to shared tools without authorization. Structural failure is not evidence of cheating. Preserve existing competition rules/manual fallback/optional replay.

## Executed validation

Linux x86-64 Python 3.13.5. Direct raw GitHub DNS unavailable; reconstructed exact source subset through connector and verified local Git hashes against starting tree before testing:
- tools/evidence_archive.py 2ab8d118c630a241d3d40af8375697e3b55b93cf
- tools/evidence_inventory.py 3b35d8f043601afae17ebe8b7ba4eeca1ad9d10f
- tools/run_manifest.py b7d3d40fbdc2fcc7979acc14d6459909aedc17df
- tools/baseline_snapshot.py e84883403f5a340163689b4c2c1e2856ef9d912a
- tools/attempt_settings_evidence.py eeac3207461be11e7f4c87b85bb0695b1b525322
- tools/ssc_profiles.py b7eb82b0b77f02dca91ce7a81e0c9a5ae2376ed5
- tools/archive_publish.py 4f86e884c4275c1fe0fdac1da4c8b53ab7293254
- tests/test_evidence_archive.py 6a118015d5400d9d9078e0dfc484ec30cc4a5fbe
- tests/test_run_manifest.py 9669146075c1055046bfa999a106e616dd1588de

    python -B tests/test_validator_report.py -v
    python -B -O tests/test_validator_report.py -v
    python -B tests/test_evidence_archive.py -v
    python -B tests/test_run_manifest.py -v

All exit0: new 7/7 normal and optimized, original archive7/7 and manifest/publication6/6. New suite tests actual schema2 and legacy schema1 ZIP, Unicode filename, CLI human/JSON subprocess and exact report equality, independent member SHA256, claimed local vs no provenance, zero/null, missing video/identity/IGT, optional empty replay, actual redacted attempt-settings validation, inert executable-looking and opaque marker filenames, changed payload/forged eligibility/unknown schema/truncated/ZIP64 locator, unavailable source/verifier/exception and 16 malformed output mutations plus settings digest mismatch. Source bytes and directory entries preserved by actual validator calls; tests generate/rewrite only owned synthetic scratch fixtures. No actual video/media decode or game files used. Existing original tests cover actual roundtrip/no-replace, traversal/extra member/duplicate key, compression/ZIP64, metadata and partial publication/recovery. Full repository/native/Windows/recorder and previous B controller suites NOT_RERUN this turn; exact prior files retained. All five supplied ZIP SHA256 unchanged. Candidate source/test blob hashes compared with local and remote readback. Rollback omit five additions, no persisted state. A to append separate Python CTest after applicable existing targets, Windows regression required before wider integration. G1/G2/G4/G5/audio/license/COMMUNITY-001/DEMO-TRACK-01/privacy/optionality/manual fallback/rules unchanged.
