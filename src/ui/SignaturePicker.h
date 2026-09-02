// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDialog>
#include <QImage>
#include <QString>
#include "core/AnnotationTypes.h"
#include "core/PdfEnums.h"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

// §9.7 P0 (audit 2026-07-01): Draw/Type/Upload signature picker.
//
// The namespace below is the testable seam: Type and Upload produce a QImage
// that is placed like an image annotation, feeding the SAME persistence path
// as Draw (AnnotationLayer painting + PoDoFoBackend /Stamp appearance stream).
// Draw itself stays the existing freehand flow — the picker only decides which
// flow the controller arms. No new third-party dependencies: Type renders via
// QPainter, Upload via QImage's codecs.
namespace SignatureContent {

enum class Kind { Draw, Typed, Upload };

// Maximum dimension accepted for an uploaded signature image — mirrors the
// 10000 x 10000 cap PoDoFoBackend::replaceImage / addImageWatermark enforce.
inline constexpr int kMaxImageDim = 10000;

// Type tab: render `text` with `fontFamily` onto a transparent canvas, inked
// in `color`. Returns a null image when the text is blank — callers must treat
// a null result exactly like a cancelled dialog. QFont falls back to the
// default family gracefully when a handwriting font is not installed.
QImage renderTyped(const QString &text, const QString &fontFamily,
                   int pointSize, const QColor &color);

// Upload tab: load an image from `path`. On failure returns a null image and
// a non-empty `error` (never throws, never crashes on unreadable files).
// Images larger than kMaxImageDim are downscaled to fit (huge-file edge case).
QImage loadUploaded(const QString &path, QString *error = nullptr);

// Both tabs: build the AnnotationItem that AnnotationLayer paints and
// PoDoFoBackend persists. `typedText` is kept as the item's /Contents fallback
// so typed signatures remain searchable. Kind::Draw has no image form — it
// maps to the existing freehand ToolMode with a null image.
AnnotationItem makeAnnotation(Kind kind, int pageIndex, const QRectF &rect,
                              const QImage &image, const QString &typedText = {});

} // namespace SignatureContent

// The dialog itself: 3 tabs (Draw / Type / Upload). Draw keeps the existing
// freehand flow — pressing OK simply returns Kind::Draw and the controller
// arms ToolMode::AddSignature as before. Type and Upload produce a QImage
// here. OK stays disabled until the active tab yields a usable signature
// (non-empty text / decoded image), so neither an empty text nor an
// unreadable upload can be accepted. Tests drive the namespace-level seam
// above directly; the dialog is never exec()'d in tests.
class SignaturePickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignaturePickerDialog(QWidget *parent = nullptr);

    // Valid after accept(). Before that: Kind::Draw / null image.
    SignatureContent::Kind acceptedKind() const { return m_kind; }
    QImage acceptedImage() const { return m_image; }
    QString acceptedText() const { return m_typedText; }

    // Whether OK is currently enabled for the active tab (test + a11y seam).
    bool isAcceptEnabled() const;

    // Switch tabs programmatically (the UI uses the tab bar).
    void showTab(SignatureContent::Kind kind);

private:
    void updateTypePreview();
    void updateUploadPreview();
    void updateAccept();
    void onAccepted();

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_typeEdit = nullptr;
    QComboBox *m_fontCombo = nullptr;
    QSpinBox *m_sizeSpin = nullptr;
    QLabel *m_typePreview = nullptr;
    QLabel *m_uploadPreview = nullptr;
    QLabel *m_uploadError = nullptr;
    QPushButton *m_browseButton = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    QImage m_uploadImage;
    SignatureContent::Kind m_kind = SignatureContent::Kind::Draw;
    QImage m_image;
    QString m_typedText;
};
