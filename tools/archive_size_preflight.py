"""Read-only size preflight for the existing stored-ZIP writer, not admission."""
from dataclasses import dataclass
import json
from pathlib import Path
import stat

import evidence_archive as archive
import evidence_inventory as inventory
import attempt_settings_evidence as settings_evidence
from baseline_snapshot import linked
import run_manifest


@dataclass(frozen=True)
class Result:
    state: str
    payload_bytes: int | None = None
    manifest_bytes: int | None = None
    container_upper_bytes: int | None = None


def inspect(root, selected, *, metadata=None):
    """Explicit closed staging only. Never open payloads, create output or authorize export.

    A size-fit result is advisory for this observation, not a content/safety check.
    Reuse normal inventory, writer and verifier for any subsequent explicit export.
    """
    try:
        if not isinstance(selected, dict) or not 1 <= len(selected) <= 64:
            return Result('invalid_selection')
        names = [inventory.safe_name(name) for name in selected]
        if len({name.lower() for name in names}) != len(names):
            return Result('invalid_selection')
        if any(not isinstance(role, str) or role not in inventory.ROLES for role in selected.values()):
            return Result('invalid_selection')
        root = Path(root)
        if linked(root) or not root.is_dir():
            return Result('unsupported_source')
        entries = []
        for name in sorted(names):
            path = root / name
            if linked(path):
                return Result('unsupported_source')
            info = path.stat()
            if not stat.S_ISREG(info.st_mode) or info.st_size < 0:
                return Result('unsupported_source')
            # Placeholder of the exact serialized digest width, never a verified hash.
            entries.append({'path': name, 'role': selected[name], 'size': info.st_size, 'sha256': '0' * 64})
    except (TypeError, ValueError):
        return Result('invalid_selection')
    except OSError:
        return Result('unavailable')
    total = sum(entry['size'] for entry in entries)
    if any(entry['size'] > inventory.MAX_BYTES for entry in entries):
        return Result('inventory_file_too_large', total)
    if total > archive.MAX_TOTAL:
        return Result('archive_payload_too_large', total)
    if metadata is not None and any(entry['path'].lower() == settings_evidence.NAME
                                    and entry['size'] > settings_evidence.MAX_BYTES for entry in entries):
        return Result('settings_member_too_large', total)
    try:
        data = ({'schema': 1, 'scope': 'experimental_evidence_archive', 'files': entries}
                if metadata is None else run_manifest.build(metadata, entries))
        manifest_bytes = len(json.dumps(data, sort_keys=True, separators=(',', ':')).encode('utf-8'))
    except (TypeError, ValueError, OverflowError, RecursionError):
        return Result('invalid_metadata', total)
    if manifest_bytes > archive.MAX_MANIFEST:
        return Result('manifest_too_large', total, manifest_bytes)
    # Current writer: seekable, ZIP_STORED, ASCII names, no comments, forced
    # local ZIP64 for payload entries. Conservative allowance for every entry:
    # 30 local + 46 central + twice name bytes + 20 local ZIP64 + 24 descriptor.
    # This is NOT a generic ZIP size estimator or a replacement for verification.
    member_names = [archive.MANIFEST, *names]
    directory = sum(46 + len(name) for name in member_names)
    upper = total + manifest_bytes + 22 + sum(30 + 46 + 2 * len(name) + 20 + 24 for name in member_names)
    if directory > archive.MAX_DIRECTORY or upper > archive.MAX_TOTAL + archive.MAX_MANIFEST + archive.CHUNK:
        return Result('container_bound_unavailable', total, manifest_bytes, upper)
    return Result('within_size_limits', total, manifest_bytes, upper)
