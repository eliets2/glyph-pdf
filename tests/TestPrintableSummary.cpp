// SPDX-License-Identifier: Apache-2.0
// R27 tail: PRINTABLE review summary.
//
// Pack A's T2-3 delivered the comment-status summary WRITER
// (engines/ReviewSummaryWriter). This suite pins the print-ready SURFACE on
// top of the same content model (no second derivation path):
//   * content pin — a fixture with N annotated comments produces a summary
//     PDF whose INDEPENDENT read-back (PDFium text extraction) contains every
//     comment's text/author/status plus the header fields (document name,
//     generation timestamp, totals, distinct authors) and the table of
//     entries;
//   * honest empty state — nothing to summarize is STATED, never an empty or
//     fabricated document;
//   * pagination — 30+ comments spill across sheets and EVERY entry is still
//     present, with per-page footers naming the sheet;
//   * redaction-proof honesty — the summary header lists the proof section
//     when a proof pack is available (with the pack's own generation time and
//     verdict) and lists it as NOT AVAILABLE when it is not, never omitting
//     it silently;
//   * overwrite guard — the destination is only ever replaced through the
//     SafeSave candidate transaction: a failed commit leaves an existing
//     destination byte-identical, and a successful one is a real PDF.
//
// Serial offscreen only (shared %TEMP%/glyphpdf-candidates class FU-2: the
// overwrite-guard pin sweeps/asserts the transaction behavior).

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "engines/ReviewSummaryWriter.h"
#include "engines/SafeSave.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "core/AnnotationTypes.h"

namespace {

AnnotationItem makeComment(const QString& id, const QString& author,
                           ReviewState state, int pageIndex,
                           const QString& text, const QString& created) {
    AnnotationItem a;
    a.id = id;
    a.mode = ToolMode::AddComment;
    a.pageIndex = pageIndex;
    a.author = author;
    a.reviewState = state;
    a.text = text;
    a.djotSource = text;
    a.creationDate = created;
    a.rect = QRectF(50, 50, 24, 24);
    return a;
}

QString wholeText(PdfiumBackend& reader) {
    QString all;
    const int n = reader.pageCount();
    for (int p = 0; p < n; ++p)
        all += reader.extractText(p) + QLatin1Char('\n');
    return all;
}

// A minimal proof pack in the exact schema RedactionProof::Result::toJson
// produces (core/RedactionProof.cpp) — the summary consumes the PACK (the
// report content model), never the verifier's internals.
QByteArray makeProofPack(const QString& verdict, int excisionCount,
                         const QString& generatedAtUtc) {
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("glyphpdf-redaction-proof/1");
    root[QStringLiteral("generated_at_utc")] = generatedAtUtc;
    root[QStringLiteral("application")] = QStringLiteral("GlyphPDF test");
    root[QStringLiteral("verdict")] = verdict;
    root[QStringLiteral("error")] = QString();
    QJsonArray excisions;
    for (int i = 0; i < excisionCount; ++i) {
        QJsonObject e;
        e[QStringLiteral("page")] = i;
        excisions.append(e);
    }
    root[QStringLiteral("excisions")] = excisions;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

class TestPrintableSummary : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // The SafeSave commit-fault seam is process-global — reset it around EVERY
    // test so a failure mid-pin can never poison a later one.
    void init() {
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::None);
    }
    void cleanup() {
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::None);
    }

    // ── content pin: every comment + header fields + table of entries ────

    void contentPinCommentsHeaderFieldsAndTableOfEntries() {
        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("c1"), QStringLiteral("alice"), ReviewState::Open,      0,
                        QStringLiteral("Cover page needs the legal template"),
                        QStringLiteral("2026-09-09T10:00:00")),
            makeComment(QStringLiteral("c2"), QStringLiteral("bob"),   ReviewState::Accepted,  0,
                        QStringLiteral("Numbers verified against the ledger"),
                        QStringLiteral("2026-09-09T11:00:00")),
            makeComment(QStringLiteral("c3"), QStringLiteral("alice"), ReviewState::Rejected,  1,
                        QStringLiteral("Trademark usage is inconsistent"),
                        QStringLiteral("2026-09-09T12:00:00")),
            makeComment(QStringLiteral("c4"), QStringLiteral("carol"), ReviewState::Completed, 2,
                        QStringLiteral("Signature block confirmed"),
                        QStringLiteral("2026-09-09T13:00:00")),
        };
        const QString out = m_tmpDir.filePath(QStringLiteral("printable.pdf"));

        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     out, QStringLiteral("contract.pdf"), comments, {}, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);

        // Header fields: document name + generation timestamp + totals.
        QVERIFY2(text.contains(QStringLiteral("Review Summary")),
                 "Header title must be present");
        QVERIFY2(text.contains(QStringLiteral("contract.pdf")),
                 "The reviewed document's name must be present");
        QVERIFY2(text.contains(QStringLiteral("Generated: ")),
                 "Generation timestamp field must be present");
        QVERIFY2(text.contains(QStringLiteral("Comments: 4")),
                 "Comment total must appear in the header");
        QVERIFY2(text.contains(QStringLiteral("Distinct authors: 3")),
                 "Distinct-author markup count must appear in the header");

        // Every comment: status + author + timestamp + text.
        QVERIFY2(text.contains(QStringLiteral("[Open]")),      "status");
        QVERIFY2(text.contains(QStringLiteral("[Accepted]")),  "status");
        QVERIFY2(text.contains(QStringLiteral("[Rejected]")),  "status");
        QVERIFY2(text.contains(QStringLiteral("[Completed]")), "status");
        for (const QString& who : {QStringLiteral("alice"), QStringLiteral("bob"),
                                   QStringLiteral("carol")})
            QVERIFY2(text.contains(who), qPrintable(QStringLiteral("author %1").arg(who)));
        QVERIFY2(text.contains(QStringLiteral("Cover page needs the legal template")),
                 "comment text 1 must appear");
        QVERIFY2(text.contains(QStringLiteral("Numbers verified against the ledger")),
                 "comment text 2 must appear");
        QVERIFY2(text.contains(QStringLiteral("Trademark usage is inconsistent")),
                 "comment text 3 must appear");
        QVERIFY2(text.contains(QStringLiteral("Signature block confirmed")),
                 "comment text 4 must appear");

        // Table of entries: page groups with entry counts, numbered entries.
        QVERIFY2(text.contains(QStringLiteral("Contents")),
                 "A table of entries must open the document");
        QVERIFY2(text.contains(QStringLiteral("Page 1 — 2 entries")),
                 "Contents line for page group 1");
        QVERIFY2(text.contains(QStringLiteral("Page 2 — 1 entry")),
                 "Contents line for page group 2 (singular)");
        QVERIFY2(text.contains(QStringLiteral("Page 3 — 1 entry")),
                 "Contents line for page group 3");
        QVERIFY2(text.contains(QStringLiteral("4 entries on 3 page(s)")),
                 "Contents total line");
        QVERIFY2(text.contains(QStringLiteral("No. 1")), "entries are numbered 1..N");
        QVERIFY2(text.contains(QStringLiteral("No. 4")), "entries are numbered 1..N");

        // Single-sheet case still names its page furniture honestly.
        QVERIFY2(text.contains(QStringLiteral("Page 1 of 1")),
                 "Per-page footer with the sheet count");
    }

    // ── honest empty state ────────────────────────────────────────────────

    void emptyDocHonestState() {
        const QString out = m_tmpDir.filePath(QStringLiteral("printable_empty.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     out, QStringLiteral("nothing.pdf"), {}, {}, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);
        QVERIFY2(text.contains(QStringLiteral("No comments")),
                 "An empty review state must be STATED, not blank");
        QVERIFY2(text.contains(QStringLiteral("Comments: 0")),
                 "The zero total must be shown honestly");
        QVERIFY2(!text.contains(QStringLiteral("No. 1")),
                 "No fabricated entry numbering on an empty review state");
    }

    // ── pagination: 30+ comments, multiple sheets, ALL entries present ───

    void paginationBeyondOnePageKeepsEveryEntry() {
        QList<AnnotationItem> comments;
        for (int i = 0; i < 36; ++i) {
            comments.append(makeComment(
                QStringLiteral("p%1").arg(i),
                (i % 3 == 0) ? QStringLiteral("alice")
                             : ((i % 3 == 1) ? QStringLiteral("bob")
                                             : QStringLiteral("carol")),
                (i % 2) ? ReviewState::Accepted : ReviewState::Open,
                i % 6,
                QStringLiteral("Finding number %1: the annotated paragraph needs "
                               "a second review pass before sign-off.")
                    .arg(i),
                QStringLiteral("2026-09-09T%1:00:00").arg(i % 24, 2, 10, QLatin1Char('0'))));
        }
        const QString out = m_tmpDir.filePath(QStringLiteral("printable_long.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     out, QStringLiteral("big.pdf"), comments, {}, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        QVERIFY2(reader.pageCount() >= 2,
                 qPrintable(QStringLiteral("36 comments must paginate, got %1 page(s)")
                                .arg(reader.pageCount())));

        // ALL entries present after the spill.
        const QString text = wholeText(reader);
        for (int i = 0; i < 36; ++i)
            QVERIFY2(text.contains(QStringLiteral("Finding number %1:").arg(i)),
                     qPrintable(QStringLiteral("entry %1 must survive pagination").arg(i)));
        QVERIFY2(text.contains(QStringLiteral("Comments: 36")),
                 "Total must reflect every entry");

        // Footer furniture names the sheets end-to-end.
        QVERIFY2(text.contains(QStringLiteral("Page 1 of %1").arg(reader.pageCount())),
                 "First-sheet footer carries the sheet count");
        QVERIFY2(text.contains(QStringLiteral("Page %1 of %1").arg(reader.pageCount())),
                 "Last-sheet footer carries the sheet count");
        QVERIFY2(text.contains(QStringLiteral("No. 36")),
                 "Entry numbering reaches the last entry");
    }

    // ── redaction-proof honesty: listed when present, listed when not ─────

    void proofPackListedWhenPresent() {
        // The deterministic pack path RedactOperation writes beside a
        // committed redacted output: <dir>/<base>_redaction-proof.json.
        const QString doc = m_tmpDir.filePath(QStringLiteral("redacted-output.pdf"));
        const QString pack = m_tmpDir.filePath(
            QStringLiteral("redacted-output_redaction-proof.json"));
        QFile packFile(pack);
        QVERIFY2(packFile.open(QIODevice::WriteOnly), "write fixture pack");
        const QByteArray packJson = makeProofPack(
            QStringLiteral("PASS"), 3, QStringLiteral("2026-09-15T01:23:45Z"));
        QCOMPARE(packFile.write(packJson), qint64(packJson.size()));
        packFile.close();
        QVERIFY(QFile::exists(doc) == false || true); // doc itself need not exist

        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("r1"), QStringLiteral("alice"), ReviewState::Open, 0,
                        QStringLiteral("Check the excised region"),
                        QStringLiteral("2026-09-15T02:00:00")),
        };
        ReviewSummaryWriter::PrintOptions opts;
        opts.proofPackPath = pack;
        const QString out = m_tmpDir.filePath(QStringLiteral("printable_proof.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     out, QStringLiteral("redacted-output.pdf"), comments, opts, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);
        QVERIFY2(text.contains(QStringLiteral("Redaction proof: PASS")),
                 "The pack's verdict must be carried into the header");
        QVERIFY2(text.contains(QStringLiteral("3 excision(s)")),
                 "The pack's excision count must be carried");
        QVERIFY2(text.contains(QStringLiteral("2026-09-15T01:23:45Z")),
                 "The pack's OWN generation time must be shown (honest provenance, "
                 "never claimed fresh)");
    }

    void proofSectionListedAsUnavailableWhenAbsent() {
        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("u1"), QStringLiteral("alice"), ReviewState::Open, 0,
                        QStringLiteral("No proof pack anywhere"),
                        QStringLiteral("2026-09-15T02:00:00")),
        };
        const QString out = m_tmpDir.filePath(QStringLiteral("printable_noproof.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     out, QStringLiteral("plain.pdf"), comments, {}, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);
        QVERIFY2(text.contains(QStringLiteral("Redaction proof: not available")),
                 "An unavailable feature must be LISTED, not silently omitted");
    }

    // ── overwrite guard: destination only ever replaced via the transaction ──

    void overwriteGuardPreservesDestinationOnFailedCommit() {
        const QString dest = m_tmpDir.filePath(QStringLiteral("guarded.pdf"));
        const QByteArray sentinel = "PRECIOUS EXISTING DOCUMENT BYTES";
        {
            QFile f(dest);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "seed dest");
            f.write(sentinel);
        }

        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("g1"), QStringLiteral("alice"), ReviewState::Open, 0,
                        QStringLiteral("This write must not clobber the destination"),
                        QStringLiteral("2026-09-15T03:00:00")),
        };

        // Inject the commit failure: the candidate renders fine, the commit is
        // refused — the destination must remain byte-identical.
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        QString error;
        const bool ok = ReviewSummaryWriter::writePrintable(
            dest, QStringLiteral("guarded.pdf"), comments, {}, &error);
        QVERIFY2(!ok, "the guarded write must report the refused commit");
        QVERIFY2(!error.isEmpty(), "with a user-presentable reason");

        QFile after(dest);
        QVERIFY2(after.open(QIODevice::ReadOnly), "destination still readable");
        const QByteArray bytes = after.readAll();
        after.close();
        QVERIFY2(bytes == sentinel,
                 "A failed summary write must leave the existing destination "
                 "byte-identical — never overwrite without the transaction");
    }

    void overwriteGuardCommitReplacesWithValidPdf() {
        const QString dest = m_tmpDir.filePath(QStringLiteral("guarded2.pdf"));
        {
            QFile f(dest);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "seed dest");
            f.write("OLD CONTENT");
        }

        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("g2"), QStringLiteral("bob"), ReviewState::Accepted, 0,
                        QStringLiteral("Replacement goes through the candidate"),
                        QStringLiteral("2026-09-15T03:30:00")),
        };
        QString error;
        QVERIFY2(ReviewSummaryWriter::writePrintable(
                     dest, QStringLiteral("guarded2.pdf"), comments, {}, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY2(reader.loadDocument(dest),
                 "The committed destination is a real, readable PDF");
        QVERIFY2(wholeText(reader).contains(
                     QStringLiteral("Replacement goes through the candidate")),
                 "The committed destination carries the summary content");
    }

    // ── legacy seam compatibility: T2-3's write() keeps working ───────────

    void legacyWriteStillProducesReadableSummary() {
        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("l1"), QStringLiteral("alice"), ReviewState::Open, 0,
                        QStringLiteral("Legacy call path"),
                        QStringLiteral("2026-09-15T04:00:00")),
        };
        const QString out = m_tmpDir.filePath(QStringLiteral("legacy.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::write(out, QStringLiteral("legacy.pdf"),
                                            comments, &error),
                 qPrintable(error));
        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        QVERIFY2(wholeText(reader).contains(QStringLiteral("Legacy call path")),
                 "The T2-3 seam still produces the summary content");
    }
};

QTEST_MAIN(TestPrintableSummary)
#include "TestPrintableSummary.moc"
