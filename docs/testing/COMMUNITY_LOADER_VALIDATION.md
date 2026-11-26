# Community loader verification plan (G1 remains BLOCKED)

Inputs: private source ZIP b7d9233f8636e222bbd0a5a739e1e04d2fbc8a12 and five-patch PR66 text; Simone distributed winmm.dll has different bytes from Display Config 1.8.0; exact source-to-binary correspondence not established. No upstream binaries or code redistributed, no loader replacement, no upstream commit. Source docs and test fixture are not our runtime results. Source archive lacks native-mods/ implementation advertised by documentation; separate binary packages are references only.

Independent candidate test:
```powershell
cmake -S . -B entry-build -A Win32 -DSSC_NATIVE_CANDIDATE=ON
cmake --build entry-build --config Release --target SSCNative p0_community_entry_tests
ctest --test-dir entry-build -C Release -R "^p0_community_entry$" --verbose --no-tests=error
```
Expected: exact SSCInitialize export callable twice, nonzero, inert synthetic host only. Does not test PR66 loader, manifest parsing, game startup or lock state.

Before G1: (1) establish reviewed compatible loader build/provenance and licensing, preferably upstream-supported version; no overwrite of existing proxy, no execution of unreviewed downloaded binaries; (2) record disposable original and supported Display Config baseline using WINDOWS_BASELINE.md; (3) separately reproduce upstream fixture from reviewed source, with its own synthetic host and test dependency, verify loader outside lock, same triggering thread, once-only load, INI replacement and disabled case; (4) owner approves a disposable game test of installed-disabled SSC, including original, supported loader and keyboard-patched profiles where compatible. Keep mod disabled and load_diagnostic false, then separate explicitly reviewed inert-load diagnostic test if warranted. Confirm launch/menus, controls, audio, race finish/restart, exit, no data modifications beyond normal game state, loader log no errors, no added game hooks/capture/network, rollback removes only SSC-owned files and restores baseline. If upstream loader upgrade is necessary, treat it as separate reviewed compatibility step; do not replace existing winmm.dll automatically. Do not pass G1 until actual game launch/exit and rollback evidence meets protocol. No player intervention needed for continuing offline configuration development; human baseline needed for native progression.

Independent fixture verified on Windows x86 run 45795828198 job 139826085384; exact export callable twice, 1/1 CTest. The actual upstream fixture, launcher UI, and installed-disabled game baseline remain NOT_RUN. Do not install from development source until reviewed loader and owner baseline requirements are satisfied.

## T22 exact isolated source-fixture reproduction

Source SHA-256: 2ed8a5d2e334268432c23dff26b294bbe9b2d69b22a60073f8fd42e71b66c72b, ZIP revision b7d9233f8636e222bbd0a5a739e1e04d2fbc8a12. tools/pr66_fixture.py stages the exact archive into a new private directory, rejects a different fingerprint and existing destination, adds only SSC-authored manifest/test fixtures. Local staging and refusal passed. No upstream files committed. Windows toolchain and locked Cargo dependencies needed for execution. Workflow .github/workflows/pr66-fixture.yml attempts the public exact-archive acquisition, skips build/execute if fingerprint mismatch or acquisition unavailable, uses read-only repository permissions; no artifact upload. It builds SSC inert DLL, upstream config tests, a synthetic host named Supreme.exe (not the original game), reviewed proxy from source, and the upstream dependency/INI fixture. Then test SSC manifest under disabled, enabled-but-false, and enabled-and-true conditions using only the synthetic host. Actual source fixture success must come from completed logs, not a green skipped step.

From an x86 Visual Studio developer shell with Python, CMake, PowerShell 7 and Cargo, using an owner-reviewed exact archive and disposable workspace:
```powershell
cmake -S . -B entry-build -A Win32 -DSSC_NATIVE_CANDIDATE=ON
cmake --build entry-build --config Release --target SSCNative
rustup target add i686-pc-windows-msvc
python tools/pr66_fixture.py C:\scratch\Launcher-b7d9233f8636e222bbd0a5a739e1e04d2fbc8a12.zip C:\scratch\ssc-pr66-source --run-with-dll entry-build\Release\SSCNative.dll
```
Run only after source/build dependency review. No game installation or proxy replacement. If build/download fails, preserve logs, report blocker; do not substitute unknown binaries. Expected upstream fixture proof explicitly checks one initializer, triggering thread, worker start outside loader lock, adjacent dependency, INI values, unmodified source INI and disabled case. Manifest test calls actual upstream config API. Actual SSC fixture expects zero/zero/one successful load for the three conditions, no loader errors. These are fixture checks, not proof of real game compatibility.

## G1 owner acceptance and rollback (procedure prepared; installation not authorized)

1. Close all game/launcher instances. Keep primary installation untouched. Make a separate working copy for each supported combination under review: original game, original + supported Display Config, supported loader + keyboard patch where compatible. Use a user-writable non-network folder; do not request admin privileges.
2. Record exact Windows build, renderer/window mode, refresh/FPS, executable and DLL SHA-256, version of launcher/proxy, mods.json bytes/hash and existing mod folder inventory. Save a separate backup of mods.json (if present) and Saved_Data for the test copy. Baseline launch/exit, normal race, restart, reset, menu navigation, controls, audio, binding persistence per WINDOWS_BASELINE.md. Stop at any unexplained discrepancy. Record absent files as absent.
3. **Only after reviewed source/binary compatibility, source fixture and approval:** use a separately provided reviewed inert SSC package, no original/third-party file replacements. Refuse if .supreme/mods/ssc already exists. Capture its intended owned-file inventory outside the test copy. Use the existing launcher to discover SSC, leave it disabled and load_diagnostic=false. Do not alter other mods; compare their state against saved baseline. Do not manually replace winmm.dll. If compatible loader not installed, stop and document that prerequisite.
4. Launch normally through the supported launcher, check startup/menus, audio, controls, actual Time Attack selection/finish, restart/reset, normal exit/relaunch. Expected no SSC load or side effects beyond launcher discovery state and ordinary game save/log activity. Observe loader logs, hashes, exact displayed time and known behavior; absence of logs alone is insufficient. No active-load gameplay test under this step. Any crash, unexpected SSC load, core hash change, missing audio or changed controls stops the test.
5. Rollback only with game/launcher closed: remove the newly installed SSC-owned directory after confirming its inventory (do not delete unknown/user files); restore exact saved mods.json bytes in this disposable copy, or remove only the newly created state file if it was originally absent and no unrelated changes have occurred. If any unrelated modifications or uncertain state exist, stop and restore the entire disposable baseline copy instead. Never delete player runs/replays or change primary game. Compare exact original core/proxy hashes, prior mod settings/order and launch/exit again. Preserve test logs outside the game. Do not roll back normal save changes on the primary installation.
6. Acceptance evidence: baseline + installed-disabled + post-rollback actual observations per combination, exact hashes, launcher/proxy identity, SSC package fingerprint, disabled selection and logs, no unexpected modifications or native capability activation. No G1 PASS based on synthetic host alone. Further inert-active game observation and G2 require separate review. Any combination missing evidence remains BLOCKED, not supported.

No G1 test is requested until fixture and supported loader prerequisites are resolved. Existing original/keyboard-patch baseline can be observed independently if owner chooses.

## T23 result and owner baseline checkpoint
Reviewed-source fixture prerequisite PASS (runs 45798560012, 45800954974, final 45801293718/job 139844872583). Final actual upstream tests 3/3 + SSC manifest/serialized configuration tests 2/2; runtime fixture PASS and SSC load counts 0/0/1. This supersedes earlier pending/source-only statements, not G1. Actual installed loader identity and real-game compatibility remain unverified.

Owner can now collect baseline without SSC installation, on a disposable copy of a currently working installation. Close game/launcher, copy game including saves to a new private folder; run existing launcher/game normally from that copy without changing loader or mods. Follow WINDOWS_BASELINE.md: hash original files and installed winmm.dll, identify launcher version/provenance and executable hash if available; observe normal launch, menu selections, finish time, restart/reset, audio, controls, key persistence and exit/relaunch. Record Windows version, renderer/FPS cap if known, exact differences and any errors; redact personal paths. Optional snapshots from SSC source outside game (PowerShell):
```powershell
$Game = 'C:\scratch\Supreme-copy'
py -3 tools/baseline_snapshot.py snapshot "$Game" | Out-File -Encoding utf8 before.json
# Launch and exit copied game normally, then:
py -3 tools/baseline_snapshot.py snapshot "$Game" | Out-File -Encoding utf8 after.json
py -3 tools/baseline_snapshot.py compare before.json after.json
```
If Python unavailable, use Get-FileHash on existing Supreme.exe, winmm.dll, Main_Menu.dll, Supreme_Game.dll and launcher EXE if present; manual observations sufficient for initial review. Return comparison/selected hashes and observations, not private game files. Unexpected failure stops test.

Candidate remains same inert SSCInitialize, manifest 0.0.14-p0 default load_diagnostic=false, no rules. Source build and independent fixture commands above, outside game only. Eventual reviewed staging layout .supreme/mods/ssc/SSCNative.dll and mod.json, no other files; no proxy replacement. Do not stage into game or distribute now: review actual baseline, loader provenance, licensing and exact candidate package fingerprint first. Then follow existing installed-disabled and rollback acceptance steps. No G1 pass or active-load game test authorized by synthetic fixture alone.
