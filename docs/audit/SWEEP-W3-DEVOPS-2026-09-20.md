# SWEEP-W3 — DEVOPS ENGINEER: Operational Readiness of the R25 Candidate

**Lane:** W3 devops-engineer · **Date:** 2026-09-20 · **Tip audited:** `feat/parity-glm` @ `b17106a` (branch `feat/sweep-w3-devops`)
**Scope:** packaging integrity, repro-from-clean, install evidence, CI truthfulness, disk policy, runtime hashes — devops lens on the CURRENT candidate.
**Method:** REVIEW + EVIDENCE lane. Production-code changes only where a finding demands a small infra fix; everything else documented. Static analysis only for CI (CI cannot be run from this machine).

---

## 1. Packaging integrity at this tip (build-devops Release + deploy validation)

### 1.1 Release configure — PASSED

Command (MSYS2 UCRT64, this tree):

```
cmake -S . -B build-devops -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DGLYPHPDF_RELEASE_BUILD=ON -DGLYPHPDF_ENABLE_LTO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Exit code 0. Full log: `build-devops-config.log` (untracked, build tree). Key lines verbatim:

```
-- Using vendored podofo 1.1.0 from C:/Users/User/Projects/pdf-r18/third_party/podofo/install
-- ===== GlyphPDF 1.3.2.3 — Feature Summary (AR-11 D5) =====
--   HAS_PDFIUM    : TRUE   (rendering engine — PDF.js-speed rendering)
--   HAS_TESSERACT : TRUE  (primary OCR engine — Tesseract 5)
--   HAS_RAPIDOCR  : TRUE  (secondary OCR — PP-OCRv5 ROVER ensemble)
--   HAS_QPDF      : TRUE   (PDF linearisation)
--   HAS_QUICKJS   : TRUE  (form-JS Calculate/Format execution)
--   LTO           : ON
--   Release build : ON
-- AR-11 D5: Release build — all four shipped features confirmed present.
-- AR-11 D4: strip found at C:/msys64/ucrt64/bin/strip.exe — Release EXE will be stripped
```

The AR-11 D5 ROVER gate (HAS_RAPIDOCR hard-fail) and the vendored-podofo 1.1.0 gate both held on the current tip.

### 1.2 Release build — exe built, stripped, linked; test-exe tail interrupted (disclosed)

`cmake --build build-devops --parallel 2` ran to step **[844/945]**. At [844] the main `PdfWorkstation.exe` had completed its LTO link (76 serial LTRANS jobs — the build's long pole) and the post-link strip; the harness running this audit then killed the build task during the remaining **test-executable** links. Consequences, stated exactly:

- `build-devops/PdfWorkstation.exe` (15,702,031 bytes, PE32+, "symbols stripped / debugging information removed", link time `Sun Sep 20 21:57:03 2026`) is the complete, finished release artifact and is what deploy.ps1 consumed.
- The ~100 unbuilt steps are test executables only — deploy.ps1 does not consume them. The R25 in-suite test evidence (171/171) exists from lane W2 at this same commit (`b17106a`; the commits after the code it tested are docs-only). **UNVERIFIED here:** a fresh full `ctest` run from `build-devops` itself.
- Build-time observation for the runbook: with `-j 2`, the LTO link reports `lto-wrapper: using serial compilation of 76 LTRANS jobs` — the final exe link is single-threaded and takes ~30 min wall; LTRANS parallelism is the obvious knob if release build time ever matters (`-flto-jobs`/`-flto=partition`).

### 1.3 validate-release-build.ps1 (INF02) — PASSED

```
$ powershell -File packaging/validate-release-build.ps1 -BuildDir C:\Users\User\Projects\pdf-r18\build-devops -ProjectRoot C:\Users\User\Projects\pdf-r18
VALID (INF02): 'C:\Users\User\Projects\pdf-r18\build-devops' is a dedicated Release configuration
with GLYPHPDF_RELEASE_BUILD=ON of 'C:\Users\User\Projects\pdf-r18'.
VALIDATOR_EXIT=0
```

(No `-RequireStamp`: that path requires `build-msi.ps1` to have run; the standalone validator enforced the cache-level gates — `CMAKE_BUILD_TYPE=Release`, `GLYPHPDF_RELEASE_BUILD:BOOL=ON`, and cached `CMAKE_HOME_DIRECTORY` = this tree.)

### 1.4 deploy.ps1 — PASSED (all validation gates green)

Full transcript preserved at `deploy-run.log` (untracked scratch; key verbatim below). `DEPLOY_EXIT=0`.

```
[2/8] Running windeployqt6...
      Direct dependencies: Qt6Concurrent Qt6Core Qt6Gui Qt6Network Qt6Pdf Qt6PdfWidgets Qt6PrintSupport Qt6Svg Qt6Widgets
[3/8] Copying engine binaries (PDFium, ONNX Runtime, PoDoFo)...
      Staged vendored libpodofo.dll (1.1.0).
[4/8] Resolving MinGW DLL dependency closure...
      closure complete after 4 round(s).
[5/8] Staging Visual C++ 2022 runtime DLLs (AR-11 D4)...
WARNING: AR-11 D4: VC++ Redist payload not found. Falling back to System32 DLLs.
[6/8] Copying ONNX models...
[7/8] Copying tessdata...
[8/8] veraPDF not present at third_party/verapdf - skipping (PDF/A validation will prompt the user to install it).
      Staged packaging/licenses/ tree into deploy/licenses/
Validating deploy tree...
========================================
 Deploy complete: D:\pdf\pdf-r18\deploy
 Total size: 407.0 MB
========================================
```

- The **ROVER gate held**: the required-models hard-fail list (`models/ppocrv5/*`, `pp_doclayout_v2.onnx`) passed — note this was only possible after the §2.2 model restoration; had the models still been missing, deploy would have failed at [6/8] exactly as predicted in finding D1.
- The **podofo ABI gate held**: "Staged vendored libpodofo.dll (1.1.0)" — and the staged DLL's SHA-256 is byte-identical to `third_party/podofo/install/bin/libpodofo.dll` (§6), proving the MSYS2 ucrt64 podofo 0.10.4 never overwrote it in the closure.
- The **manifest/compliance gates held**: VERAPDF-SOURCE-OFFER.txt, full `licenses/` tree (LICENSE-veraPDF.txt, LICENSE-OpenJDK.txt present), LICENSE-3RD-PARTY.md all staged and gated.
- The final critical-file validation list (exe, Qt6 runtime, engines, models, tessdata, licenses — 24 entries) threw nothing.
- Only warnings: the expected AR-11 D4 System32 fallback (§3.3) and windeployqt's dxcompiler/dxil note (harmless — D3D shader compiler, not used by the widgets app).

## 2. Bootstrap / repro-from-clean (Q02, G18 lineage)

### 2.1 Offline hash gates vs on-disk trees

`scripts/bootstrap-vendor-deps.sh check` (MSYS2 UCRT64):

```
OK: all vendor trees present:
  third_party/podofo/install  third_party/pdfium/bin/pdfium.dll  onnxruntime-win-x64-1.17.3  mingw-w64-ucrt-x86_64-quickjs-ng
EXIT=0
```

Pinned-hash verification (offline, DLL-side of the G18 gates):

| Artifact | G18 pin (source) | Measured SHA-256 | Verdict |
|---|---|---|---|
| `third_party/pdfium/bin/pdfium.dll` | `a487e1d2…e6905` (bootstrap script + ci.yml + release.yml) | `a487e1d2a18f164adc3a17aacee158787fa86049e6d91d3712b0a43f745e6905` | **MATCH** |
| `onnxruntime-win-x64-1.17.3/lib/onnxruntime.dll` | none post-extraction (zip pin only: `356a33d0…`) | `55ea84749510ee412a3e41b5b41eefd4e2bb05d3f14e5cdb865ae8deb9d1d267` | recorded; **no extracted-DLL pin exists** (gap §2.3) |
| `third_party/podofo/install/bin/libpodofo.dll` | none (built from git tag `1.1.0`, shallow clone) | `b25f21f9feecf6130b1f84e8b1b17e7b94bdf90ab08776078c6ae66911aa5087` | recorded; source-tag provenance only |

Archive-side pins (`.tgz`/`.zip` SHA-256s) cannot be re-verified offline — the archives are deleted after extraction by design; the DLL-side pin for pdfium (the one G18 added) is what persists on disk and it matches.

### 2.2 What a fresh clone would MISS, and its bootstrap coverage

`git ls-files` vs `.gitignore` audit of every untracked thing the build/deploy needs:

| Missing-in-clean-clone item | Tracked? | Needed by | Bootstrap coverage |
|---|---|---|---|
| `third_party/podofo/install/` (bin/libpodofo.dll + cmake config; headers ARE tracked, `bin/` is not — `*.dll` ignore) | no | CMake vendored-podofo gate; deploy.ps1 | **YES** — bootstrap [1/3] builds 1.1.0 from tag |
| `third_party/pdfium/bin/pdfium.dll` | no (`*.dll`; only the `.dll.a` import lib is tracked) | rendering; deploy.ps1 hard-fails | **YES** — bootstrap [2/3], archive+DLL hash-pinned |
| `onnxruntime-win-x64-1.17.3/` | no (dir-pattern ignore) | HAS_RAPIDOCR; deploy.ps1 hard-fails | **YES** — bootstrap [3/3], zip hash-pinned |
| quickjs-ng (pacman) | n/a (host package) | HAS_QUICKJS (optional) | **YES** — bootstrap [4/4] pacman |
| `models/ppocrv5/*.onnx` + `ppocrv5_rec_dict.txt` | **no** (`models/` ignore; only PROVENANCE.md/STATUS.md tracked) | deploy.ps1 **hard-fails** ("ROVER ensemble would be dead"); app OCR | **NO — GAP** |
| `models/pp_doclayout/pp_doclayout_v2.onnx` | **no** | deploy.ps1 **hard-fails**; Djot/MRC pipelines | **NO — GAP** |
| `third_party/verapdf/` | no (optional by design) | PDF/A validation — degrades gracefully | not needed (optional, disclosed) |
| `packaging/stage/tessdata/*` | **tracked** | deploy.ps1 hard gate | n/a — ships with clone |
| VC++ runtime DLLs | n/a (host) | onnxruntime.dll loader | deploy.ps1 stages from vc_redist payload / System32 fallback |

**FINDING D1 (top): the ONNX models have no bootstrap coverage.** `scripts/bootstrap-vendor-deps.sh` — the designated fresh-clone path, the exact mirror of what CI does — stops at podofo/pdfium/ort/quickjs. A fresh clone configures and builds green (models are dev-optional: tests QSKIP), but `packaging/deploy.ps1` **hard-fails at step [6/8]** on the missing `models/ppocrv5/PP-OCRv5_mobile_det_infer.onnx` et al. The recovery data exists (`models/*/PROVENANCE.md` carries per-file source URLs, byte sizes and SHA-256 pins), but acquisition is a manual browser-and-verify procedure. This is the single largest repro-from-clean gap between "CI is green" and "a local release MSI can be produced".

**FINDING D2 (operational, discovered mid-audit): the models were in fact MISSING on this machine** in 5 of 6 sweep worktrees (`pdf-r18`, `pdf-clean`, `pdf-keyA`, `pdf-keyC`, `pdf-sec`) — consistent with collateral from the D:-disk-full reclaims — while `D:/pdf/pdf` (main) and `pdf-inst` still hold them. For this audit the 5 files were copied into `pdf-r18/models/` from the main worktree (read-only source) and **all five SHA-256s match their PROVENANCE.md pins exactly**:

```
a431985659dc921974177a95adcfbb90fd9e51989a5e04d70d0b75f597b6e61d  PP-OCRv5_mobile_det_infer.onnx
da72dc72ca4dc220df0dfde68c1dedc31c58d3e76a25871122e5056227d50092  PP-OCRv5_mobile_rec_infer.onnx
38aa97cd4be591e0ad304e659f07ba30d946f27a63315433f6659c69c8778345  PP-LCNet_x1_0_textline_ori_infer.onnx
d1979e9f794c464c0d2e0b70a7fe14dd978e9dc644c0e71f14158cdf8342af1b  ppocrv5_rec_dict.txt
cd540dc296ff3115fe78efa65b68501e6a8dc74b198acc9834a725ffaa095aac  pp_doclayout_v2.onnx
```

Consequence for other lanes: `pdf-clean` (the integration worktree) currently **cannot run deploy.ps1** until it restores models the same way. (Not fixed there — other worktrees are out of bounds per sweep rules.)

### 2.3 Honest bootstrap gaps (documented, no fix demanded)

- **Extracted ORT DLL is unpinned**: G18 pinned pdfium both archive-side and DLL-side, but onnxruntime is only pinned archive-side. A corrupted-but-valid zip could theoretically pass. Low risk (HTTPS + GitHub release), but the pdfium pattern is the model to copy if hardening is wanted (Rung 2: reuse the G18 two-sided pattern).
- **podofo 1.1.0 vendored build has no binary pin**: inherent — it is built from the upstream `1.1.0` tag each time; hash varies with toolchain. Provenance is the tag, which is acceptable; recorded here so the release evidence names it.

## 3. Clean-machine install evidence

### 3.1 The install story (as wired at this tip)

Pipeline: `build-msi.ps1` → dedicated `build-rel` (Release + `GLYPHPDF_RELEASE_BUILD=ON` + LTO, INF02 validator gates, ctest before staging, INF05 define gate, commit stamp) → `deploy.ps1 -BuildDir <abs>` → Authenticode sign EXE → `wix build` (WiX v4/v5 schema) → sign MSI → `build-portable.ps1` ZIP from the same `deploy/` → `gen-update-manifest.ps1` from the MSI `.sha256` sidecar → winget manifests pin the MSI SHA-256 (`packaging/winget/Glyph.GlyphPDF.installer.yaml` @ 1.3.2.3, `InstallerSha256: 2D7648…E0B`).

- **WiX harvest skeleton**: `packaging/GlyphPDF.wxs` uses a single `<Files Include="$(var.DeployDir)\**" />` harvester — the installer payload is exactly what deploy.ps1 validated, so nothing can be in the MSI that deploy.ps1 did not stage and gate. ProductCode pinned per release (history through v1.3.2 in-file), UpgradeCode constant, `MajorUpgrade`, WixUI_InstallDir, shortcuts + non-hijacking `.pdf` OpenWith association. Version flows in as `-d GlyphPDFVersion=$Version` from the single authoritative `project(VERSION)` (INF03).
- **Tool availability on this machine** (release-box readiness): `wix` CLI present (`C:\Users\User\.dotnet\tools\wix.exe`), `windeployqt6` present in `C:\msys64\ucrt64\bin`.

### 3.2 Findings

- **D3 (dead + broken script)**: `packaging/check-deps.bat` instructs "Run deploy-qt.bat first" — `deploy-qt.bat` no longer exists in the tree; nothing references check-deps.bat. **D4 (dead script)**: `packaging/deploy-msys2.bat` is referenced by nothing but itself (superseded by deploy.ps1 per its own header). Both are Rung-1 delete candidates. *Not deleted in this sweep* — multi-lane sweep discipline; document and let a dedicated cleanup pass execute.
- **D5 (stale comment, doc-vs-artifact)**: deploy.ps1 §7 claims "The WiX installer separately includes a MergeModule for the VC++ runtime" — `GlyphPDF.wxs` contains **no MergeModule**. The VC++ runtime ships only as the loose DLLs deploy.ps1 stages. If the deploy-staged DLLs were ever dropped from the critical list on the strength of that comment, onnxruntime would break on clean machines. Fix the comment (or actually add the merge module) in a dedicated pass.
- **D6 (stale doc header)**: `packaging/WINGET-SUBMISSION.md` says "manifests … version 1.2.0" while the manifests themselves are at 1.3.2.3. Cosmetic.

### 3.3 VC runtime requirements (evidence from this machine)

`onnxruntime.dll` is MSVC-built and imports `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, `MSVCP140.dll` (+ `_1/_2` staged defensively). deploy.ps1 searches four VS2022/2019 Redist payload locations; **none exist on this machine** (no VS Redist installed), so the run takes the **AR-11 D4 System32 fallback** (all five DLLs present in System32 → staged, with the script's own warning printed). Consequence, stated honestly: this build host can produce a *dev-grade* deploy tree; a *release-grade* one wants VS2022 (or the standalone VC++ Redistributable) installed so the official payload versions are staged. NOT-VERIFIED: whether the staged System32 DLL set satisfies a clean Windows 10 19041 machine — requires a second machine (see §3.5).

### 3.4 Dependency-closure verdict — PASS (self-contained deploy tree)

Independent objdump walk (not trusting deploy.ps1's own closure):

- `GlyphPDF.exe`: **39 imports → 24 resolve inside deploy/, 15 are Windows system DLLs** (`KERNEL32`, `ADVAPI32`, `CRYPT32`, `WINTRUST`, `WS2_32`, and the `api-ms-win-crt-*` UCRT set). Zero unresolved third-party imports.
- All **90 staged DLLs**, recursively: 150 unique imports; every import not present in deploy/ is a Windows inbox DLL (`GDI32`, `SHELL32`, `d3d11`/`dxgi`, `WINHTTP`, `ntdll`, the `api-ms-win-core-*` / `api-ms-win-crt-*` API sets, …). The VC++ runtime imports of `onnxruntime.dll` (`VCRUNTIME140*`, `MSVCP140*`) **resolve inside deploy/** because deploy.ps1 stages them (System32 fallback on this host).
- **Verdict: the deploy tree is self-contained on Windows 10 19041+** (the winget manifest's `MinimumOSVersion`); the UCRT api-set imports are inbox on Win10+, so no extra redist beyond the staged VCRT DLLs is required.

## 6. Runtime hashes (SBOM-lite) — Release build @ `feat/sweep-w3-devops` (= `feat/parity-glm` @ `b17106a`)

Built 2026-09-20 21:57, LTO ON, stripped. Source-tree commit `b17106a3982fb79c5ddb67000151d4d407a5e9bf` (dirty-tree note: `models/` + vendor binaries are untracked by design; tracked sources identical to `b17106a`).

**Primary artifact**

| File | SHA-256 |
|---|---|
| `deploy/GlyphPDF.exe` (15,702,031 B) | `66d0f790f167cf90cf10f5e07601e82e6548025a88ede55e1ccf1d0c845d5000` |

**Staged engine/vendor DLLs (deploy/ = what the MSI harvests)**

| DLL | SHA-256 | Provenance cross-check |
|---|---|---|
| `pdfium.dll` | `a487e1d2a18f164adc3a17aacee158787fa86049e6d91d3712b0a43f745e6905` | **= G18 pin** (chromium/7834) |
| `libpodofo.dll` | `b25f21f9feecf6130b1f84e8b1b17e7b94bdf90ab08776078c6ae66911aa5087` | **= vendored tree, byte-identical** (1.1.0 built from tag; NOT MSYS2 0.10.4) |
| `onnxruntime.dll` | `55ea84749510ee412a3e41b5b41eefd4e2bb05d3f14e5cdb865ae8deb9d1d267` | ORT 1.17.3 (zip pin held at download) |
| `onnxruntime_providers_shared.dll` | `d8cf89682e9d23578965e596eaa9bf55e226b53ac6ec9b9ef8685490841eaa72` | ORT 1.17.3 |
| `libtesseract-5.5.dll` | `90d16dde366476d9a6c9acb11fcfdb618408f89f0b32cac048de7976ec6716b9` | MSYS2 ucrt64 |
| `libleptonica-6.dll` | `839efebe42a270b15a9a1b64ceb9272389bc3ada7eb84508672e7334dd7f4804` | MSYS2 ucrt64 |
| `libqpdf30.dll` | `533a86edb376ee1dbf82ae6868d70e2de4c315809647b69f0f9f5c12126f4d1c` | MSYS2 ucrt64 |
| `libqjs-0.dll` | `dd3100904db03feaadf63a231c868286ab9c69053820ab4e3037022dd9573a4c` | MSYS2 ucrt64 (quickjs-ng) |
| `libcrypto-3-x64.dll` | `10a3eea333ccf3076a0530d87a29d2bf8a45135bd7fa4f8cb17f3b85e6486a6f` | MSYS2 ucrt64 (OpenSSL 3.x) |
| `libssl-3-x64.dll` | `733a07dc9a6868b408fd2feaaa3e2afa32cdd4805177e2e540eb96edb770de63` | MSYS2 ucrt64 |
| `libzip.dll` | `47d09e61e667cbc8f119af420a7519ad2525a28fde86ec429a96bb5b2f0b9167` | MSYS2 ucrt64 |
| `libopenjp2-7.dll` | `94f9507828d391cf96d700d34918114f1c1e03f91dd1243a19275e0435690925` | MSYS2 ucrt64 |

**Qt 6 runtime (windeployqt6, ucrt64 Qt 6.11.0)**: `Qt6Core.dll` `c3a7d12a…9d1cd` · `Qt6Widgets.dll` `a21a01a1…c9c0` · `Qt6Gui.dll` `de748737…680f` · `Qt6Pdf.dll` `11f78719…fe3a` · `Qt6PdfWidgets.dll` `e33bd8a2…e7cb`

**VC++ runtime (System32 fallback — see §3.3)**: `VCRUNTIME140.dll` `d1f4225d…9ce7` · `VCRUNTIME140_1.dll` `a7146c08…434e` · `MSVCP140.dll` `7c26614e…89ca`

Toolchain of record: GCC 16.1.0, Qt 6.11.0, CMake (MSYS2 ucrt64), Ninja, LTO ON — matches CLAUDE.md's declared build environment. Deploy tree: 105 top-level entries, 407 MB.

### 3.5 NOT-VERIFIED register (needs a second machine)

- Installer end-to-end: `wix build` produces the MSI here, but **install → launch → uninstall on a clean machine was not tested** (single-machine constraint; installing over the dev box proves little).
- VC runtime adequacy of the System32-fallback DLLs on a machine without any VC++ Redistributable.
- veraPDF-bundled variant (`third_party/verapdf` absent here → deploy skips it by design; the bundled path was not exercised).
- First-run behavior with no network (tessdata/models are bundled, so this should hold — untested on a clean box).

## 4. CI truthfulness (static analysis — CI not runnable from this machine)

Four workflows at this tip: `ci.yml`, `release.yml`, `license-guard.yml`, `glyphpdf-fuzz.yml`.

### 4.1 What is truthful and coherent

- **ci.yml** (build-and-test, `windows-2022`, msys2 UCRT64): installs the full pacman set, builds vendored podofo 1.1.0 into the cache, downloads ORT (zip SHA-256 pinned `356a33d0…`) and pdfium (extracted-DLL SHA-256 pinned `a487e1d2…` — same pins as the bootstrap script, single source of truth), asserts `HAS_PDFIUM/HAS_TESSERACT/HAS_RAPIDOCR` from a fresh configure, runs `ctest -j4`. **Models QSKIP is disclosed in the step comment** ("ONNX model files (models/) are gitignored — tests that need them QSKIP gracefully") — honest.
- **release.yml** (tags `v*`): same provisioning, then configure with `-DGLYPHPDF_RELEASE_BUILD=ON` + LTO, the INF05 `check-release-defines.ps1` GLYPH_TESTING gate (parses `compile_commands.json`, requires shipped targets present in evidence — the old always-green CMakeOutput.log grep is gone), then ctest. Header honestly states it does **not** build the MSI (models not in repo; winget pins the MSI hash). Truthful.
- **license-guard.yml**: poppler job installs poppler then asserts configure FAILS (exit-code capture fixed after the `|| true` clobber; `continue-on-error` + evaluator pattern is correct); mupdf job injects a stub config. Aligned with the CMakeLists FATAL_ERROR guards.
- **Branch triggers**: INF04 `feat/**` patterns on ci.yml + license-guard.yml — the active lane branches get CI.
- **Runner pinning**: ci.yml, release.yml, license-guard.yml all pin `windows-2022` with the documented reason (windows-latest → windows-2025-vs2026 beta, MSYS2 compat issues).

### 4.2 FINDING C1 — glyphpdf-fuzz.yml `redaction-oracles` likely cannot pass (static)

Two defects in the same job:

1. **`runs-on: windows-latest`** — the exact runner the other three workflows explicitly pin AWAY from, each with the comment "windows-latest redirects to windows-2025-vs2026 (beta) which has MSYS2 compatibility issues". The fuzz job predates or ignores that decision.
2. **`install:` list omits `cmake` and `ninja`** (has gcc, qt6-base, qpdf, python). The job's first step is `cmake -S . -B build -G Ninja`. Under `msys2/setup-msys2@v2` the install list is what provides the toolchain; every other workflow that invokes cmake installs `mingw-w64-ucrt-x86_64-cmake` + `mingw-w64-ucrt-x86_64-ninja`. As written the job should fail at Configure with cmake-not-found on a fresh runner. (Static finding — not executed; a run log would settle it.)

Also referenced-targets check: `pdfws_engines`, `pdfws_commands`, `pdfws_djot`, `docmodel`, `liblua`, `libjbig2enc` all exist as real targets in this tree; `fuzz/build_clang/build_redaction_driver.sh`, `fuzz/run_oracles.sh`, `fuzz/build_clang/build_djot_clang.sh`, `fuzz/dict/djot.dict`, `fuzz/corpus/djot/` all exist and are tracked. The `djot-libfuzzer` job (ubuntu-latest, clang) is self-consistent and INF06-hardened. **Only the redaction-oracles provisioning is broken.**

### 4.3 FINDING C2 — glyphpdf-fuzz.yml stale activation comment (Rung 1)

Header says: *"To activate, the OWNER copies it to .github/workflows/ (we do NOT write outside fuzz/)."* The file **is already at** `.github/workflows/glyphpdf-fuzz.yml` — the instruction describes a move that already happened. Misleading for the next operator (suggests the workflow is inactive when it is live).

### 4.4 FINDING C3 — license-guard.yml stale veraPDF-stub comment

Poppler job carries: *"veraPDF vendored dir is not in the repo (gitignored) — create a minimal stub so deploy.ps1 doesn't fail"* — but the job neither creates a stub nor invokes deploy.ps1. Vestigial comment from a prior shape of the job; harmless to execution, noise for the reader.

### 4.5 No deleted-script references in CI

All workflow `run:` steps reference scripts that exist at this tip (`packaging/check-release-defines.ps1`, fuzz scripts). No step points at a deleted target. The deleted-script hazard lives in **packaging/** instead (finding D3, §3.2).

## 5. Operational disk policy (runbook)

### 5.1 Physical layout map (what lives where, measured 2026-09-20)

```
C:\Users\User\Projects\pdf-r18   → JUNCTION → D:\pdf\pdf-r18   (this worktree, W3)
C:\Users\User\Projects\pdf-clean → D:\pdf\pdf-clean,  … pdf-keyA / pdf-keyC / pdf-sec /
                                   pdf-inst / pdf-parity / pdf-r15 / pdf-redaction /
                                   pdf-worktrees\{editing,viewing}   (all on D:)
C:\Users\User\Projects\pdf       → (legacy alias paths into D:\pdf)
D:\pdf\pdf                       → MAIN checkout (branch main) — also the only other
                                   worktree still holding models/ + pdf-inst
C:\msys64                        → JUNCTION → D:\pdf\msys64   (the real MSYS2 install)
C:\msys64.busy                   → real dir, 3 MB / 5 files (relocation leftover)
D:\pdf\Testing                   → stray root-level CTest artifacts, 2 files, ~0 MB
D:\pdf\pdf-backup-pre-purge-20260610.bundle (+ 2 cmake.txt logs) → 74 MB bundle
```

Everything lives on **D:** — all nine worktrees, the MSYS2 toolchain, and every build tree. C: only hosts junctions. **A full D: therefore stops every lane at once** — builds, pacman, and git all fail together; that is why the disk policy below is operational, not cosmetic.

### 5.2 Measured consumers (this audit)

| Item | Size | Status |
|---|---|---|
| `D:\pdf\pdf-r18\build-r18-noeng` | **17.0 GB** | W1-era build tree in the W3 worktree — the single largest reclaimable item found. Not deleted (outside my build dir, per sweep rules). **Reclaim candidate #1.** |
| `D:\pdf\pdf-r18\build-fz` | 436 MB | W1 fuzz lane build tree. Reclaim candidate #2. |
| `D:\pdf\pdf-r18\build-devops` | ~0.4 GB at [458/945], grew to ~1–2 GB by link | this audit's build |
| `C:\msys64.busy` | 3 MB, 5 files | relocation leftover; deletable, negligible |
| `D:\pdf\Testing` | 2 files, ~0 MB | stray CTest output at container root; deletable |
| per-worktree vendor trees (podofo install, pdfium.dll, onnxruntime 1.17.3, models) | ~0.3–0.5 GB each × 9 | REQUIRED for builds/deploys; **not** reclaim candidates |

### 5.3 The rules (runbook)

1. **Build-dir budget**: one build tree per lane, named `build-<lane>` inside that lane's worktree. Budget ≈ **2 GB dev / 4 GB Release+LTO** per lane. A Release+LTO build of this tree with tests is ~1–2 GB (this audit: `build-devops`); the 17 GB `build-r18-noeng` is an outlier (likely Debug/no-engine accumulation + test binaries) and must not become the norm. Before configuring, `df -h /d` must show **≥ 15 GB free**; a Release build + deploy + MSI peak needs ~5 GB transient.
2. **The reclaim rule** (what to delete when D: hits 90%): delete **build trees only**, never vendor trees, never models, never another lane's build tree without pinging that lane. Order: (1) any `build-*` older than the lane's last activity, (2) `dist/` + `deploy/` (regenerated by build-msi.ps1), (3) `third_party/podofo_build` leftovers (bootstrap re-creates). Reclaiming a build tree is always safe — `cmake -B <dir>` + rebuild reproduces it; reclaiming vendor trees costs a re-bootstrap (network + podofo compile); reclaiming models costs a manual acquisition (§2.2 — there is no script yet).
3. **Junction hygiene**: `C:\msys64` and every `C:\Users\User\Projects\pdf-*` are junctions into `D:\pdf`. Deleting a "directory" under C:\Users\User\Projects with `rmdir /s` can follow the junction on some tooling — always `git worktree remove` (or `rmdir` the bare junction first, then `rm -rf` the D: path). The `C:\msys64.busy` leftover is the scar tissue of exactly this class of operation (msys2 relocation while the target was busy); it is 3 MB and inert — delete at leisure, keep the junction intact.
4. **Stray-artifact sweep**: `D:\pdf\Testing` (root-level CTest output — some ctest ran with cwd=D:\pdf) and root-level `fz4-*.txt` scratch files accumulate at container level; sweep quarterly. `msys64.busy` disposition: **delete** (3 MB, unreferenced, regeneration is not needed — it is a partial copy of a tree that already moved).
5. **Model files are the least redundant, most expensive asset**: they exist in only 2 of 9 worktrees (D:\pdf\pdf, pdf-inst — this audit restored pdf-r18). They are gitignored by design, not bootstrapped by script (finding D1). Do NOT "reclaim" models/ anywhere until finding D1 is fixed (an acquisition script); after any disk-full incident, check `models/ppocrv5/*.onnx` presence per worktree before running deploy.ps1 — deploy fails loudly but late (step [6/8]).

## 7. Findings ledger + residuals

### 7.1 Findings (severity-ordered; all documented — none required a production-code fix this lane)

| ID | Area | Finding | Disposition |
|---|---|---|---|
| D1 | repro-from-clean | ONNX models (ROVER + layout) have **no bootstrap coverage**: `bootstrap-vendor-deps.sh` covers podofo/pdfium/ort/quickjs but not `models/`; deploy.ps1 hard-fails without them; acquisition is manual (PROVENANCE.md URLs+SHA256s). | Top gap. Fix candidate: extend the bootstrap script with a models step (URLs + pins already exist in PROVENANCE.md). Deliberately NOT coded here — network fetch + new gate logic deserves its own reviewed change. |
| D2 | ops/this machine | models/ were missing from 5 of 9 worktrees (disk-full collateral). Restored in pdf-r18 from main (read-only source), all 5 SHA-256s match PROVENANCE pins. pdf-clean (integration) still lacks them. | Restored here only; other worktrees out of bounds. pdf-inst lane should restore before any MSI run. |
| C1 | CI truth | `glyphpdf-fuzz.yml` redaction-oracles: `windows-latest` (vs the documented windows-2022 pin) **and** no cmake/ninja in the setup-msys2 install list while step 1 runs `cmake -B build -G Ninja` → likely fails at Configure. Static finding. | Fix: pin windows-2022 + add the two packages. Small, safe CI-only change — left to the fuzz lane owner to keep this lane doc-only. |
| C2 | CI hygiene | `glyphpdf-fuzz.yml` header still instructs "copy to .github/workflows" — it already lives there. | Delete stale paragraph. |
| C3 | CI hygiene | `license-guard.yml` poppler job comment claims a veraPDF stub step that does not exist and deploy.ps1 is never invoked there. | Delete stale comment. |
| D3 | packaging | `packaging/check-deps.bat` broken + dead: points at deleted `deploy-qt.bat`; referenced by nothing. | Rung-1 delete candidate (dedicated cleanup pass). |
| D4 | packaging | `packaging/deploy-msys2.bat` unreferenced (superseded by deploy.ps1). | Rung-1 delete candidate. |
| D5 | packaging | deploy.ps1 comment claims WiX carries a VC++ MergeModule — `GlyphPDF.wxs` has none; VCRT ships only as staged loose DLLs. | Fix comment or add merge module; do not act on the comment alone. |
| D6 | packaging | `WINGET-SUBMISSION.md` header says "version 1.2.0"; manifests are 1.3.2.3. | Cosmetic. |
| B1 | build env | This host has no VS2022/2019 VC++ Redist payload → deploy takes the System32 fallback (all 5 DLLs staged, warning printed). Dev-grade, not release-grade, VCRT staging. | Install VC++ 2015-2022 Redistributable (x64) on the release box. |
| — | build perf | LTO exe link runs 76 LTRANS jobs serially (~30 min) with `-j 2`. | Note only; `-flto-jobs` if release time matters. |

### 7.2 What was verified (evidence-before-done)

| Claim | Machine check | Result |
|---|---|---|
| Configure: vendored podofo + release gate | `build-devops-config.log` grep | PASS (exit 0, all features TRUE) |
| Build identity (INF02) | validate-release-build.ps1 | PASS (exit 0) |
| Deploy + all staging/compliance gates | deploy.ps1 run | PASS (exit 0, 407 MB) |
| Bootstrap trees present + pdfium hash | bootstrap `check` + sha256sum | PASS (pdfium = G18 pin) |
| Models restored correctly | sha256sum vs PROVENANCE | PASS 5/5 |
| Dependency closure | objdump walk (exe + 90 DLLs) | PASS (system-only externals) |
| podofo ABI guard | staged libpodofo sha256 = vendored sha256 | PASS (byte-identical) |
| Full fresh `ctest` from build-devops | not run (build tail killed) | **UNVERIFIED** — W2 evidence 171/171 at same code commit |
| Second-machine install/uninstall | impossible here | **NOT-VERIFIED** (§3.5 register) |

### 7.3 Residuals for the sweep coordinator

1. D1 (models bootstrap gap) is the one finding that turns "CI green" into "cannot build the MSI from clean" — recommend scheduling the bootstrap extension early in W4 or the release-hardening pass.
2. `pdf-clean` needs `models/` restored before any integration-lane packaging attempt (copy from `D:/pdf/pdf` + verify the 5 pins; command documented in §2.2).
3. C1's fuzz-job fix is a 3-line CI edit (runner + two packages) — trivial but belongs to whoever owns the fuzz lane; static-only evidence here.
4. Dead packaging scripts (D3/D4) and stale comments (C2/C3/D5/D6) are batched for the cleanup pass — one commit, no behavior change.
5. `deploy-run.log`, `build-devops-config.log`, `build-devops-build.log` are untracked scratch; the deploy transcript is quoted in full in §1.4. The 17 GB `build-r18-noeng` is the standing reclaim candidate (§5.2) — owner sign-off required before deletion.

**Commit SHA of this report:** see `git log --oneline -1` on `feat/sweep-w3-devops` (message: `docs(audit): SWEEP-W3 devops …`).
