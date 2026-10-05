// SPDX-License-Identifier: Apache-2.0
#include "modes/ComposeMode.h"

#include "shell/FlowToolbarLayout.h"
#include "util/GpTheme.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/IPdfRenderer.h"
#include "engines/BackendRouter.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSplitter>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace gp {

// ── §9.17 pure seams ─────────────────────────────────────────────────────────

int ComposeMode::insertionIndexFor(int afterPage0Based, int k)
{
    // RED stage: the position seam is the core order+position contract and is
    // completed in the GREEN stage; the shell ships it unset.
    Q_UNUSED(afterPage0Based); Q_UNUSED(k);
    return -1;
}

QString ComposeMode::pageSizeDisclosure(const QSizeF& sourcePage, const QSizeF& targetPage)
{
    Q_UNUSED(sourcePage); Q_UNUSED(targetPage);
    // RED stage: honest scaling disclosure pending (GREEN stage).
    return QString();
}

QRectF ComposeMode::fittedRect(const QSizeF& imageSizePt, const QRectF& box)
{
    Q_UNUSED(imageSizePt);
    return box;   // RED stage: aspect-preserving fit pending (GREEN stage).
}

QString ComposeMode::imagePlacementDisclosure(const QSizeF& imageSizePt, const QRectF& box)
{
    Q_UNUSED(imageSizePt); Q_UNUSED(box);
    // RED stage: honest placement disclosure pending (GREEN stage).
    return QString();
}

// ── page-count probe (the PagesMode AR-7 D2 idiom, renderer-backed) ─────────

namespace {
int probePageCount(IPdfRenderer& renderer)
{
    // pageSize() returns QSizeF() out of range — exponential doubling then a
    // binary search pins the count with O(log n) probes, no UI.
    if (renderer.pageSize(0).isEmpty())
        return 0;
    int lo = 0;
    int hi = 1;
    while (hi < 100000 && !renderer.pageSize(hi).isEmpty()) {
        lo = hi;
        hi *= 2;
    }
    while (hi - lo > 1) {
        const int mid = lo + (hi - lo) / 2;
        if (renderer.pageSize(mid).isEmpty()) hi = mid; else lo = mid;
    }
    return lo + 1;
}

QImage renderThumbnail(IPdfRenderer& renderer, int page)
{
    const QImage full = renderer.renderPage(page, 36);   // low-dpi thumbnail
    if (full.isNull()) return full;
    return full.scaled(96, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
} // namespace

int ComposeMode::pageCountFor(const QString& path)
{
    auto renderer = BackendRouter::rendererFor(path);
    if (!renderer) return -1;
    return probePageCount(*renderer);
}

QList<ComposeImageInfo> ComposeMode::imageInventory(const QString& pdfPath, int pageIndex)
{
    return ImageExtractEngine::listPageImages(pdfPath, pageIndex);
}

// ── construction ─────────────────────────────────────────────────────────────

ComposeMode::ComposeMode(QWidget* parent) : QWidget(parent)
{
    m_engineFactory = []() -> std::shared_ptr<IPdfEditorEngine> {
        return std::make_shared<PdfEditorEngine>();
    };
    buildUi();
}

ComposeMode::~ComposeMode() = default;

void ComposeMode::setAppContext(const AppContext* ctx)
{
    m_ctx = ctx;
}

void ComposeMode::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // toolbar — the CompareMode wrapping flow row
    auto* tb = new QFrame;
    tb->setProperty("role", "modeToolbar");
    auto* hrow = new FlowToolbarLayout(tb);
    hrow->setLineHeightFloor(Theme::ToolbarH);
    hrow->setContentsMargins(10, 0, 10, 0);
    hrow->setSpacing(6);
    auto mono = [](const QString& s) { auto* l = new QLabel(s); l->setProperty("mono", true); return l; };
    hrow->addWidget(mono(tr("COMPOSE")));
    m_filesLabel = mono(tr("No documents — use Choose Documents to open two PDFs"));
    m_filesLabel->setObjectName(QStringLiteral("composeFilesLabel"));
    hrow->addWidget(m_filesLabel);
    m_chooseBtn = new QToolButton;
    m_chooseBtn->setObjectName(QStringLiteral("composeBtnChoose"));
    m_chooseBtn->setText(tr("Choose Documents…"));
    m_chooseBtn->setToolTip(tr("Open the source and target documents side by side"));
    connect(m_chooseBtn, &QToolButton::clicked, this, &ComposeMode::onChooseDocuments);
    hrow->addWidget(m_chooseBtn);
    hrow->addWidget(mono(tr("INSERT TARGET AFTER PAGE")));
    m_afterSpin = new QSpinBox;
    m_afterSpin->setObjectName(QStringLiteral("composeAfterSpin"));
    m_afterSpin->setRange(-1, 0);
    m_afterSpin->setSpecialValueText(tr("document start"));
    m_afterSpin->setToolTip(tr("New pages are inserted after this page of the target document"));
    connect(m_afterSpin, &QSpinBox::valueChanged, this, [this](int v) {
        setInsertAfterPage(ComposeSide::Target, v);
    });
    hrow->addWidget(m_afterSpin);
    m_applyBtn = new QToolButton;
    m_applyBtn->setObjectName(QStringLiteral("composeBtnApply"));
    m_applyBtn->setText(tr("Apply Transfers"));
    m_applyBtn->setToolTip(tr("Insert the picked pages and images into the other document"));
    m_applyBtn->setEnabled(false);
    connect(m_applyBtn, &QToolButton::clicked, this, &ComposeMode::onApply);
    hrow->addWidget(m_applyBtn);
    m_undoBtn = new QToolButton;
    m_undoBtn->setObjectName(QStringLiteral("composeBtnUndo"));
    m_undoBtn->setText(tr("Undo"));
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QToolButton::clicked, this, &ComposeMode::onUndo);
    hrow->addWidget(m_undoBtn);
    m_redoBtn = new QToolButton;
    m_redoBtn->setObjectName(QStringLiteral("composeBtnRedo"));
    m_redoBtn->setText(tr("Redo"));
    m_redoBtn->setEnabled(false);
    connect(m_redoBtn, &QToolButton::clicked, this, &ComposeMode::onRedo);
    hrow->addWidget(m_redoBtn);
    m_statusLabel = mono(tr("IDLE"));
    m_statusLabel->setObjectName(QStringLiteral("composeStatusLabel"));
    hrow->addWidget(m_statusLabel);
    hrow->addStretch(1);
    col->addWidget(tb);

    // two live panes — source left, target right
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName(QStringLiteral("composeSplitter"));
    for (const int idx : { 0, 1 }) {
        const ComposeSide side = static_cast<ComposeSide>(idx);
        const QString role = sideTitle(side);
        auto* paneFrame = new QFrame(split);
        auto* pl = new QVBoxLayout(paneFrame);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(0);

        auto& p = m_panes[idx];
        p.role = role;
        p.header = new QLabel(tr("%1 — no document").arg(role));
        p.header->setProperty("mono", true);
        p.header->setProperty("role", "modeToolbar");
        p.header->setFixedHeight(26);
        p.header->setContentsMargins(12, 0, 12, 0);
        pl->addWidget(p.header);

        auto* pagesHead = new QLabel(tr("PAGES — check to pick"));
        pagesHead->setProperty("mono", true);
        pl->addWidget(pagesHead);
        p.grid = new QListWidget(paneFrame);
        p.grid->setObjectName(idx == 0 ? QStringLiteral("composeSourceGrid")
                                       : QStringLiteral("composeTargetGrid"));
        p.grid->setViewMode(QListView::IconMode);
        p.grid->setResizeMode(QListView::Adjust);
        p.grid->setUniformItemSizes(true);
        p.grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(p.grid, &QListWidget::itemChanged, this, &ComposeMode::onPageItemChanged);
        connect(p.grid, &QListWidget::currentRowChanged, this, &ComposeMode::onPaneCurrentRowChanged);
        pl->addWidget(p.grid, 3);

        auto* imagesHead = new QLabel(tr("IMAGES ON SELECTED PAGE — check to pick"));
        imagesHead->setProperty("mono", true);
        pl->addWidget(imagesHead);
        p.imagePicker = new QListWidget(paneFrame);
        p.imagePicker->setObjectName(idx == 0 ? QStringLiteral("composeSourceImages")
                                              : QStringLiteral("composeTargetImages"));
        p.imagePicker->setViewMode(QListView::IconMode);
        p.imagePicker->setResizeMode(QListView::Adjust);
        connect(p.imagePicker, &QListWidget::itemChanged, this, [this, side](QListWidgetItem* item) {
            const int row = m_panes[static_cast<int>(side)].imagePicker->row(item);
            if (row < 0) return;
            const ComposeImagePick pick{
                row,
                item->data(Qt::UserRole).toString(),
                item->data(Qt::UserRole + 1).toSize(),
                item->data(Qt::UserRole + 2).toStringList()
            };
            if (item->checkState() == Qt::Checked)
                addImagePick(side, pick);
            else
                m_imagePicks[static_cast<int>(side)].removeOne(pick);
            refreshTransferUi();
        });
        pl->addWidget(p.imagePicker, 2);
    }

    // transfer list + composed-order preview
    auto* bottom = new QFrame;
    auto* bl = new QHBoxLayout(bottom);
    bl->setContentsMargins(0, 0, 0, 0);
    m_transferList = new QListWidget(bottom);
    m_transferList->setObjectName(QStringLiteral("composeTransferList"));
    m_transferList->setMaximumHeight(110);
    bl->addWidget(m_transferList, 1);
    split->addWidget(paneFrameOf(ComposeSide::Source));
    split->addWidget(paneFrameOf(ComposeSide::Target));
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    col->addWidget(split, 1);
    col->addWidget(bottom);
}

// ── two-document open ────────────────────────────────────────────────────────

bool ComposeMode::setDocuments(const QString& sourcePath, const QString& targetPath)
{
    // Validate BOTH documents through throwaway renderers BEFORE any widget
    // state moves — a refusal anywhere keeps the previous state untouched.
    auto load = [](const QString& path, std::unique_ptr<IPdfRenderer>& renderer,
                   int& count, QList<QSizeF>& sizes) {
        if (path.isEmpty() || !QFileInfo::exists(path)) return false;
        renderer = BackendRouter::rendererFor(path);
        if (!renderer) return false;
        count = probePageCount(*renderer);
        if (count < 0) return false;
        sizes.clear();
        for (int i = 0; i < count; ++i)
            sizes.append(renderer->pageSize(i));
        return true;
    };
    std::unique_ptr<IPdfRenderer> srcRenderer, tgtRenderer;
    int srcCount = 0, tgtCount = 0;
    QList<QSizeF> srcSizes, tgtSizes;
    if (!load(sourcePath, srcRenderer, srcCount, srcSizes)
        || !load(targetPath, tgtRenderer, tgtCount, tgtSizes)) {
        setStatus(tr("OPEN REFUSED — both documents must be readable PDFs"));
        emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
        return false;
    }

    // New documents = new compose sessions (the DocumentSession identity
    // rule): history and picks reset, the pane widgets keep their state.
    for (int i = 0; i < 2; ++i) m_history[i].clear();
    m_pagePicks[0].clear();
    m_pagePicks[1].clear();
    m_imagePicks[0].clear();
    m_imagePicks[1].clear();

    auto commit = [this](ComposeSide side, const QString& path,
                         std::unique_ptr<IPdfRenderer>& renderer,
                         int count, const QList<QSizeF>& sizes) {
        Pane& p = pane(side);
        p.pageSizes = sizes;
        p.path = path;
        p.renderer = std::move(renderer);
        p.pageCount = count;
        p.currentPage = 0;
        // r4-ux discipline: the pane surfaces name the document role, file
        // and page count (the cmpFilesLabel disclosure, to screen readers).
        const QString fileName = QFileInfo(path).fileName();
        p.grid->setAccessibleName(
            tr("%1 document pages \xE2\x80\x94 %2 (%3 pages)")
                .arg(p.role, fileName).arg(count));
        p.grid->setAccessibleDescription(
            tr("Page thumbnails of the %1 document; check pages to pick them for composition")
                .arg(p.role.toLower()));
        p.header->setText(tr("%1 \xE2\x80\x94 %2 (%3 pages)")
                              .arg(p.role, fileName).arg(count));
        fillPageGrid(side);
        fillImagePicker(side, 0);
    };
    commit(ComposeSide::Source, sourcePath, srcRenderer, srcCount, srcSizes);
    commit(ComposeSide::Target, targetPath, tgtRenderer, tgtCount, tgtSizes);

    updateFilesLabel();
    refreshTransferUi();
    setStatus(tr("TWO DOCUMENTS OPEN — pick pages or images, then Apply Transfers"));
    emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
    return true;
}

bool ComposeMode::loadPane(ComposeSide side, const QString& path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) return false;
    auto renderer = BackendRouter::rendererFor(path);
    if (!renderer) return false;
    const int count = probePageCount(*renderer);
    if (count < 0) return false;
    Pane& p = pane(side);
    p.pageSizes.clear();
    for (int i = 0; i < count; ++i)
        p.pageSizes.append(renderer->pageSize(i));
    p.path = path;
    p.renderer = std::move(renderer);
    p.pageCount = count;
    p.currentPage = 0;
    // r4-ux discipline: pane surfaces name role, file and page count — kept
    // in step on every reload (post-apply / post-undo).
    const QString fileName = QFileInfo(path).fileName();
    p.grid->setAccessibleName(
        tr("%1 document pages \xE2\x80\x94 %2 (%3 pages)")
            .arg(p.role, fileName).arg(count));
    p.grid->setAccessibleDescription(
        tr("Page thumbnails of the %1 document; check pages to pick them for composition")
            .arg(p.role.toLower()));
    fillPageGrid(side);
    fillImagePicker(side, 0);
    p.header->setText(tr("%1 — %2 (%3 pages)").arg(p.role, fileName).arg(count));
    return true;
}

void ComposeMode::fillPageGrid(ComposeSide side)
{
    Pane& p = pane(side);
    QSignalBlocker blocker(p.grid);   // programmatic rebuild: no pick feedback loops
    p.grid->clear();
    for (int i = 0; i < p.pageCount; ++i) {
        auto* item = new QListWidgetItem(p.grid);
        item->setText(ComposeMode::tr("Page %1").arg(i + 1));
        item->setData(Qt::UserRole, i);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(m_pagePicks[static_cast<int>(side)].contains(i)
                                ? Qt::Checked : Qt::Unchecked);
        // RED stage: the r4-ux accessibleName discipline (role + page numbers
        // on every thumbnail surface) lands in the GREEN stage.
        if (p.renderer && i < 200)
            item->setIcon(QIcon(QPixmap::fromImage(renderThumbnail(*p.renderer, i))));
    }
}

void ComposeMode::fillImagePicker(ComposeSide side, int pageIndex)
{
    Pane& p = pane(side);
    p.currentPage = pageIndex;
    QSignalBlocker blocker(p.imagePicker);
    p.imagePicker->clear();
    const QList<ComposeImageInfo> inventory = imageInventory(p.path, pageIndex);
    for (const auto& info : inventory) {
        auto* item = new QListWidgetItem(p.imagePicker);
        item->setText(QStringLiteral("%1 (%2×%3)").arg(info.xobjectName)
                          .arg(info.widthPx).arg(info.heightPx));
        item->setData(Qt::UserRole, info.xobjectName);
        item->setData(Qt::UserRole + 1, QSize(info.widthPx, info.heightPx));
        item->setData(Qt::UserRole + 2, info.filters);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        if (!info.pixels.isNull())
            item->setIcon(QIcon(QPixmap::fromImage(
                info.pixels.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        // RED stage: accessible text (role + page + object identity) lands
        // in the GREEN stage.
    }
}

// ── picks ────────────────────────────────────────────────────────────────────

void ComposeMode::clearPicks()
{
    for (int i = 0; i < 2; ++i) {
        m_pagePicks[i].clear();
        m_imagePicks[i].clear();
    }
    for (int i = 0; i < 2; ++i)
        if (m_panes[i].grid) fillPageGrid(static_cast<ComposeSide>(i));
    refreshTransferUi();
}

void ComposeMode::addPagePick(ComposeSide side, int page0Based)
{
    if (page0Based < 0 || page0Based >= pane(side).pageCount) return;
    if (!m_pagePicks[static_cast<int>(side)].contains(page0Based))
        m_pagePicks[static_cast<int>(side)].append(page0Based);
    refreshTransferUi();
}

void ComposeMode::removePagePick(ComposeSide side, int page0Based)
{
    m_pagePicks[static_cast<int>(side)].removeAll(page0Based);
    refreshTransferUi();
}

QList<int> ComposeMode::pagePicks(ComposeSide side) const
{
    return m_pagePicks[static_cast<int>(side)];
}

void ComposeMode::addImagePick(ComposeSide side, const ComposeImagePick& pick)
{
    if (!m_imagePicks[static_cast<int>(side)].contains(pick))
        m_imagePicks[static_cast<int>(side)].append(pick);
    refreshTransferUi();
}

void ComposeMode::clearImagePicks(ComposeSide side)
{
    m_imagePicks[static_cast<int>(side)].clear();
    refreshTransferUi();
}

QList<ComposeImagePick> ComposeMode::imagePicks(ComposeSide side) const
{
    return m_imagePicks[static_cast<int>(side)];
}

void ComposeMode::setInsertAfterPage(ComposeSide destination, int afterPage0Based)
{
    m_insertAfter[static_cast<int>(destination)] = afterPage0Based;
    refreshTransferUi();
}

int ComposeMode::insertAfterPage(ComposeSide destination) const
{
    return m_insertAfter[static_cast<int>(destination)];
}

void ComposeMode::setImagePlacementBox(ComposeSide destination, const QRectF& box)
{
    m_imageBox[static_cast<int>(destination)] = box;
    refreshTransferUi();
}

QRectF ComposeMode::imagePlacementBox(ComposeSide destination) const
{
    return m_imageBox[static_cast<int>(destination)];
}

// ── preview + apply ──────────────────────────────────────────────────────────

QStringList ComposeMode::pendingSummary() const
{
    // RED stage: the transfer-list preview of record lands in the GREEN stage.
    return QStringList();
}

QString ComposeMode::composedOrderPreview() const
{
    // RED stage: the composed-order live preview lands in the GREEN stage.
    return QString();
}

bool ComposeMode::applyTransfers(QString* why)
{
    // RED stage: the ONE-step SafeSave transaction lands in the GREEN stage;
    // the shell refuses honestly instead of pretending.
    if (why) *why = tr("Composition is not available yet — nothing was changed.");
    setStatus(tr("APPLY UNAVAILABLE"));
    return false;
}

bool ComposeMode::undoTransfers(ComposeSide destination)
{
    Q_UNUSED(destination);
    return false;   // RED stage: checked traversal lands in the GREEN stage.
}

bool ComposeMode::redoTransfers(ComposeSide destination)
{
    Q_UNUSED(destination);
    return false;   // RED stage: checked traversal lands in the GREEN stage.
}

int ComposeMode::historyCount(ComposeSide destination) const
{
    return m_history[static_cast<int>(destination)].count();
}

// ── UI plumbing ──────────────────────────────────────────────────────────────

QString ComposeMode::sideTitle(ComposeSide side) const
{
    return side == ComposeSide::Source ? tr("SOURCE") : tr("TARGET");
}

int ComposeMode::pageCount(ComposeSide side) const
{
    return pane(side).pageCount;
}

QWidget* ComposeMode::paneFrameOf(ComposeSide side)
{
    return static_cast<QWidget*>(pane(side).grid->parent());
}

void ComposeMode::onPageItemChanged(QListWidgetItem* item)
{
    if (!item) return;
    auto* grid = qobject_cast<QListWidget*>(sender());
    for (const int idx : { 0, 1 }) {
        if (m_panes[idx].grid != grid) continue;
        const ComposeSide side = static_cast<ComposeSide>(idx);
        const int page = item->data(Qt::UserRole).toInt();
        if (item->checkState() == Qt::Checked)
            addPagePick(side, page);
        else
            removePagePick(side, page);
        return;
    }
}

void ComposeMode::onPaneCurrentRowChanged(int row)
{
    auto* grid = qobject_cast<QListWidget*>(sender());
    for (const int idx : { 0, 1 }) {
        if (m_panes[idx].grid != grid) continue;
        if (row < 0) return;
        fillImagePicker(static_cast<ComposeSide>(idx), row);
        return;
    }
}

void ComposeMode::onApply()
{
    QString why;
    if (!applyTransfers(&why))
        QMessageBox::warning(this, tr("Compose"), why);
}

void ComposeMode::onUndo()
{
    undoTransfers(ComposeSide::Target) || undoTransfers(ComposeSide::Source);
}

void ComposeMode::onRedo()
{
    redoTransfers(ComposeSide::Target) || redoTransfers(ComposeSide::Source);
}

void ComposeMode::onChooseDocuments()
{
    const QString src = QFileDialog::getOpenFileName(
        this, tr("Select Source Document"), QString(), tr("PDF files (*.pdf);;All files (*)"));
    if (src.isEmpty()) return;
    const QString tgt = QFileDialog::getOpenFileName(
        this, tr("Select Target Document"), QString(), tr("PDF files (*.pdf);;All files (*)"));
    if (tgt.isEmpty()) return;
    setDocuments(src, tgt);
}

void ComposeMode::updateFilesLabel()
{
    const Pane& s = pane(ComposeSide::Source);
    const Pane& t = pane(ComposeSide::Target);
    if (s.path.isEmpty() || t.path.isEmpty()) return;
    m_filesLabel->setText(QStringLiteral("%1 (%2 pp)   \xE2\x86\x92   %3 (%4 pp)")
                              .arg(QFileInfo(s.path).fileName()).arg(s.pageCount)
                              .arg(QFileInfo(t.path).fileName()).arg(t.pageCount));
}

void ComposeMode::refreshTransferUi()
{
    const QStringList pending = pendingSummary();
    m_transferList->clear();
    for (const QString& row : pending)
        m_transferList->addItem(row);
    m_applyBtn->setEnabled(!pending.isEmpty());
    m_undoBtn->setEnabled(m_history[1].canUndo() || m_history[0].canUndo());
    m_redoBtn->setEnabled(m_history[1].canRedo() || m_history[0].canRedo());
    const Pane& t = pane(ComposeSide::Target);
    QSignalBlocker blocker(m_afterSpin);
    m_afterSpin->setRange(-1, std::max(0, t.pageCount - 1));
}

void ComposeMode::setStatus(const QString& message)
{
    if (m_statusLabel) m_statusLabel->setText(message);
    emit statusMessageRequested(message);
}

} // namespace gp
