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
#include <QFile>
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
    // "after page N": the k-th pick of the group lands at N+1+k (N = -1 is
    // the document start, so the first pick lands at index 0).
    return afterPage0Based + 1 + k;
}

QString ComposeMode::pageSizeDisclosure(const QSizeF& sourcePage, const QSizeF& targetPage)
{
    if (std::abs(sourcePage.width() - targetPage.width()) <= 0.5
        && std::abs(sourcePage.height() - targetPage.height()) <= 0.5)
        return QString();
    return ComposeMode::tr("source page is %1\xC3\x97%2 pt, target page is %3\xC3\x97%4 pt "
                           "\xE2\x80\x94 the page keeps its own size, never stretched to match the target")
        .arg(sourcePage.width(), 0, 'f', 0).arg(sourcePage.height(), 0, 'f', 0)
        .arg(targetPage.width(), 0, 'f', 0).arg(targetPage.height(), 0, 'f', 0);
}

QRectF ComposeMode::fittedRect(const QSizeF& imageSizePt, const QRectF& box)
{
    if (box.isEmpty() || imageSizePt.isEmpty()
        || imageSizePt.width() <= 0.0 || imageSizePt.height() <= 0.0)
        return box;
    // Letterboxed, centered, aspect preserved — the honest scale contract.
    const double scale = std::min(box.width() / imageSizePt.width(),
                                  box.height() / imageSizePt.height());
    const double w = imageSizePt.width() * scale;
    const double h = imageSizePt.height() * scale;
    return QRectF(box.left() + (box.width() - w) / 2.0,
                  box.top() + (box.height() - h) / 2.0, w, h);
}

QString ComposeMode::imagePlacementDisclosure(const QSizeF& imageSizePt, const QRectF& box)
{
    const QRectF drawn = fittedRect(imageSizePt, box);
    return ComposeMode::tr("image (%1\xC3\x97%2 px at 72 ppi) is drawn %3\xC3\x97%4 pt, "
                           "aspect ratio preserved, never stretched")
        .arg(imageSizePt.width(), 0, 'f', 0).arg(imageSizePt.height(), 0, 'f', 0)
        .arg(drawn.width(), 0, 'f', 1).arg(drawn.height(), 0, 'f', 1);
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
    // r4-ux discipline: the transfer surface announces itself and carries
    // the pending rows' roles to screen readers.
    m_transferList->setAccessibleName(tr("Pending transfers"));
    m_transferList->setAccessibleDescription(
        tr("Pages and images picked in either pane, applied to the other document when you choose Apply Transfers"));
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
    // r4-ux discipline: the pane surfaces name the document role, file and
    // page count (the cmpFilesLabel disclosure, delivered to screen readers).
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
        // r4-ux discipline: every thumbnail surface carries the document
        // role (source/target), the file, the page position and the pick
        // state — the same disclosure a sighted user reads off the pane.
        item->setData(Qt::AccessibleTextRole,
                      tr("%1 document %2, page %3 of %4, %5")
                          .arg(p.role)
                          .arg(QFileInfo(p.path).fileName())
                          .arg(i + 1).arg(p.pageCount)
                          .arg(m_pagePicks[static_cast<int>(side)].contains(i)
                                   ? tr("picked") : tr("not picked")));
        if (p.renderer && i < 200)
            item->setIcon(QIcon(QPixmap::fromImage(renderThumbnail(*p.renderer, i))));
    }
}

void ComposeMode::fillImagePicker(ComposeSide side, int pageIndex)
{
    Pane& p = pane(side);
    p.currentPage = pageIndex;
    // r4-ux discipline: the image picker names the pane role, the page it
    // inventories and the document it belongs to.
    p.imagePicker->setAccessibleName(
        tr("%1 document images on page %2 of %3 \xE2\x80\x94 %4")
            .arg(p.role).arg(pageIndex + 1).arg(p.pageCount)
            .arg(QFileInfo(p.path).fileName()));
    p.imagePicker->setAccessibleDescription(
        tr("Embedded images of the selected page; check one to pick it for placement in the other document"));
    QSignalBlocker blocker(p.imagePicker);
    p.imagePicker->clear();
    const QList<ComposeImageInfo> inventory = imageInventory(p.path, pageIndex);
    for (const auto& info : inventory) {
        auto* item = new QListWidgetItem(p.imagePicker);
        item->setText(QStringLiteral("%1 (%2\xC3\x97%3)").arg(info.xobjectName)
                          .arg(info.widthPx).arg(info.heightPx));
        item->setData(Qt::UserRole, info.xobjectName);
        item->setData(Qt::UserRole + 1, QSize(info.widthPx, info.heightPx));
        item->setData(Qt::UserRole + 2, info.filters);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        // r4-ux discipline: object identity + source page in the a11y text.
        item->setData(Qt::AccessibleTextRole,
                      tr("%1 document %2, page %3, image %4 (%5\xC3\x97%6 px, %7), not picked")
                          .arg(p.role)
                          .arg(QFileInfo(p.path).fileName())
                          .arg(pageIndex + 1)
                          .arg(info.xobjectName)
                          .arg(info.widthPx).arg(info.heightPx)
                          .arg(info.filters.isEmpty()
                                   ? tr("no filter")
                                   : info.filters.join(QStringLiteral(", "))));
        if (!info.pixels.isNull())
            item->setIcon(QIcon(QPixmap::fromImage(
                info.pixels.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
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
    // The transfer list's preview of record: one row per pending transfer as
    // it will be applied, including the honest size disclosure for a
    // mismatched page (computed BEFORE anything is written).
    QStringList out;
    const Pane& src = pane(ComposeSide::Source);
    const Pane& tgt = pane(ComposeSide::Target);

    // Source picks → the target document.
    const QList<int>& srcPicks = m_pagePicks[static_cast<int>(ComposeSide::Source)];
    for (int k = 0; k < srcPicks.size(); ++k) {
        const int page = srcPicks.at(k);
        const int at = insertionIndexFor(m_insertAfter[1], k);
        const QSizeF sp = k < src.pageSizes.size() && page < src.pageSizes.size()
                              ? src.pageSizes.at(page) : QSizeF();
        const int neighbor = tgt.pageSizes.isEmpty()
                                 ? -1 : qBound(0, qMin(at, tgt.pageCount - 1), tgt.pageCount - 1);
        const QSizeF tp = neighbor >= 0 ? tgt.pageSizes.at(neighbor) : QSizeF();
        QString row = tr("Page %1 of %2 \xE2\x86\x92 insert after target page %3")
                          .arg(page + 1)
                          .arg(QFileInfo(src.path).fileName())
                          .arg(m_insertAfter[1] < 0 ? tr("(start)")
                                                    : QString::number(m_insertAfter[1] + 1));
        const QString d = (sp.isEmpty() || tp.isEmpty())
                              ? QString() : pageSizeDisclosure(sp, tp);
        if (!d.isEmpty())
            row += QStringLiteral(" \xE2\x80\x94 ") + d;
        out << row;
    }
    // Source image picks → the target document.
    for (const auto& pick : m_imagePicks[static_cast<int>(ComposeSide::Source)]) {
        const QRectF box = effectiveImageBox(ComposeSide::Target);
        QString row = tr("Image %1 from page %2 of %3 \xE2\x86\x92 place on target page %4, %5")
                          .arg(pick.xobjectName)
                          .arg(pick.sourcePage + 1)
                          .arg(QFileInfo(src.path).fileName())
                          .arg(imageTargetPage(ComposeSide::Target) + 1)
                          .arg(imagePlacementDisclosure(QSizeF(pick.pixelSize), box));
        out << row;
    }

    // Target picks → the source document (mirrored rows).
    const QList<int>& tgtPicks = m_pagePicks[static_cast<int>(ComposeSide::Target)];
    for (int k = 0; k < tgtPicks.size(); ++k) {
        const int page = tgtPicks.at(k);
        const int at = insertionIndexFor(m_insertAfter[0], k);
        const QSizeF tp = page < tgt.pageSizes.size() ? tgt.pageSizes.at(page) : QSizeF();
        const int neighbor = src.pageSizes.isEmpty()
                                 ? -1 : qBound(0, qMin(at, src.pageCount - 1), src.pageCount - 1);
        const QSizeF sp = neighbor >= 0 ? src.pageSizes.at(neighbor) : QSizeF();
        QString row = tr("Page %1 of %2 \xE2\x86\x92 insert after source page %3")
                          .arg(page + 1)
                          .arg(QFileInfo(tgt.path).fileName())
                          .arg(m_insertAfter[0] < 0 ? tr("(start)")
                                                    : QString::number(m_insertAfter[0] + 1));
        const QString d = (sp.isEmpty() || tp.isEmpty())
                              ? QString() : pageSizeDisclosure(tp, sp);
        if (!d.isEmpty())
            row += QStringLiteral(" \xE2\x80\x94 ") + d;
        out << row;
    }
    for (const auto& pick : m_imagePicks[static_cast<int>(ComposeSide::Target)]) {
        const QRectF box = effectiveImageBox(ComposeSide::Source);
        out << tr("Image %1 from page %2 of %3 \xE2\x86\x92 place on source page %4, %5")
                   .arg(pick.xobjectName)
                   .arg(pick.sourcePage + 1)
                   .arg(QFileInfo(tgt.path).fileName())
                   .arg(imageTargetPage(ComposeSide::Source) + 1)
                   .arg(imagePlacementDisclosure(QSizeF(pick.pixelSize), box));
    }
    return out;
}

QString ComposeMode::composedOrderPreview() const
{
    // Live textual preview of the composed TARGET page order:
    // "1 [S2] [S1] 2 3" — bracketed tokens are the pending source picks.
    const Pane& tgt = pane(ComposeSide::Target);
    if (tgt.pageCount <= 0) return QString();
    const QList<int>& picks = m_pagePicks[static_cast<int>(ComposeSide::Source)];
    QStringList tokens;
    int inserted = 0;
    for (int i = 0; i < tgt.pageCount; ++i) {
        while (inserted < picks.size()
               && insertionIndexFor(m_insertAfter[1], inserted) == i) {
            tokens << tr("[S%1]").arg(picks.at(inserted) + 1);
            ++inserted;
        }
        tokens << QString::number(i + 1);
    }
    while (inserted < picks.size()) {
        tokens << tr("[S%1]").arg(picks.at(inserted) + 1);
        ++inserted;
    }
    return tokens.join(QLatin1Char(' '));
}

bool ComposeMode::applyTransfers(QString* why)
{
    // One apply = ONE checked-history step per affected destination session,
    // each a full SafeSave candidate transaction. A refusal commits nothing
    // for that destination, pushes no history step, and stays retryable.
    const bool hasSourcePicks =
        !m_pagePicks[0].isEmpty() || !m_imagePicks[0].isEmpty();
    const bool hasTargetPicks =
        !m_pagePicks[1].isEmpty() || !m_imagePicks[1].isEmpty();
    if (!hasSourcePicks && !hasTargetPicks) {
        if (why) *why = tr("Nothing is picked — check pages or images to transfer first.");
        setStatus(tr("APPLY REFUSED — NOTHING PICKED"));
        return false;
    }

    QStringList failures;
    int appliedPages = 0;
    int appliedImages = 0;

    if (hasSourcePicks) {
        QString err;
        if (applyOneSide(ComposeSide::Target, appliedPages, appliedImages, &err)) {
            m_pagePicks[0].clear();
            m_imagePicks[0].clear();
            fillPageGrid(ComposeSide::Source);
        } else {
            failures << err;
        }
    }
    if (hasTargetPicks) {
        QString err;
        int pages = 0, images = 0;
        if (applyOneSide(ComposeSide::Source, pages, images, &err)) {
            appliedPages += pages;
            appliedImages += images;
            m_pagePicks[1].clear();
            m_imagePicks[1].clear();
            fillPageGrid(ComposeSide::Target);
        } else {
            failures << err;
        }
    }

    // Reload the affected destination panes (their page structure changed).
    reloadPaneIfLoaded(ComposeSide::Target);
    reloadPaneIfLoaded(ComposeSide::Source);
    updateFilesLabel();
    refreshTransferUi();

    if (!failures.isEmpty()) {
        if (why) *why = failures.join(QLatin1Char('\n'));
        setStatus(tr("APPLY REFUSED — NOTHING WRITTEN FOR THE FAILED SIDE"));
        emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
        return false;
    }
    setStatus(tr("APPLIED %1 page(s), %2 image(s) \xE2\x80\x94 use Undo to revert")
                  .arg(appliedPages).arg(appliedImages));
    emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
    return true;
}

bool ComposeMode::undoTransfers(ComposeSide destination)
{
    // Park the pane renderer: the undo commits a replacement of the
    // destination file, and pdfium's loader holds an OS handle on it.
    m_panes[static_cast<int>(destination)].renderer.reset();
    // G08 checked traversal: the history position moves only after a
    // successful restoration; a failed undo stays retryable.
    if (!CheckedHistory::undo(&m_history[static_cast<int>(destination)])) {
        reloadPaneIfLoaded(destination);   // nothing changed; restore the parked renderer
        return false;
    }
    reloadPaneIfLoaded(destination);
    updateFilesLabel();
    refreshTransferUi();
    setStatus(tr("COMPOSE STEP UNDONE"));
    emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
    return true;
}

bool ComposeMode::redoTransfers(ComposeSide destination)
{
    // The redo mirror of the undo parking (same destination replacement).
    m_panes[static_cast<int>(destination)].renderer.reset();
    // WP-R03 checked traversal mirror.
    if (!CheckedHistory::redo(&m_history[static_cast<int>(destination)])) {
        reloadPaneIfLoaded(destination);   // nothing changed; restore the parked renderer
        return false;
    }
    reloadPaneIfLoaded(destination);
    updateFilesLabel();
    refreshTransferUi();
    setStatus(tr("COMPOSE STEP REDONE"));
    emit statusMessageRequested(m_statusLabel ? m_statusLabel->text() : QString());
    return true;
}

// ── apply internals ──────────────────────────────────────────────────────────

QRectF ComposeMode::effectiveImageBox(ComposeSide destination) const
{
    const QRectF& configured = m_imageBox[static_cast<int>(destination)];
    if (configured.isValid())
        return configured;
    // Auto placement box: the centered half-page box of the placement page
    // (the drawn image is aspect-fitted INSIDE it — never stretched).
    const Pane& dst = pane(destination);
    const QSizeF ps = dst.pageSizes.value(imageTargetPage(destination));
    if (ps.isEmpty())
        return QRectF(0, 0, 200, 200);
    return QRectF(ps.width() * 0.25, ps.height() * 0.25,
                  ps.width() * 0.5, ps.height() * 0.5);
}

int ComposeMode::imageTargetPage(ComposeSide destination) const
{
    // The first destination page AFTER the whole inserted block (so an image
    // pick never lands underneath the pages inserted by the same apply).
    const Pane& dst = pane(destination);
    const int after = insertionIndexFor(m_insertAfter[static_cast<int>(destination)],
                                        m_pagePicks[1 - static_cast<int>(destination)].size());
    return dst.pageCount > 0 ? qBound(0, after, dst.pageCount - 1) : 0;
}

bool ComposeMode::applyOneSide(ComposeSide destination, int& outPages, int& outImages,
                               QString* err)
{
    const int destIdx = static_cast<int>(destination);
    const int srcIdx = 1 - destIdx;
    const Pane& src = m_panes[srcIdx];
    const Pane& dst = m_panes[destIdx];
    auto refuse = [err](const QString& message) {
        if (err) *err = message;
        return false;
    };

    if (dst.path.isEmpty())
        return refuse(tr("No destination document is open."));
    if (src.path.isEmpty())
        return refuse(tr("No source document is open."));

    // Extract the picked pages from the SOURCE through an operation-owned
    // engine (the SplitEngineFactory idiom). The source file is only ever
    // READ here — a transfer aimed at the other side never touches it.
    QList<ComposeApplyCommand::PageInsert> inserts;
    const QList<int>& picks = m_pagePicks[srcIdx];
    std::shared_ptr<IPdfEditorEngine> srcEngine;
    for (int k = 0; k < picks.size(); ++k) {
        const int page = picks.at(k);
        if (!srcEngine) {
            srcEngine = m_engineFactory ? m_engineFactory() : nullptr;
            if (!srcEngine || !srcEngine->loadDocumentForEditing(src.path))
                return refuse(tr("The PDF engine could not open %1.")
                                  .arg(QFileInfo(src.path).fileName()));
        }
        const QByteArray bytes = srcEngine->extractPageAsBytes(src.path, page);
        if (bytes.isEmpty())
            return refuse(tr("Page %1 of %2 could not be read — nothing was written.")
                              .arg(page + 1).arg(QFileInfo(src.path).fileName()));
        const int at = insertionIndexFor(m_insertAfter[destIdx], k);
        const QSizeF sp = src.pageSizes.value(page);
        const int neighbor = dst.pageCount > 0
            ? qBound(0, qMin(at, dst.pageCount - 1), dst.pageCount - 1) : 0;
        const QSizeF tp = dst.pageSizes.value(neighbor);
        inserts.append({ bytes, at, pageSizeDisclosure(sp, tp) });
    }

    // Resolve the picked images' pixels from the source inventory.
    QList<ComposeApplyCommand::ImagePlacement> placements;
    for (const auto& pick : m_imagePicks[srcIdx]) {
        const QList<ComposeImageInfo> inventory = imageInventory(src.path, pick.sourcePage);
        QImage pixels;
        for (const auto& info : inventory) {
            if (info.xobjectName == pick.xobjectName) { pixels = info.pixels; break; }
        }
        if (pixels.isNull())
            return refuse(tr("Image %1 could not be read — nothing was written.")
                              .arg(pick.xobjectName));
        const QRectF box = effectiveImageBox(destination);
        placements.append({ imageTargetPage(destination), pixels,
                            fittedRect(QSizeF(pick.pixelSize), box), 1.0, QByteArray() });
    }

    if (inserts.isEmpty() && placements.isEmpty())
        return refuse(tr("Nothing is picked for this side."));

    const QString label = tr("Compose %1 page(s) and %2 image(s) into %3")
                              .arg(inserts.size()).arg(placements.size())
                              .arg(QFileInfo(dst.path).fileName());
    auto* cmd = new ComposeApplyCommand(m_engineFactory, dst.path, inserts, placements,
                                        dst.pageCount, label);
    // Park this pane's renderer for the commit — the destination file is
    // replaced atomically and pdfium's loader holds it open. The pane is
    // reloaded from the committed bytes right after (applyTransfers).
    m_panes[destIdx].renderer.reset();
    const int historyBefore = m_history[destIdx].count();
    // QUndoStack::push runs the command's redo(); a command whose first
    // application failed is marked obsolete and NOT kept — the stack count
    // is the dangling-free failure probe.
    m_history[destIdx].push(cmd);
    if (m_history[destIdx].count() != historyBefore + 1) {
        QString reason = tr("The transfer into %1 was refused — the document is "
                            "unchanged and the transfer stays pending.")
                             .arg(QFileInfo(dst.path).fileName());
        if (!cmd->lastError().isEmpty())
            reason += QLatin1Char('\n') + cmd->lastError();
        return refuse(reason);
    }
    outPages = inserts.size();
    outImages = placements.size();
    return true;
}

void ComposeMode::reloadPaneIfLoaded(ComposeSide side)
{
    Pane& p = pane(side);
    if (p.path.isEmpty()) return;
    loadPane(side, p.path);
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
        // Keep the r4-ux pick-state disclosure in step with the checkbox.
        item->setData(Qt::AccessibleTextRole,
                      tr("%1 document %2, page %3 of %4, %5")
                          .arg(m_panes[idx].role)
                          .arg(QFileInfo(m_panes[idx].path).fileName())
                          .arg(page + 1).arg(m_panes[idx].pageCount)
                          .arg(item->checkState() == Qt::Checked
                                   ? tr("picked") : tr("not picked")));
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

// ── ComposeApplyCommand — ONE checked-history step per destination ─────────

ComposeApplyCommand::ComposeApplyCommand(EngineFactory factory, const QString& destinationPath,
                                         QList<PageInsert> inserts,
                                         QList<ImagePlacement> placements,
                                         int beforePageCount, const QString& label)
    : m_factory(std::move(factory))
    , m_destination(destinationPath)
    , m_inserts(std::move(inserts))
    , m_placements(std::move(placements))
    , m_beforePageCount(beforePageCount)
{
    setText(label);
}

bool ComposeApplyCommand::copyFile(const QString& from, const QString& to, QString* err)
{
    QFile src(from);
    if (!src.open(QIODevice::ReadOnly)) {
        if (err) *err = QObject::tr("Could not read %1: %2").arg(from, src.errorString());
        return false;
    }
    QFile dst(to);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QObject::tr("Could not stage %1: %2").arg(to, dst.errorString());
        return false;
    }
    char buffer[1 << 20];
    while (!src.atEnd()) {
        const qint64 n = src.read(buffer, sizeof buffer);
        if (n < 0 || dst.write(buffer, n) != n) {
            if (err) *err = QObject::tr("Could not stage the working copy of %1: %2")
                                .arg(to, dst.errorString());
            return false;
        }
    }
    return true;
}

bool ComposeApplyCommand::runTransaction(EngineFactory& factory, const QString& destination,
                                         const QList<PageInsert>& inserts,
                                         QList<ImagePlacement>& placements,
                                         int expectedPageCountAfter, QString* err)
{
    auto refuse = [err](const QString& message) {
        if (err) *err = message;
        return false;
    };
    if (!QFileInfo::exists(destination))
        return refuse(QObject::tr("The destination document %1 does not exist.").arg(destination));

    // E-6 destination-identity precondition: captured at operation start,
    // re-checked immediately before the atomic replace.
    const SafeSave::DestinationIdentity identity =
        SafeSave::captureDestinationIdentity(destination);

    // R01 transaction shape: serialize the COMPLETE mutation to a unique
    // candidate (never the destination), validate, then commit.
    QString candErr;
    QString candidate;
    if (!SafeSave::makeUniqueCandidate(&candidate, &candErr))
        return refuse(candErr);
    struct CandidateRemover {
        QString path;
        ~CandidateRemover() { if (!path.isEmpty()) QFile::remove(path); }
    } remover{candidate};

    if (!copyFile(destination, candidate, err))
        return false;

    // Operation-owned destination engine (the SplitEngineFactory idiom): a
    // fresh PdfEditorEngine whose resident document is the candidate only.
    std::shared_ptr<IPdfEditorEngine> engine = factory ? factory() : nullptr;
    if (!engine || !engine->loadDocumentForEditing(candidate))
        return refuse(QObject::tr("The PDF engine could not open the working copy."));

    for (int i = 0; i < inserts.size(); ++i) {
        if (!engine->insertPageFromBytes(candidate, inserts.at(i).atIndex,
                                         inserts.at(i).onePagePdf))
            return refuse(QObject::tr("Inserting a page at position %1 failed — "
                                      "the document is unchanged.")
                              .arg(inserts.at(i).atIndex + 1));
    }

    for (int i = 0; i < placements.size(); ++i) {
        auto& place = placements[i];
        // Pre-draw page backup (the undo restoration source) — captured from
        // the candidate state after the inserts, before any draw.
        if (place.pageBackup.isEmpty()) {
            place.pageBackup = engine->extractPageAsBytes(candidate, place.pageIndex);
            if (place.pageBackup.isEmpty())
                return refuse(QObject::tr("Page %1 could not be read for the image "
                                          "placement backup — the document is unchanged.")
                                  .arg(place.pageIndex + 1));
        }
        if (!engine->placeImageOnPage(candidate, place.pageIndex, place.image,
                                      place.rect, place.opacity))
            return refuse(QObject::tr("Placing an image on page %1 failed — "
                                      "the document is unchanged.")
                              .arg(place.pageIndex + 1));
    }

    // Validate the candidate through an INDEPENDENT backend (fresh renderer):
    // the composed page count must be exact and every mutated page must
    // really decode — a candidate that cannot be re-opened is never committed.
    {
        auto renderer = BackendRouter::rendererFor(candidate);
        if (!renderer)
            return refuse(QObject::tr("The composed candidate could not be re-opened — "
                                      "nothing was committed."));
        if (probePageCount(*renderer) != expectedPageCountAfter)
            return refuse(QObject::tr("The composed candidate has an unexpected page "
                                      "count — nothing was committed."));
        for (const auto& place : placements) {
            if (renderer->renderPage(place.pageIndex, 36).isNull())
                return refuse(QObject::tr("Composed page %1 failed to render — "
                                          "nothing was committed.")
                                  .arg(place.pageIndex + 1));
        }
    }
    // Release every handle that pins the candidate BEFORE the atomic replace
    // (the operation engine's lazy-parse device and the validation renderer
    // both keep an OS handle on Windows — the same GUI-held-handle discipline
    // SafeSave's coordinator enforces for viewers).
    engine.reset();

    // Bounded copy + checked atomic commit, guarded by the captured identity.
    QString commitErr;
    if (!SafeSave::commitFileToDestination(candidate, destination, &commitErr,
                                           SafeSave::CommitFaultForTesting::None,
                                           identity)) {
        return refuse(commitErr.isEmpty()
                          ? QObject::tr("The validated composition could not replace %1 "
                                        "— the document is unchanged.").arg(destination)
                          : commitErr);
    }
    remover.path.clear();   // committed: the candidate is now the destination's twin
    return true;
}

bool ComposeApplyCommand::runRestore(QString* err)
{
    auto refuse = [err](const QString& message) {
        if (err) *err = message;
        return false;
    };
    if (!QFileInfo::exists(m_destination))
        return refuse(QObject::tr("The destination document %1 does not exist.").arg(m_destination));
    const SafeSave::DestinationIdentity identity =
        SafeSave::captureDestinationIdentity(m_destination);

    QString candErr;
    QString candidate;
    if (!SafeSave::makeUniqueCandidate(&candidate, &candErr))
        return refuse(candErr);
    struct CandidateRemover {
        QString path;
        ~CandidateRemover() { if (!path.isEmpty()) QFile::remove(path); }
    } remover{candidate};

    if (!copyFile(m_destination, candidate, err))
        return false;

    std::shared_ptr<IPdfEditorEngine> engine = m_factory ? m_factory() : nullptr;
    if (!engine || !engine->loadDocumentForEditing(candidate))
        return refuse(QObject::tr("The PDF engine could not open the working copy."));

    // Exact reverse of the apply: the placed-image pages are restored from
    // their pre-draw backups FIRST (post-apply indices are still valid), the
    // inserted pages are then deleted in descending order.
    for (int i = m_placements.size() - 1; i >= 0; --i) {
        if (!engine->restorePageFromBytes(candidate, m_placements.at(i).pageIndex,
                                          m_placements.at(i).pageBackup))
            return refuse(QObject::tr("Restoring page %1 failed — the undo was not "
                                      "applied and can be retried.")
                              .arg(m_placements.at(i).pageIndex + 1));
    }
    for (int i = m_inserts.size() - 1; i >= 0; --i) {
        if (!engine->deletePage(candidate, m_inserts.at(i).atIndex))
            return refuse(QObject::tr("Removing the inserted page at position %1 failed — "
                                      "the undo was not applied and can be retried.")
                              .arg(m_inserts.at(i).atIndex + 1));
    }

    {
        auto renderer = BackendRouter::rendererFor(candidate);
        if (!renderer)
            return refuse(QObject::tr("The restored candidate could not be re-opened — "
                                      "nothing was committed."));
        if (probePageCount(*renderer) != m_beforePageCount)
            return refuse(QObject::tr("The restored candidate has an unexpected page "
                                      "count — nothing was committed."));
    }
    engine.reset();   // release the candidate's lazy-parse device before the replace

    QString commitErr;
    if (!SafeSave::commitFileToDestination(candidate, m_destination, &commitErr,
                                           SafeSave::CommitFaultForTesting::None,
                                           identity)) {
        return refuse(commitErr.isEmpty()
                          ? QObject::tr("The validated undo could not replace %1 — "
                                        "the document is unchanged.").arg(m_destination)
                          : commitErr);
    }
    remover.path.clear();
    return true;
}

bool ComposeApplyCommand::applyChecked()
{
    // WP-R03: the mutation is applied while the history index is untouched.
    QString err;
    if (!runTransaction(m_factory, m_destination, m_inserts, m_placements,
                        m_beforePageCount + m_inserts.size(), &err)) {
        m_lastError = err;
        return false;   // retryable — the history position did not move
    }
    armCheckedApply();
    return true;
}

void ComposeApplyCommand::redo()
{
    if (consumeArmedApply())
        return;   // checked traversal already applied; index-move only
    QString err;
    if (!runTransaction(m_factory, m_destination, m_inserts, m_placements,
                        m_beforePageCount + m_inserts.size(), &err)) {
        // A failed mutation must not become an undoable step.
        setObsolete(true);
        m_lastError = err;
        return;
    }
    m_lastError.clear();
    setObsolete(false);
}

bool ComposeApplyCommand::restoreChecked()
{
    // G08: the restoration runs while the index is untouched; a failure stays
    // retryable at the same history position.
    QString err;
    if (!runRestore(&err)) {
        m_lastError = err;
        return false;
    }
    armCheckedRestore();
    return true;
}

void ComposeApplyCommand::undo()
{
    if (consumeArmedRestore())
        return;   // the checked traversal already restored; index-move only
    QString err;
    if (!runRestore(&err))
        m_lastError = err;
}

} // namespace gp
