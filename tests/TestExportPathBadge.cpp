// SPDX-License-Identifier: Apache-2.0
// Audit 9.16 P0 regression test: the app must be able to report which export
// path actually ran (native OOXML vs mislabeled fallback) so the UI can tell
// the user the truth about their .docx/.xlsx files.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "engines/ConversionManager.h"
#include "shell/controllers/ConvertController.h"

class TestExportPathBadge : public QObject {
    Q_OBJECT
private slots:
    void capabilityFlagsAreConsistent();
    void wordExportTracksEngineUsed();
    void localProcessingNoticeStatesPrivacy();
};
static QString createMinimalPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    return path;
}

void TestExportPathBadge::capabilityFlagsAreConsistent() {
    // The flags must compile-time match the availability of the OOXML libs.
#ifdef HAS_DUCKX
    QVERIFY(ConversionManager::hasNativeWordExport());
#else
    QVERIFY(!ConversionManager::hasNativeWordExport());
#endif
#ifdef HAS_OPENXLSX
    QVERIFY(ConversionManager::hasNativeExcelExport());
#else
    QVERIFY(!ConversionManager::hasNativeExcelExport());
#endif
}
void TestExportPathBadge::wordExportTracksEngineUsed() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMinimalPdf(tmp.path(), "in.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::Unknown);

    const QString out = tmp.filePath("out.docx");
    const bool ok = mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Word);
    QVERIFY(ok);

    // Whichever path ran must be reported truthfully.
    if (ConversionManager::hasNativeWordExport()) {
        QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::NativeOoxml);
    } else {
        QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::Fallback);
    }
}
// §9.16 P0: the local-processing badge must exist and make an honest,
// factual claim (every conversion runs on-device; imports use a local
// LibreOffice subprocess — no network anywhere in these paths).
void TestExportPathBadge::localProcessingNoticeStatesPrivacy() {
    const QString notice = gp::ConvertController::localProcessingNotice();
    QVERIFY2(notice.contains(QStringLiteral("no upload"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state 'no upload': %1").arg(notice)));
    QVERIFY2(notice.contains(QStringLiteral("no internet"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state 'no internet': %1").arg(notice)));
    QVERIFY2(notice.contains(QStringLiteral("locally"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state the processing is local: %1").arg(notice)));
}
QTEST_MAIN(TestExportPathBadge)
#include "TestExportPathBadge.moc"
