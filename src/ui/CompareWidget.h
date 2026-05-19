#ifndef COMPAREWIDGET_H
#define COMPAREWIDGET_H

#include <QWidget>

class PdfViewerWidget;

class CompareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CompareWidget(QWidget *parent = nullptr);
    
    bool loadDocuments(const QString &file1, const QString &file2);

private:
    PdfViewerWidget *m_viewerLeft;
    PdfViewerWidget *m_viewerRight;
};

#endif // COMPAREWIDGET_H
