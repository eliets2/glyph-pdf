#!/usr/bin/env bash
# fuzz/build_clang/build_redaction_driver.sh
#
# Hand-link the P1/P2 redaction driver against GlyphPDF's ALREADY-BUILT engine
# archives (build/). Uses g++ (ucrt64) -- NO clang/ASan needed for this target.
# This is the recipe that produced fuzz/bin/redaction_driver.exe.
#
#   cd /c/Users/User/Projects/pdf && bash fuzz/build_clang/build_redaction_driver.sh
#
# Runtime: the driver needs build/*.dll on PATH at execution (podofo, pdfium,
# Qt6, leptonica, tesseract, onnxruntime, ...). run_oracles.sh sets this.
set -eu
ROOT="/c/Users/User/Projects/pdf"
cd "$ROOT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
Q=/c/msys64/ucrt64/include/qt6
L=/c/msys64/ucrt64/lib
mkdir -p fuzz/bin fuzz/scratch

echo "[1/2] compile driver object"
g++ -std=c++17 -g -O1 -c fuzz/harnesses/redaction_driver.cpp -o fuzz/scratch/redaction_driver.o \
  -Isrc -Isrc/core -Isrc/core/interfaces \
  -I$Q -I$Q/QtCore -I$Q/QtGui -I$Q/QtWidgets -I$Q/QtNetwork -I$Q/QtConcurrent \
  -DQT_NO_KEYWORDS -fexceptions

echo "[2/2] link against built engine archives"
g++ -std=c++17 -g -O1 -o fuzz/bin/redaction_driver.exe \
  -Wl,--start-group \
    fuzz/scratch/redaction_driver.o \
    build/libpdfws_engines.a build/libpdfws_commands.a \
    build/src/pdfws_djot/libpdfws_djot.a build/src/docmodel/libdocmodel.a \
    build/third_party/libliblua.a build/third_party/jbig2enc/liblibjbig2enc.a \
    third_party/podofo/install/lib/libpodofo.dll.a \
    third_party/pdfium/lib/libpdfium.dll.a \
  -Wl,--end-group \
  -L$L -lQt6Core -lQt6Gui -lQt6Widgets -lQt6Network -lQt6Concurrent \
  -lfreetype -lfontconfig -lxml2 -lz -lssl -lcrypto -lzip -lqpdf \
  -lleptonica -ltesseract -lopenjp2 -lgomp \
  onnxruntime-win-x64-1.17.3/lib/onnxruntime.lib \
  -lws2_32 -lCredui -lAdvapi32 -lCrypt32

echo "OK -> fuzz/bin/redaction_driver.exe"
