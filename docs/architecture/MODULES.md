# Module boundaries (P0-T17)

Current source-level dependency map, offline only. Bootstrap host owns module start/stop and admission; bootstrap diagnostic depends on observation port and diagnostics query/controller/view. EngineBridge unavailable source and offline admission expose zero native capabilities; extension C mock isolated. No game integration.

- categories: catalog and plan validation; no dependency on engine.
- player_setup: loadout, Availability, recording intent/proof. PreferencesController depends on Availability and injected PreferencesStore; test adapter only.
- session: consumes explicit synthetic ordered events and category catalog; owns attempt state, no native events.
- capture_boundary: ledger of manual/automatic acquisition/finalization ownership; no acquisition backend.
- timing: IClock and independent interval; infrastructure QPC/steady_clock; no implicit race events.
- submission: draft/review with category and player_setup proof; no transport or credentials.
- tools: Python reference library (SQLite), evidence_inventory (uses baseline link check), evidence_archive (uses inventory), replay header/positions/comparison, request preview. Not linked to bootstrap; no shipped Python/SQLite decision.
- media probe: standalone opt-in Windows executable, COM/MF lifetime local to process, synthetic frames only, no game access.

No feature depends on engine offsets. All new code removable with its isolated test targets. Offline ports are owner-thread synchronous unless explicitly stated, not safe to invoke blocking I/O on the game thread. Native bootstrap composition, backend cancellation, runtime dependencies and actual capability graph remain blocked by G1/G2.

T18 additions: recorder Worker owns bounded FIFO and worker-thread Sink lifetime calls; independent from capture_boundary ledger and timing port. run_library Repository source-level interface and pure comparison; no persistence adapter. tools/run_manifest and archive_publish extend evidence_archive with optional schema 2 and explicit verified partial publication/recovery. All offline, no bootstrap wiring.

T18 follow-up: optional standalone synthetic media probe consumes recorder Worker and implements its Sink; no production dependency on Media Foundation added to bootstrap. The Python probe wrapper exercises experimental archive_publish after successful synthetic encode/decode. No live capture.

T19: recorder quality helper is pure, used by Worker; QPC conversion requires explicit common epoch, no additional engine dependency.

T19 isolated optional WGC and process-loopback probes have no bootstrap dependencies. Synthetic media wrapper exports a hashed quality diagnostic separate from eligibility. Standalone local SQLite experiment not linked to any module; Repository port remains unchanged.

T20: recorder pixel_conversion.hpp and audio_observation.hpp are pure C++17 helpers. Isolated WGC probe uses pixel conversion and temporary captured-frame fixture to exercise existing MF worker; separate own-process sampling probe consumes audio observation. WinSQLite probe uses explicit system-library API binding, not a production Repository. No bootstrap dependency or native capability added.

T21 adds offline SSC profile contract/store and CLI (tools/ssc_profiles.py, tools/ssc_profile_cli.py), launcher manifest (integrations/launcher/ssc/mod.json) and opt-in inert community_loader adapter. No dependencies on bootstrap, engine, capture, storage or submission. Native configuration adapter and UI pending. See SSC_CONFIGURATION.md and community_loader README.

T21 follow-up: C++ configuration Controller injects Store and PathPolicy; independent typed settings and copy snapshots, no engine or I/O implementation. Existing Python reference remains standalone.

T22 configuration: public/profile_codec.hpp and public/path_policy.hpp portable; infrastructure/windows_profile_store.hpp implements injected Store. Offline Tkinter controller uses existing Python reference. Native consumption and launcher state parsing remain separate pending adapters; no dependencies on game/recorder.

T23 configuration: launcher_json read-only projection and windows_launcher_state plain-file adapter feed standalone preview with injected profile store; AttemptSettings immutable owned value available to future session boundary, not connected to race events. Python exchange/editor independent. Upstream source-fixture verifies integration without engine or launcher UI. Keep DLL inert until applicable gate review.

T24: run_library/public record validation and QueryRepository; native adapter depends only on public port and shared system WinSQLite binding (also used by existing isolated probe). Native derived index owned by library, no engine access, no archive import yet. Exporter/public AttemptBinding consumes only configuration public immutable settings/codec; Python archive verifier checks optional diagnostic identity/redaction/hash. No coupling from configuration to run library or engine. Proposed native observation boundaries documented in RACE_OBSERVATION_PLAN.md; no implementation/hook admission.

T25: offline index_archive controller depends on verified archive report and injected index function, standalone native ssc_index composes existing Repository. No bootstrap or local attempt admission; imported/synthetic only. Private timed_frames bounded handoff shared by WGC/MF test tools, no engine dependency. Existing Worker and quality remain independent; actual capture sequence staged then encoded, live concurrent producer pending. ADR-015.

T26: pure recorder frame_lookahead, shared MF test-only sink, standalone WGC producer with concurrent Worker. No bootstrap/engine dependency. Offline archive_status injects read-only lookup via ssc_index compare/presence, no writes/repair; local admission PROPOSED only. ADR-016.

T28: isolated synthetic_journal_store adapter depends on pure synthetic_admission Context/Journal and Python standard library; fixed private snapshot names, append-only transitions and read-only inspection. No native repository, archive or bootstrap wiring. Fixture performance summary is a test-only pure C++ helper consumed by standalone WGC probe, no production recorder change. ADR-018.

T29: tools/synthetic_recovery.py composes pure synthetic model, Store and archive_status with injected read-only lookup. It writes only an explicit journal snapshot after reinspection. Existing archive publication and native index remain independently callable, no bootstrap integration. Media fixture gains fixed 720p synthetic spec and measured resource reporting; no production recorder change. ADR-019.

T30: synthetic_retry separate adapter depends on existing observe, Store, archive_publish and verified index_archive; injected lookup/insert pair, one explicit operation, journal reconciliation separate. WGC fixture reuses existing worker and MF video spec; lookahead explicit bounded frame byte count, no production capture integration. ADR-020.

T31 synthetic_index_port test-only adapter owns resolved database and bridge identity, exposes lookup/insert; synthetic_retry includes port identity in plan. Resource-series parser validates standalone WGC fixture samples, no runtime recorder integration. ADR-021.

T33: standalone WGC probe owns fixed three recording scopes and process resource samples; fixture_lifecycle.py validates markers/identity, independently reconstructs timeline and consumes existing fixture_repeat/resource_series parsing. run_hd_lifecycle.py composes display preparation, child process, independent decode. No production dependencies or capture hooks. Existing synthetic retry and journal coordinator unchanged; native fault tests exercise process exits at explicit archive/journal boundaries. ADR-023.

T34 fixture-only cancellation and injected device-status branches in standalone WGC probe; fixture_fault_lifecycle parser and independent decoder compose existing lifecycle/timeline contracts. No production recorder or journal API changes.

T35 fixture-only sink write counter and post-join injection evidence; production Worker interface unchanged. fixture_retention.py consumes existing lifecycle/fault/timeline parsers; run_hd_write_failure.py composes display scope, actual capture and independent decode. Native retry fixture extends existing process interruption scenario. No production dependencies.

## T36 fixture-only boundaries
`fixture_isolation.hpp` owns scoped resource probes; `fixture_isolation.py` validates evidence and `run_fixture_isolation.py` orchestrates independent children and decodes. Existing WGC fixture shares scoped device creation and close guards. Optional `--finalize` in existing failure runner uses existing retention and lifecycle parsers. `mf_fixture_sink.hpp` owns fault injection. No production Worker or engine adapter interface change.

T39 adds submission/public/track_claims.hpp, pure per-leg typed claim agreement using categories public catalog only. No runtime source, archive parser, video/replay inspection, attempt binding, proof or eligibility. Existing draft review unchanged. Tested in workflow target and standalone smoke; native tests pending. See docs/product/COMMUNITY_ACCEPTANCE.md.

T40 adds submission/public/bound_track_claims.hpp: pure owned per-source run/session/attempt/leg keys and optional expected category track, malformed/conflicting/incomplete/consistent_claims only. Delegates to T39 comparison, no production admission or schema changes. Existing draft review unchanged. Not verified provenance, timing, coverage or native replay parsing. See ADR-030.

T41 adds submission/public/track_interval_claims.hpp: pure caller-supplied interval containment and sequence/domain/key checks atop T40 comparison. Owned values, synchronous, no OS/engine/UI dependency, no schema/provenance/eligibility or actual coverage claim. Existing draft review unchanged. See ADR-031.


T42 adds pure submission/public/track_coverage_claims.hpp over interval claims. Test-only active own-window destruction and Closed notification reuse standalone WGC fixture and retention parser. Provider-neutral future publishing boundaries documented in [HOSTED_SUBMISSION.md](../contracts/HOSTED_SUBMISSION.md); no production transport or credential dependencies.

T43 offline tools/hosted_text.py is a pure text-composition boundary: Easy defaults / Advanced edited text share one composer; separate protected and personal text, explicit attribution flags and limits, no transport, authentication, native dependencies or eligibility authority. UI/controller and persistent preferences are future consumers. See HOSTED_SUBMISSION.md; all provider-neutral publishing ports remain proposals.

T44: read-only menu/loader observation CLI and separate synthetic tests; no production use or G1 change. See [NATIVE_MENU_READINESS.md](../contracts/NATIVE_MENU_READINESS.md). No new capture fixture; focus on real native boundary and provenance.
