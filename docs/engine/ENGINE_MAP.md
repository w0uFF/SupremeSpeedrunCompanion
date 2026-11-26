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
