# Brand assets

The GlyphPDF logo set (app icon, wordmark, splash, tray icon, installer logo)
is maintained in [`resources/branding/`](../../../resources/branding/) — those
PNGs are compiled into the application via `resources.qrc`, so that directory is
the single source of truth.

For press/media use, copy the files you need from `resources/branding/`:

| Asset | File |
|-------|------|
| App icon | `resources/branding/app_icon.png` |
| Wordmark | `resources/branding/wordmark.png` |
| Splash | `resources/branding/splash.png` |
| Tray icon | `resources/branding/tray_icon.png` |
| Installer logo | `resources/branding/installer_logo.png` |

(They previously lived here too as byte-identical duplicates — removed to avoid
the source tree carrying the same 10 MB twice.)
