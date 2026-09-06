// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDialog>
#include <QImage>
#include <QObject>
#include <QString>
#include "core/AnnotationTypes.h"
#include "core/PdfEnums.h"

class QCheckBox;
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

enum class Kind { Draw, Typed, Upload, Initials };

// Maximum dimension accepted for an uploaded signature image — mirrors the
// 10000 x 10000 cap PoDoFoBackend::replaceImage / addImageWatermark enforce.
inline constexpr int kMaxImageDim = 10000;

// §9.7 P1: default render sizes. The Type tab writes the full name at 36pt;
// the Initials tab writes the derived monogram ("John Hancock" → "JH") at a
// deliberately smaller 24pt so the initials read as a compact paraph.
inline constexpr int kTypedPointSizeDefault = 36;
inline constexpr int kInitialsPointSizeDefault = 24;

// Type tab: render `text` with `fontFamily` onto a transparent canvas, inked
// in `color`. Returns a null image when the text is blank — callers must treat
// a null result exactly like a cancelled dialog. QFont falls back to the
// default family gracefully when a handwriting font is not installed.
QImage renderTyped(const QString &text, const QString &fontFamily,
                   int pointSize, const QColor &color);

// §9.7 P1 Initials tab: derive the monogram from a full name — the first
// letter of every whitespace-separated token, uppercased ("John Hancock" →
// "JH", "madonna" → "M"). Blank/non-alphabetic names yield an empty string.
QString initialsForName(const QString &name);

// Initials tab: render the monogram derived from `name` at
// kInitialsPointSizeDefault (24pt). Null image when the name yields no
// initials — callers must treat that exactly like a cancelled dialog.
QImage initialsFromName(const QString &name, const QString &fontFamily,
                        const QColor &color);

// Upload tab: load an image from `path`. On failure returns a null image and
// a non-empty `error` (never throws, never crashes on unreadable files).
// Images larger than kMaxImageDim are downscaled to fit (huge-file edge case).
QImage loadUploaded(const QString &path, QString *error = nullptr);

// Both tabs: build the AnnotationItem that AnnotationLayer paints and
// PoDoFoBackend persists. `typedText` is kept as the item's /Contents fallback
// so typed signatures remain searchable. Kind::Draw has no image form — it
// maps to the existing freehand ToolMode with a null image. Kind::Initials
// reuses the typed ToolMode as well (PdfEnums.h ordinals are frozen, so the
// variant shares the AddSignatureTyped persistence instead of minting a new
// ordinal — the monogram image and the /Contents name carry the difference).
AnnotationItem makeAnnotation(Kind kind, int pageIndex, const QRectF &rect,
                              const QImage &image, const QString &typedText = {});

} // namespace SignatureContent

// §9.7 P1: per-session memory of the last ACCEPTED signature. One cache is
// parented to the DocumentSession (file-local helper in EditController.cpp),
// so its lifetime IS the document session's and no extra teardown wiring is
// needed. Scoped to one document: noteDocument() clears the stored signature
// when the document path switches. The app has no close-to-welcome transition
// (showWelcome() has zero call sites), so the document path IS the session
// identity here.
class SignatureSessionCache : public QObject {
    Q_OBJECT
public:
    explicit SignatureSessionCache(QObject *parent = nullptr);

    // EditController calls this each time the picker is about to open.
    // Same path as last time → keep the signature; different path → clear it.
    void noteDocument(const QString &path);

    bool hasSignature() const { return !m_image.isNull(); }

    // Remember the last accepted signature. A null image (Kind::Draw has no
    // image form) is ignored — freehand strokes are not reusable here.
    void store(SignatureContent::Kind kind, const QImage &image,
               const QString &typedText);

    SignatureContent::Kind kind() const { return m_kind; }
    QImage image() const { return m_image; }
    QString typedText() const { return m_typedText; }

private:
    QString m_docPath;
    SignatureContent::Kind m_kind = SignatureContent::Kind::Upload;
    QImage m_image;
    QString m_typedText;
};

// The dialog itself: 4 tabs (Draw / Type / Upload / Initials). Draw keeps the
// existing freehand flow — pressing OK simply returns Kind::Draw and the
// controller arms ToolMode::AddSignature as before. Type, Upload and Initials
// produce a QImage here. OK stays disabled until the active tab yields a
// usable signature (non-empty text / decoded image), so neither an empty text
// nor an unreadable upload can be accepted. Tests drive the namespace-level
// seam above directly; the dialog is never exec()'d in tests.
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

    // §9.7 P1: attach the DocumentSession's cache. When it holds a signature
    // the "Reuse last signature" checkbox is offered and default-checked;
    // accepting with it checked delivers the cached signature unchanged, and
    // a fresh acceptance updates the cache for the next activation.
    void setSessionCache(SignatureSessionCache *cache);

private:
    void updateTypePreview();
    void updateUploadPreview();
    void updateInitialsPreview();
    void updateAccept();
    void onAccepted();

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_typeEdit = nullptr;
    QComboBox *m_fontCombo = nullptr;
    QSpinBox *m_sizeSpin = nullptr;
    QLabel *m_typePreview = nullptr;
    QLineEdit *m_initialsEdit = nullptr;
    QLabel *m_initialsPreview = nullptr;
    QLabel *m_uploadPreview = nullptr;
    QLabel *m_uploadError = nullptr;
    QPushButton *m_browseButton = nullptr;
    QCheckBox *m_reuseCheck = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    SignatureSessionCache *m_cache = nullptr;

    QImage m_uploadImage;
    SignatureContent::Kind m_kind = SignatureContent::Kind::Draw;
    QImage m_image;
    QString m_typedText;
};
