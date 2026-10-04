# 7-Zip Provenance (vendored, committed)

Vendored for the encrypted-package feature (`HomeController::createEncryptedPackage`,
PARITY-SCORECARD-2026-09-30 §4 row 14): removes the runtime dependency on a
system-installed 7z.exe. Unlike `third_party/pdfium/` (CI-downloaded), these
binaries are COMMITTED to git deliberately — they are small (2.4 MB), they are
the offline-pitch-critical capability, and a configure-time SHA-256 pin
(`CMakeLists.txt`, quickjs-ng pin discipline) fails the build if the committed
bytes are ever swapped without updating this file.

- **Component**: 7-Zip console tool + format engine (`7z.exe`, `7z.dll`)
- **Version**: 26.03 (x64), Copyright (C) 1999-2026 Igor Pavlov, 2026-09-03
- **Source URLs** (both official; downloaded from EACH and cross-verified):
  - https://www.7-zip.org/a/7z2603-x64.exe (canonical site artifact)
  - https://github.com/ip7z/7zip/releases/download/26.03/7z2603-x64.exe
- **Artifact**: `7z2603-x64.exe`, 1,661,239 bytes
- **SHA-256 (installer, identical from both sources)**:
  `0859c524b8a63551848f0c246abddcb1d0b7b656b0fbfe879f8d85e61a9e6edd`
- **Extraction**: `7z x 7z2603-x64.exe 7z.exe 7z.dll License.txt` (the installer
  is a 7z SFX; member extraction only — the installer was NOT executed).
  Performed 2026-10-04 by the then-committed, pin-verified 26.02 binary; both
  downloads extracted separately, members byte-identical
  (`docs/audit/evidence-7z-2603/downloads.md`).
- **SHA-256 (`bin/7z.exe`)**: `6ee3c0ed0b27663c1b948ae85a7c0bb073aed1498983182f3f0df1f6a8c30b2f`
- **SHA-256 (`bin/7z.dll`)**: `65e4c1f855f9ef6e8f0f5df8e3f27d9eb5f07311408639da0a1ca0b8f4871b0d`

## Version history

- **26.02 → 26.03 (2026-10-04, feat/7z-2603).** Version-hygiene bump onto the
  CVE-2026-58052 fix release. The CVE ("7-Zip failed to preserve the
  Mark-of-the-Web when extracting a crafted archive") is EXTRACTION-side —
  a surface GlyphPDF never invokes (the app's runtime surface is `7z a`
  create + `7z t` validate only) — so the exposure was never reachable from
  GlyphPDF; the bump retires the vulnerable bytes on principle. 26.03 release
  notes (7-zip.org history.txt + GitHub release, checked verbatim) contain NO
  CLI/switch changes: the `a`/`t` surface the transaction relies on is
  unchanged, and the M-1 password-stdin contract was re-verified empirically
  against the 26.03 binary (create with bare `-p` + stdin password, read-back
  `t` with NO `-p` + stdin password, wrong password exits 2, bare `-p` on `t`
  still parses as the empty-password trap; `docs/audit/evidence-7z-2603/`).
  License.txt is byte-identical to the 26.02 copy — no license change.
- **26.02 (2026-10-01, initial vendoring).** Version the M-1
  password-stdin contract was first verified against
  (`docs/audit/evidence-m1-package-argv/`; the `-p`-prompt-on-stdin behavior
  is version-sensitive, which is why 26.02 was vendored rather than the then
  unreleased/newer 26.03). Installer `7z2602-x64.exe` (1,657,896 bytes),
  SHA-256 `6745fa76dc2ea031596d8678f6f6b99c3c1b435b4164a63485adbbc7b8d82ef0`
  (identical from www.7-zip.org and GitHub); extracted pair
  `83967f1b02b43c4efeda302795722c809e0e81b8307de73558d10484d5676a7d` (exe) /
  `69fd4df057985c40e510e2fac182881c7f85e90aa13ec703f763a8fdb2ce61f8` (dll),
  byte-identical to the locally installed 7-Zip 26.02 x64.

## License

`License.txt` (committed beside this file, copied verbatim from the artifact):
7-Zip Copyright (C) 1999-2026 Igor Pavlov.

- `7z.exe` and all other files: **GNU LGPL** (2.1+).
- `7z.dll`: **GNU LGPL** for most code, **LGPL + unRAR license restriction** for
  some code, **BSD 3-clause** and **BSD 2-clause** for some code.

Unchanged in 26.03: the extracted `License.txt` differs in no byte from the
26.02 copy (SHA-256 `519ac0a4bded9c18ea02e0afb71f663d8c47373bd9facd3ac96a79f51d77765d`).

GlyphPDF invokes 7z.exe as a SUBPROCESS only (never linked in-process) — the
same license-safe aggregation as the veraPDF bundling (LGPL §4 aggregation; no
GlyphPDF code is derived from or linked to 7z). The license text travels with
the binary in installs (`packaging/deploy.ps1` stages `third_party/7zip/` incl.
`License.txt`); the repo-side license record also lives in
`LICENSE-3RD-PARTY.md` and `packaging/licenses/LICENSE-7-Zip.txt`.

## Build integration

- `CMakeLists.txt` pins the two SHA-256 values above (configure-time
  `file(SHA256)` check, FATAL_ERROR on mismatch) and stages `bin/7z.exe` +
  `bin/7z.dll` next to the application binary (`stage_runtime_dlls`).
- r3-sec (security M, CWE-494): the same pins travel into the binary as
  compile definitions on `pdfws_engines`, so `SafeSave::locateSevenZip`
  RE-VERIFIES the staged pair at resolution (first use per session) — a
  tampered/stale copy is refused with an honest integrity disclosure and no
  fallback leg.
- `packaging/deploy.ps1` stages the same files (plus `License.txt`) into the
  deploy layout for MSI/portable artifacts.
- Runtime resolution order (`SafeSave::locateSevenZip` / `SevenZipLocator`):
  application-owned directory first (the bundled copy), then `PATH`, then the
  conventional `C:/Program Files/7-Zip/` install locations; empty result is
  disclosed honestly by the encrypted-package dialog.
- Tests: `TestSevenZipBundle` (bundle pins + runtime integrity + real M-1
  round trip through the BUNDLED binary), `TestEncryptedPackageSafeWrite`
  (safe-replacement transaction with a real-7z end-to-end leg),
  `TestControllers::testEncryptedPackageArgsCarryNoPassword` (the M-1 argv
  shape + live stdin-password pin).
