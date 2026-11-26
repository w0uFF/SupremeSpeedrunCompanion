"""Explicit synthetic-only external retry adapter; journal reconciliation stays separate."""
from dataclasses import dataclass
from pathlib import Path
from synthetic_admission import Journal
from synthetic_journal_store import Store, ReviewRequired
from synthetic_recovery import observe


ACTIONS = ('retry_archive_publication', 'retry_archive_alias_cleanup', 'retry_index')


@dataclass(frozen=True)
class RetryPlan:
    action: str
    journal: Journal
    journal_directory: Path
    archive_directory: Path


def plan(store: Store, directory, lookup):
    """Read-only, under the trusted single-owner contract and same bound index port."""
    try:
        state = store.inspect()
        if state.latest is None or state.pending is not None:
            return None
        decision = observe(store, directory, lookup)
        if decision.action not in ACTIONS:
            return None
        return RetryPlan(decision.action, state.latest, store.directory.resolve(), Path(directory).resolve())
    except (OSError, ValueError):
        return None


def execute(store: Store, directory, lookup, insert, expected: RetryPlan):
    """Execute exactly one explicitly selected operation; never append a journal snapshot.

    lookup and insert must address the same trusted synthetic fixture database.
    No competing writers or hostile path changes are supported.
    """
    if not isinstance(expected, RetryPlan) or plan(store, directory, lookup) != expected:
        raise ReviewRequired('external retry plan changed or unsupported')
    from index_archive import checked
    from archive_publish import publish
    j = expected.journal
    final = Path(directory) / ('synthetic-' + j.context.run_id + '.zip')

    def bound_report(path):
        report = checked(path, 'synthetic')
        m = report['manifest']
        if (report['run_id'] != j.context.run_id or report['manifest_sha256'] != j.digest
                or m['attempt_status'] != j.terminal
                or (j.capture_complete and m['capture_status'] != 'finalized')):
            raise ReviewRequired('external archive evidence changed')
        return report

    if expected.action in ('retry_archive_publication', 'retry_archive_alias_cleanup'):
        publish(final, bound_report)
        bound_report(final)
        return 'published' if expected.action == 'retry_archive_publication' else 'alias_cleaned'
    if expected.action == 'retry_index':
        report = bound_report(final)
        answer = insert(report, 'synthetic')
        if answer not in ('inserted', 'identical', 'conflict'):
            raise ReviewRequired('unexpected index result; re-inspect')
        return answer
    raise ReviewRequired('unsupported external retry')
