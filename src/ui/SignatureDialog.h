// SPDX-License-Identifier: Apache-2.0
#ifndef SIGNATUREDIALOG_H
#define SIGNATUREDIALOG_H

#include <QDialog>
#include <QImage>

class QLineEdit;
class QLabel;
class QPushButton;

// Live "how will the signature look on the page" widget. Defined in the .cpp;
// fed by the SAME planner the engine uses at signing time
// (SignatureManager::planSignatureAppearance), so the preview is faithful:
// white swatch, hairline border, optional signature image left, text lines
// right, auto-fit font size. View-only — nothing here is persisted except
// through the engine's signing path.
class SignatureAppearancePreview;

class SignatureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignatureDialog(QWidget *parent = nullptr);

    QString certificatePath() const;
    QString password() const;
    QString reason() const;
    QString location() const;

    // Optional signature image embedded on the left of the /AP /N appearance.
    // Null unless the user picked one. Handed to the engine through the
    // consume-once SignatureManager::setPendingAppearanceImage slot on accept.
    QImage appearanceImage() const { return m_appearanceImage; }

private slots:
    void browseCertificate();
    void browseAppearanceImage();
    void clearAppearanceImage();
    void updatePreview();

private:
    QLineEdit *m_certPathEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_reasonEdit;
    QLineEdit *m_locationEdit;

    SignatureAppearancePreview *m_appearancePreview;
    QLabel *m_imageErrorLabel;
    QPushButton *m_imageBrowseButton;
    QPushButton *m_imageClearButton;
    QImage m_appearanceImage;
};

#endif // SIGNATUREDIALOG_H
