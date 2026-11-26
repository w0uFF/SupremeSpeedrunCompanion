# Windows CI and development releases (P0-CI01)

Decision: official GitHub-hosted windows-latest runner, Visual Studio default generator, -A Win32, Debug and Release, standard-library Python tests. No self-hosted runner or Actions artifact storage. Checkout pinned to v4.2.2 commit. Record the runner/toolchain versions from actual logs; windows-latest is a moving image, not a fully reproducible compiler environment.

The workflow runs on main pushes and pull requests; read-only build job. It requires nine or more CTest targets and fails on configure, build or test failure. A source-less bootstrap commit skips compilation explicitly. Release job only runs on main push after CI passes and SSC_RELEASE_APPROVED equals true, with contents:write scoped to that job. It rebuilds and tests Release and checks the LICENSE file exists and the diagnostic PE machine is x86. Packages only the offline diagnostic and license/readme, not game files.

To enable publication after licensing and owner approval: repository Settings > Secrets and variables > Actions > Variables > New repository variable: SSC_RELEASE_APPROVED=true. The next successful main push publishes a development pre-release tagged dev-<full commit SHA>. The release manifest is schema 1 with channel=development, installable=false, loader_gate=blocked, architecture=x86, archive name, checksum and source commit. GitHub Releases assets, not Actions artifacts. Never mark as latest stable. Re-running the same commit will not replace an existing release; duplicate release creation fails safely. Disable publication by removing or setting the variable false. Existing releases remain unchanged.

This is not a launcher update contract or validated installer. Upstream review, stable channel approval, license compliance, manifest evolution and rollback tests required before allowing a launcher to install anything. Protect main against unreviewed contributions when practical. Never use pull_request_target to execute untrusted code with release credentials.

Baseline run 45751578053 was verified from actual logs (Windows x86 7/7 Debug and Release; release skipped). v0.0.5 raises the expected test floor to eight. No change to release gating or artifact policy.

Run 45752930744 verified from logs (v0.0.5 Windows x86, 8/8 Debug and Release, release skipped). v0.0.6 requires nine CTest targets. Release permission and gating unchanged.
