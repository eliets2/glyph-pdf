#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QTimer>

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

    // Try to sync scrollbars after a short delay so the views are initialized
    QTimer::singleShot(100, this, [this]() {
        auto view1 = m_viewerLeft->findChild<QAbstractScrollArea*>();
        auto view2 = m_viewerRight->findChild<QAbstractScrollArea*>();
        if (view1 && view2) {
            connect(view1->verticalScrollBar(), &QScrollBar::valueChanged, view2->verticalScrollBar(), &QScrollBar::setValue);
            connect(view2->verticalScrollBar(), &QScrollBar::valueChanged, view1->verticalScrollBar(), &QScrollBar::setValue);
        }
    });
}

bool CompareWidget::loadDocuments(const QString &file1, const QString &file2)
{
    bool ok1 = m_viewerLeft->loadDocument(file1);
    bool ok2 = m_viewerRight->loadDocument(file2);
    return ok1 && ok2;
}

void CompareWidget::setDiffResult(const DiffResult &result) {
    m_diffResult = result;
}

void CompareWidget::setShowPixelDiff(bool show) {
    m_showPixelDiff = show;
    // Apply overlay logic here using a QLabel if needed, or by passing to viewer
    if (m_showPixelDiff && !m_diffResult.pages.isEmpty()) {
        m_viewerRight->setOverlayImage(m_diffResult.pages.first().diffImage);
    } else {
        m_viewerRight->setOverlayImage(QImage());
    }
}

void CompareWidget::nextChange() {}
void CompareWidget::prevChange() {}

