# Launcher and SSC-specific profiles — proposed, not upstream-approved

## Player objective
SSC is a workflow companion, not a new game mode or substitute for the community's Online Time Attack replay upload. User feedback relayed from Xinfe: current OBS/overlay/timer setup interrupts die-and-retry play; optional launcher-distributed mods are a promising fit. This is product feedback, not a license grant, technical approval or leaderboard decision. Keep the product-first README clear, optionality explicit, and future features visibly distinguished from implemented behavior.

## Current prototype
Use PR66's existing mod parameter controls for the 21 SSC fields, including a false diagnostic load switch. The loader reads active.order intersect active.enabled, then evaluates enable_if. SSC uses whole-group launcher overrides only when requested. A separate Tkinter development editor (tools/ssc_profile_gui.py) manages a caller-chosen private SSC profile store; create, duplicate, rename, delete inactive, select, edit and save are represented. No game files, actual launcher state or other mods touched. No GUI runtime in the native DLL.

## Upstream proposal, if maintainers want it
- Keep existing launcher global presets, mod order and enablement intact. Add a generic optional mod settings-editor mechanism or explicitly reviewed SSC-specific view rather than hardcode an unreviewed SSC dependency.
- Render an SSC profile list with active profile, editable settings grouped by Recording, Overlay/Player and Storage/Hotkeys; profile CRUD, import/export with validation and no silent replacement. File picker for package directory, hardware-supported modes only after capability detection. No execution of arbitrary commands from untrusted manifests.
- Use an explicitly agreed private configuration path/ownership boundary, same versioned schema for native and UI adapters, explicit validation and conflict reporting. Launch-time override snapshot is separate from persisted SSC profile. Use single-writer discipline, do not implicitly save every in-game change back to launcher.
- If upstream declines or delays the extension, keep normal manifest controls and optional SSC-owned standalone settings interface. No upstream fork required for SSC; upstream changes only with review.

## Acceptance
Profile edits persist after restarting launcher or game; GUI and native parser agree; no other mod's order/enablement/settings or preset changes; malformed/unknown schema preserved; missing profile/path and unsupported recording mode reported, not silently corrected; disabled SSC has no game effects; no writes to active recording configuration. Distinguish profile names from competition categories. No actual launcher UI or game runtime validated. Future in-game settings use the same contract, not a second incompatible store.

T23 evidence: actual PR66 source parses all manifest fields and passes runtime gating, and Rust save_mods_state output feeds native read-only settings preview. Standalone profile editor supports single-profile import/export with package path stripped, collision refusal and stale state check. Actual launcher GUI interaction remains NOT_RUN; this is a tested configuration contract, not an integrated launcher page or in-game feature. No upstream changes required for existing declarative controls.
