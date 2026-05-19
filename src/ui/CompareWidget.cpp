#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"
#include <QHBoxLayout>
#include <QSplitter>

CompareWidget::CompareWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    
    m_viewerLeft = new PdfViewerWidget(this);
    m_viewerRight = new PdfViewerWidget(this);

    splitter->addWidget(m_viewerLeft);
    splitter->addWidget(m_viewerRight);
    
    layout->addWidget(splitter);
}

bool CompareWidget::loadDocuments(const QString &file1, const QString &file2)
{
    bool ok1 = m_viewerLeft->loadDocument(file1);
    bool ok2 = m_viewerRight->loadDocument(file2);
    return ok1 && ok2;
}
