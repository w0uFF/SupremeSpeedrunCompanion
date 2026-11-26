"""Experimental stored ZIP evidence archive; verifies without extracting or trusting eligibility."""
import hashlib
import json
from pathlib import Path
import re
import stat
import struct
import zipfile
from evidence_inventory import inventory, safe_name, ROLES
from baseline_snapshot import linked
import run_manifest
import attempt_settings_evidence as ase

MAX_TOTAL = 1024**3
MAX_DIRECTORY = 128 * 1024
MAX_MANIFEST = 64 * 1024
CHUNK = 1024 * 1024
MANIFEST = 'manifest.json'


def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError('duplicate JSON key')
        obj[key] = value
    return obj


def stream_digest(source, limit, destination=None):
    digest = hashlib.sha256()
    count = 0
    while block := source.read(CHUNK):
        count += len(block)
        if count > limit:
            raise ValueError('entry exceeds declared bound')
        digest.update(block)
        if destination is not None:
            destination.write(block)
    return count, digest.hexdigest()


def check_directory(path):
    # This prototype accepts only a single-disk, comment-free, classic EOCD.
    # ZIP64 local member headers remain allowed; ZIP64 central directories do not.
    size = path.stat().st_size
    if size < 22:
        raise ValueError('truncated archive')
    with path.open('rb') as handle:
        if size >= 42:
            handle.seek(-42, 2)
            if handle.read(4) == b'PK\x06\x07':
                raise ValueError('ZIP64 central directory unsupported')
        handle.seek(-22, 2)
        signature, disk, start_disk, disk_count, count, length, offset, comment = struct.unpack('<4s4H2IH', handle.read(22))
    if (signature != b'PK\x05\x06' or disk or start_disk or comment or disk_count != count
            or not 2 <= count <= 65 or length > MAX_DIRECTORY or offset + length != size - 22):
        raise ValueError('unsupported central directory')
    return count


def verify(path, *, include_manifest=False):
    # No extraction. Preflight physical size bounds; trusted file path, untrusted contents.
    if linked(Path(path)) or not Path(path).is_file() or Path(path).stat().st_size > MAX_TOTAL + MAX_MANIFEST + CHUNK:
        raise ValueError('unsupported archive file')
    count = check_directory(Path(path))
    with zipfile.ZipFile(path, 'r') as archive:
        entries = archive.infolist()
        if len(entries) != count:
            raise ValueError('directory count mismatch')
        names = [e.filename for e in entries]
        if not 2 <= len(entries) <= 65 or len(set(n.lower() for n in names)) != len(names) or MANIFEST not in names:
            raise ValueError('invalid member set')
        for entry in entries:
            mode = (entry.external_attr >> 16) & 0xFFFF
            if (entry.orig_filename != entry.filename or entry.is_dir() or stat.S_IFMT(mode) not in (0, stat.S_IFREG)
                    or entry.compress_type != zipfile.ZIP_STORED or entry.flag_bits & 1
                    or entry.compress_size != entry.file_size):
                raise ValueError('unsupported ZIP member')
            if entry.filename != MANIFEST:
                safe_name(entry.filename)
        manifest_entry = archive.getinfo(MANIFEST)
        if manifest_entry.file_size > MAX_MANIFEST:
            raise ValueError('manifest too large')
        with archive.open(manifest_entry) as stream:
            raw = stream.read(MAX_MANIFEST + 1)
        if len(raw) > MAX_MANIFEST:
            raise ValueError('manifest too large')
        manifest = json.loads(raw, object_pairs_hook=unique_object)
        if (not isinstance(manifest, dict) or type(manifest.get('schema')) is not int
                or manifest['schema'] not in (1, 2) or manifest.get('scope') != 'experimental_evidence_archive'
                or not isinstance(manifest.get('files'), list)
                or (manifest['schema'] == 1 and set(manifest) != {'schema', 'scope', 'files'})):
            raise ValueError('unsupported manifest')
        listed = set()
        total = 0
        settings_digest = None
        for item in manifest['files']:
            if not isinstance(item, dict) or set(item) != {'path', 'role', 'size', 'sha256'}:
                raise ValueError('invalid manifest entry')
            name = safe_name(item['path'])
            size, digest, role = item['size'], item['sha256'], item['role']
            if (name in listed or name not in names or type(size) is not int or not 0 <= size <= MAX_TOTAL
                    or not isinstance(digest, str) or re.fullmatch('[0-9a-f]{64}', digest) is None
                    or not isinstance(role, str) or role not in ROLES):
                raise ValueError('invalid member metadata')
            listed.add(name)
            total += size
            if total > MAX_TOTAL or archive.getinfo(name).file_size != size:
                raise ValueError('size mismatch or archive exceeds bound')
            with archive.open(name) as stream:
                if manifest['schema'] == 2 and name.lower() == ase.NAME:
                    if name != ase.NAME or role != 'diagnostic' or size > ase.MAX_BYTES:
                        raise ValueError('unsupported attempt settings member')
                    settings_raw = stream.read(ase.MAX_BYTES + 1)
                    if (len(settings_raw), hashlib.sha256(settings_raw).hexdigest()) != (size, digest):
                        raise ValueError('evidence digest mismatch')
                    ase.validate(settings_raw, manifest)
                    settings_digest = digest
                elif stream_digest(stream, size) != (size, digest):
                    raise ValueError('evidence digest mismatch')
        if listed != set(names) - {MANIFEST}:
            raise ValueError('unlisted member')
    report = {'schema': manifest['schema'], 'files_verified': len(listed), 'bytes_verified': total,
              'integrity_matches_manifest': True, 'eligibility_verified': False, 'complete_submission': False}
    if manifest['schema'] == 2:
        run_manifest.validate(manifest)
        if raw != json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode('utf-8'):
            raise ValueError('noncanonical schema 2 manifest')
        report.update(run_id=manifest['run_id'], manifest_sha256=hashlib.sha256(raw).hexdigest())
        if settings_digest is not None:
            report.update(attempt_settings_sha256=settings_digest, attempt_settings_consistent=True)
    if include_manifest:
        report['manifest'] = manifest
    return report


def create(root, selected, output, *, metadata=None):
    """Exclusive new output; on failure remove only the partial file created here."""
    root, output = Path(root), Path(output)
    report = inventory(root, selected)
    entries = report['files']
    if sum(e['size'] for e in entries) > MAX_TOTAL:
        raise ValueError('archive exceeds total bound')
    data = ({'schema': 1, 'scope': 'experimental_evidence_archive', 'files': entries}
            if metadata is None else run_manifest.build(metadata, entries))
    manifest = json.dumps(data, sort_keys=True, separators=(',', ':')).encode('utf-8')
    if len(manifest) > MAX_MANIFEST:
        raise ValueError('manifest exceeds bound')
    # Require a separate output directory; do not write into the evidence staging directory.
    if root.resolve() in (output.parent.resolve(), *output.parent.resolve().parents):
        raise ValueError('output must be outside staging directory')
    with output.open('xb') as handle:
        try:
            with zipfile.ZipFile(handle, 'w', compression=zipfile.ZIP_STORED) as archive:
                archive.writestr(MANIFEST, manifest)
                for item in entries:
                    source_path = root / item['path']
                    if linked(source_path) or not source_path.is_file():
                        raise ValueError('source changed')
                    with source_path.open('rb') as source, archive.open(item['path'], 'w', force_zip64=True) as dest:
                        if stream_digest(source, item['size'], dest) != (item['size'], item['sha256']):
                            raise ValueError('source changed')
            handle.flush()
        except BaseException:
            handle.close()
            output.unlink()
            raise
    try:
        return verify(output)
    except BaseException:
        output.unlink()
        raise
