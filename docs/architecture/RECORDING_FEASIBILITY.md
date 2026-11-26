# Recording backend feasibility — P0-T16

Status DOCUMENTATION RESEARCH ONLY, no encoder/capture implementation or capability test. Reviewed 2026-11-26:
- Windows Graphics Capture CreateForWindow: https://learn.microsoft.com/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow (Windows 10 1903+).
- WASAPI loopback: https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording (endpoint mixed audio, shared mode).
- Process-filtered loopback: https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ne-audioclient_activation_type (minimum build 20348).
- Media Foundation encoding: https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--using-the-sink-writer-to-encode-video and https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--encoding-an-mp4-file- (COM/MF initialization, sink writer, finalized output, H.264/AAC MP4 candidate).

Proposed separate capture worker, explicit selected window, no injection or competing proxy. Windows Graphics Capture candidate for windowed/borderless renderer; fullscreen, legacy renderer, overlay capture, frames and device loss untested. Prefer process-filtered audio where available; endpoint-wide capture risks unrelated private audio and requires explicit user choice, never silently fallback. Hardware acceleration and software fallback depend on actual codec/device enumeration and licenses; no third-party codec distribution selected. Media Foundation requires worker-owned lifetime and nonblocking game thread; retain timestamp discontinuity and dropped-frame warnings. Finalize asynchronously, preserve incomplete evidence; crash recoverability and output format not established. QPC samples from same monotonic timebase only after verified mapping; never infer race finish from duration. Next isolated synthetic encode/decode probe (no game or real display), then owner interactive window/audio smoke tests. Hosted Windows CI can test standalone SDK/API usage, not real GPU, gameplay capture or G3. Keep existing capture-boundary ledger; no capture functionality enabled.

## T17 standalone Media Foundation probe (implemented, Windows execution pending)

tools/media/mf_probe.cpp is an opt-in Windows x86 executable. Initializes COM and Media Foundation, uses sink writer with 160x96 NV12 synthetic frames at 25 fps (12 frames, two luma levels, neutral chroma), H.264 output and 500 kbps target, timestamps in 100 ns units. Finalizes MP4, opens Source Reader, requests NV12 decode, checks H.264 subtype, dimensions, 12 samples, increasing timestamps, EOS and coarse first-luma pattern. No actual desktop/game/audio capture or hardware acceleration guarantee. Failure prints stage/HRESULT and exits nonzero; Python wrapper uses temporary directory and 90-second process timeout, CTest 120 seconds. No upload or persistence of synthetic video. Separate Windows hosted media-probe job; core build uses default OFF. Release gate remains unchanged for offline diagnostic only; no media asset included. Build and run:
```powershell
cmake -S . -B media-build -A Win32 -DSSC_MEDIA_PROBE=ON
cmake --build media-build --config Release --target p0_media_probe
ctest --test-dir media-build -C Release -R "^P0_media_probe$" --verbose --no-tests=error
```
Sources reviewed: https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--using-the-sink-writer-to-encode-video ; https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--encoding-an-mp4-file- ; https://learn.microsoft.com/en-us/windows/win32/medfound/processing-media-data-with-the-source-reader ; https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-encoder . No Windows SDK or runtime locally; do not mark PASS until exact CI logs confirm. Interactive G3, audio, capture performance, device-loss and encoder recovery remain NOT_RUN.

First hosted media job 139704913379 failed compilation with MSVC C4245/C2220 on SDK enum-to-DWORD stream selectors; explicit casts applied without relaxing warnings. Encode/decode execution remains unverified until the follow-up job completes.

VERIFIED_AUTOMATED: Windows x86 run 45759324352, job 139705335489, commit f3dded0e8accd0c9e6dc1a841648478dc6a950a3. MSVC 19.52.37029.0, strict warnings retained. Actual CTest verbose log: Decoded 12 H264 frames, 160x96, monotonic timestamps and luma pattern verified. 1/1 passed, 1.54 s. No output uploaded. This establishes synthetic file round-trip in the hosted environment only, not G3 or interactive capture.
