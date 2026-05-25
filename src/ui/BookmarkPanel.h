#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QTreeView;
class QLineEdit;
class QLabel;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class QPdfBookmarkModel;
class PdfViewerWidget;

class BookmarkPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BookmarkPanel(QWidget *parent = nullptr);

    void setViewer(PdfViewerWidget *viewer);

signals:
    void navigateToPage(int page);

private slots:
    void onItemClicked(const QModelIndex &proxyIndex);
    void onFilterChanged(const QString &text);
    void updateBookmarkCount();

private:
    PdfViewerWidget *m_viewer = nullptr;
    QTreeView *m_tree = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_countLabel = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
};
