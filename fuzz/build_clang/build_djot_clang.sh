#!/usr/bin/env bash
# fuzz/build_clang/build_djot_clang.sh
#
# Build the P3 djot libFuzzer harness (harness_djot.cpp) WITH clang + libFuzzer
# + ASan/UBSan.
#
# INF06 contract (2026-09-07 infrastructure review): this script must FAIL —
# never exit 0 — when the harness cannot be built. A green CI result has to
# mean "built, ran, and survived". A deliberately unavailable clang must be
# represented by SKIPPING the CI job (a job-level `if:` condition), not by a
# zero exit code here.
#
# The repository root is derived from THIS SCRIPT's location, so the script
# works from any checkout/worktree and any working directory (it used to
# hardcode /c/Users/User/Projects/pdf, which silently targeted the wrong tree
# and could not exist on the Ubuntu CI runner at all).
#
# Provision clang on MSYS2 ucrt64:
#   pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-compiler-rt
# (GATED: ~250 MB download + install. Get owner approval first.)
#
# IMPORTANT ABI NOTE: liblua/pdfws_djot/docmodel in build/ were compiled with
# g++. libFuzzer needs clang. Mixing g++ and clang libstdc++ objects can fail to
# link. This script therefore REBUILDS the three needed libs with clang into
# fuzz/build_clang/ so the whole chain is one compiler -- the correct approach
# per the MSan/instrumentation rule (whole-chain consistency).
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# ucrt64 toolchain: prefer the ACTIVE msys2 root (/ucrt64 — under a CI
# `shell: msys2 {0}` job '/' is the setup-msys2 install, not /c/msys64);
# fall back to the historical /c/msys64 for local dev only.
if [ -d /ucrt64/bin ]; then
  export PATH="/ucrt64/bin:$PATH"
elif [ -d /c/msys64/ucrt64/bin ]; then
  export PATH="/c/msys64/ucrt64/bin:$PATH"
fi
CLANGXX="${CLANGXX:-clang++}"
CLANG="${CLANG:-clang}"
OUT="fuzz/build_clang"
mkdir -p "$OUT/obj" fuzz/bin

if ! command -v "$CLANGXX" >/dev/null 2>&1; then
  echo "ERROR: clang++ ('$CLANGXX') not found — refusing to report success (INF06)." >&2
  echo "  Provision with: pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-compiler-rt" >&2
  echo "  (Ubuntu runners: sudo apt-get install clang)" >&2
  echo "  To skip this target deliberately, gate the CI JOB with a condition — do not" >&2
  echo "  turn this script's failure into a green result." >&2
  exit 1
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  echo "ERROR: clang ('$CLANG') not found — refusing to report success (INF06)." >&2
  exit 1
fi

# M6-P4 D3 (save-time dual-write, commit 6cf0c267) gave src/pdfws_djot a
# QtCore dependency: DjotToRichTextXhtml includes <QString>/<QStringList>.
# The clang rebuild therefore needs the Qt6Core include paths and the final
# link needs Qt6Core libraries. Resolve through pkg-config and fail honestly
# (INF06) when the dependency is not provisioned — never silently skip.
if ! command -v pkg-config >/dev/null 2>&1; then
  echo "ERROR: pkg-config not found — the djot chain links QtCore (M6-P4 D3) and its paths cannot be resolved (INF06)." >&2
  exit 1
fi
if ! pkg-config --exists Qt6Core; then
  echo "ERROR: Qt6Core not found via pkg-config — the djot chain needs QtCore since M6-P4 D3 (DjotToRichTextXhtml includes <QString>). Ubuntu: sudo apt-get install -y qt6-base-dev pkg-config; MSYS2: pacman -S mingw-w64-ucrt-x86_64-qt6-base (INF06)." >&2
  exit 1
fi
QT_CFLAGS="$(pkg-config --cflags Qt6Core)"
QT_LIBS="$(pkg-config --libs Qt6Core)"

# PdfStructureMapper (the save-time dual-write chain, M6-P4 D3) includes
# <podofo/podofo.h>. Resolve the SAME pinned vendored prefix the engine and
# the redaction-oracles job use (GLYPHPDF_PODOFO_DIR; default
# <root>/third_party/podofo/install) — never a distro podofo, whose version
# drifts against the pinned 1.1.0 API the rest of the chain is written to.
PODOFO_DIR="${GLYPHPDF_PODOFO_DIR:-$ROOT/third_party/podofo/install}"
if [ ! -f "$PODOFO_DIR/include/podofo/podofo.h" ]; then
  echo "ERROR: podofo headers not found at $PODOFO_DIR/include/podofo/podofo.h — build the vendored podofo 1.1.0 (commit 712fb0e80e0e9404525d8db54fa0baa4ae469963) into that prefix first; CI: the glyphpdf-fuzz.yml djot job provisions it (INF06)." >&2
  exit 1
fi

SAN="-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g -O1"
INC="-Isrc -Isrc/pdfws_djot -Isrc/core/interfaces -Isrc/docmodel -Ithird_party/lua-5.4/src -I$PODOFO_DIR/include"

echo "[1] rebuild liblua (C) with clang"
for f in third_party/lua-5.4/src/*.c; do
  b=$(basename "$f" .c)
  [ "$b" = "lua" ] && continue   # skip standalone interpreter main
  [ "$b" = "luac" ] && continue
  "$CLANG" $SAN -w -c "$f" -o "$OUT/obj/lua_$b.o"
done

echo "[2] rebuild docmodel + pdfws_djot (C++) with clang"
for f in $(find src/docmodel src/pdfws_djot -name '*.cpp'); do
  b=$(echo "$f" | tr '/' '_')
  "$CLANGXX" -std=c++17 $SAN $INC $QT_CFLAGS -c "$f" -o "$OUT/obj/${b%.cpp}.o"
done

echo "[3] build + link the fuzzer"
# PoDoFo 1.x installs TWO static archives: libpodofo.a (public API) and
# libpodofo_private.a (utls::RecursionGuard, LogMessage, PdfFilterFactory,
# GetPdfVersion — run 36647764910's link died on exactly those). Group both
# (plus the freetype/zlib the static font code pulls) so GNU ld resolves the
# cycle; fall back to the single shared archive when no private archive
# exists (shared-build installs).
PODOFO_LIBS="-L$PODOFO_DIR/lib"
if [ -f "$PODOFO_DIR/lib/libpodofo_private.a" ]; then
  PODOFO_LIBS="$PODOFO_LIBS -Wl,--start-group -lpodofo -lpodofo_private -Wl,--end-group"
else
  PODOFO_LIBS="$PODOFO_LIBS -lpodofo"
fi
PODOFO_LIBS="$PODOFO_LIBS $(pkg-config --libs freetype2 2>/dev/null || true)"
# The optional codecs podofo's static configure may have baked in (deterministic
# since the job installs libjpeg-dev/libpng-dev) — each guarded.
for _pc in libjpeg libpng; do
  if pkg-config --exists "$_pc" 2>/dev/null; then
    PODOFO_LIBS="$PODOFO_LIBS $(pkg-config --libs "$_pc")"
  fi
done
# PdfEncrypt uses OpenSSL EVP when the configure saw libssl-dev (preinstalled
# on the runner — run 36648744086's link died on EVP_MD_CTX_new et al.);
# guarded so an openssl-free podofo stays linkable.
if pkg-config --exists openssl 2>/dev/null; then
  PODOFO_LIBS="$PODOFO_LIBS $(pkg-config --libs openssl)"
fi
PODOFO_LIBS="$PODOFO_LIBS -lz"

"$CLANGXX" -std=c++17 $SAN -fsanitize=fuzzer $INC $QT_CFLAGS \
  -DDJOT_LIB_PATH="\"$ROOT/third_party/djot\"" \
  fuzz/harnesses/harness_djot.cpp "$OUT"/obj/*.o $QT_LIBS $PODOFO_LIBS \
  -o fuzz/bin/djot_fuzzer

# INF06: a build that produces no executable must not exit 0.
if [ ! -x fuzz/bin/djot_fuzzer ]; then
  echo "ERROR: fuzz/bin/djot_fuzzer missing after link — build did not produce the harness." >&2
  exit 1
fi

echo "OK -> fuzz/bin/djot_fuzzer"
echo "Run:  ASAN_OPTIONS=abort_on_error=1:allocator_may_return_null=1 \\"
echo "      ./fuzz/bin/djot_fuzzer -dict=fuzz/dict/djot.dict -max_len=65536 fuzz/corpus/djot/"
