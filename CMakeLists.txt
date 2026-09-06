cmake_minimum_required(VERSION 3.16)
project(PdfWorkstation VERSION 1.3.2.3 LANGUAGES CXX)

# MSYS2 ucrt64 — single coherent toolchain + Qt6 + all deps via pacman.
# Explicit prefix paths so CMake resolves packages regardless of calling shell.
if(NOT DEFINED MSYS2_ROOT)
    set(MSYS2_ROOT "C:/msys64/ucrt64")
endif()
list(APPEND CMAKE_PREFIX_PATH "${MSYS2_ROOT}")
list(APPEND CMAKE_PREFIX_PATH "${MSYS2_ROOT}/share")
list(APPEND CMAKE_MODULE_PATH "${MSYS2_ROOT}/lib/cmake")
# Restrict pkg-config to ucrt64 only so mingw64 headers don't leak in.
set(ENV{PKG_CONFIG_PATH} "${MSYS2_ROOT}/lib/pkgconfig:${MSYS2_ROOT}/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${MSYS2_ROOT}/lib/pkgconfig:${MSYS2_ROOT}/share/pkgconfig")
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
    # AR-11 D4: PE security flags (Windows ASLR/DEP/HEASLR) — Release/RelWithDebInfo.
    # --dynamicbase  → IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE (ASLR)
    # --nxcompat     → IMAGE_DLLCHARACTERISTICS_NX_COMPAT (DEP/NX)
    # --high-entropy-va → 64-bit HEASLR (requires --dynamicbase)
    add_link_options(
        "$<$<NOT:$<CONFIG:Debug>>:-Wl,--dynamicbase,--nxcompat,--high-entropy-va>"
    )
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
    find_program(STRIP_PROGRAM strip
        PATHS "${MSYS2_ROOT}/bin" /c/msys64/ucrt64/bin
        NO_DEFAULT_PATH)
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

# Vendored podofo 1.1.0 (built from source with ucrt64 toolchain).
# Prepend to CMAKE_PREFIX_PATH so it is found before MSYS2 ucrt64's 0.10.4.
set(_podofo_vendor "${CMAKE_CURRENT_SOURCE_DIR}/third_party/podofo/install")
if(EXISTS "${_podofo_vendor}/lib/cmake/podofo/podofo-config.cmake" AND
   EXISTS "${_podofo_vendor}/bin/libpodofo.dll")
    list(PREPEND CMAKE_PREFIX_PATH "${_podofo_vendor}")
    message(STATUS "Using vendored podofo 1.1.0 from ${_podofo_vendor}")
endif()
# Path to the vendored podofo 1.1.0 DLL — deployed next to test executables
# so the runtime loader picks it up instead of the system 0.10.4 DLL.
set(PODOFO_VENDOR_DLL "${_podofo_vendor}/bin/libpodofo.dll")

find_package(podofo CONFIG REQUIRED)
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

# LibreOffice headless (Office→PDF import) is detected at RUNTIME by
# ConversionManager::locateSoffice() — never baked at build time, so the shipped
# binary works on whatever machine the user has. No build-time probe needed.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Pdfium QUIET)
find_package(libzip REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PKG_TESSERACT QUIET tesseract)
    pkg_check_modules(PKG_LEPTONICA QUIET lept)
endif()

# Robustness fallback (CI fix): in minimal environments pkg-config can fail to
# resolve tesseract.pc / lept.pc even when libtesseract + leptonica and their
# headers ARE installed (the .pc may be absent from the package or its transitive
# Requires.private graph is unsatisfiable). Probe the library + headers directly so
# OCR (HAS_TESSERACT) is enabled wherever Tesseract is genuinely present, instead of
# silently shipping an OCR-less build. find_library/find_path do not depend on .pc.
if(NOT PKG_TESSERACT_FOUND)
    find_library(_TESS_LIB NAMES tesseract libtesseract tesseract55 tesseract54 tesseract-5)
    find_path(_TESS_INC NAMES tesseract/capi.h tesseract/baseapi.h)
    if(_TESS_LIB AND _TESS_INC)
        set(PKG_TESSERACT_FOUND TRUE)
        set(PKG_TESSERACT_LIBRARIES ${_TESS_LIB})
        set(PKG_TESSERACT_INCLUDE_DIRS ${_TESS_INC})
        message(STATUS "Tesseract found via find_library fallback: ${_TESS_LIB}")
    endif()
endif()
if(NOT PKG_LEPTONICA_FOUND)
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
    src/core/TempFileManager.h       src/core/TempFileManager.cpp
    src/core/CredentialManager.h     src/core/CredentialManager.cpp
    src/core/ISecretStore.h
    src/core/EncryptedFileSecretStore.h src/core/EncryptedFileSecretStore.cpp
    src/engines/OcrEngine.h          src/engines/OcrEngine.cpp
    src/engines/PdfEditorEngine.h    src/engines/PdfEditorEngine.cpp
    src/engines/FormManager.h        src/engines/FormManager.cpp
    src/engines/SafeSave.h           src/engines/SafeSave.cpp
    src/engines/RedactOperation.h    src/engines/RedactOperation.cpp
    src/engines/SignatureManager.h   src/engines/SignatureManager.cpp
    src/engines/ConversionManager.h  src/engines/ConversionManager.cpp
    src/engines/DocumentSession.h    src/engines/DocumentSession.cpp
    src/engines/AutosaveManager.h    src/engines/AutosaveManager.cpp
    src/engines/BackendRouter.h      src/engines/BackendRouter.cpp
    src/engines/PatternRedactor.h       src/engines/PatternRedactor.cpp
    src/engines/podofo/PdfStringEscape.h src/engines/podofo/PdfStringEscape.cpp
    src/engines/podofo/GlyphAdvanceCalculator.h src/engines/podofo/GlyphAdvanceCalculator.cpp
    src/engines/podofo/PoDoFoBackend.h src/engines/podofo/PoDoFoBackend.cpp
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-1.17.3")
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

message(STATUS "")
message(STATUS "===== GlyphPDF ${PROJECT_VERSION} — Feature Summary (AR-11 D5) =====")
message(STATUS "  HAS_PDFIUM    : ${_has_pdfium}   (rendering engine — PDF.js-speed rendering)")
message(STATUS "  HAS_TESSERACT : ${_has_tesseract}  (primary OCR engine — Tesseract 5)")
message(STATUS "  HAS_RAPIDOCR  : ${_has_rapidocr}  (secondary OCR — PP-OCRv5 ROVER ensemble)")
message(STATUS "  HAS_QPDF      : ${_has_qpdf}   (PDF linearisation)")
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
        list(APPEND _missing_features "HAS_TESSERACT (Tesseract not found)")
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

# ── Stage runtime DLLs beside test/app binaries ─────────────────────────────
# Test executables live in CMAKE_BINARY_DIR; a fresh checkout has none of the
# non-MSYS2 runtime DLLs (vendored podofo, ONNX Runtime, PDFium), so stage
# them once here. Windows resolves exe-dir DLLs before System32, which also
# shields tests from the incompatible onnxruntime.dll Windows ships there.
set(_runtime_dlls "")
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

    src/ui/WelcomeWidget.h           src/ui/WelcomeWidget.cpp
    src/ui/FindBar.h                 src/ui/FindBar.cpp
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
    src/ui/SignaturePicker.h         src/ui/SignaturePicker.cpp
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
    add_custom_command(TARGET TestExpiryInterface POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestExpiryInterface>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestExpiryInterface"
    )
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
    target_link_libraries(TestSecretStore PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui
    )
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
        pdfws_engines pdfws_core
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestRapidOcr>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestOcrReviewLifecycle>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestOcrVerifyNavigation>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestControllers>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestRibbonIntegrity>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestMenuBarIntegrity>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestMenuBarIntegrity"
    )
    add_test(NAME TestMenuBarIntegrity COMMAND TestMenuBarIntegrity)
    set_tests_properties(TestMenuBarIntegrity PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;menubar;integrity;ui;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFormBuilder>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFormBuilder>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormBuilder"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFormPersistence>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFormPersistence>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormPersistence"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestCompareEntry>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestCompareEntry"
    )
    add_test(NAME TestCompareEntry COMMAND TestCompareEntry)
    set_tests_properties(TestCompareEntry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;modes;compare;ui;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestPagesMode>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestPagesMode"
    )
    add_test(NAME TestPagesMode COMMAND TestPagesMode)
    set_tests_properties(TestPagesMode PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;modes;pages;ui;qt;headless"
    )
endif()

# §9.9 P1 groundwork: pure /PageLabels seam — number-tree entries + label
# strings (Decimal/UppercaseRoman/LowercaseRoman/letters) for a future
# Page-Labels mode. Compiles its own copy of the seam (TestUpdateChecker
# precedent); no document I/O, no GUI, no offscreen plugin needed.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestPageLabels.cpp")
    add_executable(TestPageLabels
        tests/TestPageLabels.cpp
        src/core/PageLabels.h  src/core/PageLabels.cpp
    )
    target_include_directories(TestPageLabels PRIVATE src tests)
    target_link_libraries(TestPageLabels PRIVATE
        Qt6::Test Qt6::Core
    )
    add_test(NAME TestPageLabels COMMAND TestPageLabels)
    set_tests_properties(TestPageLabels PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
        LABELS "unit;pages;pagelabels;core;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestBatchMode>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestBatchOcrLanguage>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOcrLanguage"
    )
    add_test(NAME TestBatchOcrLanguage COMMAND TestBatchOcrLanguage)
    set_tests_properties(TestBatchOcrLanguage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;batch;ocr;language;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestOcrPreprocessPrefs>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestOcrPreprocessPrefs"
    )
    add_test(NAME TestOcrPreprocessPrefs COMMAND TestOcrPreprocessPrefs)
    set_tests_properties(TestOcrPreprocessPrefs PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;ocr;preprocess;persistence;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestBatchOpsCoverage>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestBatchOpsCoverage>"
        COMMENT "Deploying qoffscreen platform plugin for TestBatchOpsCoverage"
    )
    add_test(NAME TestBatchOpsCoverage COMMAND TestBatchOpsCoverage)
    set_tests_properties(TestBatchOpsCoverage PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;batch;watermark;pdfa;merge;redact;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestReadingOrderThreshold>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestLinkBookmarkRoundTrip>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestSignatureSessionCache>/platforms/qoffscreen.dll"
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
    target_link_libraries(TestCapabilityRegistry PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestCapabilityRegistry POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestCapabilityRegistry>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestCapabilityRegistry>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestCapabilityRegistry"
    )
    add_test(NAME TestCapabilityRegistry COMMAND TestCapabilityRegistry)
    set_tests_properties(TestCapabilityRegistry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;capability;u08;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestTaskNavRegistry>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestScreenStateSync>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestRibbonCollapse>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestRibbonCollapse"
    )
    add_test(NAME TestRibbonCollapse COMMAND TestRibbonCollapse)
    set_tests_properties(TestRibbonCollapse PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;shell;ribbon;collapse;u02;qt;headless"
    )
endif()

# ── TestStatusBarSlim (U02 — slim status bar + details facts) ───────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestStatusBarSlim.cpp")
    add_executable(TestStatusBarSlim
        tests/TestStatusBarSlim.cpp
    )
    target_include_directories(TestStatusBarSlim PRIVATE src tests)
    target_link_libraries(TestStatusBarSlim PRIVATE
        pdfws_ui pdfws_engines pdfws_commands pdfws_core podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
        Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestStatusBarSlim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestStatusBarSlim>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestStatusBarSlim>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestCommentsReview>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestWelcomeLayout>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestSignatureBadges>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestBatchOcrConfidence>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestInspector>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestPatternRedact>/platforms/qoffscreen.dll"
        COMMENT "Deploying qoffscreen platform plugin for TestPatternRedact"
    )
    add_test(NAME TestPatternRedact COMMAND TestPatternRedact)
    set_tests_properties(TestPatternRedact PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;redaction;patterns;qt;headless"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestDjotRoundtrip.cpp")
    add_executable(TestDjotRoundtrip
        tests/TestDjotRoundtrip.cpp
    )
    target_include_directories(TestDjotRoundtrip PRIVATE src tests)
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestWatermarkFont>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFillFormLock>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFillFormLock>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFillFormLock"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFillFormNoOp>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFillFormNoOp>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFillFormNoOp"
    )
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
    add_custom_command(TARGET TestShapeInkPersistence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestShapeInkPersistence>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestShapeInkPersistence"
    )
    add_test(NAME TestShapeInkPersistence COMMAND TestShapeInkPersistence)
    set_tests_properties(TestShapeInkPersistence PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;annotations;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestCompressStripSanitize.cpp")
    add_executable(TestCompressStripSanitize tests/TestCompressStripSanitize.cpp)
    target_include_directories(TestCompressStripSanitize PRIVATE src tests)
    target_link_libraries(TestCompressStripSanitize PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestCompressStripSanitize POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestCompressStripSanitize>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestCompressStripSanitize"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestCompressDialogHonesty>/platforms/qoffscreen.dll"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFieldMetadata>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFieldMetadata>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFieldMetadata"
    )
    add_test(NAME TestFieldMetadata COMMAND TestFieldMetadata)
    set_tests_properties(TestFieldMetadata PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;forms;form-fields;persistence;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFormSafety.cpp")
    add_executable(TestFormSafety
        tests/TestFormSafety.cpp
        src/engines/DocumentSession.cpp
    )
    target_include_directories(TestFormSafety PRIVATE src tests)
    target_link_libraries(TestFormSafety PRIVATE
        pdfws_engines pdfws_core pdfws_commands podofo::podofo
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestFormSafety POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TestFormSafety>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFormSafety>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFormSafety>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormSafety"
    )
    add_test(NAME TestFormSafety COMMAND TestFormSafety)
    set_tests_properties(TestFormSafety PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 LABELS "unit;forms;form-fields;persistence;qt;headless")
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestFormUndo>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestFormUndo>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestFormUndo"
    )
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
    add_custom_command(TARGET TestRedactSanitizeBundle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactSanitizeBundle>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactSanitizeBundle"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestRedactTransaction>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactTransaction>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo DLL for TestRedactTransaction"
    )
    add_test(NAME TestRedactTransaction COMMAND TestRedactTransaction)
    set_tests_properties(TestRedactTransaction PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 180
        LABELS "unit;redaction;transaction;security;privacy;qt;headless"
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestAutoDetectHeuristic>/platforms/qoffscreen.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestAutoDetectHeuristic>/libpodofo.dll"
        COMMENT "Deploying qoffscreen platform plugin and podofo 1.1.0 DLL for TestAutoDetectHeuristic"
    )
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
    add_custom_command(TARGET TestHyperlinkNavigation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestHyperlinkNavigation>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestHyperlinkNavigation"
    )
    add_test(NAME TestHyperlinkNavigation COMMAND TestHyperlinkNavigation)
    set_tests_properties(TestHyperlinkNavigation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;viewing;links;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactMarkAll.cpp")
    add_executable(TestRedactMarkAll tests/TestRedactMarkAll.cpp)
    target_include_directories(TestRedactMarkAll PRIVATE src tests)
    target_link_libraries(TestRedactMarkAll PRIVATE
        pdfws_ui pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
    )
    add_custom_command(TARGET TestRedactMarkAll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactMarkAll>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactMarkAll"
    )
    add_test(NAME TestRedactMarkAll COMMAND TestRedactMarkAll)
    set_tests_properties(TestRedactMarkAll PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60 LABELS "unit;redaction;qt;headless")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRedactApplyMarks.cpp")
    add_executable(TestRedactApplyMarks tests/TestRedactApplyMarks.cpp)
    target_include_directories(TestRedactApplyMarks PRIVATE src tests)
    target_link_libraries(TestRedactApplyMarks PRIVATE
        pdfws_engines pdfws_core podofo::podofo Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_custom_command(TARGET TestRedactApplyMarks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PODOFO_VENDOR_DLL}"
            "$<TARGET_FILE_DIR:TestRedactApplyMarks>/libpodofo.dll"
        COMMENT "Deploying podofo 1.1.0 DLL for TestRedactApplyMarks"
    )
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
            "${QT6_PLUGINS_DIR}/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:TestAnnotationDjot>/platforms/qoffscreen.dll"
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

