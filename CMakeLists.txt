cmake_minimum_required(VERSION 3.16)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic")
project(PdfWorkstation VERSION 1.0.0 LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-mingw-dynamic")
enable_testing()
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ── Dependencies ──────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Gui Widgets Pdf PdfWidgets PrintSupport Svg Network Test Concurrent REQUIRED)
find_package(Qt6 COMPONENTS LinguistTools QUIET)
find_package(podofo CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenXLSX CONFIG QUIET)
find_package(duckx CONFIG QUIET)
find_package(CURL QUIET)
find_package(CURL QUIET)
find_package(qpdf QUIET)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Pdfium QUIET)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PKG_TESSERACT QUIET tesseract)
    pkg_check_modules(PKG_LEPTONICA QUIET lept)
endif()

find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(PKG_ONNXRUNTIME QUIET libonnxruntime)
endif()

# FORBIDDEN: MuPDF is AGPL-3.0 — linking it forces GlyphPDF into AGPL.
# Use PDFium (BSD-3) for rendering instead.
if(TARGET mupdf OR TARGET libmupdf)
    message(FATAL_ERROR "MuPDF/libmupdf detected. AGPL-3.0 license is incompatible with this project. Remove it and use PDFium.")
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
    src/engines/CollaborationManager.h src/engines/CollaborationManager.cpp
    src/engines/DocumentSession.h    src/engines/DocumentSession.cpp
    src/engines/BackendRouter.h      src/engines/BackendRouter.cpp
    src/engines/podofo/PoDoFoBackend.h src/engines/podofo/PoDoFoBackend.cpp
    src/engines/pdfium/PdfiumBackend.h src/engines/pdfium/PdfiumBackend.cpp
    src/engines/qpdf/QpdfBackend.h     src/engines/qpdf/QpdfBackend.cpp
    src/engines/RenderCache.h          src/engines/RenderCache.cpp
    src/engines/DiffEngine.h           src/engines/DiffEngine.cpp
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
    PRIVATE podofo::podofo OpenSSL::SSL OpenSSL::Crypto Qt6::Network Qt6::Widgets Qt6::Concurrent
)
if(WIN32)
    target_link_libraries(pdfws_engines PUBLIC Credui Advapi32)
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
if(qpdf_FOUND AND (TARGET qpdf::qpdf OR TARGET qpdf::libqpdf))
    if(MINGW AND EXISTS "C:/vcpkg/installed/x64-mingw-dynamic/lib/libqpdf.dll.a")
        target_link_libraries(pdfws_engines PUBLIC "C:/vcpkg/installed/x64-mingw-dynamic/lib/libqpdf.dll.a")
    else()
        if(TARGET qpdf::qpdf)
            target_link_libraries(pdfws_engines PUBLIC qpdf::qpdf)
        else()
            target_link_libraries(pdfws_engines PUBLIC qpdf::libqpdf)
        endif()
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

# ── pdfws_commands (STATIC — mostly header-only but needs moc for some) ──
add_library(pdfws_commands STATIC
    src/commands/RotatePageCommand.h
    src/commands/DeletePageCommand.h
    src/commands/InsertPageCommand.h
    src/commands/SetMetadataCommand.h
    src/commands/AddFormFieldCommand.h
    src/commands/EditAnnotationCommand.h
    src/commands/EncryptDocumentHelper.h
    src/commands/SignDocumentHelper.h
    src/commands/SanitizeDocumentHelper.h
    src/commands/RedactCommand.h
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
    src/ui/EditAnnotationCommand.cpp src/ui/RedactCommand.cpp
    src/ui/WelcomeWidget.h           src/ui/WelcomeWidget.cpp
    src/ui/FindBar.h                 src/ui/FindBar.cpp
    src/ui/AnnotationToolBar.h       src/ui/AnnotationToolBar.cpp
    src/ui/EditToolBar.h             src/ui/EditToolBar.cpp
    src/ui/InspectorWidget.h         src/ui/InspectorWidget.cpp
    src/ui/CompareWidget.h           src/ui/CompareWidget.cpp
    src/ui/BookmarkPanel.h           src/ui/BookmarkPanel.cpp
    src/ui/CommentsWidget.h          src/ui/CommentsWidget.cpp
    src/ui/DocumentPropertiesWidget.h src/ui/DocumentPropertiesWidget.cpp
    src/ui/SignaturesWidget.h        src/ui/SignaturesWidget.cpp
    src/ui/PageManagementDialog.h    src/ui/PageManagementDialog.cpp
    src/ui/EncryptionDialog.h        src/ui/EncryptionDialog.cpp
    src/ui/SignatureDialog.h         src/ui/SignatureDialog.cpp
    src/ui/MetadataDialog.h          src/ui/MetadataDialog.cpp
    src/ui/AnnotationLayer.h         src/ui/AnnotationLayer.cpp
    src/ui/ThumbnailSidebar.h        src/ui/ThumbnailSidebar.cpp
    src/ui/ShortcutHelpDialog.h      src/ui/ShortcutHelpDialog.cpp
    src/ui/PreferencesDialog.h       src/ui/PreferencesDialog.cpp
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
    src/engines/ocr/RapidOcrEngine.h         src/engines/ocr/RapidOcrEngine.cpp
    src/engines/ocr/OcrPreprocessor.h        src/engines/ocr/OcrPreprocessor.cpp
    src/engines/ocr/OcrPipeline.h            src/engines/ocr/OcrPipeline.cpp

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
    src/modes/AIChatPanel.h                 src/modes/AIChatPanel.cpp
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
    PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::PrintSupport Qt6::Svg Qt6::Network podofo::podofo
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
)

# Windows: hide console
if(WIN32 AND NOT MSVC)
    target_link_options(PdfWorkstation PRIVATE "-Wl,-subsystem,windows")
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSignatureValidation.cpp")
    add_executable(TestSignatureValidation
        tests/TestSignatureValidation.cpp
    )
    target_include_directories(TestSignatureValidation PRIVATE src tests)
    target_link_libraries(TestSignatureValidation PRIVATE
        pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestSignatureValidation COMMAND TestSignatureValidation)
    set_tests_properties(TestSignatureValidation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;security;signatures;qt;headless"
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
    target_link_libraries(TestEncryption PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets podofo::podofo
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
            "${Qt6_DIR}/../../../plugins/platforms/qoffscreen.dll"
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

# ── Translations (Qt Linguist) ───────────────────────────────────────────
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        translations/glyphpdf_ar.ts
        translations/glyphpdf_fr.ts
        translations/glyphpdf_de.ts
    )
    # Generate .qm files from .ts files at build time
    qt_add_translations(PdfWorkstation
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
    )
    message(STATUS "Qt LinguistTools found — translation targets enabled")
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
