# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Prepare offline release source companions; never download or publish files."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def check_notices(directory):
    manifest = json.loads((directory/'sources.json').read_text())
    inventory = json.loads((directory/'components.json').read_text())
    if inventory['qt_version'] != manifest['qt_version']:
        raise ValueError('Qt notice inventory and source version do not match')
    for name in ['README.md', 'SOURCE.txt', 'LGPL-3.0.txt', 'GPL-3.0.txt',
                 'THIRD-PARTY-NOTICES.txt', 'QT-BUILD-CONFIG.txt']:
        if not (directory/name).is_file() or (directory/name).stat().st_size == 0:
            raise ValueError('Missing Qt legal document: '+name)
    if not inventory['attributions']:
        raise ValueError('Empty Qt attribution inventory')
    for item in inventory['attributions']:
        for name in item['notice_files']:
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or not name.startswith('original/'):
                raise ValueError('Unsafe notice path: '+name)
            if not (directory/name).is_file() or (directory/name).is_symlink():
                raise ValueError('Missing original notice: '+name)
    return manifest


def public_files(root):
    # Respect ignores even for a mistakenly tracked local file. No .git contents.
    result = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others',
                             '--exclude-standard'], cwd=root, check=True, capture_output=True)
    names = sorted(set(result.stdout.decode().strip('\0').split('\0'))-{''})
    ignored = subprocess.run(['git', 'check-ignore', '--no-index', '-z', '--stdin'],
                             cwd=root, input=('\0'.join(names)+'\0').encode(), capture_output=True)
    if ignored.returncode not in (0, 1):
        raise ValueError('Could not check source ignore rules')
    excluded = set(ignored.stdout.decode().strip('\0').split('\0'))
    files = []
    for name in names:
        if name in excluded:
            continue
        path = PurePosixPath(name)
        if path.is_absolute() or any(part in ('..', '.git') for part in path.parts):
            raise ValueError('Unsafe public source path: '+name)
        file = root/name
        if file.is_symlink():
            raise ValueError('Review source symlink before packaging: '+name)
        if file.is_file():  # Deleted tracked files are not part of this snapshot.
            files.append(file)
    for name in ['CMakeLists.txt', 'LICENSE', 'ADDITIONAL_TERMS.md', 'NOTICE', 'src/main.cpp']:
        if root/name not in files:
            raise ValueError('Missing application source: '+name)
    return files


def prepare(root, cache, output):
    legal = root/'third_party/qt'
    manifest = check_notices(legal)
    archives = []
    for source in manifest['sources']:
        name = source['url'].rsplit('/', 1)[1]
        path = cache/name
        if path.stat().st_size != source['size'] or sha256(path) != source['sha256']:
            raise ValueError('Qt source archive verification failed: '+name)
        archives.append(path)
    files = public_files(root)
    if output.resolve().is_relative_to(root.resolve()) and not output.resolve().is_relative_to((root/'artifacts').resolve()):
        raise ValueError('Keep generated release companions outside public source or under artifacts/')
    version = re.search(r'project\(mclauncher VERSION ([0-9.]+)',
                        (root/'CMakeLists.txt').read_text()).group(1)
    qt_name = f'qt-{manifest["qt_version"]}-corresponding-source'
    app_name = f'm2cl-{version}-source'
    output.mkdir(parents=True, exist_ok=True)
    # Complete validation before replacing any existing companion files.
    with tempfile.TemporaryDirectory(prefix='.source-stage-', dir=output) as staging:
        staging = Path(staging)
        with zipfile.ZipFile(staging/(qt_name+'.zip'), 'w', zipfile.ZIP_DEFLATED) as bundle:
            for archive in archives:
                bundle.write(archive, qt_name+'/'+archive.name, compress_type=zipfile.ZIP_STORED)
            for file in sorted(legal.rglob('*')):
                if file.is_file() and not file.is_symlink():
                    bundle.write(file, qt_name+'/notices/'+file.relative_to(legal).as_posix())
        source_hashes = {}
        with zipfile.ZipFile(staging/(app_name+'.zip'), 'w', zipfile.ZIP_DEFLATED) as bundle:
            for file in files:
                name = file.relative_to(root).as_posix()
                data = file.read_bytes()
                source_hashes[name] = hashlib.sha256(data).hexdigest()
                bundle.writestr(app_name+'/'+name, data)
            bundle.writestr(app_name+'/SOURCE-MANIFEST.json', json.dumps({
                'project_version': version, 'qt_version': manifest['qt_version'],
                'files_sha256': source_hashes}, indent=2)+'\n')
        sums = ''
        for file in sorted(staging.glob('*.zip')):
            with zipfile.ZipFile(file) as bundle:
                bad = bundle.testzip()
                if bad:
                    raise ValueError('Invalid source ZIP entry: '+bad)
            sums += sha256(file)+'  '+file.name+'\n'
        (staging/'SHA256SUMS').write_text(sums)
        (staging/'README.txt').write_text(
            'M2CL release source companions\n\n'
            'Publish BOTH source ZIP files and SHA256SUMS alongside the matching binary,\n'
            'with equivalent access. Preserve the licenses shipped in that binary.\n'
            'The Qt archive contains unchanged, SHA-256-verified upstream module sources.\n'
            'The M2CL archive is the current public source snapshot; its SOURCE-MANIFEST.json\n'
            'records each included file hash. Recreate after any release source changes.\n'
            'No files have been uploaded or published by this preparation step.\n')
        for file in staging.iterdir():
            file.replace(output/file.name)
    print(f'Prepared {len(files)} public source files and {len(archives)} verified Qt archives in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/distribution-materials')
    args = parser.parse_args()
    prepare(ROOT, args.source_cache, args.output)
