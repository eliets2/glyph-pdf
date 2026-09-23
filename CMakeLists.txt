cmake_minimum_required(VERSION 3.16)
project(PdfWorkstation VERSION 1.4.0 LANGUAGES CXX)

# MSYS2 ucrt64 — single coherent toolchain + Qt6 + all deps via pacman.
# Explicit prefix paths so CMake resolves packages regardless of calling shell.
# L01 (NATIVE-LINUX-READINESS-2026-09-10): this is an MSYS2/Windows layout
# assumption. Injecting C:/msys64 into CMAKE_PREFIX_PATH and overwriting
# PKG_CONFIG_PATH/PKG_CONFIG_LIBDIR on every platform broke ordinary native
# Linux find_package/pkg-config discovery. Windows keeps the explicit prefix;
# Linux uses its normal search paths (no environment clobbering).
if(WIN32)
    if(NOT DEFINED MSYS2_ROOT)
        set(MSYS2_ROOT "C:/msys64/ucrt64")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${MSYS2_ROOT}")
    list(APPEND CMAKE_PREFIX_PATH "${MSYS2_ROOT}/share")
    list(APPEND CMAKE_MODULE_PATH "${MSYS2_ROOT}/lib/cmake")
    # Restrict pkg-config to ucrt64 only so mingw64 headers don't leak in.
    set(ENV{PKG_CONFIG_PATH} "${MSYS2_ROOT}/lib/pkgconfig:${MSYS2_ROOT}/share/pkgconfig")
    set(ENV{PKG_CONFIG_LIBDIR} "${MSYS2_ROOT}/lib/pkgconfig:${MSYS2_ROOT}/share/pkgconfig")
endif()
enable_testing()
# SECFIX-1: GLYPH_TESTING must NOT be defined for production builds. It guards an
# OCSP test-fixture bypass in SignatureManager.cpp. Opt in only for CI test builds.
option(GLYPHPDF_ENABLE_TEST_FIXTURES "Enable OCSP test fixture loading (for CI only)" OFF)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# AR-11 D4: Default to Release when CMAKE_BUILD_TYPE is unset.
# Without this, a bare `cmake -B build` yields an unoptimised assert-enabled build
# that ships DWARF paths (C:\Users\User\Projects\pdf) in every binary.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "AR-11 D4: CMAKE_BUILD_TYPE not set — defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default: Release)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)
if(MSVC)
    add_compile_options(/we4715 /we4834 /we4456) # return-type, nodiscard, shadow equivalents
else()
    add_compile_options(-Werror=return-type -Werror=unused-result -Werror=shadow)
endif()

# AR-11 D4: Security hardening flags (GCC/MinGW, Release builds only).
# -fstack-protector-strong  — stack smashing detection (SSP) on functions with
#                              complex stack frames (GCC 4.9+)
# -D_FORTIFY_SOURCE=2       — buffer overflow detection in glibc wrappers;
#                              requires optimisation (-O1 or higher).
# Linker PE flags (Windows x64):
#   --dynamicbase            → sets IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE (ASLR)
#   --nxcompat               → sets IMAGE_DLLCHARACTERISTICS_NX_COMPAT  (DEP/NX)
#   --high-entropy-va        → 64-bit ASLR (HEASLR; requires --dynamicbase)
# LTO:
#   -flto / -flto=auto       — link-time optimisation; reduces binary size, speeds
#                              up release builds, eliminates cross-TU dead code.
# These flags are NOT applied to Debug builds so developers don't pay the LTO
# link-time cost during iteration, and _FORTIFY_SOURCE=2 won't warn without -O.
if(NOT MSVC)
    # AR-11 D4: Compile-time hardening flags for Release / RelWithDebInfo / MinSizeRel.
    # add_compile_options applies globally to all targets in this directory tree.
    # Generator expressions gate them to non-Debug configurations.
    add_compile_options(
        "$<$<NOT:$<CONFIG:Debug>>:-fstack-protector-strong>"
        "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>"
    )
    if(WIN32)
        # AR-11 D4: PE security flags (Windows ASLR/DEP/HEASLR) — Release/RelWithDebInfo.
        # --dynamicbase  → IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE (ASLR)
        # --nxcompat     → IMAGE_DLLCHARACTERISTICS_NX_COMPAT (DEP/NX)
        # --high-entropy-va → 64-bit HEASLR (requires --dynamicbase)
        add_link_options(
            "$<$<NOT:$<CONFIG:Debug>>:-Wl,--dynamicbase,--nxcompat,--high-entropy-va>"
        )
    else()
        # L02 (NATIVE-LINUX-READINESS-2026-09-10): the PE flags above are not
        # valid ELF linker options — Linux GNU ld rejects them. Use the ELF
        # equivalents instead: full RELRO (relro + immediate binding), Release
        # builds only, matching the Windows gating shape.
        add_link_options(
            "$<$<NOT:$<CONFIG:Debug>>:-Wl,-z,relro,-z,now>"
        )
    endif()
    # AR-11 D4: LTO — opt-in via -DGLYPHPDF_ENABLE_LTO=ON.
    # Default OFF: LTO dramatically increases link time (minutes on large codebases).
    # The release CI leg sets GLYPHPDF_ENABLE_LTO=ON; dev builds get fast links.
    option(GLYPHPDF_ENABLE_LTO "Enable Link-Time Optimisation (LTO) for Release builds" OFF)
    if(GLYPHPDF_ENABLE_LTO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "AR-11 D4: LTO enabled (GLYPHPDF_ENABLE_LTO=ON)")
        add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
        add_link_options("$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
    endif()
endif()

# AR-11 D4: Strip release binaries so they do not leak build-host DWARF paths
# (the default MinGW Release build includes full debug symbols including
# the absolute source path C:\Users\User\Projects\pdf\src\...).
# strip is run as a post-build step on PdfWorkstation.exe in Release mode.
# Test executables are not stripped (dev-time debugging value).
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    if(WIN32)
        find_program(STRIP_PROGRAM strip
            PATHS "${MSYS2_ROOT}/bin" /c/msys64/ucrt64/bin
            NO_DEFAULT_PATH)
    else()
        # L01: on Linux the system strip is the right binary; the MSYS2-scoped
        # lookup above (NO_DEFAULT_PATH) never finds it.
        find_program(STRIP_PROGRAM strip)
    endif()
    if(STRIP_PROGRAM)
        message(STATUS "AR-11 D4: strip found at ${STRIP_PROGRAM} — Release EXE will be stripped")
    else()
        message(WARNING "AR-11 D4: strip not found — Release EXE will contain DWARF debug paths")
    endif()
endif()
# ── Dependencies ──────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Gui Widgets Pdf PdfWidgets PrintSupport Svg Network Test Concurrent REQUIRED)
find_package(Qt6 COMPONENTS LinguistTools QUIET)

# Query the actual Qt6 plugins dir — MSYS2 places them at share/qt6/plugins,
# not lib/../plugins as the Qt installer layout does.
get_target_property(_qt6_qmake Qt6::qmake IMPORTED_LOCATION)
if(_qt6_qmake)
    execute_process(
        COMMAND "${_qt6_qmake}" -query QT_INSTALL_PLUGINS
        OUTPUT_VARIABLE QT6_PLUGINS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT QT6_PLUGINS_DIR)
    set(QT6_PLUGINS_DIR "${Qt6_DIR}/../../../plugins")
    cmake_path(NORMAL_PATH QT6_PLUGINS_DIR)
endif()
message(STATUS "Qt6 plugins dir: ${QT6_PLUGINS_DIR}")

# L05 (NATIVE-LINUX-READINESS-2026-09-10): portable QPA plugin staging name for
# the per-test POST_BUILD deploys below — Windows stages qoffscreen.dll, Linux
# stages libqoffscreen.so (same relative platforms/ layout next to the binary).
if(WIN32)
    set(GLYPH_QPA_PLUGIN "qoffscreen.dll")
else()
    set(GLYPH_QPA_PLUGIN "libqoffscreen.so")
endif()

# Vendored podofo 1.1.0 (built from source with ucrt64 toolchain).
# Prepend to CMAKE_PREFIX_PATH so it is found before MSYS2 ucrt64's 0.10.4.
# L04 (NATIVE-LINUX-READINESS-2026-09-10): the vendored artifact is
# platform-specific. Windows deploys bin/libpodofo.dll (+ import lib); the
# Linux bootstrap installs a SEPARATE prefix (third_party/podofo/install-linux,
# lib/libpodofo.so + cmake config) so the Windows tree is never clobbered and
# the Linux build can never pick up Windows-only artifacts.
if(WIN32)
    set(_podofo_vendor "${CMAKE_CURRENT_SOURCE_DIR}/third_party/podofo/install")
    set(_podofo_vendor_art "${_podofo_vendor}/bin/libpodofo.dll")
else()
    set(_podofo_vendor "${CMAKE_CURRENT_SOURCE_DIR}/third_party/podofo/install-linux")
    set(_podofo_vendor_art "${_podofo_vendor}/lib/libpodofo.so")
endif()
if(EXISTS "${_podofo_vendor}/lib/cmake/podofo/podofo-config.cmake" AND
   EXISTS "${_podofo_vendor_art}")
    list(PREPEND CMAKE_PREFIX_PATH "${_podofo_vendor}")
    message(STATUS "Using vendored podofo 1.1.0 from ${_podofo_vendor}")
endif()
# Path to the vendored podofo 1.1.0 DLL — deployed next to test executables
# so the runtime loader picks it up instead of the system 0.10.4 DLL.
# Windows-only: on Linux the link graph resolves the vendored .so through the
# CMake config and RPATH, so no runtime staging exists or is needed.
if(WIN32)
    set(PODOFO_VENDOR_DLL "${_podofo_vendor}/bin/libpodofo.dll")
else()
    set(PODOFO_VENDOR_DLL "")
endif()

find_package(podofo CONFIG REQUIRED)
# Q02 trap guard: without the vendored tree, find_package silently resolves
# MSYS2 ucrt64's podofo 0.10.4, whose API mismatches this source (obscure
# build errors; tests exit 0xc0000135 when the 1.1.0 DLL is missing).
# scripts/bootstrap-vendor-deps.sh provisions third_party/podofo/install.
# NOTE: GLYPHPDF_RELEASE_BUILD is declared as an option further below; a
# command-line -D is already visible here, which is exactly how the release
# pipeline sets it — so the hard failure applies to release configuring only.
get_filename_component(_podofo_found_dir "${podofo_DIR}" ABSOLUTE)
file(TO_CMAKE_PATH "${_podofo_found_dir}" _podofo_found_dir)
if(NOT _podofo_found_dir MATCHES "third_party/podofo/install")
    if(GLYPHPDF_RELEASE_BUILD)
        message(FATAL_ERROR
            "Q02: podofo resolved to NON-VENDORED '${podofo_DIR}' - release builds "
            "must link the vendored podofo 1.1.0 from third_party/podofo/install. "
            "Run scripts/bootstrap-vendor-deps.sh, then reconfigure.")
    else()
        message(WARNING
            "Q02: podofo resolved to NON-VENDORED '${podofo_DIR}' - expected "
            "third_party/podofo/install (1.1.0). MSYS2's 0.10.x is API-incompatible "
            "with this source; run scripts/bootstrap-vendor-deps.sh (see README, "
            "'Vendored binary dependencies').")
    endif()
endif()
# podofo 1.1.0 exports 'podofo::podofo_shared' (namespaced).
# MSYS2 ucrt64 0.10.4 exports bare 'podofo_shared'.
# Alias whichever is present to the canonical 'podofo::podofo'.
if(TARGET podofo::podofo_shared AND NOT TARGET podofo::podofo)
    add_library(podofo::podofo ALIAS podofo::podofo_shared)
elseif(TARGET podofo_shared AND NOT TARGET podofo::podofo)
    add_library(podofo::podofo ALIAS podofo_shared)
endif()
find_package(OpenSSL REQUIRED)
find_package(OpenXLSX CONFIG QUIET)
find_package(duckx CONFIG QUIET)
find_package(CURL QUIET)
find_package(qpdf QUIET)
# quickjs-ng — the form-JavaScript execution engine (MIT; MSYS2 ucrt64 package
# `mingw-w64-ucrt-x86_64-quickjs-ng`, pinned 0.15.0-1). Optional: when absent
# the build stays green and form-JS capabilities disclose honestly via
# CapabilityRegistry (design doc §5 fallback).
find_package(qjs CONFIG QUIET)

# LibreOffice headless (Office→PDF import) is detected at RUNTIME by
# ConversionManager::locateSoffice() — never baked at build time, so the shipped
# binary works on whatever machine the user has. No build-time probe needed.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Pdfium QUIET)
find_package(libzip REQUIRED)

# D03 (QUALITY-GATE-2026-09-09): Tesseract OCR is an OPTIONAL dependency —
# every HAS_TESSERACT guard must compile and link in its absence. The switch
# is explicit (not environment-silent): GLYPHPDF_ENABLE_TESSERACT=OFF skips
# detection entirely, producing the complete no-Tesseract configuration the
# gate requires to be configured+built+linked end to end. The release feature
# gate still hard-fails when Tesseract is ACCIDENTALLY absent (option ON,
# detection failed) and only honours a deliberate OFF.
option(GLYPHPDF_ENABLE_TESSERACT
    "Build with Tesseract/Leptonica OCR (defines HAS_TESSERACT). OFF = complete no-Tesseract configuration (D03)"
    ON)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND GLYPHPDF_ENABLE_TESSERACT)
    pkg_check_modules(PKG_TESSERACT QUIET tesseract)
    pkg_check_modules(PKG_LEPTONICA QUIET lept)
endif()

# Robustness fallback (CI fix): in minimal environments pkg-config can fail to
# resolve tesseract.pc / lept.pc even when libtesseract + leptonica and their
# headers ARE installed (the .pc may be absent from the package or its transitive
# Requires.private graph is unsatisfiable). Probe the library + headers directly so
# OCR (HAS_TESSERACT) is enabled wherever Tesseract is genuinely present, instead of
# silently shipping an OCR-less build. find_library/find_path do not depend on .pc.
if(GLYPHPDF_ENABLE_TESSERACT AND NOT PKG_TESSERACT_FOUND)
    find_library(_TESS_LIB NAMES tesseract libtesseract tesseract55 tesseract54 tesseract-5)
    find_path(_TESS_INC NAMES tesseract/capi.h tesseract/baseapi.h)
    if(_TESS_LIB AND _TESS_INC)
        set(PKG_TESSERACT_FOUND TRUE)
        set(PKG_TESSERACT_LIBRARIES ${_TESS_LIB})
        set(PKG_TESSERACT_INCLUDE_DIRS ${_TESS_INC})
        message(STATUS "Tesseract found via find_library fallback: ${_TESS_LIB}")
    endif()
endif()
if(GLYPHPDF_ENABLE_TESSERACT AND NOT PKG_LEPTONICA_FOUND)
    find_library(_LEPT_LIB NAMES lept leptonica liblept lept-5)
    find_path(_LEPT_INC NAMES leptonica/allheaders.h allheaders.h)
    if(_LEPT_LIB AND _LEPT_INC)
        set(PKG_LEPTONICA_FOUND TRUE)
        set(PKG_LEPTONICA_LIBRARIES ${_LEPT_LIB})
        set(PKG_LEPTONICA_INCLUDE_DIRS ${_LEPT_INC})
        message(STATUS "Leptonica found via find_library fallback: ${_LEPT_LIB}")
    endif()
endif()

find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(PKG_ONNXRUNTIME QUIET libonnxruntime)
endif()

# ── License contamination guards (MuPDF AGPL-3.0 + Poppler GPL-2.0+) ──────────
# Four detection methods each: (1) CMake targets (bare + namespaced),
# (2) find_package, (3) pkg-config, (4) namespaced/aliased targets.
# AR-11 D3: broadened to cover namespaced targets (Poppler::poppler,
# unofficial::poppler, etc.) that vcpkg and some system CMake configs export.
# CI exercises this gate with a matrix leg that installs poppler and asserts
# configure fails (see .github/workflows/license-guard.yml).
#
# MuPDF AGPL-3.0 — NEVER link in-process
if(TARGET mupdf
   OR TARGET libmupdf
   OR TARGET mupdf-pkg
   OR TARGET MuPDF::MuPDF
   OR TARGET mupdf::mupdf
   OR TARGET PkgConfig::mupdf)
    message(FATAL_ERROR
        "MuPDF detected (AGPL-3.0). Linking MuPDF would force GlyphPDF under "
        "AGPL-3.0. Use PDFium (BSD-3-Clause) for rendering.")
endif()
# Poppler GPL-2.0+ — NEVER link in-process
if(TARGET poppler
   OR TARGET poppler-cpp
   OR TARGET libpoppler
   OR TARGET Poppler::poppler
   OR TARGET Poppler::poppler-cpp
   OR TARGET unofficial::poppler
   OR TARGET unofficial::poppler-cpp
   OR TARGET PkgConfig::POPPLER
   OR TARGET PkgConfig::poppler)
    message(FATAL_ERROR
        "Poppler detected (GPL-2.0+). Linking Poppler would force GlyphPDF under "
        "GPL-2.0+. Use PDFium (BSD-3-Clause) for rendering.")
endif()

find_package(MuPDF QUIET)
if(MuPDF_FOUND)
    message(FATAL_ERROR "MuPDF found via find_package (AGPL-3.0). Remove.")
endif()
find_package(Poppler QUIET)
if(Poppler_FOUND)
    message(FATAL_ERROR "Poppler found via find_package (GPL-2.0+). Remove.")
endif()

include(FindPkgConfig)
pkg_check_modules(MUPDF QUIET mupdf)
if(MUPDF_FOUND)
    message(FATAL_ERROR "MuPDF found via pkg-config (AGPL-3.0). Remove.")
endif()
pkg_check_modules(POPPLER QUIET poppler)
if(POPPLER_FOUND)
    message(FATAL_ERROR "Poppler found via pkg-config (GPL-2.0+). Remove.")
endif()
# Extra: catch poppler-cpp pkg-config module (Poppler can be found via either)
pkg_check_modules(POPPLER_CPP QUIET poppler-cpp)
if(POPPLER_CPP_FOUND)
    message(FATAL_ERROR "Poppler (poppler-cpp) found via pkg-config (GPL-2.0+). Remove.")
endif()

# 5th method — direct library probe. Some distributions (e.g. MSYS2) ship the
# poppler/mupdf shared library WITHOUT a matching top-level .pc or CMake config,
# so the target/find_package/pkg-config probes above miss them. find_library
# catches an installed libpoppler/libmupdf regardless of packaging metadata. On a
# normal GlyphPDF build host neither is installed (we use PDFium), so this is inert
# there; it only fires in the deliberate license-guard CI leg.
find_library(_GUARD_MUPDF_LIB NAMES mupdf libmupdf mupdf-third)
if(_GUARD_MUPDF_LIB)
    message(FATAL_ERROR
        "MuPDF library detected (${_GUARD_MUPDF_LIB}, AGPL-3.0). Linking MuPDF would "
        "force GlyphPDF under AGPL-3.0. Use PDFium (BSD-3-Clause).")
endif()
find_library(_GUARD_POPPLER_LIB NAMES poppler poppler-cpp libpoppler poppler-glib)
if(_GUARD_POPPLER_LIB)
    message(FATAL_ERROR
        "Poppler library detected (${_GUARD_POPPLER_LIB}, GPL-2.0+). Linking Poppler "
        "would force GlyphPDF under GPL-2.0+. Use PDFium (BSD-3-Clause).")
endif()

# ── pdfws_core (INTERFACE — header-only) ──────────────────────────────────
add_library(pdfws_core INTERFACE)
target_include_directories(pdfws_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pdfws_core INTERFACE
    Qt6::Core
    Qt6::Gui
)

# ── Third-party vendored dependencies ──────────────────────────────────────
add_subdirectory(third_party)

# ── docmodel (STATIC) ──────────────────────────────────────────────────────
add_subdirectory(src/docmodel)

# ── pdfws_djot (STATIC) ───────────────────────────────────────────────────
add_subdirectory(src/pdfws_djot)

# ── pdfws_engines (STATIC) ───────────────────────────────────────────────
add_library(pdfws_engines STATIC
    src/core/AnnotationSerializer.h  src/core/AnnotationSerializer.cpp
    src/core/ErrorInfo.h             src/core/ErrorInfo.cpp
    src/core/PageLabels.h            src/core/PageLabels.cpp
    src/core/Capability.h            src/core/Capability.cpp
    # R24 wiring closure: machine policy lives BELOW the UI layer so the
    # engine-layer enforcement points (OcrEngine download gate) can honor it
    # directly; pdfws_ui consumers (PreferencesDialog, GpMainWindow,
    # SupportBundle, NetworkTouchpoints, OllamaProvider) link it transitively.
    src/core/PolicyController.h      src/core/PolicyController.cpp
    src/core/FormStaleFieldTracker.h src/core/FormStaleFieldTracker.cpp
    src/core/TempFileManager.h       src/core/TempFileManager.cpp
    src/core/CredentialManager.h     src/core/CredentialManager.cpp
    src/core/ISecretStore.h
    src/core/BatchPreset.h           src/core/BatchPreset.cpp
    # Shared versioned-JSON file mechanics (SWEEP-QUALITY-NEW D1): the one
    # atomic artifact write behind BatchPreset store files / signing-request
    # sidecars / policy.json (fail-closed envelope laws stay per-surface).
    src/core/VersionedJson.h         src/core/VersionedJson.cpp
    src/core/EncryptedFileSecretStore.h src/core/EncryptedFileSecretStore.cpp
    src/engines/OcrEngine.h          src/engines/OcrEngine.cpp
    src/engines/PdfEditorEngine.h    src/engines/PdfEditorEngine.cpp
    src/engines/FormManager.h        src/engines/FormManager.cpp
    src/engines/SafeSave.h           src/engines/SafeSave.cpp
    # T2-4 accessibility P1: gap DETECTION (checker) + the cheap structural
    # fixes — no auto-tagging, no PDF/UA conformance claim (disclosed in UI).
    # PoFoDictRead.h: shared read-only PoDoFo dictionary helpers (D2).
    src/engines/PoFoDictRead.h
    src/engines/AccessibilityChecker.h src/engines/AccessibilityChecker.cpp
    src/engines/AccessibilityFixes.h   src/engines/AccessibilityFixes.cpp
    # T2-4 accessibility P2: the auto-tagging engine — raw-dict structure-tree
    # construction + BDC/EMC content-stream rewrite behind a text-preservation
    # invariant (paragraphs + headings only; /Alt never auto-generated).
    src/engines/AccessibilityTagger.h  src/engines/AccessibilityTagger.cpp
    src/engines/formjs/AFormShim.h   src/engines/formjs/AFormShim.cpp
    src/engines/formjs/FormJsSandbox.h src/engines/formjs/FormJsSandbox.cpp
    src/engines/formjs/FormJsRunner.h src/engines/formjs/FormJsRunner.cpp
    src/engines/RedactOperation.h    src/engines/RedactOperation.cpp
    src/core/RedactionProof.h        src/core/RedactionProof.cpp
    src/engines/SignatureManager.h   src/engines/SignatureManager.cpp
    src/core/SigningRequestModel.h   src/core/SigningRequestModel.cpp
    src/core/SigningRequestRunner.h  src/core/SigningRequestRunner.cpp
    src/core/SigningLabels.h         src/core/SigningLabels.cpp
    src/engines/SignatureFieldCreator.h src/engines/SignatureFieldCreator.cpp
    src/engines/ConversionManager.h  src/engines/ConversionManager.cpp
    src/engines/DocumentSession.h    src/engines/DocumentSession.cpp
    src/engines/AutosaveManager.h    src/engines/AutosaveManager.cpp
    src/engines/BackendRouter.h      src/engines/BackendRouter.cpp
    src/engines/PatternRedactor.h       src/engines/PatternRedactor.cpp
    src/engines/TextMatchFinder.h       src/engines/TextMatchFinder.cpp
    src/engines/ReviewSummaryWriter.h   src/engines/ReviewSummaryWriter.cpp
    src/engines/HeadingOutlineDetector.h src/engines/HeadingOutlineDetector.cpp
    src/core/StampLibrary.h             src/core/StampLibrary.cpp
    src/engines/podofo/PdfStringEscape.h src/engines/podofo/PdfStringEscape.cpp
    src/engines/podofo/GlyphAdvanceCalculator.h src/engines/podofo/GlyphAdvanceCalculator.cpp
    src/engines/podofo/PoDoFoBackend.h src/engines/podofo/PoDoFoBackend.cpp
    src/engines/podofo/ContentSpans.h src/engines/podofo/ContentSpans.cpp
    src/engines/podofo/PdfEncryptPubSec.h src/engines/podofo/PdfEncryptPubSec.cpp
    src/engines/podofo/PdfPageOps.h       src/engines/podofo/PdfPageOps.cpp
    src/engines/pdfium/PdfiumEnvironment.h src/engines/pdfium/PdfiumEnvironment.cpp
    src/engines/pdfium/PdfiumBackend.h src/engines/pdfium/PdfiumBackend.cpp
    src/engines/qpdf/QpdfBackend.h     src/engines/qpdf/QpdfBackend.cpp
    src/engines/RenderCache.h          src/engines/RenderCache.cpp
    src/engines/DiffEngine.h           src/engines/DiffEngine.cpp
    src/engines/MyersDiff.h            src/engines/MyersDiff.cpp
    src/engines/VeraPdfValidator.h     src/engines/VeraPdfValidator.cpp
    src/engines/scheduling/ILaneScheduler.h
    src/engines/scheduling/LaneScheduler.h  src/engines/scheduling/LaneScheduler.cpp
    src/engines/scheduling/PipelineStage.h
    src/engines/ocr/ILayoutDetector.h
    src/engines/ocr/PpDocLayoutDetector.h   src/engines/ocr/PpDocLayoutDetector.cpp
    src/engines/ocr/SuryaDetector.h         src/engines/ocr/SuryaDetector.cpp
    src/engines/ocr/LayoutEnsemble.h        src/engines/ocr/LayoutEnsemble.cpp
    src/engines/ocr/OcrPreprocessor.h       src/engines/ocr/OcrPreprocessor.cpp
    src/engines/ocr/OcrPipeline.h           src/engines/ocr/OcrPipeline.cpp
    src/engines/ocr/OcrDjotMapper.h         src/engines/ocr/OcrDjotMapper.cpp
    src/engines/ocr/PpOcrDecoder.h          src/engines/ocr/PpOcrDecoder.cpp
    src/engines/ocr/RapidOcrEngine.h        src/engines/ocr/RapidOcrEngine.cpp
    src/engines/mrc/MrcPageProcessor.h      src/engines/mrc/MrcPageProcessor.cpp
    src/core/interfaces/IPdfDocument.h
    src/core/interfaces/IPdfRenderer.h
    src/core/interfaces/IPdfWriter.h
    src/core/interfaces/IPdfSearcher.h
)
target_include_directories(pdfws_engines PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pdfws_engines
    PUBLIC  pdfws_core
    PRIVATE podofo::podofo OpenSSL::SSL OpenSSL::Crypto Qt6::Network Qt6::Widgets Qt6::Concurrent libzip::zip
            pdfws_djot docmodel
)
if(WIN32)
    target_link_libraries(pdfws_engines PRIVATE Credui Advapi32 Crypt32)
endif()

# ── L07: Linux Secret Service credential storage (libsecret) ─────────────────
# NATIVE-LINUX-READINESS-2026-09-10 L07: the non-Windows default write path
# derived its AES key from PUBLIC identifiers (home path + machine id + a
# constant) — the source itself calls it obfuscation. When libsecret is
# available, LibSecretStore (Secret Service / freedesktop keyring standard)
# becomes the Linux primary backend and no new secret is ever written through
# the identifier-derived path; a locked/unavailable keyring is a LOUD failure.
# Optional at build time: without libsecret the build stays green and the
# labelled encrypted-file fallback remains (documented obfuscation-only).
option(GLYPHPDF_ENABLE_LIBSECRET
    "Build with Secret Service credential storage via libsecret (Linux)" ON)
if(GLYPHPDF_ENABLE_LIBSECRET AND NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET QUIET libsecret-1)
    endif()
endif()
if(LIBSECRET_FOUND)
    target_sources(pdfws_engines PRIVATE
        src/core/LibSecretStore.h src/core/LibSecretStore.cpp
    )
    target_include_directories(pdfws_engines PUBLIC ${LIBSECRET_INCLUDE_DIRS})
    target_link_directories(pdfws_engines PUBLIC ${LIBSECRET_LIBRARY_DIRS})
    target_link_libraries(pdfws_engines PUBLIC ${LIBSECRET_LIBRARIES})
    target_compile_definitions(pdfws_engines PUBLIC HAS_LIBSECRET)
    message(STATUS "libsecret found — Secret Service credential storage enabled (L07)")
else()
    message(STATUS "libsecret not found/disabled — Secret Service storage off (labelled encrypted-file fallback remains)")
endif()
if(Tesseract_FOUND AND Leptonica_FOUND)
    find_package(OpenMP REQUIRED)
    target_link_libraries(pdfws_engines PUBLIC Tesseract::libtesseract leptonica OpenMP::OpenMP_CXX)
    target_compile_definitions(pdfws_engines PUBLIC HAS_TESSERACT)
elseif(PKG_TESSERACT_FOUND AND PKG_LEPTONICA_FOUND)
    target_include_directories(pdfws_engines PUBLIC ${PKG_TESSERACT_INCLUDE_DIRS} ${PKG_LEPTONICA_INCLUDE_DIRS})
    target_link_directories(pdfws_engines PUBLIC ${PKG_TESSERACT_LIBRARY_DIRS} ${PKG_LEPTONICA_LIBRARY_DIRS})
    target_link_libraries(pdfws_engines PUBLIC ${PKG_TESSERACT_LIBRARIES} ${PKG_LEPTONICA_LIBRARIES})
    target_compile_definitions(pdfws_engines PUBLIC HAS_TESSERACT)
else()
    message(STATUS "Tesseract/Leptonica not found — OCR disabled")
endif()

# ── jbig2enc (Apache-2.0) — MRC foreground JBIG2 lossless encoding ──────────
# NEVER pattern-matching mode (Xerox 2013 digit-substitution incident).
# Lossless generic-region only — enforced in MrcPageProcessor::encodeForegroundJbig2.
option(HAS_JBIG2ENC "Enable JBIG2 foreground encoding for MRC pipeline (Apache-2.0)" ON)
if(HAS_JBIG2ENC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/jbig2enc")
    add_subdirectory(third_party/jbig2enc EXCLUDE_FROM_ALL)
    target_link_libraries(pdfws_engines PRIVATE libjbig2enc)
    target_include_directories(pdfws_engines PRIVATE third_party/jbig2enc/src)
    target_compile_definitions(pdfws_engines PRIVATE HAS_JBIG2ENC)
    message(STATUS "jbig2enc: ENABLED (Apache-2.0) — JBIG2 lossless foreground encoding active")
else()
    message(STATUS "jbig2enc: DISABLED — MRC foreground will use Flate fallback")
endif()

# ── OpenJPEG2 (BSD-2-Clause) — MRC background encoding ─────────────────────
# REQUIRED: MrcPageProcessor.cpp includes <openjpeg.h> unconditionally.
# Prefer the CMake config package (MSYS2 ships openjpeg-2.5 config; pkg-config
# detection is unreliable on CI runners); fall back to pkg-config.
find_package(OpenJPEG CONFIG QUIET)
if(OpenJPEG_FOUND)
    target_include_directories(pdfws_engines PUBLIC ${OPENJPEG_INCLUDE_DIRS})
    target_link_libraries(pdfws_engines PUBLIC openjp2)
    target_compile_definitions(pdfws_engines PUBLIC HAS_OPENJPEG2)
    message(STATUS "OpenJPEG2 found (CMake config ${OPENJPEG_VERSION}) — MRC JPEG2000 encoding enabled")
else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(OPENJPEG2 QUIET libopenjp2)
    endif()
    if(OPENJPEG2_FOUND)
        target_include_directories(pdfws_engines PUBLIC ${OPENJPEG2_INCLUDE_DIRS})
        target_link_directories(pdfws_engines PUBLIC ${OPENJPEG2_LIBRARY_DIRS})
        target_link_libraries(pdfws_engines PUBLIC ${OPENJPEG2_LIBRARIES})
        target_compile_definitions(pdfws_engines PUBLIC HAS_OPENJPEG2)
        message(STATUS "OpenJPEG2 found (pkg-config ${OPENJPEG2_VERSION}) — MRC JPEG2000 encoding enabled")
    else()
        message(FATAL_ERROR "OpenJPEG2 not found — required by MrcPageProcessor (MRC JPEG2000 encoding). "
                            "Install mingw-w64-ucrt-x86_64-openjpeg2 (MSYS2) or provide OpenJPEGConfig.cmake.")
    endif()
endif()

# L04 (NATIVE-LINUX-READINESS-2026-09-10): the Windows ONNX runtime tree
# (onnxruntime-win-x64-1.17.3: .dll/.lib PE artifacts) must only ever be
# imported on Windows. On Linux its mere presence on disk used to create an
# imported target pointing at PE binaries — unresolvable at link time. Linux
# builds use a native onnxruntime (pkg-config) when present, else RapidOCR
# stays disabled and CapabilityRegistry discloses it.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3")
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3/lib/onnxruntime.dll"
        IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3/lib/onnxruntime.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3/include"
    )
    set(onnxruntime_FOUND TRUE)
endif()

if(onnxruntime_FOUND OR TARGET onnxruntime)
    target_link_libraries(pdfws_engines PUBLIC onnxruntime)
    target_compile_definitions(pdfws_engines PUBLIC HAS_RAPIDOCR)
elseif(PKG_ONNXRUNTIME_FOUND)
    target_include_directories(pdfws_engines PUBLIC ${PKG_ONNXRUNTIME_INCLUDE_DIRS})
    target_link_directories(pdfws_engines PUBLIC ${PKG_ONNXRUNTIME_LIBRARY_DIRS})
    target_link_libraries(pdfws_engines PUBLIC ${PKG_ONNXRUNTIME_LIBRARIES})
    target_compile_definitions(pdfws_engines PUBLIC HAS_RAPIDOCR)
else()
    message(STATUS "onnxruntime not found — RapidOCR PP-OCRv5 disabled")
endif()

# Deploy OCR models beside the build output so RapidOcrEngine + tests find them
# at runtime via applicationDirPath()/models/ppocrv5 (dev/test convenience).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models/ppocrv5")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/models/ppocrv5"
         DESTINATION "${CMAKE_BINARY_DIR}/models")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models/pp_doclayout")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/models/pp_doclayout"
         DESTINATION "${CMAKE_BINARY_DIR}/models")
endif()
if(qpdf_FOUND AND (TARGET qpdf::qpdf OR TARGET qpdf::libqpdf))
    if(TARGET qpdf::qpdf)
        target_link_libraries(pdfws_engines PUBLIC qpdf::qpdf)
    else()
        target_link_libraries(pdfws_engines PUBLIC qpdf::libqpdf)
    endif()
    target_compile_definitions(pdfws_engines PUBLIC HAS_QPDF)
else()
    message(STATUS "qpdf not found — linearization disabled")
endif()

if(Pdfium_FOUND AND TARGET Pdfium::Pdfium)
    target_link_libraries(pdfws_engines PUBLIC Pdfium::Pdfium)
    target_compile_definitions(pdfws_engines PUBLIC HAS_PDFIUM)
else()
    message(STATUS "PDFium not found — PdfiumBackend disabled")
endif()

# quickjs-ng (form JavaScript execution — Phase 1 Calculate/Format, MIT).
# The sandbox contract (design doc §3) lives in FormJsSandbox: memory cap,
# interrupt deadline, and ZERO host I/O — the quickjs-libc I/O module is never
# initialized, so no filesystem/network primitive is reachable from scripts.
if(TARGET qjs)
    target_link_libraries(pdfws_engines PUBLIC qjs)
    target_compile_definitions(pdfws_engines PUBLIC HAS_QUICKJS)

    # R18(c): the exact quickjs-ng version is an ENFORCED PIN, not a comment
    # (PERFORMANCE-AND-SCRIPT-REVIEW-2026-09-10: "Comments and an unversioned
    # find_package(qjs) are not an enforced dependency pin"). The sandbox and
    # AF-shim goldens (tests/TestFormJsCalc.cpp) pin the scripting semantics of
    # the authorized package (ledger rows 2026-09-10 form-JS Phase 1 and
    # 2026-09-13 R18: user-authorized Option A, MSYS2
    # mingw-w64-ucrt-x86_64-quickjs-ng 0.15.0-1, MIT, runtime DLL
    # libqjs-0.dll sha256 dd3100904db03feaadf63a231c868286ab9c69053820ab4e3037022dd9573a4c);
    # an unreviewed runtime bump silently changes those semantics. The check
    # parses the version the LINKED headers advertise (quickjs.h QJS_VERSION_*)
    # and hard-fails on mismatch. The deliberate-upgrade path: bump
    # GLYPHPDF_QUICKJS_PIN, re-run the goldens, record the new package/runtime
    # hash + re-authorization in the ledger.
    set(GLYPHPDF_QUICKJS_PIN "0.15.0" CACHE STRING
        "Enforced quickjs-ng version pin (R18c); upgrade deliberately, see ledger")
    get_target_property(_qjs_inc_dirs qjs INTERFACE_INCLUDE_DIRECTORIES)
    set(_qjs_version_header "")
    foreach(_dir ${_qjs_inc_dirs})
        if(EXISTS "${_dir}/quickjs.h")
            set(_qjs_version_header "${_dir}/quickjs.h")
            break()
        endif()
    endforeach()
    if(NOT _qjs_version_header)
        message(FATAL_ERROR
            "quickjs-ng pin check: quickjs.h not found under ${_qjs_inc_dirs} — refusing to link an unverifiable scripting runtime")
    endif()
    file(STRINGS "${_qjs_version_header}" _qjs_ver_lines
        REGEX "^#define QJS_VERSION_(MAJOR|MINOR|PATCH|SUFFIX)")
    set(_qjs_found "")
    foreach(_line IN LISTS _qjs_ver_lines)
        if(_line MATCHES "#define QJS_VERSION_MAJOR ([0-9]+)")
            string(APPEND _qjs_found "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "#define QJS_VERSION_MINOR ([0-9]+)")
            string(APPEND _qjs_found ".${CMAKE_MATCH_1}")
        elseif(_line MATCHES "#define QJS_VERSION_PATCH ([0-9]+)")
            string(APPEND _qjs_found ".${CMAKE_MATCH_1}")
        elseif(_line MATCHES "#define QJS_VERSION_SUFFIX \"?([^\"]*)\"?")
            string(APPEND _qjs_found "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT _qjs_found)
        message(FATAL_ERROR
            "quickjs-ng pin check: could not parse QJS_VERSION_* from ${_qjs_version_header} — refusing to link an unpinned scripting runtime")
    endif()
    if(NOT _qjs_found STREQUAL GLYPHPDF_QUICKJS_PIN)
        message(FATAL_ERROR
            "quickjs-ng version pin violated: the linked headers advertise ${_qjs_found}, the enforced pin is ${GLYPHPDF_QUICKJS_PIN}. "
            "Authorized package: mingw-w64-ucrt-x86_64-quickjs-ng 0.15.0-1 (ledger 2026-09-10/2026-09-13). "
            "A runtime bump changes the scripting semantics the AF-shim goldens pin — upgrade deliberately: "
            "bump GLYPHPDF_QUICKJS_PIN, re-run the golden tests, record the new package/runtime hash in the ledger.")
    endif()
    # Attribution: the engine version is compiled in so the CapabilityRegistry
    # disclosure and diagnostics name exactly what is linked.
    target_compile_definitions(pdfws_engines PUBLIC GLYPHPDF_QUICKJS_VERSION="${_qjs_found}")
    message(STATUS "quickjs-ng version pin satisfied: ${_qjs_found} (enforced, R18c)")

    # L05: the runtime-DLL staging path is a Windows concept; on Linux the
    # system libqjs resolves at link time, so no runtime artifact is tracked.
    if(WIN32)
        set(QUICKJS_RUNTIME_DLL "${QT6_PLUGINS_DIR}/../bin/libqjs-0.dll")
        get_target_property(_qjs_location qjs IMPORTED_LOCATION_RELEASE)
        if(_qjs_location)
            get_filename_component(QUICKJS_RUNTIME_DLL "${_qjs_location}" REALPATH)
        endif()
    else()
        set(QUICKJS_RUNTIME_DLL "")
    endif()
    message(STATUS "quickjs-ng found — form-JS Calculate/Format execution enabled (${QUICKJS_RUNTIME_DLL})")
else()
    set(QUICKJS_RUNTIME_DLL "")
    message(STATUS "quickjs-ng not found — form-JS execution disabled (CapabilityRegistry disclosure)")
endif()

# AR-11 D5: Feature/version summary + hard-fail on missing shipped-feature defines.
# The four core features listed in README.md / release notes MUST be present in any
# artifact that will be signed and submitted to winget. A QUIET find_package() today
# silently drops features with no build error. This gate makes omissions loud.
#
# GLYPHPDF_RELEASE_BUILD=ON is set by the release CI leg (packaging/build-msi.ps1)
# and the release workflow. Developer builds with missing deps are allowed to proceed
# (STATUS messages only); release builds hard-fail.
option(GLYPHPDF_RELEASE_BUILD "Hard-fail on any missing shipped feature (set by release pipeline)" OFF)

# Track which shipped features are present
set(_has_pdfium   FALSE)
set(_has_tesseract FALSE)
set(_has_rapidocr  FALSE)
set(_has_qpdf      FALSE)

if(Pdfium_FOUND AND TARGET Pdfium::Pdfium)
    set(_has_pdfium TRUE)
endif()
if(PKG_TESSERACT_FOUND OR Tesseract_FOUND)
    set(_has_tesseract TRUE)
endif()
if(onnxruntime_FOUND OR TARGET onnxruntime OR PKG_ONNXRUNTIME_FOUND)
    set(_has_rapidocr TRUE)
endif()
if(qpdf_FOUND AND (TARGET qpdf::qpdf OR TARGET qpdf::libqpdf))
    set(_has_qpdf TRUE)
endif()
set(_has_quickjs FALSE)
if(TARGET qjs)
    set(_has_quickjs TRUE)
endif()

message(STATUS "")
message(STATUS "===== GlyphPDF ${PROJECT_VERSION} — Feature Summary (AR-11 D5) =====")
message(STATUS "  HAS_PDFIUM    : ${_has_pdfium}   (rendering engine — PDF.js-speed rendering)")
message(STATUS "  HAS_TESSERACT : ${_has_tesseract}  (primary OCR engine — Tesseract 5)")
message(STATUS "  HAS_RAPIDOCR  : ${_has_rapidocr}  (secondary OCR — PP-OCRv5 ROVER ensemble)")
message(STATUS "  HAS_QPDF      : ${_has_qpdf}   (PDF linearisation)")
message(STATUS "  HAS_QUICKJS   : ${_has_quickjs}  (form-JS Calculate/Format execution)")
message(STATUS "  LTO           : ${GLYPHPDF_ENABLE_LTO}")
message(STATUS "  Release build : ${GLYPHPDF_RELEASE_BUILD}")
message(STATUS "===================================================================")
message(STATUS "")

if(GLYPHPDF_RELEASE_BUILD)
    set(_missing_features "")
    if(NOT _has_pdfium)
        list(APPEND _missing_features "HAS_PDFIUM (PDFium not found)")
    endif()
    if(NOT _has_tesseract)
        if(GLYPHPDF_ENABLE_TESSERACT)
            list(APPEND _missing_features "HAS_TESSERACT (Tesseract not found)")
        else()
            # D03: an EXPLICIT GLYPHPDF_ENABLE_TESSERACT=OFF is a deliberate
            # optional-dependency build, not an accidentally missing feature.
            # The gate stays hard for accidental absence (option ON, no
            # Tesseract found) and records the deliberate opt-out loudly here.
            message(STATUS
                "  release gate: HAS_TESSERACT absent by explicit "
                "GLYPHPDF_ENABLE_TESSERACT=OFF (D03 optional-dependency build)")
        endif()
    endif()
    if(NOT _has_rapidocr)
        list(APPEND _missing_features "HAS_RAPIDOCR (ONNX Runtime not found)")
    endif()
    if(NOT _has_qpdf)
        list(APPEND _missing_features "HAS_QPDF (qpdf not found)")
    endif()
    if(_missing_features)
        message(FATAL_ERROR
            "AR-11 D5: RELEASE BUILD FAILED — missing shipped feature(s): "
            "${_missing_features} — "
            "All four core features must be present before signing and publishing. "
            "Resolve missing dependencies or set GLYPHPDF_RELEASE_BUILD=OFF for dev builds.")
    else()
        message(STATUS "AR-11 D5: Release build — all four shipped features confirmed present.")
    endif()
endif()

# ── Stage runtime DLLs beside test/app binaries (WINDOWS ONLY) ──────────────
# L05 (NATIVE-LINUX-READINESS-2026-09-10): this whole staging step is a
# Windows-loader workaround (exe-dir DLL resolution, MinGW toolchain PATH
# dependency). Linux resolves shared libraries through RPATH and system paths;
# staging DLLs there is meaningless. The block is gated on WIN32 — Linux
# configure/build is identical but skips it entirely.
if(WIN32)
set(_runtime_dlls "")
get_filename_component(_glyph_toolchain_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
file(GLOB _glyph_toolchain_dlls
    "${_glyph_toolchain_bin}/Qt6*.dll"
    "${_glyph_toolchain_bin}/lib*.dll"
    "${_glyph_toolchain_bin}/zlib1.dll")
list(FILTER _glyph_toolchain_dlls EXCLUDE REGEX "podofo")
list(APPEND _runtime_dlls ${_glyph_toolchain_dlls})
if(EXISTS "${PODOFO_VENDOR_DLL}")
    list(APPEND _runtime_dlls "${PODOFO_VENDOR_DLL}")
endif()
if(TARGET onnxruntime)
    list(APPEND _runtime_dlls "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3/lib/onnxruntime.dll")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll")
    list(APPEND _runtime_dlls "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll")
endif()
if(_runtime_dlls)
    add_custom_target(stage_runtime_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_runtime_dlls} "${CMAKE_BINARY_DIR}"
        COMMENT "Staging runtime DLLs into build dir")
endif()
endif()
# Staging the Qt DLLs beside the exes changes Qt's RELATIVE plugin-prefix
# resolution: plugins resolve next to the LOADED Qt6Core library
# (<dir>/../share/qt6/plugins — previously a toolchain path, now the build
# dir's parent), which silently removed qjpeg and every other Qt image-format
# plugin from the tests (TestCompressJpegReencode: "test JPEG encode failed").
# Pin the real plugin directory with a qt.conf; the application directory
# stays in Qt's plugin search path, so the per-exe qoffscreen deploys keep
# working.
# L05 (NATIVE-LINUX-READINESS-2026-09-10): use the ALREADY-QUERIED plugin
# directory (qmake -query QT_INSTALL_PLUGINS) instead of inferring the MSYS2
# share/qt6/plugins layout from Qt6_DIR. On MSYS2 the queried value is the
# same directory the inference produced, so Windows behavior is unchanged; on
# Linux the query returns the distro plugin dir (e.g.
# /usr/lib/x86_64-linux-gnu/qt6/plugins), which the old inference got wrong.
if(QT6_PLUGINS_DIR)
    file(WRITE "${CMAKE_BINARY_DIR}/qt.conf"
        "[Paths]\nPlugins=${QT6_PLUGINS_DIR}\n")
else()
    get_filename_component(_glyph_qt_prefix "${Qt6_DIR}/../../../" ABSOLUTE)
    file(WRITE "${CMAKE_BINARY_DIR}/qt.conf"
        "[Paths]\nPlugins=${_glyph_qt_prefix}/share/qt6/plugins\n")
endif()
if(OpenXLSX_FOUND)
    target_link_libraries(pdfws_engines PRIVATE OpenXLSX::OpenXLSX)
    target_compile_definitions(pdfws_engines PRIVATE HAS_OPENXLSX)
endif()
if(duckx_FOUND)
    target_link_libraries(pdfws_engines PRIVATE duckx::duckx)
    target_compile_definitions(pdfws_engines PRIVATE HAS_DUCKX)
endif()

# ── DjVu importer (GPL-2.0+ optional; default OFF) ──────────────────────────
# When enabled: links DjVuLibre. User accepts GPL-2.0+ by enabling this option.
# Import-only — no DjVu output (excluded per ROADMAP).
# See docs/MRC-ENCODER-LICENSE-AUDIT.md §3 for license rationale.
option(HAS_DJVU "Enable optional DjVu importer via DjVuLibre (GPL-2.0+)" OFF)
if(HAS_DJVU)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(DJVULIBRE REQUIRED ddjvuapi)
        target_include_directories(pdfws_engines PUBLIC ${DJVULIBRE_INCLUDE_DIRS})
        target_link_directories(pdfws_engines PUBLIC ${DJVULIBRE_LIBRARY_DIRS})
        target_link_libraries(pdfws_engines PUBLIC ${DJVULIBRE_LIBRARIES})
    else()
        find_library(DJVULIBRE_LIB djvulibre REQUIRED)
        target_link_libraries(pdfws_engines PUBLIC ${DJVULIBRE_LIB})
    endif()
    target_compile_definitions(pdfws_engines PUBLIC HAS_DJVU)
    message(STATUS "DjVu importer: ENABLED via DjVuLibre (GPL-2.0+ accepted)")
else()
    message(STATUS "DjVu importer: DISABLED (HAS_DJVU=OFF, default)")
endif()

# DjvuImporter sources (always compiled; #ifdef HAS_DJVU guards the actual API calls)
target_sources(pdfws_engines PRIVATE
    src/engines/conversion/DjvuImporter.h
    src/engines/conversion/DjvuImporter.cpp
)

# veraPDF CLI (AGPL-3.0 — subprocess only, never linked in-process) is located at
# RUNTIME by VeraPdfValidator::locateCli(): a copy bundled next to the app
# (verapdf/ — staged by deploy.ps1), the GLYPHPDF_VERAPDF env var, or the PATH.
# No build-time path is baked, so the shipped binary works regardless of the build
# machine. PdfAValidationPanel shows 'validator unavailable' only when none is found.

# ── pdfws_commands (STATIC — mostly header-only but needs moc for some) ──
add_library(pdfws_commands STATIC
    src/commands/RotatePageCommand.h
    src/commands/DeletePageCommand.h
    src/commands/InsertPageCommand.h
    src/commands/SetMetadataCommand.h
    src/commands/AddFormFieldCommand.h
    src/commands/EditFormFieldCommand.h
    src/commands/DeleteFormFieldCommand.h
    src/commands/MoveFormFieldCommand.h
    src/commands/ResizeFormFieldCommand.h
    src/commands/EncryptDocumentHelper.h
    src/commands/SignDocumentHelper.h
    src/commands/SanitizeDocumentHelper.h
    src/commands/MoveImageCommand.h
    src/commands/ResizeImageCommand.h
    src/commands/RotateImageCommand.h
    src/commands/ReplaceImageCommand.h
    src/commands/DeleteImageCommand.h
    src/commands/ReorderPermutationCommand.h
    src/commands/ReorderPermutationCommand.cpp
    src/commands/CropPageCommand.h
    src/commands/CropPageCommand.cpp
    src/commands/AutoDetectPlacement.h
    src/commands/AutoDetectPlacement.cpp
)
target_include_directories(pdfws_commands PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pdfws_commands
    PUBLIC pdfws_core Qt6::Widgets  # QUndoCommand is in Qt6::Widgets
)

# ── pdfws_ui (STATIC) ────────────────────────────────────────────────────
add_library(pdfws_ui STATIC
    src/ui/PdfViewerWidget.h         src/ui/PdfViewerWidget.cpp
    src/ui/EditAnnotationCommand.h   src/ui/EditAnnotationCommand.cpp
    src/ui/PrintJob.h                src/ui/PrintJob.cpp

    src/ui/WelcomeWidget.h           src/ui/WelcomeWidget.cpp
    src/ui/FindBar.h                 src/ui/FindBar.cpp
    src/ui/FindReplaceDialog.h       src/ui/FindReplaceDialog.cpp
    src/ui/StampLibraryDialog.h      src/ui/StampLibraryDialog.cpp
    src/ui/AutoBookmarkDialog.h      src/ui/AutoBookmarkDialog.cpp
    src/ui/ResizeDialog.h            src/ui/ResizeDialog.cpp
    src/ui/HeaderFooterDialog.h      src/ui/HeaderFooterDialog.cpp
    src/ui/BatesNumberingDialog.h    src/ui/BatesNumberingDialog.cpp
    src/ui/EditToolBar.h             src/ui/EditToolBar.cpp
    src/ui/InspectorWidget.h         src/ui/InspectorWidget.cpp
    src/ui/CompareWidget.h           src/ui/CompareWidget.cpp
    src/ui/BookmarkPanel.h           src/ui/BookmarkPanel.cpp
    src/ui/CommentsWidget.h          src/ui/CommentsWidget.cpp
    src/ui/DocumentPropertiesWidget.h src/ui/DocumentPropertiesWidget.cpp
    src/ui/SignaturesWidget.h        src/ui/SignaturesWidget.cpp
    src/ui/PageManagementDialog.h    src/ui/PageManagementDialog.cpp
    src/ui/EncryptionDialog.h        src/ui/EncryptionDialog.cpp
    src/ui/PermissionsDialog.h       src/ui/PermissionsDialog.cpp
    src/ui/SignatureDialog.h         src/ui/SignatureDialog.cpp
    src/ui/SigningRequestDialog.h    src/ui/SigningRequestDialog.cpp
    src/ui/SigningProgressPanel.h    src/ui/SigningProgressPanel.cpp
    src/ui/RecipientPickerDialog.h   src/ui/RecipientPickerDialog.cpp
    src/ui/SignaturePicker.h         src/ui/SignaturePicker.cpp

    # R24 wiring closure: the OCSP network consent surface (per-document
    # consent dialog + global never-network switch consumed by
    # SecurityController::runSigning; see OcspConsentDialog.h).
    src/ui/OcspConsentDialog.h       src/ui/OcspConsentDialog.cpp
    src/ui/MetadataDialog.h          src/ui/MetadataDialog.cpp
    src/ui/AnnotationLayer.h         src/ui/AnnotationLayer.cpp
    src/ui/ThumbnailSidebar.h        src/ui/ThumbnailSidebar.cpp
    src/ui/ShortcutHelpDialog.h      src/ui/ShortcutHelpDialog.cpp
    src/ui/PreferencesDialog.h       src/ui/PreferencesDialog.cpp
    src/ui/UpdateDialog.h            src/ui/UpdateDialog.cpp
    src/ui/RecoveryDialog.h          src/ui/RecoveryDialog.cpp
    src/ui/ErrorDialog.h             src/ui/ErrorDialog.cpp
    src/ui/PageSetupDialog.h         src/ui/PageSetupDialog.cpp
    src/ui/ExportPresetsPanel.h      src/ui/ExportPresetsPanel.cpp

    src/core/UpdateChecker.h         src/core/UpdateChecker.cpp

    # R24(b): redacted support bundle (counts only, no paths/PDF content).
    src/core/SupportBundle.h         src/core/SupportBundle.cpp

    # R24(c): network touchpoint enumeration (read-only audit surface).
    src/core/NetworkTouchpoints.h    src/core/NetworkTouchpoints.cpp

    src/core/ToolId.h                       src/core/ToolId.cpp
    src/shell/ToolRegistry.h                src/shell/ToolRegistry.cpp
    src/core/interfaces/IToolController.h

    # ── New Glyph Design UI (gp:: namespace) ──
    src/GpMainWindow.h                      src/GpMainWindow.cpp
    src/shell/controllers/HomeController.h  src/shell/controllers/HomeController.cpp
    src/shell/controllers/ViewController.h  src/shell/controllers/ViewController.cpp
    src/shell/controllers/EditController.h  src/shell/controllers/EditController.cpp
    src/shell/controllers/PagesController.h src/shell/controllers/PagesController.cpp
    src/shell/controllers/ConvertController.h src/shell/controllers/ConvertController.cpp
    src/shell/controllers/FormsController.h  src/shell/controllers/FormsController.cpp
    src/shell/controllers/SecurityController.h src/shell/controllers/SecurityController.cpp
    src/shell/controllers/CertEncryptController.h src/shell/controllers/CertEncryptController.cpp
    src/shell/controllers/SendForSigningController.h src/shell/controllers/SendForSigningController.cpp
    src/shell/controllers/TaskNavController.h src/shell/controllers/TaskNavController.cpp
    src/util/GpTheme.h
    # D-07 fix: OCR sources removed from pdfws_ui — they are compiled once in
    # pdfws_engines (lines above) and exposed to pdfws_ui via the PRIVATE link.
    # Duplicating them caused two ODR copies of static state in the final binary.

    src/util/Icons.h                        src/util/Icons.cpp
    src/util/Badge.h                        src/util/Badge.cpp
    src/util/Slider.h                       src/util/Slider.cpp
    src/shell/MenuBar.h                     src/shell/MenuBar.cpp
    src/shell/Ribbon.h                      src/shell/Ribbon.cpp
    src/shell/RibbonModel.h                 src/shell/RibbonModel.cpp
    src/shell/ModeStrip.h                   src/shell/ModeStrip.cpp
    src/shell/ScreenNav.h                   src/shell/ScreenNav.cpp
    # F1 (SWEEP-W3-UI): wrapping toolbar layout for the mode strips.
    src/shell/FlowToolbarLayout.h           src/shell/FlowToolbarLayout.cpp
    src/shell/StatusBar.h                   src/shell/StatusBar.cpp
    src/shell/TaskNav.h                     src/shell/TaskNav.cpp
    src/shell/TaskStateSync.h               src/shell/TaskStateSync.cpp
    src/shell/Sidebar.h                     src/shell/Sidebar.cpp
    src/modes/ModeController.h              src/modes/ModeController.cpp
    src/modes/OCRMode.h                     src/modes/OCRMode.cpp
    src/modes/OcrConfidence.h               src/modes/OcrConfidence.cpp
    src/ui/OcrScanCanvas.h                  src/ui/OcrScanCanvas.cpp
    src/ui/OcrWordMagnifier.h               src/ui/OcrWordMagnifier.cpp
    src/modes/RedactMode.h                  src/modes/RedactMode.cpp
    src/modes/RedactApplyDialog.h           src/modes/RedactApplyDialog.cpp
    src/modes/CompareMode.h                 src/modes/CompareMode.cpp
    src/modes/PagesMode.h                   src/modes/PagesMode.cpp
    src/modes/BatchMode.h                   src/modes/BatchMode.cpp
    src/modes/FormBuilderMode.h             src/modes/FormBuilderMode.cpp
    src/modes/FormFieldPropertiesPanel.h    src/modes/FormFieldPropertiesPanel.cpp
    src/modes/AIChatPanel.h                 src/modes/AIChatPanel.cpp
    src/engines/ai/IAiProvider.h
    src/engines/ai/OllamaProvider.h         src/engines/ai/OllamaProvider.cpp
    src/modes/SignaturesPanel.h             src/modes/SignaturesPanel.cpp
    src/modes/PdfAValidationPanel.h         src/modes/PdfAValidationPanel.cpp
    # T2-4 accessibility P1: checker panel (detection + disclosure only).
    src/modes/AccessibilityPanel.h          src/modes/AccessibilityPanel.cpp
    src/modes/MeasureMode.h                 src/modes/MeasureMode.cpp
    src/modes/CompressDialog.h              src/modes/CompressDialog.cpp
    src/modes/WatermarkDialog.h             src/modes/WatermarkDialog.cpp
)
target_include_directories(pdfws_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pdfws_ui
    PUBLIC  pdfws_core pdfws_commands
    PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::PrintSupport Qt6::Svg Qt6::Network
            Qt6::Concurrent podofo::podofo pdfws_djot docmodel
            pdfws_engines  # D2: ThumbnailSidebar uses RenderCache for thumbnails
    # N17: RecipientPickerDialog parses recipient certificates itself
    # (subject + SHA-256 fingerprint) so the list shows certificate identity.
    PRIVATE OpenSSL::Crypto
)
target_compile_definitions(pdfws_ui PRIVATE GLYPH_HAS_SVG=1)

if(WIN32)
    # N-2: WinVerifyTrust for MSI Authenticode checking
    target_link_libraries(pdfws_ui PRIVATE wintrust)
endif()
# Disable PCH for files with heavy initializer lists (prevents OOM in g++)
set_source_files_properties(
    src/shell/RibbonModel.cpp
    src/modes/OCRMode.cpp
    src/modes/CompareMode.cpp
    src/modes/BatchMode.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# ── PdfWorkstation (EXECUTABLE) ───────────────────────────────────────────
add_executable(PdfWorkstation
    src/app/main.cpp
    src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    resources.qrc
)
target_link_libraries(PdfWorkstation PRIVATE
    pdfws_ui
    pdfws_engines
    pdfws_commands
    pdfws_core
    Qt6::Concurrent
)
# pdfws_djot + liblua in a link group so GNU ld makes multiple passes
# and resolves forward references between the static archives.
if(NOT MSVC)
    target_link_libraries(PdfWorkstation PRIVATE
        "-Wl,--start-group"
        pdfws_djot
        docmodel
        liblua
        "-Wl,--end-group"
    )
else()
    target_link_libraries(PdfWorkstation PRIVATE pdfws_djot docmodel liblua)
endif()

# ARCH-4: Inject djot path.
# IMPORTANT: DJOT_LIB_DIR is defined only for PdfWorkstation. If any test target
# ever adds Bootstrapper.cpp to its sources, add the same target_compile_definitions
# to that test target, otherwise Bootstrapper.cpp's #error will abort the build.
target_compile_definitions(PdfWorkstation PRIVATE
    DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    GLYPHPDF_VERSION="${PROJECT_VERSION}"
)

# Windows: hide console
if(WIN32 AND NOT MSVC)
    target_link_options(PdfWorkstation PRIVATE "-Wl,-subsystem,windows")
endif()

# Windows: embed the application icon into the executable (Explorer/taskbar file icon)
if(WIN32)
    enable_language(RC)
    target_sources(PdfWorkstation PRIVATE src/app/app_icon.rc)
endif()

# INF03: embed the release version in the EXE's Windows VERSIONINFO resource so
# the built artifact itself carries PROJECT_VERSION. Packaging (build-msi.ps1,
# WiX package metadata, portable ZIP name + README) reads this same single
# source — one authoritative version, verifiable in the binary, not just in
# CMake variables.
if(NOT PROJECT_VERSION_TWEAK)
    set(PROJECT_VERSION_TWEAK 0)
endif()
set(GLYPHPDF_EXE_NAME "PdfWorkstation.exe")
configure_file(src/app/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)
if(WIN32)
    target_sources(PdfWorkstation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
endif()

# AR-11 D4: Strip PdfWorkstation.exe in Release builds to remove DWARF debug paths.
# DWARF in a MinGW/GCC Release binary leaks the build-host path
# (C:\Users\User\Projects\pdf) into every shipped binary.
# Post-build step: strip is applied only when explicitly found (above) in Release mode.
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release" AND STRIP_PROGRAM)
    add_custom_command(TARGET PdfWorkstation POST_BUILD
        COMMAND "${STRIP_PROGRAM}" "$<TARGET_FILE:PdfWorkstation>"
        COMMENT "AR-11 D4: Stripping DWARF paths from Release PdfWorkstation.exe"
        VERBATIM
    )
endif()

# ── Precompiled Headers ───────────────────────────────────────────────────
target_precompile_headers(pdfws_engines PRIVATE
    <QString>
    <QList>
    <QObject>
    <QRectF>
    <QImage>
)

target_precompile_headers(pdfws_ui PRIVATE
    <QWidget>
    <QAction>
    <QToolBar>
    <QMenu>
    <QLabel>
    <QVBoxLayout>
    <QFileDialog>
    <QMessageBox>
)

# ── Test targets ──────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/UnitTests.cpp")
    add_executable(UnitTests
        tests/UnitTests.cpp
    )
    target_include_directories(UnitTests PRIVATE src)
    target_link_libraries(UnitTests PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets
    )
    add_test(NAME UnitTests COMMAND UnitTests)
    set_tests_properties(UnitTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPdfEditorInterface.cpp")
    add_executable(TestInterfaces
        tests/TestPdfEditorInterface.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestInterfaces PRIVATE src)
    target_link_libraries(TestInterfaces PRIVATE
        pdfws_core pdfws_commands
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestInterfaces COMMAND TestInterfaces)
    set_tests_properties(TestInterfaces PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;interfaces;commands;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestExpiryInterface.cpp")
    add_executable(TestExpiryInterface tests/TestExpiryInterface.cpp)
    target_include_directories(TestExpiryInterface PRIVATE src tests)
    target_link_libraries(TestExpiryInterface PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestExpiryInterface POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestExpiryInterface>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestExpiryInterface"
    )
    endif()
    add_test(NAME TestExpiryInterface COMMAND TestExpiryInterface)
    set_tests_properties(TestExpiryInterface PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;interfaces;security;expiry;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_test.cpp")
    add_executable(SmokeTest
        tests/smoke_test.cpp
    )
    target_include_directories(SmokeTest PRIVATE src)
    target_link_libraries(SmokeTest PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME SmokeTest COMMAND SmokeTest)
    set_tests_properties(SmokeTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "smoke;integration;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSanitization.cpp")
    add_executable(TestSanitization
        tests/TestSanitization.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestSanitization PRIVATE src tests)
    target_link_libraries(TestSanitization PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSanitization COMMAND TestSanitization)
    set_tests_properties(TestSanitization PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;sanitization;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSanitizeTrailerUaf.cpp")
    add_executable(TestSanitizeTrailerUaf
        tests/TestSanitizeTrailerUaf.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestSanitizeTrailerUaf PRIVATE src tests)
    target_link_libraries(TestSanitizeTrailerUaf PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSanitizeTrailerUaf COMMAND TestSanitizeTrailerUaf)
    set_tests_properties(TestSanitizeTrailerUaf PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;sanitization;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAutosave.cpp")
    add_executable(TestAutosave
        tests/TestAutosave.cpp
    )
    target_include_directories(TestAutosave PRIVATE src tests)
    target_link_libraries(TestAutosave PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestAutosave COMMAND TestAutosave)
    set_tests_properties(TestAutosave PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;autosave;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDocumentIdentity.cpp")
    # ARC05/ARC01/ARC02 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): real-MainWindow
    # integration tests over the document-identity boundary. Compiles the real
    # Bootstrapper (see the DJOT_LIB_DIR note at the PdfWorkstation target — a
    # test target including Bootstrapper.cpp must define DJOT_LIB_DIR too).
    add_executable(TestDocumentIdentity
        tests/TestDocumentIdentity.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestDocumentIdentity PRIVATE src tests)
    target_compile_definitions(TestDocumentIdentity PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestDocumentIdentity PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestDocumentIdentity POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestDocumentIdentity>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestDocumentIdentity>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestDocumentIdentity"
    )
    add_test(NAME TestDocumentIdentity COMMAND TestDocumentIdentity)
    set_tests_properties(TestDocumentIdentity PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;session;identity;integration;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatesBatchSafety.cpp")
    # G03 (QUALITY-GATE-2026-09-09): real-MainWindow regression tests for the
    # Bates batch input/output collision boundary (same shape as the
    # TestDocumentIdentity target: real Bootstrapper context, offscreen).
    add_executable(TestBatesBatchSafety
        tests/TestBatesBatchSafety.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestBatesBatchSafety PRIVATE src tests)
    target_compile_definitions(TestBatesBatchSafety PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestBatesBatchSafety PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatesBatchSafety POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatesBatchSafety>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatesBatchSafety>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatesBatchSafety"
    )
    add_test(NAME TestBatesBatchSafety COMMAND TestBatesBatchSafety)
    set_tests_properties(TestBatesBatchSafety PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;pages;bates;integration;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRecoverySave.cpp")
    # G05 (QUALITY-GATE-2026-09-09): real-MainWindow regression tests for the
    # recovered-document save destination and the close-after-Save contract
    # (same shape as the TestDocumentIdentity target).
    add_executable(TestRecoverySave
        tests/TestRecoverySave.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestRecoverySave PRIVATE src tests)
    target_compile_definitions(TestRecoverySave PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestRecoverySave PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestRecoverySave POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRecoverySave>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRecoverySave>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestRecoverySave"
    )
    add_test(NAME TestRecoverySave COMMAND TestRecoverySave)
    set_tests_properties(TestRecoverySave PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;recovery;session;integration;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestReadOnlyGate.cpp")
    # ARC07 (TEAM-ARCHITECTURE-REVIEW-2026-09-07, wave 4A): real-MainWindow
    # integration tests for the shared read-only mutation gate. Same shape as
    # the TestDocumentIdentity target (real Bootstrapper context, offscreen).
    add_executable(TestReadOnlyGate
        tests/TestReadOnlyGate.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestReadOnlyGate PRIVATE src tests)
    target_compile_definitions(TestReadOnlyGate PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestReadOnlyGate PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestReadOnlyGate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestReadOnlyGate>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestReadOnlyGate>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestReadOnlyGate"
    )
    add_test(NAME TestReadOnlyGate COMMAND TestReadOnlyGate)
    set_tests_properties(TestReadOnlyGate PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;shell;readonly;integration;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSidecarReopenState.cpp")
    # G14 (QUALITY-GATE-2026-09-09): sidecar-only annotation edits keep their
    # unsaved-PDF state across a reopen. Real MainWindow + real artifacts +
    # the modal-driving pattern (checked Save/Discard/Cancel transition), same
    # shape as the TestReadOnlyGate target.
    add_executable(TestSidecarReopenState
        tests/TestSidecarReopenState.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestSidecarReopenState PRIVATE src tests)
    target_compile_definitions(TestSidecarReopenState PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestSidecarReopenState PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSidecarReopenState POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSidecarReopenState>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSidecarReopenState>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSidecarReopenState"
    )
    add_test(NAME TestSidecarReopenState COMMAND TestSidecarReopenState)
    set_tests_properties(TestSidecarReopenState PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;shell;annotations;sidecar;integration;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureValidationMock.cpp")
    add_executable(TestSignatureValidationMock
        tests/TestSignatureValidationMock.cpp
    )
    target_include_directories(TestSignatureValidationMock PRIVATE src tests)
    target_link_libraries(TestSignatureValidationMock PRIVATE
        pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSignatureValidationMock COMMAND TestSignatureValidationMock)
    set_tests_properties(TestSignatureValidationMock PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;signatures;mock;interface;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRobustness.cpp")
    add_executable(TestRobustness
        tests/TestRobustness.cpp
    )
    target_include_directories(TestRobustness PRIVATE src tests)
    target_link_libraries(TestRobustness PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestRobustness COMMAND TestRobustness)
    set_tests_properties(TestRobustness PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;robustness;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSecretStore.cpp")
    add_executable(TestSecretStore
        tests/TestSecretStore.cpp
    )
    target_include_directories(TestSecretStore PRIVATE src tests)
    # SEP13:5: OpenSSL::Crypto crafts the legacy 0x01 (no-AAD) migration
    # fixture; Crypt32 crafts the legacy 0x02 DPAPI migration fixture.
    target_link_libraries(TestSecretStore PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui
    )
    if(WIN32)
        target_link_libraries(TestSecretStore PRIVATE Crypt32)
    endif()
    add_test(NAME TestSecretStore COMMAND TestSecretStore)
    set_tests_properties(TestSecretStore PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;secrets;privacy;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureValidation.cpp")
    add_executable(TestSignatureValidation
        tests/TestSignatureValidation.cpp
    )
    target_include_directories(TestSignatureValidation PRIVATE src tests)
    target_compile_definitions(TestSignatureValidation PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(TestSignatureValidation PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSignatureValidation PRIVATE Crypt32)
    endif()
    add_test(NAME TestSignatureValidation COMMAND TestSignatureValidation)
    set_tests_properties(TestSignatureValidation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;real-crypto;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureRealCrypto.cpp")
    add_executable(TestSignatureRealCrypto
        tests/TestSignatureRealCrypto.cpp
    )
    target_include_directories(TestSignatureRealCrypto PRIVATE src tests)
    # Inject the absolute source root so kFixtureDir in the test is not relative
    # (ctest CWD = build/; a relative path would not find tests/fixtures/).
    target_compile_definitions(TestSignatureRealCrypto PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestSignatureRealCrypto PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestSignatureRealCrypto PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSignatureRealCrypto PRIVATE Crypt32)
        # Deploy the vendored podofo 1.1.0 DLL so the runtime loader picks up the correct
        # version instead of the ucrt64 system 0.10.4. Same pattern as TestFormBuilder.
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSignatureRealCrypto POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSignatureRealCrypto>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSignatureRealCrypto"
            )
        endif()
    endif()
    add_test(NAME TestSignatureRealCrypto COMMAND TestSignatureRealCrypto)
    set_tests_properties(TestSignatureRealCrypto PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;crypto;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# R26 send-for-signing P1: signing-request sidecar + workflow. Real-crypto
# where the fixtures exist (same estate as TestSignatureRealCrypto: synthetic
# P12, offline B-B). Model/handshake/creator slots run without fixtures.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSendForSigning.cpp")
    add_executable(TestSendForSigning
        tests/TestSendForSigning.cpp
    )
    target_include_directories(TestSendForSigning PRIVATE src tests)
    target_compile_definitions(TestSendForSigning PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestSendForSigning PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestSendForSigning PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSendForSigning PRIVATE Crypt32)
        # Deploy the vendored podofo 1.1.0 DLL (same pattern as
        # TestSignatureRealCrypto).
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSendForSigning POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSendForSigning>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSendForSigning"
            )
        endif()
    endif()
    add_test(NAME TestSendForSigning COMMAND TestSendForSigning)
    set_tests_properties(TestSendForSigning PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        # FU-2 seam class (same as TestPrintableSummary/TestAccessibilityFixes):
        # the suite counts the SHARED %TEMP%/glyphpdf-candidates dir
        # (leftover-candidate leak pin), so a concurrent SafeSave suite would
        # perturb the count — run alone.
        RUN_SERIAL TRUE
        LABELS "integration;security;signatures;workflow;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# Audit 9.2 P0 (ER-2): the eraser must not bypass the signed-document guard.
# deleteObjectAt on a signed document must be refused (incremental save would
# leak excised bytes into revision 1). Uses the real signing fixtures.
# NOTE: a byte-identical duplicate of the TestSignatureAppearance block (re-added
# by cec39a7, "lost to a concurrent CMakeLists edit", while the canonical block
# further down survived) was removed here — the twin target broke configure with
# a CMP0002 duplicate-target error for every lane sharing this tree.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEraseSignedGuard.cpp")
    add_executable(TestEraseSignedGuard tests/TestEraseSignedGuard.cpp)
    target_include_directories(TestEraseSignedGuard PRIVATE src tests)
    target_compile_definitions(TestEraseSignedGuard PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestEraseSignedGuard PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestEraseSignedGuard PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestEraseSignedGuard PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestEraseSignedGuard POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestEraseSignedGuard>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestEraseSignedGuard"
            )
        endif()
    endif()
    add_test(NAME TestEraseSignedGuard COMMAND TestEraseSignedGuard)
    set_tests_properties(TestEraseSignedGuard PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;erase;qt;headless"
    )
endif()

# §9.7 P0: visible signature appearance for the cryptographic signing path.
# Pins the pure planning seam (auto-fit ladder, ETSI line set) and the
# end-to-end signing path: /AP /N form XObject written in the same incremental
# update as the digest, Reason/Location only when set, name-only in tiny rects,
# optional signature image embedded as an /Image XObject.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureAppearance.cpp")
    add_executable(TestSignatureAppearance tests/TestSignatureAppearance.cpp)
    target_include_directories(TestSignatureAppearance PRIVATE src tests)
    target_compile_definitions(TestSignatureAppearance PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestSignatureAppearance PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestSignatureAppearance PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSignatureAppearance PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSignatureAppearance POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSignatureAppearance>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSignatureAppearance"
            )
        endif()
    endif()
    add_test(NAME TestSignatureAppearance COMMAND TestSignatureAppearance)
    set_tests_properties(TestSignatureAppearance PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;appearance;qt;headless"
    )
endif()

# §9.13 F2 regression test: optimizeDocument (Compress / Strip metadata) must
# refuse signed documents — writeUpdate's incremental SaveUpdate would keep the
# "stripped" metadata recoverable in revision 1 while the dialog reports success.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOptimizeSignedGuard.cpp")
    add_executable(TestOptimizeSignedGuard tests/TestOptimizeSignedGuard.cpp)
    target_include_directories(TestOptimizeSignedGuard PRIVATE src tests)
    target_compile_definitions(TestOptimizeSignedGuard PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestOptimizeSignedGuard PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestOptimizeSignedGuard PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestOptimizeSignedGuard PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestOptimizeSignedGuard POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestOptimizeSignedGuard>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestOptimizeSignedGuard"
            )
        endif()
    endif()
    add_test(NAME TestOptimizeSignedGuard COMMAND TestOptimizeSignedGuard)
    set_tests_properties(TestOptimizeSignedGuard PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;security;signatures;compression;qt;headless"
    )
endif()

# ── TestImageDedup (gateD cleanup-lane finding: dedup last-mile) ────────────
# optimizeDocument's duplicate-image dedup must survive the SAVE: after
# optimizeDocument + save + reload, both page /XObject entries must still be
# indirect references pointing at the SAME canonical image object.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestImageDedup.cpp")
    add_executable(TestImageDedup tests/TestImageDedup.cpp)
    target_include_directories(TestImageDedup PRIVATE src tests)
    target_link_libraries(TestImageDedup PRIVATE
        pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestImageDedup PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestImageDedup POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestImageDedup>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestImageDedup"
            )
        endif()
    endif()
    add_test(NAME TestImageDedup COMMAND TestImageDedup)
    set_tests_properties(TestImageDedup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;compression;optimize;qt;headless"
    )
endif()

# R2-1 CHAIN-1 closure tests (D3)
# Tests only ProvenanceGuard logic — no pdfws_engines, no OpenSSL, no podofo.
# This keeps TestChain1 runnable in all environments without DLL-path issues.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestChain1.cpp")
    add_executable(TestChain1
        tests/TestChain1.cpp
    )
    target_include_directories(TestChain1 PRIVATE src tests)
    target_link_libraries(TestChain1 PRIVATE
        pdfws_djot pdfws_core
        Qt6::Test Qt6::Core
    )
    add_test(NAME TestChain1 COMMAND TestChain1)
    set_tests_properties(TestChain1 PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;chain1;provenance;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedaction.cpp")
    add_executable(TestRedaction
        tests/TestRedaction.cpp
    )
    target_include_directories(TestRedaction PRIVATE src tests)
    target_link_libraries(TestRedaction PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestRedaction COMMAND TestRedaction)
    set_tests_properties(TestRedaction PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;security;redaction;regression;qt;headless"
    )
endif()

# Audit 9.13 P0 regression test (DEFECT 3, commit 6c41e12): the duplicate-image
# dedup hash folds in image-dict params. Two images with identical stream bytes
# but different /SMask must NOT be merged.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDedupSMask.cpp")
    add_executable(TestDedupSMask tests/TestDedupSMask.cpp)
    target_include_directories(TestDedupSMask PRIVATE src tests)
    target_link_libraries(TestDedupSMask PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestDedupSMask POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestDedupSMask>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestDedupSMask"
            )
        endif()
    endif()
    add_test(NAME TestDedupSMask COMMAND TestDedupSMask)
    set_tests_properties(TestDedupSMask PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;compression;dedup;regression;qt;headless"
    )
endif()

# Audit 9.13 P0 regression test: Compress Quality/DPI must do real work —
# DCTDecode images decoded, downsampled and JPEG re-encoded honoring
# jpegQuality; below-threshold/SMask/malformed images left untouched.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompressJpegReencode.cpp")
    add_executable(TestCompressJpegReencode tests/TestCompressJpegReencode.cpp)
    target_include_directories(TestCompressJpegReencode PRIVATE src tests)
    target_link_libraries(TestCompressJpegReencode PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestCompressJpegReencode POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestCompressJpegReencode>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestCompressJpegReencode"
            )
        endif()
    endif()
    add_test(NAME TestCompressJpegReencode COMMAND TestCompressJpegReencode)
    set_tests_properties(TestCompressJpegReencode PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;compression;jpeg;regression;qt;headless"
    )
endif()

# Audit 9.13 P0 regression test: the size-estimate heuristics must not claim
# savings for passes that don't run in the write path (subsetFonts /
# removeUnusedObjects are not implemented in optimizeDocument).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOptimizeEstimate.cpp")
    add_executable(TestOptimizeEstimate tests/TestOptimizeEstimate.cpp)
    target_include_directories(TestOptimizeEstimate PRIVATE src tests)
    target_link_libraries(TestOptimizeEstimate PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestOptimizeEstimate POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestOptimizeEstimate>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestOptimizeEstimate"
            )
        endif()
    endif()
    add_test(NAME TestOptimizeEstimate COMMAND TestOptimizeEstimate)
    set_tests_properties(TestOptimizeEstimate PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;compression;estimate;regression;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRapidOcr.cpp" AND (onnxruntime_FOUND OR TARGET onnxruntime OR PKG_ONNXRUNTIME_FOUND))
    add_executable(TestRapidOcr
        tests/TestRapidOcr.cpp
    )
    target_include_directories(TestRapidOcr PRIVATE src tests)
    target_link_libraries(TestRapidOcr PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    target_compile_definitions(TestRapidOcr PRIVATE
        GLYPH_OCR_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models/ppocrv5")
    add_custom_command(TARGET TestRapidOcr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TestRapidOcr>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRapidOcr>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestRapidOcr"
    )
    add_test(NAME TestRapidOcr COMMAND TestRapidOcr)
    set_tests_properties(TestRapidOcr PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "ocr;ml;rapidocr;qt;headless"
    )
endif()

# ── TestLayoutEnsemble (M5-P2 D7) ────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLayoutEnsemble.cpp")
    add_executable(TestLayoutEnsemble
        tests/TestLayoutEnsemble.cpp
    )
    target_include_directories(TestLayoutEnsemble PRIVATE src tests)
    target_link_libraries(TestLayoutEnsemble PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestLayoutEnsemble COMMAND TestLayoutEnsemble)
    set_tests_properties(TestLayoutEnsemble PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "ocr;layout;ensemble;iou;qt;headless"
    )
endif()

# ── TestOcrPipeline (M5-P2 D7) ───────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrPipeline.cpp")
    add_executable(TestOcrPipeline
        tests/TestOcrPipeline.cpp
    )
    target_include_directories(TestOcrPipeline PRIVATE src tests)
    target_link_libraries(TestOcrPipeline PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestOcrPipeline COMMAND TestOcrPipeline)
    set_tests_properties(TestOcrPipeline PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "ocr;pipeline;rover;timing;cross-page;qt;headless"
    )
endif()

# ── TestOcrReviewLifecycle (R07/R08 — OCR lifecycle + reviewed-word authority) ──
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrReviewLifecycle.cpp")
    add_executable(TestOcrReviewLifecycle
        tests/TestOcrReviewLifecycle.cpp
    )
    target_include_directories(TestOcrReviewLifecycle PRIVATE src tests)
    target_link_libraries(TestOcrReviewLifecycle PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestOcrReviewLifecycle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestOcrReviewLifecycle>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestOcrReviewLifecycle>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestOcrReviewLifecycle"
    )
    add_test(NAME TestOcrReviewLifecycle COMMAND TestOcrReviewLifecycle)
    set_tests_properties(TestOcrReviewLifecycle PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;ocr;lifecycle;review;qt;headless"
    )
endif()

# ── TestOcrVerifyNavigation (U03 — source-image verification workflow) ───────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrVerifyNavigation.cpp")
    add_executable(TestOcrVerifyNavigation
        tests/TestOcrVerifyNavigation.cpp
    )
    target_include_directories(TestOcrVerifyNavigation PRIVATE src tests)
    target_link_libraries(TestOcrVerifyNavigation PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestOcrVerifyNavigation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestOcrVerifyNavigation>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestOcrVerifyNavigation>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestOcrVerifyNavigation"
    )
    add_test(NAME TestOcrVerifyNavigation COMMAND TestOcrVerifyNavigation)
    set_tests_properties(TestOcrVerifyNavigation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;ocr;review;navigation;u03;qt;headless"
    )
endif()

# ── TestOcrPreprocessor (audit §9.4 P0) ──────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrPreprocessor.cpp")
    add_executable(TestOcrPreprocessor
        tests/TestOcrPreprocessor.cpp
    )
    target_include_directories(TestOcrPreprocessor PRIVATE src tests)
    target_link_libraries(TestOcrPreprocessor PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestOcrPreprocessor COMMAND TestOcrPreprocessor)
    set_tests_properties(TestOcrPreprocessor PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "ocr;preprocess;orientation;qt;headless"
    )
endif()

# ── TestOcrDjotMapper (M5-P4 D4) ─────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrDjotMapper.cpp")
    add_executable(TestOcrDjotMapper
        tests/TestOcrDjotMapper.cpp
    )
    target_include_directories(TestOcrDjotMapper PRIVATE src tests)
    target_link_libraries(TestOcrDjotMapper PRIVATE
        pdfws_engines pdfws_core pdfws_djot docmodel
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Concurrent
    )
    add_test(NAME TestOcrDjotMapper COMMAND TestOcrDjotMapper)
    set_tests_properties(TestOcrDjotMapper PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "ocr;mapper;djot;structural;roundtrip;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestThreadSafety.cpp")
    add_executable(TestThreadSafety
        tests/TestThreadSafety.cpp
    )
    target_include_directories(TestThreadSafety PRIVATE src tests)
    target_link_libraries(TestThreadSafety PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Concurrent podofo::podofo
    )
    add_test(NAME TestThreadSafety COMMAND TestThreadSafety)
    set_tests_properties(TestThreadSafety PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;threads;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEncryption.cpp")
    add_executable(TestEncryption
        tests/TestEncryption.cpp
    )
    target_include_directories(TestEncryption PRIVATE src tests)
    # ZLIB is needed by the A-01/G-03 PubSec round-trip test to FlateDecode the
    # AES-decrypted content stream and assert the plaintext marker is recoverable.
    find_package(ZLIB REQUIRED)
    target_link_libraries(TestEncryption PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo ZLIB::ZLIB
    )
    add_test(NAME TestEncryption COMMAND TestEncryption)
    set_tests_properties(TestEncryption PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;encryption;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestResourceLimits.cpp")
    add_executable(TestResourceLimits
        tests/TestResourceLimits.cpp
    )
    target_include_directories(TestResourceLimits PRIVATE src tests)
    target_link_libraries(TestResourceLimits PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestResourceLimits COMMAND TestResourceLimits)
    set_tests_properties(TestResourceLimits PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;resources;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestControllers.cpp")
    add_executable(TestControllers
        tests/TestControllers.cpp
    )
    target_include_directories(TestControllers PRIVATE src)
    target_link_libraries(TestControllers PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    # TestControllers uses QTEST_MAIN (QApplication), so it needs the offscreen
    # platform plugin at runtime. Deploy it next to the test executable.
    add_custom_command(TARGET TestControllers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestControllers>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestControllers>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestControllers"
    )
    add_test(NAME TestControllers COMMAND TestControllers)
    set_tests_properties(TestControllers PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;controllers;ui;qt;headless"
    )
endif()

# ── TestRibbonIntegrity (R2-4 D5) ────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRibbonIntegrity.cpp")
    add_executable(TestRibbonIntegrity
        tests/TestRibbonIntegrity.cpp
    )
    target_include_directories(TestRibbonIntegrity PRIVATE src)
    target_link_libraries(TestRibbonIntegrity PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestRibbonIntegrity POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRibbonIntegrity>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRibbonIntegrity>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestRibbonIntegrity"
    )
    add_test(NAME TestRibbonIntegrity COMMAND TestRibbonIntegrity)
    set_tests_properties(TestRibbonIntegrity PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;ribbon;integrity;ui;qt;headless"
    )
endif()

# ── TestMenuBarIntegrity (UI-wiring hardening 2026-06-22) ────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMenuBarIntegrity.cpp")
    add_executable(TestMenuBarIntegrity
        tests/TestMenuBarIntegrity.cpp
    )
    target_include_directories(TestMenuBarIntegrity PRIVATE src)
    target_link_libraries(TestMenuBarIntegrity PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestMenuBarIntegrity POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestMenuBarIntegrity>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestMenuBarIntegrity>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestMenuBarIntegrity"
    )
    add_test(NAME TestMenuBarIntegrity COMMAND TestMenuBarIntegrity)
    set_tests_properties(TestMenuBarIntegrity PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;menubar;integrity;ui;qt;headless"
    )
endif()

# ── TestCommandBinding (R15 2026-09-13: canonical command binding) ──────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCommandBinding.cpp")
    add_executable(TestCommandBinding
        tests/TestCommandBinding.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestCommandBinding PRIVATE src tests)
    target_compile_definitions(TestCommandBinding PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestCommandBinding PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestCommandBinding POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCommandBinding>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCommandBinding>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCommandBinding"
    )
    add_test(NAME TestCommandBinding COMMAND TestCommandBinding)
    set_tests_properties(TestCommandBinding PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        LABELS "unit;ui;ribbon;menubar;binding;r15;qt;headless"
    )
endif()

# ── TestWelcomeRoutes (R16 2026-09-13: task-oriented welcome) ───────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWelcomeRoutes.cpp")
    add_executable(TestWelcomeRoutes
        tests/TestWelcomeRoutes.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestWelcomeRoutes PRIVATE src tests)
    target_compile_definitions(TestWelcomeRoutes PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestWelcomeRoutes PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestWelcomeRoutes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestWelcomeRoutes>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestWelcomeRoutes>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestWelcomeRoutes"
    )
    add_test(NAME TestWelcomeRoutes COMMAND TestWelcomeRoutes)
    set_tests_properties(TestWelcomeRoutes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;ui;welcome;routes;r16;qt;headless"
    )
endif()

# ── TestSweepW3UxFlows (SWEEP-W3 UX audit 2026-09-20: real-flow completion + honesty) ──
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSweepW3UxFlows.cpp")
    add_executable(TestSweepW3UxFlows
        tests/TestSweepW3UxFlows.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestSweepW3UxFlows PRIVATE src tests)
    target_compile_definitions(TestSweepW3UxFlows PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
        SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(TestSweepW3UxFlows PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSweepW3UxFlows POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSweepW3UxFlows>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSweepW3UxFlows>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSweepW3UxFlows"
    )
    add_test(NAME TestSweepW3UxFlows COMMAND TestSweepW3UxFlows)
    set_tests_properties(TestSweepW3UxFlows PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 900
        LABELS "unit;ui;ux-audit;sweep-w3;qt;headless"
    )
endif()

# ── TestUiAccessibility (R17 2026-09-13: keyboard/RTL/CJK/DPI pass) ─────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestUiAccessibility.cpp")
    add_executable(TestUiAccessibility
        tests/TestUiAccessibility.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestUiAccessibility PRIVATE src tests)
    target_compile_definitions(TestUiAccessibility PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestUiAccessibility PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestUiAccessibility POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestUiAccessibility>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestUiAccessibility>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestUiAccessibility"
    )
    add_test(NAME TestUiAccessibility COMMAND TestUiAccessibility)
    set_tests_properties(TestUiAccessibility PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        LABELS "unit;ui;accessibility;keyboard;rtl;r17;qt;headless"
    )
    # R17: the 200%-scale entry runs the SAME binary with the scale factor
    # applied — every accessibility pin must hold at high DPI too, and
    # highDpiScaleFactorIsAppliedAndLayoutStable asserts the factor itself.
    add_test(NAME TestUiAccessibility200 COMMAND TestUiAccessibility)
    set_tests_properties(TestUiAccessibility200 PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2"
        TIMEOUT 240
        LABELS "unit;ui;accessibility;dpi;r17;qt;headless"
    )
endif()

# ── Integration tests (Session 20) ───────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestIntegration.cpp")
    add_executable(TestIntegration
        tests/TestIntegration.cpp
    )
    target_include_directories(TestIntegration PRIVATE src tests)
    target_link_libraries(TestIntegration PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestIntegration COMMAND TestIntegration)
    set_tests_properties(TestIntegration PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "integration;e2e;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPerformance.cpp")
    add_executable(TestPerformance
        tests/TestPerformance.cpp
    )
    target_include_directories(TestPerformance PRIVATE src tests)
    target_link_libraries(TestPerformance PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestPerformance COMMAND TestPerformance)
    set_tests_properties(TestPerformance PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "performance;benchmark;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestVeraPdf.cpp")
    add_executable(TestVeraPdf tests/TestVeraPdf.cpp)
    target_include_directories(TestVeraPdf PRIVATE src tests)
    target_link_libraries(TestVeraPdf PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
    )
    add_test(NAME TestVeraPdf COMMAND TestVeraPdf)
    set_tests_properties(TestVeraPdf PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;integration;pdfa;verapdf;subprocess"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestReadingOrderAsync.cpp")
    add_executable(TestReadingOrderAsync tests/TestReadingOrderAsync.cpp)
    target_include_directories(TestReadingOrderAsync PRIVATE src tests)
    target_link_libraries(TestReadingOrderAsync PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_test(NAME TestReadingOrderAsync COMMAND TestReadingOrderAsync)
    set_tests_properties(TestReadingOrderAsync PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;pdfa;accessibility;reading-order;async;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAccessibilityChecker.cpp")
    add_executable(TestAccessibilityChecker tests/TestAccessibilityChecker.cpp)
    target_include_directories(TestAccessibilityChecker PRIVATE src tests)
    target_link_libraries(TestAccessibilityChecker PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core podofo::podofo
    )
    add_test(NAME TestAccessibilityChecker COMMAND TestAccessibilityChecker)
    set_tests_properties(TestAccessibilityChecker PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;accessibility;a11y-checker;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAccessibilityPanel.cpp")
    add_executable(TestAccessibilityPanel tests/TestAccessibilityPanel.cpp)
    target_include_directories(TestAccessibilityPanel PRIVATE src tests)
    target_link_libraries(TestAccessibilityPanel PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_test(NAME TestAccessibilityPanel COMMAND TestAccessibilityPanel)
    set_tests_properties(TestAccessibilityPanel PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;accessibility;a11y-panel;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAccessibilityFixes.cpp")
    add_executable(TestAccessibilityFixes tests/TestAccessibilityFixes.cpp)
    target_include_directories(TestAccessibilityFixes PRIVATE src tests)
    target_link_libraries(TestAccessibilityFixes PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestAccessibilityFixes COMMAND TestAccessibilityFixes)
    set_tests_properties(TestAccessibilityFixes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        # Same seam class as TestPrintableSummary: process-global commit
        # fault + shared SafeSave candidate temp dir — run alone.
        RUN_SERIAL TRUE
        LABELS "unit;accessibility;a11y-fixes;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAccessibilityTagger.cpp")
    add_executable(TestAccessibilityTagger tests/TestAccessibilityTagger.cpp)
    target_include_directories(TestAccessibilityTagger PRIVATE src tests)
    target_link_libraries(TestAccessibilityTagger PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestAccessibilityTagger COMMAND TestAccessibilityTagger)
    set_tests_properties(TestAccessibilityTagger PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        # Same seam class as TestAccessibilityFixes: the commit-fault seam is
        # process-global and SafeSave candidates share one temp dir — run
        # alone. The tagging transaction itself is multi-second for larger
        # fixtures (rewrite + independent reopen per page).
        RUN_SERIAL TRUE
        LABELS "unit;accessibility;a11y-tagger;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLaneScheduler.cpp")
    add_executable(TestLaneScheduler tests/TestLaneScheduler.cpp)
    target_include_directories(TestLaneScheduler PRIVATE src tests)
    target_link_libraries(TestLaneScheduler PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Concurrent podofo::podofo
    )
    add_test(NAME TestLaneScheduler COMMAND TestLaneScheduler)
    set_tests_properties(TestLaneScheduler PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        # Thread-scheduling assertions starve under parallel ctest on 2-core
        # CI runners (timeout with -j4); run alone.
        RUN_SERIAL TRUE
        LABELS "unit;concurrency;scheduling;lane-scheduler;qt"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormBuilder.cpp")
    add_executable(TestFormBuilder
        tests/TestFormBuilder.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFormBuilder PRIVATE src tests)
    target_link_libraries(TestFormBuilder PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormBuilder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormBuilder>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormBuilder>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormBuilder"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFormBuilder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormBuilder>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormBuilder"
        )
    endif()
    add_test(NAME TestFormBuilder COMMAND TestFormBuilder)
    set_tests_properties(TestFormBuilder PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;forms;form-fields;builder;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormPersistence.cpp")
    add_executable(TestFormPersistence
        tests/TestFormPersistence.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFormPersistence PRIVATE src tests)
    target_link_libraries(TestFormPersistence PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormPersistence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormPersistence>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormPersistence>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormPersistence"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFormPersistence POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormPersistence>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormPersistence"
        )
    endif()
    add_test(NAME TestFormPersistence COMMAND TestFormPersistence)
    set_tests_properties(TestFormPersistence PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;forms;form-fields;persistence;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompareEntry.cpp")
    add_executable(TestCompareEntry
        tests/TestCompareEntry.cpp
    )
    target_include_directories(TestCompareEntry PRIVATE src tests)
    target_link_libraries(TestCompareEntry PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestCompareEntry POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCompareEntry>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCompareEntry>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCompareEntry"
    )
    add_test(NAME TestCompareEntry COMMAND TestCompareEntry)
    set_tests_properties(TestCompareEntry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;modes;compare;ui;qt;headless"
    )
endif()

# §9.10 P1: end-to-end compare INTEGRATION tests — real DiffEngine on
# hand-built N-page PDFs feeding the real CompareWidget + CompareMode
# compareFiles() watcher path: identical docs, token edit + anchor, trailing
# page structural navigation with both viewers, export reports, filter-aware
# export scope.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompareIntegration.cpp")
    add_executable(TestCompareIntegration
        tests/TestCompareIntegration.cpp
    )
    target_include_directories(TestCompareIntegration PRIVATE src tests)
    target_link_libraries(TestCompareIntegration PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestCompareIntegration POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCompareIntegration>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCompareIntegration>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCompareIntegration"
    )
    add_test(NAME TestCompareIntegration COMMAND TestCompareIntegration)
    set_tests_properties(TestCompareIntegration PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;modes;compare;integration;ui;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPagesMode.cpp")
    add_executable(TestPagesMode
        tests/TestPagesMode.cpp
    )
    target_include_directories(TestPagesMode PRIVATE src tests)
    target_link_libraries(TestPagesMode PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestPagesMode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestPagesMode>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestPagesMode>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestPagesMode"
    )
    add_test(NAME TestPagesMode COMMAND TestPagesMode)
    set_tests_properties(TestPagesMode PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;modes;pages;ui;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# §9.9 P1: pure /PageLabels seam (number-tree entries + label strings) PLUS
# the catalog writer (writeNumberTree — PoDoFo /Nums serialization). Compiles
# its own copy of the seam (TestUpdateChecker precedent); the writer needs the
# podofo 1.1.0 import lib + DLL at runtime (TestCompressJpegReencode
# precedent). No GUI; offscreen plugin only.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPageLabels.cpp")
    add_executable(TestPageLabels
        tests/TestPageLabels.cpp
        src/core/PageLabels.h  src/core/PageLabels.cpp
        src/engines/SafeSave.h src/engines/SafeSave.cpp   # G13: candidate+commit transaction
    )
    target_include_directories(TestPageLabels PRIVATE src tests)
    target_link_libraries(TestPageLabels PRIVATE
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Pdf podofo::podofo
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestPageLabels POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestPageLabels>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestPageLabels"
            )
        endif()
    endif()
    add_test(NAME TestPageLabels COMMAND TestPageLabels)
    set_tests_properties(TestPageLabels PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;pages;pagelabels;core;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchMode.cpp")
    add_executable(TestBatchMode
        tests/TestBatchMode.cpp
    )
    target_include_directories(TestBatchMode PRIVATE src tests)
    target_link_libraries(TestBatchMode PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    # Deploy qoffscreen platform plugin next to the test executable
    add_custom_command(TARGET TestBatchMode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchMode>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchMode>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchMode"
    )
    add_test(NAME TestBatchMode COMMAND TestBatchMode)
    set_tests_properties(TestBatchMode PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;modes;batch;ui;qt;headless"
        RESOURCE_LOCK BatchModeIO
        # Timing-sensitive batch progress assertions flake under parallel
        # ctest (-j4); run alone.
        RUN_SERIAL TRUE
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchPresets.cpp")
    add_executable(TestBatchPresets
        tests/TestBatchPresets.cpp
    )
    target_include_directories(TestBatchPresets PRIVATE src tests)
    target_link_libraries(TestBatchPresets PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatchPresets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchPresets>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchPresets>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchPresets"
    )
    # R26 (batch-presets): the run fixtures are written with PoDoFo directly
    # (TestBatchOpsCoverage idiom), so the vendored DLL is staged on Windows;
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestBatchPresets POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestBatchPresets>"
            COMMENT "Deploying podofo 1.1.0 DLL for TestBatchPresets"
        )
    endif()
    add_test(NAME TestBatchPresets COMMAND TestBatchPresets)
    set_tests_properties(TestBatchPresets PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;modes;batch;presets;ui;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchOcrLanguage.cpp")
    add_executable(TestBatchOcrLanguage
        tests/TestBatchOcrLanguage.cpp
    )
    target_include_directories(TestBatchOcrLanguage PRIVATE src tests)
    target_link_libraries(TestBatchOcrLanguage PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatchOcrLanguage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchOcrLanguage>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchOcrLanguage>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOcrLanguage"
    )
    add_test(NAME TestBatchOcrLanguage COMMAND TestBatchOcrLanguage)
    set_tests_properties(TestBatchOcrLanguage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;batch;ocr;language;qt;headless"
    )
endif()

# ── TestBatchOcrSkipText (N3 — pdf24 skip-already-text batch OCR) ─────────────
# Mixed text/image-only corpora through the REAL batch OCR worker: skip-files
# produces outputs only for image-only files; skip-pages keeps text pages
# original; force-OCR overrides; idempotent second run. Truthful skip bucket
# in the batch summary.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchOcrSkipText.cpp")
    add_executable(TestBatchOcrSkipText
        tests/TestBatchOcrSkipText.cpp
    )
    target_include_directories(TestBatchOcrSkipText PRIVATE src tests)
    target_link_libraries(TestBatchOcrSkipText PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatchOcrSkipText POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchOcrSkipText>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchOcrSkipText>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOcrSkipText"
    )
    add_test(NAME TestBatchOcrSkipText COMMAND TestBatchOcrSkipText)
    set_tests_properties(TestBatchOcrSkipText PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;batch;ocr;skip-text;qt;headless"
    )
endif()

# §9.4 regression test: the OCR preprocessing checkboxes (Deskew/Binarize/
# Denoise) are persisted prefs honored by the pipeline — previously dead UI.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrPreprocessPrefs.cpp")
    add_executable(TestOcrPreprocessPrefs
        tests/TestOcrPreprocessPrefs.cpp
    )
    target_include_directories(TestOcrPreprocessPrefs PRIVATE src tests)
    target_link_libraries(TestOcrPreprocessPrefs PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestOcrPreprocessPrefs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestOcrPreprocessPrefs>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestOcrPreprocessPrefs>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestOcrPreprocessPrefs"
    )
    add_test(NAME TestOcrPreprocessPrefs COMMAND TestOcrPreprocessPrefs)
    set_tests_properties(TestOcrPreprocessPrefs PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;ocr;preprocess;persistence;qt;headless"
    )
endif()

# §9.9 P1 — Bates numbering continuity across documents: pins the three-arg
# applyBatesNumbering overload that reports the last used counter (so a batch
# caller stamps doc N+1 at doc N's last number + 1), with PDFium-verified
# cross-document text ("ABC-1/ABC-2" then "ABC-3/ABC-4").
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatesCrossDoc.cpp")
    add_executable(TestBatesCrossDoc
        tests/TestBatesCrossDoc.cpp
    )
    target_include_directories(TestBatesCrossDoc PRIVATE src tests)
    target_link_libraries(TestBatesCrossDoc PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestBatesCrossDoc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatesCrossDoc>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatesCrossDoc>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatesCrossDoc"
    )
    add_test(NAME TestBatesCrossDoc COMMAND TestBatesCrossDoc)
    set_tests_properties(TestBatesCrossDoc PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;pages;bates;batch;qt;headless"
    )
endif()

# §9.16 P1 — unified open/drag-drop routing: pins the pure routing seam on
# MainWindow (routeForFile extension matrix + planDrop drop policy) that lets
# File>Open and drag-and-drop accept Office/image files and route them to the
# existing LibreOffice / images-to-PDF conversions.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOpenRouting.cpp")
    add_executable(TestOpenRouting
        tests/TestOpenRouting.cpp
    )
    target_include_directories(TestOpenRouting PRIVATE src tests)
    target_link_libraries(TestOpenRouting PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestOpenRouting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestOpenRouting>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestOpenRouting>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestOpenRouting"
    )
    add_test(NAME TestOpenRouting COMMAND TestOpenRouting)
    set_tests_properties(TestOpenRouting PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;import;open;routing;qt;headless"
    )
endif()

# E-1 regression: redacting one line must never corrupt a byte-adjacent
# same-stream Tj (evidence ledger 2026-09-05, "Newly discovered engine defect").
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestExcisionCorruption.cpp")
    add_executable(TestExcisionCorruption
        tests/TestExcisionCorruption.cpp
    )
    target_include_directories(TestExcisionCorruption PRIVATE src tests)
    target_link_libraries(TestExcisionCorruption PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestExcisionCorruption POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestExcisionCorruption>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestExcisionCorruption>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestExcisionCorruption"
    )
    add_test(NAME TestExcisionCorruption COMMAND TestExcisionCorruption)
    set_tests_properties(TestExcisionCorruption PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;redaction;excision;corruption;qt;headless"
    )
endif()

# §9.12 P1: gap-only automated coverage for the batch operations —
# watermark stamps every page, Export-PDF/A carries structural identification,
# merge preserves page count + list order, and a preset checkbox alone excises
# its matches (plus one expected-fail characterization finding for the 2U/3U
# conformance fall-through).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchOpsCoverage.cpp")
    add_executable(TestBatchOpsCoverage
        tests/TestBatchOpsCoverage.cpp
    )
    target_include_directories(TestBatchOpsCoverage PRIVATE src tests)
    target_link_libraries(TestBatchOpsCoverage PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatchOpsCoverage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchOpsCoverage>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchOpsCoverage>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOpsCoverage"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestBatchOpsCoverage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestBatchOpsCoverage>"
            COMMENT "Deploying podofo 1.1.0 DLL for TestBatchOpsCoverage"
        )
    endif()
    add_test(NAME TestBatchOpsCoverage COMMAND TestBatchOpsCoverage)
    set_tests_properties(TestBatchOpsCoverage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;batch;watermark;pdfa;merge;redact;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPdfACidSetSafety.cpp")
    # SEP13:1 — CID /W out-of-bounds heap write + DoS in exportPdfA's
    # ensureCompleteCidSets: crafted hostile-/W fixtures through the real
    # engine export, CIDSet read back from the saved artifacts.
    add_executable(TestPdfACidSetSafety
        tests/TestPdfACidSetSafety.cpp
    )
    target_include_directories(TestPdfACidSetSafety PRIVATE src tests)
    target_link_libraries(TestPdfACidSetSafety PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        add_custom_command(TARGET TestPdfACidSetSafety POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestPdfACidSetSafety>"
            COMMENT "Deploying podofo 1.1.0 DLL for TestPdfACidSetSafety"
        )
    endif()
    add_test(NAME TestPdfACidSetSafety COMMAND TestPdfACidSetSafety)
    set_tests_properties(TestPdfACidSetSafety PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        LABELS "unit;pdfa;security;qt;headless"
    )
endif()

# §9.14 P1: the reading-order heuristic threshold is a NAMED, documented
# constant (kReadingOrderSlotTolerance) with boundary tests — displacement of
# exactly 2 slots is not flagged, exactly 3 is.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestReadingOrderThreshold.cpp")
    add_executable(TestReadingOrderThreshold
        tests/TestReadingOrderThreshold.cpp
    )
    target_include_directories(TestReadingOrderThreshold PRIVATE src tests)
    target_link_libraries(TestReadingOrderThreshold PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestReadingOrderThreshold POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestReadingOrderThreshold>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestReadingOrderThreshold>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestReadingOrderThreshold"
    )
    add_test(NAME TestReadingOrderThreshold COMMAND TestReadingOrderThreshold)
    set_tests_properties(TestReadingOrderThreshold PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;accessibility;pdfa;reading-order;qt;headless"
    )
endif()

# §9.16 P1: round-trip CONTRACT test — plain saveDocument preserves outline
# bookmarks + URI/GoTo links; the sanitize path's stripping (Outlines, URI
# actions) is pinned as intentional-and-visible, never silent.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLinkBookmarkRoundTrip.cpp")
    add_executable(TestLinkBookmarkRoundTrip
        tests/TestLinkBookmarkRoundTrip.cpp
    )
    target_include_directories(TestLinkBookmarkRoundTrip PRIVATE src tests)
    target_link_libraries(TestLinkBookmarkRoundTrip PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestLinkBookmarkRoundTrip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestLinkBookmarkRoundTrip>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestLinkBookmarkRoundTrip>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestLinkBookmarkRoundTrip"
    )
    add_test(NAME TestLinkBookmarkRoundTrip COMMAND TestLinkBookmarkRoundTrip)
    set_tests_properties(TestLinkBookmarkRoundTrip PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;conversion;sanitize;links;roundtrip;qt;headless"
    )
endif()

# §9.7 P1 regression test: the per-DocumentSession signature cache — store the
# last accepted signature, clear it on a document switch, and surface it in the
# picker as a default-checked "Reuse last signature" checkbox.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureSessionCache.cpp")
    add_executable(TestSignatureSessionCache
        tests/TestSignatureSessionCache.cpp
    )
    target_include_directories(TestSignatureSessionCache PRIVATE src tests)
    target_link_libraries(TestSignatureSessionCache PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSignatureSessionCache POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSignatureSessionCache>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSignatureSessionCache>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSignatureSessionCache"
    )
    add_test(NAME TestSignatureSessionCache COMMAND TestSignatureSessionCache)
    set_tests_properties(TestSignatureSessionCache PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;signatures;cache;persistence;qt;headless"
    )
endif()

# ── TestCapabilityRegistry (U08 — the ONE capability registry) ───────────────
# Cache/invalidate behavior, UnavailableBuild vs UnavailableRuntime, the
# non-empty whyNot/alternative rule, applyToWidget and fileFilterFor, and the
# truthfulness of the engine probes registered at boot.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCapabilityRegistry.cpp")
    add_executable(TestCapabilityRegistry
        tests/TestCapabilityRegistry.cpp
    )
    target_include_directories(TestCapabilityRegistry PRIVATE src tests)
    # G11 (QUALITY-GATE-2026-09-09): real-model readiness case resolves the
    # bundled model directory the same way TestRapidOcr does (skips when the
    # models are absent, like the inherent RapidOCR-inference skip).
    target_compile_definitions(TestCapabilityRegistry PRIVATE
        GLYPH_OCR_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models/ppocrv5")
    target_link_libraries(TestCapabilityRegistry PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestCapabilityRegistry POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCapabilityRegistry>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCapabilityRegistry>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCapabilityRegistry"
    )
    add_test(NAME TestCapabilityRegistry COMMAND TestCapabilityRegistry)
    set_tests_properties(TestCapabilityRegistry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;capability;u08;qt;headless"
    )
endif()

# ── TestXfaHonestyBanner (N2 — XFA honesty disclosure) ───────────────────────
# Real /AcroForm-/XFA and plain fixtures through the REAL FormManager::
# hasXfaForms and the registry contract (Degraded ⇒ non-empty whyNot +
# alternative; non-XFA ⇒ Available, nothing raised).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestXfaHonestyBanner.cpp")
    add_executable(TestXfaHonestyBanner
        tests/TestXfaHonestyBanner.cpp
    )
    target_include_directories(TestXfaHonestyBanner PRIVATE src tests)
    target_link_libraries(TestXfaHonestyBanner PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestXfaHonestyBanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestXfaHonestyBanner>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestXfaHonestyBanner>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestXfaHonestyBanner"
    )
    # R22 (2026-09-14): the podofo DLL copy previously shared the custom_command
    # above UNGUARDED — on Linux PODOFO_VENDOR_DLL is the empty string, so
    # copy_if_different collapsed to a single argument and failed the build
    # (same L05-mangling class the ledger records for TestBatchOpsCoverage).
    # Split out and WIN32-gated like every other podofo staging block.
    if(WIN32)
    add_custom_command(TARGET TestXfaHonestyBanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestXfaHonestyBanner>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestXfaHonestyBanner"
    )
    endif()
    add_test(NAME TestXfaHonestyBanner COMMAND TestXfaHonestyBanner)
    set_tests_properties(TestXfaHonestyBanner PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;capability;forms;xfa;honesty;qt;headless"
    )
endif()

# ── TestTaskNavRegistry (U02 — the ONE navigation table) ────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTaskNavRegistry.cpp")
    add_executable(TestTaskNavRegistry
        tests/TestTaskNavRegistry.cpp
    )
    target_include_directories(TestTaskNavRegistry PRIVATE src tests)
    target_link_libraries(TestTaskNavRegistry PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestTaskNavRegistry POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestTaskNavRegistry>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestTaskNavRegistry>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestTaskNavRegistry"
    )
    add_test(NAME TestTaskNavRegistry COMMAND TestTaskNavRegistry)
    set_tests_properties(TestTaskNavRegistry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;navigation;tasknav;u02;qt;headless"
    )
endif()

# ── TestScreenStateSync (U02 — single-writer visible-state sync) ────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestScreenStateSync.cpp")
    add_executable(TestScreenStateSync
        tests/TestScreenStateSync.cpp
    )
    target_include_directories(TestScreenStateSync PRIVATE src tests)
    target_link_libraries(TestScreenStateSync PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestScreenStateSync POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestScreenStateSync>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestScreenStateSync>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestScreenStateSync"
    )
    add_test(NAME TestScreenStateSync COMMAND TestScreenStateSync)
    set_tests_properties(TestScreenStateSync PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;navigation;sync;u02;qt;headless"
    )
endif()

# ── TestRibbonCollapse (U02 — collapsible ribbon geometry + triggers) ───────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRibbonCollapse.cpp")
    add_executable(TestRibbonCollapse
        tests/TestRibbonCollapse.cpp
    )
    target_include_directories(TestRibbonCollapse PRIVATE src tests)
    target_link_libraries(TestRibbonCollapse PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestRibbonCollapse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRibbonCollapse>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRibbonCollapse>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestRibbonCollapse"
    )
    add_test(NAME TestRibbonCollapse COMMAND TestRibbonCollapse)
    set_tests_properties(TestRibbonCollapse PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;ribbon;collapse;u02;qt;headless"
    )
endif()

# ── TestModeStripPins (SWEEP-W2 — matrix gap pins: mode pills, toggle-ai, task-chooser) ──
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestModeStripPins.cpp")
    add_executable(TestModeStripPins
        tests/TestModeStripPins.cpp
    )
    target_include_directories(TestModeStripPins PRIVATE src tests)
    target_link_libraries(TestModeStripPins PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestModeStripPins POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestModeStripPins>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestModeStripPins>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestModeStripPins"
    )
    add_test(NAME TestModeStripPins COMMAND TestModeStripPins)
    set_tests_properties(TestModeStripPins PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;mode-strip;matrix-pins;sweep-w2;qt;headless"
    )
endif()

# ── TestStatusBarSlim (U02 — slim status bar + details facts) ───────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestStatusBarSlim.cpp")
    add_executable(TestStatusBarSlim
        tests/TestStatusBarSlim.cpp
    )
    target_include_directories(TestStatusBarSlim PRIVATE src tests)
    # G20 (QUALITY-GATE-2026-09-09): the theme QSS sheets are NOT compiled
    # into test binaries (resources.qrc belongs to PdfWorkstation), so the
    # suite must resolve them from the SOURCE tree. Inject the absolute
    # source resource dir (the DJOT_LIB_DIR CMake-injection pattern) — the
    # old applicationDirPath()+"/../resources" assumption breaks every
    # genuine out-of-source build whose build directory is not a direct
    # child of the source root.
    target_compile_definitions(TestStatusBarSlim PRIVATE
        GLYPHPDF_SOURCE_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
    target_link_libraries(TestStatusBarSlim PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestStatusBarSlim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestStatusBarSlim>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestStatusBarSlim>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestStatusBarSlim"
    )
    add_test(NAME TestStatusBarSlim COMMAND TestStatusBarSlim)
    set_tests_properties(TestStatusBarSlim PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;statusbar;u02;qt;headless"
    )
endif()

# U07 comments review: active-filter summary + result count + clear action,
# a table view over the SAME annotation records, and CSV export of the
# displayed scope with RFC-4180 escaping (no second markup store).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCommentsReview.cpp")
    add_executable(TestCommentsReview
        tests/TestCommentsReview.cpp
    )
    target_include_directories(TestCommentsReview PRIVATE src tests)
    target_link_libraries(TestCommentsReview PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestCommentsReview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCommentsReview>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCommentsReview>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCommentsReview"
    )
    add_test(NAME TestCommentsReview COMMAND TestCommentsReview)
    set_tests_properties(TestCommentsReview PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;comments;review;qt;headless"
    )
endif()

# U01 layout regression: the welcome screen's six action cards sat in one row
# needing 900px inside a 600px column (final cards clipped) and never reflowed.
# The test pins geometric overflow at 1280x720 and the responsive 3/2/1-column
# reflow headlessly; pixel-level spacing/palette checks remain manual (plan §U01).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWelcomeLayout.cpp")
    add_executable(TestWelcomeLayout
        tests/TestWelcomeLayout.cpp
    )
    target_include_directories(TestWelcomeLayout PRIVATE src tests)
    target_link_libraries(TestWelcomeLayout PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestWelcomeLayout POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestWelcomeLayout>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestWelcomeLayout>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestWelcomeLayout"
    )
    add_test(NAME TestWelcomeLayout COMMAND TestWelcomeLayout)
    set_tests_properties(TestWelcomeLayout PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "ui;welcome;layout;qt;headless"
    )
endif()

# §9.7 P0 audit item: on-page signature validity badges. The badge layer is a
# Qt VIEW-LAYER overlay only (never persisted into the PDF); the test pins the
# state→color mapping, the on-page anchor (field rect top-right corner),
# clearing, tooltips and the SignaturesPanel → PdfViewerWidget badge push.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureBadges.cpp")
    add_executable(TestSignatureBadges
        tests/TestSignatureBadges.cpp
    )
    target_include_directories(TestSignatureBadges PRIVATE src tests)
    target_compile_definitions(TestSignatureBadges PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(TestSignatureBadges PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSignatureBadges POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSignatureBadges>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSignatureBadges>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSignatureBadges"
    )
    add_test(NAME TestSignatureBadges COMMAND TestSignatureBadges)
    set_tests_properties(TestSignatureBadges PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;signatures;badge;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchOcrConfidence.cpp")
    add_executable(TestBatchOcrConfidence
        tests/TestBatchOcrConfidence.cpp
    )
    target_include_directories(TestBatchOcrConfidence PRIVATE src tests)
    target_link_libraries(TestBatchOcrConfidence PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestBatchOcrConfidence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestBatchOcrConfidence>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestBatchOcrConfidence>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOcrConfidence"
    )
    add_test(NAME TestBatchOcrConfidence COMMAND TestBatchOcrConfidence)
    set_tests_properties(TestBatchOcrConfidence PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;batch;ocr;confidence;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestInspector.cpp")
    add_executable(TestInspector
        tests/TestInspector.cpp
    )
    target_include_directories(TestInspector PRIVATE src tests)
    target_link_libraries(TestInspector PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestInspector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestInspector>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestInspector>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestInspector"
    )
    add_test(NAME TestInspector COMMAND TestInspector)
    set_tests_properties(TestInspector PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;inspector;properties;ui;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPatternRedact.cpp")
    add_executable(TestPatternRedact
        tests/TestPatternRedact.cpp
    )
    target_include_directories(TestPatternRedact PRIVATE src tests)
    target_link_libraries(TestPatternRedact PRIVATE
        pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestPatternRedact POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestPatternRedact>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestPatternRedact>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestPatternRedact"
    )
    add_test(NAME TestPatternRedact COMMAND TestPatternRedact)
    set_tests_properties(TestPatternRedact PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;redaction;patterns;qt;headless"
    )
endif()

# T2-2 Find & Replace: flag-aware matcher (exact/case/whole-words/regex) over
# the PDFium text layer, the ITextReplacer excise+redraw engine seam verified
# in a saved+reopened artifact, measured width warnings, and the modeless
# FindReplaceDialog (match count before replace, page scoping, injected
# invoker — no MainWindow, no modals).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFindReplace.cpp")
    add_executable(TestFindReplace
        tests/TestFindReplace.cpp
        # packa-F1: the controller-boundary regression drives the REAL
        # MainWindow + EditController (Bootstrapper context) for the
        # zero-mutation guarantee at the shared pipeline boundary.
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestFindReplace PRIVATE src tests)
    target_compile_definitions(TestFindReplace PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestFindReplace PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestFindReplace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFindReplace>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFindReplace>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestFindReplace"
    )
    add_test(NAME TestFindReplace COMMAND TestFindReplace)
    set_tests_properties(TestFindReplace PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;edit;find-replace;t2-2;qt;headless"
    )
endif()

# T2-3 review-summary document: grouped printable summary artifact (page →
# author → date, statuses, timestamps, totals header) verified as a real
# saved+reopened PDF, plus the displayed-scope export contract on
# CommentsWidget (filter respected, empty scope refused).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestReviewSummary.cpp")
    add_executable(TestReviewSummary
        tests/TestReviewSummary.cpp
    )
    target_include_directories(TestReviewSummary PRIVATE src tests)
    target_link_libraries(TestReviewSummary PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestReviewSummary POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestReviewSummary>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestReviewSummary>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestReviewSummary"
    )
    add_test(NAME TestReviewSummary COMMAND TestReviewSummary)
    set_tests_properties(TestReviewSummary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;comments;review-summary;t2-3;qt;headless"
    )
endif()

# R27 tail: printable review summary — the print-ready SURFACE over the T2-3
# content model (numbered entries + table of entries, per-sheet footers,
# redaction-proof availability honesty, SafeSave overwrite guard on the
# destination). Serial offscreen only (the guard pin uses the process-global
# SafeSave commit-fault seam and the shared %TEMP%/glyphpdf-candidates class).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPrintableSummary.cpp")
    add_executable(TestPrintableSummary
        tests/TestPrintableSummary.cpp
    )
    target_include_directories(TestPrintableSummary PRIVATE src tests)
    target_link_libraries(TestPrintableSummary PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestPrintableSummary POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestPrintableSummary>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestPrintableSummary>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestPrintableSummary"
    )
    add_test(NAME TestPrintableSummary COMMAND TestPrintableSummary)
    set_tests_properties(TestPrintableSummary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        RUN_SERIAL TRUE
        LABELS "unit;comments;printable-summary;qt;headless"
    )
endif()

# T2-6 dynamic stamps: apply-time placeholder substitution, custom-stamp
# persistence (file written + reloaded, built-in ids protected), the SAVED
# annotation text carrying the resolved values (real embed/extract roundtrip),
# the placement gesture consuming the pending resolution, and the library
# dialog + registry wiring of the previously silent Stamps menu items.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDynamicStamps.cpp")
    add_executable(TestDynamicStamps
        tests/TestDynamicStamps.cpp
    )
    target_include_directories(TestDynamicStamps PRIVATE src tests)
    target_link_libraries(TestDynamicStamps PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestDynamicStamps POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestDynamicStamps>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestDynamicStamps>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestDynamicStamps"
    )
    add_test(NAME TestDynamicStamps COMMAND TestDynamicStamps)
    set_tests_properties(TestDynamicStamps PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;comments;stamps;t2-6;qt;headless"
    )
endif()

# T2-9 auto-bookmarks: heading/TOC heuristics (disclosed basis), the
# IOutlineEditor committed outline write verified in a saved+reopened
# artifact, the preview dialog (editable titles, uncheckable rows), and the
# undoable SetOutlineCommand (undo restores the previous outline from disk).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAutoBookmarks.cpp")
    add_executable(TestAutoBookmarks
        tests/TestAutoBookmarks.cpp
        # packa-F3: the single-write regression drives the REAL controller +
        # window (Bootstrapper context, counting engine injected).
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestAutoBookmarks PRIVATE src tests)
    target_compile_definitions(TestAutoBookmarks PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestAutoBookmarks PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestAutoBookmarks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestAutoBookmarks>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestAutoBookmarks>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestAutoBookmarks"
    )
    add_test(NAME TestAutoBookmarks COMMAND TestAutoBookmarks)
    set_tests_properties(TestAutoBookmarks PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;edit;auto-bookmarks;t2-9;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDjotRoundtrip.cpp")
    add_executable(TestDjotRoundtrip
        tests/TestDjotRoundtrip.cpp
    )
    target_include_directories(TestDjotRoundtrip PRIVATE src tests)
    # Q02 source-root: inject the vendored djot library path (same
    # DJOT_LIB_DIR pattern as PdfWorkstation) instead of relying on
    # applicationDirPath()+"/../" resolving to the source root.
    target_compile_definitions(TestDjotRoundtrip PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestDjotRoundtrip PRIVATE
        pdfws_djot docmodel
        Qt6::Test Qt6::Core
    )
    add_test(NAME TestDjotRoundtrip COMMAND TestDjotRoundtrip)
    set_tests_properties(TestDjotRoundtrip PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;djot;codec;provenance"
    )
endif()

# ── TestDjotFuzz (R2-7 G-07) ─────────────────────────────────────────────
# DocumentFuzzer round-trip fuzz harness: generates random SemanticDocument
# instances via DocumentFuzzer, encodes them to Djot, then decodes back and
# asserts no crash + content-presence. Structural equivalence tests are
# QEXPECT_FAIL until djotToDocument AST-walking is fully implemented (M5).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDjotFuzz.cpp")
    add_executable(TestDjotFuzz
        tests/TestDjotFuzz.cpp
    )
    target_include_directories(TestDjotFuzz PRIVATE src tests)
    # Q02 source-root: injected absolute djot-lib path (see TestDjotRoundtrip).
    target_compile_definitions(TestDjotFuzz PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestDjotFuzz PRIVATE
        pdfws_djot docmodel
        Qt6::Test Qt6::Core
    )
    add_test(NAME TestDjotFuzz COMMAND TestDjotFuzz)
    set_tests_properties(TestDjotFuzz PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;djot;fuzz;codec;roundtrip"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestUpdateChecker.cpp")
    add_executable(TestUpdateChecker
        tests/TestUpdateChecker.cpp
        src/core/UpdateChecker.cpp
        src/core/TempFileManager.cpp
    )
    target_include_directories(TestUpdateChecker PRIVATE src tests)
    target_compile_definitions(TestUpdateChecker PRIVATE GLYPH_TESTING=1)
    target_link_libraries(TestUpdateChecker PRIVATE
        Qt6::Test Qt6::Core Qt6::Network
    )
    if(WIN32)
        target_link_libraries(TestUpdateChecker PRIVATE Wintrust Crypt32)
    endif()
    add_test(NAME TestUpdateChecker COMMAND TestUpdateChecker)
    set_tests_properties(TestUpdateChecker PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
        LABELS "unit;update;security;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestWatermarkFont.cpp")
    add_executable(TestWatermarkFont
        tests/TestWatermarkFont.cpp
    )
    target_include_directories(TestWatermarkFont PRIVATE src tests)
    target_link_libraries(TestWatermarkFont PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestWatermarkFont POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestWatermarkFont>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestWatermarkFont>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestWatermarkFont"
    )
    add_test(NAME TestWatermarkFont COMMAND TestWatermarkFont)
    set_tests_properties(TestWatermarkFont PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;security;watermark;font;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrLanguage.cpp")
    add_executable(TestOcrLanguage
        tests/TestOcrLanguage.cpp
    )
    target_include_directories(TestOcrLanguage PRIVATE src tests)
    target_link_libraries(TestOcrLanguage PRIVATE
        pdfws_core Qt6::Test Qt6::Core Qt6::Gui
    )
    add_test(NAME TestOcrLanguage COMMAND TestOcrLanguage)
    set_tests_properties(TestOcrLanguage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
        LABELS "unit;ocr;language;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactClearMarks.cpp")
    add_executable(TestRedactClearMarks
        tests/TestRedactClearMarks.cpp
    )
    target_include_directories(TestRedactClearMarks PRIVATE src tests)
    target_link_libraries(TestRedactClearMarks PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_test(NAME TestRedactClearMarks COMMAND TestRedactClearMarks)
    set_tests_properties(TestRedactClearMarks PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;redaction;security;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLinkSchemeSafety.cpp")
    add_executable(TestLinkSchemeSafety tests/TestLinkSchemeSafety.cpp)
    target_include_directories(TestLinkSchemeSafety PRIVATE src tests)
    target_link_libraries(TestLinkSchemeSafety PRIVATE pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent)
    add_test(NAME TestLinkSchemeSafety COMMAND TestLinkSchemeSafety)
    set_tests_properties(TestLinkSchemeSafety PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;security;navigation;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestThumbnailZoom.cpp")
    add_executable(TestThumbnailZoom tests/TestThumbnailZoom.cpp)
    target_include_directories(TestThumbnailZoom PRIVATE src tests)
    target_link_libraries(TestThumbnailZoom PRIVATE pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent)
    add_test(NAME TestThumbnailZoom COMMAND TestThumbnailZoom)
    set_tests_properties(TestThumbnailZoom PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;search;navigation;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMergeSuccess.cpp")
    add_executable(TestMergeSuccess tests/TestMergeSuccess.cpp)
    target_include_directories(TestMergeSuccess PRIVATE src tests)
    target_link_libraries(TestMergeSuccess PRIVATE pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent)
    add_test(NAME TestMergeSuccess COMMAND TestMergeSuccess)
    set_tests_properties(TestMergeSuccess PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;pages;merge;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFillFormLock.cpp")
    add_executable(TestFillFormLock
        tests/TestFillFormLock.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFillFormLock PRIVATE src tests)
    target_link_libraries(TestFillFormLock PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFillFormLock POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFillFormLock>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFillFormLock>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFillFormLock"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFillFormLock POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFillFormLock>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFillFormLock"
        )
    endif()
    add_test(NAME TestFillFormLock COMMAND TestFillFormLock)
    set_tests_properties(TestFillFormLock PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;forms;form-fields;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFillFormNoOp.cpp")
    add_executable(TestFillFormNoOp
        tests/TestFillFormNoOp.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFillFormNoOp PRIVATE src tests)
    target_link_libraries(TestFillFormNoOp PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFillFormNoOp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFillFormNoOp>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFillFormNoOp>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFillFormNoOp"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFillFormNoOp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFillFormNoOp>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFillFormNoOp"
        )
    endif()
    add_test(NAME TestFillFormNoOp COMMAND TestFillFormNoOp)
    set_tests_properties(TestFillFormNoOp PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;forms;form-fields;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestReadingOrderInheritance.cpp")
    add_executable(TestReadingOrderInheritance tests/TestReadingOrderInheritance.cpp)
    target_include_directories(TestReadingOrderInheritance PRIVATE src tests)
    target_link_libraries(TestReadingOrderInheritance PRIVATE
        pdfws_ui pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestReadingOrderInheritance COMMAND TestReadingOrderInheritance)
    set_tests_properties(TestReadingOrderInheritance PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;accessibility;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestShapeInkPersistence.cpp")
    add_executable(TestShapeInkPersistence tests/TestShapeInkPersistence.cpp)
    target_include_directories(TestShapeInkPersistence PRIVATE src tests)
    target_link_libraries(TestShapeInkPersistence PRIVATE
        pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestShapeInkPersistence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestShapeInkPersistence>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestShapeInkPersistence"
    )
    endif()
    add_test(NAME TestShapeInkPersistence COMMAND TestShapeInkPersistence)
    set_tests_properties(TestShapeInkPersistence PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;annotations;persistence;qt;headless")
endif()

# T1 measurement toolset: interactive UI honesty — the Measure panel's pinned
# disclosures (persistence, session-only calibration, deferred scopes) and the
# AnnotationLayer click-input state machine (G21 overlay rect, G22 label
# semantics, Esc/Enter/double-click/Backspace handling).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMeasurePanelHonesty.cpp")
    add_executable(TestMeasurePanelHonesty tests/TestMeasurePanelHonesty.cpp)
    target_include_directories(TestMeasurePanelHonesty PRIVATE src tests)
    target_link_libraries(TestMeasurePanelHonesty PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui
        Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_test(NAME TestMeasurePanelHonesty COMMAND TestMeasurePanelHonesty)
    set_tests_properties(TestMeasurePanelHonesty PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;measure;ui;qt;headless")
endif()

# T1 measurement toolset: pure calibration/geometry/formatting math
# (header-only src/core/MeasureCore.h — no GUI, no engines, no I/O).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMeasureCore.cpp")
    add_executable(TestMeasureCore tests/TestMeasureCore.cpp)
    target_include_directories(TestMeasureCore PRIVATE src tests)
    target_link_libraries(TestMeasureCore PRIVATE
        pdfws_core Qt6::Test Qt6::Core
    )
    add_test(NAME TestMeasureCore COMMAND TestMeasureCore)
    set_tests_properties(TestMeasureCore PROPERTIES TIMEOUT 60 LABELS "unit;measure;qt;headless"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

# T1 measurement toolset: saved-artifact round-trip (extractAnnotations + raw
# PoDoFo dict inspection + PDFium annotation read path + uncalibrated negative
# control + plain-DrawLine regression guard).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMeasureRoundTrip.cpp")
    add_executable(TestMeasureRoundTrip tests/TestMeasureRoundTrip.cpp)
    target_include_directories(TestMeasureRoundTrip PRIVATE src tests)
    target_link_libraries(TestMeasureRoundTrip PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestMeasureRoundTrip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestMeasureRoundTrip>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestMeasureRoundTrip"
    )
    endif()
    add_test(NAME TestMeasureRoundTrip COMMAND TestMeasureRoundTrip)
    set_tests_properties(TestMeasureRoundTrip PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;measure;annotations;persistence;qt;headless")
endif()

# sweep-legacy (2026-09-19): the /Rotate + offset-MediaBox origin class at the
# annotation embed/extract and form-field creation/update boundaries — the same
# law PageSpaceTransform.h established for redaction (SEP13 L5/L8, F1), pinned
# for the legacy annotation and form surfaces via raw /Rect arrays + PDFium.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLegacyOriginSpace.cpp")
    add_executable(TestLegacyOriginSpace tests/TestLegacyOriginSpace.cpp)
    target_include_directories(TestLegacyOriginSpace PRIVATE src tests)
    target_link_libraries(TestLegacyOriginSpace PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
    add_custom_command(TARGET TestLegacyOriginSpace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestLegacyOriginSpace>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestLegacyOriginSpace"
    )
    endif()
    add_test(NAME TestLegacyOriginSpace COMMAND TestLegacyOriginSpace)
    set_tests_properties(TestLegacyOriginSpace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;annotations;forms;persistence;qt;headless")
endif()

# N1 (backlog 2026-09-10): measurement CSV export — the item the T1 lane
# explicitly deferred. Pins the RFC-4180 CSV contract (calibrated perimeter
# rows reproduce value+unit+page; empty export = headers only) plus the
# panel's Export CSV wiring.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMeasureCsvExport.cpp")
    add_executable(TestMeasureCsvExport tests/TestMeasureCsvExport.cpp)
    target_include_directories(TestMeasureCsvExport PRIVATE src tests)
    target_link_libraries(TestMeasureCsvExport PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestMeasureCsvExport COMMAND TestMeasureCsvExport)
    set_tests_properties(TestMeasureCsvExport PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;measure;csv;ui;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompressStripSanitize.cpp")
    add_executable(TestCompressStripSanitize tests/TestCompressStripSanitize.cpp)
    target_include_directories(TestCompressStripSanitize PRIVATE src tests)
    target_link_libraries(TestCompressStripSanitize PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestCompressStripSanitize POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestCompressStripSanitize>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestCompressStripSanitize"
    )
    endif()
    add_test(NAME TestCompressStripSanitize COMMAND TestCompressStripSanitize)
    set_tests_properties(TestCompressStripSanitize PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;compression;sanitization;privacy;qt;headless")
endif()

# R12 regression test: CompressDialog's "Subset fonts" / "Remove unused objects"
# checkboxes shipped CHECKED while the backend implements neither pass. Pins the
# UI-honesty contract: the controls stay visible but disabled+unchecked with the
# availability explanation, presets never re-check them, and the OptimizeOptions
# reaching the engine never request either unsupported pass.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompressDialogHonesty.cpp")
    add_executable(TestCompressDialogHonesty
        tests/TestCompressDialogHonesty.cpp
    )
    target_include_directories(TestCompressDialogHonesty PRIVATE src tests)
    target_link_libraries(TestCompressDialogHonesty PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestCompressDialogHonesty POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCompressDialogHonesty>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestCompressDialogHonesty>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestCompressDialogHonesty"
    )
    add_test(NAME TestCompressDialogHonesty COMMAND TestCompressDialogHonesty)
    set_tests_properties(TestCompressDialogHonesty PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;compression;honesty;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFieldMetadata.cpp")
    add_executable(TestFieldMetadata
        tests/TestFieldMetadata.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFieldMetadata PRIVATE src tests)
    target_link_libraries(TestFieldMetadata PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFieldMetadata POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFieldMetadata>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFieldMetadata>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFieldMetadata"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFieldMetadata POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFieldMetadata>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFieldMetadata"
        )
    endif()
    add_test(NAME TestFieldMetadata COMMAND TestFieldMetadata)
    set_tests_properties(TestFieldMetadata PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;forms;form-fields;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormSafety.cpp")
    add_executable(TestFormSafety
        tests/TestFormSafety.cpp
    )
    target_include_directories(TestFormSafety PRIVATE src tests)
    # S2-2: the pin drives the REAL FormBuilderMode widget route — pdfws_ui
    # (and its Qt deps, same link set as the other pdfws_ui suites) needed.
    # DocumentSession.cpp is NOT compiled into the target any more: pdfws_ui
    # pulls the pdfws_engines archive member, which would duplicate its
    # staticMetaObject with a locally compiled copy.
    target_link_libraries(TestFormSafety PRIVATE
        pdfws_engines pdfws_core pdfws_commands pdfws_ui podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestFormSafety POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormSafety>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormSafety>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormSafety"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFormSafety POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormSafety>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormSafety"
        )
    endif()
    add_test(NAME TestFormSafety COMMAND TestFormSafety)
    set_tests_properties(TestFormSafety PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 LABELS "unit;forms;form-fields;persistence;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormJsCalc.cpp")
    add_executable(TestFormJsCalc
        tests/TestFormJsCalc.cpp
    )
    target_include_directories(TestFormJsCalc PRIVATE src tests)
    target_link_libraries(TestFormJsCalc PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormJsCalc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormJsCalc>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormJsCalc>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormJsCalc"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFormJsCalc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormJsCalc>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormJsCalc"
        )
    endif()
    if(QUICKJS_RUNTIME_DLL AND EXISTS "${QUICKJS_RUNTIME_DLL}")
        add_custom_command(TARGET TestFormJsCalc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QUICKJS_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:TestFormJsCalc>"
            COMMENT "Deploying quickjs-ng runtime DLL for TestFormJsCalc"
        )
    endif()
    add_test(NAME TestFormJsCalc COMMAND TestFormJsCalc)
    set_tests_properties(TestFormJsCalc PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 240 LABELS "unit;forms;form-js;javascript;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormStaleDisclosure.cpp")
    add_executable(TestFormStaleDisclosure
        tests/TestFormStaleDisclosure.cpp
    )
    target_include_directories(TestFormStaleDisclosure PRIVATE src tests)
    target_link_libraries(TestFormStaleDisclosure PRIVATE
        pdfws_engines pdfws_core pdfws_commands pdfws_ui podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormStaleDisclosure POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormStaleDisclosure>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormStaleDisclosure>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormStaleDisclosure"
    )
    if(WIN32)
        add_custom_command(TARGET TestFormStaleDisclosure POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormStaleDisclosure>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormStaleDisclosure"
        )
    endif()
    if(QUICKJS_RUNTIME_DLL AND EXISTS "${QUICKJS_RUNTIME_DLL}")
        add_custom_command(TARGET TestFormStaleDisclosure POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QUICKJS_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:TestFormStaleDisclosure>"
            COMMENT "Deploying quickjs-ng runtime DLL for TestFormStaleDisclosure"
        )
    endif()
    add_test(NAME TestFormStaleDisclosure COMMAND TestFormStaleDisclosure)
    set_tests_properties(TestFormStaleDisclosure PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 240 LABELS "unit;forms;form-js;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormKeystroke.cpp")
    add_executable(TestFormKeystroke
        tests/TestFormKeystroke.cpp
    )
    target_include_directories(TestFormKeystroke PRIVATE src tests)
    target_link_libraries(TestFormKeystroke PRIVATE
        pdfws_engines pdfws_core pdfws_commands pdfws_ui podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormKeystroke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormKeystroke>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormKeystroke>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormKeystroke"
    )
    if(WIN32)
        add_custom_command(TARGET TestFormKeystroke POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormKeystroke>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormKeystroke"
        )
    endif()
    if(QUICKJS_RUNTIME_DLL AND EXISTS "${QUICKJS_RUNTIME_DLL}")
        add_custom_command(TARGET TestFormKeystroke POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QUICKJS_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:TestFormKeystroke>"
            COMMENT "Deploying quickjs-ng runtime DLL for TestFormKeystroke"
        )
    endif()
    add_test(NAME TestFormKeystroke COMMAND TestFormKeystroke)
    set_tests_properties(TestFormKeystroke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 240 LABELS "unit;forms;form-js;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEngineSave.cpp")
    add_executable(TestEngineSave
        tests/TestEngineSave.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestEngineSave PRIVATE src tests)
    target_link_libraries(TestEngineSave PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestEngineSave POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestEngineSave>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestEngineSave>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestEngineSave"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestEngineSave POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestEngineSave>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestEngineSave"
        )
    endif()
    add_test(NAME TestEngineSave COMMAND TestEngineSave)
    set_tests_properties(TestEngineSave PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 LABELS "unit;engine;save;persistence;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEncryptedPackageSafeWrite.cpp")
    add_executable(TestEncryptedPackageSafeWrite
        tests/TestEncryptedPackageSafeWrite.cpp
        src/engines/SafeSave.cpp
    )
    target_include_directories(TestEncryptedPackageSafeWrite PRIVATE src tests)
    target_link_libraries(TestEncryptedPackageSafeWrite PRIVATE
        Qt6::Test Qt6::Core Qt6::Gui
    )
    add_test(NAME TestEncryptedPackageSafeWrite COMMAND TestEncryptedPackageSafeWrite)
    set_tests_properties(TestEncryptedPackageSafeWrite PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300 LABELS "unit;export;safesave;shell;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPrintJob.cpp")
    add_executable(TestPrintJob
        tests/TestPrintJob.cpp
        src/ui/PrintJob.cpp
    )
    target_include_directories(TestPrintJob PRIVATE src tests)
    target_link_libraries(TestPrintJob PRIVATE
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PrintSupport
    )
    add_custom_command(TARGET TestPrintJob POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestPrintJob>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestPrintJob>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestPrintJob"
    )
    add_test(NAME TestPrintJob COMMAND TestPrintJob)
    set_tests_properties(TestPrintJob PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300 LABELS "unit;print;ui;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTempOwnership.cpp")
    add_executable(TestTempOwnership
        tests/TestTempOwnership.cpp
        src/core/TempFileManager.cpp
    )
    target_include_directories(TestTempOwnership PRIVATE src tests)
    target_link_libraries(TestTempOwnership PRIVATE
        Qt6::Test Qt6::Core
    )
    add_test(NAME TestTempOwnership COMMAND TestTempOwnership)
    set_tests_properties(TestTempOwnership PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300 LABELS "unit;temp;infrastructure;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCleanupCli.cpp")
    add_executable(TestCleanupCli
        tests/TestCleanupCli.cpp
    )
    target_include_directories(TestCleanupCli PRIVATE src tests)
    target_compile_definitions(TestCleanupCli PRIVATE
        CLEANUP_CLI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(TestCleanupCli PRIVATE
        Qt6::Test Qt6::Core Qt6::Gui
    )
    add_test(NAME TestCleanupCli COMMAND TestCleanupCli)
    set_tests_properties(TestCleanupCli PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300 LABELS "unit;tools;cleanup-cli;infrastructure;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormUndo.cpp")
    add_executable(TestFormUndo
        tests/TestFormUndo.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFormUndo PRIVATE src tests)
    target_link_libraries(TestFormUndo PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormUndo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormUndo>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestFormUndo>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormUndo"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestFormUndo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestFormUndo>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestFormUndo"
        )
    endif()
    add_test(NAME TestFormUndo COMMAND TestFormUndo)
    set_tests_properties(TestFormUndo PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 LABELS "unit;forms;form-fields;undo;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestValidateAllSignatures.cpp")
    add_executable(TestValidateAllSignatures tests/TestValidateAllSignatures.cpp)
    target_include_directories(TestValidateAllSignatures PRIVATE src tests)
    target_link_libraries(TestValidateAllSignatures PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestValidateAllSignatures COMMAND TestValidateAllSignatures)
    set_tests_properties(TestValidateAllSignatures PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;signatures;security;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestExportPathBadge.cpp")
    add_executable(TestExportPathBadge tests/TestExportPathBadge.cpp)
    target_include_directories(TestExportPathBadge PRIVATE src tests)
    target_link_libraries(TestExportPathBadge PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestExportPathBadge COMMAND TestExportPathBadge)
    set_tests_properties(TestExportPathBadge PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;conversion;export;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestImageEditWiring.cpp")
    add_executable(TestImageEditWiring tests/TestImageEditWiring.cpp)
    target_include_directories(TestImageEditWiring PRIVATE src tests)
    target_link_libraries(TestImageEditWiring PRIVATE
        pdfws_commands pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui
    )
    add_test(NAME TestImageEditWiring COMMAND TestImageEditWiring)
    set_tests_properties(TestImageEditWiring PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;editing;images;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactSanitizeBundle.cpp")
    add_executable(TestRedactSanitizeBundle tests/TestRedactSanitizeBundle.cpp)
    target_include_directories(TestRedactSanitizeBundle PRIVATE src tests)
    target_link_libraries(TestRedactSanitizeBundle PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestRedactSanitizeBundle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactSanitizeBundle>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactSanitizeBundle"
    )
    endif()
    add_test(NAME TestRedactSanitizeBundle COMMAND TestRedactSanitizeBundle)
    set_tests_properties(TestRedactSanitizeBundle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;redaction;sanitization;security;privacy;qt;headless")
endif()

# ── TestRedactTransaction (U05 — one transactional redaction operation) ──────
# Pins the R01 safe-save transaction behind both redaction entry paths: the
# source PDF is byte-identical after every failure/cancel, the committed output
# is verified with an independent extractor (Pdfium), the ER-2 signed refusal
# fires at Preflight, and sanitize failure yields the labeled partial state.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactTransaction.cpp")
    add_executable(TestRedactTransaction tests/TestRedactTransaction.cpp)
    target_include_directories(TestRedactTransaction PRIVATE src tests)
    target_compile_definitions(TestRedactTransaction PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(TestRedactTransaction PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestRedactTransaction POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRedactTransaction>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRedactTransaction>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo DLL for TestRedactTransaction"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestRedactTransaction POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestRedactTransaction>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestRedactTransaction"
        )
    endif()
    add_test(NAME TestRedactTransaction COMMAND TestRedactTransaction)
    set_tests_properties(TestRedactTransaction PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;redaction;transaction;security;privacy;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# ── TestRedactionProof (T1-2 — Redaction Proof Mode) ─────────────────────────
# Pins the post-commit proof contract on saved artifacts: per-excision manifest
# (region/method/stream digests/operator counts), the full-document survival
# sweep (raw bytes, object strings, decoded streams, extracted text, info
# dictionary, XMP, embedded files, revision structure), loud failure on planted
# survivors (metadata/XMP/annotation/attachment/missed-spot), proof pack
# export, and the whole-flow RedactOperation integration with produceProof.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactionProof.cpp")
    add_executable(TestRedactionProof tests/TestRedactionProof.cpp)
    target_include_directories(TestRedactionProof PRIVATE src tests)
    target_compile_definitions(TestRedactionProof PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(TestRedactionProof PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestRedactionProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRedactionProof>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRedactionProof>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo DLL for TestRedactionProof"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestRedactionProof POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestRedactionProof>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestRedactionProof"
        )
    endif()
    add_test(NAME TestRedactionProof COMMAND TestRedactionProof)
    set_tests_properties(TestRedactionProof PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;redaction;proof;security;privacy;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPptxOverlayAlpha.cpp")
    add_executable(TestPptxOverlayAlpha tests/TestPptxOverlayAlpha.cpp)
    target_include_directories(TestPptxOverlayAlpha PRIVATE src tests)
    target_link_libraries(TestPptxOverlayAlpha PRIVATE
        pdfws_engines pdfws_core zip Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestPptxOverlayAlpha COMMAND TestPptxOverlayAlpha)
    set_tests_properties(TestPptxOverlayAlpha PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;conversion;pptx;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAutoDetectHeuristic.cpp")
    add_executable(TestAutoDetectHeuristic
        tests/TestAutoDetectHeuristic.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestAutoDetectHeuristic PRIVATE src tests)
    target_link_libraries(TestAutoDetectHeuristic PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestAutoDetectHeuristic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestAutoDetectHeuristic>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestAutoDetectHeuristic>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestAutoDetectHeuristic"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestAutoDetectHeuristic POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestAutoDetectHeuristic>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestAutoDetectHeuristic"
        )
    endif()
    add_test(NAME TestAutoDetectHeuristic COMMAND TestAutoDetectHeuristic)
    set_tests_properties(TestAutoDetectHeuristic PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;forms;autodetect;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestConversionExtraction.cpp")
    add_executable(TestConversionExtraction tests/TestConversionExtraction.cpp)
    target_include_directories(TestConversionExtraction PRIVATE src tests)
    target_link_libraries(TestConversionExtraction PRIVATE
        pdfws_engines pdfws_core zip Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestConversionExtraction COMMAND TestConversionExtraction)
    set_tests_properties(TestConversionExtraction PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;conversion;extraction;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTextExtractionCoords.cpp")
    add_executable(TestTextExtractionCoords tests/TestTextExtractionCoords.cpp)
    target_include_directories(TestTextExtractionCoords PRIVATE src tests)
    target_link_libraries(TestTextExtractionCoords PRIVATE
        pdfws_engines pdfws_core zip Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestTextExtractionCoords COMMAND TestTextExtractionCoords)
    set_tests_properties(TestTextExtractionCoords PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;conversion;pptx;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEraseWiring.cpp")
    add_executable(TestEraseWiring tests/TestEraseWiring.cpp)
    target_include_directories(TestEraseWiring PRIVATE src tests)
    target_link_libraries(TestEraseWiring PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestEraseWiring COMMAND TestEraseWiring)
    set_tests_properties(TestEraseWiring PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;editing;erase;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAnnotationToolBar.cpp")
    add_executable(TestAnnotationToolBar tests/TestAnnotationToolBar.cpp)
    target_include_directories(TestAnnotationToolBar PRIVATE src tests)
    target_link_libraries(TestAnnotationToolBar PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestAnnotationToolBar COMMAND TestAnnotationToolBar)
    set_tests_properties(TestAnnotationToolBar PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;annotations;toolbar;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestViewerRotation.cpp")
    add_executable(TestViewerRotation tests/TestViewerRotation.cpp)
    target_include_directories(TestViewerRotation PRIVATE src tests)
    target_link_libraries(TestViewerRotation PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_test(NAME TestViewerRotation COMMAND TestViewerRotation)
    set_tests_properties(TestViewerRotation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;viewing;rotation;qt;headless")
endif()

# §9.1 P0 regression test: two-page mode must not silently hide annotations and
# search highlights — the spread pixmaps composite the same AnnotationItem list
# and the live QPdfSearchModel results the single-page overlay path consumes.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTwoPageOverlay.cpp")
    add_executable(TestTwoPageOverlay tests/TestTwoPageOverlay.cpp)
    target_include_directories(TestTwoPageOverlay PRIVATE src tests)
    target_link_libraries(TestTwoPageOverlay PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestTwoPageOverlay POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestTwoPageOverlay>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestTwoPageOverlay"
            )
        endif()
    endif()
    add_test(NAME TestTwoPageOverlay COMMAND TestTwoPageOverlay)
    set_tests_properties(TestTwoPageOverlay PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;viewing;twopage;regression;qt;headless"
    )
endif()

# Reading filters: Night Mode (page-pixel inversion, Wave 2B viewing-parity
# port) and Eye Care (sepia) — repeated toggling, mutual exclusion, and both
# page surfaces (single-page view and two-page spread).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestViewingModes.cpp")
    add_executable(TestViewingModes tests/TestViewingModes.cpp)
    target_include_directories(TestViewingModes PRIVATE src tests)
    target_link_libraries(TestViewingModes PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestViewingModes POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestViewingModes>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestViewingModes"
            )
        endif()
    endif()
    add_test(NAME TestViewingModes COMMAND TestViewingModes)
    set_tests_properties(TestViewingModes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;viewing;nightmode;eyecare;regression;qt;headless"
    )
endif()

# §9.7 P0 regression test: the signature tool must grow Draw/Type/Upload modes
# that feed the SAME annotation persistence as Draw (typed/uploaded signatures
# persist as real PDF annotations and survive a save/reload round-trip).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignaturePicker.cpp")
    add_executable(TestSignaturePicker tests/TestSignaturePicker.cpp)
    target_include_directories(TestSignaturePicker PRIVATE src tests)
    target_link_libraries(TestSignaturePicker PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSignaturePicker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSignaturePicker>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSignaturePicker"
            )
        endif()
    endif()
    add_test(NAME TestSignaturePicker COMMAND TestSignaturePicker)
    set_tests_properties(TestSignaturePicker PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;signatures;picker;regression;qt;headless"
    )
endif()

# N17: certificate-encryption recipient picker — dialog honesty + engine
# boundary through the dialog's recipient list, verified on the SAVED artifact
# via an independent PoDoFo read and per-recipient CMS FEK recovery.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCertEncryptPicker.cpp")
    add_executable(TestCertEncryptPicker tests/TestCertEncryptPicker.cpp)
    target_include_directories(TestCertEncryptPicker PRIVATE src tests)
    target_link_libraries(TestCertEncryptPicker PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestCertEncryptPicker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestCertEncryptPicker>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestCertEncryptPicker"
            )
        endif()
    endif()
    add_test(NAME TestCertEncryptPicker COMMAND TestCertEncryptPicker)
    set_tests_properties(TestCertEncryptPicker PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;security;encryption;recipients;n17;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# N18: DocMDP certify-vs-approve selector — dialog honesty + engine boundary:
# dialog-selected levels fed to certifyDocument produce saved artifacts whose
# /DocMDP /P (independent fresh-PoDoFo read) equals the requested level, and
# out-of-range levels fail loud with no output.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCertifySelector.cpp")
    add_executable(TestCertifySelector tests/TestCertifySelector.cpp)
    target_include_directories(TestCertifySelector PRIVATE src tests)
    target_link_libraries(TestCertifySelector PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestCertifySelector POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestCertifySelector>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestCertifySelector"
            )
        endif()
    endif()
    add_test(NAME TestCertifySelector COMMAND TestCertifySelector)
    set_tests_properties(TestCertifySelector PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;security;signatures;certify;n18;qt;headless"
    )
endif()

# R24(a): machine-level policy (policy.json overrides bounded user prefs,
# override visible in Preferences). Pins precedence, the readSigningConfig
# consumption seam, the visible managed-row disclosure, the save guard, and
# honest invalid/tampered-policy handling.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPolicyController.cpp")
    add_executable(TestPolicyController tests/TestPolicyController.cpp)
    target_include_directories(TestPolicyController PRIVATE src tests)
    target_link_libraries(TestPolicyController PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestPolicyController COMMAND TestPolicyController)
    set_tests_properties(TestPolicyController PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;policy;r24;qt;headless"
    )
endif()

# R24 wiring closure: the four "enforcement pending" policy keys pinned at
# their REAL enforcement points (startup update check/channel, Ollama
# endpoint gate, OCR download gate) + the network page's policy-aware rows.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPolicyWiring.cpp")
    add_executable(TestPolicyWiring tests/TestPolicyWiring.cpp)
    target_include_directories(TestPolicyWiring PRIVATE src tests)
    target_link_libraries(TestPolicyWiring PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    )
    add_test(NAME TestPolicyWiring COMMAND TestPolicyWiring)
    set_tests_properties(TestPolicyWiring PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;policy;r24;qt;headless"
    )
endif()

# R24 wiring closure: OCSP consent — per-document consent dialog (allow
# once / allow for this document / deny), remember-for-document, and the
# global never-network switch; offscreen dialog-driving pins.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcspConsent.cpp")
    add_executable(TestOcspConsent tests/TestOcspConsent.cpp)
    target_include_directories(TestOcspConsent PRIVATE src tests)
    target_link_libraries(TestOcspConsent PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestOcspConsent COMMAND TestOcspConsent)
    set_tests_properties(TestOcspConsent PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;policy;r24;network;qt;headless"
    )
endif()

# R24(b): redacted support bundle — redaction-by-construction pin (a bundle
# built from a state seeded with usernames/PDF-text/TSA-URL secrets must not
# contain any of them in its bytes), counts-only recents/documents, the
# settings allowlist with disclosed exclusions, capability disclosure without
# probe detail, and the dialog entry point.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSupportBundle.cpp")
    add_executable(TestSupportBundle tests/TestSupportBundle.cpp)
    target_include_directories(TestSupportBundle PRIVATE src tests)
    target_link_libraries(TestSupportBundle PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSupportBundle COMMAND TestSupportBundle)
    set_tests_properties(TestSupportBundle PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;support-bundle;privacy;r24;qt;headless"
    )
endif()

# R24(c): network behavior made explicit — one read-only enumeration of every
# network touchpoint (Ollama/TSA/OCSP/update-check/OCR download) with states
# derived from existing code paths, rendered on a Preferences "Network" page
# and carried (on/off only) into the support bundle.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestNetworkDisclosure.cpp")
    add_executable(TestNetworkDisclosure tests/TestNetworkDisclosure.cpp)
    target_include_directories(TestNetworkDisclosure PRIVATE src tests)
    target_link_libraries(TestNetworkDisclosure PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestNetworkDisclosure COMMAND TestNetworkDisclosure)
    set_tests_properties(TestNetworkDisclosure PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;network-disclosure;privacy;r24;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrAcceptSeam.cpp")
    add_executable(TestOcrAcceptSeam tests/TestOcrAcceptSeam.cpp)
    target_include_directories(TestOcrAcceptSeam PRIVATE src tests)
    target_link_libraries(TestOcrAcceptSeam PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestOcrAcceptSeam COMMAND TestOcrAcceptSeam)
    set_tests_properties(TestOcrAcceptSeam PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;ocr;accept;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSearchFlags.cpp")
    add_executable(TestSearchFlags tests/TestSearchFlags.cpp)
    target_include_directories(TestSearchFlags PRIVATE src tests)
    target_link_libraries(TestSearchFlags PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestSearchFlags COMMAND TestSearchFlags)
    set_tests_properties(TestSearchFlags PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;search;flags;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestExportPresets.cpp")
    add_executable(TestExportPresets tests/TestExportPresets.cpp)
    target_include_directories(TestExportPresets PRIVATE src tests)
    target_link_libraries(TestExportPresets PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestExportPresets COMMAND TestExportPresets)
    set_tests_properties(TestExportPresets PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;export;presets;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOcrAcceptScope.cpp")
    add_executable(TestOcrAcceptScope tests/TestOcrAcceptScope.cpp)
    target_include_directories(TestOcrAcceptScope PRIVATE src tests)
    target_link_libraries(TestOcrAcceptScope PRIVATE
        pdfws_ui pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestOcrAcceptScope COMMAND TestOcrAcceptScope)
    set_tests_properties(TestOcrAcceptScope PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;ocr;accept;honesty;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestViewerLinkSeam.cpp")
    add_executable(TestViewerLinkSeam tests/TestViewerLinkSeam.cpp)
    target_include_directories(TestViewerLinkSeam PRIVATE src tests)
    target_link_libraries(TestViewerLinkSeam PRIVATE
        pdfws_ui pdfws_engines pdfws_core Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
        Qt6::Pdf Qt6::PdfWidgets Qt6::Concurrent
    )
    add_test(NAME TestViewerLinkSeam COMMAND TestViewerLinkSeam)
    set_tests_properties(TestViewerLinkSeam PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;viewing;links;seam;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestHyperlinkNavigation.cpp")
    add_executable(TestHyperlinkNavigation tests/TestHyperlinkNavigation.cpp)
    target_include_directories(TestHyperlinkNavigation PRIVATE src tests)
    target_link_libraries(TestHyperlinkNavigation PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestHyperlinkNavigation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestHyperlinkNavigation>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestHyperlinkNavigation"
    )
    endif()
    add_test(NAME TestHyperlinkNavigation COMMAND TestHyperlinkNavigation)
    set_tests_properties(TestHyperlinkNavigation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;viewing;links;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactMarkAll.cpp")
    add_executable(TestRedactMarkAll tests/TestRedactMarkAll.cpp)
    target_include_directories(TestRedactMarkAll PRIVATE src tests)
    target_link_libraries(TestRedactMarkAll PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestRedactMarkAll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactMarkAll>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactMarkAll"
    )
    endif()
    add_test(NAME TestRedactMarkAll COMMAND TestRedactMarkAll)
    set_tests_properties(TestRedactMarkAll PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;redaction;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactApplyMarks.cpp")
    add_executable(TestRedactApplyMarks tests/TestRedactApplyMarks.cpp)
    target_include_directories(TestRedactApplyMarks PRIVATE src tests)
    target_link_libraries(TestRedactApplyMarks PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
    add_custom_command(TARGET TestRedactApplyMarks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactApplyMarks>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactApplyMarks"
    )
    endif()
    add_test(NAME TestRedactApplyMarks COMMAND TestRedactApplyMarks)
    set_tests_properties(TestRedactApplyMarks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;redaction;security;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAnnotationDjot.cpp")
    add_executable(TestAnnotationDjot
        tests/TestAnnotationDjot.cpp
    )
    target_include_directories(TestAnnotationDjot PRIVATE src tests)
    target_link_libraries(TestAnnotationDjot PRIVATE
        pdfws_engines pdfws_core pdfws_djot podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestAnnotationDjot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestAnnotationDjot>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestAnnotationDjot>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestAnnotationDjot"
    )
    add_test(NAME TestAnnotationDjot COMMAND TestAnnotationDjot)
    set_tests_properties(TestAnnotationDjot PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;annot;djot;richtext;roundtrip;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAiProvider.cpp")
    add_executable(TestAiProvider
        tests/TestAiProvider.cpp
    )
    target_include_directories(TestAiProvider PRIVATE src tests)
    target_link_libraries(TestAiProvider PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Widgets Qt6::Network Qt6::Concurrent
    )
    add_test(NAME TestAiProvider COMMAND TestAiProvider)
    set_tests_properties(TestAiProvider PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
        LABELS "unit;ai;provider;mock;qt;headless"
    )
endif()

# R03/R04 (2026-09-05 plan): OllamaProvider request-lifetime and endpoint-policy
# regression suite. Local QTcpServer stub on the loopback only — no external
# network; the deadline is test-configurable via QSettings ai/ollamaTimeoutMs.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOllamaProvider.cpp")
    add_executable(TestOllamaProvider
        tests/TestOllamaProvider.cpp
    )
    target_include_directories(TestOllamaProvider PRIVATE src tests)
    target_link_libraries(TestOllamaProvider PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Network Qt6::Concurrent
    )
    add_test(NAME TestOllamaProvider COMMAND TestOllamaProvider)
    set_tests_properties(TestOllamaProvider PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;ai;ollama;provider;lifetime;endpoint;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDiffEngine.cpp")
    add_executable(TestDiffEngine
        tests/TestDiffEngine.cpp
    )
    target_include_directories(TestDiffEngine PRIVATE src tests)
    target_link_libraries(TestDiffEngine PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Widgets
    )
    add_test(NAME TestDiffEngine COMMAND TestDiffEngine)
    set_tests_properties(TestDiffEngine PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;diff;lcs;myers;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOfficeImport.cpp")
    add_executable(TestOfficeImport
        tests/TestOfficeImport.cpp
    )
    target_include_directories(TestOfficeImport PRIVATE src tests)
    target_compile_definitions(TestOfficeImport PRIVATE
        $<TARGET_PROPERTY:pdfws_engines,INTERFACE_COMPILE_DEFINITIONS>)
    target_link_libraries(TestOfficeImport PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestOfficeImport COMMAND TestOfficeImport)
    set_tests_properties(TestOfficeImport PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;convert;office;import;images;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMrcPipeline.cpp")
    add_executable(TestMrcPipeline
        tests/TestMrcPipeline.cpp
    )
    target_include_directories(TestMrcPipeline PRIVATE src tests)
    target_compile_definitions(TestMrcPipeline PRIVATE
        $<TARGET_PROPERTY:pdfws_engines,INTERFACE_COMPILE_DEFINITIONS>)
    target_link_libraries(TestMrcPipeline PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_test(NAME TestMrcPipeline COMMAND TestMrcPipeline)
    set_tests_properties(TestMrcPipeline PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;mrc;compression;pdfa;sandwich;qt;headless"
    )
endif()

# ── Fuzz / verification rig (opt-in, NEVER part of the normal build) ─────────
# Set -DGLYPHPDF_FUZZ=ON to wire in fuzz/CMakeLists.txt. Default is OFF so the
# normal cmake --build build + ctest -j4 suite is completely unaffected.
# Use a SEPARATE build directory for fuzz builds — never reuse build/ (the fuzz
# harnesses carry sanitizer flags that would taint the test executables):
#   cmake -S . -B fuzz/build -G Ninja -DGLYPHPDF_FUZZ=ON <same -D flags as main>
#   cmake --build fuzz/build
option(GLYPHPDF_FUZZ "Wire in fuzz/ apparatus (default OFF — never ship)" OFF)
if(GLYPHPDF_FUZZ)
    add_subdirectory(fuzz)
endif()

# ── Translations (Qt Linguist) ───────────────────────────────────────────
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        translations/glyphpdf_ar.ts
        translations/glyphpdf_fr.ts
        translations/glyphpdf_de.ts
    )
    # Generate .qm files from .ts files at build time and embed as Qt resources
    # at :/translations/glyphpdf_<lang>.qm — loaded by main.cpp via QTranslator.
    # RESOURCE_PREFIX requires Qt 6.7+ (we have 6.11).
    qt_add_translations(PdfWorkstation
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/translations"
    )
    message(STATUS "Qt LinguistTools found — translation targets enabled (${TS_FILES})")
else()
    message(STATUS "Qt LinguistTools not found — translation targets disabled")
endif()

# Deployment
# Prefer qt_generate_deploy_app_script for Qt 6.5+
if(QT_VERSION VERSION_GREATER_EQUAL "6.5")
    qt_generate_deploy_app_script(
        TARGET PdfWorkstation
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT ${deploy_script})
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCheckedMutationCoverage.cpp")
    # WP-R03 (WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A02): rotate and inline-text
    # command families under the checked mutation/history seams — fault-injected
    # engines for the failure paths, real-engine saved artifacts for the
    # round-trips, and the production HomeController Undo/Redo seam.
    add_executable(TestCheckedMutationCoverage
        tests/TestCheckedMutationCoverage.cpp
    )
    target_include_directories(TestCheckedMutationCoverage PRIVATE src tests)
    target_link_libraries(TestCheckedMutationCoverage PRIVATE
        pdfws_ui pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestCheckedMutationCoverage COMMAND TestCheckedMutationCoverage)
    set_tests_properties(TestCheckedMutationCoverage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;history;persistence;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestHistoryIntegrity.cpp")
    # Step-3 history truthfulness (EC03/EC05/V02): command-level regression
    # tests over the real engine (saved/reopened artifacts via PoDoFo/PDFium)
    # plus fault-injected engines for the failure paths.
    add_executable(TestHistoryIntegrity
        tests/TestHistoryIntegrity.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestHistoryIntegrity PRIVATE src tests)
    target_link_libraries(TestHistoryIntegrity PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestHistoryIntegrity POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestHistoryIntegrity>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestHistoryIntegrity>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestHistoryIntegrity"
    )
    # L05 (NATIVE-LINUX-READINESS-2026-09-10): podofo DLL staging is Windows-only —
    # Linux links the vendored .so directly (no runtime staging).
    if(WIN32)
        add_custom_command(TARGET TestHistoryIntegrity POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestHistoryIntegrity>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestHistoryIntegrity"
        )
    endif()
    add_test(NAME TestHistoryIntegrity COMMAND TestHistoryIntegrity)
    set_tests_properties(TestHistoryIntegrity PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;history;commands;undo;persistence;qt;headless"
    )
endif()

# Image stacking order + opacity (Wave 2B/2C port): gp::content byte-exact
# edits, then the real engine judged by rendered pixels at the images' overlap.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestImageAppearance.cpp")
    add_executable(TestImageAppearance
        tests/TestImageAppearance.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestImageAppearance PRIVATE src tests)
    target_link_libraries(TestImageAppearance PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf
    )
    add_custom_command(TARGET TestImageAppearance POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestImageAppearance>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestImageAppearance>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestImageAppearance"
    )
    if(WIN32)
        add_custom_command(TARGET TestImageAppearance POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestImageAppearance>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestImageAppearance"
        )
    endif()
    add_test(NAME TestImageAppearance COMMAND TestImageAppearance)
    set_tests_properties(TestImageAppearance PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;editing;image;zorder;opacity;commands;undo;qt;headless"
    )
endif()

# PDF → Word/Excel export (Wave 1A §9.5, ported from feature/editing-parity):
# the output must be what its extension promises — a real OOXML ZIP, or the
# documented HTML/CSV fallback — and a missing input must fail, not "succeed".
# The test sniffs the output bytes because HAS_DUCKX/HAS_OPENXLSX are PRIVATE
# definitions of pdfws_engines.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestOfficeExport.cpp")
    add_executable(TestOfficeExport tests/TestOfficeExport.cpp)
    target_include_directories(TestOfficeExport PRIVATE src tests)
    target_compile_definitions(TestOfficeExport PRIVATE
        $<TARGET_PROPERTY:pdfws_engines,INTERFACE_COMPILE_DEFINITIONS>)
    target_link_libraries(TestOfficeExport PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestOfficeExport POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestOfficeExport>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestOfficeExport"
            )
        endif()
    endif()
    add_test(NAME TestOfficeExport COMMAND TestOfficeExport)
    set_tests_properties(TestOfficeExport PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;convert;office;export;ooxml;qt;headless"
    )
endif()

# Inline text edit style (Wave 2B/2C port): letter spacing, line spacing and
# opacity, judged on the rendered page — including that a translucent edit
# never lets the replaced text show through its white cover.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTextEditStyle.cpp")
    add_executable(TestTextEditStyle tests/TestTextEditStyle.cpp)
    target_include_directories(TestTextEditStyle PRIVATE src tests)
    target_link_libraries(TestTextEditStyle PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestTextEditStyle POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestTextEditStyle>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestTextEditStyle"
            )
        endif()
    endif()
    add_test(NAME TestTextEditStyle COMMAND TestTextEditStyle)
    set_tests_properties(TestTextEditStyle PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;editing;text;opacity;spacing;qt;headless"
    )
endif()

# Eraser on the real engine (Wave 1B port from feature/editing-parity):
# deleteObjectAt removes the text under the click from the SAVED file (judged
# by text extraction), an empty page is a harmless success, a missing page
# fails without touching the file.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestEraserEngine.cpp")
    add_executable(TestEraserEngine tests/TestEraserEngine.cpp)
    target_include_directories(TestEraserEngine PRIVATE src tests)
    target_link_libraries(TestEraserEngine PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestEraserEngine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestEraserEngine>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestEraserEngine"
            )
        endif()
    endif()
    add_test(NAME TestEraserEngine COMMAND TestEraserEngine)
    set_tests_properties(TestEraserEngine PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;editing;eraser;redaction;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPersistenceOutcomes.cpp")
    # Step-3 persistence truthfulness (ARC03/ARC04/V01 + the GUI-held-handle
    # residual): real-MainWindow integration tests over the close/save/dirty
    # boundaries and the in-place write coordination.
    add_executable(TestPersistenceOutcomes
        tests/TestPersistenceOutcomes.cpp
        src/app/Bootstrapper.h  src/app/Bootstrapper.cpp
    )
    target_include_directories(TestPersistenceOutcomes PRIVATE src tests)
    target_compile_definitions(TestPersistenceOutcomes PRIVATE
        DJOT_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/djot"
    )
    target_link_libraries(TestPersistenceOutcomes PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestPersistenceOutcomes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestPersistenceOutcomes>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestPersistenceOutcomes>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestPersistenceOutcomes"
    )
    add_test(NAME TestPersistenceOutcomes COMMAND TestPersistenceOutcomes)
    set_tests_properties(TestPersistenceOutcomes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;session;persistence;identity;integration;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

# ── R12 (PERF-04): render-path profiling harness (tools/) ──────────────────
# Measures the real QPdfView / two-page / thumbnail render paths offscreen and
# records guard behaviour on the named extremes. A measurement tool, NOT a
# ctest gate — numbers are machine-dependent raw samples (see file header).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/render_path_profile.cpp")
    add_executable(render_path_profile
        tools/render_path_profile.cpp
    )
    target_include_directories(render_path_profile PRIVATE src tests)
    target_link_libraries(render_path_profile PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET render_path_profile POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:render_path_profile>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:render_path_profile>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for render_path_profile"
    )
endif()

# ── R12 (PERF-04): render-guard regression tests ───────────────────────────
# Finite scale/dimension checks, the pixel-allocation ceiling, zoom clamps and
# stale-output suppression on the real viewer render path.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRenderGuards.cpp")
    add_executable(TestRenderGuards
        tests/TestRenderGuards.cpp
    )
    target_include_directories(TestRenderGuards PRIVATE src tests)
    target_link_libraries(TestRenderGuards PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestRenderGuards POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestRenderGuards>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestRenderGuards>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestRenderGuards"
    )
    add_test(NAME TestRenderGuards COMMAND TestRenderGuards)
    set_tests_properties(TestRenderGuards PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;render;guards;perf04;qt;headless"
    )
endif()

# ── SEP13-LEADS adversarial-confirmation repros (2026-09-14, review-only lane)
# One block per lead cluster; expected failures are the confirmation evidence.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadCapability.cpp")
    add_executable(TestSep13LeadCapability tests/TestSep13LeadCapability.cpp)
    target_include_directories(TestSep13LeadCapability PRIVATE src tests)
    target_link_libraries(TestSep13LeadCapability PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestSep13LeadCapability POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadCapability>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadCapability>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadCapability"
    )
    add_test(NAME TestSep13LeadCapability COMMAND TestSep13LeadCapability)
    set_tests_properties(TestSep13LeadCapability PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;core;capability;sep13leads;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadBatchMerge.cpp")
    add_executable(TestSep13LeadBatchMerge tests/TestSep13LeadBatchMerge.cpp)
    target_include_directories(TestSep13LeadBatchMerge PRIVATE src tests)
    target_link_libraries(TestSep13LeadBatchMerge PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSep13LeadBatchMerge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadBatchMerge>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadBatchMerge>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadBatchMerge"
    )
    add_test(NAME TestSep13LeadBatchMerge COMMAND TestSep13LeadBatchMerge)
    set_tests_properties(TestSep13LeadBatchMerge PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;modes;batch;sep13leads;qt;headless"
        RESOURCE_LOCK BatchModeIO
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadOcrGuards.cpp")
    add_executable(TestSep13LeadOcrGuards tests/TestSep13LeadOcrGuards.cpp)
    target_include_directories(TestSep13LeadOcrGuards PRIVATE src tests)
    target_link_libraries(TestSep13LeadOcrGuards PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_custom_command(TARGET TestSep13LeadOcrGuards POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadOcrGuards>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadOcrGuards>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadOcrGuards"
    )
    add_test(NAME TestSep13LeadOcrGuards COMMAND TestSep13LeadOcrGuards)
    set_tests_properties(TestSep13LeadOcrGuards PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;modes;ocr;sep13leads;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadConversionExport.cpp")
    add_executable(TestSep13LeadConversionExport tests/TestSep13LeadConversionExport.cpp)
    target_include_directories(TestSep13LeadConversionExport PRIVATE src tests)
    target_link_libraries(TestSep13LeadConversionExport PRIVATE
        pdfws_engines pdfws_core libzip::zip Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSep13LeadConversionExport COMMAND TestSep13LeadConversionExport)
    set_tests_properties(TestSep13LeadConversionExport PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;conversion;sep13leads;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadRedactionProof.cpp")
    add_executable(TestSep13LeadRedactionProof tests/TestSep13LeadRedactionProof.cpp)
    target_include_directories(TestSep13LeadRedactionProof PRIVATE src tests)
    target_link_libraries(TestSep13LeadRedactionProof PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestSep13LeadRedactionProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadRedactionProof>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadRedactionProof>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadRedactionProof"
    )
    add_test(NAME TestSep13LeadRedactionProof COMMAND TestSep13LeadRedactionProof)
    set_tests_properties(TestSep13LeadRedactionProof PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;core;redactionproof;sep13leads;qt;headless"
    )
endif()

# R14 INDEPENDENT REVIEWER probe (temporary registration — restored after review).
# Registration removed at integration: the probe demonstrates the F1 false-PASS and
# now fails BY DESIGN since the F1 fix (7189149) landed; its demonstration slots
# live permanently in TestSep13LeadRedactionProof. Probe source kept unregistered.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/R14ProbeRedactSpace.cpp")
    add_executable(R14ProbeRedactSpace tests/R14ProbeRedactSpace.cpp)
    target_include_directories(R14ProbeRedactSpace PRIVATE src tests)
    target_link_libraries(R14ProbeRedactSpace PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET R14ProbeRedactSpace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:R14ProbeRedactSpace>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:R14ProbeRedactSpace>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for R14ProbeRedactSpace"
    )
endif()

# R14 INDEPENDENT REVIEWER probe 2 (temporary registration — restored after review).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/R14ProbeSep13Fixes.cpp")
    add_executable(R14ProbeSep13Fixes tests/R14ProbeSep13Fixes.cpp)
    set_target_properties(R14ProbeSep13Fixes PROPERTIES OUTPUT_NAME "R14ProbeSep13Fixes-r2") # stale-exe lock workaround
    target_include_directories(R14ProbeSep13Fixes PRIVATE src tests)
    target_compile_definitions(R14ProbeSep13Fixes PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(R14ProbeSep13Fixes PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    add_custom_command(TARGET R14ProbeSep13Fixes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:R14ProbeSep13Fixes>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:R14ProbeSep13Fixes>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for R14ProbeSep13Fixes"
    )
    add_test(NAME R14ProbeSep13Fixes COMMAND R14ProbeSep13Fixes)
    set_tests_properties(R14ProbeSep13Fixes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;core;r14review;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadBtDowngrade.cpp")
    add_executable(TestSep13LeadBtDowngrade tests/TestSep13LeadBtDowngrade.cpp)
    target_include_directories(TestSep13LeadBtDowngrade PRIVATE src tests)
    target_compile_definitions(TestSep13LeadBtDowngrade PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(TestSep13LeadBtDowngrade PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSep13LeadBtDowngrade PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSep13LeadBtDowngrade POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSep13LeadBtDowngrade>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSep13LeadBtDowngrade"
            )
        endif()
    endif()
    add_custom_command(TARGET TestSep13LeadBtDowngrade POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadBtDowngrade>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadBtDowngrade>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadBtDowngrade"
    )
    add_test(NAME TestSep13LeadBtDowngrade COMMAND TestSep13LeadBtDowngrade)
    set_tests_properties(TestSep13LeadBtDowngrade PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;engines;signature;sep13leads;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSep13LeadComparePerf.cpp")
    add_executable(TestSep13LeadComparePerf tests/TestSep13LeadComparePerf.cpp)
    target_include_directories(TestSep13LeadComparePerf PRIVATE src tests)
    target_link_libraries(TestSep13LeadComparePerf PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestSep13LeadComparePerf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestSep13LeadComparePerf>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
            "$<TARGET_FILE_DIR:TestSep13LeadComparePerf>/platforms/${GLYPH_QPA_PLUGIN}"
        COMMENT "Deploying qoffscreen platform plugin for TestSep13LeadComparePerf"
    )
    add_test(NAME TestSep13LeadComparePerf COMMAND TestSep13LeadComparePerf)
    set_tests_properties(TestSep13LeadComparePerf PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;modes;compare;perf;sep13leads;qt;headless"
    )
endif()

# ── SWEEP-W1 ADVERSARY (2026-09-19) — failing repros + probes ────────────────
# REVIEW-ONLY round: these suites prove (or refute) the sweep's hostile-input
# findings. Tests are expected to FAIL until the corresponding defect is
# fixed; probes pin defended behavior. Do not "fix the tests" — fix the code.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSweepW1PresetAdversary.cpp")
    add_executable(TestSweepW1PresetAdversary tests/TestSweepW1PresetAdversary.cpp)
    target_include_directories(TestSweepW1PresetAdversary PRIVATE src tests)
    target_link_libraries(TestSweepW1PresetAdversary PRIVATE
        pdfws_ui pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui
    )
    add_test(NAME TestSweepW1PresetAdversary COMMAND TestSweepW1PresetAdversary)
    set_tests_properties(TestSweepW1PresetAdversary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;batch;presets;adversary;sweep-w1;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSweepW1SigningAdversary.cpp")
    add_executable(TestSweepW1SigningAdversary tests/TestSweepW1SigningAdversary.cpp)
    target_include_directories(TestSweepW1SigningAdversary PRIVATE src tests)
    target_compile_definitions(TestSweepW1SigningAdversary PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestSweepW1SigningAdversary PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestSweepW1SigningAdversary PRIVATE
        pdfws_engines pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSweepW1SigningAdversary PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSweepW1SigningAdversary POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSweepW1SigningAdversary>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSweepW1SigningAdversary"
            )
        endif()
    endif()
    add_test(NAME TestSweepW1SigningAdversary COMMAND TestSweepW1SigningAdversary)
    set_tests_properties(TestSweepW1SigningAdversary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        LABELS "integration;security;signatures;workflow;adversary;sweep-w1;qt;headless"
        RESOURCE_LOCK GlyphpdfCandidates
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSweepW1SummaryPolicyAdversary.cpp")
    add_executable(TestSweepW1SummaryPolicyAdversary tests/TestSweepW1SummaryPolicyAdversary.cpp)
    target_include_directories(TestSweepW1SummaryPolicyAdversary PRIVATE src tests)
    target_link_libraries(TestSweepW1SummaryPolicyAdversary PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSweepW1SummaryPolicyAdversary POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSweepW1SummaryPolicyAdversary>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSweepW1SummaryPolicyAdversary"
            )
        endif()
    endif()
    add_test(NAME TestSweepW1SummaryPolicyAdversary COMMAND TestSweepW1SummaryPolicyAdversary)
    set_tests_properties(TestSweepW1SummaryPolicyAdversary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 300
        LABELS "unit;engines;summary;policy;adversary;sweep-w1;qt;headless"
    )
endif()

# ── SWEEP-W1 SECURITY (2026-09-20) — failing repros from the security lane ───
# REVIEW-ONLY probe: asserts the DESIRED security behavior from
# docs/audit/SWEEP-W1-SECURITY-2026-09-20.md. Slots FAIL until the finding is
# remediated (S4/F1 is the documented design item). Do not "fix the tests".
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/SweepW1SecProbe.cpp")
    add_executable(TestSweepW1SecProbe tests/SweepW1SecProbe.cpp)
    target_include_directories(TestSweepW1SecProbe PRIVATE src tests)
    target_compile_definitions(TestSweepW1SecProbe PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(GLYPHPDF_ENABLE_TEST_FIXTURES)
        target_compile_definitions(TestSweepW1SecProbe PRIVATE GLYPH_TESTING=1)
    endif()
    target_link_libraries(TestSweepW1SecProbe PRIVATE
        pdfws_engines pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(TestSweepW1SecProbe PRIVATE Crypt32)
        if(EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET TestSweepW1SecProbe POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:TestSweepW1SecProbe>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for TestSweepW1SecProbe"
            )
        endif()
    endif()
    add_test(NAME TestSweepW1SecProbe COMMAND TestSweepW1SecProbe)
    set_tests_properties(TestSweepW1SecProbe PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 240
        LABELS "integration;security;signatures;workflow;security-sweep-w1;qt;headless"
    )
endif()

# ── SWEEP-W2B verification probes (guarantee-verification-engine, W2 wave -b).
# Independent read-path probes over the sweep-fix rows; not part of the
# default ctest gates, run manually per docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeNaming.cpp")
    add_executable(W2BProbeNaming tests/W2BProbeNaming.cpp)
    target_include_directories(W2BProbeNaming PRIVATE src tests)
    target_link_libraries(W2BProbeNaming PRIVATE
        pdfws_engines pdfws_core Qt6::Test Qt6::Core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeSigning.cpp")
    add_executable(W2BProbeSigning tests/W2BProbeSigning.cpp)
    target_include_directories(W2BProbeSigning PRIVATE src tests)
    target_compile_definitions(W2BProbeSigning PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(W2BProbeSigning PRIVATE
        pdfws_engines pdfws_ui pdfws_engines pdfws_core
        OpenSSL::SSL OpenSSL::Crypto
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
    if(WIN32)
        target_link_libraries(W2BProbeSigning PRIVATE Crypt32)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeSummaryPolicy.cpp")
    add_executable(W2BProbeSummaryPolicy tests/W2BProbeSummaryPolicy.cpp)
    target_include_directories(W2BProbeSummaryPolicy PRIVATE src tests)
    target_compile_definitions(W2BProbeSummaryPolicy PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(W2BProbeSummaryPolicy PRIVATE
        pdfws_engines pdfws_ui pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets podofo::podofo
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeA11y.cpp")
    add_executable(W2BProbeA11y tests/W2BProbeA11y.cpp)
    target_include_directories(W2BProbeA11y PRIVATE src tests)
    target_compile_definitions(W2BProbeA11y PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(W2BProbeA11y PRIVATE
        pdfws_engines pdfws_core Qt6::Test Qt6::Core podofo::podofo
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeLegacySpace.cpp")
    add_executable(W2BProbeLegacySpace tests/W2BProbeLegacySpace.cpp)
    target_include_directories(W2BProbeLegacySpace PRIVATE src tests)
    target_link_libraries(W2BProbeLegacySpace PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2BProbeOfficeSave.cpp")
    add_executable(W2BProbeOfficeSave tests/W2BProbeOfficeSave.cpp)
    target_include_directories(W2BProbeOfficeSave PRIVATE src tests)
    target_compile_definitions(W2BProbeOfficeSave PRIVATE
        $<TARGET_PROPERTY:pdfws_engines,INTERFACE_COMPILE_DEFINITIONS>)
    target_link_libraries(W2BProbeOfficeSave PRIVATE
        pdfws_engines pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
endif()

# Runtime deployment: podofo DLL + the offscreen platform plugin for every
# W2B probe target that links a GUI-capable Qt.
foreach(_w2b_probe
        W2BProbeNaming W2BProbeSigning W2BProbeSummaryPolicy
        W2BProbeA11y W2BProbeLegacySpace W2BProbeOfficeSave)
    if(TARGET ${_w2b_probe})
        if(WIN32 AND EXISTS "${PODOFO_VENDOR_DLL}")
            add_custom_command(TARGET ${_w2b_probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PODOFO_VENDOR_DLL}"
                    "$<TARGET_FILE_DIR:${_w2b_probe}>/libpodofo.dll"
                COMMENT "Deploying podofo 1.1.0 DLL for ${_w2b_probe}"
            )
        endif()
        if(WIN32 AND QT6_PLUGINS_DIR AND GLYPH_QPA_PLUGIN AND NOT _w2b_probe STREQUAL "W2BProbeNaming")
            add_custom_command(TARGET ${_w2b_probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${_w2b_probe}>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
                    "$<TARGET_FILE_DIR:${_w2b_probe}>/platforms/${GLYPH_QPA_PLUGIN}"
                COMMENT "Deploying qoffscreen platform plugin for ${_w2b_probe}"
            )
        endif()
    endif()
endforeach()

# ── W2B-1 (sweep-w2b-verify 2026-09-20): the /Rotate page-space law re-pinned ──
# All four rotations + offset origins through every consumer of
# gp::PageSpace::pageGeometry (annotation embed/extract, form fields, the F5
# signature containment, SL3 suggestion bounds) + the GetMediaBox-vs-file-bytes
# root-cause pin. The committed fixtures were rotation-blind (rot {0,90}); the
# /Rotate 270 shape stored transposed rects (see docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRotate270PageSpace.cpp")
    add_executable(TestRotate270PageSpace tests/TestRotate270PageSpace.cpp)
    target_include_directories(TestRotate270PageSpace PRIVATE src tests)
    target_link_libraries(TestRotate270PageSpace PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        add_custom_command(TARGET TestRotate270PageSpace POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestRotate270PageSpace>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestRotate270PageSpace"
        )
    endif()
    add_test(NAME TestRotate270PageSpace COMMAND TestRotate270PageSpace)
    set_tests_properties(TestRotate270PageSpace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 LABELS "unit;annotations;forms;persistence;qt;headless")
endif()

# ── SWEEP-W2-GSD (gsd-verifier W2 pass, 2026-09-21): the verifier's OWN
# independent probes for the 10 implemented-awaiting-review rows (EM-1..EM-6,
# the sanitize UAF fix, the ri-fix, F2b-D1, the timestampAttempted three-state).
# One slot per row, each with its own fixture values / harness shape / entry
# path — docs/audit/SWEEP-W2-GSD-2026-09-21.md §Method.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestGsdW2Probe.cpp")
    add_executable(TestGsdW2Probe tests/TestGsdW2Probe.cpp)
    target_include_directories(TestGsdW2Probe PRIVATE src tests)
    target_link_libraries(TestGsdW2Probe PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    )
    if(WIN32)
        add_custom_command(TARGET TestGsdW2Probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:TestGsdW2Probe>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for TestGsdW2Probe"
        )
    endif()
    add_test(NAME TestGsdW2Probe COMMAND TestGsdW2Probe)
    set_tests_properties(TestGsdW2Probe PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300 LABELS "unit;gsd;probe;qt;headless")
endif()

# ── SWEEP-W2C (guarantee-verification-engine, 2026-09-20): independent probe
# for the W2B-1 re-submission — move/resize through the production edit path,
# form-field in-place move, F5 containment honesty on the offset 270 shape,
# SEP13 L5/L8 on an offset-270 fixture (incl. the wrong-region honesty leg),
# and the signature-field verbatim /Rect store. Registered at the tail like
# the W2B probes; run manually, not a ctest member.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/W2CProbeRotate270.cpp")
    add_executable(W2CProbeRotate270 tests/W2CProbeRotate270.cpp)
    target_include_directories(W2CProbeRotate270 PRIVATE src tests)
    target_link_libraries(W2CProbeRotate270 PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        add_custom_command(TARGET W2CProbeRotate270 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PODOFO_VENDOR_DLL}"
                "$<TARGET_FILE_DIR:W2CProbeRotate270>/libpodofo.dll"
            COMMENT "Deploying podofo 1.1.0 DLL for W2CProbeRotate270"
        )
        if(QT6_PLUGINS_DIR AND GLYPH_QPA_PLUGIN)
            add_custom_command(TARGET W2CProbeRotate270 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:W2CProbeRotate270>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT6_PLUGINS_DIR}/platforms/${GLYPH_QPA_PLUGIN}"
                    "$<TARGET_FILE_DIR:W2CProbeRotate270>/platforms/${GLYPH_QPA_PLUGIN}"
                COMMENT "Deploying qoffscreen platform plugin for W2CProbeRotate270"
            )
        endif()
    endif()
endif()

# ── Per-test temp roots ─────────────────────────────────────────────────────
# SafeSave stages every write under <QDir::tempPath()>/glyphpdf-candidates, and
# several suites prove "no candidate left behind" by listing that directory
# (after sweeping debris at start). With one shared temp root, a parallel ctest
# run let those suites delete each other's in-flight candidates and count each
# other's transient files: TestFormSafety, TestEngineSave,
# TestEncryptedPackageSafeWrite and TestWelcomeRoutes failed under -j6 while
# passing alone. Every test gets its own temp root (TMP/TEMP on Windows,
# TMPDIR elsewhere), so the candidate directory is per test process.
get_property(_gp_all_tests DIRECTORY PROPERTY TESTS)
foreach(_gp_test IN LISTS _gp_all_tests)
    set(_gp_test_tmp "${CMAKE_BINARY_DIR}/test-tmp/${_gp_test}")
    file(MAKE_DIRECTORY "${_gp_test_tmp}")
    set_property(TEST ${_gp_test} APPEND PROPERTY ENVIRONMENT
        "TMP=${_gp_test_tmp}" "TEMP=${_gp_test_tmp}" "TMPDIR=${_gp_test_tmp}")
endforeach()
