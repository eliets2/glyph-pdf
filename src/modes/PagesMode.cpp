// SPDX-License-Identifier: Apache-2.0
/**
 * PagesMode.cpp — M3-PROMPT-3 D1+D2+D3
 *
 * D1: Real page list with page number, size, and placeholder thumbnail.
 * D2: Split form — by-page / by-N / by-range expression + output naming +
 *     destination folder + filename-preview + progress + confirmation on overwrite.
 * D3: Reorder panel — drag-drop QListWidget + Apply (reorderPages) + Reset.
 *
 * U06: page organization around the existing thumbnail grid — visible
 * selected-page count/range ("pagesSelectionLabel"), a clear insertion
 * indicator during InternalMove drags, theme-token page-number labels,
 * Ctrl+Shift+Up/Down and context-menu moves through the SAME atomic
 * permutation command as drag (commitGridOrder), and selection +
 * current-page restore across undo-triggered reloads.
 *
 * Split implementation strategy (no splitDocument() on engine):
 *   For each output part (a QList<int> of 0-based page indices):
 *     1. Write a minimal valid PDF stub to the output path.
 *     2. Loop extractPageAsBytes(sourcePath, pageIdx) for each index in the part.
 *     3. Call insertPageFromBytes(outputPath, insertionIndex, pageBytes) to append.
 *     4. After all insertions, delete the stub page 0 via deletePage(outputPath, 0).
 *   This avoids needing a splitDocument() engine method.
 *
 * CONSTRAINT: Never name a local QLayout* variable `tr` (shadows QObject::tr()).
 */
#include "PagesMode.h"
#include "core/AppContext.h"
#include "engines/DocumentSession.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/BackendRouter.h"
#include "commands/ReorderPermutationCommand.h"
#include "core/interfaces/IPdfRenderer.h"

#include "util/GpTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QUndoStack>
#include <QtConcurrent/QtConcurrent>

// U06: selection visibility, insertion indicator, keyboard moves, undo restore.
#include <QAction>
#include <QBrush>
#include <QCursor>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QSet>
#include <algorithm>
#include <functional>

namespace gp {

// Out-of-line destructor: IPdfRenderer (unique_ptr member) is only a complete
// type here (PagesMode.h only forward-declares it).
PagesMode::~PagesMode()
{
    cancelThumbnailRenders();
}

// ── Static helpers ────────────────────────────────────────────────────────────

/**
 * Write a minimal valid one-page PDF so that insertPageFromBytes can operate on
 * an existing file at the given path.  The page is a 612×792 blank page (letter).
 * After building the real content with insertPageFromBytes, this stub page is
 * removed via deletePage(path, 0).
 */
bool PagesMode::writeMinimalPdf(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // Minimal PDF-1.4 with 1 blank letter page.  Object offsets are byte-exact.
    const QByteArray pdf =
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n"
        "0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n"
        "190\n"
        "%%EOF\n";

    f.write(pdf);
    f.close();
    return true;
}

/**
 * Parse a page-range expression such as "1-3,5,7-9" into a sorted, deduplicated
 * list of 0-based page indices.  The expression uses 1-based human page numbers.
 * Invalid tokens are silently skipped.  Out-of-range values are clamped to
 * [0, pageCount-1].
 *
 * Examples:
 *   "1-3,5,7-9" with pageCount=10 → [0,1,2,4,6,7,8]
 *   "2"         with pageCount=5  → [1]
 *   ""          with pageCount=5  → []
 */
QList<int> PagesMode::parsePageRange(const QString& expr, int pageCount)
{
    QList<int> result;
    if (expr.trimmed().isEmpty() || pageCount <= 0)
        return result;

    const QStringList tokens = expr.split(',', Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        const QString t = token.trimmed();
        if (t.contains('-')) {
            const QStringList parts = t.split('-', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                bool ok1 = false, ok2 = false;
                int from = parts[0].trimmed().toInt(&ok1) - 1; // 0-based
                int to   = parts[1].trimmed().toInt(&ok2) - 1;
                if (ok1 && ok2) {
                    from = qBound(0, from, pageCount - 1);
                    to   = qBound(0, to,   pageCount - 1);
                    for (int i = from; i <= to; ++i)
                        if (!result.contains(i)) result.append(i);
                }
            }
        } else {
            bool ok = false;
            int idx = t.toInt(&ok) - 1;
            if (ok) {
                idx = qBound(0, idx, pageCount - 1);
                if (!result.contains(idx)) result.append(idx);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

// ── PagesMode construction ────────────────────────────────────────────────────

QString PagesMode::localFirstClaim()
{
    // Factual: merge/split/Bates/redaction/OCR are all in-process engines.
    return PagesMode::tr("100% local processing — merging, Bates numbering and "
                         "redaction never leave this machine. No internet, no upload.");
}

// ── U06 file-local helpers ────────────────────────────────────────────────────

namespace {

// Map a QAbstractItemView::DropIndicatorPosition (as int: 0=OnItem, 1=AboveItem,
// 2=BelowItem, 3=OnViewport) plus the hovered row to the 0-based insertion index
// where the dragged page(s) will land; `count` means "append at the end".
int insertionRowForDrop(int dropPosition, int row, int count)
{
    if (count <= 0) return -1;
    int insertion;
    switch (dropPosition) {
    case 1:  insertion = row;      break; // AboveItem — land before the hovered page
    case 2:  insertion = row + 1;  break; // BelowItem — land after the hovered page
    case 3:  insertion = count;    break; // OnViewport — append after the last page
    default: insertion = row + 1;  break; // OnItem — treat as landing after it
    }
    return qBound(0, insertion, count);
}

// Shift the selected rows by delta (-1 = up, +1 = down), keeping the selected
// pages' relative order and clamping at the edges. Pure helper so keyboard
// moves are trivially reviewable; the result feeds the same commit path as drag.
QList<int> movedOrder(const QList<int>& order, const QList<int>& selRowsIn, int delta)
{
    QList<int> selRows = selRowsIn;
    std::sort(selRows.begin(), selRows.end());
    if (order.size() <= 1 || selRows.isEmpty() || delta == 0) return order;
    QSet<int> selValues;
    for (int r : selRows) {
        if (r < 0 || r >= order.size()) return order; // invalid — refuse to guess
        selValues.insert(order[r]);
    }
    QList<int> result = order;
    if (delta < 0) {
        for (int r : selRows) {                        // ascending
            if (r - 1 < 0) continue;
            if (selValues.contains(result[r - 1])) continue; // block member already placed
            result.move(r, r - 1);
        }
    } else {
        for (int i = selRows.size() - 1; i >= 0; --i) { // descending
            const int r = selRows[i];
            if (r + 1 >= result.size()) continue;
            if (selValues.contains(result[r + 1])) continue;
            result.move(r, r + 1);
        }
    }
    return result;
}

/**
 * U06: the thumbnail grid — a QListWidget that draws a clear insertion
 * indicator while an InternalMove drag is in progress (where the dragged
 * page(s) will land) and routes Ctrl+Shift+Up/Down to the keyboard-move seam.
 */
class PagesGridWidget final : public QListWidget {
public:
    explicit PagesGridWidget(QWidget* parent = nullptr) : QListWidget(parent) {}

    std::function<void(int)> keyMoveRequested; // set by PagesMode (moveSelectedPagesBy)

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QListWidget::paintEvent(event);
        paintInsertionIndicator();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const Qt::KeyboardModifiers mods = event->modifiers();
        if (keyMoveRequested
                && (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)
                && !(mods & ~(Qt::ControlModifier | Qt::ShiftModifier))) {
            if (event->key() == Qt::Key_Up)   { keyMoveRequested(-1); event->accept(); return; }
            if (event->key() == Qt::Key_Down) { keyMoveRequested(1);  event->accept(); return; }
        }
        QListWidget::keyPressEvent(event);
    }

private:
    void paintInsertionIndicator()
    {
        if (!(state() & QAbstractItemView::DraggingState)) return;
        const int count = model() ? model()->rowCount() : 0;
        if (count <= 0) return;

        const QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
        const QModelIndex hovered = indexAt(pos);
        const int insertion = insertionRowForDrop(
            int(dropIndicatorPosition()), hovered.isValid() ? hovered.row() : -1, count);
        if (insertion < 0) return;

        const bool appendAtEnd = (insertion >= count);
        const QRect target =
            visualRect(model()->index(appendAtEnd ? count - 1 : insertion, 0));
        if (!target.isValid()) return;

        QPainter painter(viewport());
        const QColor accent = gp::Theme::accent(); // theme token, all three themes

        if (!appendAtEnd && insertion > 0) {
            const QRect prev = visualRect(model()->index(insertion - 1, 0));
            if (prev.isValid() && prev.top() == target.top()) {
                // Same visual row: mark the gap between the two thumbnails.
                const int x = target.left() - spacing() / 2 - 2;
                painter.fillRect(x, target.top() - 2, 4, target.height() + 4, accent);
                return;
            }
        }
        // Row boundary (or append at the very end): a full-width accent bar.
        const int y = appendAtEnd ? target.bottom() - 1 : target.top() - 1;
        painter.fillRect(0, y, width(), 4, accent);
    }
};

} // namespace

PagesMode::PagesMode(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Mode toolbar ──────────────────────────────────────────────────────
    auto* tb = new QFrame;
    tb->setProperty("role", "modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* tbLayout = new QHBoxLayout(tb);
    tbLayout->setContentsMargins(10, 0, 10, 0);
    tbLayout->setSpacing(4);

    auto* modeLabel = new QLabel(PagesMode::tr("PAGES"));
    modeLabel->setProperty("mono", true);
    tbLayout->addWidget(modeLabel);

    // AR-8 D3: the per-page action buttons (Insert Before/After, Delete, Extract,
    // Replace, Rotate, Split Here, Merge) are HIDDEN until wired to engine actions.
    // Their planned ToolId enum entries and engine method stubs are preserved;
    // re-enable by connecting them in a future session.  Do NOT show disabled
    // controls with "future release" tooltips (SCOPE LOCK §5).
    tbLayout->addStretch(1);

    for (const QString& s : QStringList{"S", "M", "L"}) {
        auto* btn = new QToolButton;
        btn->setText(s);
        btn->setProperty("variant", "pill");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        if (s == "M") btn->setChecked(true);
        connect(btn, &QToolButton::clicked, this, &PagesMode::onThumbnailSizeChanged);
        tbLayout->addWidget(btn);
    }
    mainLayout->addWidget(tb);

    // ── Three-pane splitter: page list | split form | reorder panel ───────
    auto* splitter = new QSplitter(Qt::Horizontal);

    // Left pane: real page list (D1)
    auto* leftPane = new QWidget;
    buildPageListPanel(leftPane);
    splitter->addWidget(leftPane);
    splitter->setStretchFactor(0, 2);

    // Middle pane: split form (D2)
    auto* midPane = new QFrame;
    midPane->setFixedWidth(280);
    buildSplitPanel(midPane);
    splitter->addWidget(midPane);
    splitter->setStretchFactor(1, 0);

    // Right pane: reorder panel (D3)
    auto* rightPane = new QFrame;
    rightPane->setFixedWidth(220);
    buildReorderPanel(rightPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(2, 0);

    mainLayout->addWidget(splitter, 1);
}

// ── D1: Real page list panel ──────────────────────────────────────────────────

void PagesMode::buildPageListPanel(QWidget* host)
{
    auto* vLayout = new QVBoxLayout(host);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Sub-header
    auto* header = new QFrame;
    header->setProperty("role", "modeToolbar");
    header->setFixedHeight(26);
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(12, 0, 12, 0);
    auto* hdrLabel = new QLabel(PagesMode::tr("PAGE LIST"));
    hdrLabel->setProperty("mono", true);
    hdrLayout->addWidget(hdrLabel);
    hdrLayout->addStretch(1);
    // U06: selected-page count and affected range stay visible while working.
    m_selectionLabel = new QLabel;
    m_selectionLabel->setObjectName("pagesSelectionLabel");
    m_selectionLabel->setProperty("mono", true);
    hdrLayout->addWidget(m_selectionLabel);
    m_pageCountLabel = new QLabel;
    m_pageCountLabel->setProperty("mono", true);
    hdrLayout->addWidget(m_pageCountLabel);
    vLayout->addWidget(header);

    // §9.9 P0: the offline differentiator stated on the page-management
    // surface itself (muted, one line — factual, not marketing fluff).
    auto* localClaim = new QLabel(PagesMode::localFirstClaim());
    localClaim->setObjectName("pagesLocalClaimLabel");
    localClaim->setWordWrap(true);
    localClaim->setStyleSheet(QString("color:%1; font-size:8pt;")
                                  .arg(gp::Theme::fg2().name()));
    vLayout->addWidget(localClaim);

    // Page list widget — grid / icon mode so thumbnails display naturally
    m_pageList = new PagesGridWidget;
    m_pageList->setObjectName("pagesGrid");
    m_pageList->setViewMode(QListView::IconMode);
    m_pageList->setResizeMode(QListView::Adjust);
    m_pageList->setSpacing(8);
    m_pageList->setIconSize(QSize(100, 130));
    m_pageList->setMovement(QListView::Snap);
    m_pageList->setDragDropMode(QAbstractItemView::InternalMove);
    m_pageList->setDefaultDropAction(Qt::MoveAction);
    m_pageList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    static_cast<PagesGridWidget*>(m_pageList)->keyMoveRequested =
        [this](int delta) { moveSelectedPagesBy(delta); };
    // U06: translate QListWidget internal moves into an atomic page reorder.
    // Qt's InternalMove inserts the drop COPY (rowsInserted) before removing
    // the source rows (rowsAboutToBeRemoved), so the pre-drag order must be
    // captured at the FIRST model mutation (rowsAboutToBeInserted — before the
    // copy exists). The previous first-removal capture stored the duplicate
    // too, so every snapshot/newOrder size pair mismatched and
    // gridMovePermutation returned empty: drags silently committed nothing.
    connect(m_pageList->model(), &QAbstractItemModel::rowsAboutToBeInserted,
            this, [this](const QModelIndex&, int, int) {
        if (m_gridRebuildGuard) return;
        if (!m_dragSnapshot.isEmpty()) return;
        for (int i = 0; i < m_pageList->count(); ++i)
            m_dragSnapshot.append(m_pageList->item(i)->data(Qt::UserRole).toInt());
    });
    // A removal with no captured snapshot means rows left the grid outside an
    // internal move (e.g. drag-out to another widget): capture and reconcile
    // so the grid never lies about the document order.
    connect(m_pageList->model(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex&, int, int) {
        if (m_gridRebuildGuard) return;
        if (m_dragSnapshot.isEmpty()) {
            for (int i = 0; i < m_pageList->count(); ++i)
                m_dragSnapshot.append(m_pageList->item(i)->data(Qt::UserRole).toInt());
        }
        QMetaObject::invokeMethod(this, &PagesMode::finishGridReorder, Qt::QueuedConnection);
    });
    connect(m_pageList->model(), &QAbstractItemModel::rowsInserted,
            this, [this]() {
        if (m_gridRebuildGuard) return;
        QMetaObject::invokeMethod(this, &PagesMode::finishGridReorder, Qt::QueuedConnection);
    });
    // U06: selection drives the visible count/range label and the snapshot
    // used to restore the selection across undo-triggered reloads.
    connect(m_pageList, &QListWidget::itemSelectionChanged,
            this, &PagesMode::onGridSelectionChanged);
    // U06: context actions reuse the grid's own commands — no destructive
    // entries near incidental thumbnail clicks (those stay ribbon/controller-owned).
    m_pageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pageList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        fillGridContextMenu(&menu);
        if (!menu.isEmpty())
            menu.exec(m_pageList->viewport()->mapToGlobal(pos));
    });
    vLayout->addWidget(m_pageList, 1);
}

// ── D2: Split form panel ──────────────────────────────────────────────────────

void PagesMode::buildSplitPanel(QWidget* host)
{
    auto* vLayout = new QVBoxLayout(host);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Sub-header
    auto* header = new QFrame;
    header->setProperty("role", "modeToolbar");
    header->setFixedHeight(26);
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(12, 0, 12, 0);
    auto* hdrLabel = new QLabel(PagesMode::tr("SPLIT DOCUMENT"));
    hdrLabel->setProperty("mono", true);
    hdrLayout->addWidget(hdrLabel);
    hdrLayout->addStretch(1);
    vLayout->addWidget(header);

    auto* inner = new QWidget;
    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(12, 8, 12, 8);
    innerLayout->setSpacing(8);

    // Split mode radio group
    auto* modeGroup = new QGroupBox(PagesMode::tr("Split mode"));
    auto* modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->setSpacing(4);

    m_splitAtRadio    = new QRadioButton(PagesMode::tr("Split at page:"));
    m_splitEveryRadio = new QRadioButton(PagesMode::tr("Split every N pages:"));
    m_splitRangeRadio = new QRadioButton(PagesMode::tr("Split by range:"));
    m_splitAtRadio->setChecked(true);

    auto* atRow = new QHBoxLayout;
    atRow->addWidget(m_splitAtRadio);
    m_splitAtSpin = new QSpinBox;
    m_splitAtSpin->setRange(1, 9999);
    m_splitAtSpin->setValue(1);
    m_splitAtSpin->setToolTip(PagesMode::tr("Split after this page (1-based)"));
    atRow->addWidget(m_splitAtSpin);
    atRow->addStretch(1);
    modeLayout->addLayout(atRow);

    auto* everyRow = new QHBoxLayout;
    everyRow->addWidget(m_splitEveryRadio);
    m_splitEverySpin = new QSpinBox;
    m_splitEverySpin->setRange(1, 9999);
    m_splitEverySpin->setValue(2);
    m_splitEverySpin->setEnabled(false);
    m_splitEverySpin->setToolTip(PagesMode::tr("Number of pages per output part"));
    everyRow->addWidget(m_splitEverySpin);
    everyRow->addStretch(1);
    modeLayout->addLayout(everyRow);

    modeLayout->addWidget(m_splitRangeRadio);
    m_splitRangeEdit = new QLineEdit;
    m_splitRangeEdit->setPlaceholderText(PagesMode::tr("e.g. 1-3,5,7-9"));
    m_splitRangeEdit->setEnabled(false);
    m_splitRangeEdit->setToolTip(PagesMode::tr(
        "Comma-separated page ranges (1-based).\n"
        "Example: \"1-3,5,7-9\" extracts pages 1,2,3,5,7,8,9 as one part.\n"
        "For multiple parts, enter each part on a new line (not yet supported in v1.0)."));
    modeLayout->addWidget(m_splitRangeEdit);

    innerLayout->addWidget(modeGroup);

    // Wire radio buttons to enable/disable their inputs
    connect(m_splitAtRadio, &QRadioButton::toggled, this, [this](bool on) {
        m_splitAtSpin->setEnabled(on);
    });
    connect(m_splitEveryRadio, &QRadioButton::toggled, this, [this](bool on) {
        m_splitEverySpin->setEnabled(on);
    });
    connect(m_splitRangeRadio, &QRadioButton::toggled, this, [this](bool on) {
        m_splitRangeEdit->setEnabled(on);
    });

    // Output naming
    innerLayout->addWidget(new QLabel(PagesMode::tr("Output name pattern:")));
    m_namingEdit = new QLineEdit;
    m_namingEdit->setPlaceholderText("{stem}_part{n}.pdf");
    m_namingEdit->setText("{stem}_part{n}.pdf");
    m_namingEdit->setToolTip(PagesMode::tr(
        "{stem} — replaced with the source filename stem.\n"
        "{n}    — replaced with the 1-based part number."));
    innerLayout->addWidget(m_namingEdit);

    // Output folder
    innerLayout->addWidget(new QLabel(PagesMode::tr("Output folder:")));
    auto* dirRow = new QHBoxLayout;
    m_outDirEdit = new QLineEdit;
    m_outDirEdit->setPlaceholderText(PagesMode::tr("(same as source)"));
    m_outDirEdit->setToolTip(PagesMode::tr("Leave empty to save next to the source file."));
    dirRow->addWidget(m_outDirEdit, 1);
    auto* browseBtn = new QPushButton(PagesMode::tr("Browse…"));
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, PagesMode::tr("Select output folder"), m_outDirEdit->text());
        if (!dir.isEmpty()) m_outDirEdit->setText(dir);
    });
    dirRow->addWidget(browseBtn);
    innerLayout->addLayout(dirRow);

    // Filename preview list
    innerLayout->addWidget(new QLabel(PagesMode::tr("Output files (preview):")));
    m_previewList = new QListWidget;
    m_previewList->setMaximumHeight(90);
    m_previewList->setToolTip(PagesMode::tr("Files that will be created when you click Split."));
    innerLayout->addWidget(m_previewList);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    auto* previewBtn = new QPushButton(PagesMode::tr("Preview"));
    previewBtn->setToolTip(PagesMode::tr("Populate the filename list without writing any files."));
    connect(previewBtn, &QPushButton::clicked, this, &PagesMode::onPreviewSplit);
    btnRow->addWidget(previewBtn);

    auto* splitBtn = new QPushButton(PagesMode::tr("Split"));
    splitBtn->setProperty("variant", "primary");
    splitBtn->setToolTip(PagesMode::tr("Split the document and write output files."));
    connect(splitBtn, &QPushButton::clicked, this, &PagesMode::onSplit);
    btnRow->addWidget(splitBtn);
    innerLayout->addLayout(btnRow);

    innerLayout->addStretch(1);
    vLayout->addWidget(inner, 1);
}

// ── D3: Reorder panel ─────────────────────────────────────────────────────────

void PagesMode::buildReorderPanel(QWidget* host)
{
    auto* vLayout = new QVBoxLayout(host);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Sub-header
    auto* header = new QFrame;
    header->setProperty("role", "modeToolbar");
    header->setFixedHeight(26);
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(12, 0, 12, 0);
    auto* hdrLabel = new QLabel(PagesMode::tr("REORDER PAGES"));
    hdrLabel->setProperty("mono", true);
    hdrLayout->addWidget(hdrLabel);
    hdrLayout->addStretch(1);
    vLayout->addWidget(header);

    auto* inner = new QWidget;
    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(8, 8, 8, 8);
    innerLayout->setSpacing(6);

    auto* hint = new QLabel(PagesMode::tr("Drag rows to reorder,\nthen click Apply."));
    hint->setWordWrap(true);
    hint->setProperty("mono", true);
    innerLayout->addWidget(hint);

    m_reorderList = new QListWidget;
    m_reorderList->setDragDropMode(QAbstractItemView::InternalMove);
    m_reorderList->setDefaultDropAction(Qt::MoveAction);
    m_reorderList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_reorderList->setToolTip(PagesMode::tr("Drag rows to set the desired page order."));
    innerLayout->addWidget(m_reorderList, 1);

    auto* btnRow = new QHBoxLayout;
    auto* applyBtn = new QPushButton(PagesMode::tr("Apply"));
    applyBtn->setProperty("variant", "primary");
    applyBtn->setToolTip(PagesMode::tr(
        "Apply the displayed order to the document.\n"
        "This operation can be undone via Edit > Undo."));
    connect(applyBtn, &QPushButton::clicked, this, &PagesMode::onApplyReorder);
    btnRow->addWidget(applyBtn);

    auto* resetBtn = new QPushButton(PagesMode::tr("Reset"));
    resetBtn->setToolTip(PagesMode::tr("Revert to the original page order without saving."));
    connect(resetBtn, &QPushButton::clicked, this, &PagesMode::onResetReorder);
    btnRow->addWidget(resetBtn);
    innerLayout->addLayout(btnRow);

    vLayout->addWidget(inner, 1);
}

// ── setAppContext / refreshPageList ───────────────────────────────────────────

void PagesMode::cancelThumbnailRenders()
{
    for (auto* w : m_thumbWatchers) {
        w->cancel();
        // Do NOT waitForFinished() here — the watcher callback is a queued
        // connection; the watcher object is not safe to destroy until finished.
        // We disconnect the signal so any pending callbacks are no-ops, then
        // schedule deletion after the future completes.
        w->disconnect();
        if (w->isRunning())
            connect(w, &QFutureWatcherBase::finished, w, &QObject::deleteLater);
        else
            w->deleteLater();
    }
    m_thumbWatchers.clear();
    m_thumbRenderer.reset();
}

void PagesMode::setAppContext(const AppContext* ctx)
{
    m_ctx = ctx;
    // U06: reset the undo-mapping state — a previous context's command pointer
    // must never alias the new stack's commands.
    m_lastGridCmd = nullptr;
    m_lastGridSnapshot.clear();
    m_lastGridNewOrder.clear();
    // U06: undo/redo anywhere (including other modes' commands) can change the
    // page order this grid displays. Reload on index changes and restore the
    // selection + current page across the reload.
    if (m_ctx && m_ctx->undoStack)
        connect(m_ctx->undoStack.get(), &QUndoStack::indexChanged,
                this, &PagesMode::onUndoStackIndexChanged, Qt::UniqueConnection);
    refreshPageList();
}

void PagesMode::refreshPageList()
{
    // Cancel any in-flight thumbnail renders from the previous document.
    cancelThumbnailRenders();

    // Programmatic rebuild: the drag-signal machinery must not fire.
    m_gridRebuildGuard = true;
    m_pageList->clear();
    m_reorderList->clear();
    m_gridRebuildGuard = false;
    // The old load generation's selection no longer exists; undo/commit flows
    // re-arm m_pendingSelection before triggering a reload.
    m_selectedPages.clear();
    m_currentPageData = -1;
    m_originalOrder.clear();

    if (!m_ctx || !m_ctx->document) {
        m_pageCountLabel->setText(PagesMode::tr("No document"));

        // Update split spin limits
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        restorePendingSelection(); // U06: drop stale pending rows — nothing to restore into
        return;
    }

    const QString path = m_ctx->document->path();
    if (path.isEmpty()) {
        m_pageCountLabel->setText(PagesMode::tr("No document"));
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        restorePendingSelection();
        return;
    }

    // AR-7 D2: the page-count binary search issues multiple extractPageAsBytes()
    // calls that each do engine I/O — run it on a worker thread so the GUI stays
    // responsive, then populate the UI from onPageCountReady() on the GUI thread.
    m_pageCountLabel->setText(PagesMode::tr("Loading…"));
    if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
    if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);

    if (!m_ctx->pdfEditor) {
        m_pageCountLabel->setText(PagesMode::tr("0 pages"));
        restorePendingSelection();
        return;
    }

    if (!m_pageCountWatcher) {
        m_pageCountWatcher = new QFutureWatcher<int>(this);
        connect(m_pageCountWatcher, &QFutureWatcher<int>::finished,
                this, &PagesMode::onPageCountReady);
    }

    // Cancel any still-running query for a previous document.
    if (m_pageCountWatcher->isRunning()) {
        m_pageCountWatcher->cancel();
        m_pageCountWatcher->waitForFinished();
    }

    std::weak_ptr<IPdfEditorEngine> weakEditor = m_ctx->pdfEditor;
    m_pageCountWatcher->setFuture(QtConcurrent::run([weakEditor, path]() -> int {
        auto engine = weakEditor.lock();
        if (!engine) return 0;
        engine->loadDocumentForEditing(path);
        // O(log N) binary search: extractPageAsBytes returns non-empty for valid indices.
        int lo = 0, hi = 1;
        while (!engine->extractPageAsBytes(path, hi).isEmpty()) {
            hi *= 2;
            if (hi > 4096) break;
        }
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (engine->extractPageAsBytes(path, mid).isEmpty())
                hi = mid;
            else
                lo = mid + 1;
        }
        return lo;
    }));
}

void PagesMode::onPageCountReady()
{
    if (!m_pageCountWatcher || m_pageCountWatcher->isCanceled()) return;
    const int pageCount = m_pageCountWatcher->result();

    if (pageCount <= 0) {
        m_pageCountLabel->setText(PagesMode::tr("0 pages"));
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        restorePendingSelection(); // U06: nothing to restore into an empty grid
        return;
    }

    m_pageCountLabel->setText(PagesMode::tr("%1 pages").arg(pageCount));
    if (m_splitAtSpin)    m_splitAtSpin->setRange(1, qMax(1, pageCount - 1));
    if (m_splitEverySpin) m_splitEverySpin->setRange(1, pageCount);

    // Populate page list and reorder list
    m_gridRebuildGuard = true;
    m_pageList->clear();
    m_reorderList->clear();
    m_originalOrder.clear();

    // AR-8 D5 + U06: placeholder items (gray until real PDFium renders arrive)
    // built through makePageItem — theme-token label foreground, stable identity.
    for (int i = 0; i < pageCount; ++i) {
        m_pageList->addItem(makePageItem(i));

        m_reorderList->addItem(PagesMode::tr("Page %1").arg(i + 1));
        m_originalOrder.append(i);
    }
    m_gridRebuildGuard = false;
    // U06: reselect the tracked pages (and current page) across undo- or
    // command-triggered reloads — before the thumbnail-launch early returns.
    restorePendingSelection();

    // AR-8 D5: launch off-thread PDFium thumbnail renders.
    // We build ONE renderer (PdfiumBackend loaded once), then spawn a lightweight
    // future per page.  PdfiumBackend is mutex-protected so concurrent calls are safe.
    if (!m_ctx || !m_ctx->document) return;
    const QString docPath = m_ctx->document->path();
    if (docPath.isEmpty()) return;

#ifdef HAS_PDFIUM
    // Build the renderer on the GUI thread (loadDocument is cheap) and keep it
    // alive for the lifetime of the render jobs via shared_ptr.
    auto sharedRenderer = std::shared_ptr<IPdfRenderer>(
        BackendRouter::rendererFor(docPath).release());
    if (!sharedRenderer) return;

    // Keep a raw alias in m_thumbRenderer for cleanup; shared_ptr is captured
    // by the futures so it outlives any individual watcher.
    m_thumbRenderer = nullptr; // already reset in cancelThumbnailRenders

    // Thumbnail DPI: 72 dpi × 100/130 ≈ 55pt page fits in 100px icon slot.
    static constexpr int kThumbDpi = 55;

    for (int pageIdx = 0; pageIdx < pageCount; ++pageIdx) {
        auto* watcher = new QFutureWatcher<QImage>(this);
        m_thumbWatchers.append(watcher);

        // Capture by value: sharedRenderer (ref-counted), pageIdx.
        QFuture<QImage> future = QtConcurrent::run(
            [sharedRenderer, pageIdx]() -> QImage {
                return sharedRenderer->renderPage(pageIdx, kThumbDpi);
            });
        watcher->setFuture(future);

        const int idx = pageIdx; // capture for lambda
        connect(watcher, &QFutureWatcher<QImage>::finished,
                this, [this, watcher, idx]() {
                    if (!watcher->isCanceled()) {
                        onThumbnailReady(idx, watcher->result());
                    }
                    m_thumbWatchers.removeOne(watcher);
                    watcher->deleteLater();
                });
    }
#endif // HAS_PDFIUM
}

void PagesMode::onThumbnailReady(int pageIndex, const QImage& img)
{
    if (img.isNull()) return; // render failed — keep the gray placeholder
    if (pageIndex < 0 || pageIndex >= m_pageList->count()) return;

    auto* item = m_pageList->item(pageIndex);
    if (!item) return;

    const QSize iconSz = m_pageList->iconSize();
    QPixmap pm = QPixmap::fromImage(img).scaled(
        iconSz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    item->setIcon(QIcon(pm));
}

// ── Thumbnail size toggle ─────────────────────────────────────────────────────

void PagesMode::onThumbnailSizeChanged()
{
    auto* btn = qobject_cast<QToolButton*>(sender());
    if (!btn) return;

    const QString label = btn->text();
    if (label == "S") {
        m_pageList->setIconSize(QSize(70, 90));
        m_pageList->setSpacing(4);
    } else if (label == "M") {
        m_pageList->setIconSize(QSize(100, 130));
        m_pageList->setSpacing(8);
    } else { // L
        m_pageList->setIconSize(QSize(140, 180));
        m_pageList->setSpacing(12);
    }
}

// ── D2: Split logic ───────────────────────────────────────────────────────────

QList<QList<int>> PagesMode::computeSplitGroups() const
{
    if (!m_ctx || !m_ctx->document) return {};

    const int pageCount = m_pageList->count();
    if (pageCount <= 0) return {};

    QList<QList<int>> groups;

    if (m_splitAtRadio->isChecked()) {
        // Split after page N: [0..N-1] and [N..end]
        const int splitAfter = m_splitAtSpin->value() - 1; // 0-based
        QList<int> part1, part2;
        for (int i = 0; i <= qMin(splitAfter, pageCount - 1); ++i) part1.append(i);
        for (int i = splitAfter + 1; i < pageCount; ++i)           part2.append(i);
        if (!part1.isEmpty()) groups.append(part1);
        if (!part2.isEmpty()) groups.append(part2);

    } else if (m_splitEveryRadio->isChecked()) {
        // Split every N pages
        const int n = qMax(1, m_splitEverySpin->value());
        for (int start = 0; start < pageCount; start += n) {
            QList<int> part;
            for (int i = start; i < qMin(start + n, pageCount); ++i) part.append(i);
            if (!part.isEmpty()) groups.append(part);
        }

    } else if (m_splitRangeRadio->isChecked()) {
        // Range expression: treat the whole range as a single output part
        const QList<int> indices = parsePageRange(m_splitRangeEdit->text(), pageCount);
        if (!indices.isEmpty()) groups.append(indices);
    }

    return groups;
}

QString PagesMode::makeOutputName(const QString& pattern, const QString& stem, int part) const
{
    QString name = pattern.isEmpty() ? QString("{stem}_part{n}.pdf") : pattern;
    name.replace("{stem}", stem);
    name.replace("{n}", QString::number(part));
    if (!name.endsWith(".pdf", Qt::CaseInsensitive)) name += ".pdf";
    return name;
}

void PagesMode::onPreviewSplit()
{
    m_previewList->clear();

    if (!m_ctx || !m_ctx->document) {
        m_previewList->addItem(PagesMode::tr("(no document open)"));
        return;
    }

    const QString sourcePath = m_ctx->document->path();
    const QString stem = QFileInfo(sourcePath).completeBaseName();
    const QString outDir = m_outDirEdit->text().trimmed().isEmpty()
        ? QFileInfo(sourcePath).absolutePath()
        : m_outDirEdit->text().trimmed();
    const QString pattern = m_namingEdit->text().trimmed();

    const QList<QList<int>> groups = computeSplitGroups();
    if (groups.isEmpty()) {
        m_previewList->addItem(PagesMode::tr("(invalid split configuration)"));
        return;
    }

    for (int i = 0; i < groups.size(); ++i) {
        const QString name = makeOutputName(pattern, stem, i + 1);
        const QString fullPath = outDir + "/" + name;
        const QString pageInfo = PagesMode::tr("%1 page(s)").arg(groups[i].size());
        m_previewList->addItem(QString("%1  [%2]").arg(fullPath, pageInfo));
    }
}

void PagesMode::onSplit()
{
    if (!m_ctx || !m_ctx->document || !m_ctx->pdfEditor) {
        QMessageBox::warning(this, PagesMode::tr("Split"),
            PagesMode::tr("No document is open."));
        return;
    }

    const QString sourcePath = m_ctx->document->path();
    if (sourcePath.isEmpty()) {
        QMessageBox::warning(this, PagesMode::tr("Split"),
            PagesMode::tr("Please open a document before splitting."));
        return;
    }

    const QList<QList<int>> groups = computeSplitGroups();
    if (groups.isEmpty()) {
        QMessageBox::warning(this, PagesMode::tr("Split"),
            PagesMode::tr("The current split configuration produces no output parts. "
                          "Adjust the split mode settings and try again."));
        return;
    }

    const QString stem = QFileInfo(sourcePath).completeBaseName();
    const QString outDir = m_outDirEdit->text().trimmed().isEmpty()
        ? QFileInfo(sourcePath).absolutePath()
        : m_outDirEdit->text().trimmed();
    const QString pattern = m_namingEdit->text().trimmed();

    // Build output paths and check for overwrites
    QStringList outputPaths;
    for (int i = 0; i < groups.size(); ++i) {
        outputPaths.append(outDir + "/" + makeOutputName(pattern, stem, i + 1));
    }

    // Overwrite confirmation: collect existing files and ask once
    QStringList existingFiles;
    for (const QString& p : outputPaths)
        if (QFile::exists(p)) existingFiles.append(p);

    if (!existingFiles.isEmpty()) {
        const QString msg = PagesMode::tr(
            "The following output files already exist and will be overwritten:\n\n%1\n\n"
            "Do you want to continue?").arg(existingFiles.join("\n"));
        const auto btn = QMessageBox::question(this, PagesMode::tr("Overwrite?"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }

    // Execute split with progress dialog
    const QStringList produced = executeSplit(sourcePath, groups, outDir, pattern);

    if (produced.isEmpty()) {
        QMessageBox::critical(this, PagesMode::tr("Split failed"),
            PagesMode::tr("The split operation did not produce any output files.\n"
                          "Check that the document is valid and the output directory is writable."));
    } else {
        QMessageBox::information(this, PagesMode::tr("Split complete"),
            PagesMode::tr("Split complete. %1 file(s) written:\n\n%2")
                .arg(produced.size())
                .arg(produced.join("\n")));
        onPreviewSplit(); // refresh preview list to show produced paths
    }
}

QStringList PagesMode::executeSplit(const QString& sourcePath,
                                    const QList<QList<int>>& groups,
                                    const QString& outputDir,
                                    const QString& stemPattern)
{
    QStringList produced;
    if (!m_ctx || !m_ctx->pdfEditor || groups.isEmpty()) return produced;

    const QString stem = QFileInfo(sourcePath).completeBaseName();

    auto* progress = new QProgressDialog(
        PagesMode::tr("Splitting document…"), PagesMode::tr("Cancel"),
        0, groups.size(), this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(500);

    for (int gi = 0; gi < groups.size(); ++gi) {
        if (progress->wasCanceled()) break;
        progress->setValue(gi);

        const QList<int>& pages = groups[gi];
        const QString outName   = makeOutputName(stemPattern, stem, gi + 1);
        const QString outPath   = outputDir + "/" + outName;

        // Step 1: Create a minimal stub PDF as the output file
        if (!writeMinimalPdf(outPath)) {
            qWarning("PagesMode::executeSplit: cannot create stub at %s",
                     qPrintable(outPath));
            continue;
        }

        // Step 2: Insert each source page into the output document.
        // The stub already has 1 blank page at index 0.
        // We insert source pages starting at index 0 (pushing stub page to the end),
        // so insertionIndex for page[k] = k.
        bool partOk = true;
        for (int k = 0; k < pages.size(); ++k) {
            const QByteArray pageBytes =
                m_ctx->pdfEditor->extractPageAsBytes(sourcePath, pages[k]);
            if (pageBytes.isEmpty()) {
                qWarning("PagesMode::executeSplit: extractPageAsBytes failed for page %d",
                         pages[k]);
                partOk = false;
                break;
            }
            if (!m_ctx->pdfEditor->insertPageFromBytes(outPath, k, pageBytes)) {
                qWarning("PagesMode::executeSplit: insertPageFromBytes failed at index %d",
                         k);
                partOk = false;
                break;
            }
        }

        if (!partOk) {
            QFile::remove(outPath);
            continue;
        }

        // Step 3: Delete the stub page (it is now the last page at index pages.size()).
        m_ctx->pdfEditor->deletePage(outPath, pages.size());

        produced.append(outPath);
    }

    progress->setValue(groups.size());
    progress->deleteLater();
    return produced;
}

// ── D3: Reorder logic ─────────────────────────────────────────────────────────

void PagesMode::onApplyReorder()
{
    if (!m_ctx || !m_ctx->document || !m_ctx->pdfEditor) {
        QMessageBox::warning(this, PagesMode::tr("Reorder"),
            PagesMode::tr("No document is open."));
        return;
    }

    const QString path = m_ctx->document->path();
    if (path.isEmpty()) return;

    const int count = m_reorderList->count();
    if (count <= 1) return; // nothing to reorder

    // Collect desired order (0-based original page indices from display text).
    // Each item text is "Page N" (1-based); recover the 0-based original index.
    QList<int> desiredOrder;
    desiredOrder.reserve(count);
    for (int i = 0; i < count; ++i) {
        const QString text = m_reorderList->item(i)->text();
        bool ok = false;
        const int pageNum = text.mid(text.lastIndexOf(' ') + 1).toInt(&ok);
        desiredOrder.append(ok ? pageNum - 1 : i);
    }

    // AR-8 D5 (atomic reorder): push a single ReorderPermutationCommand onto the
    // undo stack.  This replaces the previous N sequential reorderPages() calls
    // (each of which wrote to disk individually, creating N undo steps and a
    // partial-failure window).  The new command applies the whole permutation in
    // ONE reorderAllPages() call (one PoDoFo write), so:
    //   – Undo collapses to a single Ctrl+Z.
    //   – Partial failure leaves the document in its original state.
    auto* cmd = new ReorderPermutationCommand(
        m_ctx->pdfEditor.get(),
        m_ctx->document.get(),
        desiredOrder);

    if (m_ctx->undoStack) {
        m_ctx->undoStack->push(cmd); // push() calls redo() = reorderAllPages()
    } else {
        // No undo stack: execute directly and clean up.
        cmd->redo();
        delete cmd;
    }

    // Check if the engine reported an error (reorderAllPages failed).
    if (m_ctx->pdfEditor->lastError().severity >= ErrorInfo::Error) {
        QMessageBox::critical(this, PagesMode::tr("Reorder failed"),
            PagesMode::tr("An error occurred while reordering pages. "
                          "The document has not been modified."));
    } else {
        // Update the original order tracking so Reset works correctly.
        m_originalOrder = desiredOrder;
        QMessageBox::information(this, PagesMode::tr("Reorder complete"),
            PagesMode::tr("Page order applied successfully."));
    }
}

void PagesMode::onResetReorder()
{
    // Repopulate reorder list from page list (which reflects loaded order)
    m_reorderList->clear();
    for (int i = 0; i < m_pageList->count(); ++i) {
        m_reorderList->addItem(PagesMode::tr("Page %1").arg(i + 1));
    }
    m_originalOrder.clear();
    for (int i = 0; i < m_pageList->count(); ++i) m_originalOrder.append(i);
}

// §9.9 P0: pure helper — drag result → engine permutation.
QList<int> PagesMode::gridMovePermutation(const QList<int>& snapshot,
                                          const QList<int>& newOrder)
{
    if (snapshot.isEmpty() || snapshot.size() != newOrder.size()) return {};
    if (snapshot == newOrder) return {}; // no net change
    QList<int> perm;
    perm.reserve(newOrder.size());
    for (int idx : newOrder) {
        const int pos = snapshot.indexOf(idx);
        if (pos < 0) return {}; // inconsistent input — caller must not apply
        perm.append(pos);
    }
    return perm;
}

// §9.9 P0 + U06: grid drag-and-drop → atomic page reorder.
// The pre-drag order is captured at the first model mutation of the drag (see
// buildPageListPanel), so snapshot and newOrder always have equal sizes.
void PagesMode::finishGridReorder()
{
    if (m_dragSnapshot.isEmpty()) return;

    const QList<int> snapshot = m_dragSnapshot;
    m_dragSnapshot.clear();

    QList<int> newOrder;
    newOrder.reserve(m_pageList->count());
    for (int i = 0; i < m_pageList->count(); ++i)
        newOrder.append(m_pageList->item(i)->data(Qt::UserRole).toInt());

    if (newOrder.size() != snapshot.size()) {
        // Rows left the grid outside an internal move (e.g. drag-out to
        // another widget): restore the captured order so the grid never lies.
        rebuildFromOrder(snapshot);
        return;
    }

    // The selected pages keep their identity (UserRole data) across the
    // post-command reload — map them to their new rows for the restore.
    QList<int> pendingSel;
    for (int v : m_selectedPages) {
        const int pos = newOrder.indexOf(v);
        if (pos >= 0) pendingSel.append(pos);
    }
    const int pendingCur = m_currentPageData >= 0
        ? newOrder.indexOf(m_currentPageData) : -1;

    commitGridOrder(snapshot, newOrder, pendingSel, pendingCur);
}

// U06: shared commit tail — the ONE command path for drag, keyboard, and
// context-menu moves: gridMovePermutation → ReorderPermutationCommand → reload.
void PagesMode::commitGridOrder(const QList<int>& snapshot, const QList<int>& newOrder,
                                const QList<int>& pendingSelection, int pendingCurrent)
{
    // Express the new visual order as a permutation over positions in the
    // pre-move order — exactly what ReorderPermutationCommand expects.
    const QList<int> perm = gridMovePermutation(snapshot, newOrder);
    if (perm.isEmpty()) return; // no net change or inconsistent input

    if (!m_ctx || !m_ctx->document || !m_ctx->pdfEditor || m_ctx->document->path().isEmpty()) {
        // No document: revert the visual move so the grid never lies.
        rebuildFromOrder(snapshot);
        return;
    }

    m_pendingSelection = pendingSelection;
    m_pendingCurrentPage = pendingCurrent;

    auto* cmd = new ReorderPermutationCommand(
        m_ctx->pdfEditor.get(), m_ctx->document.get(), perm);
    m_lastGridCmd = cmd;
    m_ownPush = true; // our own indexChanged must not trigger the undo reload
    if (m_ctx->undoStack) {
        m_ctx->undoStack->push(cmd); // push() calls redo() = reorderAllPages()
    } else {
        cmd->redo();
        delete cmd;
        m_lastGridCmd = nullptr;
    }
    m_ownPush = false;

    if (m_ctx->pdfEditor->lastError().severity >= ErrorInfo::Error) {
        QMessageBox::critical(this, PagesMode::tr("Reorder failed"),
            PagesMode::tr("An error occurred while reordering pages. "
                          "The document has not been modified."));
        m_lastGridCmd = nullptr;
        m_pendingSelection.clear();
        m_pendingCurrentPage = -1;
        rebuildFromOrder(snapshot);
        return;
    }

    m_lastGridSnapshot = snapshot;
    m_lastGridNewOrder = newOrder;
    m_originalOrder = newOrder;
    refreshPageList(); // re-render thumbnails in the new order; restores selection
}

// U06: one grid item — placeholder thumbnail, "Page N" label with a
// theme-token foreground (readable on every theme background), and the page
// identity in Qt::UserRole.
QListWidgetItem* PagesMode::makePageItem(int pageData)
{
    QPixmap thumb(100, 130);
    thumb.fill(QColor(220, 220, 220));

    auto* item = new QListWidgetItem;
    item->setIcon(QIcon(thumb));
    item->setText(PagesMode::tr("Page %1").arg(pageData + 1));
    item->setSizeHint(QSize(120, 160));
    item->setData(Qt::UserRole, pageData);
    item->setForeground(QBrush(Theme::fg0()));
    return item;
}

// U06: keyboard/context move of the selected page(s) by delta (-1 up, +1 down)
// through the SAME command path as drag (commitGridOrder). No new mutation
// implementation — the atomic ReorderPermutationCommand does the work.
void PagesMode::moveSelectedPagesBy(int delta)
{
    if (delta == 0 || !m_pageList || m_gridRebuildGuard) return;
    if (!m_ctx || !m_ctx->document || !m_ctx->pdfEditor || m_ctx->document->path().isEmpty())
        return;
    const int count = m_pageList->count();
    if (count <= 1) return;

    QList<int> order;
    order.reserve(count);
    for (int i = 0; i < count; ++i)
        order.append(m_pageList->item(i)->data(Qt::UserRole).toInt());

    QList<int> selRows;
    const QModelIndexList selected = m_pageList->selectionModel()->selectedIndexes();
    for (const QModelIndex& idx : selected) selRows.append(idx.row());
    std::sort(selRows.begin(), selRows.end());
    if (selRows.isEmpty()) return;

    const QList<int> newOrder = movedOrder(order, selRows, delta);
    if (newOrder == order) return; // clamped at an edge — nothing to move

    // The moved pages stay selected at their new rows after the reload.
    QList<int> pendingSel;
    int pendingCur = -1;
    const int currentRow = m_pageList->currentRow();
    for (int row : selRows) {
        const int newPos = newOrder.indexOf(order[row]);
        if (newPos < 0) continue;
        pendingSel.append(newPos);
        if (row == currentRow) pendingCur = newPos;
    }
    if (pendingCur < 0 && currentRow >= 0 && currentRow < count)
        pendingCur = newOrder.indexOf(order[currentRow]);

    commitGridOrder(order, newOrder, pendingSel, pendingCur);
}

// U06: thumbnail context menu — the same existing commands the grid already
// exposes (keyboard/drag permutation path and view selection). Deliberately
// no destructive entries: per-page delete/rotate/extract stay in the
// ribbon/controller where the existing engine commands live.
void PagesMode::fillGridContextMenu(QMenu* menu)
{
    if (!menu || !m_pageList) return;
    const bool hasSelection = !m_pageList->selectedItems().isEmpty();
    QAction* moveUp = menu->addAction(PagesMode::tr("Move Up"),
                                      this, [this]() { moveSelectedPagesBy(-1); });
    QAction* moveDown = menu->addAction(PagesMode::tr("Move Down"),
                                        this, [this]() { moveSelectedPagesBy(1); });
    moveUp->setEnabled(hasSelection);
    moveDown->setEnabled(hasSelection);
    menu->addSeparator();
    menu->addAction(PagesMode::tr("Select All"), m_pageList, &QListWidget::selectAll);
    menu->addAction(PagesMode::tr("Clear Selection"), m_pageList, &QListWidget::clearSelection);
}

// U06: keep the visible selection label and the identity snapshot in step.
void PagesMode::onGridSelectionChanged()
{
    updateSelectionLabel();
    if (m_gridRebuildGuard) return; // restore helpers re-track explicitly
    m_selectedPages.clear();
    m_currentPageData = -1;
    if (!m_pageList) return;
    const QModelIndexList selected = m_pageList->selectionModel()->selectedIndexes();
    for (const QModelIndex& idx : selected)
        m_selectedPages.append(idx.data(Qt::UserRole).toInt());
    if (m_pageList->currentItem())
        m_currentPageData = m_pageList->currentItem()->data(Qt::UserRole).toInt();
}

// U06: "N pages selected · pages X-Y" (affected 1-based row range) or empty.
void PagesMode::updateSelectionLabel()
{
    if (!m_selectionLabel) return;
    QList<int> rows;
    if (m_pageList && m_pageList->selectionModel()) {
        const QModelIndexList selected = m_pageList->selectionModel()->selectedIndexes();
        for (const QModelIndex& idx : selected) rows.append(idx.row());
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    if (rows.isEmpty()) {
        m_selectionLabel->clear();
        return;
    }
    if (rows.size() == 1)
        m_selectionLabel->setText(
            PagesMode::tr("1 page selected · page %1").arg(rows.first() + 1));
    else
        m_selectionLabel->setText(
            PagesMode::tr("%1 pages selected · pages %2-%3")
                .arg(rows.size()).arg(rows.first() + 1).arg(rows.last() + 1));
}

// U06: reselect tracked rows (position-based) after a fresh load generation,
// where item data == row. Drops the pending state once applied.
void PagesMode::restorePendingSelection()
{
    const QList<int> pending = m_pendingSelection;
    const int pendingCurrent = m_pendingCurrentPage;
    m_pendingSelection.clear();
    m_pendingCurrentPage = -1;

    QList<int> restoredRows;
    QItemSelection sel;
    if (m_pageList) {
        for (int pos : pending) {
            if (pos < 0 || pos >= m_pageList->count()) continue;
            const QModelIndex idx = m_pageList->model()->index(pos, 0);
            sel.select(idx, idx);
            restoredRows.append(pos);
        }
        if (!sel.isEmpty())
            m_pageList->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
        if (pendingCurrent >= 0 && pendingCurrent < m_pageList->count())
            m_pageList->selectionModel()->setCurrentIndex(
                m_pageList->model()->index(pendingCurrent, 0), QItemSelectionModel::NoUpdate);
    }
    // Re-track explicitly (selection signals may be skipped during rebuilds).
    m_selectedPages = restoredRows;
    m_currentPageData = pendingCurrent >= 0 && pendingCurrent < m_pageList->count()
        ? pendingCurrent
        : (restoredRows.isEmpty() ? -1 : restoredRows.first());
    updateSelectionLabel();
}

// U06: reselect tracked pages (identity-based) after a visual revert, where
// items are rebuilt with their original UserRole data.
void PagesMode::restoreSelectionByData()
{
    const QList<int> wanted = m_selectedPages;
    const int wantedCurrent = m_currentPageData;
    QList<int> restoredRows;
    QItemSelection sel;
    int currentRow = -1;
    if (m_pageList) {
        for (int i = 0; i < m_pageList->count(); ++i) {
            const int itemData = m_pageList->item(i)->data(Qt::UserRole).toInt();
            if (wanted.contains(itemData)) {
                const QModelIndex idx = m_pageList->model()->index(i, 0);
                sel.select(idx, idx);
                restoredRows.append(i);
            }
            if (itemData == wantedCurrent) currentRow = i;
        }
        if (!sel.isEmpty())
            m_pageList->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
        if (currentRow >= 0)
            m_pageList->selectionModel()->setCurrentIndex(
                m_pageList->model()->index(currentRow, 0), QItemSelectionModel::NoUpdate);
    }
    m_selectedPages = restoredRows;
    m_currentPageData = currentRow >= 0 ? currentRow
        : (restoredRows.isEmpty() ? -1 : restoredRows.first());
    updateSelectionLabel();
}

// U06: coalesce bursts of QUndoStack::indexChanged signals into one reload.
void PagesMode::scheduleUndoRefresh()
{
    if (m_undoRefreshScheduled) return;
    m_undoRefreshScheduled = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_undoRefreshScheduled = false;
        refreshPageList();
    }, Qt::QueuedConnection);
}

// U06: undo/redo can change the page order this grid displays. Reload and
// restore the selection + current page, mapping page identities through our
// last grid permutation when the affected command is ours.
void PagesMode::onUndoStackIndexChanged(int index)
{
    if (m_ownPush || m_gridRebuildGuard) return;
    if (!m_ctx || !m_ctx->undoStack || !m_ctx->document) return;
    if (m_ctx->document->path().isEmpty() || m_pageList->count() == 0) return;

    const QUndoStack* stack = m_ctx->undoStack.get();
    const bool undoneOurs = m_lastGridCmd && stack->command(index) == m_lastGridCmd;
    const bool redoneOurs = m_lastGridCmd && index > 0
        && stack->command(index - 1) == m_lastGridCmd;

    const bool haveMapping = !m_lastGridSnapshot.isEmpty() && !m_lastGridNewOrder.isEmpty();
    if (undoneOurs && haveMapping) {
        // Document went from m_lastGridNewOrder order back to m_lastGridSnapshot
        // order; the tracked rows are positions in the newOrder generation.
        QList<int> mapped;
        for (int v : m_selectedPages) {
            const int identity = m_lastGridNewOrder.value(v, -1);
            const int pos = identity >= 0 ? m_lastGridSnapshot.indexOf(identity) : -1;
            if (pos >= 0) mapped.append(pos);
        }
        const int identity = m_currentPageData >= 0
            ? m_lastGridNewOrder.value(m_currentPageData, -1) : -1;
        m_pendingSelection = mapped;
        m_pendingCurrentPage = identity >= 0 ? m_lastGridSnapshot.indexOf(identity) : -1;
    } else if (redoneOurs && haveMapping) {
        // Document went from m_lastGridSnapshot order to m_lastGridNewOrder
        // order; the tracked rows are positions in the snapshot generation.
        QList<int> mapped;
        for (int v : m_selectedPages) {
            const int identity = m_lastGridSnapshot.value(v, -1);
            const int pos = identity >= 0 ? m_lastGridNewOrder.indexOf(identity) : -1;
            if (pos >= 0) mapped.append(pos);
        }
        const int identity = m_currentPageData >= 0
            ? m_lastGridSnapshot.value(m_currentPageData, -1) : -1;
        m_pendingSelection = mapped;
        m_pendingCurrentPage = identity >= 0 ? m_lastGridNewOrder.indexOf(identity) : -1;
    } else {
        // Unrelated command (rotate, delete, …): best effort — keep the same
        // page positions selected across the reload.
        m_pendingSelection = m_selectedPages;
        m_pendingCurrentPage = m_currentPageData;
    }
    scheduleUndoRefresh();
}

void PagesMode::rebuildFromOrder(const QList<int>& order)
{
    m_gridRebuildGuard = true;
    m_pageList->clear();
    for (int pos = 0; pos < order.size(); ++pos)
        m_pageList->addItem(makePageItem(order[pos]));
    m_gridRebuildGuard = false;
    restoreSelectionByData(); // U06: keep the user's pages selected across reverts
}

} // namespace gp
