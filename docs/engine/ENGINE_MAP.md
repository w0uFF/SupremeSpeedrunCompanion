# Engine map — P0-T14 (OBSERVED_STATIC)

This is a candidate research map, not a native adapter. All addresses below are RVAs, not loaded virtual addresses. Evidence: owner-supplied SupremeLight and Keyboard Persistence v0.1; GNU objdump 2.44 Intel linear sweep and local PE inventory. Full raw disassembly and game files remain in private reference storage. No candidate function is called by the companion. G1 BLOCKED.

## Build identity

- Original Supreme.exe SHA-256: 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c.
- Keyboard-patched Supreme.exe SHA-256: f7bc9074e41244fb5a9a3ab53a4e433ac14564fb47206a5e3c91caf975531e02.
- Main_Menu.dll SHA-256: aed413babf03cb64596415f8fe9a1af043da3c2f6590e4dffe1dd37ac71098f9.
- Supreme_Game.dll SHA-256: b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b.
- HMG_Kernel.dll SHA-256: 814b7c5f32a17553c168cade2474e5ae9caec8b7fff4bd41090312ad097fdf15.
- Full supported-reference profile: build-profile.json. Preferred image base of executable is 0x400000; engine DLL excerpts used 0x10000000.

## Native menu

Reference Data/Main_Menu/ID_Lists/Button_List.txt contains ID_MAIN_MENU, ID_ARCADE_MENU, ID_ARCADE_TIME_ATTACK_SEQUENCE, ID_CHAMPIONSHIP_SEQUENCE and nested selection/result entries. Page/sequence lists and templates also exist. Text assets are not a complete dispatch contract.

| Export / candidate | Evidence and meaning | Risk / next validation |
|---|---|---|
| Supreme.exe Get_Page_Names 0x2e580 / Get_Button_Names 0x2e790 | Bodies call 0x557c0 / 0x557d0, then copy elements into caller-supplied vector. Helpers return fields at +0x10 / +0x20 of object reached via global. | Legacy std::vector/std::string layout and ownership; registration producer, initialization order, cleanup unknown. |
| Button_Pressed 0x2e4a0 | Legacy string handling into listener-owned member at this+4; also clears a global object's byte at +0x50. | Selection notification, not established command injection. Never supply modern std::string. |
| Time Attack dispatch | ID_ARCADE_TIME_ATTACK_SEQUENCE referenced at 0x315b and 0x290b9; first body tests comparison result then allocates 0xa8 bytes, calls constructor-like code at 0x183c0 and initializes object/vtable-like fields. Second appears in a different comparison chain. | Static evidence supports separate asset registration and executable dispatch; exact mode semantics, ownership and navigation require runtime evidence. |
| Championship dispatch | ID_CHAMPIONSHIP_SEQUENCE reference 0x2929d. | Not a proven callable entry point. |
| Play Again | ID_PLAY_AGAIN references 0x28999 / 0x3346f. | Neither string reference establishes actual restart moment. |
| Main_Menu.dll | Menu::Cycle 0x3c580; Set_Menu_Listener exports for page 0xd440, menu button 0xcd60, mesh button 0xfbd0. | No ABI-safe registration method validated; inspect actual lifetime and callback thread. |

## Selection and configuration

Supreme.exe Select_Item 0x2f150 checks EDX against 0xc and dispatches via a jump table. Track_Selection_Radio_Button::Select 0x3f5c0 and Environment_Selection_Radio_Button::Select 0x40110 access selection object state, a game-item list, then call imported Get_Name and MSVCP60 string assignment. Track destination is candidate field +0xd0 and environment +0xb0 in a separate configuration object. Board_Selection_Radio_Button::Select 0x40470 similarly selects a game-item name and calls 0x47640. Daytime_Selection_Radio_Button::Select 0x44020 has a separate path. These are legacy object-relative observations, not verified struct definitions, stable numeric enum mappings, or permitted write locations. Character dispatch and actual launched identity remain unresolved.

All nine canonical track asset directories and four weather variants are known. Numeric track/environment IDs must not be assigned to labels by guess. Defaults and availability files describe preferences, not actual loaded race identity. Need selected UI label -> native configuration -> actual launched race observation before enabling commands.

## Race lifecycle

| Candidate | Supreme.exe call RVA / engine export RVA | Interpretation |
|---|---|---|
| Restart | 0x25b02 / Supreme_Game 0x140920 | Distinct branch; comparison of prior/current data precedes it. |
| Exit | 0x25b12 / 0x13fb20 | Alternative branch leads to Enter. |
| Enter | 0x25b30 / 0x13ea70 | Decorated signature includes Input and Supreme_Start_Info_Struct; one stack argument and EDX set in caller. |
| Stop | 0x260e9 / 0x1408f0 | Coarse lifecycle, not official finish. |
| Cycle | 0x265b8 / 0x13fe40 | Receives Time object; frame/race boundary unverified. |
| Return_Active_Player_To_Track | 0x8fa3 / 0x142d70 | Distinct reset candidate. |
| Start_Info_Struct::Reset | Supreme_Game 0x142db0 | Body only clears byte at ECX+0xe4; not a full reset/official start signal. |

Enter and Restart paths converge at executable RVA 0x25b36, followed by legacy string copies, Set_Game_Mode at 0x25bb1, and Time::Current at 0x25be6. Coarse lifecycle events must not be equated to countdown release or finish. Fastcall-like register use and decorated MSVC names are hints only. No structure size/layout, caller stack contract, official IGT result field, race-start/finish callbacks or game-thread guarantee established.

## Clock

HMG_Kernel::Time::Current 0x3900 uses a global float comparison (RVA 0xbf98 compared with constant at 0x7228) and either timeGetTime (IAT 0x71f8) or QueryPerformanceCounter (IAT 0x7000). Setup routine 0x26b0 probes QPC and QueryPerformanceFrequency (IAT 0x7040), converts the counter frequency into float, and derives scaling fields. Time::Frequency 0x3980 returns a separate float field at 0xbfa8. Time subtraction 0x39b0 subtracts two-word values and converts/scales the delta through float. This is evidence of generic engine clock infrastructure, not the authoritative race timer or rounding algorithm; no claim about the cause of the owner's occasional inaccurate IGT.

## Keyboard coexistence comparison

Run: python tools/native_xrefs.py <original Supreme.exe> --compare <patched Supreme.exe> (GNU objdump required). See docs/testing/t14/xref-comparison.json. Exactly 75 common menu string operand matches and 1341/1342 common selected IAT operand matches. Changed candidate KB_Action call: original RVA 0x53887 -> patched RVA 0x8a02c; target IAT RVA 0x6d4b8 unchanged. Candidate set uses instruction RVA, target RVA and symbol; linear sweep can contain false positives, misses indirect/computed references, and does not prove identical instruction bytes or behavior. No patch overlap/compatibility guarantee.

## Runtime gate

Retain existing Display Config proxy. Upstream source has no established native registration ABI; DllMain hooks/pinning and lazy filesystem resolution do not constitute a safe general plugin callback. Experimental offline C ABI remains unchanged and native capabilities zero. Actual menu creation, selection, track transitions, lifecycle timing, recording and rollback NOT_RUN. Next: owner disposable Windows baseline and focused upstream contract review; then a minimal approved installed-disabled diagnostic candidate, not an active race hook.

## P0-T15 — Menu registry, configuration and presentation ownership

OBSERVED_STATIC, same fingerprints as above; GNU objdump 2.44 and PE section mapping. Instruction addresses below are RVAs. No functions are invoked by the companion.

### Menu registry and lifetime

- The executable contains actual references to Data/Main_Menu/ID_Lists/Page_List.txt (string RVA 0x882e4, operand references 0x544ab/0x545b7), Sequence_List.txt (0x8828c, refs 0x54653/0x5475c) and Button_List.txt (0x88234, refs 0x547f8/0x54904). These are not merely menu ID strings.
- At 0x547f8 the Button_List path is supplied to an imported stream-related routine at IAT RVA 0x6d230; the following loop extracts a temporary value at 0x5487f, retrieves a field at owner+0x4c, uses its +0x20 member and calls 0x7510 at 0x54895. The prior Get_Button_Names helper 0x557d0 returns owner->field_4c+0x20. The Page helper 0x557c0 returns owner->field_4c+0x10. Together these establish a concrete registry producer/consumer lead. They do not prove parser grammar, valid insertion order, dispatch support, or allocator compatibility.
- Original executable allocation request 0x530b2 = 0x70 bytes; constructor import call 0x530f7; return value stored into owning object +0x10 at 0x53104. A later call at 0x5315f obtains the menu from that global owner and calls Init_Main_Menu. Main_Menu.dll constructor export RVA 0x3bce0; Init_Main_Menu 0x3c4d0; destructor 0x3c390. Destructor conditionally invokes virtual destruction on members +4, +0xc and +8 and cleans other resources. No cached menu/child pointer may be assumed valid after teardown or page discard. The observed allocation size is not a portable C++ type declaration.
- The Supreme_Game object is separately allocated (executable 0x52e13, 0xa0 bytes), constructed through IAT call at 0x52e43 and stored at owner +4 at 0x52e8c. The cleanup path at 0x53eca calls its destructor then a deallocation routine. This constrains ownership but does not prove the full recreation schedule or which thread performs it.

### Preferences vs active race configuration

Saved_Data/Defaults.txt contains default_environment, default_level, default_daytime, default_character and default_board. In executable disassembly the corresponding strings are passed at RVAs 0x4a118/0x4a182/0x4a1ec/0x4a25a/0x4a2c0. Nearby code passes owner-relative fields +0x4c/+0x5c/+0x6c/+0x7c/+0x8c to a common helper 0x2750. These are static preference-load leads only, distinct from T14's radio selection code. Do not equate default IDs with current loaded track/rider/board/weather or persist companion choices into these files. Existing nine high-time files use region and difficulty names; the executable contains suffix _high_times.txt with references at 0x60688 and 0x60cc0. Leaderboard history is not a per-attempt authoritative result and a non-PB may not update it.

### Result object and presentation side effects

- Supreme.exe export Get_Text_Results RVA 0x323e0 loads a global pointer from preferred VA 0x4889c4, follows +0x14, then +0x1ec, and returns it. This is a borrowed pointer path, not a result snapshot or thread-safe API.
- Allocation site RVA 0x49e3c requests 0x38 bytes; code initializes subobjects at +0x14 and +0x24 via calls at 0x49e5e/0x49e6e, sets byte +0x34 to 1, stores pointer at owner+0x1ec at 0x49e7f. Allocation failure stores null. Exact types, initialization of all fields, deallocation and valid lifetime windows remain unverified.
- Main_Menu.dll export Menu_Results_Shower::Get_Results RVA 0x12810 invokes a listener virtual function at vtable +0x30 (0x1281f); it branches if the returned pointer is null. It tests byte +0, uses values at +4 and +8 and an x87 float load from +0xc (0x12859), then writes to its own presentation state. At 0x12898 it clears the result object's byte +0x34; subsequent code copies row-like data, including vector-like bounds +0x18/+0x1c and stride 0x24. The exact virtual-table correspondence with Get_Text_Results is a strong hypothesis, not verified by runtime call trace.
- Supreme.exe code at 0x25037 sets byte +0x34 to 1 after updating two vector-like fields. The flag is also set during initial allocation, so it is a candidate presentation-dirty flag, not a unique finish signal. Code at 0x27089 and 0x2717e copies five DWORDs into the result object's prefix; one path also calls Set_Replay_Mode. Defaults, intermediate values and replay data can therefore reach the same presentation object. Do not treat +0xc as verified final IGT or read it without lifetime/mode validation.
- Main_Menu.dll has direct calls to Get_Results at RVAs 0x127b3 and 0x12988. It is unsafe to call this consumer as a getter because it clears state. A future adapter may observe a proven natural call or a stable read point only after G1; it must never invoke this function just to sample, suppress its behavior, or infer finish from repeated presentation refreshes.
- Official time remains the actual game-displayed final result under the reported rules. Runtime trace must independently confirm countdown/control release, race finish, result publication, result rounding and non-live/replay exclusion. No native timing field is admitted.

### Additional original/keyboard compatibility check

Nine selected executable byte windows in the original and supplied keyboard-patched build match exactly. See docs/testing/t15/byte-windows.json (RVA, length, SHA-256). This check covers only selected byte ranges, not whole functions, control flow, runtime relocation or hook coexistence. Use the existing build fingerprints and PE raw-section mapping to reproduce. No native capability enabled.

## P0-T16 — additional presentation field disambiguation

Recomputed original Supreme.exe SHA-256 (matches build profile). Fresh GNU objdump -d -Mintel --start-address=0x427151 --stop-address=0x427196 on the original PE: RVAs 0x27157 and 0x27161 write IEEE-754 0x41f00000 (30.0) to preferred VAs 0x488944 and 0x488954 respectively. RVA 0x27179 loads source 0x488948 and 0x2717e copies five DWORDs into the borrowed result object obtained via owner+0x14 then +0x1ec. Thus global 0x488954 maps to the candidate result prefix +0xc on this path and can be assigned a constant 30.0. Its exact meaning, when valid, units and authoritative final IGT producer remain unknown. Do not equate the x87-loaded presentation field or dirty flag to official race finish. No native integration changes.

## P0-T17 — second result-presentation copy path (OBSERVED_STATIC)

Original Supreme.exe, GNU objdump -d -Mintel --start-address=0x427000 --stop-address=0x427100. At RVA 0x27035 the code writes 0x41f00000 to preferred VA 0x488954, after zeroing 0x488950; at 0x27019 it stores ECX minus 0xf into 0x48894c then clamps negative values to zero. At 0x27042 it reads a value through [EBP+4] and tests 7,9,10, conditionally changes byte 0x488948; another comparison follows at 0x27061. At 0x2706f..0x27089 it follows the same borrowed pointer path and copies five DWORDs from 0x488948 into the result object. Subsequent code calls an import at 0x27094 and clears [EDX+0x74] at 0x270a4. The selected values and exact mode semantics are unresolved; this further supports distinguishing presentation setup from authoritative finish/IGT. No candidate hook or direct read enabled.

## P0-T18 — presentation copy and menu discard (OBSERVED_STATIC)

Original Supreme.exe SHA-256 verified against build profile. GNU objdump -d -Mintel linear sweep identified third presentation copy path at RVA 0x23e15..0x23e67: x87 loads preferred VA 0x488944, applies fabs, saves magnitude, performs further comparison against constants and selects either value at 0x46d770 or saved magnitude, then stores at 0x488954 (RVA 0x23e4d). The code follows owner+0x14 then +0x1ec and copies five DWORDs from 0x488948 at RVA 0x23e67. This supplies presentation prefix +0xc through a transformation, not an established authoritative IGT producer. Additionally T17 nearby call RVA 0x27123 references IAT preferred VA 0x46d47c; independently parsed PE import maps it to Main_Menu.dll Menu::Discard_Page (legacy std::string argument). Confirms a menu-discard path near the previous result initialization, not an official race-finish boundary. No native calls or hooks. Next locate actual score/race result producer and verify against displayed final value under G2.

## P0-T19 static presentation producer check
Original Supreme.exe SHA-256 confirmed 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c; Supreme_Game.dll b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b. GNU objdump -d -Mintel on original exe, grep operands 0x488944/0x488954: repeated writes of constants 30.0 (RVA 0x23c56), -10.0 (0x23cc1, 0x23d11, 0x23db9) and +10.0 (0x23cfa, 0x23d68, 0x23d9a) to source global 0x488944; x87 writes at 0x23cd9 and 0x23d60. Result +0xc receives derived value through previously mapped copy at 0x23e67. This reinforces presentation-state ambiguity; exact meaning and true final IGT producer remain unresolved. objdump -p Supreme_Game.dll export table shows no named direct final-time getter; lifecycle exports alone are insufficient. No native access or activation.

## P0-T20 — high-time persistence producer/consumer leads (OBSERVED_STATIC)
Original Supreme.exe SHA-256 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c. GNU objdump -d -Mintel ranges 0x460688..0x460748 and 0x460cc0..0x460dc6, objdump -p imports. Both suffix construction paths call legacy std::string::append at IAT 0x46d308. At RVA 0x60735 owner+0x54 passed to 0x43d940, which constructs basic_ostream via IAT 0x46d318 at 0x43d9b6 and opens filebuf at 0x43d9ee via IAT 0x46d230 with flag 0x12. At RVA 0x60db9 owner+0x54 passed to 0x43d460, which constructs basic_istream via IAT 0x46d38c at 0x43d4d5, opens via same filebuf IAT with flag 1 at 0x43d512, and uses x87 load/store at 0x43d630/0x43d658 during parse. These are distinct persistence writer and reader candidates. Nine original high-time files each have seven lines (262–282 bytes), raw personal rows not committed. These paths strengthen saved-record mapping, but no proof of actual finish producer or per-attempt official time; high times may omit non-PBs. No hook or native access.

P0-T21 community native loader and binary menu/finish/restart leads: see COMMUNITY_LOADER.md. Source-backed ABI candidate, no runtime engine capability. Xinfe's file triggers remain USER_REPORTED, not authoritative time or every-attempt event.

## P0-T24 — per-player accumulator and result writer (OBSERVED_STATIC)
Original Supreme_Game.dll SHA256 b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b. GNU objdump -d -Mintel on privately extracted matching file; PE image base 0x10000000. Section-table-backed raw mapping and RTTI locators confirm the primary vtables (RVAs) 0x169e10 Player (COL 0x17f428, type descriptor 0x1b1100), 0x16dc1c AI_Player (COL 0x1840a8, type descriptor 0x1cc2b8), 0x169ebc Player_Base (COL 0x17f4f8, type descriptor 0x1b9400). Constructor writes observed at 0x836f0 (Player), 0x129ad6 (AI), 0x85b5a (base). Player/AI both have slot +0x50 = 0x85370, original result-row writer previously found; Player_Base slot +0x50 = 0x7fd30 (ret 4). All three +0x38 slots point to 0x7fcf0, which returns object +0xb8. No general player identity or safe ABI proven.
- Candidate timer allocation: 0x861a4 allocates 0x20 bytes, 0x861c1 calls 0x7f6b0 with owner, 0x861d2 stores pointer at owner +0xb8; analogous 0x8751d/0x8753a/0x87549. Constructor stores owner at +0x1c and calls reset 0x7f720; reset clears +0xc, +0x10/11/12, +0x14, +0x18 and three initial words. 0x84575 calls reset through null-checked +0xb8.
- Accumulation 0x7f6e0: if +0x10 !=0 and +0x11 ==0, x87 reads +0xc, adds float at RVA 0x164024 (IEEE float 0.009999999776482582), writes +0xc at 0x7f6f7; otherwise if +0x12 !=0, adds same constant to +0x14 at 0x7f70b. Direct caller 0x84c02 inside routine starting 0x84990 (Player/AI primary vtable +0x1c); gets null-checked +0xb8 at 0x84bf8. This is a fixed increment per invocation, not proof of 100 Hz real rate or official seconds. +0xc is the value read by result writer 0x8540a via getter.
- Start lead: 0x7f940 sets +0x10=1 unless +0x11 already set. Call sites 0x523e4, 0x524d1, 0x52574. Nearby 0x52495 sets descriptor +0xe4 before enumerating players. 0x52399 dynamic_cast type descriptors Sbo -> Player_Base; additional cast to Player 0x523d4. Player group/mode handling, not confirmed local control release.
- Completion lead: 0x7f950 validates preceding slots against zero, writes selected slot from +0xc at 0x7f98a, for mode 5 sets +0x11/+0x12 and dispatches owner vtable +0x50 (0x7f9ab); otherwise +0x54 (0x7f9bd). Direct caller at 0x52784 and 0x52b95. Routine 0x7f9d0 requires +0x11 clear and +0x10 set, checks previous slots, updates +0x11/+0x12/+0x18; dispatches +0x50 at 0x7fa55 with +0xc on one branch, at 0x7fa6d with zero on other branch, clearing initial slots in incomplete branch. Direct caller at 0x52fcb after Player_Base cast and getter +0x38, checks +0x18 first. Same result-row writer callable through incomplete/other-mode branches; cannot treat every dispatch or +0x31 flag as legitimate completion. Row writer also branches on descriptor mode and strings Halfpipe/Ramp. User's speedrun scope remains existing competition modes.
- Hypothesis: +0xc is a simulation-step accumulated candidate game time; +0x14 is a secondary accumulation. Need actual call cadence, selected human identity, race-mode transitions, checkpoint/finish distinction, pause behavior, rounding and rendered displayed result correlation. No IGT conversion, source pointer cache, hook, ABI call or patch approved. See RACE_OBSERVATION_PLAN.md.

## P0-T25 — nominal fixed-step scheduler and result conversion (OBSERVED_STATIC)
Original Supreme.exe SHA256 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c; original Supreme_Game.dll SHA256 b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b. GNU objdump -d -Mintel and PE section mapping for constants and vtable pointers, no execution.
- EXE RVA 0x25c4a calls Time::Current IAT 0x6d15c, at 0x25c6e calls Time subtraction IAT 0x6d164, multiplies result by float 100.0 at RVA 0x6db0c (0x25c74), _ftol thunk 0x6204c -> IAT 0x6d3e4. Compares count to 20 at 0x25c81, branches to 0x26000 to set processed count 20 if above; 0x25ce0 multiplies step index by float 0.009999999776482582 at RVA 0x6db08, adds to timestamp using imported Time::operator+= at 0x25cf6; related timestamp passed at 0x25d51 to vtable slot +0x10. Loop increments index at 0x25d58 and checks processed count at 0x25d59. At 0x25d89 and 0x25db2 advances two timestamp values by processed count and original calculated count respectively, each times 0.01; at 0x25dc1..0x25de1 excess count can drive Menu::Cycle via IAT 0x6d484 (confirmed import), not same game callback. 0x25e2b also scales original count by 0.01 and conditionally accumulates object +0xc. Do not infer this field is same as player timer.
- Vtable pointers in original EXE .rdata: RVA 0x6d674 contains 0x426570, vtable base RVA 0x6d664 assigned at 0x324c; multiple vtables same +0x10. RVA 0x6e194 holds 0x435640, base 0x6e184 assigned at 0x34948; wrapper 0x357c6 calls 0x26570, which forwards Time argument to Supreme::Cycle IAT 0x6d530 at 0x265b8, gated by owner+0x14+0x1f0. Statically plausible connection to scheduler, actual live vtable/dispatch depends on mode.
- DLL exported Supreme::Cycle RVA 0x13fe40 multiplies global RVA 0x1c274c by float 0.01 RVA 0x164024 at 0x13fe65, writes global owner +0x140 at 0x13fe85, conditionally increments owner +0x24 by 1 at 0x13febd, later calls 0x11c10 via owner+0x44 at 0x13ffe7. Global speed/step-related value also set to 1.0 at 0x11650, modified at 0xb879 and 0x14086a; exact meaning unresolved. Another path 0x9eca1 dispatches +0x1c via stored Player_Base candidate after 0x9eb60 timing/position code; could include replay paths, not assumed live cadence. T24 Player/AI +0x1c leads to conditional accumulator increment at 0x84c02.
- Original result writer 0x8540a reads timing object +0xc, multiplies by -0.01666666753590107 at RVA 0x166654 and calls _ftol thunk 0x1441f2 -> IAT 0x1636a8; combines times 60 with remainder of second _ftol by unsigned division 60 at 0x85432, then multiply same input by 100 at 0x85447, _ftol, remainder mod 100 at 0x85459, scale 0.01 and add at 0x85467/0x8546d, store row +0x18 at 0x8546f. Supports candidate whole-second/centisecond reconstruction; exact _ftol behavior under game FP state, negative/overflow edge, display conversion and actual final result not proven.
HYPOTHESIS: nominal 100-step/s scheduling with per-update 0.01 accumulation and bounded catch-up may explain divergence after stalls; actual scheduling, rendered FPS, skipped steps and legitimate timing semantics require controlled runtime observation. No FPS-dependent conclusion, timer fix, hook, or official time conversion. Keep IGT and independent RTA distinct.
