#include "StatusBar.h"
#include "util/GpTheme.h"

#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QPdfDocument>
#include <QFile>
#include <QFileInfo>
#include <QSizeF>
#include <QStyle>
#include <QSettings>

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

QString StatusBar::ocrLanguageCellText() {
    // Mirror the OCR mode's persisted language code (set in OCRMode); default EN.
    QSettings settings;
    const QString code = settings.value(QStringLiteral("ocr/language"), QStringLiteral("EN"))
                             .toString().toUpper();
    return tr("OCR · %1").arg(code);
}

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
    setObjectName("glyphStatus");
    setSizeGripEnabled(false);
    setFixedHeight(Theme::StatusH);
    setAccessibleName(tr("Status bar"));
    setAccessibleDescription(tr("Displays document mode, page, zoom, and document information"));

    _mode = makeCell(tr("MODE COMMENT"));
    _mode->setAccessibleName(tr("Current editing mode"));
    _screen = makeCell(tr("SCREEN STANDARD"));
    _screen->setAccessibleName(tr("Current screen"));

    _pageSpinBox = new QSpinBox(this);
    _pageSpinBox->setPrefix(tr("PAGE "));
    _pageSpinBox->setMinimum(1);
    _pageSpinBox->setMaximum(1);
    _pageSpinBox->setValue(1);
    _pageSpinBox->setFixedWidth(100);
    _pageSpinBox->setAlignment(Qt::AlignCenter);
    _pageSpinBox->setAccessibleName(tr("Jump to page"));
    _pageSpinBox->setAccessibleDescription(tr("Enter a page number and press Enter to navigate"));
    _pageSpinBox->setKeyboardTracking(false);
    _pageSpinBox->setFocusPolicy(Qt::TabFocus);

    _pageTotal = makeCell(tr("/ 000"));
    _pageTotal->setAccessibleName(tr("Total pages"));

    _zoom = makeCell(tr("ZOOM 100%"));
    _zoom->setAccessibleName(tr("Zoom level"));
    _tool = makeCell(tr("TOOL —"));
    _tool->setAccessibleName(tr("Active tool"));
    _sel  = makeCell(tr("SEL —"));
    _sel->setAccessibleName(tr("Selection info"));
    addWidget(_mode);
    addWidget(_screen);
    addWidget(_pageSpinBox);
    addWidget(_pageTotal);
    addWidget(_zoom);
    addWidget(_tool);
    addWidget(_sel);

    _ocrLang = makeCell(ocrLanguageCellText());
    _ocrLang->setAccessibleName(tr("OCR language"));
    _pdfVersion = makeCell(tr("PDF --"));
    _pdfVersion->setAccessibleName(tr("PDF version"));
    _pageSize = makeCell(tr("A4 · --×--"));
    _pageSize->setAccessibleName(tr("Page dimensions"));
    _docInfo = makeCell(tr("0 P · 0.0 MB"));
    _docInfo->setAccessibleName(tr("Document info"));
    _unsaved = makeCell(tr("● 0 UNSAVED"));
    _unsaved->setAccessibleName(tr("Unsaved changes indicator"));

    _unsaved->setVisible(false);

    addPermanentWidget(_ocrLang);
    addPermanentWidget(_pdfVersion);
    addPermanentWidget(_pageSize);
    addPermanentWidget(_docInfo);
    addPermanentWidget(_unsaved);

    // Wire jump-to-page
    connect(_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        emit jumpToPageRequested(value - 1);  // Convert 1-based display to 0-based index
    });
}

void StatusBar::setMode(const QString& m)    { _mode->setText(tr("MODE %1").arg(m.toUpper())); }
void StatusBar::setTool(const QString& t)    { _tool->setText(tr("TOOL %1").arg(t.toUpper())); }

void StatusBar::setPage(int c, int t) {
    _totalPages = t;
    _pageSpinBox->blockSignals(true);
    _pageSpinBox->setMaximum(qMax(1, t));
    _pageSpinBox->setValue(qBound(1, c, qMax(1, t)));
    _pageSpinBox->blockSignals(false);
    _pageTotal->setText(QString("/ %1").arg(t));
}

void StatusBar::setZoom(int pct)             { _zoom->setText(tr("ZOOM %1%").arg(QLocale::system().toString(pct))); }
void StatusBar::setSelection(const QString& s){ _sel->setText(tr("SEL %1").arg(s.isEmpty() ? QStringLiteral("—") : s)); }
void StatusBar::setScreen(const QString& s)  { _screen->setText(tr("SCREEN %1").arg(s.isEmpty() ? tr("STANDARD") : s.toUpper())); }

void StatusBar::updateDocData(QPdfDocument* doc, const QString& filePath) {
    QLocale loc = QLocale::system();
    if (!doc || filePath.isEmpty()) {
        _pdfVersion->setText(tr("PDF --"));
        _pageSize->setText(tr("A4 · --×--"));
        _docInfo->setText(tr("0 P · 0.0 MB"));
        return;
    }

    QString versionStr = QStringLiteral("PDF 1.7");
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(8);
        if (header.startsWith("%PDF-")) {
            versionStr = QStringLiteral("PDF ") + QString::fromLatin1(header.mid(5).trimmed());
        }
        file.close();
    }
    _pdfVersion->setText(versionStr);

    QSizeF sz = doc->pagePointSize(0);
    QString sizeName = tr("Custom");
    int w = qRound(sz.width());
    int h = qRound(sz.height());

    if ((w >= 590 && w <= 600 && h >= 835 && h <= 847) ||
        (w >= 835 && w <= 847 && h >= 590 && h <= 600)) {
        sizeName = QStringLiteral("A4");
    } else if ((w >= 605 && w <= 618 && h >= 785 && h <= 798) ||
               (w >= 785 && w <= 798 && h >= 605 && h <= 618)) {
        sizeName = tr("Letter");
    } else if ((w >= 605 && w <= 618 && h >= 1000 && h <= 1015) ||
               (w >= 1000 && w <= 1015 && h >= 605 && h <= 618)) {
        sizeName = tr("Legal");
    }

    _pageSize->setText(tr("%1 · %2×%3").arg(sizeName).arg(loc.toString(w)).arg(loc.toString(h)));

    int pages = doc->pageCount();
    QFileInfo fi(filePath);
    qint64 size = fi.size();
    QString sizeStr = loc.formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat);

    _docInfo->setText(tr("%1 P · %2").arg(loc.toString(pages)).arg(sizeStr));
}

void StatusBar::updateUnsaved(bool dirty) {
    if (dirty) {
        _unsaved->setText(tr("● 1 UNSAVED"));
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
    QLocale loc = QLocale::system();
    if (!engine || filePath.isEmpty()) {
        _ocrLang->setVisible(false);
        _pdfVersion->setText(tr("PDF --"));
        _pageSize->setText(tr("A4 · --×--"));
        _docInfo->setText(tr("0 P · 0.0 MB"));
        _unsaved->setVisible(false);
        return;
    }

    auto* mainWindow = qobject_cast<MainWindow*>(parentWidget());
    auto* viewer = mainWindow ? mainWindow->pdfViewer() : nullptr;

    if (viewer) {
        _ocrLang->setText(ocrLanguageCellText());
        _ocrLang->setVisible(true);
    } else {
        _ocrLang->setVisible(false);
    }

    QString versionStr = QStringLiteral("PDF 1.7");
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(8);
        if (header.startsWith("%PDF-")) {
            versionStr = QStringLiteral("PDF ") + QString::fromLatin1(header.mid(5).trimmed());
        }
        file.close();
    }
    _pdfVersion->setText(versionStr);

    if (viewer && viewer->document()) {
        int currentPage = viewer->currentPage();
        QSizeF sz = viewer->document()->pagePointSize(currentPage);
        QString sizeName = tr("Custom");
        int w = qRound(sz.width());
        int h = qRound(sz.height());

        if ((w >= 590 && w <= 600 && h >= 835 && h <= 847) ||
            (w >= 835 && w <= 847 && h >= 590 && h <= 600)) {
            sizeName = QStringLiteral("A4");
        } else if ((w >= 605 && w <= 618 && h >= 785 && h <= 798) ||
                   (w >= 785 && w <= 798 && h >= 605 && h <= 618)) {
            sizeName = tr("Letter");
        } else if ((w >= 605 && w <= 618 && h >= 1000 && h <= 1015) ||
                   (w >= 1000 && w <= 1015 && h >= 605 && h <= 618)) {
            sizeName = tr("Legal");
        }
        _pageSize->setText(tr("%1 · %2×%3").arg(sizeName).arg(loc.toString(w)).arg(loc.toString(h)));

        int pages = viewer->pageCount();
        QFileInfo fi(filePath);
        qint64 size = fi.size();
        QString sizeStr = loc.formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat);

        _docInfo->setText(tr("%1 P · %2").arg(loc.toString(pages)).arg(sizeStr));
    } else {
        _pageSize->setText(tr("A4 · --×--"));
        _docInfo->setText(tr("0 P · 0.0 MB"));
    }
}

} // namespace gp
