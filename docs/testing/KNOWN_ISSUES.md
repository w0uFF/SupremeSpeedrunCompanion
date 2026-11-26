# Known issues and limitations

- G1: native loader extension, safe initialization/shutdown, Windows installed-disabled launch/exit and rollback unresolved.
- Exact Display Config source revision and binary correspondence, GPL obligations and project license pending.
- Keyboard persistence is user-reported on one installation; audio causality unestablished.
- Static fingerprint classifier does not cover live memory, every asset or extra modules and is not an eligibility check.
- Module host and C ABI are offline prototypes; no live callbacks or Rust/Windows integration validated.
- Replay header diagnostics recognize a candidate structure, not official IGT; unsupported ramp/halfpipe replays rejected. No production replay import/security claim.
- Category order and retry rules are not approved; live Speedrun.com metadata/auth/transport absent.
- No native menu, game controls, timing, recording, overlay, persistence, async workers, submission or installer.
- Original private reports are kept outside the public repository. Windows CI results must be checked independently; no game runtime test is included.

- T14 candidate cross-references are a linear sweep, not control-flow proof; menu registration, legacy object layout and actual selection/finish semantics unresolved.
- Replay position probe excludes the ambiguous terminal record and cannot infer units, geometry crossings, official IGT, RTA or authenticity. Six guide formats remain unsupported.

- T15 Get_Results clears a presentation flag; it is not safe to invoke as a getter. Dirty flag initialized before a race and may recur; no official finish/IGT mapping yet.
- Fixed-file paired snapshots are not full preflight, runtime evidence or a security sandbox. Redirected files, unknown status, no PB or missing files cannot prove a gameplay issue. Owner Windows baseline still pending.

- T16 timer is not bound to native race events; QPC smoke test is not timing correctness in-game. Supported conversion frequency limited to 1 GHz; suspend behavior and performance unmeasured.
- Offline SQLite schema is provisional and not an evidence database or C++ runtime decision; no arbitrary DB import, crash recovery, migration, deletion or native UI.
- File inventory is not full package/export verification. API preview is not dispatchable; metadata, credentials and submission authorization absent. Recording feasibility is documentation-only; no interactive capture or codec test.

- T17 archive is experimental, stored-only, 1 GiB total; no complete evidence schema, crash-safe publication, playback, import/extraction or authenticity. Do not assume arbitrary ZIP compatibility.
- Preferences storage adapter remains test-only in C++; no production backend or run-store integration. Replay comparison needs caller-confirmed matching identity; no official IGT.
- Media probe is synthetic and Windows-only; compile/runtime/codec support pending CI, no audio, live capture or performance validation.

- T18 worker uses a short mutex and allocations, cooperative cancellation only; blocking sink can delay join. No production sink adapter, race timestamps, audio/video capture or performance validation.
- Schema 2 is provisional caller-supplied metadata. Hardlink publication requires a supporting filesystem and trusted quiescent directories, no power-loss durability or automatic import/recovery. Run repository is an interface only.

- T19 per-stream handoff statistics cannot establish actual decoded coverage, missing producer samples before/after observed span, black video or valid audio. Clock domain mapping still requires platform verification.

- T19 WGC frame metadata feasibility verified, not decoded pixel integrity, resize/device loss, full-screen game or renderer compatibility. WASAPI capture service initialized, no actual audio samples or isolation/synchronization proof. Ten-second synthetic test is low-resolution and not an in-game performance benchmark. SQLite local test is not a Windows or production repository adapter.

- T20 verified pixel readback uses two own-window crops repeated on a synthetic timeline. No continuous live capture or resize/device-loss/real sync validation. Own-process audio sampling compiled but hosted waveOutOpen failed MMRESULT=2; interactive test required. No endpoint-wide fallback. WinSQLite verified on one hosted OS/version only; not a production storage adapter. Thirty-second synthetic low-resolution recording is not gameplay benchmark.

- T21 documented community loader ABI but release/source-binary correspondence, actual game startup thread/loader lock and compatible baseline unresolved. Inert candidate not installed. Profile store is offline Python reference, trusted single-writer, no native C++ adapter, actual UI, live settings or migrations. Manifest defaults are capability requests, not supported recording modes. No full crash durability. G1 and audio sample blocker unchanged.

- T22 native profile codec intentionally only canonical schema-1 ASCII JSON subset; actual launcher mods.json parser absent. Win32 store assumes trusted existing private directory and single writer, no power-loss or race-proof path guarantee; free-space check not reservation. Offline editor smoke not actual launcher UX. Source fixture result requires completed logs; G1 remains blocked. Audio playback-device blocker unchanged.

- T23 supersedes earlier source-fixture and native parser pending status: reviewed PR66 synthetic runtime and native read-only preview verified. Installed loader equivalence, real game startup/exit/rollback, actual UI and in-DLL settings consumption remain unverified. G1 blocked. Exact bounded parser may reject otherwise valid larger/deeper unrelated JSON; deliberate fail-closed behavior. Optimistic profile check is not a concurrent writer lock; exported paths stripped. No run binding yet. Original row writer is a timing candidate only. Audio skip unchanged.
