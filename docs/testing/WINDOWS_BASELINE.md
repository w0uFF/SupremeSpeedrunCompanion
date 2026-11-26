# Windows baseline evidence — G1 prerequisite

No companion installation. Close game and launcher; make a separate user-writable disposable copy of the working game including Saved_Data. Preserve the primary installation. No administrator privileges or security-policy bypass.

1. In copied game root run PowerShell Get-FileHash .\Supreme.exe, .\Display_Config.exe, .\winmm.dll, .\Main_Menu.dll, .\Supreme_Game.dll -Algorithm SHA256. Optional: from source root run python tools/preflight.py "<copied game root>". Unknown identity is not cheating; all native capabilities remain disabled.
2. Launch via Display Config normally. Check main menu, audio, renderer; complete a race, restart and reset-to-track separately, and check result and audio.
3. Change Jump and Cant and another action, verify in race, exit normally. Note whether Saved_Data\Keyboard_Bindings.bin exists and is 36 bytes. Relaunch and verify persistence. If absent, note possible redirection; never move or overwrite data.
4. Optional read-only module listing: Get-Process Supreme | Select-Object -ExpandProperty Modules | Where-Object ModuleName -eq 'winmm.dll' | Select-Object ModuleName,FileName. If denied, mark unavailable, do not elevate. Redact personal paths.
5. Exit and compare hashes before/after. Record Windows version, renderer, mode, refresh rate, FPS cap, actual observations and errors. A baseline passes when both launches/exits, race result/restart/reset, expected audio and changed key persistence work. This does not pass G1; later separate reviewed native candidate needs installed-disabled smoke tests and rollback.
6. Optional: save a replay via the original UI if available, note actual displayed result, run python tools/replay_header.py on a copied replay and compare candidate time. No official timing conclusion without actual evidence.

Do not enable an injector, replace winmm.dll, edit game files, or install this repository into the game.
