// SPDX-License-Identifier: Apache-2.0
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
struct AppContext;

class CommentsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommentsWidget(QWidget *parent = nullptr);
    void setViewer(PdfViewerWidget *viewer);
    // M6-P5 D3: AppContext gives access to the shared QUndoStack +
    // DocumentSession so review-state changes are saved via EditAnnotationCommand
    // (undoable, marks the document dirty) rather than mutated in place.
    void setContext(const AppContext *ctx);
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
    // M6-P5 D3: apply a new review state to the annotation with id `annoId`
    // and persist via EditAnnotationCommand (falls back to a direct
    // setAnnotations when no undo stack is wired, e.g. in unit harnesses).
    void applyReviewState(const QString &annoId, ReviewState newState);

    PdfViewerWidget *m_viewer = nullptr;
    const AppContext *m_ctx = nullptr;
    QString m_filePath;
    int m_currentPage = 1;

    QTreeWidget *m_tree = nullptr;
    QComboBox *m_filterStatus = nullptr;
    QComboBox *m_filterAuthor = nullptr;
    QComboBox *m_filterDate = nullptr;
    QLineEdit *m_author = nullptr;
    QTextEdit *m_editor = nullptr;
    QPushButton *m_addBtn = nullptr;
};

#endif // COMMENTSWIDGET_H

