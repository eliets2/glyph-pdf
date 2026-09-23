// SPDX-License-Identifier: Apache-2.0
// U07 — comments review tools, built ON TOP of the existing annotation
// records (AnnotationItem / PdfViewerWidget::annotations — no second markup
// store). Pins:
//   * the three existing filters (status / author / date) COMBINE with AND
//     semantics and report an active-filter summary + result count,
//   * clear-filters restores the full result set,
//   * a table view presents the SAME filtered records (page/type/status/
//     author) with the selection preserved across the view-mode toggle,
//   * CSV export covers exactly the displayed scope with RFC-4180 escaping
//     (text containing `"` and `,` round-trips quoted),
//   * double-click navigation in BOTH presentations keeps using the existing
//     commentDoubleClicked(page) plumbing,
//   * the reworked tree assembly still threads replies (a reply nests under
//     its parent and is PROMOTED to top level when the parent is filtered
//     out — the M6-P5 semantics the U07 refactor had to preserve),
//   * the table's Page column sorts numerically, not lexicographically.
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QRegularExpression>
#include <QFile>

#include "ui/CommentsWidget.h"
#include "ui/PdfViewerWidget.h"
#include "core/AnnotationTypes.h"
#include "core/PdfEnums.h"

namespace {

AnnotationItem makeComment(const QString &id, const QString &author,
                           ReviewState state, int pageIndex,
                           const QString &text, const QString &created)
{
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

// Minimal RFC-4180 row parser for the round-trip assertion (quoted fields,
// doubled quotes, CRLF or LF line endings).
QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString cur;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
        } else if (c == QLatin1Char(',')) {
            fields.append(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.append(cur);
    return fields;
}

} // namespace

class TestCommentsReview : public QObject {
    Q_OBJECT

    PdfViewerWidget *m_viewer = nullptr;
    CommentsWidget *m_comments = nullptr;

    static QString todayIso()
    {
        return QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    static QString oldIso()
    {
        return QDateTime::currentDateTime().addDays(-40).toString(Qt::ISODate);
    }

    // 6-comment fixture:
    //   c1 Alice  Open      today   page 0
    //   c2 Bob    Open      today   page 1
    //   c3 Bob    Open      -40d    page 1
    //   c4 Alice  Accepted  today   page 2
    //   c5 Bob    Accepted  -40d    page 3
    //   c6 Carol  Rejected  today   page 3
    void seedSix()
    {
        QList<AnnotationItem> items;
        items << makeComment(QStringLiteral("c1"), QStringLiteral("Alice"),
                             ReviewState::Open, 0, QStringLiteral("first"), todayIso())
              << makeComment(QStringLiteral("c2"), QStringLiteral("Bob"),
                             ReviewState::Open, 1, QStringLiteral("second"), todayIso())
              << makeComment(QStringLiteral("c3"), QStringLiteral("Bob"),
                             ReviewState::Open, 1, QStringLiteral("third"), oldIso())
              << makeComment(QStringLiteral("c4"), QStringLiteral("Alice"),
                             ReviewState::Accepted, 2, QStringLiteral("fourth"), todayIso())
              << makeComment(QStringLiteral("c5"), QStringLiteral("Bob"),
                             ReviewState::Accepted, 3, QStringLiteral("fifth"), oldIso())
              << makeComment(QStringLiteral("c6"), QStringLiteral("Carol"),
                             ReviewState::Rejected, 3, QStringLiteral("sixth"), todayIso());
        m_viewer->setAnnotations(items);
        m_comments->reloadAnnotations();
    }

    QComboBox *statusFilter() const
    {
        return m_comments->findChild<QComboBox *>(QStringLiteral("commentsFilterStatus"));
    }
    QComboBox *authorFilter() const
    {
        return m_comments->findChild<QComboBox *>(QStringLiteral("commentsFilterAuthor"));
    }
    QComboBox *dateFilter() const
    {
        return m_comments->findChild<QComboBox *>(QStringLiteral("commentsFilterDate"));
    }
    QComboBox *viewToggle() const
    {
        return m_comments->findChild<QComboBox *>(QStringLiteral("commentsViewToggle"));
    }

    // Total rendered rows in the list presentation (top-level + replies).
    int treeRowCount() const
    {
        auto *tree = m_comments->findChild<QTreeWidget *>();
        if (!tree)
            return -1;
        int n = 0;
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            stack.append(tree->topLevelItem(i));
        while (!stack.isEmpty()) {
            QTreeWidgetItem *it = stack.takeFirst();
            ++n;
            for (int c = 0; c < it->childCount(); ++c)
                stack.append(it->child(c));
        }
        return n;
    }

    QTreeWidgetItem *findTreeItem(const QString &annoId) const
    {
        auto *tree = m_comments->findChild<QTreeWidget *>();
        if (!tree)
            return nullptr;
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            stack.append(tree->topLevelItem(i));
        while (!stack.isEmpty()) {
            QTreeWidgetItem *it = stack.takeFirst();
            if (it->data(0, Qt::UserRole).toString() == annoId)
                return it;
            for (int c = 0; c < it->childCount(); ++c)
                stack.append(it->child(c));
        }
        return nullptr;
    }

private slots:
    void init()
    {
        // QObject-derived test class cannot act as a QWidget parent — own
        // both widgets explicitly and delete them in cleanup().
        m_viewer = new PdfViewerWidget;
        m_comments = new CommentsWidget;
        m_comments->setViewer(m_viewer); // triggers an (empty) reload
    }
    void cleanup()
    {
        delete m_comments;
        delete m_viewer;
        m_comments = nullptr;
        m_viewer = nullptr;
    }

    // Filters combine with AND semantics; the summary label reports the
    // active-filter count and the "shown of total" result count.
    void filtersCombinePredictablyWithSummaryAndCount();

    // Clearing the filters restores every result and resets all combos.
    void clearFiltersRestoresAllResults();

    // The reported count equals the rows actually rendered, in BOTH
    // presentations, and the table shows the same filtered records.
    void resultCountMatchesVisibleRowsInBothViews();

    // CSV export: RFC-4180 escaping of `"` and `,`, and the exported rows
    // match the displayed scope exactly (filtered-out comments absent).
    void csvExportEscapesQuotesAndCommasAndMatchesScope();

    // Selection survives the list<->table toggle in both directions and
    // double-click in each view still drives commentDoubleClicked(page).
    void selectionPreservedAcrossViewToggle();

    // The reworked tree assembly keeps the M6-P5 reply semantics: replies
    // nest under surviving parents and are promoted to top level when the
    // parent is filtered out; the table shows the same records flat.
    void replyNestingPreservedInNewPresentation();

    // The Page column sorts numerically ("2" before "10+1"), which the
    // default string comparison would get wrong for larger reviews.
    void pageColumnSortsNumerically();
};

void TestCommentsReview::filtersCombinePredictablyWithSummaryAndCount()
{
    seedSix();
    QCOMPARE(m_comments->totalCommentCount(), 6);
    QCOMPARE(m_comments->visibleCommentCount(), 6);
    QCOMPARE(m_comments->activeFilterCount(), 0);
    QCOMPARE(m_comments->activeFilterSummary(),
             QStringLiteral("0 filters \u00b7 6 of 6 shown"));
    // Nothing to clear while no filter is active.
    auto *clearBtn = m_comments->findChild<QPushButton *>(
        QStringLiteral("commentsClearFilters"));
    QVERIFY(clearBtn);
    QVERIFY(!clearBtn->isEnabled());

    statusFilter()->setCurrentIndex(1); // Open
    QCOMPARE(m_comments->visibleCommentCount(), 3); // c1, c2, c3
    QCOMPARE(m_comments->activeFilterCount(), 1);
    QCOMPARE(m_comments->activeFilterSummary(),
             QStringLiteral("1 filter \u00b7 3 of 6 shown"));

    authorFilter()->setCurrentText(QStringLiteral("Bob"));
    QCOMPARE(m_comments->visibleCommentCount(), 2); // c2, c3 (AND with status)
    QCOMPARE(m_comments->activeFilterCount(), 2);
    QCOMPARE(m_comments->activeFilterSummary(),
             QStringLiteral("2 filters \u00b7 2 of 6 shown"));

    dateFilter()->setCurrentIndex(1); // Today (c3 is 40 days old)
    QCOMPARE(m_comments->visibleCommentCount(), 1); // c2 only
    QCOMPARE(m_comments->activeFilterCount(), 3);
    QCOMPARE(m_comments->activeFilterSummary(),
             QStringLiteral("3 filters \u00b7 1 of 6 shown"));
    QCOMPARE(m_comments->totalCommentCount(), 6); // total unchanged by filters
}

void TestCommentsReview::clearFiltersRestoresAllResults()
{
    seedSix();
    statusFilter()->setCurrentIndex(1);               // Open
    authorFilter()->setCurrentText(QStringLiteral("Alice"));
    dateFilter()->setCurrentIndex(3);                 // Last 30 days
    QCOMPARE(m_comments->activeFilterCount(), 3);
    QCOMPARE(m_comments->visibleCommentCount(), 1);   // c1

    m_comments->clearFilters();
    QCOMPARE(statusFilter()->currentIndex(), 0);
    QCOMPARE(authorFilter()->currentIndex(), 0);
    QCOMPARE(dateFilter()->currentIndex(), 0);
    QCOMPARE(m_comments->activeFilterCount(), 0);
    QCOMPARE(m_comments->visibleCommentCount(), 6);
    QCOMPARE(m_comments->activeFilterSummary(),
             QStringLiteral("0 filters \u00b7 6 of 6 shown"));

    // The Clear button drives the same path.
    authorFilter()->setCurrentText(QStringLiteral("Bob"));
    QCOMPARE(m_comments->visibleCommentCount(), 3);
    auto *clearBtn = m_comments->findChild<QPushButton *>(
        QStringLiteral("commentsClearFilters"));
    QVERIFY(clearBtn && clearBtn->isEnabled());
    clearBtn->click();
    QCOMPARE(m_comments->activeFilterCount(), 0);
    QCOMPARE(m_comments->visibleCommentCount(), 6);
    QCOMPARE(treeRowCount(), 6);
}

void TestCommentsReview::resultCountMatchesVisibleRowsInBothViews()
{
    seedSix();
    statusFilter()->setCurrentIndex(1); // Open
    QCOMPARE(m_comments->visibleCommentCount(), 3);
    QCOMPARE(treeRowCount(), 3);

    viewToggle()->setCurrentIndex(1); // Table presentation of the SAME records
    auto *table = m_comments->findChild<QTableWidget *>(
        QStringLiteral("commentsTable"));
    QVERIFY(table);
    QVERIFY(!table->isHidden());
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(m_comments->visibleCommentCount(), 3); // count follows the view

    // Plan-mandated columns exist: page/type/status/author.
    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c) {
        if (auto *h = table->horizontalHeaderItem(c))
            headers << h->text();
    }
    QVERIFY(headers.contains(QStringLiteral("Page")));
    QVERIFY(headers.contains(QStringLiteral("Type")));
    QVERIFY(headers.contains(QStringLiteral("Status")));
    QVERIFY(headers.contains(QStringLiteral("Author")));

    // Every rendered row maps to a live annotation of the filtered scope.
    QSet<QString> rowIds;
    for (int r = 0; r < table->rowCount(); ++r)
        rowIds.insert(table->item(r, 0)->data(Qt::UserRole).toString());
    const QSet<QString> expectedIds{
        QStringLiteral("c1"), QStringLiteral("c2"), QStringLiteral("c3") };
    QCOMPARE(rowIds, expectedIds);

    // Author page/type/status values agree with the underlying records.
    const int c3Row = [&] {
        for (int r = 0; r < table->rowCount(); ++r)
            if (table->item(r, 0)->data(Qt::UserRole).toString() == QStringLiteral("c3"))
                return r;
        return -1;
    }();
    QVERIFY(c3Row >= 0);
    QCOMPARE(table->item(c3Row, 0)->text(), QStringLiteral("2")); // 1-based page
    QCOMPARE(table->item(c3Row, 1)->text(), QStringLiteral("Comment"));
    QCOMPARE(table->item(c3Row, 2)->text(), QStringLiteral("Open"));
    QCOMPARE(table->item(c3Row, 3)->text(), QStringLiteral("Bob"));

    QCOMPARE(m_comments->visibleCommentCount(), 3);
}

void TestCommentsReview::csvExportEscapesQuotesAndCommasAndMatchesScope()
{
    // Field-level escaping first: quotes doubled, field quoted when needed.
    QCOMPARE(CommentsWidget::csvEscapeField(QStringLiteral("plain")),
             QStringLiteral("plain"));
    QCOMPARE(CommentsWidget::csvEscapeField(QStringLiteral("He said \"wait\", go")),
             QStringLiteral("\"He said \"\"wait\"\", go\""));
    QCOMPARE(CommentsWidget::csvEscapeField(QStringLiteral("line1\nline2")),
             QStringLiteral("\"line1\nline2\""));

    // Capture "now" ONCE: the assertion must compare against the seeded
    // timestamp, not a re-derived one (a second-boundary crossing between
    // seed and assert would otherwise fail spuriously).
    const QString nowIso = todayIso();
    QList<AnnotationItem> items;
    const QString nasty = QStringLiteral("He said \"wait\", go");
    items << makeComment(QStringLiteral("n1"), QStringLiteral("Alice"),
                         ReviewState::Open, 0, nasty, nowIso)
          << makeComment(QStringLiteral("n2"), QStringLiteral("Bob"),
                         ReviewState::Accepted, 1, QStringLiteral("ok"), nowIso)
          << makeComment(QStringLiteral("n3"), QStringLiteral("Carol"),
                         ReviewState::Open, 2, QStringLiteral("plain"), nowIso);
    m_viewer->setAnnotations(items);
    m_comments->reloadAnnotations();

    // Export the DISPLAYED scope only: filter to Open (n1 + n3).
    statusFilter()->setCurrentIndex(1);
    QCOMPARE(m_comments->visibleCommentCount(), 2);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath("comments.csv");
    QVERIFY(m_comments->exportDisplayedCsv(path));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString body = QString::fromUtf8(f.readAll());
    f.close();

    const QStringList lines = body.split(QRegularExpression("\r?\n"),
                                         Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 3); // header + 2 displayed rows (n2 absent)
    QCOMPARE(lines.at(0),
             QStringLiteral("Page,Type,Status,Author,Date,Text"));

    const QStringList n1 = parseCsvLine(lines.at(1));
    QCOMPARE(n1.size(), 6);
    QCOMPARE(n1.at(0), QStringLiteral("1"));
    QCOMPARE(n1.at(1), QStringLiteral("Comment"));
    QCOMPARE(n1.at(2), QStringLiteral("Open"));
    QCOMPARE(n1.at(3), QStringLiteral("Alice"));
    QCOMPARE(n1.at(4), nowIso);
    // The escaped text round-trips to the original `"` and `,` content.
    QCOMPARE(n1.at(5), nasty);

    const QStringList n3 = parseCsvLine(lines.at(2));
    QCOMPARE(n3.at(5), QStringLiteral("plain"));

    // Filtered-out record must not leak into the export.
    QVERIFY(!body.contains(QStringLiteral("n2")));
    QVERIFY(!body.contains(QStringLiteral("Bob")));
    QVERIFY(!body.contains(QStringLiteral("Accepted")));
}

void TestCommentsReview::selectionPreservedAcrossViewToggle()
{
    seedSix();
    auto *tree = m_comments->findChild<QTreeWidget *>();
    QVERIFY(tree);

    QTreeWidgetItem *c2 = findTreeItem(QStringLiteral("c2"));
    QVERIFY(c2);
    tree->setCurrentItem(c2);
    QCOMPARE(m_comments->selectedAnnotationId(), QStringLiteral("c2"));

    // Existing list-presentation navigation plumbing still fires.
    QSignalSpy spy(m_comments, &CommentsWidget::commentDoubleClicked);
    QVERIFY(spy.isValid());
    emit tree->itemDoubleClicked(c2, 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 1); // c2 lives on 0-based page 1

    // Toggle to the table: same annotation stays selected.
    viewToggle()->setCurrentIndex(1);
    QCOMPARE(m_comments->selectedAnnotationId(), QStringLiteral("c2"));
    auto *table = m_comments->findChild<QTableWidget *>(
        QStringLiteral("commentsTable"));
    QVERIFY(table);
    QVERIFY(table->currentRow() >= 0);
    QCOMPARE(table->item(table->currentRow(), 0)->data(Qt::UserRole).toString(),
             QStringLiteral("c2"));

    // Table double-click drives the SAME existing navigation signal.
    emit table->cellDoubleClicked(table->currentRow(), 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 1);

    // Toggle back to the list: selection restored to c2.
    viewToggle()->setCurrentIndex(0);
    QCOMPARE(m_comments->selectedAnnotationId(), QStringLiteral("c2"));
    QVERIFY(tree->currentItem() != nullptr);
    QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toString(),
             QStringLiteral("c2"));
}

void TestCommentsReview::replyNestingPreservedInNewPresentation()
{
    // Thread fixture: p1 <- r1 (nested), p2 <- r2 (nested).
    AnnotationItem p1 = makeComment(QStringLiteral("p1"), QStringLiteral("Alice"),
                                    ReviewState::Open, 0,
                                    QStringLiteral("parent one"), todayIso());
    AnnotationItem r1 = makeComment(QStringLiteral("r1"), QStringLiteral("Bob"),
                                    ReviewState::Accepted, 0,
                                    QStringLiteral("reply one"), todayIso());
    r1.parentId = QStringLiteral("p1");
    AnnotationItem p2 = makeComment(QStringLiteral("p2"), QStringLiteral("Alice"),
                                    ReviewState::Open, 1,
                                    QStringLiteral("parent two"), todayIso());
    AnnotationItem r2 = makeComment(QStringLiteral("r2"), QStringLiteral("Bob"),
                                    ReviewState::Accepted, 1,
                                    QStringLiteral("reply two"), todayIso());
    r2.parentId = QStringLiteral("p2");
    m_viewer->setAnnotations({p1, r1, p2, r2});
    m_comments->reloadAnnotations();

    QCOMPARE(m_comments->totalCommentCount(), 4);
    QCOMPARE(m_comments->visibleCommentCount(), 4);
    QCOMPARE(treeRowCount(), 4);

    // A surviving parent keeps its reply nested beneath it.
    QTreeWidgetItem *r1Item = findTreeItem(QStringLiteral("r1"));
    QVERIFY(r1Item);
    QVERIFY(r1Item->parent() != nullptr);
    QCOMPARE(r1Item->parent()->data(0, Qt::UserRole).toString(),
             QStringLiteral("p1"));

    // Filter to Accepted: only the replies survive. Each must be PROMOTED to
    // a top-level node — a matching reply is never hidden by a filtered-out
    // parent (the semantic the U07 list/table refactor had to preserve).
    statusFilter()->setCurrentIndex(2); // Accepted
    QCOMPARE(m_comments->visibleCommentCount(), 2);
    QCOMPARE(treeRowCount(), 2);
    QTreeWidgetItem *r1Filtered = findTreeItem(QStringLiteral("r1"));
    QVERIFY(r1Filtered);
    QVERIFY2(r1Filtered->parent() == nullptr,
             "a reply whose parent was filtered out must be promoted to top level");
    QVERIFY(findTreeItem(QStringLiteral("r2")) != nullptr);
    QVERIFY(findTreeItem(QStringLiteral("p1")) == nullptr);

    // The table is another presentation of the SAME reply records, flat.
    viewToggle()->setCurrentIndex(1);
    auto *table = m_comments->findChild<QTableWidget *>(
        QStringLiteral("commentsTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(m_comments->visibleCommentCount(), 2);
    QSet<QString> rowIds;
    for (int r = 0; r < table->rowCount(); ++r)
        rowIds.insert(table->item(r, 0)->data(Qt::UserRole).toString());
    const QSet<QString> expectedReplyIds{
        QStringLiteral("r1"), QStringLiteral("r2")};
    QCOMPARE(rowIds, expectedReplyIds);
}

void TestCommentsReview::pageColumnSortsNumerically()
{
    // Pages 0, 1 and 10 render as "1", "2" and "11". Under the default
    // string comparison an ascending sort yields "1", "11", "2" — the
    // numeric PageSortItem must instead yield "1", "2", "11".
    QList<AnnotationItem> items;
    items << makeComment(QStringLiteral("s1"), QStringLiteral("Alice"),
                         ReviewState::Open, 0, QStringLiteral("p0"), todayIso())
          << makeComment(QStringLiteral("s2"), QStringLiteral("Alice"),
                         ReviewState::Open, 1, QStringLiteral("p1"), todayIso())
          << makeComment(QStringLiteral("s10"), QStringLiteral("Alice"),
                         ReviewState::Open, 10, QStringLiteral("p10"), todayIso());
    m_viewer->setAnnotations(items);
    m_comments->reloadAnnotations();

    viewToggle()->setCurrentIndex(1);
    auto *table = m_comments->findChild<QTableWidget *>(
        QStringLiteral("commentsTable"));
    QVERIFY(table);

    const auto columnTexts = [table]() {
        QStringList texts;
        for (int r = 0; r < table->rowCount(); ++r)
            texts << table->item(r, 0)->text();
        return texts;
    };

    table->sortItems(0, Qt::AscendingOrder);
    QCOMPARE(columnTexts(), (QStringList{QStringLiteral("1"),
                                         QStringLiteral("2"),
                                         QStringLiteral("11")}));
    table->sortItems(0, Qt::DescendingOrder);
    QCOMPARE(columnTexts(), (QStringList{QStringLiteral("11"),
                                         QStringLiteral("2"),
                                         QStringLiteral("1")}));
}

QTEST_MAIN(TestCommentsReview)
#include "TestCommentsReview.moc"
