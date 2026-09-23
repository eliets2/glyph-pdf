// SPDX-License-Identifier: Apache-2.0
#include "ui/AutoBookmarkDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHeaderView>

AutoBookmarkDialog::AutoBookmarkDialog(const QString& pdfPath, QWidget* parent)
    : QDialog(parent)
    , m_pdfPath(pdfPath)
{
    setWindowTitle(tr("Auto-Bookmarks from Text Styles"));
    setModal(true);

    auto* col = new QVBoxLayout(this);

    // Honesty: the heuristic basis is disclosed, and nothing is written
    // until the user confirms the preview.
    m_disclosure = new QLabel(
        tr("Headings were detected heuristically: text noticeably larger than the "
           "document's body size, bold body-size text, and TOC pages (\"title … 12\" "
           "lines). This is a best-effort guess — review, edit the titles, uncheck "
           "anything wrong, then confirm. Nothing is written until you press "
           "Create Bookmarks."), this);
    m_disclosure->setObjectName(QStringLiteral("abDisclosure"));
    m_disclosure->setWordWrap(true);
    col->addWidget(m_disclosure);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("abCountLabel"));
    col->addWidget(m_countLabel);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("abTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Keep"), tr("Bookmark title"),
                                        tr("Page"), tr("Detected by")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 44);
    m_table->setColumnWidth(1, 240);
    m_table->setColumnWidth(2, 56);
    col->addWidget(m_table, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_okBtn = new QPushButton(tr("Create Bookmarks"), this);
    m_okBtn->setObjectName(QStringLiteral("abCreateButton"));
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    btnRow->addWidget(m_okBtn);
    btnRow->addWidget(m_cancelBtn);
    col->addLayout(btnRow);

    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // ── detection over the document's real text layer ──────────────────────
    QList<QList<HeadingOutlineDetector::TextLine>> pages;
    int detectedPages = 0;
    {
        PdfiumBackend backend;
        if (backend.loadDocument(m_pdfPath)) {
            const int pageCount = qMin(backend.pageCount(), 200);   // bound preview work
            for (int p = 0; p < pageCount; ++p) {
                pages.append(HeadingOutlineDetector::groupRuns(backend.extractPageTextRuns(p)));
                ++detectedPages;
            }
        }
    }
    const QList<HeadingOutlineDetector::Candidate> candidates =
        HeadingOutlineDetector::detect(pages);

    m_table->setRowCount(candidates.size());
    for (int i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];

        auto* keep = new QTableWidgetItem;
        keep->setCheckState(Qt::Checked);
        m_table->setItem(i, 0, keep);

        auto* title = new QTableWidgetItem(c.title);
        // The detected nesting level travels with the row so a confirmed
        // preview can rebuild the same hierarchy.
        title->setData(Qt::UserRole + 1, c.level);
        m_table->setItem(i, 1, title);

        auto* page = new QTableWidgetItem(QString::number(
            (c.fromToc ? c.tocTargetPage : c.pageIndex) + 1));
        page->setFlags(page->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 2, page);

        const QString source = c.fromToc
            ? tr("TOC pattern")
            : tr("font %1 pt").arg(c.fontSize, 0, 'f', 1);
        auto* srcItem = new QTableWidgetItem(source);
        srcItem->setFlags(srcItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 3, srcItem);
    }

    m_countLabel->setText(tr("%1 heading candidate(s) on %2 page(s) scanned.")
                              .arg(candidates.size()).arg(detectedPages));
    m_okBtn->setEnabled(!candidates.isEmpty());
}

QList<HeadingOutlineDetector::Candidate>
AutoBookmarkDialog::acceptedCandidates() const {
    QList<HeadingOutlineDetector::Candidate> out;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (m_table->item(i, 0)->checkState() != Qt::Checked) continue;

        HeadingOutlineDetector::Candidate c;
        c.title = m_table->item(i, 1)->text().trimmed();
        if (c.title.isEmpty()) continue;
        c.level = m_table->item(i, 1)->data(Qt::UserRole + 1).toInt();
        c.level = qBound(1, c.level, 3);
        // The destination comes from the Page column (the single source of
        // truth the user saw) — 1-based display → 0-based entry.
        bool ok = false;
        const int page = m_table->item(i, 2)->text().toInt(&ok);
        if (!ok || page < 1) continue;
        c.pageIndex = page - 1;
        c.tocTargetPage = page - 1;
        out.append(c);
    }
    return out;
}

QList<OutlineEntry> AutoBookmarkDialog::buildTree() const {
    return HeadingOutlineDetector::buildOutlineTree(acceptedCandidates());
}

QString AutoBookmarkDialog::candidateCountText() const {
    return m_countLabel ? m_countLabel->text() : QString();
}
