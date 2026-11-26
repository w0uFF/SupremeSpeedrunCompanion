# Project status

Revision v0.0.9-p0 (offline increment). Baseline GitHub main 5c56048cef89aa868abebdc2325a11d3a2c158b9. G0 recorded; G1 BLOCKED. No native hooks, game installation, live recording, native menu, live race timing or online submission.

P0-T17 final Windows CI verified from actual logs: 16/16 Debug and Release and 1/1 synthetic H.264 decode/encode, release skipped (run 45759479502). P0-T18 core: bounded worker with injected sink, explicit per-stream timestamp validation and counters, drain/cancel/failure lifecycle; source-level C++ run repository port; schema 2 metadata and verified no-replace partial publication/recovery with schema 1 compatibility. Isolated Linux tests passed (worker Debug, optimized 50 runs, UBSan; repository; six manifest/publication tests). Local full checkout unavailable due to DNS; no full local CTest claim. Hosted 19-target CI pending. All ten reference hashes unchanged. No actual encoder attached to worker, no production storage backend, complete submission/import or durability guarantee.

Next: validate exact hosted CI; isolated AAC/A-V synthetic feasibility, native result producer research, backend and recording lifecycle integration. Owner disposable baseline and reviewed extension boundary needed before installed-disabled G1 test; Xinfe not a dependency for isolated work. License and release approval pending; no public releases.
