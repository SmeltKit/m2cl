# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Offline regression checks for legal/source distribution integrity."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('source_bundles', ROOT/'tools/prepare_source_bundles.py')
bundles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundles)


class LegalTests(unittest.TestCase):
    def test_original_notices_and_full_texts_are_present(self):
        directory = ROOT/'third_party/qt'
        manifest = bundles.check_notices(directory)
        self.assertEqual(manifest['qt_version'], '6.10.3')
        self.assertEqual(hashlib.sha256((directory/'LGPL-3.0.txt').read_bytes()).hexdigest(),
                         hashlib.sha256((directory/'original/qtbase/LICENSES/LGPL-3.0-only.txt').read_bytes()).hexdigest())
        self.assertEqual((directory/'GPL-3.0.txt').read_bytes(),
                         (directory/'original/qtbase/LICENSES/GPL-3.0-only.txt').read_bytes())
        inventory = json.loads((directory/'components.json').read_text())
        self.assertIn('libwebp', {item['id'] for item in inventory['attributions']})
        self.assertTrue(any(item['license'] == 'MPL-2.0' for item in inventory['attributions']))
        for module in manifest['sources']:
            self.assertGreater(inventory['source_verification'][module['module']]['files_compared'], 100)

    def test_missing_component_notice_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)/'qt'
            shutil.copytree(ROOT/'third_party/qt', target)
            inventory = json.loads((target/'components.json').read_text())
            (target/inventory['attributions'][0]['notice_files'][0]).unlink()
            with self.assertRaisesRegex(ValueError, 'Missing original notice'):
                bundles.check_notices(target)

    def make_source_fixture(self, directory):
        root = Path(directory)/'project'
        root.mkdir()
        shutil.copytree(ROOT/'third_party/qt', root/'third_party/qt')
        for name in ['LICENSE', 'ADDITIONAL_TERMS.md', 'NOTICE', '.gitignore']:
            shutil.copyfile(ROOT/name, root/name)
        (root/'CMakeLists.txt').write_text('project(mclauncher VERSION 0.1.0 LANGUAGES CXX)\n')
        (root/'src').mkdir()
        (root/'src/main.cpp').write_text('int main() { return 0; }\n')
        (root/'.env').write_text('TEST_CREDENTIAL=do-not-package\n')
        (root/'docs/internal').mkdir(parents=True)
        (root/'docs/internal/private.md').write_text('Private development note\n')
        subprocess.run(['git', 'init', '-q', str(root)], check=True, capture_output=True)
        # Even accidentally tracked ignored data must stay out of the companion.
        subprocess.run(['git', 'add', '-f', '.env', 'docs/internal/private.md'],
                       cwd=root, check=True, capture_output=True)
        cache = Path(directory)/'cache'
        cache.mkdir()
        source = cache/'qt-test-source.tar.xz'
        source.write_bytes(b'verifiable offline fixture, not a Qt distribution')
        manifest = {'qt_version': '6.10.3', 'sources': [{'module':'qt-test',
            'url':'https://example.invalid/'+source.name, 'size':source.stat().st_size,
            'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}]}
        (root/'third_party/qt/sources.json').write_text(json.dumps(manifest))
        return root, cache, Path(directory)/'output'

    def test_corrupt_source_stops_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root, cache, output = self.make_source_fixture(directory)
            source = next(cache.iterdir())
            data = source.read_bytes()
            source.write_bytes(b'X'+data[1:])  # Same size, different checksum.
            with self.assertRaisesRegex(ValueError, 'verification failed'):
                bundles.prepare(root, cache, output)
            self.assertFalse(output.exists())

    def test_companions_include_notices_and_exclude_private_or_git_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root, cache, output = self.make_source_fixture(directory)
            bundles.prepare(root, cache, output)
            with zipfile.ZipFile(output/'m2cl-0.1.0-source.zip') as archive:
                paths = {name.removeprefix('m2cl-0.1.0-source/') for name in archive.namelist()}
                self.assertIn('src/main.cpp', paths)
                self.assertNotIn('.env', paths)
                self.assertFalse(any(name.startswith(('docs/internal/', '.git/')) for name in paths))
                manifest = json.loads(archive.read('m2cl-0.1.0-source/SOURCE-MANIFEST.json'))
                for name, digest in manifest['files_sha256'].items():
                    self.assertEqual(hashlib.sha256(archive.read('m2cl-0.1.0-source/'+name)).hexdigest(), digest)
            with zipfile.ZipFile(output/'qt-6.10.3-corresponding-source.zip') as archive:
                self.assertIn('qt-6.10.3-corresponding-source/notices/LGPL-3.0.txt', archive.namelist())
            for line in (output/'SHA256SUMS').read_text().splitlines():
                digest, name = line.split('  ')
                self.assertEqual(bundles.sha256(output/name), digest)


if __name__ == '__main__':
    unittest.main()
