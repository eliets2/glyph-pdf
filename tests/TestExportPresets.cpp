// SPDX-License-Identifier: Apache-2.0
// §9.16 regression test: the export presets (linearize / PDF-A) must map onto
// the concrete post-processing steps the export flow executes. The mapping
// lives in HomeController::planForExport(), the pure seam used by
// HomeController::onExportPresets() — previously the preset execution shipped
// without any test.
#include <QtTest/QtTest>
#include "shell/controllers/HomeController.h"

using gp::ExportPresetsPanel;
using gp::HomeController;

class TestExportPresets : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void plainPresetRunsNoPostProcessing();
    void linearizedPresetRequestsLinearize();
    void pdfAPresetDefaultsTo2b();
    void pdfA3bPresetRequestsLevel3();
    void webOptimizedDefaultPresetIsLinearized();
    void highQualityDefaultPresetIsPdfA2b();
    void legalArchiveDefaultPresetIsPdfA3b();
};

void TestExportPresets::initTestCase() {
    // Isolate QSettings so the test neither reads the developer's real presets
    // nor clobbers them (ExportPresetsPanel persists via the default QSettings).
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestExportPresets"));
    QSettings().remove(QStringLiteral("export/presets"));
}

void TestExportPresets::cleanupTestCase() {
    QSettings().remove(QStringLiteral("export/presets"));
}

void TestExportPresets::plainPresetRunsNoPostProcessing() {
    ExportPresetsPanel::Preset p;
    p.name = QStringLiteral("Plain");
    const auto plan = HomeController::planForExport(p);
    QVERIFY(!plan.linearize);
    QCOMPARE(plan.pdfALevel, 0);
}

void TestExportPresets::linearizedPresetRequestsLinearize() {
    ExportPresetsPanel::Preset p;
    p.name = QStringLiteral("Web");
    p.linearized = true;
    const auto plan = HomeController::planForExport(p);
    QVERIFY(plan.linearize);
    QCOMPARE(plan.pdfALevel, 0);
}

void TestExportPresets::pdfAPresetDefaultsTo2b() {
    ExportPresetsPanel::Preset p;
    p.name = QStringLiteral("Archive");
    p.pdfA = true; // pdfALevel left empty → 2b fallback
    const auto plan = HomeController::planForExport(p);
    QVERIFY(!plan.linearize);
    QCOMPARE(plan.pdfALevel, 2);
}

void TestExportPresets::pdfA3bPresetRequestsLevel3() {
    ExportPresetsPanel::Preset p;
    p.name = QStringLiteral("Legal");
    p.pdfA = true;
    p.pdfALevel = QStringLiteral("3b");
    const auto plan = HomeController::planForExport(p);
    QCOMPARE(plan.pdfALevel, 3);
}

void TestExportPresets::webOptimizedDefaultPresetIsLinearized() {
    // The shipped "Web Optimized" default must keep requesting linearization.
    ExportPresetsPanel::ensureDefaults();
    const auto presets = ExportPresetsPanel::loadPresets();
    bool found = false;
    for (const auto& p : presets) {
        if (p.name == QStringLiteral("Web Optimized")) {
            found = true;
            const auto plan = HomeController::planForExport(p);
            QVERIFY(plan.linearize);
            QCOMPARE(plan.pdfALevel, 0);
        }
    }
    QVERIFY(found);
}

void TestExportPresets::highQualityDefaultPresetIsPdfA2b() {
    ExportPresetsPanel::ensureDefaults();
    const auto presets = ExportPresetsPanel::loadPresets();
    bool found = false;
    for (const auto& p : presets) {
        if (p.name == QStringLiteral("High Quality PDF/A")) {
            found = true;
            const auto plan = HomeController::planForExport(p);
            QVERIFY(!plan.linearize);
            QCOMPARE(plan.pdfALevel, 2);
        }
    }
    QVERIFY(found);
}

void TestExportPresets::legalArchiveDefaultPresetIsPdfA3b() {
    ExportPresetsPanel::ensureDefaults();
    const auto presets = ExportPresetsPanel::loadPresets();
    bool found = false;
    for (const auto& p : presets) {
        if (p.name == QStringLiteral("Legal Archive")) {
            found = true;
            const auto plan = HomeController::planForExport(p);
            QCOMPARE(plan.pdfALevel, 3);
        }
    }
    QVERIFY(found);
}

QTEST_MAIN(TestExportPresets)
#include "TestExportPresets.moc"
