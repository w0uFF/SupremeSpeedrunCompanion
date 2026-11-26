# T20 independent Windows capture validation

Status: automated own-window pixel capture/encode PASS, audio signal collection BLOCKED on hosted Windows runner. This procedure is for an optional standalone interactive Windows validation, not game installation or G1. No unrelated application capture. Use a disposable checkout of exact source revision eda75eb2adc1d7ed319526b8a083696658a7bbf0, current MSVC with Win32 support, Windows SDK, CMake and Python. Windows must have an active playback endpoint and an interactive desktop. Keep audio at a comfortable volume; the probe plays a one-second low-amplitude test tone from its own process. It saves no PCM samples. Do not select or record another process or install an audio driver just for this check.

From the checkout root in a Developer PowerShell:

```powershell
cmake -S . -B capture-build -A Win32 -DSSC_CAPTURE_PROBES=ON -DSSC_MEDIA_PROBE=ON
cmake --build capture-build --config Release --target p0_process_audio_probe p0_wgc_probe p0_media_probe
ctest --test-dir capture-build -C Release -R "^p0_process_audio_sample_probe$" --verbose --no-tests=error
ctest --test-dir capture-build -C Release -R "^p0_captured_video_probe$" --verbose --no-tests=error
```

Audio acceptance: actual own-process PCM packets, both positive and negative samples >=2000, at least two valid QPC samples and zero timestamp sanity outliers; successful exit with "Own-process controlled PCM signal observed with QPC timestamp sanity check; no audio stored". Report the printed packet/frame, silent, discontinuity, timestamp-error, position-gap, regression and QPC counters; report Windows version and whether playback endpoint was active. A skip (exit 77) is BLOCKED, never a successful signal test; in hosted CI waveOutOpen returned MMRESULT=2 (bad device ID), before capture Start/GetBuffer. Unexpected nonzero exit is FAIL. No audio sample signal, actual A/V synchronization or cross-app isolation is established by activation alone. Capture counts may include silence or source discontinuities; do not infer missing gameplay from this test. No gameplay or competitive verdict.

Video acceptance: actual dark/bright 160x96 own-window crop, verified luma/chroma, encoder finalizes and decodes 12 frames. Captured frames repeated with synthetic timestamps and synthetic audio; not continuous capture. Do not use the test with any other application's window. Temporary files cleaned by Python wrapper. G1 remains blocked.
