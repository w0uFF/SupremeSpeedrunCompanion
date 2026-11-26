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
