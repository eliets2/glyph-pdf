# CMYK downsampling: the lcms2 decision, sized (investigation lane, 2026-10-04)

- Lane: decision prep for parity scorecard row 13 (`docs/audit/PARITY-SCORECARD-2026-09-30.md:318` ← §3 row 93, `:242`) and the owner item left open by `docs/audit/LANE-REPORT-cmyk-downsample-2026-10-01.md` ("Owner items" §1: *vendor an ICC engine (lcms2) or take a product decision to accept naive conversion; the three CMYK-adjacent pins are the gate*).
- Worktree: `D:/pdf/pdf-cmyk-plan`, branch `feat/cmyk-plan`, base `2c09393b`. Docs-only — no product code touched.
- Method: every claim cites file:line in this tree, installed-package facts (`pacman -Si`/`-Qi`/`-Ql`, MSYS2 UCRT64), Qt 6.11.0 headers at `C:/msys64/ucrt64/include/qt6/`, Qt 6.11-matching qtbase source (fetched from `raw.githubusercontent.com/qt/qtbase`), or a **runtime probe executed on this machine's installed Qt** (§3.4). Items that could not be verified are marked UNCERTAIN.

## 0. Verdict (one click)

**Recommendation A: Qt 6.11 QColorSpace CMYK decode — ZERO new dependencies. The premise of the lcms2 decision ("no color-managed decode is available in-tree") is no longer true on the Qt version this tree pins.**

The decision as framed ("vendored lcms2 exception vs keep the skip") assumed the only color-managed path was a new linked dependency. That was true when the lane report was written (2026-10-01, and even then Qt 6.8+ already had it — the report's "Qt's JPEG read does a naive CMYK→RGB conversion" statement describes pre-6.8 Qt) and it is demonstrably false on the installed Qt 6.11.0: **Qt 6.8 shipped a color-managed CMYK pipeline** (CMYK ICC profile parsing + `QImage::Format_CMYK8888` + CLUT-based `QColorTransform`), verified here at runtime to match the lcms2 reference output within ≤5/255 per channel (§3.4). Option B (lcms2 via pacman exception) is documented in §2 in full so the owner can still take it, but it buys almost nothing Option A doesn't already deliver, and it is the exact "new dependency exception" class the quickjs-ng decision reserved for cases with no in-tree alternative. There is an in-tree alternative.

| Option | New deps | Owner sign-off needed | Colorimetrically correct | Effort | Verdict |
|---|---|---|---|---|---|
| **A. QColorSpace CMYK decode (Qt 6.11 built-in)** | **0** | **No** (no new dep; behavior change is reviewed via pin flips) | **Yes** — probe-matched lcms2 reference (§3.4) | ~2–3 days | **RECOMMENDED** |
| B. lcms2 via pacman exception (MIT core) | 1 new linked dep (+GPL-3.0 `fast_float` sub-lib trap, §2.2) | **Yes** — new-dep exception per form-JS precedent | Yes (reference implementation) | ~4–6 days | Fallback if A is disqualified by review findings (§4.5) |
| C. Keep the skip | 0 | No (status quo) | n/a (no conversion) | 0 | Safe default if A and B are declined |

Option A still needs an explicit reviewed change (three characterization pins flip, §4.3) and it keeps the guard's core promise: **profile-less CMYK stays skipped** — only images whose source colorspace is genuinely color-managed (embedded ICC) get converted. PDF 2.0 Annex B defines no default CMYK→RGB conversion (§1.3), so "assume a standard CMYK" remains out of scope unless the owner separately authorizes bundling a redistributable CMYK profile (§4.4).

---

## 1. The exact decode gap

### 1.1 Where the guard sits

`PoDoFoBackend::optimizeDocument` Phase 1 (`src/engines/podofo/PoDoFoBackend.cpp`, `optimizeDocument` begins at :6856; downsample block guarded by `options.downsampleImages && options.targetDpi > 0` at :6887). The in-source rationale (:6864-6884) documents the deliberate skip set:

- :6872-6875 — skip set enumerated: "CMYK, /ImageMask, images carrying /SMask or /Mask — rescaling would desync the mask — or a /Decode array the re-encode would invalidate".
- :6878-6881 — the CMYK rationale of record: "CMYK (named or indexed base) stays skipped: lifting that guard needs a color-managed decode, and this tree has no ICC transform engine — a naive CMYK→RGB conversion is exactly the silent color shift the guard was written to prevent."

There are two decode "doors" a CMYK image could enter through, both closed:

1. **DCT door** (:6947-6950): a `/DCTDecode` image re-encodes only if `/ColorSpace` is `/DeviceRGB` or `/DeviceGray` —
   ```cpp
   auto* csObjDct = dict.FindKey("ColorSpace");
   if (!isName(csObjDct, "DeviceRGB") && !isName(csObjDct, "DeviceGray")) {
       qDebug() << "optimizeDocument: DCT image with unsupported colorspace, left untouched";
       continue;
   ```
   with the comment "A re-encoded JPEG must stay /DeviceRGB or /DeviceGray; CMYK JPEGs would silently change colors." (:6946)
2. **Raw/Flate door** (:6963-6968): non-JPEG streams qualify only as `isRgb = isName(csObj, "DeviceRGB")`, `isGray = isName(csObj, "DeviceGray")`, or `/Indexed` 8bpc (:6965-6968) — and the indexed branch re-closes the CMYK base: `/Indexed /DeviceCMYK` fails the base check at :6983-6987 with the comment "A /DeviceCMYK base stays blocked on the CMYK rationale above (no color-managed decode in this tree)" (:6979-6981).

Shared guards that apply to any lifted path and must keep standing (they are independent of colorspace): `ImageMask` skip (:6925), `SMask`/`Mask`/`Decode`-array skip (:6926), dimension cap (:6922), predictor-coded stream skip (:7048-7059), media-filter-chain skip (:7066-7072), per-image `PdfError` containment (:7147-7150). Note :6926 means **CMYK images carrying /SMask or /Mask stay skipped even after any lift** (mask-desync rationale, unchanged).

### 1.2 What a correct CMYK→RGB requires

CMYK image data is device-dependent; converting it to sRGB for re-encode as JPEG is a colorimetric transform from a *source CMYK colorspace* to sRGB. The source colorspace must come from the image itself — for PDFs that means:

- **`/ICCBased` with `/N 4`** — the image dictionary's colorspace is an array whose first element is a stream carrying an ICC profile stream. The profile is the ground truth.
- **CMYK JPEG with an embedded ICC profile** (JPEG APP2 `ICC_PROFILE` marker inside the DCT stream).
- **`/DeviceCMYK` (or profile-less CMYK JPEG)** — PDF 2.0 Annex B defines **no default CMYK→RGB conversion** (DeviceRGB routes through sRGB; DeviceCMYK→RGB is device-dependent with no standard default; UNCERTAIN on exact clause wording — ISO 32000-2 is paywalled; secondary source: Pdftools, "How to process device specific colors in PDF", pdf-tools.com 2023-02-21). Every PDF renderer guesses a CMYK space for this case; the guard's rationale says this tree must not guess. **Under this plan, profile-less CMYK stays skipped.**

The alternative to color management — the naive photographic conversion `RGB = 255·(1−C)(1−K), …` (what pre-6.8 Qt and PIL's plain `.convert("RGB")` do) — was measured in the §3.4 probe against the real FOGRA39 press profile: pure magenta `(0,255,255,0)` decodes to **(255,0,0)** naively vs **(227,6,20)** colorimetrically; pure cyan to **(0,255,255)** vs **(0,159,227)**. That is the "silent color shift" the guard exists to prevent, quantified: up to ~97/255 per channel of uncontrolled recolor.

### 1.3 What the tree's pins demand

Three characterization pins go RED if the skip is lifted without color management (`tests/TestCompressJpegReencode.cpp`):

| Pin | Lines | What it asserts |
|---|---|---|
| `malformedImagesAreSkippedSafely` | :409, :423-424, :466-468 | a `/DeviceCMYK`-labeled JPEG survives untouched (dims intact) |
| `malformedIndexedImagesAreSkippedSafely` (b) | :1049-1050, :1085, :1117-1120 | `/Indexed /DeviceCMYK` base stays byte-identical ("CMYK remains blocked until a color-managed decode exists") |
| `rawCmykImageStaysSkippedUntilColorManagedDecode` | :1123-1179 | raw 4-bpp `/DeviceCMYK` stream stays byte-identical and keeps `/DeviceCMYK`; the comment (:1124-1130) states the lift condition verbatim: "If this pin ever goes RED, someone lifted the CMYK skip without color management." |

The pins demand two things of any lift: (a) the lift is color-managed (the pin comments define the pass condition, not just the fail condition), and (b) the flip itself is a reviewed event — they are written as tripwires, so the implementing PR must consciously rewrite them into positive pins (§4.3).

---

## 2. Option B — lcms2 via the pacman exception (sized, per the quickjs-ng precedent)

### 2.1 Package facts (verified local, UCRT64 repo)

`pacman -Si mingw-w64-ucrt-x86_64-lcms2` (and `-Q`, it is already installed on this dev machine as a build-env side effect):

| Item | Value |
|---|---|
| Version | 2.19.1-1 (repo: ucrt64, build date 2026-05-06, SHA-256 validated) |
| License (pacman metadata) | `spdx:MIT AND GPL-3.0-or-later` — see the trap in §2.2 |
| License file | `/c/msys64/ucrt64/share/licenses/lcms2/LICENSE` = MIT ("Copyright (c) 2023 Marti Maria Saguer") — **core liblcms2 is MIT**, confirmed |
| Download / installed size | 391.33 KiB / 2133.67 KiB (whole package incl. 6 CLI tools) |
| Runtime DLL | `liblcms2-2.dll` = **448,202 B (438 KiB)**; optional `liblcms2_fast_float-2.dll` = 72,404 B |
| Static lib | `liblcms2.a` = 569,212 B (MIT permits static linking) |
| Build integration | pkg-config only: `lib/pkgconfig/lcms2.pc` — **no CMake config shipped** |
| Depends | `libjpeg-turbo`, `libtiff` — **libtiff is already in the app's chain** (`pacman -Qi libtiff` "Required By" includes `leptonica`, `tesseract-ocr`, `openjpeg2` — all existing dependencies; libjpeg-turbo likewise) |

### 2.2 The GPL trap inside the MIT package (decision-grade nuance)

The pacman license string is `MIT AND GPL-3.0-or-later` because the package ships **two** license files: core lcms2 (`LICENSE`, MIT) and the `fast_float` sub-library (`LICENSE-fast_float`, **GPL-3.0**). The `lcms2.pc` `Libs:` line is `-llcms2 -llcms2_fast_float` — so a naive `pkg_check_modules(LCMS2 lcms2)` (the Tesseract pattern, `CMakeLists.txt:241-243`) would import **both** libraries and link GPL-3.0 code into the app. Any lcms2 integration must link **`lcms2` only** (find_library on `lcms2` / `liblcms2.dll.a`, never the fast_float import lib), or record `lcms2_fast_float` in `LICENSE-3RD-PARTY.md` under a conditional row like the DjVuLibre one (`LICENSE-3RD-PARTY.md:22`). This trap is invisible in the package description and is the kind of finding that must be pinned at configure time (a CMake guard rejecting the GPL import lib, mirroring the MuPDF/Poppler contamination guards at `CMakeLists.txt:279-303`).

### 2.3 Shape under the form-JS precedent

`docs/research/form-js-implementation-plan.md` is the governing precedent (`LICENSE-3RD-PARTY.md:17` quickjs-ng row is its record). The discipline it fixes (§2.0: "`LICENSE-3RD-PARTY.md` is the governance ledger … each addition was an accepted decision, not an agent one"; §6: "Silence = no dependency"; §6.1 runtime pin of record; §6.2 no-engine build proof) maps 1:1:

1. **Authorize**: owner explicitly accepts `mingw-w64-ucrt-x86_64-lcms2` as a new linked dependency (the same standing no-new-deps rule that required quickjs-ng sign-off). New row in `LICENSE-3RD-PARTY.md` (MIT, core lib only, §2.2 caveat recorded).
2. **CMake**: `pkg_check_modules(PKG_LCMS2 QUIET lcms2)` + find_library fallback (Tesseract shape, `CMakeLists.txt:241-261`), then **filter to `lcms2` only** and a configure-time assert that no `lcms2_fast_float` target/import lib is linked (§2.2). Optional-dependency discipline: absent lcms2 ⇒ the CMYK skip compiles back in (D03 "every HAS_ guard must compile and link in its absence", `CMakeLists.txt:230-238` precedent) — the current honest skip becomes the documented no-lcms2 configuration, proven by a TU-compile probe like form-JS §6.2.
3. **Pin**: `GLYPHPDF_LCMS2_PIN "2.19"` enforced at configure time from the linked headers (`LCMS_VERSION` macro in `lcms2.h`, the `GLYPHPDF_QUICKJS_PIN` shape at `CMakeLists.txt:726,758-763`).
4. **Decode path**: for DCT CMYK, Qt still produces the pixels (`Format_CMYK8888`, §3.1) — so lcms2 would replace *only the transform*, i.e. `cmsCreateTransform(cmykProfile, TYPE_CMYK_8_REV, sRGBProfile, TYPE_RGB_8, INTENT_PERCEPTUAL, 0)` over the CMYK8888 scanlines. Profile source: `/ICCBased /N 4` stream or the JPEG-embedded ICC (already extracted by Qt — `QImage::colorSpace().iccProfile()` round-trips it, `qcolorspace.h:152`). Profile-less CMYK stays skipped (no profile to transform from — identical behavior to Option A).
5. **Disclosure**: CapabilityRegistry entry (Compress dialog) stating which CMYK classes are downsampled and which are left untouched, mirroring the form-JS `whyNot`/`alternative` idiom.

**Effort: ~4–6 days** (CMake + pin + ledger + transform wiring + no-dep build proof + the §4.3 test plan). The transform work itself is the *smaller* part — the pins, ledger, pin-enforcement, and no-dep proof are most of it, exactly as with quickjs-ng.

---

## 3. Option A — the Qt 6.11 CMYK path (verified; the decision dissolver)

### 3.1 API surface on the installed headers (C:/msys64/ucrt64/include/qt6/, Qt 6.11.0 — `qmake6 -query QT_VERSION` = 6.11.0)

- `QtGui/qcolorspace.h:60-66` — `enum class ColorModel { …, Cmyk = 3 }` (CMYK added in Qt 6.8); `:55-59` `TransformModel::ElementListProcessing` (CMYK spaces are always this — LUT-based).
- `qcolorspace.h:151` — `static QColorSpace fromIccProfile(const QByteArray&)`; `:144` `isValidTarget()`; `:154` `transformationToColorSpace(const QColorSpace&)`.
- `QtGui/qimage.h:78` — `Format_CMYK8888` ("32-bit byte-ordered CMYK format", added 6.8; painting on it unsupported — irrelevant here, we only memcpy and transform).
- `qimage.h:253-258` — `colorTransformed(const QColorTransform&, QImage::Format, Qt::ImageConversionFlags)` and `applyColorTransform(...)` — the 6.8 3-arg overloads that convert color space **and** pixel format in one pass.
- `QtGui/6.11.0/QtGui/private/qcmyk_p.h` — `QCmyk32`: on little-endian the memory layout is **byte0=C, byte1=M, byte2=Y, byte3=K** — identical to a raw PDF CMYK sample stream; a raw `/DeviceCMYK` 8bpc stream can be wrapped row-wise into `Format_CMYK8888` with no unpacking (stride is exactly 4·W, no Qt padding).

### 3.2 Source of truth: Qt's ICC engine handles CMYK (qtbase, matches 6.11)

Fetched `qt/qtbase` `src/gui/painting/qicc.cpp` and `src/gui/image/qimage.cpp` / `src/gui/painting/qcolortransform.cpp` / `src/plugins/imageformats/jpeg/qjpeghandler.cpp`:

- `qicc.cpp:1920-1926` — a CMYK profile is accepted iff it carries `A2B0` ("CMYK, not n-LUT" rejection otherwise; matrix/TRC CMYK profiles do not exist in ICC). `:1976-1977` sets `ColorModel::Cmyk`.
- `qicc.cpp:1235+` (`parseMft`) handles **lut8/lut16 (mft1/mft2)** A2B0 tags with 4-channel input/output support; `qicc.cpp:1384+` (`parseMabData`) handles **mAB/mBA** (ICC v4); both feed the 4-input `QColorCLUT` (`qcolorspace_p.h:97-100` mAB/mBA element lists). I.e. both ICC v2 (Adobe-era) and v4 CMYK profiles parse.
- `qcolortransform.cpp:2025-2044` — explicit template instantiations `apply<QRgb, QCmyk32>`, `apply<quint8, QCmyk32>`, … : CMYK scanline in, RGB out, CLUT-interpolated. `qimage.cpp` `colorTransformed()` has a dedicated CMYK→RGB(32/64/32F) segment (~:5696-5720).
- **JPEG handler** (`qjpeghandler.cpp`): 4-component JPEGs decode to `QImage::Format_CMYK8888` (not naive RGB); the Adobe CMYK inversion (0 = 100 % ink) is undone on read (`invertCMYK`, default true for Automatic subtype); and the APP2 `ICC_PROFILE` marker is applied via `image->setColorSpace(QColorSpace::fromIccProfile(iccProfile))`.
- Qt 6.8 what's-new (doc.qt.io/qt-6/whatsnew68.html), QtGui section: "[QImage::Format_CMYK8888] 32bit CMYK image format has been added."; "[QColorSpace] support for ICC A2B color spaces processing has been added, along with explicit support for grayscale and CMYK color spaces."; "QImage::colorTransformed() and QImage::applyColorTransform() variants with three arguments has been added…".
- **Contradiction noted, resolved toward source**: the `fromIccProfile` qdoc note still says "QColorSpace only supports RGB or Gray ICC profiles" (qtbase `src/gui/painting/qcolorspace.cpp:1199-1200`, dev branch; same text on doc.qt.io). This is stale text that pre-dates 6.8: the parser, the changelog, and (decisively) the §3.4 runtime probe on the *installed* 6.11.0 all demonstrate CMYK profile support. A doc bug in Qt's favor is still worth a risk note (§4.5).

### 3.3 What the in-tree alternatives are NOT

The 2026-10-01 lane report's negative findings still hold and remain the reason neither of these dissolves the decision instead:

- **MRC/OCR pipeline**: `src/engines/mrc/MrcPageProcessor.cpp:66-78` consumes an already-rendered `const QImage&` and immediately converts `Format_RGB32`; `src/engines/ocr/*` likewise never see CMYK pixels. They are render-side consumers, not decoders — no help.
- **The hand-built sRGB blob** (`buildSrgbIec6196621ProfileV2`, `PoDoFoBackend.cpp:3281+`): a static ICC *profile* embedded as PDF/A output intent — a profile, not a transform engine; RGB-only.
- **Pdfium** renders CMYK internally but exposes no public transform API usable in the optimize pass (lane report, verified unchanged: `grep -ri cmyk src/` hits only ContentSpans text-color parsing and the guard comments).

### 3.4 Runtime probe on the installed Qt 6.11.0 (the decisive evidence)

A standalone probe (`build-rel/probe-cmyk.cpp`, compiled with the UCRT64 g++ against `Qt6::Gui`, run against `C:/msys64/ucrt64/bin` Qt 6.11.0; source + outputs in the worktree's gitignored `build-rel/`, method reproducible from this section):

- **P1 — CMYK ICC profile parsing**: `QColorSpace::fromIccProfile(CoatedFOGRA39.icc)` (654,352 B, Windows-installed press standard) → `isValid=1, colorModel=3 (Cmyk), transformModel=1 (ElementListProcessing), desc="Coated FOGRA39 (ISO 12647-2:2004)"`.
- **P2 — CMYK→sRGB transform vs references** (same six CMYK values fed to (a) Qt `colorTransformed` → `Format_RGB888`, (b) **lcms2 itself** via PIL 11.3 `ImageCms.profileToProfile` with the same profile, (c) the naive photographic conversion):

  | CMYK | Qt 6.11 QColorSpace | lcms2 (PIL ImageCms, perceptual) | naive |
  |---|---|---|---|
  | (0,255,255,0) | (227,6,20) | (227,6,20) — **exact match** | (255,0,0) |
  | (255,0,0,0) | (0,159,227) | (0,159,227) — **exact match** | (0,255,255) |
  | (0,0,0,255) | (29,29,27) | (29,29,27) — **exact match** | (0,0,0) |
  | (0,0,0,0) | (255,255,255) | (255,255,255) — exact | (255,255,255) |
  | (180,40,30,10) | (56,163,201) | (51,163,201) — Δ5 R | (72,206,216) |
  | (90,20,120,40) | (161,181,140) | (161,181,140) — **exact match** | (139,198,113) |

  Qt's built-in CLUT engine tracks the lcms2 reference to ≤5/255 per channel and is categorically distinct from the naive conversion (up to ~97/255 off). That is a color-managed decode in the exact sense the guard pins demand.
- **P3 — the DCT door end-to-end**: a PIL-encoded Adobe CMYK JPEG **with embedded FOGRA39 ICC** loaded via `QImage::loadFromData(..., "JPG")` → `format=36 (Format_CMYK8888)`, `colorSpace().isValid()=1, colorModel=3`, description attached; raw bytes after decode are the un-inverted CMYK values (Adobe inversion correctly undone); `colorTransformed(→SRgb, Format_RGB888)` → (227,6,20) for magenta. The full DCT-door lift chain (decode → profile attach → colorimetric downsample input) works today with public API only.
- **P3 control — profile-less CMYK JPEG** (no ICC): decodes to `Format_CMYK8888` with `colorSpace().isValid()=0` → under Option A these **stay skipped** (no source profile ⇒ no colorimetric transform ⇒ guard rationale preserved).
- **Perf**: 1240×1754 `Format_CMYK8888` → `Format_RGB888` transform = **21 ms** (ElementListProcessing is "rather slow" per Qt docs — irrelevant at raster sizes bounded by the 10 000 px cap, `PoDoFoBackend.cpp:6917-6919`).

---

## 4. Recommendation

### 4.1 Decision

**Adopt Option A** — lift the CMYK skip on the Qt 6.11 QColorSpace path, zero new dependencies. Rationale: the owner's framing ("new-dep exception or keep the skip") rested on "this tree has no ICC transform engine" (`PoDoFoBackend.cpp:6879-6880`); the pinned Qt version has one, verified at runtime to be colorimetrically equivalent to lcms2 (§3.4). Taking Option B after this evidence would spend the project's scarce new-dependency-exception currency on a transform engine Qt already links and loads. Option C remains the safe default if the owner prefers zero behavior change; it costs nothing today and nothing later (this plan stays valid).

### 4.2 Implementation sketch for A (~2–3 days)

1. **DCT door** (`:6947-6950`): admit the image when `src.loadFromData(..., "JPG")` yields `Format_CMYK8888` **and** `src.colorSpace()` is a valid CMYK `QColorSpace`; then `src = src.colorTransformed(src.colorSpace().transformationToColorSpace(QColorSpace::SRgb), QImage::Format_RGB888)` and fall through to the existing downsample/re-encode (which already normalizes to `Format_RGB888`, `:7121`). Images decoding to CMYK8888 without a valid colorspace keep the skip.
2. **Raw/Flate door** (`:6963-6983`): admit `/DeviceCMYK` 8bpc and `/Indexed /DeviceCMYK` base **only when** the image carries a resolvable `/ICCBased /N 4` colorspace (or — for `/DeviceCMYK` — nothing; PDF defines no default, so plain `/DeviceCMYK` with no embedded profile **stays skipped**; CMYK JPEGs bring their profile inside the DCT stream). Wrap samples row-wise into `Format_CMYK8888`, `setColorSpace(fromIccProfile(profileStreamBytes))`, transform as above. Indexed: transform the ≤256-entry palette once (CMYK→RGB per entry), then reuse the existing palette-expansion path unchanged.
3. **Keep everything else**: `ImageMask`/`SMask`/`Mask`/`Decode`-array/predictor/media-filter skips and per-image containment apply unchanged; the re-encode contract stays "real `/DCTDecode` JPEG at `/DeviceRGB` (or `/DeviceGray` for gray)", `/BitsPerComponent 8`.
4. **Disclosure**: Compress dialog / CapabilityRegistry line stating CMYK images with embedded profiles are downsampled colorimetrically and CMYK without a profile is left untouched.

### 4.3 Test plan (the pins flip deliberately)

- **Flip the three characterization pins** (`tests/TestCompressJpegReencode.cpp:466-468, :1117-1120, :1173-1179`) into positive pins, and add:
  - `cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically` — fixture JPEG with an embedded profile (probe P3 method); assert re-encode to `/DeviceRGB` JPEG, band colors survive within tolerance vs the §3.4 reference table (not vs the naive values — the naive values are the *failure* signature and must NOT match).
  - `rawCmykIccBasedImageIsDownsampledColorimetrically` — `/ICCBased /N 4` stream fixture, same contract.
  - `indexedCmykBaseWithProfileIsDownsampled` — `/Indexed /DeviceCMYK` + profile → RGB JPEG, palette-transformed.
  - `profileLessCmykStaysSkipped` — `/DeviceCMYK` with no profile and profile-less CMYK JPEG remain byte-identical (this is the surviving half of the old guard; the new tripwire: never naive-convert).
  - Keep/adapt `malformedImagesAreSkippedSafely` (its CMYK fixture has no ICC — it becomes an instance of the profile-less pin).
- **Render-color pins**: a before/after render diff on a fixture CMYK document (existing render-diff discipline used by TestFontSubset per the Phase 3.5 comment, `PoDoFoBackend.cpp:7242`) — pixels must be visually equivalent to pre-change renders *of the same viewer*, plus the JPEG round-trip tolerance.
- **Profile-handling pins**: a corrupt/truncated ICC stream or a profile `fromIccProfile` rejects (e.g. non-n-LUT CMYK, `qicc.cpp:1920-1923`) ⇒ skip byte-identical; `isValidTarget()` contract asserted (CMYK is never a target).
- **Estimate honesty**: `TestOptimizeEstimate` gains the CMYK class only if estimate modeling is extended; otherwise estimates keep not claiming what doesn't run (row-13 precedent).

### 4.4 Sub-decision the owner may take later (independent of A/B)

Bundling a standard CMYK profile would additionally lift profile-less `/DeviceCMYK`. Windows' FOGRA39/RSWOP files are **not redistributable**; Ghostscript's default_cmyk is AGPL (forbidden, `LICENSE-3RD-PARTY.md:27`); ECI-offsets licensing is UNCERTAIN and needs review. Not needed for A.

### 4.5 Risk notes

- Qt's CMYK support is 6.8+; the tree pins 6.11.0 (`CMakeLists.txt:123`) — a future downgrade breaks the feature; a configure-time `QT_VERSION >= 6.8` check on the CMYK path would pin that (cheap).
- Qt's qdoc still claims RGB/Gray-only for `fromIccProfile` (§3.2) — a Qt doc bug, but if a reviewer rejects on doc grounds, Option B is fully specified in §2 as the fallback.
- Rendering intent: Qt applies the transform via its CLUT engine; probe deltas vs lcms2-perceptual reached Δ5/255 on one sample (§3.4). The test tolerance should be set from a measured reference sweep, not from the naive values.
- Adobe-inversion edge cases for non-Adobe CMYK JPEGs (no APP14 marker) are handled by Qt's `invertCMYK` heuristic; rare non-inverted producers could decode inverted (UNCERTAIN — bounded by the colorimetric fixture tests, and the re-encode is disclosed).
- `QImage::Format_CMYK8888` cannot be painted on (Qt docs) — irrelevant to this pass (no QPainter involved), noted for future users of the helper.

## Sources

- Qt 6.8 what's new: https://doc.qt.io/qt-6/whatsnew68.html
- QColorSpace / QImage docs: https://doc.qt.io/qt-6/qcolorspace.html , https://doc.qt.io/qt-6/qimage.html
- qtbase source (qicc.cpp, qcolorspace.cpp, qimage.cpp, qcolortransform.cpp, qjpeghandler.cpp): https://raw.githubusercontent.com/qt/qtbase/dev/src/gui/painting/qicc.cpp (and sibling paths)
- Pdftools, "How to process device specific colors in PDF" (PDF 2.0 Annex B, DeviceCMYK no-default): https://www.pdf-tools.com (2023-02-21)
- In-tree: all file:line citations above; probe sources/outputs in gitignored `build-rel/` (probe-cmyk.cpp, probe-cmyk-perf.cpp, probe_pil_cmyk.jpg).
