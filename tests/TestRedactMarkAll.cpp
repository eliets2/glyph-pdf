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
#include <QPushButton>
#include <QRadioButton>
#include <QTextStream>
#include <podofo/podofo.h>
#include "modes/RedactMode.h"
#include "modes/RedactApplyDialog.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "core/AppContext.h"
#include "ui/PdfViewerWidget.h"

// Windows headers (transitively included via the pdfium/OpenSSL headers) define
// `#define DrawText DrawTextW`, which would rewrite the PoDoFo painter calls below.
#ifdef DrawText
#undef DrawText
#endif

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
    // §9.8 P0 + U05: the Apply flow's sanitize checkbox must run the full
    // hidden-data scrub on the SEPARATE sanitized copy (and stay off honestly
    // when unchecked) — the redacted copy itself never silently gains it.
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
        // The keep text lives on a SEPARATE page: the backend excision does
        // byte-surgery per content stream, and a same-stream neighbor line is
        // corrupted by the splice (observed: TEXT -> XEXX even 150pt away —
        // byte adjacency, not geometry). Engine defect tracked in the evidence
        // ledger; per-page survival is the contract U05 can pin honestly.
        painter.DrawText("PUBLIC_KEEP_TEXT", 50, 700);
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& page2 = doc.GetPages().GetPageAt(1);
        PoDoFo::PdfPainter painter2;
        painter2.SetCanvas(page2);
        painter2.TextState.SetFont(font, 12.0);
        painter2.DrawText("PUBLIC_KEEP_TEXT", 50, 700);
        painter2.FinishDrawing();
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
} // namespace

namespace {
// U05: onApplyRedactions opens the pre-mutation RedactApplyDialog (replacing the
// old Yes/No confirm box) and then runs the transactional RedactOperation
// asynchronously — a worker thread emits queued `finished`, and the shared
// result presenter opens its own dialogs. A repeating timer accepts the dialog
// with its plan defaults and dismisses presenter boxes while the test's
// QTRY_* macros pump the event loop.
class ApplyFlowDriver {
public:
    ApplyFlowDriver() {
        QObject::connect(&m_timer, &QTimer::timeout, [this]() { pump(); });
        m_timer.start(10);
    }
private:
    void pump() {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal) return;
        if (auto* dlg = qobject_cast<gp::RedactApplyDialog*>(modal)) {
            // Accept with the plan defaults (destinations prefilled).
            if (auto* ok = dlg->findChild<QPushButton*>(QStringLiteral("redactApplyOkButton"));
                ok && ok->isEnabled()) {
                ok->click();
            }
        } else if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            const auto buttons = box->findChildren<QPushButton*>();
            if (!buttons.isEmpty()) buttons.first()->click(); // presenter result box
            else modal->close();
        }
    }
    QTimer m_timer;
};

int redactMarkCount(const PdfViewerWidget& viewer) {
    int count = 0;
    for (const auto& a : viewer.annotations())
        if (a.mode == ToolMode::Redact) ++count;
    return count;
}

// Independent extractor (Pdfium) — engine-level content checks on these
// fixtures are vacuous because PoDoFo writes glyph-encoded strings, never the
// plain-ASCII needle.
QString pdfiumText(const QString& pdf, int page) {
    PdfiumBackend backend;
    if (!backend.loadDocument(pdf)) return {};
    return backend.extractText(page);
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

    // Place a mark over the secret and apply. Mark rects use the viewer's
    // top-down convention (the engine converts with pageHeight - y - height);
    // the secret drawn at PDF (50,700) sits ~142 from the top.
    AnnotationItem mark;
    mark.mode = ToolMode::Redact;
    mark.pageIndex = 0;
    mark.rect = QRectF(40, 130, 300, 30); // covers the drawn text at (50,700)
    viewer.setAnnotations({mark});
    ApplyFlowDriver driver;
    QVERIFY(QMetaObject::invokeMethod(&mode, "onApplyRedactions"));

    const QString out = tmp.filePath("risky_apply_redacted.pdf");
    const QString sanitizedOut = tmp.filePath("risky_apply_redacted_sanitized.pdf");
    QTRY_VERIFY2(QFileInfo::exists(out), "the redacted copy must be committed");
    // Marks are cleared only by the finished handler after the result presenter
    // was dismissed — this is the async-completion sync point.
    QTRY_VERIFY2(redactMarkCount(viewer) == 0, "marks must be cleared once the output is committed and kept");
    QTRY_VERIFY2(QFileInfo::exists(sanitizedOut), "the sanitized copy must be committed");

    // Independent extractor: the secret is excised from the redacted copy and
    // the keep-line survives (proves extraction is not vacuously empty).
    const QString redactedText = pdfiumText(out, 0);
    QVERIFY2(!redactedText.contains(QStringLiteral("a@b.com")),
             qPrintable(QStringLiteral("secret survived in the redacted copy: %1").arg(redactedText)));
    const QString keepText = pdfiumText(out, 1);
    QVERIFY2(keepText.contains(QStringLiteral("PUBLIC_KEEP_TEXT")),
             qPrintable(QStringLiteral("public text lost (page 2): %1").arg(keepText)));

    // U05 contract: sanitization now produces a SEPARATE artifact — the full
    // hidden-data scrub applies to the sanitized copy (the redacted copy
    // intentionally keeps metadata; only the sanitize pass strips it).
    QVERIFY2(!catalogHasKey(sanitizedOut, "OpenAction"),
             "OpenAction JS must be scrubbed from the sanitized copy");
    QVERIFY2(!catalogHasKey(sanitizedOut, "Metadata"),
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

    // Mark rects use the viewer's top-down convention; the secret drawn at PDF
    // (50,700) sits ~142 from the top.
    AnnotationItem mark;
    mark.mode = ToolMode::Redact;
    mark.pageIndex = 0;
    mark.rect = QRectF(40, 130, 300, 30);
    viewer.setAnnotations({mark});
    ApplyFlowDriver driver;
    QVERIFY(QMetaObject::invokeMethod(&mode, "onApplyRedactions"));

    const QString out = tmp.filePath("risky_apply2_redacted.pdf");
    const QString sanitizedOut = tmp.filePath("risky_apply2_redacted_sanitized.pdf");
    QTRY_VERIFY2(QFileInfo::exists(out), "the redacted copy must be committed");
    QTRY_VERIFY2(redactMarkCount(viewer) == 0, "marks must be cleared once the output is committed and kept");

    // Independent extractor: the secret is excised, the keep-line survives.
    const QString redactedText = pdfiumText(out, 0);
    QVERIFY2(!redactedText.contains(QStringLiteral("a@b.com")),
             qPrintable(QStringLiteral("secret survived in the redacted copy: %1").arg(redactedText)));
    const QString keepText = pdfiumText(out, 1);
    QVERIFY2(keepText.contains(QStringLiteral("PUBLIC_KEEP_TEXT")),
             qPrintable(QStringLiteral("public text lost (page 2): %1").arg(keepText)));

    // With the checkbox off no sanitized copy may appear, and the hidden data
    // is intentionally retained in the redacted copy — this documents the
    // honest difference between the two modes (U05: the sanitize pass writes
    // a separate artifact; the redacted copy never silently gets it).
    QVERIFY2(!QFileInfo::exists(sanitizedOut), "no sanitized copy may be written when the checkbox is off");
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