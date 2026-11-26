# Immutable supplied reference inventory — P0-T14

Original supplied files are private references, not repository assets. Rechecked against pre-existing SHA-256 manifest with sha256sum -c, all 10 inputs unchanged. Source revision of upstream launcher not established; source declares 1.8.0 and includes GPL v3 LICENCE. Exact source-to-binary reproducibility not verified. See build-profile.json for per-binary game and loader fingerprints.

| Reference | SHA-256 |
|---|---|
| Display_Config-1.8.0.zip | 7cf5759b26d191c7aeff8cbc0ce8927e99c2725954d296cbfdfa41de1bb3e719 |
| Launcher-maincodeberg.zip | b25b6592626057801ca41ab4e3e1a9dc68dd6abb01fc1ba5622966d3e63ee0cc |
| SupremeLight_9617efe6.zip | a6028c9aa4042d9f1e0195e4250ed0b2fbf6ce74fff95138541808c01197a9c3 |
| Supreme_Keyboard_Persistence_v0.1.zip | ac167eba3781a12ba1f01c7f254296db8caa331435995170cfab919f2076a786 |
| Supreme_Speedrun_Project_GPT_Docs_v1.0.0.zip | 13f19bfc21f718a3159f8d347f7552ff9ca69923778e00af77ab1ef3ea6892a8 |
| SSR_PRODUCT_FLOW_ADDENDUM_v1.0.md | 7bc7ac05c7a89193d3bc3b9e703fefae5f57c1407991ab2e7642b809b2476b12 |
| Supreme_Speedrun_Mod_v0.0.1-p0.zip | 64e6c54878bf3434c8434f9f20ac248022a6ba15bded93b985df0b26c8e7242d |
| Supreme_Speedrun_Mod_v0.0.2-p0.zip | a5c5e104007db4035bcf8ab4a88525d22ac3410b36ceadda01126b0a9e1b38fc |
| Supreme_Speedrun_Mod_v0.0.3-p0.zip | ce2a944015ec473797a13254fc0f1eb927dab503bd19827a14f6a1004d0145e4 |
| Supreme_Speedrun_Mod_v0.0.4-p0.zip | e12d00a52f095fc76ccdf2d9c68e7e38245f33855a96d35e7cbc083517dc48f7 |

USER_REPORTED: keyboard persistence works on the owner's Windows installation; audio also works but causal connection is unestablished. Not a comprehensive regression or competition approval.

## T21 new immutable references

All ten previous SHA-256 values rechecked and unchanged. Eight newly supplied/downloaded references hashed before and after private static inspection. No execution, redistribution or edits.

| Reference | SHA-256 |
|---|---|
| Launcher-b7d9233f8636e222bbd0a5a739e1e04d2fbc8a12.zip | 2ed8a5d2e334268432c23dff26b294bbe9b2d69b22a60073f8fd42e71b66c72b |
| Markdown collé.md (PR66 patch text) | d3545532a02592f539c1d0c602880f68b8c08e7a4b10de18670ace14b610dbd6 |
| winmm.dll (Simone folder Launcher) | 4f42fb3ac2a774fff11622b17495299fd5af34bc6e5af3e6c3430bc7cad1ee4e |
| TrackLoader-1.0.1.zip | c097a266b4006d8ddc67895d5613f1b7aa37742d595f2dca69bd150e91a62472 |
| OnlinePlay-0.3.7.zip | 68a381f6d93ba7be07a8c70494eed10d3821a652493e1d466fdfeeff3fbb3b62 |
| XInputController-0.3.2.zip | f6ed128b9e290cfb9f6d917a227f39b32eed13cfa9616f52a782f051f77e4767 |
| CheckpointGates-1.1.1.zip | 4d422e00629e8adbc22aa5d773bc52abb86390a6219df60d67dadd8913359a4a |
| U.I. Upscaler.zip | 95093e086166707b874bb2e5d12bac361f76637fe9df2c2a81cd24791846ccc7 |

See COMMUNITY_LOADER.md for exact source provenance and binary evidence.

T22: sha256sum -c of all 18 supplied references passed after inspection; no originals modified. Source fixture staging uses exact ZIP fingerprint and fresh private destination; no third-party files added to SSC.

T23: all 18 SHA-256 reference checks passed; original Supreme_Game.dll inspected statically; reviewed PR66 source privately staged in CI after exact archive hash check. Originals not modified; no upstream source or third-party binary committed.

T24: all 18 exact references rechecked before and after research (sha256sum -c against existing manifest, 18 OK). Original Supreme_Game.dll private copy hash b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b confirmed twice. Only static inspection, no changes or game execution.

T25: computed SHA-256 of all 18 immutable references before and after static analysis and compared with inventory; all match. Private original EXE and DLL SHA-256 confirmed 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c and b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b. No original modification/execution.

T26 all 18 immutable inputs SHA-256 checked before and after static research; all match existing inventory. Private extracted original Supreme.exe, Supreme_Game.dll and Main_Menu.dll fingerprints recorded in ENGINE_MAP.md. No original execution, modification or redistribution.

### T27 reference recheck
All five mounted ZIPs (Project GPT Docs v1.0.0, Keyboard Persistence v0.1, Display Config 1.8.0, SupremeLight 9617efe6, Launcher-maincodeberg) matched recorded inventory before/after static research. Other 13 previously inventoried inputs were not mounted and not freshly rechecked. Private extracted original Supreme.exe SHA-256 615fd47962cd174b51f1e466acc7b84d64ce87526c52271c9e12200c730a5c8c; Supreme_Game.dll b1cc9c71f33b729f67f0cc28d8f1da8aec72e76509373c506abaccfce7d0350b; Main_Menu.dll aed413babf03cb64596415f8fe9a1af043da3c2f6590e4dffe1dd37ac71098f9. No execution, edits or redistribution.
