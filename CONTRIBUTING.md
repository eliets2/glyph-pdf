# Contributing to GlyphPDF

Thank you for your interest in contributing to GlyphPDF. This document explains how to set
up the build environment, the coding conventions we enforce, and the process for submitting
changes.

---

## Table of Contents

1. [Build environment](#1-build-environment)
2. [Code style](#2-code-style)
3. [Testing requirement](#3-testing-requirement)
4. [Submitting changes — DCO sign-off](#4-submitting-changes--dco-sign-off)
5. [Issue and PR conventions](#5-issue-and-pr-conventions)
6. [Architectural non-negotiables](#6-architectural-non-negotiables)
7. [What we do NOT require](#7-what-we-do-not-require)

---

## 1. Build environment

GlyphPDF builds exclusively with **MSYS2 ucrt64** on Windows 10/11. No other toolchain
is supported for contributions at this time.

### 1.1 Install MSYS2

Download from <https://www.msys2.org/> and install to `C:\msys64\`.

### 1.2 Install dependencies

Open the **MSYS2 UCRT64** shell (`C:\msys64\ucrt64.exe`) and run:

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

> **Why ucrt64?** `qt6-pdf` (required for the PDF viewer) is packaged only for ucrt64.
> UCRT is the modern Universal C Runtime, system-native on all Windows 10+ installs.
> Mixing with the Qt installer, vcpkg, or mingw64 causes ABI mismatches.

### 1.3 Configure and build

```bash
cd /c/path/to/glyphpdf
mkdir -p build && cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
```

### 1.4 Run tests

```bash
export QT_QPA_PLATFORM=offscreen
ctest --output-on-failure -j4
```

All 32 test cases must pass before a PR is mergeable.

---

## 2. Code style

- **Language standard:** C++17 (`-std=c++17`).
- **Formatting:** clang-format is configured at `.clang-format` in the repo root. Run
  `clang-format -i <file>` before committing, or let the CI format check catch it.
- **Naming conventions:**
  - Classes: `PascalCase`
  - Functions / methods: `camelCase`
  - Member variables: `m_camelCase`
  - Constants and enumerators: `UPPER_SNAKE_CASE`
  - Files: `PascalCase.h` / `PascalCase.cpp` matching the class name
- **Qt patterns:** Use Qt signal/slot (`connect`/`emit`), avoid raw function pointers for
  callbacks across module boundaries.
- **No raw `new`/`delete` in new code.** Use `std::unique_ptr`, `std::shared_ptr`, or Qt
  parent-ownership (`new Foo(parent)`).
- **SPDX header:** Every new `.h` / `.cpp` file must begin with:
  ```cpp
  // SPDX-License-Identifier: Apache-2.0
  ```
- **Do not name local `QLayout*` variables `tr`.** `tr` shadows `QObject::tr()` inside
  any `QObject` subclass member function and causes a compile error.

---

## 3. Testing requirement

- Every new feature or bug fix must be accompanied by at least one test that would have
  caught the regression.
- Tests live in `tests/`. Add new test targets to `tests/CMakeLists.txt`.
- Security-relevant changes (redaction, signing, encryption, sanitization, parsing) require
  a dedicated security test in the appropriate `Test*` target.
- Run `ctest --output-on-failure` locally before opening a PR. CI will block the merge if
  any of the 32 test cases regress.

---

## 4. Submitting changes — DCO sign-off

GlyphPDF uses the **Developer Certificate of Origin (DCO)** instead of a Contributor
License Agreement. No CLA is required. Apache-2.0 Section 5 provides the implicit patent
grant for all contributions.

By signing off a commit you certify that:

> I wrote this change, or I have the right to submit it under the Apache-2.0 license, and
> I understand it will be incorporated into the project and redistributed under that license.

**Add a sign-off to every commit:**

```bash
git commit -s -m "fix: correct byte-range calculation for PAdES B-LT signatures"
```

This adds the line:

```
Signed-off-by: Your Name <your@email.com>
```

PRs that are missing DCO sign-offs on any commit will be blocked until the sign-off is
added (use `git commit --amend -s` or `git rebase --signoff`).

---

## 5. Issue and PR conventions

### Issues

- **Bugs:** Use the bug report template. Include the GlyphPDF version, Windows version,
  steps to reproduce, and expected vs. actual behavior.
- **Features:** Use the feature request template. Link to a related issue or discussion
  if one exists.
- **Security vulnerabilities:** Do NOT open a public issue. Follow `SECURITY.md`.

### Pull requests

- Open a draft PR early to get feedback on direction before the implementation is complete.
- The PR description must summarize: what changed, why, and how it was tested.
- Link the related issue: `Closes #NNN`.
- One logical change per PR. If a PR grows beyond a single concern, split it.
- All CI checks must be green before requesting a review.

### Branch naming

- `fix/short-description` for bug fixes
- `feat/short-description` for new features
- `refactor/short-description` for refactoring
- `docs/short-description` for documentation-only changes

---

## 6. Architectural non-negotiables

The following constraints are enforced by CMake guards, runtime checks, and CI:

| Constraint | Reason |
|---|---|
| **MuPDF (AGPL-3.0) must never be linked** | AGPL contamination — CMake FATAL_ERROR guard |
| **Poppler (GPL-2.0+) must never be linked** | GPL contamination — CMake FATAL_ERROR guard |
| **veraPDF must only be invoked as subprocess** | AGPL §13 subprocess exception |
| **DjVu is excluded as an output format** | Legacy format, dead browser support |
| **Redaction must always excise content stream, never overlay** | Visual overlay is recoverable |
| **SHA-256 only for signature hashing** | MD5/SHA-1 collision-prone, deprecated for signing |
| **Incremental save when signatures exist** | Full rewrite invalidates `/ByteRange` |
| **qpdf must never be in the signing path** | qpdf flattens xref, invalidates ByteRange |
| **JBIG2 pattern-matching mode never** | 2013 Xerox digit-substitution incident |
| **No spawn-per-page ONNX process** | ONNX init cost defeats GPU warm-up |

See `docs/planning/AUDIT-v1.0.0.md` for the full anti-pattern catalog.

---

## 7. What we do NOT require

- **No CLA.** Apache-2.0 §5 grants the necessary patent license implicitly.
- **No telemetry opt-in.** GlyphPDF is privacy-first; telemetry is opt-in only and off by
  default.
- **No specific IDE.** Any editor that respects `.clang-format` and `.editorconfig` works.

---

## Questions?

Open a GitHub Discussion or reach out via the issue tracker. For security issues, see
`SECURITY.md`.
