# 7-Zip Provenance (vendored, committed)

Vendored for the encrypted-package feature (`HomeController::createEncryptedPackage`,
PARITY-SCORECARD-2026-09-30 §4 row 14): removes the runtime dependency on a
system-installed 7z.exe. Unlike `third_party/pdfium/` (CI-downloaded), these
binaries are COMMITTED to git deliberately — they are small (2.4 MB), they are
the offline-pitch-critical capability, and a configure-time SHA-256 pin
(`CMakeLists.txt`, quickjs-ng pin discipline) fails the build if the committed
bytes are ever swapped without updating this file.

- **Component**: 7-Zip console tool + format engine (`7z.exe`, `7z.dll`)
- **Version**: 26.02 (x64), Copyright (C) 1999-2026 Igor Pavlov, 2026-06-25
- **Source URLs** (both official; downloaded from EACH and cross-verified):
  - https://www.7-zip.org/a/7z2602-x64.exe (canonical site artifact)
  - https://github.com/ip7z/7zip/releases/download/26.02/7z2602-x64.exe
- **Artifact**: `7z2602-x64.exe`, 1,657,896 bytes
- **SHA-256 (installer, identical from both sources)**:
  `6745fa76dc2ea031596d8678f6f6b99c3c1b435b4164a63485adbbc7b8d82ef0`
- **Extraction**: `7z x 7z2602-x64.exe 7z.exe 7z.dll License.txt` (the installer
  is a 7z SFX; member extraction only — the installer was NOT executed)
- **SHA-256 (`bin/7z.exe`)**: `83967f1b02b43c4efeda302795722c809e0e81b8307de73558d10484d5676a7d`
- **SHA-256 (`bin/7z.dll`)**: `69fd4df057985c40e510e2fac182881c7f85e90aa13ec703f763a8fdb2ce61f8`

## Corroboration

The extracted binaries are byte-identical (same two SHA-256 values) to the
locally installed 7-Zip 26.02 x64 (`C:/Program Files/7-Zip/`) — the exact
version the M-1 password-stdin contract was verified against
(`docs/audit/evidence-m1-package-argv/`; the `-p`-prompt-on-stdin behavior is
version-sensitive, which is why 26.02 is vendored rather than the newer 26.03).
Verified 2026-10-01.

## License

`License.txt` (committed beside this file, copied verbatim from the artifact):
7-Zip Copyright (C) 1999-2026 Igor Pavlov.

- `7z.exe` and all other files: **GNU LGPL** (2.1+).
- `7z.dll`: **GNU LGPL** for most code, **LGPL + unRAR license restriction** for
  some code, **BSD 3-clause** and **BSD 2-clause** for some code.

GlyphPDF invokes 7z.exe as a SUBPROCESS only (never linked in-process) — the
same license-safe aggregation as the veraPDF bundling (LGPL §4 aggregation; no
GlyphPDF code is derived from or linked to 7z). The license text travels with
the binary in installs (`packaging/deploy.ps1` stages `third_party/7zip/` incl.
`License.txt`); the repo-side license record also lives in
`LICENSE-3RD-PARTY.md` and `packaging/licenses/LICENSE-7-Zip.txt`.

## Why 26.02 and not 26.03

26.03 (2026-09, fixes CVE-2026-58052: Mark-of-the-Web not preserved when
EXTRACTING a crafted archive) shipped after the M-1 contract verification. The
app's runtime surface is `7z a` (create) + `7z t` (validate) only — it never
extracts untrusted archives — so 26.02 stays the pinned, contract-verified
version. Bumping to 26.03+ is a deliberate owner follow-up: bump the CMake pin
hashes, re-run the M-1 argv probe and the encrypted-package suites, and update
this file.

## Build integration

- `CMakeLists.txt` pins the two SHA-256 values above (configure-time
  `string(SHA256)` check, FATAL_ERROR on mismatch) and stages `bin/7z.exe` +
  `bin/7z.dll` next to the application binary (`stage_runtime_dlls`).
- `packaging/deploy.ps1` stages the same files (plus `License.txt`) into the
  deploy layout for MSI/portable artifacts.
- Runtime resolution order (`HomeController::locateSevenZip`):
  application-owned directory first (the bundled copy), then `PATH`, then the
  conventional `C:/Program Files/7-Zip/` install locations; empty result is
  disclosed honestly by the encrypted-package dialog.
