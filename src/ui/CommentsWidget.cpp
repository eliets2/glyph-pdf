// SPDX-License-Identifier: Apache-2.0
#include "ui/CommentsWidget.h"
#include "ui/PdfViewerWidget.h"
#include "core/AnnotationTypes.h"
#include "core/AppContext.h"
#include "ui/EditAnnotationCommand.h"
#include "engines/DocumentSession.h"
#include "pdfws_djot/DjotToRichTextXhtml.h"
#include "util/GpTheme.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QUuid>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QPixmap>
#include <QHash>
#include <QDate>
#include <QDateTime>
#include <QUndoStack>
#include <QPalette>
#include <QApplication>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <functional>
#include <QToolButton>
#include <QTextCursor>
#include <QInputDialog>

namespace {

static const QColor kAvatarPalette[] = {
    QColor(0x3B, 0x82, 0xF6), // blue
    QColor(0xEF, 0x44, 0x44), // red
    QColor(0x10, 0xB9, 0x81), // emerald
    QColor(0xF5, 0x9E, 0x0B), // amber
    QColor(0x8B, 0x5C, 0xF6), // violet
    QColor(0xEC, 0x48, 0x99), // pink
    QColor(0x06, 0xB6, 0xD4), // cyan
    QColor(0xF9, 0x73, 0x16), // orange
};
constexpr int kPaletteSize = sizeof(kAvatarPalette) / sizeof(kAvatarPalette[0]);

QPixmap generateAvatar(const QString &authorName, int size = 28)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Deterministic color from author name
    uint hash = qHash(authorName);
    QColor bg = kAvatarPalette[hash % kPaletteSize];

    // Draw circle
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);

    // Draw initial letter
    QChar letter = authorName.isEmpty() ? QChar('?') : authorName.at(0).toUpper();
    p.setPen(Qt::white);
    QFont f;
    f.setPixelSize(size * 0.52);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(letter));
    p.end();

    return pm;
}

QString reviewStateLabel(ReviewState s)
{
    switch (s) {
    case ReviewState::Open:      return QObject::tr("Open");
    case ReviewState::Accepted:  return QObject::tr("Accepted");
    case ReviewState::Rejected:  return QObject::tr("Rejected");
    case ReviewState::Completed: return QObject::tr("Completed");
    case ReviewState::Cancelled: return QObject::tr("Cancelled");
    default:                     return QObject::tr("None");
    }
}

QColor reviewStateColor(ReviewState s)
{
    switch (s) {
    case ReviewState::Open:      return QColor(0x3B, 0x82, 0xF6);
    case ReviewState::Accepted:  return QColor(0x10, 0xB9, 0x81);
    case ReviewState::Rejected:  return QColor(0xEF, 0x44, 0x44);
    case ReviewState::Completed: return QColor(0x8B, 0x5C, 0xF6);
    case ReviewState::Cancelled: return QColor(0x94, 0xA3, 0xB8);
    default:                     return QColor(0x64, 0x74, 0x8B);
    }
}

// U07: human type label for the table presentation / CSV export. The review
// tools manage AddComment records today; the switch keeps the table honest if
// the filtered scope is ever broadened to other persisted markup modes.
QString annotationTypeLabel(ToolMode m)
{
    switch (m) {
    case ToolMode::AddComment:   return QObject::tr("Comment");
    case ToolMode::AddTextBox:   return QObject::tr("Text box");
    case ToolMode::Highlight:    return QObject::tr("Highlight");
    case ToolMode::DrawFreehand: return QObject::tr("Ink");
    default:                     return QObject::tr("Annotation");
    }
}

// U07: the Page column must sort numerically — the default QTableWidgetItem
// string comparison orders a 10+ page document as "1", "10", "2", which is
// wrong exactly for the larger reviews the table presentation targets. The
// 0-based pageIndex is already carried in Qt::UserRole + 1, so compare that.
class PageSortItem final : public QTableWidgetItem
{
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole + 1).toInt() < other.data(Qt::UserRole + 1).toInt();
    }
};

} // anonymous namespace

CommentsWidget::CommentsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Thread"), this);
    // T-01: use palette role instead of hardcoded dark hex so the label renders
    // correctly in Light and High-Contrast themes.
    title->setStyleSheet("font-weight: 700; font-size: 11px; text-transform: uppercase; letter-spacing: 0.6px;");
    title->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(title);

    auto *filterLayout = new QHBoxLayout();
    m_filterStatus = new QComboBox(this);
    m_filterStatus->setObjectName(QStringLiteral("commentsFilterStatus"));
    m_filterStatus->addItems({tr("All"), tr("Open"), tr("Accepted"),
                              tr("Rejected"), tr("Completed"), tr("Cancelled")});
    m_filterAuthor = new QComboBox(this);
    m_filterAuthor->setObjectName(QStringLiteral("commentsFilterAuthor"));
    m_filterAuthor->addItem(tr("All Authors"));
    // M6-P5 D1: date filter — recency buckets computed against the annotation's
    // ISO-8601 creationDate. "All Dates" disables the filter.
    m_filterDate = new QComboBox(this);
    m_filterDate->setObjectName(QStringLiteral("commentsFilterDate"));
    m_filterDate->addItems({tr("All Dates"), tr("Today"), tr("Last 7 days"),
                            tr("Last 30 days")});
    filterLayout->addWidget(m_filterStatus);
    filterLayout->addWidget(m_filterAuthor);
    filterLayout->addWidget(m_filterDate);
    layout->addLayout(filterLayout);

    // ── U07: active-filter summary, view-mode toggle, clear + export ─────
    // The summary always names the active-filter count and the displayed
    // result count ("2 filters · 5 of 12 shown"); the clear action resets
    // every combo; the toggle switches between two presentations of the SAME
    // records; Export CSV writes the displayed scope only.
    auto *summaryLayout = new QHBoxLayout();
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("commentsSummaryLabel"));
    m_summaryLabel->setForegroundRole(QPalette::PlaceholderText);
    summaryLayout->addWidget(m_summaryLabel);
    summaryLayout->addStretch();

    m_viewMode = new QComboBox(this);
    m_viewMode->setObjectName(QStringLiteral("commentsViewToggle"));
    m_viewMode->setToolTip(tr("Switch between the thread list and the summary table"));
    m_viewMode->addItems({tr("List"), tr("Table")});
    summaryLayout->addWidget(m_viewMode);

    m_clearBtn = new QPushButton(tr("Clear Filters"), this);
    m_clearBtn->setObjectName(QStringLiteral("commentsClearFilters"));
    m_clearBtn->setToolTip(tr("Reset status, author and date filters"));
    m_clearBtn->setEnabled(false);
    summaryLayout->addWidget(m_clearBtn);

    auto *exportBtn = new QToolButton(this);
    exportBtn->setText(tr("Export CSV\u2026"));
    exportBtn->setObjectName(QStringLiteral("commentsExportCsv"));
    exportBtn->setToolTip(tr("Export the displayed comments to a CSV file"));
    summaryLayout->addWidget(exportBtn);
    layout->addLayout(summaryLayout);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setWordWrap(true);
    layout->addWidget(m_tree, 1);

    // U07: table presentation of the SAME filtered annotation records —
    // page/type/status/author (+ date/text) columns over AnnotationItem, not
    // a second markup store.
    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("commentsTable"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("Page"), tr("Type"), tr("Status"),
                                        tr("Author"), tr("Date"), tr("Text")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    m_table->setColumnWidth(0, 44);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->hide();
    layout->addWidget(m_table, 1);

    // T-01: composer background and input colors use palette roles so they adapt
    // to Light and High-Contrast themes instead of the former hardcoded dark hex.
    auto *composer = new QWidget(this);
    composer->setObjectName("composer");
    composer->setAutoFillBackground(true);
    // Use the "mid" role as a slightly-offset surface (equivalent to bg2/bg3)
    // — this is already set by the QSS base rules per theme so we just let it inherit.
    auto *cLyt = new QVBoxLayout(composer);
    cLyt->setContentsMargins(8,8,8,8);
    cLyt->setSpacing(6);

    m_author = new QLineEdit(this);
    m_author->setPlaceholderText(tr("Your Name"));
    // No inline stylesheet: the QSS rules for QLineEdit already apply the
    // correct theme-aware background/border/color for each theme.
    cLyt->addWidget(m_author);

    // ── M6-P5: Djot formatting toolbar ──────────────────────────────────
    auto* djotToolbar = new QWidget(composer);
    auto* toolbarRow  = new QHBoxLayout(djotToolbar);
    toolbarRow->setContentsMargins(0, 0, 0, 0);
    toolbarRow->setSpacing(2);

    const QString toolBtnSheet = QStringLiteral(
        "QToolButton {"
        "  background: #2b2d30; border: 1px solid #393b40; border-radius: 3px;"
        "  color: #a8abb0; font-family: 'Consolas', monospace; font-size: 11px;"
        "  min-width: 22px; min-height: 20px; padding: 0 4px;"
        "}"
        "QToolButton:hover { background: rgba(255,140,66,0.15); color: #ff8c42; }");

    auto addToolBtn = [&](const QString& label, const QString& tip,
                          std::function<void()> handler) {
        auto* b = new QToolButton;
        b->setText(label);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(toolBtnSheet);
        QObject::connect(b, &QToolButton::clicked, this, handler);
        toolbarRow->addWidget(b);
        return b;
    };
    addToolBtn(QStringLiteral("B"),   tr("Bold (**strong**)"),    [this]() { wrapSelection(QStringLiteral("**"), QStringLiteral("**")); });
    addToolBtn(QStringLiteral("I"),   tr("Italic (_emphasis_)"),  [this]() { wrapSelection(QStringLiteral("_"),  QStringLiteral("_")); });
    addToolBtn(QStringLiteral("</>"), tr("Inline code (`code`)"), [this]() { wrapSelection(QStringLiteral("`"),  QStringLiteral("`")); });
    addToolBtn(QStringLiteral("\xF0\x9F\x94\x97"), tr("Link ([text](url))"), [this]() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, tr("Insert link"),
                                                  tr("URL:"), QLineEdit::Normal,
                                                  QStringLiteral("https://"), &ok);
        if (ok && !url.isEmpty())
            wrapSelection(QStringLiteral("["), QStringLiteral("](") + url + QStringLiteral(")"));
    });
    addToolBtn(QStringLiteral("\xE2\x80\xA2"), tr("List item (- )"), [this]() { insertLinePrefix(QStringLiteral("- ")); });
    addToolBtn(QStringLiteral("H"),  tr("Heading (# )"),             [this]() { insertLinePrefix(QStringLiteral("# ")); });
    toolbarRow->addStretch();
    cLyt->addWidget(djotToolbar);

    m_editor = new QTextEdit(this);
    m_editor->setPlaceholderText(tr("Add a comment or reply..."));
    m_editor->setFixedHeight(60);
    // No inline stylesheet: the QSS rules for QTextEdit cover all three themes.
    cLyt->addWidget(m_editor);

    // ── M6-P5: Live Djot preview ──────────────────────────────────────────
    m_djotPreview = new QTextEdit(composer);
    m_djotPreview->setReadOnly(true);
    m_djotPreview->setFixedHeight(64);
    m_djotPreview->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: #2b2d30; border: 1px dashed #393b40; border-radius: 4px;"
        "  color: #dfe1e5; font-family: 'Manrope', sans-serif; font-size: 10.5px; padding: 6px;"
        "}"));
    cLyt->addWidget(m_djotPreview);

    QObject::connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        refreshDjotPreview();
    });

    auto *btnLyt = new QHBoxLayout;
    btnLyt->addStretch();
    m_addBtn = new QPushButton(tr("Post"), this);
    // Use accent color from GpTheme for the Post button so it is visible in all themes.
    {
        const QColor accent = gp::Theme::accent();
        const QString isDark = (gp::Theme::current() == gp::Theme::Light) ? "#ffffff" : "#1a1b1e";
        m_addBtn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; border: none; "
                    "border-radius: 4px; padding: 4px 12px; font-weight: 600; }")
                .arg(accent.name(), isDark));
    }
    btnLyt->addWidget(m_addBtn);

    auto *replyBtn = new QPushButton(tr("Reply"), this);
    // Use okGreen token — theme-consistent; leave text white (sufficient contrast on all themes).
    {
        const QColor ok = gp::Theme::okGreen();
        replyBtn->setStyleSheet(
            QString("QPushButton { background: %1; color: #ffffff; border: none; "
                    "border-radius: 4px; padding: 4px 12px; font-weight: 600; }")
                .arg(ok.name()));
    }
    btnLyt->addWidget(replyBtn);

    cLyt->addLayout(btnLyt);
    layout->addWidget(composer);

    connect(m_addBtn, &QPushButton::clicked, this, &CommentsWidget::addComment);
    connect(replyBtn, &QPushButton::clicked, this, &CommentsWidget::replyToComment);
    connect(m_filterStatus, &QComboBox::currentTextChanged, this, &CommentsWidget::refreshList);
    connect(m_filterAuthor, &QComboBox::currentTextChanged, this, &CommentsWidget::refreshList);
    connect(m_filterDate, &QComboBox::currentIndexChanged, this, &CommentsWidget::refreshList);

    // ── U07 wiring ─────────────────────────────────────────────────────────
    // View-mode toggle: rebuild only the active presentation, preserving the
    // selected annotation id across the switch. The combo index has already
    // flipped when this fires, so the selection is read from the OUTGOING
    // presentation explicitly.
    connect(m_viewMode, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool incomingTable = (index == 1);
        const QString selId = selectionId(!incomingTable);
        m_table->setVisible(incomingTable);
        m_tree->setVisible(!incomingTable);
        rebuildActiveView();
        restoreSelection(selId);
        updateFilterSummary();
    });
    connect(m_clearBtn, &QPushButton::clicked, this, &CommentsWidget::clearFilters);
    connect(exportBtn, &QToolButton::clicked, this, [this]() {
        if (m_lastFiltered.isEmpty()) return;
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Export Comments CSV"),
            QStringLiteral("comments.csv"),
            tr("CSV files (*.csv);;All files (*)"));
        if (path.isEmpty()) return;
        exportDisplayedCsv(path);
    });
    // Table activation reuses the SAME navigation plumbing as the list:
    // commentDoubleClicked(pageIndex) → Sidebar → viewer->goToPage().
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        Q_UNUSED(column)
        if (!m_table->item(row, 0)) return;
        bool ok = false;
        const int page = m_table->item(row, 0)->data(Qt::UserRole + 1).toInt(&ok);
        if (ok && page >= 0)
            emit commentDoubleClicked(page);
    });

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_tree->itemAt(pos);
        if (!item) return;
        QString annoId = item->data(0, Qt::UserRole).toString();
        if (annoId.isEmpty()) return;

        QMenu menu(this);
        // T-01: clear the hardcoded dark inline stylesheet so the context menu
        // inherits the theme QSS rules (QMenu selector) for all three themes.
        menu.setStyleSheet(QString());

        auto *actOpen      = menu.addAction(tr("Mark Open"));
        auto *actAccepted   = menu.addAction(tr("Mark Accepted"));
        auto *actRejected   = menu.addAction(tr("Mark Rejected"));
        auto *actCompleted  = menu.addAction(tr("Mark Completed"));
        auto *actCancelled  = menu.addAction(tr("Mark Cancelled"));

        // M6-P5 D3: route every state change through applyReviewState so it is
        // pushed onto the shared QUndoStack via EditAnnotationCommand (undoable +
        // marks the document dirty). The QMenu is non-modal per constraint.
        connect(actOpen,      &QAction::triggered, this, [=]{ applyReviewState(annoId, ReviewState::Open); });
        connect(actAccepted,  &QAction::triggered, this, [=]{ applyReviewState(annoId, ReviewState::Accepted); });
        connect(actRejected,  &QAction::triggered, this, [=]{ applyReviewState(annoId, ReviewState::Rejected); });
        connect(actCompleted, &QAction::triggered, this, [=]{ applyReviewState(annoId, ReviewState::Completed); });
        connect(actCancelled, &QAction::triggered, this, [=]{ applyReviewState(annoId, ReviewState::Cancelled); });

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        Q_UNUSED(column)
        bool ok;
        int p = item->data(0, Qt::UserRole + 1).toInt(&ok);
        if (ok && p >= 0) {
            emit commentDoubleClicked(p);
        }
    });

    // U07: seed the summary before the first reload so the label never reads
    // as empty (e.g. a widget constructed without a viewer).
    updateFilterSummary();
}

void CommentsWidget::setViewer(PdfViewerWidget *viewer)
{
    m_viewer = viewer;
    reloadAnnotations();
}

void CommentsWidget::setContext(const AppContext *ctx)
{
    m_ctx = ctx;
}

void CommentsWidget::setDocumentFile(const QString &filePath)
{
    m_filePath = filePath;
}

void CommentsWidget::setCurrentPage(int page)
{
    m_currentPage = page;
}

void CommentsWidget::reloadAnnotations()
{
    if (!m_viewer) return;
    // U07: preserve the selection across the rebuild (e.g. after a review
    // state change or a composer post).
    const QString selId = selectedAnnotationId();

    m_allComments.clear();
    const QList<AnnotationItem> items = m_viewer->annotations();
    for (const auto &anno : items) {
        if (anno.mode == ToolMode::AddComment)
            m_allComments.append(anno);
    }
    m_totalComments = m_allComments.size();
    m_lastFiltered = applyFilters();

    rebuildActiveView();
    restoreSelection(selId);
    updateFilterSummary();
}

void CommentsWidget::refreshList()
{
    reloadAnnotations();
}

// Map a status-filter combo label to the ReviewState it selects.
// Returns true when the annotation's state passes the active filter.
static bool statusPasses(const QString &statusFilter, ReviewState s)
{
    if (statusFilter == QObject::tr("All")) return true;
    if (statusFilter == QObject::tr("Open"))      return s == ReviewState::Open;
    if (statusFilter == QObject::tr("Accepted"))  return s == ReviewState::Accepted;
    if (statusFilter == QObject::tr("Rejected"))  return s == ReviewState::Rejected;
    if (statusFilter == QObject::tr("Completed")) return s == ReviewState::Completed;
    if (statusFilter == QObject::tr("Cancelled")) return s == ReviewState::Cancelled;
    return true;
}

// Date-recency filter. index: 0=All, 1=Today, 2=Last 7 days, 3=Last 30 days.
// creationDate is stored ISO-8601 (Qt::ISODate); a blank/unparseable date
// only passes the "All Dates" bucket.
static bool datePasses(int dateIndex, const QString &creationDate)
{
    if (dateIndex <= 0) return true;
    const QDateTime dt = QDateTime::fromString(creationDate, Qt::ISODate);
    if (!dt.isValid()) return false;
    const qint64 days = dt.date().daysTo(QDate::currentDate());
    if (days < 0) return true; // future-dated: never hide
    switch (dateIndex) {
    case 1:  return days == 0;   // Today
    case 2:  return days <= 7;   // Last 7 days
    case 3:  return days <= 30;  // Last 30 days
    default: return true;
    }
}

// U07: run the status/author/date filters over the cached AddComment records.
// All three filters combine with AND semantics; "All"/"All Authors"/"All
// Dates" disable their respective clause.
QList<AnnotationItem> CommentsWidget::applyFilters() const
{
    const QString statusFilter = m_filterStatus ? m_filterStatus->currentText() : QString();
    const QString authorFilter = m_filterAuthor ? m_filterAuthor->currentText() : QString();
    const int dateIndex = m_filterDate ? m_filterDate->currentIndex() : 0;

    QList<AnnotationItem> out;
    for (const auto &anno : m_allComments) {
        if (!statusPasses(statusFilter, anno.reviewState)) continue;
        if (authorFilter != tr("All Authors") && authorFilter != anno.author) continue;
        if (!datePasses(dateIndex, anno.creationDate)) continue;
        out.append(anno);
    }
    return out;
}

void CommentsWidget::rebuildActiveView()
{
    const bool tableMode = m_viewMode && m_viewMode->currentIndex() == 1;
    if (tableMode)
        rebuildTable();
    else
        buildTree();
}

void CommentsWidget::buildTree()
{
    m_tree->clear();
    m_filterAuthor->blockSignals(true);
    QString currentAuthor = m_filterAuthor->currentText();
    m_filterAuthor->clear();
    m_filterAuthor->addItem(tr("All Authors"));

    QSet<QString> authors;
    QMap<QString, QTreeWidgetItem*> itemMap;

    // The author combo always offers every author in the document, even when
    // the author filter currently hides some of them.
    for (const auto &anno : m_allComments) {
        if (!anno.author.isEmpty()) authors.insert(anno.author);
    }

    for (const auto &anno : m_lastFiltered) {
        auto *node = new QTreeWidgetItem();

        // Generate circular avatar icon
        QPixmap avatar = generateAvatar(anno.author);
        node->setIcon(0, QIcon(avatar));

        // Build rich display text with review state badge
        const QString stateTag = reviewStateLabel(anno.reviewState);
        const QString display = QString("%1  \u2022  %2  [%3]\n%4")
            .arg(anno.author, anno.creationDate, stateTag, anno.text);
        node->setText(0, display);

        // Tint the row toward the review-state color so Open/Accepted/
        // Rejected/etc. are distinguishable at a glance (depth dimming
        // below may override this for nested replies).
        node->setForeground(0, QBrush(reviewStateColor(anno.reviewState)));
        node->setToolTip(0, reviewStateLabel(anno.reviewState));

        node->setData(0, Qt::UserRole, anno.id);
        node->setData(0, Qt::UserRole + 1, anno.pageIndex);
        node->setData(0, Qt::UserRole + 2, static_cast<int>(anno.reviewState));
        node->setData(0, Qt::UserRole + 3, anno.parentId);
        itemMap.insert(anno.id, node);
    }

    // Parent/child assembly. A reply nests under its parent when the parent
    // also survived the filter; otherwise it is promoted to a top-level node
    // so a matching reply is never hidden by a filtered-out parent.
    QList<QTreeWidgetItem*> roots;
    for (const auto &anno : m_lastFiltered) {
        if (!itemMap.contains(anno.id)) continue;
        if (!anno.parentId.isEmpty() && itemMap.contains(anno.parentId)) {
            itemMap[anno.parentId]->addChild(itemMap[anno.id]);
        } else {
            m_tree->addTopLevelItem(itemMap[anno.id]);
            roots.append(itemMap[anno.id]);
        }
    }

    // M6-P5 D1: explicit depth indent. QTreeWidget already nests children, but
    // we prepend a depth-scaled guide ("    \u21b3 ") and dim deeper replies so the
    // reply depth is legible even with word-wrapped multi-line comment text.
    std::function<void(QTreeWidgetItem*, int)> applyDepth =
        [&](QTreeWidgetItem *node, int depth) {
            if (depth > 0) {
                const QString indent = QString(depth * 2, QChar(' '))
                                       + QStringLiteral("\u21b3 ");
                node->setText(0, indent + node->text(0));
                // Progressively dim nested replies (floor at a readable grey).
                const int shade = qMax(0x94, 0xF8 - depth * 0x1C);
                node->setForeground(0, QBrush(QColor(shade, shade, shade)));
            }
            for (int i = 0; i < node->childCount(); ++i)
                applyDepth(node->child(i), depth + 1);
        };
    for (QTreeWidgetItem *root : roots)
        applyDepth(root, 0);

    for (const QString &a : authors) {
        m_filterAuthor->addItem(a);
    }
    m_filterAuthor->setCurrentText(currentAuthor);
    m_filterAuthor->blockSignals(false);
    m_tree->expandAll();
}

// U07: the table presentation renders the SAME filtered records as the list —
// page/type/status/author (+ date/text) columns over the AnnotationItem set.
// Row item data mirrors the tree contract: col 0 carries the annotation id in
// Qt::UserRole and the 0-based pageIndex in Qt::UserRole + 1.
void CommentsWidget::rebuildTable()
{
    if (!m_table) return;
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    for (const auto &anno : m_lastFiltered) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *pageItem = new PageSortItem(QString::number(anno.pageIndex + 1));
        pageItem->setData(Qt::UserRole, anno.id);
        pageItem->setData(Qt::UserRole + 1, anno.pageIndex);
        m_table->setItem(row, 0, pageItem);
        m_table->setItem(row, 1, new QTableWidgetItem(annotationTypeLabel(anno.mode)));
        m_table->setItem(row, 2, new QTableWidgetItem(reviewStateLabel(anno.reviewState)));
        m_table->setItem(row, 3, new QTableWidgetItem(anno.author));
        m_table->setItem(row, 4, new QTableWidgetItem(anno.creationDate));
        m_table->setItem(row, 5, new QTableWidgetItem(anno.text));
    }
    m_table->setSortingEnabled(true);
}

void CommentsWidget::updateFilterSummary()
{
    if (!m_summaryLabel) return;
    const int active = activeFilterCount();
    const int shown = visibleCommentCount();
    if (active == 1)
        m_summaryLabel->setText(tr("1 filter \u00B7 %1 of %2 shown")
                                    .arg(shown).arg(m_totalComments));
    else
        m_summaryLabel->setText(tr("%1 filters \u00B7 %2 of %3 shown")
                                    .arg(active).arg(shown).arg(m_totalComments));
    if (m_clearBtn)
        m_clearBtn->setEnabled(active > 0);
}

int CommentsWidget::visibleCommentCount() const
{
    const bool tableMode = m_viewMode && m_viewMode->currentIndex() == 1;
    if (tableMode && m_table)
        return m_table->rowCount();
    if (!m_tree)
        return 0;
    int n = 0;
    QList<QTreeWidgetItem*> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        stack.append(m_tree->topLevelItem(i));
    while (!stack.isEmpty()) {
        QTreeWidgetItem *it = stack.takeFirst();
        ++n;
        for (int c = 0; c < it->childCount(); ++c)
            stack.append(it->child(c));
    }
    return n;
}

int CommentsWidget::activeFilterCount() const
{
    int n = 0;
    if (m_filterStatus && m_filterStatus->currentIndex() > 0) ++n;
    if (m_filterAuthor && m_filterAuthor->currentIndex() > 0) ++n;
    if (m_filterDate && m_filterDate->currentIndex() > 0) ++n;
    return n;
}

QString CommentsWidget::activeFilterSummary() const
{
    return m_summaryLabel ? m_summaryLabel->text() : QString();
}

void CommentsWidget::clearFilters()
{
    if (m_filterStatus) m_filterStatus->setCurrentIndex(0);
    if (m_filterDate) m_filterDate->setCurrentIndex(0);
    if (m_filterAuthor) {
        m_filterAuthor->blockSignals(true);
        m_filterAuthor->setCurrentIndex(0);
        m_filterAuthor->blockSignals(false);
    }
    reloadAnnotations();
}

QString CommentsWidget::selectedAnnotationId() const
{
    const bool tableMode = m_viewMode && m_viewMode->currentIndex() == 1;
    return selectionId(tableMode);
}

// Reads the current-row annotation id from ONE specific presentation. The
// view-mode toggle uses this to grab the selection from the OUTGOING view
// (the combo index has already flipped when the toggled signal fires).
QString CommentsWidget::selectionId(bool tableMode) const
{
    if (tableMode && m_table) {
        const int row = m_table->currentRow();
        if (row < 0 || !m_table->item(row, 0)) return QString();
        return m_table->item(row, 0)->data(Qt::UserRole).toString();
    }
    if (m_tree && m_tree->currentItem())
        return m_tree->currentItem()->data(0, Qt::UserRole).toString();
    return QString();
}

void CommentsWidget::restoreSelection(const QString &annoId)
{
    if (annoId.isEmpty()) return;
    const bool tableMode = m_viewMode && m_viewMode->currentIndex() == 1;
    if (tableMode && m_table) {
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (m_table->item(r, 0)
                && m_table->item(r, 0)->data(Qt::UserRole).toString() == annoId) {
                // setCurrentCell (not selectRow) so the current index moves
                // with the selection under the SelectRows behavior.
                m_table->setCurrentCell(r, 0);
                m_table->scrollToItem(m_table->item(r, 0));
                return;
            }
        }
        return;
    }
    if (!m_tree) return;
    QList<QTreeWidgetItem*> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        stack.append(m_tree->topLevelItem(i));
    while (!stack.isEmpty()) {
        QTreeWidgetItem *it = stack.takeFirst();
        if (it->data(0, Qt::UserRole).toString() == annoId) {
            m_tree->setCurrentItem(it);
            m_tree->scrollToItem(it);
            return;
        }
        for (int c = 0; c < it->childCount(); ++c)
            stack.append(it->child(c));
    }
}

// U07: RFC-4180 field escaping — fields containing '"', ',' or a newline are
// double-quoted with inner quotes doubled. Public so the escaping contract is
// directly testable.
QString CommentsWidget::csvEscapeField(const QString &raw)
{
    const bool needsQuoting = raw.contains(QLatin1Char('"'))
                           || raw.contains(QLatin1Char(','))
                           || raw.contains(QLatin1Char('\n'))
                           || raw.contains(QLatin1Char('\r'));
    if (!needsQuoting) return raw;
    QString escaped = raw;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// CSV of the DISPLAYED scope only. Columns use the persisted fields confirmed
// in AnnotationSerializer::toJson: pageIndex, mode, reviewState, author,
// creationDate and text. modificationDate is NOT persisted by the serializer
// and is therefore deliberately not exported. Page numbers are 1-based to
// match what the reviewer sees in the table.
QString CommentsWidget::displayedCsv() const
{
    const QStringList header = {QStringLiteral("Page"), QStringLiteral("Type"),
                                QStringLiteral("Status"), QStringLiteral("Author"),
                                QStringLiteral("Date"), QStringLiteral("Text")};
    QStringList lines;
    lines.append(header.join(QLatin1Char(',')));
    for (const auto &anno : m_lastFiltered) {
        QStringList row;
        row.append(QString::number(anno.pageIndex + 1));
        row.append(csvEscapeField(annotationTypeLabel(anno.mode)));
        row.append(csvEscapeField(reviewStateLabel(anno.reviewState)));
        row.append(csvEscapeField(anno.author));
        row.append(csvEscapeField(anno.creationDate));
        row.append(csvEscapeField(anno.text));
        lines.append(row.join(QLatin1Char(',')));
    }
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

bool CommentsWidget::exportDisplayedCsv(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray payload = displayedCsv().toUtf8();
    if (file.write(payload) != payload.size()) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

void CommentsWidget::addComment()
{
    if (!m_viewer) return;
    QString author = m_author->text().trimmed();
    QString content = m_editor->toPlainText().trimmed();
    if (author.isEmpty() || content.isEmpty()) return;

    AnnotationItem anno;
    anno.id = QUuid::createUuid().toString();
    anno.mode = ToolMode::AddComment;
    anno.pageIndex = qMax(0, m_currentPage - 1);
    anno.author = author;
    // M6-P4 D6: comments share the Djot rich-text model. The composer is still
    // plain text, so the entered text is the (trivial) Djot source; the shared
    // PoDoFoBackend::embedAnnotations dual-write then emits /Contents + /RC +
    // /PieceInfo for comments exactly as for InspectorWidget annotations.
    anno.djotSource = content;
    anno.text = pdfws_djot::djotToPlainText(content);
    anno.creationDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    anno.reviewState = ReviewState::Open;
    anno.color = Qt::yellow;
    anno.rect = QRectF(50, 50, 24, 24); // default pos

    QList<AnnotationItem> annos = m_viewer->annotations();
    annos.append(anno);
    m_viewer->setAnnotations(annos);

    m_editor->clear();
    reloadAnnotations();
}

void CommentsWidget::replyToComment()
{
    if (!m_viewer) return;
    auto *sel = m_tree->currentItem();
    if (!sel) return;

    QString parentId = sel->data(0, Qt::UserRole).toString();
    QString author = m_author->text().trimmed();
    QString content = m_editor->toPlainText().trimmed();
    if (author.isEmpty() || content.isEmpty()) return;

    AnnotationItem anno;
    anno.id = QUuid::createUuid().toString();
    anno.parentId = parentId;
    anno.mode = ToolMode::AddComment;
    anno.pageIndex = sel->data(0, Qt::UserRole + 1).toInt();
    anno.author = author;
    // M6-P4 D6: replies use the same Djot rich-text dual-write as top-level
    // comments (see addComment).
    anno.djotSource = content;
    anno.text = pdfws_djot::djotToPlainText(content);
    anno.creationDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    anno.reviewState = ReviewState::Open;

    QList<AnnotationItem> annos = m_viewer->annotations();
    
    // Add reply ID to parent
    for (int i = 0; i < annos.size(); ++i) {
        if (annos[i].id == parentId) {
            annos[i].replies.append(anno.id);
            break;
        }
    }
    
    annos.append(anno);
    m_viewer->setAnnotations(annos);

    m_editor->clear();
    reloadAnnotations();
}

void CommentsWidget::applyReviewState(const QString &annoId, ReviewState newState)
{
    if (!m_viewer || annoId.isEmpty()) return;

    const QList<AnnotationItem> oldList = m_viewer->annotations();
    QList<AnnotationItem> newList = oldList;

    bool found = false;
    for (int i = 0; i < newList.size(); ++i) {
        if (newList[i].id == annoId) {
            if (newList[i].reviewState == newState) return; // no-op
            newList[i].reviewState = newState;
            newList[i].modificationDate =
                QDateTime::currentDateTime().toString(Qt::ISODate);
            found = true;
            break;
        }
    }
    if (!found) return;

    // Persist via EditAnnotationCommand when an undo stack is available
    // (undoable + marks the DocumentSession dirty so the change is saved).
    // Falls back to a direct setAnnotations when no context is wired, e.g.
    // a standalone widget in a unit harness.
    if (m_ctx && m_ctx->undoStack) {
        DocumentSession *docSession = m_ctx->document ? m_ctx->document.get() : nullptr;
        m_ctx->undoStack->push(
            new EditAnnotationCommand(m_viewer, docSession, oldList, newList));
    } else {
        m_viewer->setAnnotations(newList);
    }

    reloadAnnotations();
}

void CommentsWidget::changeReviewState()
{
    // M6-P5 D3: apply a review-state change to the currently selected comment.
    // The richer per-state choices live in the tree context menu (non-modal);
    // this slot is a keyboard/programmatic convenience that cycles the
    // selected item's state is intentionally NOT implemented as a blocking
    // dialog (constraint: no modal for review-state change).
    if (!m_tree) return;
    auto *sel = m_tree->currentItem();
    if (!sel) return;
    const QString annoId = sel->data(0, Qt::UserRole).toString();
    if (annoId.isEmpty()) return;
    // Advance Open → Accepted as a sensible default single-action toggle.
    const ReviewState cur =
        static_cast<ReviewState>(sel->data(0, Qt::UserRole + 2).toInt());
    applyReviewState(annoId, cur == ReviewState::Accepted
                                 ? ReviewState::Open
                                 : ReviewState::Accepted);
}

void CommentsWidget::wrapSelection(const QString& prefix, const QString& suffix)
{
    if (!m_editor) return;
    QTextCursor cur = m_editor->textCursor();
    if (cur.hasSelection()) {
        const QString sel = cur.selectedText();
        cur.insertText(prefix + sel + suffix);
    } else {
        cur.insertText(prefix + suffix);
        cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, suffix.length());
        m_editor->setTextCursor(cur);
    }
    m_editor->setFocus();
}

void CommentsWidget::insertLinePrefix(const QString& prefix)
{
    if (!m_editor) return;
    QTextCursor cur = m_editor->textCursor();
    cur.movePosition(QTextCursor::StartOfLine);
    cur.insertText(prefix);
    m_editor->setFocus();
}

void CommentsWidget::refreshDjotPreview()
{
    if (!m_djotPreview || !m_editor) return;
    const QString djot = m_editor->toPlainText();
    if (djot.trimmed().isEmpty()) {
        m_djotPreview->clear();
        return;
    }
    m_djotPreview->setHtml(pdfws_djot::djotToHtmlFragment(djot));
}
