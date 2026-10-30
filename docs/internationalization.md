# Translating M²CL

The interface supports Simplified Chinese (`zh_CN`), Traditional Chinese
(`zh_TW`) and English (`en_US`). The default follows the system language;
unsupported system languages fall back to English. Preferences apply after
restarting the launcher and do not change Minecraft's own language.

## Catalogs and tools

Translations use Qt's QTranslator and TS / QM formats. The source language
for project messages is Simplified Chinese. Editable catalogs are in
[`translations/`](../translations); compiled QM files are generated during the
build and embedded as resources, so users do not need to download language packs.

Install the matching Qt LinguistTools package and its runtime dependencies.
Python is optional for ordinary builds, but is used by the catalog checker.

```sh
cmake --build build --target update_translations
python3 tools/check_translations.py
cmake --build build --parallel
ctest --test-dir build -R 'i18n_tests|translation_catalogs' --output-on-failure
```

`update_translations` extracts source messages; it does not translate new text.
Complete all three catalogs before submitting new user-visible messages.
The checker verifies source coverage, completed entries, placeholders and HTML
structure. It also detects accidental Chinese text in English project messages.

## Conventions

- Use complete messages with `QCoreApplication::translate("M2CL", ...)` and
  numbered placeholders; avoid assembling sentences from translated fragments.
- Use Qt numerus forms when singular / plural wording differs. Use QLocale for
  user-facing dates and numbers where appropriate.
- Preserve placeholders, version strings, paths, URLs, Mod IDs, JSON field names
  and other machine-readable identifiers.
- Keep custom instance names, author-written descriptions, game logs, original
  service responses and license texts unchanged. Translate the surrounding
  explanation rather than the original content.
- Keep language names recognizable in their own language. The separate
  `LanguageNames` context deliberately preserves these names.
- Traditional Chinese uses Taiwan interface terminology, such as 介面、記憶體、
  載入器、執行、檔案 and 帳號. Review terminology in context rather than relying
  only on character conversion.

Translations for standard Qt action captions are maintained by this project;
source keys are listed in `src/core/qttranslationkeys.h`. Native OS dialogs and
window controls may follow the operating system's own language.

## Review

Check the affected flows in light and dark modes and at a compact window size.
Long English captions may require layout adjustments. Verify that language
changes do not alter instance settings, dependency choices, account IDs or
file verification. Screenshot fixtures are useful for review but do not replace
native Windows, macOS and Linux checks.

See [Qt's internationalization documentation](https://doc.qt.io/qt-6/internationalization.html)
for the underlying framework.
