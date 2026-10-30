# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Collect original Qt license notices from verified source archives and SDK SBOMs.

No network access and no source extraction to the filesystem. The archive hashes
are pinned in third_party/qt/sources.json. Runtime dependencies are resolved from
the selected SDK's SPDX metadata, not from every installed development module.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import tarfile

ROOT = Path(__file__).resolve().parent.parent


def normalize(value):
    return re.sub(r'[^a-z0-9]', '', value.lower())


def source_checks(path):
    result, name = {}, None
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith('FileName: ./'):
            name = line[len('FileName: ./'):]
        elif name and line.startswith('FileChecksum: SHA1: '):
            result[name] = line.rsplit(' ', 1)[1]
    return result


def read_archive(path, expected, checks):
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('Source archive SHA-256 mismatch: '+path.name)
    legal, attributions, mismatches, compared = {}, [], [], 0
    prefix = path.name.removesuffix('.tar.xz')+'/'
    with tarfile.open(path, 'r|xz') as archive:
        for member in archive:
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            name = member.name[len(prefix):]
            if name.startswith('/') or '..' in PurePosixPath(name).parts:
                raise ValueError('Unsafe source archive path: '+name)
            basename = PurePosixPath(name).name.lower()
            keep = ('/LICENSES/' in '/'+name or any(term in basename for term in
                    ['license', 'licence', 'copying', 'copyright', 'notice', 'patents'])
                    or basename in ['authors', 'readme', 'readme.md', '.tag'])
            attribution = basename == 'qt_attribution.json'
            if not (keep or attribution or name in checks):
                continue
            data = archive.extractfile(member).read()
            if name in checks:
                compared += 1
                if hashlib.sha1(data).hexdigest() != checks[name]:
                    mismatches.append(name)
            if keep:
                legal[name] = data
            if attribution:
                value = json.loads(data, strict=False)
                for item in value if isinstance(value, list) else [value]:
                    attributions.append((name, item))
    # Qt's release archive adds a release stamp; it is not compiled source.
    if any(name != '.tag' for name in mismatches):
        raise ValueError('SDK/source content differs: '+repr(mismatches[:12]))
    if compared < 100:
        raise ValueError('Too few SDK source files could be checked')
    return legal, attributions, {'files_compared': compared,
                                  'release_metadata_differences': mismatches}


def sdk_inventory(prefix, version):
    documents = {}
    for module in ['qtbase', 'qtsvg', 'qtimageformats']:
        path = prefix/'sbom'/f'{module}-{version}.spdx.json'
        documents[module] = json.loads(path.read_text())
    packages = {p['SPDXID']: p for d in documents.values() for p in d['packages']}
    files = {p['SPDXID']: p for d in documents.values() for p in d.get('files', [])}
    dependencies, contained = {}, {}
    for document in documents.values():
        for rel in document['relationships']:
            source, target = rel['spdxElementId'], rel['relatedSpdxElement']
            if target.startswith('DocumentRef-'):
                target = target.split(':', 1)[1]
            if rel['relationshipType'] == 'DEPENDS_ON':
                dependencies.setdefault(source, set()).add(target)
            if rel['relationshipType'] == 'CONTAINS':
                contained.setdefault(source, set()).add(target)
    roots, runtime_paths = [], []
    for ident, package in packages.items():
        primary = '-qt-module-' in ident and package['name'] in {
            'Core', 'Gui', 'Widgets', 'Network', 'Concurrent', 'Svg'}
        selected_plugin = False
        if '-qt-plugin-' in ident:
            for child in contained.get(ident, []):
                name = files.get(child, {}).get('fileName', '')
                accepted = name.startswith(('./plugins/imageformats/', './plugins/styles/',
                                            './plugins/tls/', './plugins/networkinformation/'))
                # This baseline is a macOS SDK, not a claim about all platforms.
                accepted |= name.startswith('./plugins/platforms/') and 'qcocoa' in name
                selected_plugin |= accepted
        if primary or selected_plugin:
            roots.append(ident)
            for child in contained.get(ident, []):
                file = files.get(child, {})
                if file.get('fileName'):
                    runtime_paths.append(file['fileName'].removeprefix('./'))
    seen, todo = set(), list(roots)
    while todo:
        ident = todo.pop()
        if ident in seen:
            continue
        seen.add(ident)
        todo.extend(dependencies.get(ident, set())-seen)
    return documents, [packages[key] for key in sorted(seen) if key in packages], sorted(set(runtime_paths))


def collect(prefix, cache, output):
    source_manifest = json.loads((output/'sources.json').read_text())
    version = source_manifest['qt_version']
    documents, packages, runtime_paths = sdk_inventory(prefix, version)
    # Refuse a different platform instead of presenting the macOS inventory as universal.
    if not any('plugins/platforms/libqcocoa' in name for name in runtime_paths):
        raise ValueError('This baseline collector expects the macOS SDK. Review platform roots for another SDK.')
    needed = set()
    bundled = {'BundledPcre2':'pcre2', 'BundledFreetype':'freetype',
               'BundledHarfbuzz':'harfbuzz-ng', 'BundledLibjpeg':'libjpeg', 'BundledLibpng':'libpng'}
    for package in packages:
        name = package['name']
        if '_Attribution_' in name:
            needed.add(normalize(name.split('_Attribution_', 1)[1]))
        elif name in bundled:
            needed.add(normalize(bundled[name]))
    original_files, records, verifications = {}, [], {}
    for source in source_manifest['sources']:
        module = source['module']
        archive = cache/Path(source['url']).name
        legal, attrs, verification = read_archive(archive, source['sha256'],
                source_checks(prefix/'sbom'/f'{module}-{version}.source.spdx'))
        verifications[module] = verification
        def preserve(path):
            if path not in legal:
                raise ValueError(f'Missing original license file: {module}/{path}')
            original_files[module+'/'+path] = legal[path]
            return 'original/'+module+'/'+path
        for name in ['LGPL-3.0-only', 'GPL-3.0-only']:
            preserve('LICENSES/'+name+'.txt')
        if module == 'qtbase':
            output.mkdir(parents=True, exist_ok=True)
            (output/'LGPL-3.0.txt').write_bytes(legal['LICENSES/LGPL-3.0-only.txt'])
            (output/'GPL-3.0.txt').write_bytes(legal['LICENSES/GPL-3.0-only.txt'])
        for attr_path, item in attrs:
            if normalize(item.get('Id', '')) not in needed:
                continue
            paths = []
            filenames = item.get('LicenseFiles', [])
            if item.get('LicenseFile'):
                filenames += item['LicenseFile'] if isinstance(item['LicenseFile'], list) else [item['LicenseFile']]
            if item.get('CopyrightFile'):
                filenames += [item['CopyrightFile']]
            for filename in filenames:
                paths.append(preserve(posixpath.normpath(str(PurePosixPath(attr_path).parent/filename))))
            # Keep full standard texts as well as component-specific copyright files.
            for token in re.findall(r'[A-Za-z0-9][A-Za-z0-9.+-]*', item.get('LicenseId', '')):
                name = 'LICENSES/'+token+'.txt'
                if name in legal:
                    paths.append(preserve(name))
            for extra in ['NOTICE', 'NOTICE.txt', 'PATENTS', 'AUTHORS']:
                name = str(PurePosixPath(attr_path).parent/extra)
                if name in legal:
                    paths.append(preserve(name))
            if not paths:
                raise ValueError('No license text for '+item['Id'])
            copyright_text = item.get('Copyright', '')
            if isinstance(copyright_text, list):
                copyright_text = '\n'.join(copyright_text)
            records.append({'module':module, 'id':item['Id'], 'name':item['Name'],
                'version':item.get('Version','unspecified'), 'license':item['LicenseId'],
                'copyright':copyright_text, 'usage':item.get('QtUsage',''),
                'source':item.get('DownloadLocation',item.get('Homepage','')),
                'notice_files':sorted(set(paths))})
    found = {normalize(r['id']) for r in records}
    if needed-found:
        raise ValueError('Missing attributions: '+repr(sorted(needed-found)))
    notice = [f'Qt {version} — third-party notices',
        'Copyright (C) The Qt Company Ltd. and other contributors.',
        'Qt modules are used under LGPL-3.0-only. Third-party parts retain their own licenses.',
        'This inventory follows the SDK metadata for the runtime libraries and macOS deployment plugins.',
        'Some metadata entries describe platform-dependent or optional paths; listing them does not claim they are all active.',
        'QtTest, Linguist executables, QML and other SDK development modules are not bundled runtime dependencies.', '']
    for record in records:
        notice += ['='*72,record['name']+' ('+record['id']+')',
            'Version: '+record['version'], 'License: '+record['license'],
            record['copyright'], 'Usage: '+record['usage'], 'Source: '+record['source'], '']
        for file in record['notice_files']:
            notice += ['--- '+file+' ---', original_files[file.removeprefix('original/')].decode('utf-8',errors='strict'), '']
    (output/'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(notice),encoding='utf-8')
    for name, data in original_files.items():
        path=output/'original'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    inventory = {'qt_version':version,'platform':'macos','qt_license':'LGPL-3.0-only',
        'qt_copyright':'Copyright (C) The Qt Company Ltd. and other contributors.',
        'runtime_files':runtime_paths,'source_verification':verifications,
        'sdk_source_revisions':{name:doc['packages'][0]['downloadLocation'] for name,doc in documents.items()},
        'components':[{key:p.get(key,'NOASSERTION') for key in ['name','versionInfo','licenseConcluded','copyrightText']} for p in packages],
        'attributions':records}
    (output/'components.json').write_text(json.dumps(inventory,ensure_ascii=False,indent=2)+'\n')
    config='Qt official SDK build configuration (verbatim upstream SBOM metadata)\n\n'
    for module,document in documents.items():
        config+=module+'\n'+document['packages'][0].get('comment','')+'\n'
    (output/'QT-BUILD-CONFIG.txt').write_text(config)
    print(f'Collected {len(records)} component notices and {len(original_files)} original license files; Qt {version}.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-prefix',type=Path,required=True)
    parser.add_argument('--source-cache',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'third_party/qt')
    args=parser.parse_args()
    collect(args.qt_prefix,args.source_cache,args.output)
