#include "ThumbnailSidebar.h"
#include "PdfViewerWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <QSpacerItem>
#include <QStyle>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ThumbnailSidebar::ThumbnailSidebar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("thumbnailSidebar");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar: "PAGES . N" + zoom buttons
    auto* toolbar = new QWidget;
    toolbar->setObjectName("thumbToolbar");
    toolbar->setFixedHeight(24);

    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 0, 8, 0);
    tbLayout->setSpacing(4);

    m_pageCountLabel = new QLabel(QStringLiteral("PAGES \xC2\xB7 0"));
    m_pageCountLabel->setObjectName("thumbPageCountLabel");
    tbLayout->addWidget(m_pageCountLabel);
    tbLayout->addStretch();

    auto* zoomOutBtn = new QPushButton(QStringLiteral("\xE2\x88\x92")); // minus sign
    zoomOutBtn->setFixedSize(20, 18);
    zoomOutBtn->setProperty("variant", "mini");
    zoomOutBtn->setToolTip(QStringLiteral("Smaller thumbnails"));
    tbLayout->addWidget(zoomOutBtn);

    auto* zoomInBtn = new QPushButton(QStringLiteral("+"));
    zoomInBtn->setFixedSize(20, 18);
    zoomInBtn->setProperty("variant", "mini");
    zoomInBtn->setToolTip(QStringLiteral("Larger thumbnails"));
    tbLayout->addWidget(zoomInBtn);

    mainLayout->addWidget(toolbar);

    // Scroll area
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("thumbScrollArea");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget;
    m_container->setObjectName("thumbContainer");
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(12, 8, 12, 8);
    m_layout->setSpacing(8);

    // Spacers for virtualization (Fix 4)
    m_topSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_bottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->addSpacerItem(m_topSpacer);
    m_layout->addSpacerItem(m_bottomSpacer);

    m_scroll->setWidget(m_container);
    mainLayout->addWidget(m_scroll, 1);

    // Re-virtualize on scroll (Fix 4)
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ThumbnailSidebar::updateVisibleThumbnails);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ThumbnailSidebar::setViewer(PdfViewerWidget* viewer)
{
    m_viewer = viewer;
    rebuild();
}

void ThumbnailSidebar::rebuild()
{
    // Clear all live widgets
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        m_layout->removeWidget(it.value());
        it.value()->deleteLater();
    }
    m_liveWidgets.clear();

    if (!m_viewer) {
        m_totalPages = 0;
        m_pageCountLabel->setText(QStringLiteral("PAGES \xC2\xB7 0"));
        m_topSpacer->changeSize(0, 0);
        m_bottomSpacer->changeSize(0, 0);
        m_layout->invalidate();
        return;
    }

    m_totalPages = m_viewer->pageCount();
    m_pageCountLabel->setText(QStringLiteral("PAGES \xC2\xB7 %1").arg(m_totalPages));

    // Set total virtual height so the scrollbar is correct
    int totalHeight = m_totalPages * ThumbItemHeight;
    m_topSpacer->changeSize(0, 0);
    m_bottomSpacer->changeSize(0, totalHeight);
    m_layout->invalidate();

    // Force an initial population
    QMetaObject::invokeMethod(this, &ThumbnailSidebar::updateVisibleThumbnails, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Virtualization engine (Fix 4)
// ---------------------------------------------------------------------------

void ThumbnailSidebar::updateVisibleThumbnails()
{
    if (m_totalPages == 0) return;

    int scrollY = m_scroll->verticalScrollBar()->value();
    int viewportH = m_scroll->viewport()->height();

    // Determine which page indices are visible
    int firstVisible = qMax(0, scrollY / ThumbItemHeight - VisibleBuffer);
    int lastVisible  = qMin(m_totalPages - 1, (scrollY + viewportH) / ThumbItemHeight + VisibleBuffer);

    // Remove widgets outside the visible range
    QList<int> toRemove;
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        if (it.key() < firstVisible || it.key() > lastVisible)
            toRemove.append(it.key());
    }
    for (int idx : toRemove) {
        QWidget *w = m_liveWidgets.take(idx);
        m_layout->removeWidget(w);
        w->deleteLater();
    }

    // Create widgets for newly visible pages
    for (int i = firstVisible; i <= lastVisible; ++i) {
        if (m_liveWidgets.contains(i))
            continue;
        QWidget *w = createThumbWidget(i);
        m_liveWidgets.insert(i, w);
    }

    // Re-order: remove all widgets from layout, set spacers, re-add in order
    // First remove everything except spacers
    for (auto it = m_liveWidgets.begin(); it != m_liveWidgets.end(); ++it) {
        m_layout->removeWidget(it.value());
    }
    // Remove spacers from layout so we can re-add in order
    m_layout->removeItem(m_topSpacer);
    m_layout->removeItem(m_bottomSpacer);

    // Top spacer covers pages [0, firstVisible)
    int topH = firstVisible * ThumbItemHeight;
    m_topSpacer->changeSize(0, topH);
    m_layout->addSpacerItem(m_topSpacer);

    // Add widgets in page order
    QList<int> sortedKeys = m_liveWidgets.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (int key : sortedKeys) {
        m_layout->addWidget(m_liveWidgets[key]);
    }

    // Bottom spacer covers pages (lastVisible, totalPages)
    int bottomH = qMax(0, (m_totalPages - lastVisible - 1) * ThumbItemHeight);
    m_bottomSpacer->changeSize(0, bottomH);
    m_layout->addSpacerItem(m_bottomSpacer);

    m_layout->invalidate();
}

QWidget* ThumbnailSidebar::createThumbWidget(int pageIndex)
{
    bool isCurrent = (pageIndex == m_currentPage);

    auto* thumbWidget = new QWidget;
    thumbWidget->setObjectName("thumbItem");
    thumbWidget->setCursor(Qt::PointingHandCursor);
    thumbWidget->setProperty("pageIndex", pageIndex);
    thumbWidget->setProperty("current", isCurrent);
    thumbWidget->installEventFilter(this);

    auto* thumbLayout = new QVBoxLayout(thumbWidget);
    thumbLayout->setContentsMargins(4, 4, 4, 4);
    thumbLayout->setSpacing(4);
    thumbLayout->setAlignment(Qt::AlignHCenter);

    // Page number label
    auto* numLabel = new QLabel(QStringLiteral("%1").arg(pageIndex + 1, 3, 10, QChar('0')));
    numLabel->setObjectName("thumbNumLabel");
    numLabel->setAlignment(Qt::AlignCenter);
    numLabel->setProperty("current", isCurrent);
    thumbLayout->addWidget(numLabel);

    // Frame (dark border container)
    auto* frame = new QWidget;
    frame->setObjectName("thumbFrame");
    frame->setProperty("current", isCurrent);

    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(6, 6, 6, 6);
    frameLayout->setSpacing(0);

    // Paper (8.5:11 aspect ratio placeholder)
    auto* paper = new QWidget;
    paper->setObjectName("thumbPaper");
    paper->setFixedSize(140, 181);

    // Fake content blocks on paper
    auto* paperLayout = new QVBoxLayout(paper);
    paperLayout->setContentsMargins(10, 12, 10, 12);
    paperLayout->setSpacing(6);

    // Title block
    auto* titleBlock = new QWidget;
    titleBlock->setFixedHeight(8);
    titleBlock->setObjectName("thumbPaperTitle");
    paperLayout->addWidget(titleBlock);

    // Text lines
    for (int line = 0; line < 5; ++line) {
        auto* textBlock = new QWidget;
        textBlock->setFixedHeight(4);
        textBlock->setObjectName("thumbPaperText");
        int widthPercent = (line == 4) ? 60 : 90 + (line % 2) * 5;
        textBlock->setMaximumWidth(paper->width() * widthPercent / 100);
        paperLayout->addWidget(textBlock);
    }

    paperLayout->addStretch();

    // Another block (simulating image or table)
    auto* imgBlock = new QWidget;
    imgBlock->setFixedHeight(28);
    imgBlock->setObjectName("thumbPaperImg");
    paperLayout->addWidget(imgBlock);

    // More text lines
    for (int line = 0; line < 3; ++line) {
        auto* textBlock = new QWidget;
        textBlock->setFixedHeight(4);
        textBlock->setObjectName("thumbPaperText");
        paperLayout->addWidget(textBlock);
    }

    paperLayout->addStretch();

    frameLayout->addWidget(paper, 0, Qt::AlignCenter);
    thumbLayout->addWidget(frame);

    // Page label text below
    auto* labelText = new QLabel(QStringLiteral("Page %1").arg(pageIndex + 1));
    labelText->setObjectName("thumbLabelText");
    labelText->setAlignment(Qt::AlignCenter);
    thumbLayout->addWidget(labelText);

    return thumbWidget;
}

void ThumbnailSidebar::setCurrentPage(int page)
{
    if (page == m_currentPage) return;

    int oldPage = m_currentPage;
    m_currentPage = page;

    // Update old thumb styling if it is live
    if (m_liveWidgets.contains(oldPage)) {
        auto* oldWidget = m_liveWidgets[oldPage];
        oldWidget->setProperty("current", false);
        oldWidget->style()->unpolish(oldWidget);
        oldWidget->style()->polish(oldWidget);

        auto* numLabel = oldWidget->findChild<QLabel*>("thumbNumLabel");
        if (numLabel) {
            numLabel->setProperty("current", false);
            numLabel->style()->unpolish(numLabel);
            numLabel->style()->polish(numLabel);
        }

        auto* frame = oldWidget->findChild<QWidget*>("thumbFrame");
        if (frame) {
            frame->setProperty("current", false);
            frame->style()->unpolish(frame);
            frame->style()->polish(frame);
        }
    }

    // Update new thumb styling if it is live
    if (m_liveWidgets.contains(page)) {
        auto* newWidget = m_liveWidgets[page];
        newWidget->setProperty("current", true);
        newWidget->style()->unpolish(newWidget);
        newWidget->style()->polish(newWidget);

        auto* numLabel = newWidget->findChild<QLabel*>("thumbNumLabel");
        if (numLabel) {
            numLabel->setProperty("current", true);
            numLabel->style()->unpolish(numLabel);
            numLabel->style()->polish(numLabel);
        }

        auto* frame = newWidget->findChild<QWidget*>("thumbFrame");
        if (frame) {
            frame->setProperty("current", true);
            frame->style()->unpolish(frame);
            frame->style()->polish(frame);
        }

        // Ensure visible
        m_scroll->ensureWidgetVisible(newWidget, 0, 40);
    } else {
        // Scroll to the page position so virtualization picks it up
        int targetY = page * ThumbItemHeight;
        m_scroll->verticalScrollBar()->setValue(targetY);
    }
}

// ---------------------------------------------------------------------------
// Event filter (click handling)
// ---------------------------------------------------------------------------

bool ThumbnailSidebar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            bool ok = false;
            int pageIndex = widget->property("pageIndex").toInt(&ok);
            if (ok) {
                emit pageClicked(pageIndex);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
