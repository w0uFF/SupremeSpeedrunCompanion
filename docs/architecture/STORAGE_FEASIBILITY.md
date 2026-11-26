# P0-T19 — Native storage adapter evaluation

PROPOSED: use a transactional SQLite backend behind the existing C++ Repository and PreferencesStore ports, outside the game thread. Do not select or distribute a dependency before license/release review. Existing Python SQLite tool remains an offline reference, not a production schema or backend. Preserve evidence archives as immutable source material; database indexes reference exact manifest digests, annotations separate.

Options: system WinSQLite (winsqlite3.dll on Windows 10+, serviced with OS, varying versions/features and OS minimum; verify actual Win32 ABI and required exports), reviewed pinned SQLite amalgamation (reproducible dependency, distribution and security-update obligations), custom flat-file index (no dependency, but would need implement transactions, locking, migrations and recovery; not preferred). Keep journal/database local, no network share support until tested. Start with rollback journal and explicit FULL synchronous policy; WAL may be considered after concurrency and checkpoint/backup tests. No durability claim based on PRAGMA alone.

VERIFIED_AUTOMATED, local Debian GCC 14.2.0 and SQLite 3.46.1: standalone tools/storage/sqlite_probe.cpp, using installed system development library, no repository runtime dependency. Demonstrates successful commit, duplicate primary-key failure, explicit rollback of attempted annotation change, foreign-key rejection, zero time and persistence after close/reopen. Refuses an existing file in a trusted, quiescent private directory (not a secure atomic creation primitive). Tests tiny synthetic rows and fixed SQL only; not a Repository implementation, parameterized query design, production migration, crash/power loss, adversarial DB, Unicode path or Windows test.

Reproduce on Linux with SQLite development headers/library installed:
```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror tools/storage/sqlite_probe.cpp -lsqlite3 -o /tmp/ssc-sqlite-probe
d=$(mktemp -d)
/tmp/ssc-sqlite-probe "$d/run.db" # expected exit 0
/tmp/ssc-sqlite-probe "$d/run.db" # expected exit 2, no overwrite
```

Not included in default CMake/CI or any release. Next: separate WinSQLite native SDK probe, version/feature guard, prepared-statement binding and transaction/recovery tests, then durable adapter behind Repository with exact manifest linkage and immutable conflicts. Sources: https://learn.microsoft.com/en-us/dotnet/standard/data/sqlite/custom-versions ; https://sqlite.org/atomiccommit.html ; https://sqlite.org/wal.html . No game files involved, no G1/G4 advancement.

T20 adds optional WinSQLite SDK probe (NOT_RUN at preparation): system winsqlite3 import library, UTF-16 path, prepared statements with quoted ID, nullable IGT and zero RTA, explicit rollback on duplicate, foreign keys and close/reopen, exclusive test-path refusal in trusted temp directory. No production adapter or bootstrap dependency. Windows hosted execution required.

T20 initial build failed C1083 missing winsqlite3.h in hosted SDK (run 45783668360). Use system-directory-only LoadLibraryExW and fixed-arity documented SQLite system ABI (stdcall on Win32), resolve required symbols before opening. No callbacks supplied, no implementation copied, no header/import library needed. Standalone separate job; no production backend decision. Reference: https://www.sqlite.org/compile.html (SQLITE_APICALL), https://www.sqlite.org/c3ref/open.html and https://www.sqlite.org/capi3ref.html. Runtime ABI verification pending.

T20 actual Win32 storage-probe PASS: run 45783962593 job 139785959280, exact code commit eda75eb2adc1d7ed319526b8a083696658a7bbf0. System WinSQLite 3.53.4, prepared binding, null/zero, rollback, foreign keys, reopen passed, Unicode temporary path and refusal of existing DB checked by Python wrapper. 1/1 CTest. No production adapter, crash recovery, migration, older Windows/SQLite or release claim. Use reviewed SDK/header or documented ABI boundary with version and export checks in production; no callbacks exercised.
