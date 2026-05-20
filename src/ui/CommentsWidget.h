#ifndef COMMENTSWIDGET_H
#define COMMENTSWIDGET_H

#include <QWidget>
#include <QDateTime>
#include "core/AnnotationTypes.h"

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QComboBox;
QT_END_NAMESPACE

class PdfViewerWidget;

class CommentsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommentsWidget(QWidget *parent = nullptr);
    void setViewer(PdfViewerWidget *viewer);
    void setDocumentFile(const QString &filePath);
    void setCurrentPage(int page);

public slots:
    void reloadAnnotations();

signals:
    void commentDoubleClicked(int page);

private slots:
    void addComment();
    void replyToComment();
    void changeReviewState();

private:
    void refreshList();
    void buildTree(const QList<AnnotationItem> &items);

    PdfViewerWidget *m_viewer = nullptr;
    QString m_filePath;
    int m_currentPage = 1;

    QTreeWidget *m_tree = nullptr;
    QComboBox *m_filterStatus = nullptr;
    QComboBox *m_filterAuthor = nullptr;
    QLineEdit *m_author = nullptr;
    QTextEdit *m_editor = nullptr;
    QPushButton *m_addBtn = nullptr;
};

#endif // COMMENTSWIDGET_H

