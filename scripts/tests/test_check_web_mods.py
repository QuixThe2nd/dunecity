import hashlib
import importlib.util
import json
import re
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'check_web_mods', Path(__file__).resolve().parents[1] / 'check-web-mods.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class WebModPayloadTests(unittest.TestCase):
    def setUp(self):
        self.files = {name: (name + '\n').encode() for name in checker.REQUIRED}
        self.files['/mods/Tornie/checksums.sha256'] = ''.join(
            hashlib.sha256(data).hexdigest() + '  ' + name[len('/mods/Tornie/'):] + '\n'
            for name, data in self.files.items()
            if name.startswith('/mods/Tornie/') and not name.endswith('checksums.sha256')
        ).encode()

    def package(self, minified=False):
        data = b''
        records = []
        for name, content in self.files.items():
            records.append({'filename': name, 'start': len(data), 'end': len(data) + len(content)})
            data += content
        js = 'loadPackage(' + json.dumps({'files': records}) + ');'
        if minified:
            for key in ('files', 'filename', 'start', 'end'):
                js = js.replace('"' + key + '":', key + ':')
        return js, data

    def test_checks_optimized_and_unoptimized_file_tables(self):
        for minified in (False, True):
            self.assertEqual(checker.check_payload(*self.package(minified))['verified_tornie_files'], 6)

    def test_checks_minified_scientific_notation_offsets(self):
        js, data = self.package(True)
        js = re.sub(r'(start|end): (\d+)', lambda m: m[1] + ': ' + m[2] + 'e0', js)
        self.assertEqual(checker.check_payload(js, data)['verified_tornie_files'], 6)

    def test_rejects_missing_dune2r_and_tornie_files(self):
        for name in ('/mods/Dune2R/mod.ini', '/mods/Tornie/manifest.json'):
            data = self.files.pop(name)
            with self.assertRaisesRegex(ValueError, 'Missing bundled mod file'):
                checker.check_payload(*self.package())
            self.files[name] = data

    def test_rejects_corrupt_tornie_content(self):
        self.files['/mods/Tornie/ObjectData.ini'] = b'changed'
        with self.assertRaisesRegex(ValueError, 'corrupt Tornie'):
            checker.check_payload(*self.package())

    def test_rejects_truncated_archive(self):
        js, data = self.package()
        with self.assertRaisesRegex(ValueError, 'Invalid or duplicate preload range'):
            checker.check_payload(js, data[:-1])

    def test_rejects_accidentally_embedded_optional_art(self):
        self.files['/mods/Dune2R/graphics_hd/units/tank/art.png'] = b'art'
        with self.assertRaisesRegex(ValueError, 'Optional Dune2R art'):
            checker.check_payload(*self.package())


if __name__ == '__main__':
    unittest.main()
