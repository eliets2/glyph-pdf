# GlyphPDF Translations

## Status

| Language | Code | File | Strings | Status |
|---|---|---|---|---|
| English | `en` | (source — no .ts) | — | ✅ Source language |
| Arabic | `ar` | `glyphpdf_ar.ts` | 1394 | ⏳ Awaiting translator |
| French | `fr` | `glyphpdf_fr.ts` | 1394 | ⏳ Awaiting translator |
| German | `de` | `glyphpdf_de.ts` | 1394 | ⏳ Awaiting translator |

## Engineering status (2026-05-30)

- `.ts` files populated via `lupdate` (1394 source strings extracted per language).
- `.qm` binary files compiled at build time via `qt_add_translations` in `CMakeLists.txt`.
- RTL layout (Arabic): Qt auto-mirrors most layouts; per-widget audit confirmed no explicit
  `setLayoutDirection` overrides are required for the current widget set.
- Language selector in Preferences → General is functional; switching language restarts
  the locale and reloads `.qm` files on next launch.

## For translators

To contribute a translation:

1. Open the relevant `.ts` file in **Qt Linguist** (`C:\msys64\ucrt64\bin\linguist.exe`
   or any build of Qt Linguist).
2. Translate each `<source>` string; mark as "Done" (green checkmark) when complete.
3. Save the `.ts` file and submit a pull request.
4. The `.qm` binary will be regenerated automatically at the next build.

Alternatively, send the `.ts` file to a translation vendor and commit the returned file.

## Commissioning tracker

| Language | Vendor / contact | Date sent | Date received | Notes |
|---|---|---|---|---|
| Arabic (ar) | — | — | — | Pending vendor selection |
| French (fr) | — | — | — | Pending vendor selection |
| German (de) | — | — | — | Pending vendor selection |

Update this table when translations are commissioned or delivered.

## Technical notes

- **lupdate** command to refresh after adding new `tr()` strings in source:
  ```
  lupdate src/ -ts translations/glyphpdf_ar.ts translations/glyphpdf_fr.ts translations/glyphpdf_de.ts
  ```
- **lrelease** is run automatically at build time (`qt_add_translations` in CMakeLists.txt).
  The generated `.qm` files land in the build directory and are loaded at runtime.
- **RTL (Arabic)**: The main window uses `QApplication::setLayoutDirection(Qt::RightToLeft)`
  when `ui/language = ar`. Qt 6 auto-mirrors most standard widgets.
- Translation strings that contain `%1`, `%2` etc. are Qt argument placeholders — keep them
  in the translated text in the correct grammatical position for the target language.
