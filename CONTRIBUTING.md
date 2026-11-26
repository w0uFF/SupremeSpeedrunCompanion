# Contributing

Use feature-oriented MVC and ports-and-adapters. Keep native addresses and game-specific calls inside versioned engine adapters; public contracts must be testable without the game. Preserve existing gameplay and upstream compatibility. No speculative hooks, anti-debugging, or mandatory anti-cheat.

Build and test as described in README. Add a regression test with each change, document public contract changes and distinguish static, automated, runtime and user-reported evidence. Work on small reviewed branches; never modify the original reference archives. Do not commit proprietary game binaries, upstream DLLs, personal data or credentials. Confirm project license before distributing or accepting contributions under it. The maintainer controls release approval; moderators control competition policy.

Current source is offline-only. G1 requires actual Windows installed-disabled launch/exit and rollback evidence; CI compilation is not a substitute. Keep PROJECT_STATUS.md, changelog, roadmap, decisions and known issues synchronized.
