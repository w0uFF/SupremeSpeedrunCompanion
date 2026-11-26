# Race observation boundary — P0-T24 design, not a runtime adapter

Status PROPOSED. G1 BLOCKED. No hooks, game memory reads or original-game execution performed. Evidence: ENGINE_MAP.md T24, COMMUNITY_LOADER.md T21–T23, original build fingerprints. Scope is existing Time Attack and later approved category semantics; not a new game mode. Existing IGT remains official; independent QPC RTA separate.

| Candidate observation | Static source | What is not known |
|---|---|---|
| Session prepared/destroyed | original EXE Enter/Exit/Restart/Stop imports, game global allocation/clearing | actual owner-thread lifecycle and object identity |
| Timer begins | 0x7f940 flag +0x10, callers 0x523e4/0x524d1/0x52574 | relation to countdown, first control and human-only race |
| Timer samples | 0x7f6e0 conditional +0.01 accumulator; Player/AI +0xb8 | call cadence, pause/resume, timer unit, selected player |
| Candidate finish | 0x7f9d0 and 0x7f950 dispatch +0x50 to 0x85370 | successful finish vs incomplete/other modes/replay/AI |
| Result visible | 0x8546f row +0x18 and separate EXE/Main_Menu consumer | exact display source and rounding, stable read point |
| Reset/retry/abandon | original EXE existing lifecycle imports and game reset path | reset-to-track does not necessarily reset timer; actual event order |
| File hints | Xinfe USER_REPORTED replay reads/high_times writes | not every-attempt, menu/playback/non-PB false positives |

## Proposed narrow port
Keep per-build addresses only in EngineBridge. On verified owner thread after a naturally executed engine transition, copy bounded typed observations and raw numeric bits to owned values; never call a mutating function to sample, cache native pointers across transitions, call game APIs from a worker or loader initializer, or send raw pointer values in evidence. Version/build identity and explicit valid/unavailable fields, monotonic sequence, independent QPC ticks/frequency, adapter-defined session generation and stable attempt ID. Source evidence classified as candidate until G2. Current contracts/observation.hpp remains unchanged, no native capabilities enabled. No generic event-polling or hooking architecture accepted before owner-thread validation.

Proposed lifecycle: prepared establishes new generation; verified human control release begins an attempt; reset-to-track only event; genuine restart ends previous attempt and prepares a new ID; confirmed finish freezes independent clocks; separately observe result display and retain video until evidence visible. Playback/menu transitions invalidate borrowed state and cannot produce live attempts. Duplicate/stale event rejected, invalid/missing coverage reported rather than synthesized. Use existing session model only with validated adapter events; no conversion of a row flag into a finish event. Worker receives copied values/IDs, never engine pointers. Database, media and archive I/O off engine callback.

## Future G2 controlled observations (after G1 and separate review)
Compare a normal live race and non-PB finish, multiple restarts before/after countdown, fall/reset-to-track, abandon, menu/replay viewing, pause if supported, AI race with human selected, different track/difficulty and permitted weather. Establish exact human identity, generation, natural event ordering, observed raw accumulator and displayed digits, QPC boundaries; record supported renderer/FPS/refresh and repeat at representative 60/120/144/240Hz if possible. Do not change physics or intentionally alter timing. Correlate existing high_times/replay hints only; absence not proof. Capture log metadata and optional screen evidence, no private player rows. Acceptance: repeatable matched start/finish semantics, no false finish/start on non-live or incomplete transitions, exact display precision with documented rounding, no unsafe pointer access or regression/overhead. Unknown cases disable affected capability. G1 must pass first; owner test not requested for this native observation plan yet.

T25: add scheduler observation candidates from ENGINE_MAP.md (EXE elapsed-time-derived count, processed count clamp, actual virtual game dispatch, independent render cadence, raw player accumulator and displayed result). Under G2 after G1, compare naturally occurring stalls and representative FPS caps without injecting delays or altering gameplay. Record both computed and executed update counts if safe, selected human and game mode, QPC, raw timer bits and displayed digits. Do not infer missed-step policy or IGT rounding until observed. No native observation implemented.

T26: scheduler Paint outside step loop; EXE owner +0xc separate from candidate DLL timer +0xc. Candidate direct minute/second/centisecond formatter identified (ENGINE_MAP.md); under reviewed G2 correlate human identity/mode, raw float bits, rendered digits, row and final screen separately, exact rounding and non-PB finish. Natural stalls only. No native observation implemented.

T28: RTTI-backed Checkpoint1/Checkpoint2 callbacks and distinct Finishpoint vtable slot mapped; timer terminal branches distinguish complete and incomplete checkpoint slots but share flags. On_Screen_Display vtable and text dispatch corroborated. Human selection vs AI/replay, actual legitimate finish and authoritative displayed result require G2 observation; no hooks or conversion enabled. See ENGINE_MAP.md.

T29: RTTI confirms AI_Player derives from Player (shared timer and result callback), Ghost/Net derive Player_Base; Replay_Controller derives Player_Controller. A Player cast is not human proof. Separate original completion argument and row time computation; checkpoint-count global candidate. G2 requires controlled runtime identity and final display correlation after G1. No speculative hook or authoritative conversion.

T30: shared Player+0x14c is replay-related, with Replay.dat construction and replay-duration formatting path, not human controller proof. Separate Race_Controller cast and name-based 0x3c row lookup observed; trace concrete controller owner/lifecycle and compare raw IGT, replay duration, row value and actual final result after G1. No new observation hook.

T31: trace original EXE Game::Make_Text_Results() row+0x18 conversion, completed/incomplete string branches, leading zero/punctuation and string vector insertion; Main_Menu text-line consumption. Observe final screen digits vs raw timer/row value across controlled game modes after G1; don't conflate row+0x31 with valid human finish. Race/Replay controller vtables differ; selected-object lifecycle still unproven. See ENGINE_MAP.md exact RVAs.

T32: trace conditional result regeneration in EXE Game::Cycle diagnostic path and alternate caller with mode-like 3..4 condition. Do not use result text generation as finish-event uniqueness proof. Race_Controller cast skips one spatial adjustment branch, not a human-identity predicate. Exact runtime relation still requires G1/G2 controlled observation.

T33: dirty-state candidate byte written even on missing named result row, and cycle helper copies/ranks result rows. Compare actual object identity and lifecycle, completed human event, rendered official result and raw timer only after G1; dirty-state/regeneration/rank list is not attempt proof. See ENGINE_MAP.md.
