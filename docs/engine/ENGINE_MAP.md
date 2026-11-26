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
