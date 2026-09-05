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
class QLabel;
class QTableWidget;
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

    // ── U07: review-tool affordances over the SAME annotation records ────
    // Number of AddComment records in the document (unfiltered total).
    int totalCommentCount() const { return m_totalComments; }
    // Number of records rendered by the ACTIVE presentation (list or table).
    int visibleCommentCount() const;
    // How many of the status/author/date filters are currently active.
    int activeFilterCount() const;
    // Human summary, e.g. "2 filters · 5 of 12 shown" / "0 filters · 6 of 6 shown".
    QString activeFilterSummary() const;
    // Reset every filter combo to "All…" and reload (the Clear action).
    void clearFilters();
    // Annotation id of the current row in the ACTIVE presentation ('' if none).
    QString selectedAnnotationId() const;
    // RFC-4180 field escaping: quotes doubled, field quoted when it contains
    // '"', ',' or a newline. Public so the escaping contract is testable.
    static QString csvEscapeField(const QString &raw);
    // CSV of the DISPLAYED scope only (the rows the active view renders),
    // using persisted fields confirmed in AnnotationSerializer::toJson
    // (pageIndex, mode, reviewState, author, creationDate, text —
    // modificationDate is NOT persisted and therefore not exported).
    QString displayedCsv() const;
    bool exportDisplayedCsv(const QString &filePath) const;

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
    // U07: the list presentation now consumes the pre-filtered records cache.
    void buildTree();
    // U07: table presentation of the SAME filtered records (page/type/status/
    // author/date/text columns). Another view of the model, not a second store.
    void rebuildTable();
    void rebuildActiveView();
    // U07: run the status/author/date filters over the cached comment records.
    QList<AnnotationItem> applyFilters() const;
    void updateFilterSummary();
    void restoreSelection(const QString &annoId);
    // Current-row annotation id read from ONE named presentation (used by
    // selectedAnnotationId and by the view-mode toggle for the outgoing view).
    QString selectionId(bool tableMode) const;
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

    // ── U07 state ─────────────────────────────────────────────────────────
    // Cached comment records: m_allComments is the unfiltered AddComment set
    // from the viewer; m_lastFiltered is the displayed scope (shared by the
    // tree, the table and the CSV export so all three always agree).
    QList<AnnotationItem> m_allComments;
    QList<AnnotationItem> m_lastFiltered;
    int m_totalComments = 0;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QComboBox *m_viewMode = nullptr;
    QTableWidget *m_table = nullptr;

    // M6-P5: Djot composer helpers
    void wrapSelection(const QString& prefix, const QString& suffix);
    void insertLinePrefix(const QString& prefix);
    void refreshDjotPreview();

    QTextEdit   *m_djotPreview  = nullptr;
};

#endif // COMMENTSWIDGET_H

