# Synthetic recovery coordination (P0-T29 candidate)

Developer-only, trusted single owner, explicit synthetic run. Compose Store.inspect and archive_status.inspect with read-only lookup. Real archive verification must precede observation; the pure decide function consumes trusted fixture inspection facts and must not be used to authorize external metadata. Bind exact journal digest, run ID and synthetic origin; guard independently verified attempt status, and refuse complete fixture capture if manifest capture is not finalized. Finalized metadata does not prove completeness; generation/settings are private fixture facts already frozen at verification. No local or PB authorization.

| Journal | Observed archive/index | Action |
|---|---|---|
| verified | absent/missing | await_archive |
| verified | exact pending/missing | retry_archive_publication |
| verified | exact final/missing or identical | append_published |
| published | exact final/missing | retry_index |
| published | exact final/identical | append_indexed |
| indexed | exact final/identical | none |
| verified or published | exact final with same-file pending alias, no journal pending, matching index status | retry_archive_alias_cleanup |
| published/indexed staged snapshot | exact final and supporting index state | retry_journal |
| anything conflicting, unavailable, unknown, inconsistent or unsupported | — | review |

Exact same-file archive alias is advisory only; existing archive controller performs any explicit cleanup. No write of archive, DB or pending evidence by this coordinator. Reconcile requires caller-selected expected decision, re-inspects, rejects stale/changed or non-journal-only decisions, and appends exactly one snapshot. Exact pending published/indexed snapshots may be retried through Store only with matching external facts; corrupt partial is review, not auto-delete. No automatic scan, concurrent-writer/TOCTOU defense, distributed transaction, directory durability or power-loss guarantee.

Tests: local pure decisions and subprocess death during published journal partial write, flush and publication; Windows native fixture verifies real ZIP, actual index comparison and child exit after archive publication or row insertion, journal-only writes and stale decision refusal. Native fixture pending at candidate preparation. Previous nonlocal index bridge and all earlier tests unchanged.
