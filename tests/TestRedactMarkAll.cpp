// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 regression test: Mark All Occurrences actually places
// redaction marks for every regex match (via PatternRedactor geometry), and
// Mark Region activates the canvas drag-placement mode — the pills are no
// longer decorative toggles.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>
#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QRadioButton>
#include <QTextStream>
#include <podofo/podofo.h>
#include "modes/RedactMode.h"
#include "core/AppContext.h"
#include "engines/PdfEditorEngine.h"
#include "ui/PdfViewerWidget.h"

class TestRedactMarkAll : public QObject {
    Q_OBJECT
private slots:
    void markAllPlacesMatchRects();
    void markRegionSetsRedactToolMode();
    // §9.8 F1: an explicit page-range LIST ("1, 3") must mark exactly those
    // pages — never the pages in between (they are irreversibly destroyed on
    // Apply), and an unparseable range must mark nothing at all.
    void rangeMarksExactlyTheListedPages();
    void invalidRangeMarksNothing();
    // §9.8 P0: the Apply flow's sanitize checkbox must run the full hidden-
    // data scrub on the saved copy (and stay off honestly when unchecked).
    void sanitizeCopyCheckboxProducesCleanOutput();
    void redactPanelShowsLocalClaim();
    void sanitizeUncheckedKeepsMetadata();
private:
    static QString createPdfWithText(const QTemporaryDir& tmpDir,
                                     const QString& name, const QString& text);
    static QString createNPagePdfWithText(const QTemporaryDir& tmpDir,
                                          const QString& name, int pageCount,
                                          const QString& text);
};
QString TestRedactMarkAll::createPdfWithText(const QTemporaryDir& tmpDir,
                                             const QString& name, const QString& text) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText(text.toUtf8().constData(), 50, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

void TestRedactMarkAll::markAllPlacesMatchRects() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createPdfWithText(tmp, "emails.pdf",
                                          QStringLiteral("Contact a@b.com or c@d.org today"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    AppContext ctx;
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    // Select the built-in Email pattern (combo index 0), whole-document scope.
    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0);
    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));

    const auto annos = viewer.annotations();
    int redactCount = 0;
    for (const auto& a : annos)
        if (a.mode == ToolMode::Redact) ++redactCount;
    QFile diag(QStringLiteral("rma_diag.txt"));
    if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diag);
        ts << "redactCount=" << redactCount << " total=" << annos.size() << "\n";
    }
    QVERIFY2(redactCount >= 2,
             qPrintable(QStringLiteral("expected >=2 redaction marks for two email addresses, got %1")
                            .arg(redactCount)));
}

void TestRedactMarkAll::markRegionSetsRedactToolMode() {
    PdfViewerWidget viewer;
    gp::RedactMode mode;
    mode.setViewer(&viewer);
    viewer.setToolMode(ToolMode::HandTool);
    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkRegion"));
    QCOMPARE(viewer.toolMode(), ToolMode::Redact);
}

QString TestRedactMarkAll::createNPagePdfWithText(const QTemporaryDir& tmpDir,
                                                  const QString& name, int pageCount,
                                                  const QString& text) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int i = 0; i < pageCount; ++i) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(text.toUtf8().constData(), 50, 700);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

namespace {
// Select the "Page range:" scope radio and return the range expression edit.
// Identified by its placeholder ("e.g. 1-3, 5, 7-9") — findChild<QLineEdit*>
// alone would grab the custom-regex edit created earlier in buildUi.
QLineEdit* selectRangeScope(gp::RedactMode& mode) {
    const auto radios = mode.findChildren<QRadioButton*>();
    for (QRadioButton* r : radios) {
        if (r->text().contains(QStringLiteral("Page range"))) {
            r->setChecked(true);
            break;
        }
    }
    const auto edits = mode.findChildren<QLineEdit*>();
    for (QLineEdit* e : edits)
        if (e->placeholderText().contains(QStringLiteral("1-3")))
            return e;
    return nullptr;
}
} // namespace

void TestRedactMarkAll::rangeMarksExactlyTheListedPages() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdfWithText(tmp, "five_pages.pdf", 5,
                                               QStringLiteral("Secret a@b.com"));
    QVERIFY(!pdf.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    // Non-owning: the engine is stack-allocated, the context only borrows it.
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0); // built-in Email pattern

    QLineEdit* rangeEdit = selectRangeScope(mode);
    QVERIFY2(rangeEdit && rangeEdit->isEnabled(), "range edit must be enabled by the Page range scope");
    rangeEdit->setText(QStringLiteral("1, 3")); // 1-based pages 1 and 3 → indices {0, 2}

    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));

    int marksOn[5] = {0, 0, 0, 0, 0};
    for (const auto& a : viewer.annotations())
        if (a.mode == ToolMode::Redact && a.pageIndex >= 0 && a.pageIndex < 5)
            ++marksOn[a.pageIndex];
    QVERIFY2(marksOn[0] >= 1, "page 1 was explicitly selected and must be marked");
    QVERIFY2(marksOn[2] >= 1, "page 3 was explicitly selected and must be marked");
    QVERIFY2(marksOn[1] == 0 && marksOn[3] == 0 && marksOn[4] == 0,
             qPrintable(QStringLiteral("pages 2/4/5 were NOT selected — marking them would "
                                      "destroy unselected content on Apply (got %1,%2,%3)")
                            .arg(marksOn[1]).arg(marksOn[3]).arg(marksOn[4])));
}

void TestRedactMarkAll::invalidRangeMarksNothing() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdfWithText(tmp, "three_pages.pdf", 3,
                                               QStringLiteral("Secret a@b.com"));
    QVERIFY(!pdf.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0);

    QLineEdit* rangeEdit = selectRangeScope(mode);
    QVERIFY(rangeEdit);
    rangeEdit->setText(QStringLiteral("abc")); // unparseable → invalid sentinel

    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));
    for (const auto& a : viewer.annotations())
        QVERIFY2(a.mode != ToolMode::Redact,
                 "an invalid range must mark nothing — falling through to 'all pages' "
                 "silently selects every page for irreversible destruction");
}

namespace {
// A redactable document that also carries hidden data the sanitize pass must
// remove: catalog OpenAction JS + XMP metadata (same risky content the
// TestCompressStripSanitize fixture uses).
QString createRiskyRedactablePdf(const QTemporaryDir& tmpDir, const QString& name) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText("Secret a@b.com", 50, 700);
        painter.FinishDrawing();

        auto& cat = doc.GetCatalog().GetDictionary();
        PoDoFo::PdfDictionary oa;
        oa.AddKey("S", PoDoFo::PdfName("JavaScript"));
        oa.AddKey("JS", PoDoFo::PdfString("app.alert(1);"));
        cat.AddKey("OpenAction", PoDoFo::PdfObject(oa));
        auto& xmp = doc.GetObjects().CreateDictionaryObject();
        xmp.GetOrCreateStream().SetData(PoDoFo::bufferview("<?xpacket xmp-secret?>"));
        cat.AddKey("Metadata", PoDoFo::PdfObject(xmp.GetIndirectReference()));
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

bool catalogHasKey(const QString& pdf, const char* key) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        return doc.GetCatalog().GetDictionary().HasKey(key);
    } catch (...) {
        return true; // treat unloadable output as "not clean"
    }
}

bool contentContains(const QString& pdf, const QByteArray& needle, QString* err = nullptr) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        for (unsigned i = 0; i < doc.GetPages().GetCount(); ++i) {
            auto* co = doc.GetPages().GetPageAt(i).GetContents();
            if (!co) continue;
            PoDoFo::charbuff buf;
            co->CopyTo(buf);
            if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(needle))
                return true;
        }
        return false;
    } catch (const std::exception& e) {
        if (err) *err = QString::fromLatin1(e.what());
        return true; // treat unloadable output as "not clean"
    }
}
} // namespace

namespace {
// onApplyRedactions asks a modal Yes/No confirmation before burning marks in —
// accept it from a queued callback so headless tests can drive the flow.
void acceptApplyConfirmation() {
    QTimer::singleShot(0, [] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            if (auto* yes = box->button(QMessageBox::Yes)) { yes->click(); return; }
        }
        if (QWidget* m = QApplication::activeModalWidget()) m->close();
    });
}
} // namespace

void TestRedactMarkAll::sanitizeCopyCheckboxProducesCleanOutput() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createRiskyRedactablePdf(tmp, "risky_apply.pdf");
    QVERIFY2(!pdf.isEmpty(), "risky fixture failed");

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    // Default ON — the offer must not hide behind an opt-in.
    auto* chk = mode.findChild<QCheckBox*>(QStringLiteral("redactChkSanitizeCopy"));
    QVERIFY2(chk, "the Apply flow must expose the sanitize-copy checkbox");
    QVERIFY2(chk->isChecked(), "sanitize copy must default to ON");

    // Place a mark over the secret and apply.
    AnnotationItem mark;
    mark.mode = ToolMode::Redact;
    mark.pageIndex = 0;
    mark.rect = QRectF(40, 690, 300, 30); // covers the drawn text at (50,700)
    viewer.setAnnotations({mark});
    acceptApplyConfirmation();
    QVERIFY(QMetaObject::invokeMethod(&mode, "onApplyRedactions"));

    const QString out = tmp.filePath("risky_apply_redacted.pdf");
    QVERIFY2(QFileInfo::exists(out), "the redacted copy must be written");
    QVERIFY2(!contentContains(out, "a@b.com"),
             "the secret must be excised from the redacted copy");
    QVERIFY2(!catalogHasKey(out, "OpenAction"),
             "OpenAction JS must be scrubbed from the sanitized copy");
    QVERIFY2(!catalogHasKey(out, "Metadata"),
             "XMP metadata must be scrubbed from the sanitized copy");
}

void TestRedactMarkAll::sanitizeUncheckedKeepsMetadata() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createRiskyRedactablePdf(tmp, "risky_apply2.pdf");
    QVERIFY(!pdf.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    auto* chk = mode.findChild<QCheckBox*>(QStringLiteral("redactChkSanitizeCopy"));
    QVERIFY(chk);
    chk->setChecked(false); // honest opt-out: only content excision runs

    AnnotationItem mark;
    mark.mode = ToolMode::Redact;
    mark.pageIndex = 0;
    mark.rect = QRectF(40, 690, 300, 30);
    viewer.setAnnotations({mark});
    acceptApplyConfirmation();
    QVERIFY(QMetaObject::invokeMethod(&mode, "onApplyRedactions"));

    const QString out = tmp.filePath("risky_apply2_redacted.pdf");
    QVERIFY2(QFileInfo::exists(out), "the redacted copy must be written");
    QVERIFY2(!contentContains(out, "a@b.com"),
             "the secret must be excised even without sanitization");
    QVERIFY2(catalogHasKey(out, "OpenAction"),
             "with the checkbox off, hidden data is intentionally retained — "
             "this documents the honest difference between the two modes");
}

void TestRedactMarkAll::redactPanelShowsLocalClaim() {
    // §9.8 P0: the redaction surface must state the compliance differentiator.
    gp::RedactMode mode;
    auto* label = mode.findChild<QLabel*>(QStringLiteral("redactLocalClaimLabel"));
    QVERIFY2(label, "RedactMode must display the local-first claim label");
    QVERIFY2(label->text().contains(QStringLiteral("no upload"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("claim must state no-upload: %1").arg(label->text())));
}
QTEST_MAIN(TestRedactMarkAll)
#include "TestRedactMarkAll.moc"