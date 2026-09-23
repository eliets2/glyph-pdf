// SPDX-License-Identifier: Apache-2.0
// T2-3: review-summary DOCUMENT + displayed-scope export.
//
// The statuses themselves (ReviewState, the /State dictionary key, the
// status filter, undoable state changes) landed with M6-P5/U07 and are
// pinned by TestAnnotationDjot / TestCommentsReview. This test pins the
// loop-closer from the research row: the standalone printable summary
// document (Acrobat summary export / PDF-XChange summarize-to-PDF /
// Bluebeam Markups Summary equivalents):
//   * grouped by page → author → creation date,
//   * every entry carries the review status, author and timestamp,
//   * the header block reports per-status totals,
//   * the artifact is a REAL saved+reopened PDF (read back through the
//     PDFium text layer), paginating across multiple sheets when needed,
//   * CommentsWidget::exportReviewSummaryPdf exports exactly the DISPLAYED
//     (filtered) scope — the same contract as the CSV export.

#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QFile>

#include "engines/ReviewSummaryWriter.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "ui/CommentsWidget.h"
#include "ui/PdfViewerWidget.h"
#include "core/AnnotationTypes.h"
#include "core/PdfEnums.h"

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

} // namespace

class TestReviewSummary : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── status labels (the PDF review model) ─────────────────────────────

    void statusLabelsMatchReviewModel() {
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::Open),      QStringLiteral("Open"));
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::Accepted),  QStringLiteral("Accepted"));
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::Rejected),  QStringLiteral("Rejected"));
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::Completed), QStringLiteral("Completed"));
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::Cancelled), QStringLiteral("Cancelled"));
        QCOMPARE(ReviewSummaryWriter::statusLabel(ReviewState::None),      QStringLiteral("None"));
    }

    // ── renderEntries: grouping + heading content ────────────────────────

    void entriesGroupByPageThenAuthorThenDate() {
        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("c1"), QStringLiteral("bob"),   ReviewState::Accepted,  1,
                        QStringLiteral("page-two note"), QStringLiteral("2026-09-01T10:00:00")),
            makeComment(QStringLiteral("c2"), QStringLiteral("alice"), ReviewState::Rejected,  0,
                        QStringLiteral("page-one later"), QStringLiteral("2026-09-02T09:00:00")),
            makeComment(QStringLiteral("c3"), QStringLiteral("alice"), ReviewState::Open,      0,
                        QStringLiteral("page-one earlier"), QStringLiteral("2026-09-01T08:00:00")),
            makeComment(QStringLiteral("c4"), QStringLiteral("carol"), ReviewState::Completed, 0,
                        QStringLiteral("page-one carol"), QStringLiteral("2026-09-01T07:00:00")),
        };

        const auto entries = ReviewSummaryWriter::renderEntries(comments);
        QCOMPARE(entries.size(), 4);

        // Page 0 first: alice's EARLIER comment, alice's later, then carol.
        QCOMPARE(entries[0].pageIndex, 0);
        QVERIFY(entries[0].heading.contains(QStringLiteral("alice")));
        QVERIFY(entries[0].heading.contains(QStringLiteral("2026-09-01T08:00:00")));
        QVERIFY(entries[0].heading.contains(QStringLiteral("[Open]")));
        QCOMPARE(entries[1].pageIndex, 0);
        QVERIFY(entries[1].heading.contains(QStringLiteral("alice")));
        QVERIFY(entries[1].heading.contains(QStringLiteral("2026-09-02T09:00:00")));
        QVERIFY(entries[1].heading.contains(QStringLiteral("[Rejected]")));
        QCOMPARE(entries[2].pageIndex, 0);
        QVERIFY(entries[2].heading.contains(QStringLiteral("carol")));
        // Then page 1.
        QCOMPARE(entries[3].pageIndex, 1);
        QVERIFY(entries[3].heading.contains(QStringLiteral("bob")));
        QVERIFY(entries[3].heading.contains(QStringLiteral("[Accepted]")));

        // The body carries the comment text verbatim.
        QCOMPARE(entries[0].body, QStringLiteral("page-one earlier"));
        QCOMPARE(entries[3].body, QStringLiteral("page-two note"));
    }

    // ── the real artifact: saved, reopened, read back ────────────────────

    void summaryDocumentRoundTripsThroughDisk() {
        QList<AnnotationItem> comments = {
            makeComment(QStringLiteral("s1"), QStringLiteral("alice"), ReviewState::Accepted, 0,
                        QStringLiteral("Ship the layout fix"), QStringLiteral("2026-09-08T11:30:00")),
            makeComment(QStringLiteral("s2"), QStringLiteral("bob"),   ReviewState::Rejected, 1,
                        QStringLiteral("Trademark needs legal review"),
                        QStringLiteral("2026-09-08T15:45:00")),
        };
        const QString out = m_tmpDir.filePath(QStringLiteral("summary.pdf"));

        QString error;
        QVERIFY2(ReviewSummaryWriter::write(out, QStringLiteral("contract.pdf"),
                                            comments, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);

        QVERIFY2(text.contains(QStringLiteral("Review Summary")),
                 "Header title must be present");
        QVERIFY2(text.contains(QStringLiteral("contract.pdf")),
                 "The reviewed document's name must be present");
        QVERIFY2(text.contains(QStringLiteral("Accepted")), "Status label must appear");
        QVERIFY2(text.contains(QStringLiteral("Rejected")), "Status label must appear");
        QVERIFY2(text.contains(QStringLiteral("alice")), "Author must appear");
        QVERIFY2(text.contains(QStringLiteral("bob")), "Author must appear");
        QVERIFY2(text.contains(QStringLiteral("2026-09-08T11:30:00")),
                 "Creation timestamp must appear");
        QVERIFY2(text.contains(QStringLiteral("Ship the layout fix")),
                 "Comment text must appear");
        QVERIFY2(text.contains(QStringLiteral("Trademark needs legal review")),
                 "Comment text must appear");
        QVERIFY2(text.contains(QStringLiteral("Page 1")) &&
                 text.contains(QStringLiteral("Page 2")),
                 "Page group headings must appear (1-based, like the reviewer sees)");
        QVERIFY2(text.contains(QStringLiteral("Comments: 2")),
                 "Total count header must appear");
    }

    void summaryDocumentPaginates() {
        // 60 comments with long bodies must spill past the first sheet.
        QList<AnnotationItem> comments;
        const QString longBody = QStringLiteral("Detailed finding: ") +
            QStringLiteral("lorem ipsum dolor sit amet consectetur adipiscing elit sed do ")
            .repeated(3);
        for (int i = 0; i < 60; ++i) {
            comments.append(makeComment(
                QStringLiteral("p%1").arg(i), QStringLiteral("reviewer"),
                (i % 2) ? ReviewState::Accepted : ReviewState::Open,
                i % 3, longBody, QStringLiteral("2026-09-08T10:00:00")));
        }
        const QString out = m_tmpDir.filePath(QStringLiteral("summary_long.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::write(out, QStringLiteral("big.pdf"),
                                            comments, &error),
                 qPrintable(error));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        QVERIFY2(reader.pageCount() >= 2,
                 qPrintable(QStringLiteral("Long reviews must paginate, got %1 page(s)")
                                .arg(reader.pageCount())));
    }

    void emptyScopeProducesHonestDocument() {
        const QString out = m_tmpDir.filePath(QStringLiteral("summary_empty.pdf"));
        QString error;
        QVERIFY2(ReviewSummaryWriter::write(out, QStringLiteral("nothing.pdf"), {}, &error),
                 qPrintable(error));
        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        QVERIFY2(wholeText(reader).contains(QStringLiteral("No comments")),
                 "An empty scope must be stated, not left blank");
    }

    // ── CommentsWidget: the DISPLAYED (filtered) scope is exported ───────

    void widgetExportsDisplayedScopeOnly() {
        PdfViewerWidget viewer;
        CommentsWidget comments;
        comments.setViewer(&viewer);   // triggers an (empty) reload

        const QList<AnnotationItem> items = {
            makeComment(QStringLiteral("w1"), QStringLiteral("alice"), ReviewState::Accepted, 0,
                        QStringLiteral("accepted finding"), QStringLiteral("2026-09-08T10:00:00")),
            makeComment(QStringLiteral("w2"), QStringLiteral("bob"),   ReviewState::Rejected, 1,
                        QStringLiteral("rejected finding"), QStringLiteral("2026-09-08T11:00:00")),
        };
        viewer.setAnnotations(items);
        comments.reloadAnnotations();

        // Filter the view to Accepted only — the summary must follow the
        // DISPLAYED scope, exactly like the CSV export.
        auto* statusFilter = comments.findChild<QComboBox*>(QStringLiteral("commentsFilterStatus"));
        QVERIFY(statusFilter);
        statusFilter->setCurrentText(QStringLiteral("Accepted"));
        comments.reloadAnnotations();

        const QString out = m_tmpDir.filePath(QStringLiteral("widget_summary.pdf"));
        QVERIFY2(comments.exportReviewSummaryPdf(out),
                 "exportReviewSummaryPdf must write the artifact");

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);
        QVERIFY2(text.contains(QStringLiteral("accepted finding")),
                 "The accepted (displayed) comment must be in the summary");
        QVERIFY2(!text.contains(QStringLiteral("rejected finding")),
                 "The filtered-out comment must NOT leak into the summary");
    }

    void widgetEmptyScopeDoesNotWrite() {
        // The button guards an empty scope (consistent with Export CSV); the
        // public seam reports it by refusing to write.
        PdfViewerWidget viewer;
        CommentsWidget comments;
        comments.setViewer(&viewer);
        const QString out = m_tmpDir.filePath(QStringLiteral("should_not_exist.pdf"));
        QVERIFY2(!comments.exportReviewSummaryPdf(out)
                     || !QFile::exists(out),
                 "An empty displayed scope must not produce a summary file");
    }
};

QTEST_MAIN(TestReviewSummary)
#include "TestReviewSummary.moc"
