// SPDX-License-Identifier: Apache-2.0
#include "ui/WelcomeWidget.h"
#include "util/Icons.h"
#include "util/GpTheme.h"

#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

// =============================================================================
// Metrics (U01)
// =============================================================================

namespace {
// The centered content column keeps its 600px identity cap; on narrower
// windows it shrinks with the scroll-area viewport (never a display-size
// assumption) and the card grid reflows 3 -> 2 -> 1 columns inside it.
constexpr int kContainerMaxWidth   = 600;
constexpr int kCardSpacing         = 12;   // grid gutter between action cards
constexpr int kMaxCardColumns      = 3;
constexpr int kRecentMaxVisibleRows = 5;  // more rows scroll inside the list
// Matches the recent-list QSS below: 36px item min-height + 2x8px padding.
constexpr int kRecentRowFallback    = 52;

QString argb(const QColor& c) { return c.name(QColor::HexArgb); }
} // namespace

// =============================================================================
// Helpers
// =============================================================================

static QPushButton* makeActionCard(QWidget* parent, const QString& iconName,
                                   const QString& label)
{
    auto* card = new QPushButton(parent);
    card->setObjectName("actionCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setFocusPolicy(Qt::StrongFocus); // keyboard-reachable actions (Tab + Space)
    card->setMinimumSize(140, 96);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignCenter);

    auto* iconLbl = new QLabel(card);
    iconLbl->setPixmap(gp::Icons::get(iconName, gp::Theme::accent()).pixmap(24, 24));
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(iconLbl);

    auto* textLbl = new QLabel(label, card);
    textLbl->setAlignment(Qt::AlignCenter);
    textLbl->setStyleSheet(QString(
        "font-size: 12px; font-weight: 500; color: %1; "
        "background: transparent; border: none;").arg(gp::Theme::fg0().name()));
    layout->addWidget(textLbl);

    card->setStyleSheet(QString(
        "QPushButton#actionCard {"
        "  background: %1; border: 1px solid %2; border-radius: 6px;"
        "}"
        "QPushButton#actionCard:hover {"
        "  border-color: %3;"
        "}"
        "QPushButton#actionCard:focus {"
        "  border-color: %3;"
        "}")
        .arg(gp::Theme::bg2().name(), gp::Theme::line().name(),
             gp::Theme::accent().name()));

    return card;
}

// =============================================================================
// WelcomeWidget
// =============================================================================

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

int WelcomeWidget::columnsForWidth(int availableWidth, int cardMinWidth, int spacing)
{
    if (availableWidth <= 0 || cardMinWidth <= 0)
        return 1;
    const int fit = (availableWidth + spacing) / (cardMinWidth + spacing);
    return qBound(1, fit, kMaxCardColumns);
}

void WelcomeWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), gp::Theme::bg0());
}

bool WelcomeWidget::eventFilter(QObject* watched, QEvent* event)
{
    // The viewport width is the true available width: it resizes whenever the
    // window resizes AND when a scrollbar appears or disappears. (The content
    // widget's width is unusable here — it is clamped to the content's
    // minimumSizeHint, which the fixed-width container would pin, stalling
    // the reflow when the window narrows.)
    if (watched == m_viewport && event->type() == QEvent::Resize)
        reflowActionCards();
    return QWidget::eventFilter(watched, event);
}

void WelcomeWidget::reflowActionCards()
{
    if (!m_viewport || !m_container || !m_cardsGrid || m_actionCards.isEmpty())
        return;

    // Real per-card minimum: the explicit minimum and the widget's own
    // minimumSizeHint, whichever is larger.
    int cardMinWidth = 0;
    for (const auto* card : m_actionCards) {
        cardMinWidth = qMax(cardMinWidth, qMax(card->minimumWidth(),
                                               card->minimumSizeHint().width()));
    }

    const int available = qMin(m_viewport->width(), kContainerMaxWidth);
    if (available <= 0)
        return;

    m_container->setFixedWidth(available);

    const int columns = columnsForWidth(available, cardMinWidth, kCardSpacing);
    if (columns == m_columns)
        return;
    m_columns = columns;

    for (int i = 0; i < m_actionCards.size(); ++i) {
        m_cardsGrid->removeWidget(m_actionCards.at(i));
        m_cardsGrid->addWidget(m_actionCards.at(i), i / columns, i % columns);
    }
}

void WelcomeWidget::setupUi()
{
    setObjectName("welcomeWidget");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }")
                              .arg(gp::Theme::bg0().name()));

    m_content = new QWidget(scroll);
    m_content->setObjectName("welcomeContent");
    m_content->setStyleSheet(QString("background: %1;").arg(gp::Theme::bg0().name()));
    scroll->setWidget(m_content);

    m_viewport = scroll->viewport();
    m_viewport->installEventFilter(this);

    // The single intentional vertical stretch lives here, outside the content
    // block; the content block's own height is driven by its children.
    root->addWidget(scroll, 1);

    auto* layout = new QVBoxLayout(m_content);
    layout->setContentsMargins(0, 0, 0, 24);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    // -- Container (centered column, capped at kContainerMaxWidth) --
    m_container = new QWidget(m_content);
    m_container->setObjectName("welcomeContainer");
    m_container->setMaximumWidth(kContainerMaxWidth);
    m_container->setStyleSheet("background: transparent;");
    auto* innerLayout = new QVBoxLayout(m_container);
    innerLayout->setContentsMargins(0, 40, 0, 0);
    innerLayout->setSpacing(0);
    innerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // -- Logo --
    auto* logoRow = new QHBoxLayout();
    logoRow->setAlignment(Qt::AlignCenter);
    logoRow->setSpacing(12);

    auto* glyphIcon = new QLabel(m_container);
    glyphIcon->setText(QString::fromUtf8("\xe2\x97\x86")); // diamond glyph
    glyphIcon->setStyleSheet(QString(
        "font-size: 48px; color: %1; background: transparent; border: none;").arg(gp::Theme::accent().name()));
    logoRow->addWidget(glyphIcon);

    auto* appTitle = new QLabel("GLYPH\xC2\xB7PDF", m_container);
    appTitle->setStyleSheet(QString(
        "font-size: 24px; font-weight: 700; color: %1; background: transparent; border: none; "
        "font-family: 'Manrope','Segoe UI',sans-serif; letter-spacing: 2px;").arg(gp::Theme::fg0().name()));
    logoRow->addWidget(appTitle);

    innerLayout->addLayout(logoRow);
    innerLayout->addSpacing(8);

    // -- Subtitle --
    auto* subtitle = new QLabel(tr("Professional PDF Workstation"), m_container);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QString(
        "font-size: 14px; color: %1; background: transparent; border: none;").arg(gp::Theme::fg1().name()));
    innerLayout->addWidget(subtitle);
    innerLayout->addSpacing(28);

    // -- Action cards: responsive grid (U01) --
    // Positions are (re)assigned by reflowActionCards(): three columns when
    // space permits, two for a narrower column, one for very narrow.
    m_cardsGrid = new QGridLayout();
    m_cardsGrid->setContentsMargins(0, 0, 0, 0);
    m_cardsGrid->setHorizontalSpacing(kCardSpacing);
    m_cardsGrid->setVerticalSpacing(kCardSpacing);

    // "Open PDF" first (primary action, first in creation/tab order).
    auto* openCard    = makeActionCard(m_container, "folder-open",   tr("Open PDF"));
    auto* mergeCard   = makeActionCard(m_container, "merge",         tr("Merge files"));
    auto* convertCard = makeActionCard(m_container, "file-code",     tr("Convert"));
    auto* protectCard = makeActionCard(m_container, "shield-check",  tr("Protect"));
    // U01 icon audit: "file-plus" has no asset in resources.qrc, so
    // Icons::svg() returned empty and the factory painted a fallback blob.
    // Map the card onto the existing registered "to-p-d-f" document glyph
    // instead of inventing a new asset.
    auto* importCard  = makeActionCard(m_container, "to-p-d-f",      tr("Import Office"));
    auto* imgsCard    = makeActionCard(m_container, "image",         tr("Images to PDF"));

    openCard->setAccessibleName(tr("Open PDF file"));
    openCard->setAccessibleDescription(tr("Browse and open an existing PDF document"));
    mergeCard->setAccessibleName(tr("Merge PDF files"));
    mergeCard->setAccessibleDescription(tr("Combine multiple PDF files into one document"));
    convertCard->setAccessibleName(tr("Convert documents"));
    convertCard->setAccessibleDescription(tr("Convert between PDF and other file formats"));
    protectCard->setAccessibleName(tr("Protect PDF"));
    protectCard->setAccessibleDescription(tr("Add passwords, encryption, or digital signatures"));
    importCard->setAccessibleName(tr("Import Office document"));
    importCard->setAccessibleDescription(tr("Convert a Word, Excel or PowerPoint file to PDF via LibreOffice"));
    imgsCard->setAccessibleName(tr("Images to PDF"));
    imgsCard->setAccessibleDescription(tr("Combine PNG, JPEG or TIFF images into a single PDF"));
    // §9.16 P0: the privacy badge at the exact moment users compare us to
    // upload-based converters — both conversions run on this machine.
    const QString localNotice = tr("Processed 100% locally — no internet, no upload.");
    importCard->setToolTip(localNotice);
    imgsCard->setToolTip(localNotice);

    connect(openCard,    &QPushButton::clicked, this, &WelcomeWidget::openFileRequested);
    connect(mergeCard,   &QPushButton::clicked, this, &WelcomeWidget::mergeFilesRequested);
    connect(convertCard, &QPushButton::clicked, this, &WelcomeWidget::convertRequested);
    connect(protectCard, &QPushButton::clicked, this, &WelcomeWidget::protectRequested);
    connect(importCard,  &QPushButton::clicked, this, &WelcomeWidget::importOfficeRequested);
    connect(imgsCard,    &QPushButton::clicked, this, &WelcomeWidget::imagesToPdfRequested);

    m_actionCards = {openCard, mergeCard, convertCard, protectCard, importCard, imgsCard};

    innerLayout->addLayout(m_cardsGrid);
    innerLayout->addSpacing(28);

    // -- Recent files section (directly beneath the actions, U01) --
    auto* recentHeader = new QLabel(tr("Recent files"), m_container);
    recentHeader->setObjectName("recentFilesHeader");
    recentHeader->setStyleSheet(QString(
        "font-size: 12px; font-weight: 600; color: %1; letter-spacing: 0.4px; "
        "background: transparent; border: none;").arg(gp::Theme::fg2().name()));
    innerLayout->addWidget(recentHeader);
    innerLayout->addSpacing(8);

    m_recentEmpty = new QLabel(tr("No recent files"), m_container);
    m_recentEmpty->setObjectName("recentFilesEmpty");
    m_recentEmpty->setStyleSheet(QString(
        "font-size: 13px; color: %1; background: transparent; border: none; "
        "padding: 8px 12px;").arg(gp::Theme::fg2().name()));
    m_recentEmpty->hide();
    innerLayout->addWidget(m_recentEmpty);

    m_recentList = new QListWidget(m_container);
    m_recentList->setObjectName("recentFilesList");
    m_recentList->setFrameShape(QFrame::NoFrame);
    m_recentList->setCursor(Qt::PointingHandCursor);
    m_recentList->setUniformItemSizes(true);
    m_recentList->setStyleSheet(QString(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item {"
        "  padding: 8px 12px; border-bottom: 1px solid %1; min-height: 36px;"
        "}"
        "QListWidget::item:hover {"
        "  background: %2;"
        "}"
        "QListWidget::item:selected {"
        "  background: %3; border-left: 2px solid %4;"
        "}")
        .arg(gp::Theme::line().name(),
             argb(gp::Theme::accentDim()),
             argb(gp::Theme::accent()),
             gp::Theme::accent().name()));

    connect(m_recentList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            bool exists = QFileInfo::exists(path);
            onRecentItemClicked(path, exists);
        }
    });

    innerLayout->addWidget(m_recentList);

    layout->addWidget(m_container, 0, Qt::AlignHCenter);

    refreshRecentList();
    reflowActionCards(); // initial column count; real resize events refine it
}

// =============================================================================
// Public API
// =============================================================================

void WelcomeWidget::setRecentFiles(const QStringList& files)
{
    m_recentFiles = files;
    refreshRecentList();
}

// =============================================================================
// Private
// =============================================================================

void WelcomeWidget::refreshRecentList()
{
    if (!m_recentList) return;
    m_recentList->clear();

    if (m_recentFiles.isEmpty()) {
        // Empty state is a compact label: the page's vertical size stays
        // driven by its children instead of a fixed-height empty list.
        if (m_recentEmpty) m_recentEmpty->show();
        m_recentList->hide();
        return;
    }
    if (m_recentEmpty) m_recentEmpty->hide();
    m_recentList->show();

    QIcon fileIcon = gp::Icons::get("file-text", gp::Theme::accent());
    for (const QString& path : m_recentFiles) {
        QFileInfo fi(path);
        const bool exists = fi.exists();
        const QString name   = displayName(path);
        const QString folder = exists ? fi.absolutePath() : tr("(file not found)");

        // Truncate long paths
        QString displayPath = folder;
        if (displayPath.length() > 60)
            displayPath = "..." + displayPath.right(57);

        auto* item = new QListWidgetItem(m_recentList);
        item->setData(Qt::UserRole, path);
        item->setIcon(fileIcon);
        item->setText(name + "\n" + displayPath);
        if (!exists) item->setForeground(gp::Theme::fg2());
    }

    // U01: the list takes exactly the height of its (capped) visible rows, so
    // the content block's vertical size stays driven by its children. Extra
    // rows scroll inside the list.
    m_recentList->ensurePolished();
    int rowHeight = m_recentList->sizeHintForRow(0);
    if (rowHeight <= 0)
        rowHeight = kRecentRowFallback;
    rowHeight = qMax(rowHeight, kRecentRowFallback);
    const int visibleRows = qMin(m_recentFiles.size(), kRecentMaxVisibleRows);
    m_recentList->setFixedHeight(visibleRows * rowHeight);
}

QString WelcomeWidget::displayName(const QString& path) const
{
    QFileInfo fi(path);
    return fi.fileName().isEmpty() ? path : fi.fileName();
}

void WelcomeWidget::onRecentItemClicked(const QString& path, bool exists)
{
    if (exists) {
        emit recentFileRequested(path);
        return;
    }

    // File is missing -- prompt to remove from recents
    auto result = QMessageBox::question(
        this,
        tr("File Not Found"),
        tr("The file \"%1\" no longer exists.\n\nRemove it from the recent files list?")
            .arg(QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (result == QMessageBox::Yes) {
        // Remove from internal list and refresh
        m_recentFiles.removeAll(path);
        refreshRecentList();

        // Also remove from QSettings
        QSettings settings;
        QStringList recent = settings.value("recentFiles").toStringList();
        recent.removeAll(path);
        settings.setValue("recentFiles", recent);

        emit removeRecentFileRequested(path);
    }
}
