#include "ui/WelcomeWidget.h"
#include "util/Icons.h"

#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPaintEvent>
#include <QSettings>

// =============================================================================
// Color constants (spec palette)
// =============================================================================

namespace WC {
static constexpr const char* BG_0   = "#1a1b1e";
static constexpr const char* BG_1   = "#1e1f22";
static constexpr const char* BG_2   = "#2b2d30";
static constexpr const char* LINE   = "#393b40";
static constexpr const char* FG_0   = "#dfe1e5";
static constexpr const char* FG_1   = "#a8abb0";
static constexpr const char* FG_2   = "#71747a";
static constexpr const char* ACCENT = "#ff8c42";
}

// =============================================================================
// Helpers
// =============================================================================

static QPushButton* makeActionCard(QWidget* parent, const QString& iconName,
                                   const QString& label)
{
    auto* card = new QPushButton(parent);
    card->setObjectName("actionCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedSize(140, 100);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignCenter);

    auto* iconLbl = new QLabel(card);
    iconLbl->setPixmap(gp::Icons::get(iconName, QColor(WC::ACCENT)).pixmap(24, 24));
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(iconLbl);

    auto* textLbl = new QLabel(label, card);
    textLbl->setAlignment(Qt::AlignCenter);
    textLbl->setStyleSheet(QString(
        "font-size: 11px; font-weight: 500; color: %1; "
        "background: transparent; border: none;").arg(WC::FG_0));
    layout->addWidget(textLbl);

    card->setStyleSheet(QString(
        "QPushButton#actionCard {"
        "  background: %1; border: 1px solid %2; border-radius: 6px;"
        "}"
        "QPushButton#actionCard:hover {"
        "  border-color: %3;"
        "}").arg(WC::BG_2, WC::LINE, WC::ACCENT));

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

void WelcomeWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(WC::BG_0));
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
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }").arg(WC::BG_0));

    auto* content = new QWidget(scroll);
    content->setObjectName("welcomeContent");
    content->setStyleSheet(QString("background: %1;").arg(WC::BG_0));
    scroll->setWidget(content);
    root->addWidget(scroll);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 40);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    // -- Container (max-width 600px) --
    auto* container = new QWidget(content);
    container->setMaximumWidth(600);
    container->setStyleSheet("background: transparent;");
    auto* innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 60, 0, 0);
    innerLayout->setSpacing(0);
    innerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // -- Logo --
    auto* logoRow = new QHBoxLayout();
    logoRow->setAlignment(Qt::AlignCenter);
    logoRow->setSpacing(12);

    auto* glyphIcon = new QLabel(container);
    glyphIcon->setText(QString::fromUtf8("\xe2\x97\x87")); // diamond glyph
    glyphIcon->setStyleSheet(QString(
        "font-size: 48px; color: %1; background: transparent; border: none;").arg(WC::ACCENT));
    logoRow->addWidget(glyphIcon);

    auto* appTitle = new QLabel("GLYPH\xC2\xB7PDF", container);
    appTitle->setStyleSheet(QString(
        "font-size: 24px; font-weight: 700; color: %1; background: transparent; border: none; "
        "font-family: 'Manrope','Segoe UI',sans-serif; letter-spacing: 2px;").arg(WC::FG_0));
    logoRow->addWidget(appTitle);

    innerLayout->addLayout(logoRow);
    innerLayout->addSpacing(8);

    // -- Subtitle --
    auto* subtitle = new QLabel("Professional PDF Workstation", container);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QString(
        "font-size: 14px; color: %1; background: transparent; border: none;").arg(WC::FG_1));
    innerLayout->addWidget(subtitle);
    innerLayout->addSpacing(36);

    // -- Action cards row --
    auto* cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(12);
    cardsRow->setAlignment(Qt::AlignCenter);

    auto* openCard    = makeActionCard(container, "folder-open",   "Open File");
    auto* mergeCard   = makeActionCard(container, "merge",         "Merge Files");
    auto* convertCard = makeActionCard(container, "file-code",     "Convert");
    auto* protectCard = makeActionCard(container, "shield-check",  "Protect");

    openCard->setAccessibleName(tr("Open PDF file"));
    openCard->setAccessibleDescription(tr("Browse and open an existing PDF document"));
    mergeCard->setAccessibleName(tr("Merge PDF files"));
    mergeCard->setAccessibleDescription(tr("Combine multiple PDF files into one document"));
    convertCard->setAccessibleName(tr("Convert documents"));
    convertCard->setAccessibleDescription(tr("Convert between PDF and other file formats"));
    protectCard->setAccessibleName(tr("Protect PDF"));
    protectCard->setAccessibleDescription(tr("Add passwords, encryption, or digital signatures"));

    connect(openCard,    &QPushButton::clicked, this, &WelcomeWidget::openFileRequested);
    connect(mergeCard,   &QPushButton::clicked, this, [this]() {
        emit mergeRequested();
        emit mergeFilesRequested();
    });
    connect(convertCard, &QPushButton::clicked, this, &WelcomeWidget::convertRequested);
    connect(protectCard, &QPushButton::clicked, this, &WelcomeWidget::protectRequested);

    cardsRow->addWidget(openCard);
    cardsRow->addWidget(mergeCard);
    cardsRow->addWidget(convertCard);
    cardsRow->addWidget(protectCard);

    innerLayout->addLayout(cardsRow);
    innerLayout->addSpacing(40);

    // -- Recent files section --
    auto* recentHeader = new QLabel("RECENT FILES", container);
    recentHeader->setStyleSheet(QString(
        "font-size: 10px; font-weight: 700; color: %1; "
        "font-family: 'Consolas','Courier New',monospace; "
        "letter-spacing: 1.5px; background: transparent; border: none;").arg(WC::FG_2));
    innerLayout->addWidget(recentHeader);
    innerLayout->addSpacing(8);

    auto* recentList = new QListWidget(container);
    recentList->setObjectName("recentFilesList");
    recentList->setMinimumHeight(200);
    recentList->setMaximumHeight(400);
    recentList->setFrameShape(QFrame::NoFrame);
    recentList->setCursor(Qt::PointingHandCursor);
    recentList->setStyleSheet(QString(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item {"
        "  padding: 8px 12px; border-bottom: 1px solid %1; min-height: 36px;"
        "}"
        "QListWidget::item:hover {"
        "  background: rgba(255,140,66,0.08);"
        "}"
        "QListWidget::item:selected {"
        "  background: rgba(255,140,66,0.15); border-left: 2px solid %2;"
        "}").arg(WC::LINE, WC::ACCENT));

    connect(recentList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            bool exists = QFileInfo::exists(path);
            onRecentItemClicked(path, exists);
        }
    });

    innerLayout->addWidget(recentList);

    layout->addWidget(container, 0, Qt::AlignHCenter);

    refreshRecentList();
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
    auto* list = findChild<QListWidget*>("recentFilesList");
    if (!list) return;
    list->clear();

    if (m_recentFiles.isEmpty()) {
        auto* item = new QListWidgetItem("No recent files", list);
        item->setForeground(QColor(WC::FG_2));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        return;
    }

    QIcon fileIcon = gp::Icons::get("file-text", QColor(WC::ACCENT));
    for (const QString& path : m_recentFiles) {
        QFileInfo fi(path);
        const bool exists = fi.exists();
        const QString name   = displayName(path);
        const QString folder = exists ? fi.absolutePath() : "(file not found)";

        // Truncate long paths
        QString displayPath = folder;
        if (displayPath.length() > 60)
            displayPath = "..." + displayPath.right(57);

        auto* item = new QListWidgetItem(list);
        item->setData(Qt::UserRole, path);
        item->setIcon(fileIcon);
        item->setText(name + "\n" + displayPath);
        if (!exists) item->setForeground(QColor(WC::FG_2));
    }
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
