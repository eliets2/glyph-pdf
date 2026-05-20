#ifndef COMPAREWIDGET_H
#define COMPAREWIDGET_H

#include <QWidget>

#include "engines/DiffEngine.h"

class PdfViewerWidget;

class CompareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CompareWidget(QWidget *parent = nullptr);
    
    bool loadDocuments(const QString &file1, const QString &file2);

    void setDiffResult(const DiffResult &result);
    void setShowPixelDiff(bool show);
    void nextChange();
    void prevChange();

private:
    PdfViewerWidget *m_viewerLeft;
    PdfViewerWidget *m_viewerRight;
    DiffResult m_diffResult;
    bool m_showPixelDiff = false;
};

#endif // COMPAREWIDGET_H
