// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include <QList>
#include <QString>

// §9.7 P0: SignatureBadgeSpec / SignatureBadgeState (badge seam payload) —
// the signal parameter type must be COMPLETE where moc expands the
// staticMetaObject, so this header includes the definition directly.
#include "ui/PdfViewerWidget.h"

class ISignatureManager;
class QLabel;
class QLineEdit;
class QPushButton;

namespace gp {

class Badge;

class SignaturesPanel : public QFrame {
    Q_OBJECT
public:
    explicit SignaturesPanel(QWidget* parent = nullptr);

    /// Populate the DIGITAL ID card from the real signatures present in
    /// `filePath` (via ISignatureManager::validateSignatures). When the file
    /// is unsigned or no manager is available, the card shows an honest
    /// "no signatures" state rather than fabricated identity data.
    void setDocument(const QString& filePath, ISignatureManager* signing);

signals:
    /// Emitted when the user clicks "Place Signature". MainWindow routes this
    /// to the same ribbon Sign flow (SecurityController::signDocument).
    void placeSignatureRequested();

    /// §9.7 P0: per-signature on-page validity badges, derived from the SAME
    /// validateSignatures() output that fills the DIGITAL ID card (integrity /
    /// validity / trust). Emitted after every setDocument() validation run;
    /// an EMPTY list means "no signatures" and clears stale badges. Consumers
    /// connect this to PdfViewerWidget::setSignatureBadges.
    void signatureBadgesChanged(const QList<SignatureBadgeSpec>& badges);

private:
    void showNoSignatures(const QString& reason);

    // DIGITAL ID card value labels (updated by setDocument; layout never changes).
    QLabel* m_subjectVal     = nullptr;
    QLabel* m_issuerVal      = nullptr;
    QLabel* m_expiresVal     = nullptr;
    QLabel* m_algorithmVal   = nullptr;
    QLabel* m_fingerprintVal = nullptr;
    Badge*  m_statusBadge    = nullptr;
    QLabel* m_chainLabel     = nullptr;

    // Appearance preview (reflects the most recent real signer, else neutral).
    QLabel* m_previewText    = nullptr;

    // "Place Signature" CTA — disabled until a document is loaded.
    QPushButton* m_placeBtn  = nullptr;

    QString m_currentPath;
};

} // namespace gp
