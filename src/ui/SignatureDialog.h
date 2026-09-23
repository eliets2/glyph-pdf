// SPDX-License-Identifier: Apache-2.0
#ifndef SIGNATUREDIALOG_H
#define SIGNATUREDIALOG_H

#include <QDialog>
#include <QImage>

#include "core/interfaces/ISignatureManager.h"

class QLineEdit;
class QLabel;
class QComboBox;
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

    // ── N18: DocMDP certify-vs-approve selector ─────────────────────────────
    // The purpose combo (Approve / Certify) and the certification-level combo
    // (plain-language wording mapped 1:1 to /DocMDP P=1..3). Approve is the
    // default so the existing sign flow is untouched; certificationLevel()
    // returns 0 for Approve and the selected 1..3 for Certify.
    int certificationLevel() const;
    bool isCertifySelected() const;

    // Refuse-safely BEFORE any engine call: a certification signature must be
    // the FIRST signature in a document, so `count > 0` keeps the Certify
    // choice VISIBLE but disabled with the exact reason (count + why). The
    // dialog never hides the unavailable choice — it discloses it.
    void setExistingSignatureCount(int count);

    // Pure 1:1 level ↔ wording mapper (unit-testable without UI). Level 1 →
    // no changes allowed, 2 → form filling allowed, 3 → form filling and
    // commenting allowed; each label carries its /DocMDP P value. Any other
    // level (including 0) yields an EMPTY string — levels outside 1..3 do not
    // exist, so the dialog refuses to invent wording for them (the engine
    // fail-louds them exactly the same way).
    static QString certificationLevelLabel(int level);

    // Honest ATTAINED-level wording for the outcome the engine reported (the
    // engine's contract: a requested level is either attained exactly or the
    // whole certification FAILED — never a silent downgrade). Failed/NotRun
    // never claim certification happened.
    static QString attainmentWord(SignOutcome outcome, int requestedLevel);

    // Consume-once seam for the certify request (the pattern of
    // SignatureManager::setPendingAppearanceImage/takePendingAppearanceImage):
    // an accepted certify dialog publishes the chosen level here; the signing
    // request takes it once. NOTE (lane handoff): the production consumer is
    // one line in SecurityController::certifyDocument() — that file is locked
    // to another lane, so the pending slot is the seam it will consume.
    static void setPendingCertificationLevel(int level);
    static int takePendingCertificationLevel();

    // Optional signature image embedded on the left of the /AP /N appearance.
    // Null unless the user picked one. Handed to the engine through the
    // consume-once SignatureManager::setPendingAppearanceImage slot on accept.
    QImage appearanceImage() const { return m_appearanceImage; }

private slots:
    void browseCertificate();
    void browseAppearanceImage();
    void clearAppearanceImage();
    void updatePreview();
    void updateCertifyUi();

private:
    QLineEdit *m_certPathEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_reasonEdit;
    QLineEdit *m_locationEdit;

    // N18 selector state
    QComboBox *m_purposeCombo = nullptr;
    QComboBox *m_levelCombo = nullptr;
    QLabel *m_levelDescription = nullptr;
    QLabel *m_certifyUnavailableLabel = nullptr;
    int m_existingSignatureCount = 0;

    SignatureAppearancePreview *m_appearancePreview;
    QLabel *m_imageErrorLabel;
    QPushButton *m_imageBrowseButton;
    QPushButton *m_imageClearButton;
    QImage m_appearanceImage;
};

#endif // SIGNATUREDIALOG_H
