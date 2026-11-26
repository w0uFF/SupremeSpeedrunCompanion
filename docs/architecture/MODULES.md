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
