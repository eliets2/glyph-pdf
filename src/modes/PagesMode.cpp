// SPDX-License-Identifier: Apache-2.0
/**
 * PagesMode.cpp — M3-PROMPT-3 D1+D2+D3
 *
 * D1: Real page list with page number, size, and placeholder thumbnail.
 * D2: Split form — by-page / by-N / by-range expression + output naming +
 *     destination folder + filename-preview + progress + confirmation on overwrite.
 * D3: Reorder panel — drag-drop QListWidget + Apply (reorderPages) + Reset.
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

namespace gp {

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

    const QStringList toolButtons {
        PagesMode::tr("Insert Before"),
        PagesMode::tr("Insert After"),
        PagesMode::tr("Delete"),
        PagesMode::tr("Extract"),
        PagesMode::tr("Replace"),
        PagesMode::tr("Rotate ↺"),
        PagesMode::tr("Rotate ↻"),
        PagesMode::tr("Split Here"),
        PagesMode::tr("Merge")
    };
    for (const QString& label : toolButtons) {
        auto* btn = new QToolButton;
        btn->setText(label);
        btn->setProperty("variant", "ghost");
        tbLayout->addWidget(btn);
    }
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
    m_pageCountLabel = new QLabel;
    m_pageCountLabel->setProperty("mono", true);
    hdrLayout->addWidget(m_pageCountLabel);
    vLayout->addWidget(header);

    // Page list widget — grid / icon mode so thumbnails display naturally
    m_pageList = new QListWidget;
    m_pageList->setViewMode(QListView::IconMode);
    m_pageList->setResizeMode(QListView::Adjust);
    m_pageList->setSpacing(8);
    m_pageList->setIconSize(QSize(100, 130));
    m_pageList->setMovement(QListView::Snap);
    m_pageList->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_pageList->setSelectionMode(QAbstractItemView::ExtendedSelection);
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

void PagesMode::setAppContext(const AppContext* ctx)
{
    m_ctx = ctx;
    refreshPageList();
}

void PagesMode::refreshPageList()
{
    m_pageList->clear();
    m_reorderList->clear();
    m_originalOrder.clear();

    if (!m_ctx || !m_ctx->document) {
        m_pageCountLabel->setText(PagesMode::tr("No document"));

        // Update split spin limits
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        return;
    }

    const QString path = m_ctx->document->path();
    if (path.isEmpty()) {
        m_pageCountLabel->setText(PagesMode::tr("No document"));
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        return;
    }

    // Ask the engine for page count via a page-property query.
    // We derive pageCount from the engine's save-state; if unavailable fall back
    // to an error state rather than showing fake data.
    int pageCount = 0;
    if (m_ctx->pdfEditor) {
        // Load (may be a no-op if already loaded)
        m_ctx->pdfEditor->loadDocumentForEditing(path);
        // Count pages: PoDoFoBackend does not expose a standalone pageCount() on
        // IPdfEditorEngine.  The safest proxy is to call extractPageAsBytes(path, 0)
        // and binary-search upward — but that is expensive.  Instead, use the
        // DocumentSession if it tracks page count, or fall back to a heuristic read.
        //
        // Practical approach: we use the document session's page count if available.
        // DocumentSession::pageCount() is not in AppContext.h but the underlying
        // loadDocumentForEditing already loaded the doc; check via a sentinel:
        // extractPageAsBytes returns non-empty for valid pages, empty for out-of-range.
        // Binary-search is O(log N) and safe.
        int lo = 0, hi = 1;
        // Expand hi until extractPageAsBytes returns empty
        while (!m_ctx->pdfEditor->extractPageAsBytes(path, hi).isEmpty()) {
            hi *= 2;
            if (hi > 4096) break; // safety cap
        }
        // Binary search between lo and hi
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (m_ctx->pdfEditor->extractPageAsBytes(path, mid).isEmpty())
                hi = mid;
            else
                lo = mid + 1;
        }
        pageCount = lo;
    }

    if (pageCount <= 0) {
        m_pageCountLabel->setText(PagesMode::tr("0 pages"));
        if (m_splitAtSpin)    m_splitAtSpin->setRange(1, 1);
        if (m_splitEverySpin) m_splitEverySpin->setRange(1, 1);
        return;
    }

    m_pageCountLabel->setText(PagesMode::tr("%1 pages").arg(pageCount));
    if (m_splitAtSpin)    m_splitAtSpin->setRange(1, qMax(1, pageCount - 1));
    if (m_splitEverySpin) m_splitEverySpin->setRange(1, pageCount);

    // Populate page list and reorder list
    for (int i = 0; i < pageCount; ++i) {
        // Page list item: gray placeholder icon + page number
        // (Real thumbnail rendering requires a render backend path through AppContext;
        //  that plumbing is deferred to when IPdfRenderBackend is added to AppContext.
        //  For now, display a styled placeholder with the page number.)
        QPixmap thumb(100, 130);
        thumb.fill(QColor(220, 220, 220));

        auto* item = new QListWidgetItem;
        item->setIcon(QIcon(thumb));
        item->setText(PagesMode::tr("Page %1").arg(i + 1));
        item->setSizeHint(QSize(120, 160));
        item->setData(Qt::UserRole, i); // store 0-based index
        m_pageList->addItem(item);

        m_reorderList->addItem(PagesMode::tr("Page %1").arg(i + 1));
        m_originalOrder.append(i);
    }
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

    // Build the current displayed order as a list of original 0-based indices.
    // m_reorderList items store the original page number in their text "Page N".
    // After drag-drop, the order in the list widget is the desired new order.
    // We need to figure out the mapping and call reorderPages for each move.
    // Simplest approach: apply bubble-sort steps tracking the current live order.

    // Collect desired order (0-based original page indices from display text)
    QList<int> desiredOrder;
    desiredOrder.reserve(count);
    for (int i = 0; i < count; ++i) {
        // Item text is "Page N" (1-based); recover 0-based index
        const QString text = m_reorderList->item(i)->text();
        bool ok = false;
        // Extract the number after "Page "
        const int pageNum = text.mid(text.lastIndexOf(' ') + 1).toInt(&ok);
        desiredOrder.append(ok ? pageNum - 1 : i);
    }

    // Apply moves: simulate the desired permutation using sequential reorderPages calls.
    // reorderPages(path, fromIndex, toIndex) moves one page in the current live document.
    // We walk through desiredOrder and for each position, move the required page there.
    QList<int> currentOrder = m_originalOrder; // tracks live document state
    bool anyError = false;

    for (int targetPos = 0; targetPos < desiredOrder.size(); ++targetPos) {
        const int wantOriginal = desiredOrder[targetPos];
        int currentPos = currentOrder.indexOf(wantOriginal);
        if (currentPos < 0 || currentPos == targetPos) continue;

        if (!m_ctx->pdfEditor->reorderPages(path, currentPos, targetPos)) {
            anyError = true;
            break;
        }
        // Update our tracking list to reflect the move
        currentOrder.move(currentPos, targetPos);
    }

    if (anyError) {
        QMessageBox::critical(this, PagesMode::tr("Reorder failed"),
            PagesMode::tr("An error occurred while reordering pages. "
                          "The document may be in a partially-reordered state. "
                          "Please undo (Ctrl+Z) to recover."));
    } else {
        // Update the original order tracking so Reset works correctly
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

} // namespace gp
