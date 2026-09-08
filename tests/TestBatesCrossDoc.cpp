// SPDX-License-Identifier: Apache-2.0
// §9.9 P1 — Bates numbering continuity across documents.
//
// Real Bates usage is almost always multi-document: a production set of
// files is numbered as ONE continuous sequence. Previously the backend only
// offered the two-argument applyBatesNumbering(path, options), which stamps a
// single document and reports nothing about the counter it ended on — so a
// batch caller had no way to stamp document N+1 continuing from document N
// without re-reading the stamped file and guessing.
//
// Contract pinned here:
//   * applyBatesNumbering(path, options, &lastNumberOut) stamps exactly like
//     the two-argument overload and, on success, reports the LAST Bates
//     number used through `lastNumberOut`. If nothing was stamped (empty or
//     out-of-range page range) it reports options.startNumber - 1, so the
//     batch continuation `next.startNumber = last + 1` is always safe.
//   * doc1 (2 pages), start=1, prefix "ABC-" → ABC-1 / ABC-2, last == 2;
//     doc2 stamped at start = last+1 → ABC-3 / ABC-4. Verified by PDFium
//     text extraction, not content-stream sniffing.
//   * a 3-page doc1 keeps the same arithmetic: doc2 starts at 4.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "engines/PdfEditorEngine.h"

class TestBatesCrossDoc : public QObject {
    Q_OBJECT

private slots:
    // The mission fixture: two 2-page documents, start=1, prefix "ABC-" —
    // doc1 shows ABC-1/ABC-2, doc2 shows ABC-3/ABC-4 (PDFium-extracted).
    void twoDocBatchContinuity();
    // Zero-based continuity: a 3-page doc1 advances doc2's start to 4.
    void threePageDocContinuity();
    // Nothing stamped (out-of-range page range) → last == startNumber-1, so
    // `next = last + 1` never skips or repeats; the 2-arg overload keeps
    // working unchanged (backward compatibility).
    void emptyRangeKeepsCounterSafe();
    // The engine-interface overload reaches the backend (the UI controller
    // calls through IPdfEditorEngine, not the concrete backend).
    void engineInterfaceOverloadReportsCounter();

private:
    static QString makeBlankPdf(const QString &path, int pages);
    static BatesNumberingOptions baseOpts();
    static QString pageText(const QString &pdf, int page);
};

QString TestBatesCrossDoc::makeBlankPdf(const QString &path, int pages) {
    try {
        PoDoFo::PdfMemDocument doc;
        for (int i = 0; i < pages; ++i) {
            (void)doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        }
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception &e) {
        qWarning("makeBlankPdf failed: %s", e.what());
        return {};
    }
}

BatesNumberingOptions TestBatesCrossDoc::baseOpts() {
    BatesNumberingOptions opts;
    opts.prefix = QStringLiteral("ABC-");
    opts.suffix.clear();
    opts.startNumber = 1;
    opts.digitCount = 1; // literal "ABC-1", not "ABC-000001"
    opts.fontFamily.clear(); // empty → Helvetica standard-14
    opts.fontSize = 12;
    opts.position = HeaderFooterOptions::Position::BottomRight;
    opts.firstPage = 0;  // all pages
    opts.lastPage = 0;
    return opts;
}

QString TestBatesCrossDoc::pageText(const QString &pdf, int page) {
    PdfiumBackend reader;
    if (!reader.loadDocument(pdf)) return QStringLiteral("<load failed>");
    return reader.extractText(page);
}

void TestBatesCrossDoc::twoDocBatchContinuity() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString doc1 = makeBlankPdf(tmp.filePath("doc1.pdf"), 2);
    const QString doc2 = makeBlankPdf(tmp.filePath("doc2.pdf"), 2);
    QVERIFY(!doc1.isEmpty());
    QVERIFY(!doc2.isEmpty());

    PoDoFoBackend backend;

    BatesNumberingOptions opts = baseOpts();
    int last = -1;
    QVERIFY(backend.loadDocument(doc1)); // batch caller contract: make each doc resident first
    QVERIFY2(backend.applyBatesNumbering(doc1, opts, &last),
             "stamping doc1 must succeed");
    QCOMPARE(last, 2); // doc1 consumed ABC-1..ABC-2

    QVERIFY(backend.loadDocument(doc2)); // swap resident doc before stamping the next file
    opts.startNumber = last + 1; // the cross-document continuation
    QVERIFY2(backend.applyBatesNumbering(doc2, opts, &last),
             "stamping doc2 must succeed");
    QCOMPARE(last, 4); // doc2 consumed ABC-3..ABC-4

    // PDFium-verified page text on both documents.
    QVERIFY2(pageText(doc1, 0).contains(QStringLiteral("ABC-1")),
             qPrintable(QStringLiteral("doc1 p1 must show ABC-1; got: %1")
                            .arg(pageText(doc1, 0))));
    QVERIFY2(pageText(doc1, 1).contains(QStringLiteral("ABC-2")),
             qPrintable(QStringLiteral("doc1 p2 must show ABC-2; got: %1")
                            .arg(pageText(doc1, 1))));
    QVERIFY2(pageText(doc2, 0).contains(QStringLiteral("ABC-3")),
             qPrintable(QStringLiteral("doc2 p1 must show ABC-3; got: %1")
                            .arg(pageText(doc2, 0))));
    QVERIFY2(pageText(doc2, 1).contains(QStringLiteral("ABC-4")),
             qPrintable(QStringLiteral("doc2 p2 must show ABC-4; got: %1")
                            .arg(pageText(doc2, 1))));

    // Continuity, not restart: neither doc may carry the other's range.
    QVERIFY2(!pageText(doc1, 0).contains(QStringLiteral("ABC-3")),
             "doc1 must not contain doc2's numbers");
    QVERIFY2(!pageText(doc2, 0).contains(QStringLiteral("ABC-1")),
             "doc2 must not restart at doc1's start");
}

void TestBatesCrossDoc::threePageDocContinuity() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString doc1 = makeBlankPdf(tmp.filePath("three.pdf"), 3);
    const QString doc2 = makeBlankPdf(tmp.filePath("two.pdf"), 2);
    QVERIFY(!doc1.isEmpty());
    QVERIFY(!doc2.isEmpty());

    PoDoFoBackend backend;
    BatesNumberingOptions opts = baseOpts();
    int last = -1;
    QVERIFY(backend.loadDocument(doc1)); // batch caller contract: make each doc resident first
    QVERIFY(backend.applyBatesNumbering(doc1, opts, &last));
    QCOMPARE(last, 3); // ABC-1..ABC-3 across doc1's 3 pages

    QVERIFY(backend.loadDocument(doc2)); // swap resident doc before stamping the next file
    opts.startNumber = last + 1; // 4
    QVERIFY(backend.applyBatesNumbering(doc2, opts, &last));
    QCOMPARE(last, 5);

    QVERIFY2(pageText(doc1, 2).contains(QStringLiteral("ABC-3")),
             qPrintable(QStringLiteral("doc1 p3 must show ABC-3; got: %1")
                            .arg(pageText(doc1, 2))));
    QVERIFY2(pageText(doc2, 0).contains(QStringLiteral("ABC-4")),
             qPrintable(QStringLiteral("doc2 p1 must show ABC-4; got: %1")
                            .arg(pageText(doc2, 0))));
    QVERIFY2(pageText(doc2, 1).contains(QStringLiteral("ABC-5")),
             qPrintable(QStringLiteral("doc2 p2 must show ABC-5; got: %1")
                            .arg(pageText(doc2, 1))));
}

void TestBatesCrossDoc::emptyRangeKeepsCounterSafe() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString doc = makeBlankPdf(tmp.filePath("doc.pdf"), 2);
    QVERIFY(!doc.isEmpty());

    PoDoFoBackend backend;
    BatesNumberingOptions opts = baseOpts();
    opts.firstPage = 5; // beyond the document → nothing gets stamped
    opts.lastPage = 9;

    int last = -1;
    QVERIFY2(backend.applyBatesNumbering(doc, opts, &last),
             "an empty range is not an error");
    QCOMPARE(last, opts.startNumber - 1); // 0 → continuation stays at 1

    // The 2-argument overload keeps compiling and working (pre-existing callers).
    BatesNumberingOptions plain = baseOpts();
    QVERIFY(backend.applyBatesNumbering(doc, plain));
    QVERIFY2(pageText(doc, 0).contains(QStringLiteral("ABC-1")),
             qPrintable(QStringLiteral("2-arg overload still stamps; got: %1")
                            .arg(pageText(doc, 0))));
}

void TestBatesCrossDoc::engineInterfaceOverloadReportsCounter() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString doc1 = makeBlankPdf(tmp.filePath("e1.pdf"), 2);
    const QString doc2 = makeBlankPdf(tmp.filePath("e2.pdf"), 2);
    QVERIFY(!doc1.isEmpty());
    QVERIFY(!doc2.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(doc1));

    BatesNumberingOptions opts = baseOpts();
    int last = -1;
    QVERIFY2(engine.applyBatesNumbering(doc1, opts, &last),
             "engine-interface overload must reach the backend");
    QCOMPARE(last, 2);

    QVERIFY(engine.loadDocumentForEditing(doc2));
    opts.startNumber = last + 1;
    QVERIFY(engine.applyBatesNumbering(doc2, opts, &last));
    QCOMPARE(last, 4);
    QVERIFY2(pageText(doc2, 0).contains(QStringLiteral("ABC-3")),
             qPrintable(QStringLiteral("doc2 p1 must show ABC-3; got: %1")
                            .arg(pageText(doc2, 0))));
}

QTEST_GUILESS_MAIN(TestBatesCrossDoc)
#include "TestBatesCrossDoc.moc"
