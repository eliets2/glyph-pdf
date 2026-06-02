# Audit Test Environment Setup

> **Deliverable:** M7-PROMPT-1 / D4
> **Date:** 2026-06-02
> **Audience:** Third-party security auditor
> **Purpose:** Reproducible Windows VM for running GlyphPDF security tests,
> fuzzing harnesses, and memory-safety checks.

---

## 1. VM baseline

| Item | Requirement |
| :-- | :-- |
| OS | Windows 10 22H2 (build 19045) or Windows 11 23H2 — 64-bit |
| RAM | 16 GB minimum (32 GB recommended for ASAN + fuzzing corpora) |
| Disk | 100 GB free (build tree + MSYS2 packages + fuzzing corpora) |
| CPU | 4 cores minimum (8+ recommended for parallel build + fuzzing) |
| Network | Internet access for MSYS2 pacman, OCSP/TSA probing (scoped to audit) |
| Snapshots | Take a clean snapshot after OS install, before GlyphPDF install. Revert between test scenarios. |

**Isolation recommendation:** Run the VM with a separate non-routable network
segment for active exploit testing; only enable external network when
specifically testing OCSP/TSA/AI-provider calls.

---

## 2. Install GlyphPDF from MSI

1. Obtain `GlyphPDF-1.0.0-x64.msi` from the Client (see onboarding checklist
   Step 2). Verify SHA-256 before running.
2. Run the installer as a standard (non-admin) user:
   ```bat
   msiexec /i GlyphPDF-1.0.0-x64.msi
   ```
3. Accept defaults. The installer registers `.pdf` file associations via
   `OpenWithProgids` (does not hijack the default handler).
4. Launch GlyphPDF. Smoke-test the following to confirm all features are
   accessible:
   - Open a PDF (drag-and-drop or File > Open).
   - Apply a text redaction (draw a box over text, apply).
   - Encrypt with a password (Security ribbon > Encrypt).
   - Sign with the test certificate from `tests/fixtures/` (Security ribbon >
     Sign; use the `.p12` from `tests/fixtures/`).
   - Run Sanitize (Security ribbon > Sanitize).
   - Run OCR on a scanned PDF (OCR ribbon > Run OCR).

---

## 3. Build from source (required for fuzzing harnesses)

### 3.1 Install MSYS2

Download from https://www.msys2.org/ and install to `C:\msys64\`. Open the
**MSYS2 UCRT64** shell.

### 3.2 Install build dependencies

```bash
pacman -Syu --noconfirm
pacman -S --noconfirm --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-tools \
    mingw-w64-ucrt-x86_64-qt6-svg \
    mingw-w64-ucrt-x86_64-qt6-translations \
    mingw-w64-ucrt-x86_64-qt6-imageformats \
    mingw-w64-ucrt-x86_64-qt6-pdf \
    mingw-w64-ucrt-x86_64-podofo \
    mingw-w64-ucrt-x86_64-qpdf \
    mingw-w64-ucrt-x86_64-openssl \
    mingw-w64-ucrt-x86_64-tesseract-ocr \
    mingw-w64-ucrt-x86_64-tesseract-data-eng \
    mingw-w64-ucrt-x86_64-leptonica \
    mingw-w64-ucrt-x86_64-libxml2 \
    mingw-w64-ucrt-x86_64-freetype \
    mingw-w64-ucrt-x86_64-zlib \
    mingw-w64-ucrt-x86_64-curl \
    mingw-w64-ucrt-x86_64-libpng \
    mingw-w64-ucrt-x86_64-libjpeg-turbo \
    mingw-w64-ucrt-x86_64-libtiff
```

### 3.3 Build (release)

```bash
cd /c/Users/User/Projects/pdf   # or wherever source is extracted
mkdir -p build && cd build
cmake .. -G Ninja
cmake --build . --parallel 8
```

### 3.4 Build with AddressSanitizer + UBSan (recommended for all security testing)

```bash
mkdir -p build-asan && cd build-asan
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build . --parallel 8
```

Run any PDF through the ASAN build and observe stderr for heap/stack errors.
ASAN exit codes and stack traces are the primary signal for memory-safety
findings.

### 3.5 Build the test suite

```bat
set QT_QPA_PLATFORM=offscreen
cd build
ctest --output-on-failure
```

All 14 test targets should pass on the audited revision before starting the
assessment (a failing baseline test is a pre-existing issue to record, not a
finding).

---

## 4. Network capture — AI provider traffic analysis

The AI provider code (`src/engines/ai/`) makes outbound HTTPS POST requests to:
- `api.anthropic.com`
- `api.openai.com`
- Gemini endpoint (Google)

These providers are currently orphaned in the shipping UI but the network code
is live. To observe any traffic that escapes the gating:

### 4.1 Wireshark

1. Install Wireshark (https://www.wireshark.org/).
2. Capture on the VM's network adapter.
3. Filter: `ssl` or `tls.handshake.extensions_server_name contains "anthropic"`
4. Launch GlyphPDF with a document loaded; observe whether any TLS connections
   are initiated to AI endpoints without user interaction.

### 4.2 mitmproxy (TLS interception)

For decrypted inspection of AI provider calls:
```bash
pip install mitmproxy
mitmproxy --mode transparent --ssl-insecure
```
Set the VM's proxy to `127.0.0.1:8080`. Trust the mitmproxy CA in the Windows
cert store. Then trigger any AI-adjacent UI path and inspect requests.

### 4.3 Credential Manager observation

To confirm which key storage path (`CredentialManager` vs plaintext QSettings)
is triggered, use Process Monitor (Sysinternals):
- Filter: `Process Name = GlyphPDF.exe`, `Path contains Credentials`.
- Observe whether `CertWriteW` / `CredReadW` calls precede or follow any
  QSettings write to `ai/keyAnthropicCached`.

---

## 5. Memory safety checks — Dr. Memory

[Dr. Memory](https://drmemory.org/) is a Valgrind-equivalent for Windows that
catches uninitialized reads, heap overflows, and use-after-free without needing
compiler instrumentation (useful for testing the MSI-installed binary directly).

```bat
drmemory.exe -light -- "C:\Program Files\GlyphPDF\GlyphPDF.exe"
```

Use `-light` mode for faster iteration during fuzzing; drop `-light` for
exhaustive checks on targeted PoC PDFs.

**Recommended workflow:**
1. Run Dr. Memory on the MSI-installed binary with a clean PDF.
2. Run Dr. Memory on each adversarial PDF category from `adversarial-test-corpus.md`.
3. For any crash: reproduce on the ASAN build to get a symbolized stack trace.

---

## 6. Fuzzing harness setup

The auditor is expected to build at least two libFuzzer / AFL++ harnesses:

### 6.1 Harness 1 — PDF-open path

Target: `PoDoFoBackend::loadDocument` (or the PoDoFo `PdfMemDocument::LoadFromBuffer`
entry point with GlyphPDF's validation wrapper).

Template (libFuzzer):
```cpp
// fuzz_pdf_open.cpp
#include <stdint.h>
#include <stddef.h>
// Include PoDoFo and PoDoFoBackend headers
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Feed data as a PDF byte buffer to GlyphPDF's load path.
    // Catch exceptions; report crashes to ASAN.
    return 0;
}
```

Build with:
```bash
clang++ -fsanitize=address,fuzzer fuzz_pdf_open.cpp ... -o fuzz_pdf_open
```

Note: GlyphPDF uses GCC on MSYS2. For libFuzzer on Windows, either use a
Clang cross-compilation or switch to AFL++ with `afl-g++`.

### 6.2 Harness 2 — Redaction content-stream parser

Target: `redactCanvasRecursively` in `src/engines/podofo/PoDoFoBackend.cpp`.
Feed synthesized content streams (not full PDFs) to exercise the
text-operator parser, form-XObject recursion, and the image-position check
that corresponds to the T-RED-1 limitation.

### 6.3 Seed corpora

See `docs/security/audit-environment/adversarial-test-corpus.md` for corpus
sources. Minimum starting corpus for the PDF-open harness:
- 50–100 real PDF files of varying complexity.
- 10–20 hand-crafted adversarial PDFs from the corpus list.
- Filtered from Corkami and the PDF.js test suite (see corpus doc).

---

## 7. Adversarial PDF corpus sources

See `docs/security/audit-environment/adversarial-test-corpus.md` for the full
category list. Quick-start sources:

| Source | URL | Notes |
| :-- | :-- | :-- |
| Corkami PDF PoCs | https://github.com/corkami/pocs/tree/master/pdf | Malformed, polyglot, edge-case PDFs |
| PDF.js test suite | https://github.com/mozilla/pdf.js/tree/master/test/pdfs | Broad format coverage |
| PDF Corpus (GitHub) | https://github.com/mozilla/pdf.js/wiki/Frequently-Asked-Questions#faq-support | Additional sources |
| SafeDocs corpus | https://www.pdfa.org/safedocs/ (registration required) | Large-scale PDF corpus |
| pdfium test data | https://pdfium.googlesource.com/pdfium/+/refs/heads/main/testing/ | Edge-case PDFs from Chromium |

---

## 8. Recommended tool summary

| Tool | Purpose | Install |
| :-- | :-- | :-- |
| Wireshark | Network capture for AI provider traffic | https://www.wireshark.org |
| mitmproxy | TLS interception / decryption | `pip install mitmproxy` |
| Dr. Memory | Memory safety for MSI-installed binary | https://drmemory.org |
| Process Monitor | Credential Manager / file I/O tracing | Sysinternals Suite |
| AFL++ (MSYS2) | Fuzzing | `pacman -S mingw-w64-ucrt-x86_64-afl++` |
| ASAN build | In-process memory error detection | CMake flags in §3.4 |
| CyberChef | PDF stream decode / manipulation | https://gchq.github.io/CyberChef |
| iText RUPS | PDF structure browser for PoC verification | https://github.com/itext/rups |

---

## 9. Known environment quirks

- **ucrt64 vs mingw64:** GlyphPDF must be built in MSYS2 ucrt64, not mingw64.
  `qt6-pdf` is only packaged for ucrt64. Using the wrong environment produces
  ABI mismatches.
- **Qt offscreen for headless testing:** `set QT_QPA_PLATFORM=offscreen` is
  required to run CTest in a headless VM (no GPU / display adapter).
- **PDF file associations:** The MSI uses `OpenWithProgids` and does not steal
  the `.pdf` handler. Testing "open PDF by double-click" requires setting
  GlyphPDF as the default PDF handler manually in the test VM.
- **veraPDF (optional):** PDF/A validation via the subprocess path requires
  veraPDF installed separately. Without it, `PdfAValidationPanel` shows
  "validator unavailable" — all other features work.
- **LibreOffice (optional):** Office import requires LibreOffice (`soffice.exe`)
  in PATH. Without it, the Office import panel shows "LibreOffice not installed."
  Install for subprocess-invocation testing (T-EOP-1).
