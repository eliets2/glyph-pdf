cmake_minimum_required(VERSION 3.16)
project(PdfWorkstation VERSION 1.0.0 LANGUAGES CXX)

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
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter)
if(MSVC)
    add_compile_options(/we4715 /we4834 /we4456) # return-type, nodiscard, shadow equivalents
else()
    add_compile_options(-Werror=return-type -Werror=unused-result -Werror=shadow)
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

# LibreOffice headless — optional dep for Office→PDF import.
# Not a hard requirement; graceful runtime no-op when absent.
find_program(LIBREOFFICE_SOFFICE soffice
    PATHS
        "C:/Program Files/LibreOffice/program"
        "C:/Program Files (x86)/LibreOffice/program"
    DOC "LibreOffice headless soffice binary for Office→PDF conversion")
if(LIBREOFFICE_SOFFICE)
    message(STATUS "LibreOffice found at ${LIBREOFFICE_SOFFICE} — Office→PDF import enabled")
else()
    message(STATUS "LibreOffice not found — Office→PDF import will gracefully no-op at runtime")
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Pdfium QUIET)
find_package(libzip REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PKG_TESSERACT QUIET tesseract)
    pkg_check_modules(PKG_LEPTONICA QUIET lept)
endif()

find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(PKG_ONNXRUNTIME QUIET libonnxruntime)
endif()

# MuPDF + Poppler FATAL_ERROR guards (license: AGPL-3.0 + GPL-2.0+ incompatible)
if(TARGET mupdf OR TARGET libmupdf OR TARGET mupdf-pkg)
    message(FATAL_ERROR "MuPDF detected (AGPL-3.0). Use PDFium for rendering.")
endif()
if(TARGET poppler OR TARGET poppler-cpp OR TARGET libpoppler)
    message(FATAL_ERROR "Poppler detected (GPL-2.0+). Use PDFium for rendering.")
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
    src/core/TempFileManager.h       src/core/TempFileManager.cpp
    src/core/CredentialManager.h     src/core/CredentialManager.cpp
    src/engines/OcrEngine.h          src/engines/OcrEngine.cpp
    src/engines/PdfEditorEngine.h    src/engines/PdfEditorEngine.cpp
    src/engines/FormManager.h        src/engines/FormManager.cpp
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
    src/core/interfaces/IPdfPage.h
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
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENJPEG2 QUIET libopenjp2)
    if(OPENJPEG2_FOUND)
        target_include_directories(pdfws_engines PUBLIC ${OPENJPEG2_INCLUDE_DIRS})
        target_link_directories(pdfws_engines PUBLIC ${OPENJPEG2_LIBRARY_DIRS})
        target_link_libraries(pdfws_engines PUBLIC ${OPENJPEG2_LIBRARIES})
        target_compile_definitions(pdfws_engines PUBLIC HAS_OPENJPEG2)
        message(STATUS "OpenJPEG2 found: ${OPENJPEG2_VERSION} — MRC JPEG2000 encoding enabled")
    else()
        message(STATUS "OpenJPEG2 not found — MRC JPEG2000 encoding will be disabled")
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
if(OpenXLSX_FOUND)
    target_link_libraries(pdfws_engines PRIVATE OpenXLSX::OpenXLSX)
    target_compile_definitions(pdfws_engines PRIVATE HAS_OPENXLSX)
endif()
if(duckx_FOUND)
    target_link_libraries(pdfws_engines PRIVATE duckx::duckx)
    target_compile_definitions(pdfws_engines PRIVATE HAS_DUCKX)
endif()
if(LIBREOFFICE_SOFFICE)
    target_compile_definitions(pdfws_engines PRIVATE
        HAS_LIBREOFFICE=1
        LIBREOFFICE_SOFFICE_PATH="${LIBREOFFICE_SOFFICE}")
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

# veraPDF CLI — AGPL-3.0 subprocess only; never link in-process
set(VERAPDF_CLI_PATH "" CACHE FILEPATH "Path to veraPDF CLI executable (AGPL-3.0; subprocess only)")
if(VERAPDF_CLI_PATH)
    target_compile_definitions(pdfws_engines PRIVATE
        VERAPDF_CLI_PATH="${VERAPDF_CLI_PATH}")
    message(STATUS "veraPDF CLI: ${VERAPDF_CLI_PATH}")
else()
    message(STATUS "veraPDF CLI not configured. PdfAValidationPanel will show 'validator unavailable'.")
endif()

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
    src/commands/ReorderPageCommand.h
    src/commands/ReorderPageCommand.cpp
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
    src/ui/RedactCommand.h           src/ui/RedactCommand.cpp
    src/ui/WelcomeWidget.h           src/ui/WelcomeWidget.cpp
    src/ui/FindBar.h                 src/ui/FindBar.cpp
    src/ui/AnnotationToolBar.h       src/ui/AnnotationToolBar.cpp
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
    src/ui/MetadataDialog.h          src/ui/MetadataDialog.cpp
    src/ui/AnnotationLayer.h         src/ui/AnnotationLayer.cpp
    src/ui/ThumbnailSidebar.h        src/ui/ThumbnailSidebar.cpp
    src/ui/ShortcutHelpDialog.h      src/ui/ShortcutHelpDialog.cpp
    src/ui/PreferencesDialog.h       src/ui/PreferencesDialog.cpp
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
    src/shell/Sidebar.h                     src/shell/Sidebar.cpp
    src/modes/ModeController.h              src/modes/ModeController.cpp
    src/modes/OCRMode.h                     src/modes/OCRMode.cpp
    src/modes/RedactMode.h                  src/modes/RedactMode.cpp
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

# Windows: hide console
if(WIN32 AND NOT MSVC)
    target_link_options(PdfWorkstation PRIVATE "-Wl,-subsystem,windows")
endif()

# Windows: embed the application icon into the executable (Explorer/taskbar file icon)
if(WIN32)
    enable_language(RC)
    target_sources(PdfWorkstation PRIVATE src/app/app_icon.rc)
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
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
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
        Qt6::Test Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets
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
        TIMEOUT 120
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
        TIMEOUT 60
        LABELS "unit;modes;batch;ui;qt;headless"
        RESOURCE_LOCK BatchModeIO
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
    if(LIBREOFFICE_SOFFICE)
        target_compile_definitions(TestOfficeImport PRIVATE
            HAS_LIBREOFFICE=1
            LIBREOFFICE_SOFFICE_PATH="${LIBREOFFICE_SOFFICE}")
    endif()
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

