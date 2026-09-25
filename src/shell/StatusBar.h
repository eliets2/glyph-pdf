// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QStatusBar>
#include <QString>

class QLabel;
class QSpinBox;
class QToolButton;
class IPdfEditorEngine;

namespace gp {

// U02 — the slim status bar.
//
// The bar shows only useful document state: page n of m, zoom, saved/unsaved
// (permanent, right) and the current operation via the QStatusBar showMessage
// contract (transient, left). PDF version, page dimensions, selection and the
// OCR language moved into a details affordance (QToolButton popup) fed by a
// DocumentFacts struct. There is no "page 1 / 000" empty state: with no
// document the total reads "/ —" and the page spin box is disabled.
class StatusBar : public QStatusBar {
    Q_OBJECT
public:
    struct DocumentFacts {
        QString currentTask;   // human task name ("OCR Verify", "Standard")
        QString tool;          // active tool id ("ocr") or "—"
        QString selection;     // selection summary or "—"
        QString pdfVersion;    // "PDF 1.7" or "PDF --"
        QString pageSize;      // "A4 · 595×842" or "--×--"
        QString documentInfo;  // "12 P · 1.2 MB" or "0 P · 0.0 MB"
        QString ocrLanguage;   // "OCR · EN" (persisted ocr/language)
    };

    explicit StatusBar(QWidget* parent = nullptr);

    void setPage(int current, int total);
    void setZoom(int pct);
    void setSelection(const QString& sel);
    void setTool(const QString& t);
    /// The current-task line for the details affordance (not a bar cell).
    void setScreen(const QString& s);

    /// Transient operation channel (no invented timeout — QStatusBar contract).
    void setOperation(const QString& op);
    void clearOperation();

    void updateUnsaved(bool dirty);
    void updateFromDocument(IPdfEditorEngine* engine, const QString& filePath);

    DocumentFacts currentFacts() const { return _facts; }
    QString currentTask() const { return _facts.currentTask; }
    QString detailsText() const;
    void showDetailsPopup();

    /// "PDF 1.7" only when a real %PDF- header exists; "PDF --" otherwise.
    static QString parsePdfVersion(const QString& filePath);

signals:
    void jumpToPageRequested(int page);  // 0-based page index

private slots:
    void refreshDetails();

private:
    QLabel* makeCell(const QString& text);
    static QString ocrLanguageText();

    QSpinBox*    _pageSpinBox = nullptr;
    QLabel*      _pageTotal   = nullptr;
    QLabel*      _zoom        = nullptr;
    QLabel*      _unsaved     = nullptr;
    QToolButton* _detailsBtn  = nullptr;
    QMenu*       _detailsMenu = nullptr;
    QLabel*      _detailsContent = nullptr;

    DocumentFacts _facts;
};

} // namespace gp
