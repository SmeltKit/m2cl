# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Check catalog completeness, placeholders and markup without external packages."""
import ast
from collections import Counter
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent.parent
literal = r'"((?:[^"\\]|\\.)*)"'
pattern = re.compile(r'(?:QCoreApplication::translate|QT_TRANSLATE_NOOP)\(\s*'+literal+r'\s*,\s*'+literal)
keys = set()
for path in (root / 'src').rglob('*'):
    if path.suffix not in {'.h', '.cpp', '.mm'}:
        continue
    for context, source in pattern.findall(path.read_text(encoding='utf-8')):
        keys.add((ast.literal_eval('"'+context+'"'), ast.literal_eval('"'+source+'"')))
errors = []
for language in ('zh_CN', 'zh_TW', 'en_US'):
    path = root / 'translations' / f'm2cl_{language}.ts'
    tree = ET.parse(path)
    messages = {}
    for context in tree.findall('context'):
        for message in context.findall('message'):
            key = context.findtext('name'), message.findtext('source')
            if key in messages:
                errors.append(f'{language}: duplicate {key}')
            translation = message.find('translation')
            messages[key] = translation
            forms = translation.findall('numerusform') or [translation]
            if translation.get('type') in {'unfinished', 'vanished', 'obsolete'}:
                errors.append(f'{language}: unfinished {key}')
            placeholders = Counter(re.findall(r'%(?:L?\d+|n)', key[1]))
            for form in forms:
                text = form.text or ''
                if not text.strip() and key[1].strip():
                    errors.append(f'{language}: empty {key}')
                if Counter(re.findall(r'%(?:L?\d+|n)', text)) != placeholders:
                    errors.append(f'{language}: placeholders differ: {key}')
                if Counter(re.findall(r'</?([A-Za-z][A-Za-z0-9]*)\b', text)) != Counter(re.findall(r'</?([A-Za-z][A-Za-z0-9]*)\b', key[1])):
                    errors.append(f'{language}: markup differs: {key}')
                if language == 'en_US' and key[0] == 'M2CL' and re.search('[\u3400-\u9fff]', text):
                    errors.append(f'{language}: Chinese in English translation: {key}')
    for key in sorted(keys - messages.keys()):
        errors.append(f'{language}: missing {key}')
    print(f'{language}: {len(messages)} messages; {len(keys)} source keys checked')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
