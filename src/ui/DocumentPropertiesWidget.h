#ifndef DOCUMENTPROPERTIESWIDGET_H
#define DOCUMENTPROPERTIESWIDGET_H

#include <QWidget>
#include <QPdfDocument>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

class DocumentPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPropertiesWidget(QWidget *parent = nullptr);
    void setDocument(QPdfDocument *document, const QString &filePath);

private:
    void clearUi();
    void populate(QPdfDocument *document, const QString &filePath);

    QLabel *m_filePath = nullptr;
    QLabel *m_fileSize = nullptr;
    QLabel *m_pageCount = nullptr;

    QLabel *m_title = nullptr;
    QLabel *m_author = nullptr;
    QLabel *m_subject = nullptr;
    QLabel *m_keywords = nullptr;
    QLabel *m_creator = nullptr;
    QLabel *m_producer = nullptr;
    QLabel *m_creationDate = nullptr;
    QLabel *m_modDate = nullptr;
};

#endif // DOCUMENTPROPERTIESWIDGET_H

