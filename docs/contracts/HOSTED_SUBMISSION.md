# Hosted submission contract — proposed, issue #16

Owner clarification, 2026-11-27 (issue #16 and comments 7206338017, 7206371386, 7206389892, 7206493164, 7206585933, 7206640299). Additive future product requirement; original governing documents and COMMUNITY-001/DEMO-TRACK-01 remain unchanged. Explicit per-submission confirmation is required. No unattended publication or eligibility change.

## Player workflow
Select completed run from history or finish screen. Review verified category ID/level/variables, linked Speedrun.com account ID and username, official category timing (not silently substituted RTA), actual completion date/time, confirmed platform/emulator status, video privacy and evidence contents. All Tracks is a single submission with ordered per-leg evidence, never multiple silent individual submissions. Confirm then queue authorized background publishing while practice continues. Upload supplementary evidence separately; obtain durable link before finalizing video metadata. Upload original video to player's authorized YouTube or other supported service; wait for processing/accessibility; submit through supported authenticated Speedrun.com API only if verified current contract/policy and eligibility allow. Manual upload, copyable Notes and official submission flow always available. No shared account tokens or credentials in evidence; secure credential storage, explicit authorization and revocation.

## Easy and Advanced modes
Easy is the default: use non-sensitive saved preferences, verified category/account/timing/date/platform and evidence contents, show compact final preview including visibility and public evidence disclosure, then explicit per-run confirmation. Advanced starts from the same preferences and allows separate personal Notes and video description, title, supported provider/account, allowed visibility, optional supported tags/pinned comment and permitted evidence options. Do not silently exclude required proof or edit verified identity/time/category. Corrections require separate review or manual flow. Both modes share one validated draft, authorization and durable publishing queue; mode is UX only. Missing configuration or unresolved evidence prompts review or manual fallback. Saving preferences never authorizes future publication.

## Evidence hosting
Evaluate owner-preferred ONE dedicated Git branch hosting ALL run packages, ideally in an isolated evidence repository, not branch-per-run. No repository created yet. Provider-neutral EvidenceHosting port; GitHub policies, growth, quotas, privacy/takedown and availability must be reviewed before selection; object storage remains alternative. Backend authenticated, rate-limited, validates bounded inert files and checksums, no shared write token in client, append-only conflict-safe uploads, no overwrites. Preserve original full local archive/video, exclude duplicated video from shared supplementary ZIP; include permitted racefiles/replays when available, metadata, timing, telemetry, coverage, integrity and hashes. Hosted video may be transcoded and is not byte-identical original. File names: sanitized authenticated username, category key, official time, UTC run date/time and unique run ID; permanent account ID and account metadata in private submission state, not the public manifest. The owner-requested public username in the filename is a narrow proposed exception to the original public-export privacy boundary: disclose it explicitly and confirm before hosting; do not broaden that exception to account IDs or additional account data. Example runs/all-tracks/PlayerName_all-tracks_06m42s150_20261127T140530Z_<run-id>.ssc.zip. No secrets, email, proprietary binaries or path traversal; renames do not alter past paths. Commit-specific download URL if GitHub selected; durability/retention policy explicit, no promise of permanence.

## Submission and video fields
User-provided screenshot: Category*, Player 1*, Time* (h/m/s/ms), date/time (Custom for historical runs), Video URL*, Platform* (PC when confirmed), emulator toggle, Notes. Primary Speedrun.com Notes uses the protected block below; actual automatic video upload uses its own protected block. No project URL or promotional extras. Host evidence first or safely update video metadata after hosting; do not mark workflow complete until required metadata is confirmed. Optional pinned-comment mirror only if supported and explicitly authorized. Manual attribution must remain truthful. API IDs, provider support and field capabilities need current source validation; screenshot is not API proof. Review edits before sending. Traditional submissions remain eligible under existing rules.

### Protected generated text
Automatic video description:
```text
Uploaded automatically by Supreme Speedrun Companion (SSC)

Supplementary evidence package:
[Direct download URL]
```
Speedrun.com Notes for automatic submission:
```text
Submitted automatically by Supreme Speedrun Companion (SSC)

Supplementary evidence package:
[Direct download URL]
```
Store personal text separately and append the protected block. Refuse over-limit composition rather than truncating the required text or link. For SSC-recorded manually uploaded/submitted runs use truthful Recorded using attribution, never claim automatic publication. External changes cannot be prevented. T43 offline tools/hosted_text.py resolves Easy defaults and Advanced full edits, explicit caller attribution flags and UTF-8 byte budgets, bounded preliminary HTTPS query/fragment-free link syntax; these are local conservative prototype restrictions, not provider limits or accessibility/identity proof. No transport or dispatch permission. Existing preview and eligibility paths unchanged.

## Boundaries and failure recovery
Separate provider-neutral VideoPublishing, EvidenceHosting, SubmissionTransport and policy/draft interfaces. Durable queue stores per-stage IDs, request fingerprint, processing status, confirmed remote results; retries reconcile uncertain POST outcomes, never blindly duplicate; no auto-verification or automatic publication without consent. Distinguish malformed/unavailable evidence, missing proof, provider errors, unknown outcome and confirmed remote acceptance. Implement offline models first, provider integration after applicable G4/G6 review; no native or production provenance, account linking, network upload, provider selection or release in P0. Existing menu proof remains required until explicit moderator approval. Never use GitHub Actions artifact storage, upload or download (including for CI evidence); future player evidence hosting is an independent authorized product service.
