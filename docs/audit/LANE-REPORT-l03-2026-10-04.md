# LANE REPORT — L03: NATIVE-LINUX PDFIUM + ONNXRUNTIME ARTIFACT MANIFEST (wave 2c)

- Date: 2026-10-04
- Branch: `feat/linux-native` — merge-base 94d8b601; starts at e19d840f (the
  2026-10-02 native-Linux lane tip); folds main @ 9f7787ee (verification
  rounds 1-3 + rollback forensics).
- Lane container: `glyphpdf-linux2` (kalilinux/kali-rolling), clone `/work`
  (container-native ext4). All build/test work in-container; serial offscreen
  gate throughout.

## 0. Executive summary

The 2026-10-02 lane closed with the honest floor **155/200, 45 reds — 38 of
them the "pdfium-stub boundary" class (Class A), unfixable without a Linux
pdfium artifact (L03 open)**. This lane closed L03: pinned Linux builds of
PDFium (bblanchon chromium/7834 — the SAME release as the Windows pin) and
ONNX Runtime (microsoft 1.17.3 — the SAME version as the Windows pin) are now
downloaded-once, SHA-256-pinned, staged by the Q02 bootstrap script, and
enforced at configure time. With `HAS_PDFIUM=TRUE, HAS_RAPIDOCR=TRUE` the
offscreen serial gate runs **197/202 — the entire Class A (38) converted to
real passes**, RapidOCR does genuine PP-OCRv5 inference in the container, and
exactly 5 honestly-classified residuals remain (B=1, C=2, D=1, E=1 — qpdf, a
new class split out of the old Class A). Two REAL portability defects that
the stub reds had masked were found and fixed in TestEngineSave. No genuine
Linux pdfium-behavior difference surfaced anywhere.

## 1. Merge (commit 4eb9eaea)

`git merge bundle/main` — conflicts in exactly the two predicted files
(`TestHistoryIntegrity.cpp`, `TestPersistenceOutcomes.cpp`). Both were the
same shape: main's verification rounds had independently absorbed this line's
ARC03/V02 root-environment guards (fa339ae), wrapping the guard and the
`<unistd.h>` include in `#if !defined(Q_OS_WIN)`. Main's side is a strict
superset (identical Linux behavior, Windows-compile-safe); verified by
diffing both parents against the merge base, resolved onto main's side. Both
semantics survive — they are the same semantics, and the surviving form is
the more defensive one.

Merge-effect attribution (honest): no gate ran between merge and artifact
staging, so deltas are attributed by mechanism. The three office tests that
went green (SmokeTest, TestExportPathBadge, TestOfficeExport) are merge
effects — main restructured them (TestOfficeExport was re-ported wholesale in
e6497dbe and no longer touches soffice at all); pdfium/onnx staging cannot
affect soffice-boundary behavior. Only TestOfficeImport still requires
soffice. Registration also changed main-side: 201 → 204 registered
(R14ProbeRedactSpace registered DISABLED-by-design by r3-hygiene f0d8227f;
R14ProbeBatchSkip already disabled).

## 2. Artifact manifest (commit 2fa7f575)

**Pins (all enforced at configure/bootstrap time):**

| Object | SHA-256 | Verification status |
|---|---|---|
| `pdfium-linux-x64.tgz` (chromium/7834) | `e10b18234af3e988b3021547786e574b8905a24511067f14773f29c9cac12365` | **cross-verified against the GitHub release-API asset digest** (upstream-published checksum) |
| `lib/libpdfium.so` | `246872bdd5e05843b70051e6378216cc584535a1f4a7248b9f88059715d70f7c` | recorded; enforced by `cmake/FindPdfium.cmake` (platform-selective pin) |
| `onnxruntime-linux-x64-1.17.3.tgz` | `f2f11f9da1e3e19b22a8b378b9af57a58433f40e3db6a803e75c0ec0eba97a20` | **OBSERVED** — no upstream-published SHA exists (checked release API digests + release notes); enforced from now on |
| `lib/libonnxruntime.so` (SONAME `libonnxruntime.so.1.17.3`) | `8bdcd79ab25e38d1d7646948e07a7d87ed95c2164a04e43dd685147fd5c86b4c` | recorded; enforced by CMakeLists.txt |
| PP-OCRv5 weights ×4 | match the committed `models/ppocrv5/PROVENANCE.md` pins | **verified** (incl. the rec dict extracted from `inference.yml`, trailing-newline form → exact pin `d1979e9f…`) |
| `pp_doclayout_v2.onnx` | `cd540dc296ff3115fe78efa65b68501e6a8dc74b198acc9834a725ffaa095aac` | OBSERVED — this PROVENANCE previously had **no** hash pin (gap closed); honest size discrepancy recorded (~24.9 MB claimed, 213,963,712 bytes served) |

Version discipline held: same bblanchon release tag as the Windows pdfium pin
(VERSION 150.0.7834; `args.gn` committed in PROVENANCE — v8=false, xfa=false,
standalone, target_os=linux), same ORT version as the Windows pin (1.17.3,
GIT_COMMIT_ID 4beca149). The tracked `third_party/pdfium/include/` is
byte-identical to the linux artifact's headers (diffed; the artifact's extra
`include/cpp/` wrappers are unused and were NOT staged).

**Discipline carried from the 7z lane (PROVENANCE + file(SHA256)):**
artifacts NOT committed — fetched by `scripts/bootstrap-vendor-deps.sh` (its
Linux branch now stages both, archive + extracted payload both pinned — the
G18 two-object rule — with a download-once cache under
`GLYPHPDF_ARTIFACT_CACHE`, default `/opt/glyphpdf-artifact-cache`);
`third_party/pdfium/PROVENANCE.md` extended with the linux-x64 section;
`third_party/onnxruntime/PROVENANCE.md` NEW (both platforms); `.gitignore`
gains `third_party/pdfium/lib/*.so*`. `bootstrap-vendor-deps.sh check` now
treats missing Linux artifacts as a bootstrap miss, not an honest disable.

## 3. Configure + build

```
-- onnxruntime 1.17.3 staged tree (linux-x64, SHA-256 pinned) — RapidOCR PP-OCRv5 enabled
--   HAS_PDFIUM    : TRUE      HAS_RAPIDOCR : TRUE
--   HAS_TESSERACT : TRUE      HAS_QPDF     : FALSE   HAS_QUICKJS : FALSE
```

`ninja -C build-linux -j 6` → **BUILD_RC=0** (twice — post-staging and
post-fixes). Every `#ifdef HAS_PDFIUM` test TU (the 5594139 build-gate
relocations) compiled and RAN for the first time on Linux: zero failed
targets, zero assertion changes needed.

## 4. Gate: before/after

| | 2026-10-02 FINAL | L03 FINAL (2026-10-04) |
|---|---|---|
| Tree | e19d840f | 4eb9eaea + 2fa7f575 + 31d70d8b |
| Passed | 155/200 | **197/202** |
| Failed | 45 | **5** |
| Red classes | A=38, B=4, C=2, D=1 | **A=0**, B=1, C=2, D=1, E(qpdf)=1 |

Two gates were run this lane (post-staging, post-fixes); red sets differ only
by the two deliberate TestEngineSave fixes — zero flakes, nothing re-run
into a different outcome.

**Class A (38) → 0, all real passes.** Highlights: RedactionProof + budget
trip (findMatchesBounded live), quad-points/page-space law pins,
TestTextExtractionCoords (pdfium Linux coordinate semantics match the pins),
Sep13 export/proof paths, diff engine, batch/text-detection paths, and every
5594139-relocated second-engine cross-check. **No genuine Linux pdfium
behavior difference surfaced — zero residuals in that class.**

**HAS_RAPIDOCR converted**: TestRapidOcr 6/6 REAL passes (model load +
printed-English + digits recognition — genuine ONNX inference, 749 ms),
TestLayoutEnsemble + TestOcrPipeline green.

## 5. Real defects fixed (31d70d8b — none are test loosening)

1. **TestEngineSave::commitBlockedByOpenHandlePreservesSource** — the
   held-handle refusal pin is a Windows share-mode semantic (no
   FILE_SHARE_DELETE → sharing violation). POSIX has no sharing-violation
   class; the atomic replace legitimately LANDS — the e23abed K1/K2/K4
   finding, new instance, previously masked inside the Class-A attribution.
   Windows pins verbatim under `Q_OS_WIN`; POSIX pins the mechanism-consistent
   triple: commit ok, replaced path extractable through the real pdfium, and
   the READER's open handle still reads the pre-commit bytes (rename(2)
   swapped the entry; the fd keeps the old inode) — the never-truncate
   invariant via the POSIX mechanism.
2. **TestEngineSave::externalChangeWithPreservedMtimeIsStillDetected** —
   test-side write/ordering defect: `setFileTime` was applied to a
   still-buffered QFile and the destructor's close-flush write(2) hit the
   kernel AFTER the restore (strace: exactly one utimensat, then the flush) —
   a deterministic +6 ms red. Fix: `flush()` before `setFileTime`, plus an
   explicit success pin on the restore. Windows behavior unchanged; the
   preserved-mtime product contract is now genuinely exercised on Linux.

One reclassification (honest correction to the 2026-10-02 triage):
**TestEngineSave::repairedLoadMintsNewIdentityAndStaleWorkerRefuses** was
never a pdfium defect — `QpdfBackend::repair` requires qpdf (HAS_QPDF=OFF).
It founds the new **Class E (qpdf boundary)**. Owner lever: provision qpdf
(distro libqpdf-dev or vendored) and the slot becomes exercisable.

## 6. Remaining reds (5, all boundary-classified; NO unclassified red)

| Red | Class | Lever |
|---|---|---|
| TestOfficeImport | B soffice | runtime-located by design; install LibreOffice headless in a test image |
| TestFormKeystroke, TestFormJsAdversarial | C quickjs | quickjs-ng provision (2026-10-02 owner item 3 stands; the qFatal is in the TEST, product discloses honestly) |
| TestSevenZipBundle | D Windows bundle | non-Windows contract is the system-7z PATH fallback |
| TestEngineSave::repairedLoad… | E qpdf | provision qpdf (new owner lever, §5) |

## 7. UNTESTED (unchanged from 2026-10-02 — no new claims)

Desktop X11/XCb/Wayland, printing, live Secret Service keyring, portals,
tray/WM/HiDPI, 7z PATH-fallback end-to-end, Linux packaging/install rules.
Real-pdfs rendering performance is still uncharacterized (only functional
gates ran). This lane also did NOT exercise the V8/XFA-disabled pdfium API
surface beyond what the tests cover.

## 8. Owner items

1. qpdf + quickjs-ng provisioning (converts Class E + C; the ONLY code reds
   left on the lane are boundaries by design).
2. Non-root CI user so ARC03/V02 permission pins execute instead of
   honestly skipping (carried from 2026-10-02).
3. Linux install/packaging rule still absent (carried).
4. `pp_doclayout_v2.onnx`: re-pin from a release/tag URL (moving `main`
   branch served 214 MB vs the documented ~24.9 MB) and reconcile.
5. Consider a CI lint asserting the bootstrap pins match PROVENANCE files
   (today the hashes live in both by hand).
