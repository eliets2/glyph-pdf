#pragma once
#include <QFrame>
#include <QString>

class ISignatureManager;
class QLabel;
class QLineEdit;

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

    QString m_currentPath;
};

} // namespace gp
