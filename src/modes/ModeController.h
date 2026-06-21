// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QStackedWidget>
#include <QHash>
#include <QList>
#include <QRectF>

class PdfViewerWidget;
struct AppContext;
struct MergedOcrWord;   // engines/ocr/OcrPipeline.h (fwd-declared to keep QtConcurrent out of this header)

namespace gp {

class OCRMode;
class RedactMode;
class CompareMode;
class PagesMode;
class BatchMode;
class FormBuilderMode;

// Owns the central area: one QStackedWidget routing between Standard canvas and
// the 11 extended mode widgets.
class ModeController : public QStackedWidget {
    Q_OBJECT
public:
    explicit ModeController(QWidget* parent = nullptr);

    void setScreen(const QString& id);     // "" / "ocr" / "redact" / ...
    QString currentScreen() const { return _currentScreen; }

    PdfViewerWidget* viewer() const { return _viewer; }

    // Must be called before setScreen("form") is triggered.
    void setAppContext(const AppContext* ctx) { _ctx = ctx; }

    // Forward recognised OCR words to the OCR Verify screen (if it has been created).
    void deliverOcrResults(const QList<MergedOcrWord>& words);

signals:
    void screenChanged(const QString& id);
    // Emitted when the OCR Verify screen's Run button is pressed; the host wires this
    // to the real OCR pipeline (EditController::runOcr).
    void ocrRunRequested();
    // OCR review-workflow relays (mirror ocrRunRequested): the host wires these
    // to real behaviour. Accept = keep applied results; Reject = drop pending
    // results; Re-run region = re-run OCR (whole page until region mapping ships).
    void ocrReviewAccepted();
    void ocrReviewRejected();
    void ocrReRunRegionRequested(QRectF regionBbox);

private:
    QHash<QString, QWidget*> _byId;
    QString     _currentScreen;
    PdfViewerWidget* _viewer = nullptr;
    const AppContext* _ctx = nullptr;
};

} // namespace gp

