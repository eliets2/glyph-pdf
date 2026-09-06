// SPDX-License-Identifier: Apache-2.0
#include "ui/SignaturePicker.h"
#include "core/Capability.h"   // U08: signature-kind disclosure wording

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace SignatureContent {

QImage renderTyped(const QString &text, const QString &fontFamily,
                   int pointSize, const QColor &color)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return QImage();

    // Graceful fallback: if the family is not installed QFont resolves to the
    // default application family — the render never fails on a missing font.
    QFont font(fontFamily, qMax(4, pointSize));
    font.setHintingPreference(QFont::PreferNoHinting); // smooth when scaled into the placed rect

    const QFontMetrics fm(font);
    const QRect textRect = fm.boundingRect(trimmed);
    const int pad = qMax(4, pointSize / 4);

    QImage img(textRect.width() + 2 * pad, textRect.height() + 2 * pad,
               QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(font);
    p.setPen(color);
    p.drawText(img.rect().adjusted(pad, pad, -pad, -pad), Qt::AlignCenter, trimmed);
    p.end();
    return img;
}

QString initialsForName(const QString &name)
{
    // First letter of every whitespace-separated token, uppercased. Letters
    // after a leading punctuation ("'van Gogh") still count; tokens without
    // any letter contribute nothing. Empty result for a blank name.
    QString initials;
    const QStringList tokens =
        name.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        for (const QChar &ch : token) {
            if (ch.isLetter()) {
                initials.append(ch.toUpper());
                break;
            }
        }
    }
    return initials;
}

QImage initialsFromName(const QString &name, const QString &fontFamily,
                        const QColor &color)
{
    // 24pt monogram vs the Type tab's 36pt full name — the paraph is meant to
    // be placed smaller than a written-out signature.
    return renderTyped(initialsForName(name), fontFamily,
                       kInitialsPointSizeDefault, color);
}

QImage loadUploaded(const QString &path, QString *error)
{
    if (error)
        error->clear();
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QObject::tr("No image selected.");
        return QImage();
    }
    QImage img(path);
    if (img.isNull()) {
        if (error)
            *error = QObject::tr("Could not read the image file: %1").arg(path);
        return QImage();
    }
    if (img.width() > kMaxImageDim || img.height() > kMaxImageDim) {
        // Same 10000 x 10000 ceiling replaceImage enforces — cap instead of
        // rejecting so an oversized photo still becomes a usable signature.
        img = img.scaled(kMaxImageDim, kMaxImageDim, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    }
    return img;
}

AnnotationItem makeAnnotation(Kind kind, int pageIndex, const QRectF &rect,
                              const QImage &image, const QString &typedText)
{
    AnnotationItem anno;
    anno.pageIndex = pageIndex;
    anno.rect = rect;
    anno.image = image;
    anno.text = typedText;
    anno.color = Qt::darkBlue;
    anno.thickness = 2;
    switch (kind) {
    case Kind::Typed:
    case Kind::Initials:
        // §9.7 P1: the initials variant shares the TYPED persistence —
        // PdfEnums.h ordinals are frozen, so no new ToolMode is minted; the
        // monogram image + the /Contents name carry the difference.
        anno.mode = ToolMode::AddSignatureTyped;
        break;
    case Kind::Upload:
        anno.mode = ToolMode::AddSignatureUpload;
        break;
    case Kind::Draw:
        // Draw has no image form: the controller arms the existing freehand
        // flow instead of calling makeAnnotation. Map to the legacy mode so a
        // stray call degrades to something harmless.
        anno.mode = ToolMode::AddSignature;
        break;
    }
    return anno;
}

} // namespace SignatureContent

SignaturePickerDialog::SignaturePickerDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("signaturePickerDialog"));
    setWindowTitle(tr("Adopt a signature"));
    setAccessibleName(tr("Signature picker"));
    setAccessibleDescription(tr("Choose how to create your signature: draw it, "
                                "type it, upload an image, or use your initials"));

    m_tabs = new QTabWidget(this);
    m_tabs->setAccessibleName(tr("Signature source"));

    // ── Draw tab: the existing freehand flow, unchanged ─────────────────────
    auto *drawTab = new QWidget(this);
    auto *drawLayout = new QVBoxLayout(drawTab);
    QLabel *drawLabel = new QLabel(
        tr("1. Press OK.\n"
           "2. Draw your signature with the mouse directly on the page — "
           "exactly like the pencil tool."), drawTab);
    drawLabel->setWordWrap(true);
    drawLayout->addWidget(drawLabel);
    drawLayout->addStretch(1);
    m_tabs->addTab(drawTab, tr("Draw"));

    // ── Type tab ────────────────────────────────────────────────────────────
    auto *typeTab = new QWidget(this);
    auto *typeLayout = new QVBoxLayout(typeTab);
    m_typeEdit = new QLineEdit(typeTab);
    m_typeEdit->setObjectName(QStringLiteral("signatureTypeEdit"));
    m_typeEdit->setAccessibleName(tr("Signature text"));
    m_typeEdit->setPlaceholderText(tr("Type your signature…"));
    m_fontCombo = new QComboBox(typeTab);
    m_fontCombo->setAccessibleName(tr("Signature font"));
    // Handwriting-ish candidates first; Qt falls back gracefully when a font
    // is not installed (no new dependencies).
    m_fontCombo->addItems({ QStringLiteral("Segoe Script"),
                            QStringLiteral("Brush Script MT"),
                            QStringLiteral("Lucida Handwriting"),
                            QStringLiteral("Comic Sans MS"),
                            QStringLiteral("Arial") });
    m_sizeSpin = new QSpinBox(typeTab);
    m_sizeSpin->setAccessibleName(tr("Signature text size"));
    m_sizeSpin->setRange(8, 96);
    m_sizeSpin->setValue(SignatureContent::kTypedPointSizeDefault);
    m_typePreview = new QLabel(typeTab);
    m_typePreview->setObjectName(QStringLiteral("typePreview"));
    m_typePreview->setAccessibleName(tr("Signature preview"));
    m_typePreview->setMinimumHeight(84);
    m_typePreview->setAlignment(Qt::AlignCenter);
    m_typePreview->setFrameStyle(QFrame::StyledPanel);
    typeLayout->addWidget(new QLabel(tr("Signature text:"), typeTab));
    typeLayout->addWidget(m_typeEdit);
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(m_fontCombo, 1);
    fontRow->addWidget(m_sizeSpin);
    typeLayout->addLayout(fontRow);
    typeLayout->addWidget(m_typePreview, 1);
    m_tabs->addTab(typeTab, tr("Type"));

    // ── Initials tab (§9.7 P1) ──────────────────────────────────────────────
    // Derives a compact 24pt monogram from the full name — a faster variant of
    // the Type tab for people who sign with initials.
    auto *initialsTab = new QWidget(this);
    auto *initialsLayout = new QVBoxLayout(initialsTab);
    m_initialsEdit = new QLineEdit(initialsTab);
    m_initialsEdit->setObjectName(QStringLiteral("signatureInitialsEdit"));
    m_initialsEdit->setAccessibleName(tr("Full name for initials"));
    m_initialsEdit->setPlaceholderText(tr("Type your full name…"));
    m_initialsPreview = new QLabel(initialsTab);
    m_initialsPreview->setObjectName(QStringLiteral("initialsPreview"));
    m_initialsPreview->setAccessibleName(tr("Initials preview"));
    m_initialsPreview->setMinimumHeight(84);
    m_initialsPreview->setAlignment(Qt::AlignCenter);
    m_initialsPreview->setFrameStyle(QFrame::StyledPanel);
    initialsLayout->addWidget(new QLabel(tr("Your full name:"), initialsTab));
    initialsLayout->addWidget(m_initialsEdit);
    initialsLayout->addWidget(m_initialsPreview, 1);
    m_tabs->addTab(initialsTab, tr("Initials"));

    // ── Upload tab ──────────────────────────────────────────────────────────
    auto *uploadTab = new QWidget(this);
    auto *uploadLayout = new QVBoxLayout(uploadTab);
    m_browseButton = new QPushButton(tr("Choose image…"), uploadTab);
    m_browseButton->setAccessibleName(tr("Choose signature image file"));
    m_uploadPreview = new QLabel(uploadTab);
    m_uploadPreview->setObjectName(QStringLiteral("uploadPreview"));
    m_uploadPreview->setAccessibleName(tr("Uploaded signature preview"));
    m_uploadPreview->setMinimumHeight(84);
    m_uploadPreview->setAlignment(Qt::AlignCenter);
    m_uploadPreview->setFrameStyle(QFrame::StyledPanel);
    m_uploadError = new QLabel(uploadTab);
    m_uploadError->setObjectName(QStringLiteral("uploadError"));
    m_uploadError->setWordWrap(true);
    m_uploadError->hide();
    uploadLayout->addWidget(m_browseButton);
    uploadLayout->addWidget(m_uploadPreview, 1);
    uploadLayout->addWidget(m_uploadError);
    m_tabs->addTab(uploadTab, tr("Upload"));

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->setAccessibleName(tr("Signature picker actions"));

    // U08: label the flow by KIND before the user commits — Draw/Type/Upload
    // produce a graphic stamp; a certificate-backed (P12) digital signature
    // with its validation outcome is a different flow. Wording is owned by
    // the CapabilityRegistry (visibleSignatureKindDisclosure).
    auto *kindLabel = new QLabel(gp::visibleSignatureKindDisclosure(), this);
    kindLabel->setObjectName(QStringLiteral("signatureKindDisclosure"));
    kindLabel->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(kindLabel);
    layout->addWidget(m_buttons);

    connect(m_typeEdit, &QLineEdit::textChanged, this, &SignaturePickerDialog::updateAccept);
    connect(m_typeEdit, &QLineEdit::textChanged, this, &SignaturePickerDialog::updateTypePreview);
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, &SignaturePickerDialog::updateTypePreview);
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, &SignaturePickerDialog::updateInitialsPreview);
    connect(m_sizeSpin, &QSpinBox::valueChanged, this, &SignaturePickerDialog::updateTypePreview);
    connect(m_initialsEdit, &QLineEdit::textChanged, this, &SignaturePickerDialog::updateAccept);
    connect(m_initialsEdit, &QLineEdit::textChanged, this, &SignaturePickerDialog::updateInitialsPreview);
    connect(m_browseButton, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Choose signature image"), {},
            tr("Images (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*)"));
        if (file.isEmpty())
            return;                              // file-dialog cancel → nothing changes
        QString error;
        m_uploadImage = SignatureContent::loadUploaded(file, &error);
        m_uploadError->setText(error);
        m_uploadError->setVisible(!error.isEmpty());
        updateUploadPreview();
        updateAccept();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, &SignaturePickerDialog::updateAccept);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &SignaturePickerDialog::onAccepted);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateTypePreview();
    updateInitialsPreview();
    updateAccept();
}

void SignaturePickerDialog::showTab(SignatureContent::Kind kind)
{
    switch (kind) {
    case SignatureContent::Kind::Draw:     m_tabs->setCurrentIndex(0); break;
    case SignatureContent::Kind::Typed:    m_tabs->setCurrentIndex(1); break;
    case SignatureContent::Kind::Upload:   m_tabs->setCurrentIndex(2); break;
    case SignatureContent::Kind::Initials: m_tabs->setCurrentIndex(3); break;
    }
}

bool SignaturePickerDialog::isAcceptEnabled() const
{
    return m_buttons->button(QDialogButtonBox::Ok)->isEnabled();
}

void SignaturePickerDialog::updateTypePreview()
{
    const QImage ink = SignatureContent::renderTyped(
        m_typeEdit ? m_typeEdit->text() : QString(),
        m_fontCombo ? m_fontCombo->currentText() : QString(),
        m_sizeSpin ? m_sizeSpin->value() : SignatureContent::kTypedPointSizeDefault, Qt::darkBlue);
    if (ink.isNull()) {
        m_typePreview->setPixmap(QPixmap());
        m_typePreview->setText(tr("Type your signature to see a preview"));
    } else {
        m_typePreview->setText({});
        m_typePreview->setPixmap(QPixmap::fromImage(ink)
                                     .scaled(m_typePreview->size(), Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    }
}

void SignaturePickerDialog::updateInitialsPreview()
{
    const QImage ink = SignatureContent::initialsFromName(
        m_initialsEdit ? m_initialsEdit->text() : QString(),
        m_fontCombo ? m_fontCombo->currentText() : QString(), Qt::darkBlue);
    if (ink.isNull()) {
        m_initialsPreview->setPixmap(QPixmap());
        m_initialsPreview->setText(tr("Type your full name to see your initials"));
    } else {
        m_initialsPreview->setText({});
        m_initialsPreview->setPixmap(QPixmap::fromImage(ink)
                                         .scaled(m_initialsPreview->size(), Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
    }
}

void SignaturePickerDialog::updateUploadPreview()
{
    if (m_uploadImage.isNull()) {
        m_uploadPreview->setPixmap(QPixmap());
        m_uploadPreview->setText(tr("No image chosen yet"));
    } else {
        m_uploadPreview->setText({});
        m_uploadPreview->setPixmap(QPixmap::fromImage(m_uploadImage)
                                       .scaled(m_uploadPreview->size(), Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
    }
}

void SignaturePickerDialog::updateAccept()
{
    bool ok = true;
    switch (m_tabs->currentIndex()) {
    case 1:  // Type — blank text must not be acceptable
        ok = !m_typeEdit->text().trimmed().isEmpty();
        break;
    case 3:  // Initials — a blank name yields no monogram to place
        ok = !m_initialsEdit->text().trimmed().isEmpty();
        break;
    case 2:  // Upload — an unreadable image must not be acceptable
        ok = !m_uploadImage.isNull();
        break;
    default: // Draw — the existing flow needs no input here
        ok = true;
        break;
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void SignaturePickerDialog::onAccepted()
{
    switch (m_tabs->currentIndex()) {
    case 1:
        m_kind = SignatureContent::Kind::Typed;
        m_typedText = m_typeEdit->text().trimmed();
        m_image = SignatureContent::renderTyped(m_typedText, m_fontCombo->currentText(),
                                                m_sizeSpin->value(), Qt::darkBlue);
        break;
    case 3:
        m_kind = SignatureContent::Kind::Initials;
        m_typedText = m_initialsEdit->text().trimmed();
        m_image = SignatureContent::initialsFromName(m_typedText, m_fontCombo->currentText(),
                                                     Qt::darkBlue);
        break;
    case 2:
        m_kind = SignatureContent::Kind::Upload;
        m_typedText.clear();
        m_image = m_uploadImage;
        break;
    default:
        m_kind = SignatureContent::Kind::Draw;
        m_typedText.clear();
        m_image = QImage();
        break;
    }
    accept();
}
