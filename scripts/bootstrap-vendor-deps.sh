#!/usr/bin/env bash
# scripts/bootstrap-vendor-deps.sh
#
# Reproduces the three UNTRACKED vendor binary trees GlyphPDF needs to build
# and run (Q02 known-facts, docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md):
#
#   1. third_party/podofo/install          — PoDoFo 1.1.0 built from source
#                                            (bin/libpodofo.dll + cmake config)
#   2. third_party/pdfium/bin/pdfium.dll   — PDFium runtime (chromium/7834),
#                                            matching the vendored import lib
#   3. onnxruntime-win-x64-1.17.3/         — ONNX Runtime 1.17.3 (HAS_RAPIDOCR)
#
# WHY THIS EXISTS: without tree 1, CMake silently falls back to MSYS2's podofo
# 0.10.4 (API mismatch — obscure build errors) and missing trees 2/3 leave
# tests exiting 0xc0000135. The steps are exactly what .github/workflows/
# ci.yml and release.yml do on a cache miss; this script makes them runnable
# on a fresh worktree/checkout locally.
#
# Usage:
#   scripts/bootstrap-vendor-deps.sh            # install whatever is missing
#   scripts/bootstrap-vendor-deps.sh check      # verify the three trees only
#
# Environment: MSYS2 UCRT64 (git, cmake, ninja, curl, tar, unzip, sha256sum).
# Every download is checksum-pinned; JOBS=2 default (linker OOM at higher -j).
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
JOBS="${JOBS:-2}"

PODOFO_DIR="third_party/podofo/install"
PODOFO_SRC="third_party/podofo_build"
PODOFO_VER="1.1.0"
PDFIUM_DLL="third_party/pdfium/bin/pdfium.dll"
PDFIUM_TGZ_URL="https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F7834/pdfium-win-x64.tgz"
# G18 (QUALITY-GATE-2026-09-09): the ARCHIVE and the EXTRACTED DLL are two
# different objects with two different hashes. The old script pinned only the
# DLL hash and compared it against the downloaded .tgz in fetch(), so the
# fresh-install PDFium path could never validate its own download (sha256sum
# mismatch aborted the bootstrap before extraction). Both are pinned now and
# BOTH are enforced: the archive at download time, the extracted DLL before
# staging (matching what CI validates).
PDFIUM_TGZ_SHA256="0abfacf8aacc919f98eff2c3efa2927c3dc9faf07e31f22558a1f1cf93809612"
PDFIUM_DLL_SHA256="a487e1d2a18f164adc3a17aacee158787fa86049e6d91d3712b0a43f745e6905"
ORT_DIR="onnxruntime-win-x64-1.17.3"
ORT_ZIP_URL="https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-win-x64-1.17.3.zip"
ORT_ZIP_SHA256="356a33d024f2709786bebd5d4ca06cd5392875da95daa0455aae72edc8993256"

podofo_ok()   { [ -f "$PODOFO_DIR/bin/libpodofo.dll" ] && [ -f "$PODOFO_DIR/lib/cmake/podofo/podofo-config.cmake" ]; }
pdfium_ok()   { [ -f "$PDFIUM_DLL" ]; }
onnx_ok()     { [ -f "$ORT_DIR/lib/onnxruntime.dll" ]; }

fetch() { # url sha256 dest
  local url="$1" sha="$2" dest="$3"
  echo "  downloading $url"
  curl -fL --retry 3 -o "$dest" "$url"
  echo "$sha  $dest" | sha256sum -c - >/dev/null
}

install_podofo() {
  echo "== [1/3] Building vendored podofo $PODOFO_VER into $PODOFO_DIR =="
  rm -rf "$PODOFO_SRC"
  git clone --depth 1 --branch "$PODOFO_VER" https://github.com/podofo/podofo.git "$PODOFO_SRC"
  cmake -S "$PODOFO_SRC" -B "$PODOFO_SRC/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPODOFO_BUILD_TOOLS=OFF \
    -DPODOFO_BUILD_TEST=OFF \
    -DPODOFO_BUILD_EXAMPLES=OFF \
    -DCMAKE_INSTALL_PREFIX="$ROOT/$PODOFO_DIR"
  cmake --build "$PODOFO_SRC/build" --parallel "$JOBS"
  cmake --install "$PODOFO_SRC/build"
  podofo_ok || { echo "ERROR: podofo install finished but the tree is incomplete ($PODOFO_DIR)" >&2; return 1; }
  # Remove the build tree to reclaim disk; keep only the installed prefix.
  rm -rf "$PODOFO_SRC"
}

install_pdfium() {
  echo "== [2/3] Staging PDFium runtime DLL (chromium/7834) =="
  local tmp="third_party/pdfium_dl.tmp"
  rm -rf "$tmp"; mkdir -p "$tmp" third_party/pdfium/bin
  # G18: validate the ARCHIVE hash at download time...
  fetch "$PDFIUM_TGZ_URL" "$PDFIUM_TGZ_SHA256" "$tmp/pdfium.tgz"
  tar -xzf "$tmp/pdfium.tgz" -C "$tmp"
  # G18: ...and the EXTRACTED DLL hash before staging — a compromised or
  # corrupted archive that happens to be "some" valid download can never
  # reach third_party/pdfium/bin without its extracted payload matching the
  # DLL the vendored import lib was built against.
  echo "$PDFIUM_DLL_SHA256  $tmp/bin/pdfium.dll" | sha256sum -c - >/dev/null \
    || { echo "ERROR: extracted pdfium.dll hash mismatch (expected $PDFIUM_DLL_SHA256)" >&2; rm -rf "$tmp"; return 1; }
  cp "$tmp/bin/pdfium.dll" "$PDFIUM_DLL"
  rm -rf "$tmp"
  pdfium_ok || { echo "ERROR: pdfium.dll missing after staging" >&2; return 1; }
}

install_onnx() {
  echo "== [3/3] Staging ONNX Runtime 1.17.3 =="
  local tmp="onnxruntime_dl.tmp"
  rm -rf "$tmp"
  fetch "$ORT_ZIP_URL" "$ORT_ZIP_SHA256" "$tmp"
  # The zip's top-level directory IS the expected tree name.
  unzip -q -o "$tmp"
  rm -f "$tmp"
  onnx_ok || { echo "ERROR: $ORT_DIR/lib/onnxruntime.dll missing after extraction" >&2; return 1; }
}

trap_warning() {
  cat >&2 <<'EOF'
!! VENDOR TREES INCOMPLETE - DO NOT CONFIGURE/BUILD YET.
!! Without third_party/podofo/install, CMake silently falls back to MSYS2's
!! podofo 0.10.4 (API mismatch) and without pdfium/onnxruntime the tests exit
!! 0xc0000135 (missing DLLs). Fix the trees above, then reconfigure.
EOF
}

case "${1:-install}" in
  check)
    ok=1
    podofo_ok || { echo "MISSING: $PODOFO_DIR (bin/libpodofo.dll or cmake config)"; ok=0; }
    pdfium_ok || { echo "MISSING: $PDFIUM_DLL"; ok=0; }
    onnx_ok   || { echo "MISSING: $ORT_DIR/lib/onnxruntime.dll"; ok=0; }
    if [ "$ok" = 1 ]; then
      echo "OK: all three vendor trees are present:"
      echo "  $PODOFO_DIR  $PDFIUM_DLL  $ORT_DIR"
      exit 0
    fi
    trap_warning
    exit 1
    ;;
  install)
    podofo_ok || install_podofo
    pdfium_ok || install_pdfium
    onnx_ok   || install_onnx
    ;;
  *)
    echo "usage: $0 [install|check]" >&2
    exit 2
    ;;
esac

echo "bootstrap complete: podofo $PODOFO_VER + pdfium chromium/7834 + onnxruntime 1.17.3 are staged."
echo "Now configure: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug"
