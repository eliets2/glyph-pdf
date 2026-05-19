#include "StatusBar.h"
#include "util/GpTheme.h"

#include <QLabel>
#include <QPdfDocument>
#include <QFile>
#include <QFileInfo>
#include <QSizeF>
#include <QStyle>

#include "core/interfaces/IPdfEditorEngine.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"

namespace gp {

QLabel* StatusBar::makeCell(const QString& text) {
    auto* l = new QLabel(text);
    l->setProperty("role", "statusCell");
    l->setProperty("mono", true);
    return l;
}

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
    setObjectName("glyphStatus");
    setSizeGripEnabled(false);
    setFixedHeight(Theme::StatusH);

    _mode = makeCell("MODE COMMENT");
    _screen = makeCell("SCREEN STANDARD");
    _page = makeCell("PAGE 000/000");
    _zoom = makeCell("ZOOM 100%");
    _tool = makeCell("TOOL —");
    _sel  = makeCell("SEL —");
    addWidget(_mode);
    addWidget(_screen);
    addWidget(_page);
    addWidget(_zoom);
    addWidget(_tool);
    addWidget(_sel);

    _ocrLang = makeCell("OCR · EN");
    _encoding = makeCell("UTF-8");
    _pdfVersion = makeCell("PDF --");
    _pageSize = makeCell("A4 · --×--");
    _docInfo = makeCell("0 P · 0.0 MB");
    _unsaved = makeCell("● 0 UNSAVED");

    _unsaved->setVisible(false); // Hidden by default!

    addPermanentWidget(_ocrLang);
    addPermanentWidget(_encoding);
    addPermanentWidget(_pdfVersion);
    addPermanentWidget(_pageSize);
    addPermanentWidget(_docInfo);
    addPermanentWidget(_unsaved);
}

void StatusBar::setMode(const QString& m)    { _mode->setText("MODE " + m.toUpper()); }
void StatusBar::setTool(const QString& t)    { _tool->setText("TOOL " + t.toUpper()); }
void StatusBar::setPage(int c, int t)        { _page->setText(QString("PAGE %1/%2").arg(c, 3, 10, QChar('0')).arg(t, 3, 10, QChar('0'))); }
void StatusBar::setZoom(int pct)             { _zoom->setText(QString("ZOOM %1%").arg(pct)); }
void StatusBar::setSelection(const QString& s){ _sel->setText("SEL " + (s.isEmpty() ? QStringLiteral("—") : s)); }
void StatusBar::setScreen(const QString& s)  { _screen->setText("SCREEN " + (s.isEmpty() ? QStringLiteral("STANDARD") : s.toUpper())); }

void StatusBar::updateDocData(QPdfDocument* doc, const QString& filePath) {
    if (!doc || filePath.isEmpty()) {
        _pdfVersion->setText("PDF --");
        _pageSize->setText("A4 · --×--");
        _docInfo->setText("0 P · 0.0 MB");
        return;
    }

    // 1. PDF version: Parse from the first 8 bytes of the file
    QString versionStr = "PDF 1.7"; // default fallback
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(8);
        if (header.startsWith("%PDF-")) {
            versionStr = "PDF " + QString::fromLatin1(header.mid(5).trimmed());
        }
        file.close();
    }
    _pdfVersion->setText(versionStr);

    // 2. Page Size:
    QSizeF sz = doc->pagePointSize(0); // in points
    QString sizeName = "Custom";
    int w = qRound(sz.width());
    int h = qRound(sz.height());
    
    // Page dimension tolerances
    if ((w >= 590 && w <= 600 && h >= 835 && h <= 847) || 
        (w >= 835 && w <= 847 && h >= 590 && h <= 600)) {
        sizeName = "A4";
    } else if ((w >= 605 && w <= 618 && h >= 785 && h <= 798) || 
               (w >= 785 && w <= 798 && h >= 605 && h <= 618)) {
        sizeName = "Letter";
    } else if ((w >= 605 && w <= 618 && h >= 1000 && h <= 1015) || 
               (w >= 1000 && w <= 1015 && h >= 605 && h <= 618)) {
        sizeName = "Legal";
    }
    
    _pageSize->setText(QString("%1 · %2×%3").arg(sizeName).arg(w).arg(h));

    // 3. Document Info:
    int pages = doc->pageCount();
    QFileInfo fi(filePath);
    qint64 size = fi.size();
    QString sizeStr;
    if (size < 1024)
        sizeStr = QStringLiteral("%1 B").arg(size);
    else if (size < 1024 * 1024)
        sizeStr = QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else
        sizeStr = QStringLiteral("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);

    _docInfo->setText(QString("%1 P · %2").arg(pages).arg(sizeStr));
}

void StatusBar::updateUnsaved(bool dirty) {
    if (dirty) {
        _unsaved->setText("● 1 UNSAVED");
        _unsaved->setProperty("state", "unsaved");
        _unsaved->style()->unpolish(_unsaved);
        _unsaved->style()->polish(_unsaved);
        _unsaved->setVisible(true);
    } else {
        _unsaved->setProperty("state", "");
        _unsaved->style()->unpolish(_unsaved);
        _unsaved->style()->polish(_unsaved);
        _unsaved->setVisible(false);
    }
}

void StatusBar::updateFromDocument(IPdfEditorEngine* engine, const QString& filePath) {
    if (!engine || filePath.isEmpty()) {
        _ocrLang->setVisible(false);
        _pdfVersion->setText("PDF --");
        _pageSize->setText("A4 · --×--");
        _docInfo->setText("0 P · 0.0 MB");
        _unsaved->setVisible(false);
        return;
    }

    auto* mainWindow = qobject_cast<MainWindow*>(parentWidget());
    auto* viewer = mainWindow ? mainWindow->pdfViewer() : nullptr;

    // 1. OCR Languages
    if (viewer) {
        _ocrLang->setText("OCR · EN");
        _ocrLang->setVisible(true);
    } else {
        _ocrLang->setVisible(false);
    }

    // 2. Encoding
    _encoding->setText("UTF-8");

    // 3. PDF Version
    QString versionStr = "PDF 1.7";
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(8);
        if (header.startsWith("%PDF-")) {
            versionStr = "PDF " + QString::fromLatin1(header.mid(5).trimmed());
        }
        file.close();
    }
    _pdfVersion->setText(versionStr);

    // 4. Page Size from the current active page
    if (viewer && viewer->document()) {
        int currentPage = viewer->currentPage();
        QSizeF sz = viewer->document()->pagePointSize(currentPage);
        QString sizeName = "Custom";
        int w = qRound(sz.width());
        int h = qRound(sz.height());
        
        if ((w >= 590 && w <= 600 && h >= 835 && h <= 847) || 
            (w >= 835 && w <= 847 && h >= 590 && h <= 600)) {
            sizeName = "A4";
        } else if ((w >= 605 && w <= 618 && h >= 785 && h <= 798) || 
                   (w >= 785 && w <= 798 && h >= 605 && h <= 618)) {
            sizeName = "Letter";
        } else if ((w >= 605 && w <= 618 && h >= 1000 && h <= 1015) || 
                   (w >= 1000 && w <= 1015 && h >= 605 && h <= 618)) {
            sizeName = "Legal";
        }
        _pageSize->setText(QString("%1 · %2×%3").arg(sizeName).arg(w).arg(h));

        // 5. Doc Info
        int pages = viewer->pageCount();
        QFileInfo fi(filePath);
        qint64 size = fi.size();
        QString sizeStr;
        if (size < 1024)
            sizeStr = QStringLiteral("%1 B").arg(size);
        else if (size < 1024 * 1024)
            sizeStr = QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        else
            sizeStr = QStringLiteral("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);

        _docInfo->setText(QString("%1 P · %2").arg(pages).arg(sizeStr));
    } else {
        _pageSize->setText("A4 · --×--");
        _docInfo->setText("0 P · 0.0 MB");
    }
}

} // namespace gp
