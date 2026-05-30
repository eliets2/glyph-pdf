#pragma once

#include <QWidget>
#include "engines/DiffEngine.h"

class PdfViewerWidget;
class QTextBrowser;
class QLabel;

class CompareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CompareWidget(QWidget *parent = nullptr);

    bool loadDocuments(const QString &file1, const QString &file2);
    void setDiffResult(const DiffResult &result);
    void setShowPixelDiff(bool show);

public slots:
    /// Navigate to the next change (add / delete / move).  Wraps around.
    void nextChange();
    /// Navigate to the previous change (add / delete / move).  Wraps around.
    void prevChange();

private:
    void buildTextDiff();
    QString buildHtml() const;

    PdfViewerWidget* m_viewerLeft  = nullptr;
    PdfViewerWidget* m_viewerRight = nullptr;
    QTextBrowser*    m_textDiff    = nullptr;
    QLabel*          m_navLabel    = nullptr;

    DiffResult       m_diffResult;
    bool             m_showPixelDiff = false;

    // Navigation: list of anchors for each change block
    QStringList      m_anchors;
    int              m_currentAnchor = -1;
};
