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

T29 native integration verified in both Windows configurations, candidate 45a348df2fb1241a9ced5697aca5fe234d33a285 run45864597707/job140064063817. Actual tests assert child death after verified ZIP publication/after native insertion, stale decision refusal with invalid pending, preservation of DB/archive and prior snapshots, exact reconciliation and wrong terminal rejection. No actual gameplay or local authority. Remaining full capture/PR66 review pending.

T29 VERIFIED_AUTOMATED: implementation 45a348df2fb1241a9ced5697aca5fe234d33a285 run45864597707: Debug/Release41/41 (140064063817), media6/6 (140064063880), capture8 PASS/1 SKIP (140064063406), storage/entry1/1 (140064063796/140064063779), release skipped. HDPerformance 250 9997117 17187500 585 630 708 778 14095 88203264 16175104 4530176 433404 1 11059200. Actual 1200-frame LiveHandoff 1200 0 406025000 403118750 2906250 1 0 0; max decoded PTS delta400*100ns. Final documentation head dc9012a1d653ba0704614d17653949e20ad3720c run45864960360: core41/41 both (140065294341), media6/6 (140065294432), capture8 PASS/1 SKIP (140065294396), storage/entry1/1 (140065294260/140065294561), release skipped. Actual logs inspected. PR66 run45864274427/job140062944148 upstream3/3 SSC2/2 startup PASS loads0/0/1; complete comparison bf6f0af6d2b3b135cb3228b9ff2952cfb91782c1 -> implementation -> evidence head Markdown-only, so exact fixture-relevant sources unchanged. Audio MMRESULT=2, G1 BLOCKED; no game, actual device recovery, local authority or release.

T30 separate synthetic_retry.py explicit external operations: see SYNTHETIC_RETRY.md. Existing observe/reconcile retains read-only/journal-only behavior. No production local provenance or automatic repair.

T31 synthetic retry uses BoundSyntheticIndex.lookup for observation and bound insertion for its external operation. Existing journal-only reconciliation remains separate. No production local authority or cross-file transaction.
