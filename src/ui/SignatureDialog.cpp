// SPDX-License-Identifier: Apache-2.0
#include "ui/SignatureDialog.h"
#include "ui/SignaturePicker.h"
#include "engines/SignatureManager.h"
#include "util/GpTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>

// ---------------------------------------------------------------------------
// SignatureAppearancePreview — live QPainter rendering of the /AP /N
// appearance the engine will write. It consumes the SAME layout planner as
// the signing path (SignatureManager::planSignatureAppearance) with a
// QFontMetricsF-based measurer, mirroring: white background, hairline border,
// optional signature image left (aspect-fit into the clamped ~30%-width
// panel), text lines right at the auto-fitted size. Preview pixels are
// treated 1:1 as points, which keeps the layout math identical to the PDF.
// ---------------------------------------------------------------------------
class SignatureAppearancePreview final : public QWidget
{
public:
    explicit SignatureAppearancePreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(110);
        setAccessibleName(tr("Signature appearance preview"));
        setAccessibleDescription(tr("Shows how the visible signature will be rendered on the page"));
    }

    void setSignatureImage(const QImage &image)
    {
        m_image = image;
        update();
    }

    void refresh(const QString &signerName, const QString &reason, const QString &location)
    {
        m_signerName = signerName;
        m_reason = reason;
        m_location = location;
        recomputePlan();
    }

protected:
    void resizeEvent(QResizeEvent *) override
    {
        recomputePlan(); // the plan depends on the preview rect size
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        painter.fillRect(rect(), Qt::white);
        painter.setPen(QPen(QColor(140, 140, 140), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const double pad = 4.0;
        const double w = width();
        const double h = height();

        // Image panel: same clamp as the engine's appearance layout.
        if (!m_image.isNull() && w > 2 * pad) {
            const double boxW = qBound(24.0, 0.30 * w, 0.45 * w);
            const double boxH = h - 2.0 * pad;
            const double imgW = m_image.width();
            const double imgH = m_image.height();
            if (imgW > 0 && imgH > 0 && boxH > 0) {
                const double scale = qMin(boxW / imgW, boxH / imgH);
                QRectF drawRect(pad, pad + (boxH - imgH * scale) / 2.0,
                                imgW * scale, imgH * scale);
                painter.drawImage(drawRect, m_image);
            }
        }

        if (m_plan.fontSize <= 0.0)
            return;
        QFont font(QStringLiteral("Helvetica"));
        font.setPointSizeF(m_plan.fontSize);
        painter.setFont(font);
        painter.setPen(Qt::black);
        const double textX = pad + (m_plan.imageLeft
            ? qBound(24.0, 0.30 * w, 0.45 * w)
            : 0.0);
        double baseline = h - pad - m_plan.fontSize;
        for (const QString &line : m_plan.lines) {
            painter.drawText(QPointF(textX, baseline), line);
            baseline -= 1.25 * m_plan.fontSize;
        }
    }

private:
    void recomputePlan()
    {
        auto measure = [](const QString &text, double fontSize) -> double {
            QFont font(QStringLiteral("Helvetica"));
            font.setPointSizeF(fontSize);
            const QFontMetricsF metrics(font);
            return metrics.horizontalAdvance(text);
        };
        // 1px = 1pt in the preview, so the widget rect IS the appearance rect.
        m_plan = SignatureManager::planSignatureAppearance(
            width(), height(), m_signerName, QDateTime::currentDateTime(),
            m_reason, m_location, !m_image.isNull(), measure);
        update();
    }

    QImage m_image;
    SignatureManager::SignatureAppearancePlan m_plan;
    QString m_signerName;
    QString m_reason;
    QString m_location;
};

namespace {
// The dialog cannot read the P12 CN (PKCS#12 parsing needs OpenSSL, which the
// UI library deliberately does not link). The preview therefore shows a
// placeholder in the identity line; the real CN is derived from the
// certificate inside SignatureManager at signing time.
const QString kPreviewSignerName = QStringLiteral("Signer Name");
} // namespace

SignatureDialog::SignatureDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Digital Signature"));
    setMinimumWidth(450);
    setAccessibleName(tr("Digital signature dialog"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *titleLabel = new QLabel(tr("Apply Digital Signature"));
    titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(gp::Theme::accent().name()));
    mainLayout->addWidget(titleLabel);

    QLabel *trustLabel = new QLabel(tr("Choose a .p12 or .pfx certificate from a trusted source. The certificate password unlocks the private key used to sign this document; it is not stored by Glyph PDF."));
    trustLabel->setObjectName("dialogHelpText");
    trustLabel->setWordWrap(true);
    mainLayout->addWidget(trustLabel);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(10);

    m_certPathEdit = new QLineEdit();
    m_certPathEdit->setPlaceholderText(tr("Select .p12 or .pfx certificate..."));
    m_certPathEdit->setAccessibleName(tr("Certificate file"));
    m_certPathEdit->setAccessibleDescription(tr("Path to the signing certificate file"));

    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    browseBtn->setAccessibleName(tr("Browse for certificate"));
    connect(browseBtn, &QPushButton::clicked, this, &SignatureDialog::browseCertificate);

    QHBoxLayout *certLayout = new QHBoxLayout();
    certLayout->addWidget(m_certPathEdit);
    certLayout->addWidget(browseBtn);

    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Certificate Password"));
    m_passwordEdit->setAccessibleName(tr("Certificate password"));
    m_passwordEdit->setAccessibleDescription(tr("Password used to unlock the certificate private key"));

    m_reasonEdit = new QLineEdit();
    m_reasonEdit->setPlaceholderText(tr("e.g. I am the author of this document"));
    m_reasonEdit->setAccessibleName(tr("Signing reason"));

    m_locationEdit = new QLineEdit();
    m_locationEdit->setPlaceholderText(tr("e.g. New York, USA"));
    m_locationEdit->setAccessibleName(tr("Signing location"));

    formLayout->addRow(tr("Certificate:"), certLayout);
    formLayout->addRow(tr("Password:"), m_passwordEdit);
    formLayout->addRow(tr("Reason:"), m_reasonEdit);
    formLayout->addRow(tr("Location:"), m_locationEdit);

    mainLayout->addLayout(formLayout);

    // §9.7 P0: live appearance preview + optional signature image picker.
    m_appearancePreview = new SignatureAppearancePreview(this);
    mainLayout->addWidget(m_appearancePreview);

    QLabel *previewNote = new QLabel(tr("Preview of the visible signature (the signer name is taken from the certificate at signing time)."));
    previewNote->setObjectName("dialogHelpText");
    previewNote->setWordWrap(true);
    mainLayout->addWidget(previewNote);

    m_imageBrowseButton = new QPushButton(tr("Appearance Image..."));
    m_imageBrowseButton->setAccessibleName(tr("Choose a signature appearance image"));
    m_imageBrowseButton->setToolTip(tr("Optional image shown on the left of the signature (PNG/JPEG)"));
    connect(m_imageBrowseButton, &QPushButton::clicked, this, &SignatureDialog::browseAppearanceImage);

    m_imageClearButton = new QPushButton(tr("Clear"));
    m_imageClearButton->setAccessibleName(tr("Clear the signature appearance image"));
    m_imageClearButton->setEnabled(false);
    connect(m_imageClearButton, &QPushButton::clicked, this, &SignatureDialog::clearAppearanceImage);

    QHBoxLayout *imageLayout = new QHBoxLayout();
    imageLayout->addWidget(new QLabel(tr("Signature image (optional):")));
    imageLayout->addWidget(m_imageBrowseButton);
    imageLayout->addWidget(m_imageClearButton);
    imageLayout->addStretch(1);
    mainLayout->addLayout(imageLayout);

    m_imageErrorLabel = new QLabel();
    m_imageErrorLabel->setObjectName("dialogHelpText");
    m_imageErrorLabel->setWordWrap(true);
    m_imageErrorLabel->hide();
    mainLayout->addWidget(m_imageErrorLabel);

    connect(m_reasonEdit, &QLineEdit::textChanged, this, &SignatureDialog::updatePreview);
    connect(m_locationEdit, &QLineEdit::textChanged, this, &SignatureDialog::updatePreview);

    QLabel *validationLabel = new QLabel(tr("Recipients trust the signature only when the certificate chain is trusted. Use Validate Signature after signing to confirm status."));
    validationLabel->setObjectName("dialogHelpText");
    validationLabel->setWordWrap(true);
    mainLayout->addWidget(validationLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_certPathEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Certificate Required"), tr("Select a .p12 or .pfx certificate before signing."));
            m_certPathEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (m_passwordEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Password Required"), tr("Enter the certificate password to unlock the signing key."));
            m_passwordEdit->setFocus(Qt::OtherFocusReason);
            return;
        }
        // Hand the optional appearance image to the engine. Consume-once: the
        // next signDocument/certifyDocument embeds it; a null image clears any
        // stale slot entry so no image from a previous dialog can leak.
        SignatureManager::setPendingAppearanceImage(m_appearanceImage);
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Styling
    setStyleSheet(QString(
        "QDialog { background-color: #1E1E1E; }"
        "QLabel { color: #DDDDDD; }"
        "QLabel#dialogHelpText { color: #A8ABB0; line-height: 130%; }"
        "QLineEdit { background-color: #2D2D2D; color: white; border: 1px solid #444; padding: 5px; border-radius: 3px; }"
        "QLineEdit:focus { border: 1px solid %1; }"
        "QPushButton { background-color: %1; color: black; border: none; padding: 5px 15px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #FFD700; }"
    ).arg(gp::Theme::accent().name()));

    updatePreview();
}

QString SignatureDialog::certificatePath() const { return m_certPathEdit->text(); }
QString SignatureDialog::password() const { return m_passwordEdit->text(); }
QString SignatureDialog::reason() const { return m_reasonEdit->text(); }
QString SignatureDialog::location() const { return m_locationEdit->text(); }

void SignatureDialog::browseCertificate()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Certificate"), QString(), tr("Certificates (*.p12 *.pfx)"));
    if (!path.isEmpty()) {
        m_certPathEdit->setText(path);
    }
}

void SignatureDialog::browseAppearanceImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Signature Image"), QString(),
                                                tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    QString error;
    // Read-only reuse of the §9.7 picker's hardened loader: null image +
    // non-empty error on failure, huge files downscaled, never throws.
    QImage image = SignatureContent::loadUploaded(path, &error);
    if (image.isNull()) {
        m_imageErrorLabel->setText(error.isEmpty() ? tr("Could not load the image.") : error);
        m_imageErrorLabel->show();
        return;
    }
    m_appearanceImage = image;
    m_imageErrorLabel->hide();
    m_imageClearButton->setEnabled(true);
    updatePreview();
}

void SignatureDialog::clearAppearanceImage()
{
    m_appearanceImage = QImage();
    m_imageClearButton->setEnabled(false);
    m_imageErrorLabel->hide();
    updatePreview();
}

void SignatureDialog::updatePreview()
{
    m_appearancePreview->setSignatureImage(m_appearanceImage);
    m_appearancePreview->refresh(kPreviewSignerName, m_reasonEdit->text(), m_locationEdit->text());
}
