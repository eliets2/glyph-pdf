#include "ui/BookmarkPanel.h"
#include "ui/PdfViewerWidget.h"

#include <QTreeView>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QPdfBookmarkModel>

BookmarkPanel::BookmarkPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("bookmarkPanel");

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter bookmarks..."));
    m_filter->setClearButtonEnabled(true);
    m_filter->setAccessibleName(tr("Filter bookmarks"));

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName("bookmarkCount");
    m_countLabel->setAccessibleName(tr("Bookmark count"));

    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setProperty("role", "sidebarList");
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setAccessibleName(tr("Document bookmarks"));

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setRecursiveFilteringEnabled(true);
    m_tree->setModel(m_proxy);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_filter);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_countLabel);

    connect(m_filter, &QLineEdit::textChanged, this, &BookmarkPanel::onFilterChanged);
    connect(m_tree, &QTreeView::clicked, this, &BookmarkPanel::onItemClicked);
    connect(m_tree, &QTreeView::doubleClicked, this, &BookmarkPanel::onItemClicked);
}

void BookmarkPanel::setViewer(PdfViewerWidget *viewer)
{
    m_viewer = viewer;
    if (!viewer) return;

    auto *bm = viewer->bookmarkModel();
    m_proxy->setSourceModel(bm);
    m_tree->expandAll();

    connect(bm, &QAbstractItemModel::modelReset, this, &BookmarkPanel::updateBookmarkCount);
    connect(bm, &QAbstractItemModel::rowsInserted, this, &BookmarkPanel::updateBookmarkCount);
    updateBookmarkCount();
}

void BookmarkPanel::onItemClicked(const QModelIndex &proxyIndex)
{
    if (!m_viewer || !proxyIndex.isValid()) return;

    QModelIndex srcIndex = m_proxy->mapToSource(proxyIndex);
    int page = srcIndex.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt();
    if (page >= 0) {
        m_viewer->goToPage(page);
        emit navigateToPage(page);
    }
}

void BookmarkPanel::onFilterChanged(const QString &text)
{
    m_proxy->setFilterFixedString(text);
    if (!text.isEmpty())
        m_tree->expandAll();
}

void BookmarkPanel::updateBookmarkCount()
{
    if (!m_proxy) return;

    // Count all rows recursively
    std::function<int(const QModelIndex&)> countRows = [&](const QModelIndex &parent) -> int {
        int count = m_proxy->rowCount(parent);
        int total = count;
        for (int i = 0; i < count; ++i) {
            total += countRows(m_proxy->index(i, 0, parent));
        }
        return total;
    };

    int total = countRows(QModelIndex());
    m_countLabel->setText(total > 0
        ? tr("%1 bookmark(s)").arg(total)
        : tr("No bookmarks"));
}
