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
