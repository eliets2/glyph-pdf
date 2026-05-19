#ifndef SIGNATURESWIDGET_H
#define SIGNATURESWIDGET_H

#include <QWidget>
#include <QList>
#include "core/interfaces/ISignatureManager.h"

QT_BEGIN_NAMESPACE
class QListWidget;
class QLabel;
QT_END_NAMESPACE

class SignaturesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignaturesWidget(ISignatureManager *signMgr, QWidget *parent = nullptr);
    void setDocumentFile(const QString &filePath);
    void clear();

private:
    QListWidget *m_listWidget = nullptr;
    QLabel *m_emptyLabel = nullptr;
    ISignatureManager *m_signManager = nullptr;
};

#endif // SIGNATURESWIDGET_H

