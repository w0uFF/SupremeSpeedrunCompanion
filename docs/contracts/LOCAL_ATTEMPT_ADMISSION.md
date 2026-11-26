# Trusted local attempt admission — PROPOSED (T26)

No implementation or authorization. G1 blocked; G2 validated live human race identity/semantics and relevant capture/evidence gates required. Local provenance means admitted through SSC-owned workflow, not proof of honesty or eligibility.

Only internal session controller may create private attempt context after validated engine identity and live human start, with owned values: run/session ID, generation, exact build, approved category/profile mapping, immutable settings and independent clock boundaries. Stale, duplicate, unsupported and playback events cannot mint local provenance. No origin claim in ZIP, external manifest, stdin, filename or import flag authorizes local. No native calls from I/O worker.

Proposed sequence:
1. Allocate private owned staging and journal identity, prepared/recording phase, no-replace paths. Synthetic fixture separate namespace, no promotion.
2. Validated terminal event freezes attempt snapshot, missing/unknown fields explicit, capture quality retained. Reset-to-track alone not terminal; unknown finish never official finished time. RTA separate from IGT.
3. Off game thread finalize owned files, bounded archive, independently verify exact manifest/members and match immutable identity/settings/terminal fields to private context. Record exact digest and publication phase in private journal; publish no-replace. Only internal admission service can request local row, matching digest and approved comparison namespace. No approved mapping means no local PB.
4. Insert-only index; identical retry idempotent, conflict preserves existing evidence/index. Journal finalized/indexed or review-required. Never promote nonlocal rows or overwrite immutable identity.
5. Recovery requires validated private journal and exact digest/identity. If missing/inconsistent preserve for review without local admission. File/DB transactions separate, no power-loss or anti-tamper guarantee.

Required synthetic tests: terminal duplicate, restart generation mismatch, missing/stale context, synthetic/imported injection, archive-declared local, wrong settings/digest, incomplete capture, unknown finish, missing approved mapping, crash-point retry, conflicts, orphan, invalid journal, missing DB and identical retry. No promotion, overwritten bytes or untrusted local PB. Journal format, ownership, flush semantics and capability boundary require review. Native bridge still imported/synthetic only; moderator policy unchanged.

## T27 synthetic-only model (candidate)
`tools/synthetic_admission.py` is an immutable in-memory state machine with recording, terminal, verified, published and indexed phases, exact identity/generation/settings/terminal/digest checks and a conservative read-only recovery decision. It has no filesystem journal codec, trusted runtime context, archive verifier or index connection; the caller supplies already verified fixture facts. It refuses local/imported origins and emits no Record or PB. Separate pending/final facts require review; no repair or durability guarantees. Unknown completion rejected; incomplete capture and approved competition mapping remain unimplemented and cannot authorize anything. Tests exercise stale context, duplicate terminal, origin mismatch, conflicts, missing evidence and idempotent synthetic index decisions. No change to native bridge.

T27 follow-up: capture completeness is an explicit immutable bool bound at terminal and verification, not inferred from archive presence. Incomplete synthetic evidence can be retained, never authorized as local/PB. Confirmed matching final/index with stale journal phase requests explicit journal reconciliation rather than silently reporting none. Unknown/invalid facts require review. No journal persistence yet. Follow-up CI pending.

T27 final verified exact implementation 83c89e0336cc781bf2fa70cd22754f89c2acf31a, Windows run 45861739545, pure model local 5/5. T28 candidate `tools/synthetic_journal_store.py` implements a separate synthetic-only append-only snapshot store; no archive or native DB integration. See SYNTHETIC_JOURNAL_STORE.md. G1/G2 and local authority remain blocked.

T29 candidate coordinator integrates synthetic fixture journal with independent verified archive/read-only index facts. It does not implement a local attempt producer, eligibility, approved mapping, or native admission. See SYNTHETIC_RECOVERY.md. G1/G2 unchanged.
