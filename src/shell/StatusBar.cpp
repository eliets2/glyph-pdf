// SPDX-License-Identifier: Apache-2.0
#include "StatusBar.h"
#include "TaskNav.h"
#include "util/GpTheme.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QSettings>
#include <QSizeF>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>

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

QString StatusBar::ocrLanguageText() {
    // Mirror the OCR mode's persisted language code (set in OCRMode); default EN.
    QSettings settings;
    const QString code = settings.value(QStringLiteral("ocr/language"), QStringLiteral("EN"))
                             .toString().toUpper();
    return tr("OCR \xC2\xB7 %1").arg(code);
}

QString StatusBar::parsePdfVersion(const QString& filePath) {
    // A5: start with "PDF --"; only upgrade to a real version string when the
    // header is actually present and parseable.
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray header = file.read(8);
        file.close();
        if (header.startsWith("%PDF-"))
            return QStringLiteral("PDF ") + QString::fromLatin1(header.mid(5).trimmed());
    }
    return tr("PDF --");
}

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
    setObjectName("glyphStatus");
    setSizeGripEnabled(false);
    setFixedHeight(Theme::StatusH);
    setAccessibleName(tr("Status bar"));
    setAccessibleDescription(tr("Displays page, zoom, unsaved state, and the current operation"));

    // Facts defaults — before the details popup is built so its content is
    // correct from construction on.
    _facts.currentTask  = TaskNav::title(QString());
    _facts.tool         = tr("\xE2\x80\x94");
    _facts.selection    = tr("\xE2\x80\x94");
    _facts.pdfVersion   = tr("PDF --");
    _facts.pageSize     = QStringLiteral("--\u00D7--");
    _facts.documentInfo = tr("0 P \xC2\xB7 0.0 MB");
    _facts.ocrLanguage  = ocrLanguageText();

    _pageSpinBox = new QSpinBox(this);
    _pageSpinBox->setObjectName("statusPageSpin");
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
    // U02: no document-specific controls with no document open.
    _pageSpinBox->setEnabled(false);

    _pageTotal = makeCell(tr("/ \xE2\x80\x94"));   // "/ —" — never the old "/ 000"
    _pageTotal->setObjectName("statusPageTotal");
    _pageTotal->setAccessibleName(tr("Total pages"));

    _zoom = makeCell(tr("ZOOM 100%"));
    _zoom->setObjectName("statusZoom");
    _zoom->setAccessibleName(tr("Zoom level"));

    _unsaved = makeCell(tr("\xE2\x97\x8F 0 UNSAVED"));
    _unsaved->setObjectName("statusUnsaved");
    _unsaved->setAccessibleName(tr("Unsaved changes indicator"));
    _unsaved->setVisible(false);

    // Permanent right side = the whole default bar: page / zoom / unsaved /
    // details affordance. Everything else lives in the details popup or the
    // transient showMessage channel.
    addPermanentWidget(_pageSpinBox);
    addPermanentWidget(_pageTotal);
    addPermanentWidget(_zoom);
    addPermanentWidget(_unsaved);

    _detailsBtn = new QToolButton(this);
    _detailsBtn->setObjectName("statusDetails");
    _detailsBtn->setText(tr("Details"));
    _detailsBtn->setToolTip(tr("Document details: version, dimensions, selection, OCR language"));
    _detailsBtn->setAccessibleName(tr("Document details"));
    _detailsBtn->setAutoRaise(true);
    _detailsBtn->setPopupMode(QToolButton::InstantPopup);
    // Persistent details popup — a QMenu hosting the live facts label. The
    // label is refreshed in place (never rebuilt) so its objectName stays
    // stable for tests and its content never goes stale.
    _detailsMenu = new QMenu(_detailsBtn);
    _detailsContent = new QLabel(detailsText(), _detailsMenu);
    _detailsContent->setObjectName("statusDetailsLabel");
    _detailsContent->setProperty("mono", true);
    _detailsContent->setMargin(8);
    _detailsContent->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* wa = new QWidgetAction(_detailsMenu);
    wa->setDefaultWidget(_detailsContent);
    _detailsMenu->addAction(wa);
    _detailsBtn->setMenu(_detailsMenu);
    addPermanentWidget(_detailsBtn);

    // Wire jump-to-page
    connect(_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        emit jumpToPageRequested(value - 1);  // Convert 1-based display to 0-based index
    });
}

void StatusBar::refreshDetails() {
    if (_detailsContent)
        _detailsContent->setText(detailsText());
}

void StatusBar::setPage(int c, int t) {
    _pageSpinBox->setEnabled(true);
    _pageSpinBox->blockSignals(true);
    _pageSpinBox->setMaximum(qMax(1, t));
    _pageSpinBox->setValue(qBound(1, c, qMax(1, t)));
    _pageSpinBox->blockSignals(false);
    _pageTotal->setText(QString("/ %1").arg(t));
}

void StatusBar::setZoom(int pct) { _zoom->setText(tr("ZOOM %1%").arg(QLocale::system().toString(pct))); }

void StatusBar::setSelection(const QString& s) {
    _facts.selection = s.isEmpty() ? tr("\xE2\x80\x94") : s;
    refreshDetails();
}

void StatusBar::setTool(const QString& t) {
    _facts.tool = t.isEmpty() ? tr("\xE2\x80\x94") : t;
    refreshDetails();
}

void StatusBar::setScreen(const QString& s) {
    // U02: the current task is details-popup content; TaskStateSync is the
    // only writer that reaches this.
    _facts.currentTask = s.isEmpty() ? TaskNav::title(QString()) : TaskNav::title(s);
    refreshDetails();
}

void StatusBar::setOperation(const QString& op) {
    showMessage(op);   // 0 timeout: stays until replaced or cleared
}

void StatusBar::clearOperation() {
    clearMessage();
}

void StatusBar::updateUnsaved(bool dirty) {
    if (dirty) {
        _unsaved->setText(tr("\xE2\x97\x8F 1 UNSAVED"));
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
        // No document: placeholders, and the page jump is disabled (the
        // "page 1 / 000" artifact is gone by construction).
        _pageSpinBox->setEnabled(false);
        _pageTotal->setText(tr("/ \xE2\x80\x94"));
        _unsaved->setVisible(false);
        _facts.pdfVersion   = tr("PDF --");
        _facts.pageSize     = QStringLiteral("--\u00D7--");
        _facts.documentInfo = tr("0 P \xC2\xB7 0.0 MB");
        refreshDetails();
        return;
    }

    _facts.pdfVersion  = parsePdfVersion(filePath);
    _facts.ocrLanguage = ocrLanguageText();

    auto* mainWindow = qobject_cast<MainWindow*>(parentWidget());
    auto* viewer = mainWindow ? mainWindow->pdfViewer() : nullptr;

    if (viewer && viewer->document()) {
        const int currentPage = viewer->currentPage();
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
        _facts.pageSize = tr("%1 \xC2\xB7 %2\xC3\x97%3").arg(sizeName).arg(loc.toString(w)).arg(loc.toString(h));

        const int pages = viewer->pageCount();
        QFileInfo fi(filePath);
        const qint64 size = fi.size();
        const QString sizeStr = loc.formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat);
        _facts.documentInfo = tr("%1 P \xC2\xB7 %2").arg(loc.toString(pages)).arg(sizeStr);
    } else {
        _facts.pageSize     = QStringLiteral("--\u00D7--");
        _facts.documentInfo = tr("0 P \xC2\xB7 0.0 MB");
    }
    refreshDetails();
}

QString StatusBar::detailsText() const {
    // The details affordance: current task + the debug-style values that no
    // longer occupy the bar (plan U02).
    return tr("Task: %1").arg(_facts.currentTask) + QLatin1Char('\n') +
           tr("Tool: %1").arg(_facts.tool) + QLatin1Char('\n') +
           tr("Selection: %1").arg(_facts.selection) + QLatin1Char('\n') +
           tr("OCR language: %1").arg(_facts.ocrLanguage) + QLatin1Char('\n') +
           tr("PDF version: %1").arg(_facts.pdfVersion) + QLatin1Char('\n') +
           tr("Page size: %1").arg(_facts.pageSize) + QLatin1Char('\n') +
           tr("Document: %1").arg(_facts.documentInfo);
}

void StatusBar::showDetailsPopup() {
    if (_detailsBtn && _detailsBtn->menu())
        _detailsBtn->menu()->popup(QCursor::pos());   // non-blocking
}

} // namespace gp
