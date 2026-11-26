"""Stage reviewed PR66 source privately; optional Windows-only fixture execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'b7d9233f8636e222bbd0a5a739e1e04d2fbc8a12'
SHA256 = '2ed8a5d2e334268432c23dff26b294bbe9b2d69b22a60073f8fd42e71b66c72b'


def stage(archive, destination):
    archive, destination = Path(archive), Path(destination)
    raw = archive.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise ValueError('unreviewed source archive fingerprint')
    with zipfile.ZipFile(archive) as z:
        if z.comment.decode('ascii') != REVISION:
            raise ValueError('source revision mismatch')
        seen = set()
        total = 0
        for entry in z.infolist():
            name = PurePosixPath(entry.filename)
            mode = (entry.external_attr >> 16) & 0xffff
            if (not name.parts or name.parts[0] != 'launcher' or name.is_absolute()
                    or any(p in ('.', '..') or ':' in p or '\\' in p for p in name.parts)
                    or stat.S_ISLNK(mode) or entry.filename.casefold().rstrip('/') in seen):
                raise ValueError('unsafe source member')
            seen.add(entry.filename.casefold().rstrip('/'))
            total += entry.file_size
            if total > 32 * 1024 * 1024:
                raise ValueError('source size limit')
        destination.mkdir()  # exclusive: never overwrite an existing checkout
        z.extractall(destination)
    source = destination / 'launcher'
    tests = source / 'supreme_hook_config/tests'
    for src, dst in ((ROOT / 'tests/pr66_manifest.rs', tests / 'ssc_manifest.rs'),
                     (ROOT / 'integrations/launcher/ssc/mod.json', tests / 'ssc_manifest.json')):
        with dst.open('xb') as output:
            output.write(src.read_bytes())
    print('Reviewed PR66 archive staged; source not executed', flush=True)
    return source


def run(command, cwd, env=None, timeout=1200):
    print('RUN:', ' '.join(str(v) for v in command), flush=True)
    subprocess.run([str(v) for v in command], cwd=cwd, env=env, check=True, timeout=timeout)


def fixtures(source, dll, preview=None):
    if os.name != 'nt' or not all(shutil.which(tool) for tool in ('cargo', 'cl', 'pwsh')):
        raise RuntimeError('BLOCKED: requires Windows x86 developer shell, Cargo and PowerShell')
    dll = Path(dll).resolve(strict=True)
    env = dict(os.environ, SSC_TEST_DLL=str(dll))
    if preview is not None:
        env['SSC_CONFIG_PREVIEW'] = str(Path(preview).resolve(strict=True))
    else:
        env.pop('SSC_CONFIG_PREVIEW', None)
    target = 'i686-pc-windows-msvc'
    run(['cargo', 'test', '--locked', '-p', 'supreme_hook_config', '--target', target,
         '--test', 'dll_loading', '--test', 'ssc_manifest'], source, env)
    run(['cargo', 'build', '--locked', '-p', 'supreme_hook_proxy', '--release', '--target', target], source, env)
    proxy = source / 'target' / target / 'release/winmm.dll'
    build = source / 'fixture-build'
    build.mkdir()
    fixture = source / 'tools/dll_fixture.cpp'
    common = ['cl', '/nologo', '/std:c++17', '/EHsc', '/MT', '/W4', '/WX', '/O2']
    run([*common, fixture, '/Fe:Supreme.exe'], build, timeout=120)
    run([*common, '/LD', '/DFIXTURE_DEPENDENCY', fixture, '/link', '/OUT:fixture_dependency.dll', '/IMPLIB:fixture_dependency.lib'], build, timeout=120)
    run([*common, '/LD', '/DFIXTURE_PLUGIN', fixture, 'fixture_dependency.lib', '/link', '/OUT:fixture_plugin.dll'], build, timeout=120)
    run(['pwsh', '-NoProfile', '-File', source / 'tools/test_dll_runtime.ps1', '-Proxy', proxy], source, timeout=90)
    # Actual reviewed proxy + actual SSC inert DLL; only a newly built synthetic Supreme.exe.
    manifest = (ROOT / 'integrations/launcher/ssc/mod.json').read_bytes()
    with tempfile.TemporaryDirectory(prefix='ssc-pr66-') as temp:
        test = Path(temp)
        mod = test / '.supreme/mods/ssc'
        mod.mkdir(parents=True)
        shutil.copyfile(build / 'Supreme.exe', test / 'Supreme.exe')
        shutil.copyfile(proxy, test / 'winmm.dll')
        shutil.copyfile(dll, mod / 'SSCNative.dll')
        (mod / 'mod.json').write_bytes(manifest)
        (test / 'rd_config.txt').write_text('fixture', encoding='ascii')
        for enabled, requested in ((False, True), (True, False), (True, True)):
            state = {'active': {'order': ['ssc'], 'enabled': ['ssc'] if enabled else [],
                                'configuration': {'ssc': {'load_diagnostic': requested}}}, 'presets': {}}
            (mod.parent / 'mods.json').write_text(json.dumps(state), encoding='ascii')
            run([test / 'Supreme.exe'], test, timeout=15)
            log = (test / '.supreme/logs/supreme_hook.log').read_text(encoding='utf-8', errors='replace')
            count = log.count('DLL loaded: ssc /')
            if count != int(enabled and requested) or 'DLL mod skipped:' in log or 'DLL ssc /' in log:
                raise RuntimeError('SSC real proxy fixture load/gate mismatch')
            print(f'SSC real proxy fixture: enabled={enabled}, requested={requested}, load_count={count}', flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path)
    p.add_argument('destination', type=Path)
    p.add_argument('--run-with-dll', type=Path)
    p.add_argument('--config-preview', type=Path, help='Built SSC native preview executable; required by extended CI')
    args = p.parse_args()
    if args.config_preview and not args.run_with_dll:
        p.error('--config-preview requires --run-with-dll')
    source = stage(args.archive, args.destination.resolve())
    if args.run_with_dll:
        fixtures(source, args.run_with_dll, args.config_preview)


if __name__ == '__main__':
    main()
