#ifndef COMMENTSWIDGET_H
#define COMMENTSWIDGET_H

#include <QWidget>
#include <QDateTime>

QT_BEGIN_NAMESPACE
class QListWidget;
class QLineEdit;
class QTextEdit;
class QPushButton;
QT_END_NAMESPACE

class CommentsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommentsWidget(QWidget *parent = nullptr);
    void setDocumentFile(const QString &filePath);
    void setCurrentPage(int page);

signals:
    void commentDoubleClicked(int page);

private slots:
    void addComment();

private:
    struct CommentItem {
        QString author;
        QString content;
        int page = -1;
        QDateTime createdAt;
    };

    void load();
    void save() const;
    void refreshList();
    QString storagePath() const;

    QString m_filePath;
    int m_currentPage = 1;
    QList<CommentItem> m_comments;

    QListWidget *m_list = nullptr;
    QLineEdit *m_author = nullptr;
    QTextEdit *m_editor = nullptr;
    QPushButton *m_addBtn = nullptr;
};

#endif // COMMENTSWIDGET_H

