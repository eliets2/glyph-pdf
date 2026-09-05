// SPDX-License-Identifier: Apache-2.0
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
    /// Navigate to the next change (add / delete / move / structural page
    /// change).  Wraps around.
    void nextChange();
    /// Navigate to the previous change (add / delete / move / structural page
    /// change).  Wraps around.
    void prevChange();

public:
    /// R11: jump directly to change #index (0-based) in the one shared change
    /// sequence — used by CHANGES-tree selection. Same state as next/previous.
    void scrollToChange(int index);

private:
    /// One navigable change: its HTML anchor id plus the old/new page it lives
    /// on (-1 = that side has no page, e.g. an added page has no old side).
    struct ChangeAnchor {
        QString id;
        int oldPage = -1;
        int newPage = -1;
    };

    void buildTextDiff();
    QString buildHtml() const;
    void applyAnchor(int index);

    PdfViewerWidget* m_viewerLeft  = nullptr;
    PdfViewerWidget* m_viewerRight = nullptr;
    QTextBrowser*    m_textDiff    = nullptr;
    QLabel*          m_navLabel    = nullptr;

    DiffResult       m_diffResult;
    bool             m_showPixelDiff = false;

    // Navigation: anchors for each change in the shared sequence (structural
    // page changes first, then per-page token changes).
    QList<ChangeAnchor> m_anchors;
    int              m_currentAnchor = -1;
};
