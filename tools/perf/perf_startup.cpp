// SPDX-License-Identifier: Apache-2.0
// SWEEP-W3-PERF startup measurement driver.
//
// Purpose: MEASURE the cold app bootstrap path — main() entry to window-shown —
// for the W3 performance baseline (docs/audit/PERF-BASELINE-2026-09-20.md).
// This is a driver exe LINKING THE APP TARGETS (pdfws_ui/commands/engines/core)
// and running the SAME bootstrapper sequence as src/app/main.cpp:
//   QApplication -> Fusion style -> theme + QSS -> TempFileManager::install
//   -> Bootstrapper::createContext() -> gp::MainWindow -> showMaximized -> exposed.
//
// LIMITS (stated honestly):
//   * Runs with QT_QPA_PLATFORM=offscreen — no real GPU/compositor; "shown"
//     means the window is exposed per Qt's windowing contract, not painted by
//     a desktop compositor.
//   * Resources compiled into the PdfWorkstation exe target only (resources.qrc:
//     splash/icon/fonts/:/translations) are NULL-path here — the driver links
//     the static libs but not the exe qrc. Every one of those loads fails
//     gracefully in main.cpp too; the skipped work is a handful of resource
//     reads. The theme QSS resolves through the injected
//     GLYPHPDF_SOURCE_RESOURCE_DIR (the same fallback production uses when a
//     resource is absent — the TestStatusBarSlim loadThemeSheet pattern).
//   * Cold start per run = a FRESH PROCESS spawned by run_perf_suite.ps1; this
//     exe measures in-process (loader + DLL init excluded). The suite records
//     BOTH: external whole-process wall time AND these internal samples.
//   * QSettings are isolated (org "GlyphPDFPerf") so no user recents/recovery
//     state leaks into the measurement (and none is polluted).
//
// Usage: perf_startup --json
//   Prints ONE JSON line: {"total_ms":..,"context_ms":..,"ctor_ms":..,
//                          "shown_ms":..,"peak_ws_bytes":..}
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QStyleFactory>
#include <QPalette>
#include <QTimer>
#include <QWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#include "GpMainWindow.h"
#include "util/GpTheme.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/TempFileManager.h"

namespace {
qint64 peakWorkingSetBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))
               ? static_cast<qint64>(pmc.PeakWorkingSetSize) : -1;
#else
    return -1; // peak RSS sampling not needed for the Windows baseline lane
#endif
}
} // namespace

int main(int argc, char *argv[]) {
    // Timer starts at the FIRST statement of main — same anchor the suite's
    // external wall-clock measurement brackets.
    QElapsedTimer tTotal;
    tTotal.start();

    // QT_QPA_PLATFORM=offscreen is set by the spawning suite (env of the child);
    // it must exist BEFORE QApplication is constructed, never here.
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GlyphPDF"));
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFPerf"));

    QElapsedTimer tPhase;
    qint64 contextMs = -1, ctorMs = -1, shownMs = -1;

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // main.cpp: translations/fonts load from exe resources or system paths;
    // the resource-borne ones are null-path in this driver (see header LIMITS).
    // Theme: identical sequence to main.cpp.
    const auto themeMode = gp::Theme::Dark;
    gp::Theme::setMode(themeMode);
    QFile sheet(gp::Theme::sheetForMode(themeMode));
    if (sheet.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(sheet.readAll()));

    TempFileManager::install();

    // === Engine bootstrap (the measured critical path) ===
    tPhase.restart();
    auto ctx = Bootstrapper::createContext();
    contextMs = tPhase.elapsed();

    tPhase.restart();
    gp::MainWindow mainWindow(std::move(ctx));
    ctorMs = tPhase.elapsed();

    tPhase.restart();
    mainWindow.showMaximized();
    // "Shown" = the window's backing store is exposed (offscreen: the platform
    // plugin reports expose immediately after show; we pump the loop so the
    // pending show + initial layout actually run — same contract the GUI
    // tests rely on when they show() + processEvents()).
    for (int i = 0; i < 100; ++i) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        if (mainWindow.windowHandle() && mainWindow.windowHandle()->isExposed())
            break;
    }
    shownMs = tPhase.elapsed();

    QJsonObject o;
    o.insert(QStringLiteral("total_ms"), static_cast<double>(tTotal.elapsed()));
    o.insert(QStringLiteral("context_ms"), static_cast<double>(contextMs));
    o.insert(QStringLiteral("ctor_ms"), static_cast<double>(ctorMs));
    o.insert(QStringLiteral("shown_ms"), static_cast<double>(shownMs));
    o.insert(QStringLiteral("peak_ws_bytes"), static_cast<double>(peakWorkingSetBytes()));
    o.insert(QStringLiteral("window_shown"), mainWindow.isVisible());
    std::fprintf(stdout, "%s\n",
                 QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
    std::fflush(stdout);
    return 0;
}
